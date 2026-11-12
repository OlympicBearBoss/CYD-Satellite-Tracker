const KNOWN_IMAGES = {
  // NASA Johnson photograph of the ISS, Oct. 4, 2018 (NASA/Roscosmos credit).
  "25544": "https://images-assets.nasa.gov/image/iss056e201248/iss056e201248~small.jpg"
};

// The browser downsizes and recompresses this image before sending it to the
// ESP32. Do not reject a valid NASA source merely because its original JPEG
// rendition is larger than the final on-device payload.
const MAX_JPEG_BYTES = 2 * 1024 * 1024;
const CACHE_HEADERS = {
  "Cache-Control": "public, max-age=86400, s-maxage=31536000, stale-while-revalidate=604800",
  "X-Content-Type-Options": "nosniff"
};

function normalizeSource(value) {
  if (typeof value !== "string" || !value.trim()) return "";
  try {
    const raw = value.trim();
    // SatNOGS API returns image paths relative to /media/, e.g. satellites/NOAA-19.jpg.
    const normalized = /^\/?satellites\//.test(raw)
      ? "https://db.satnogs.org/media/" + raw.replace(/^\//, "")
      : raw;
    const candidate = new URL(normalized, "https://db.satnogs.org");
    const allowedHosts = new Set([
      "images-assets.nasa.gov",
      "upload.wikimedia.org",
      "www.amsat.org",
      "db.satnogs.org",
      "db-satnogs.freetls.fastly.net"
    ]);
    return candidate.protocol === "https:" && allowedHosts.has(candidate.hostname)
      ? candidate.href : "";
  } catch (_) {
    return "";
  }
}

async function resolveImageSources(norad) {
  const sources = [];
  const known = KNOWN_IMAGES[String(norad)];
  if (known) sources.push(known);

  try {
    const apiUrl = new URL("https://db.satnogs.org/api/satellites/");
    apiUrl.searchParams.set("format", "json");
    apiUrl.searchParams.set("norad_cat_id", String(norad));
    const upstream = await fetch(apiUrl.toString(), {
      headers: { Accept: "application/json" },
      cf: { cacheTtl: 31536000, cacheEverything: true }
    });
    if (upstream.ok) {
      const payload = await upstream.json();
      const satellite = Array.isArray(payload) ? payload[0] : (payload.results || [])[0];
      const image = satellite && (satellite.image || satellite.image_url);
      const value = typeof image === "string" ? image : image && (image.url || image.image);
      const fallback = normalizeSource(value);
      if (fallback && !sources.includes(fallback)) sources.push(fallback);
    }
  } catch (_) {
    // A known public source can still work when SatNOGS is temporarily offline.
  }
  return sources;
}

async function fetchCompactJpeg(source) {
  const response = await fetch(source, {
    headers: { Accept: "image/jpeg,image/*;q=0.8" },
    cf: { cacheTtl: 31536000, cacheEverything: true }
  });
  if (!response.ok) throw new Error("image_upstream_" + response.status);
  const contentType = (response.headers.get("content-type") || "").toLowerCase();
  if (!contentType.startsWith("image/jpeg")) throw new Error("image_not_jpeg");
  const bytes = await response.arrayBuffer();
  if (bytes.byteLength < 4 || bytes.byteLength > MAX_JPEG_BYTES) {
    throw new Error("image_size_out_of_range");
  }
  const view = new Uint8Array(bytes);
  if (view[0] !== 0xff || view[1] !== 0xd8 ||
      view[view.length - 2] !== 0xff || view[view.length - 1] !== 0xd9) {
    throw new Error("invalid_jpeg");
  }
  return bytes;
}

export async function onRequestGet({ request }) {
  const url = new URL(request.url);
  const norad = Number(url.searchParams.get("norad"));
  if (!Number.isInteger(norad) || norad < 1 || norad > 999999) {
    return Response.json({ ok: false, error: "invalid_norad_id" }, { status: 400 });
  }

  const isDownload = url.searchParams.get("download") === "1";
  // Bump the cache namespace so stale negative photo lookups are not reused.
  const cacheKey = new Request(new URL("/api/satellite-image?v=4&norad=" + norad + "&download=" + (isDownload ? "1" : "0"), url.origin).toString(), { method: "GET" });
  const edgeCache = caches.default;
  const cached = await edgeCache.match(cacheKey);
  if (cached) return cached;

  try {
    const sources = await resolveImageSources(norad);
    if (!sources.length) {
      const missing = Response.json(
        { ok: true, norad_id: norad, image: "", error: "photo_not_found" },
        { headers: { ...CACHE_HEADERS, "Cache-Control": "public, max-age=3600, s-maxage=86400" } }
      );
      await edgeCache.put(cacheKey, missing.clone());
      return missing;
    }

    if (isDownload) {
      let bytes = null;
      let selectedSource = "";
      for (const source of sources) {
        try {
          bytes = await fetchCompactJpeg(source);
          selectedSource = source;
          break;
        } catch (_) {
          // Try the next approved source, e.g. SatNOGS when a known image fails.
        }
      }
      if (!bytes) throw new Error("no_usable_jpeg_source");
      const imageResponse = new Response(bytes, {
        status: 200,
        headers: {
          ...CACHE_HEADERS,
          "Content-Type": "image/jpeg",
          "Content-Length": String(bytes.byteLength),
          "Content-Disposition": "inline; filename=satellite.jpg",
          "X-Satellite-Image-Source": new URL(selectedSource).hostname
        }
      });
      await edgeCache.put(cacheKey, imageResponse.clone());
      return imageResponse;
    }

    // Verify at least one source is a valid JPEG before advertising it.
    let usableSource = false;
    let selectedSource = "";
    for (const source of sources) {
      try {
        await fetchCompactJpeg(source);
        usableSource = true;
        selectedSource = source;
        break;
      } catch (_) {
        // Continue through the approved source list.
      }
    }
    if (!usableSource) {
      return Response.json(
        { ok: true, norad_id: norad, image: "", error: "no_usable_jpeg_source" },
        { headers: CACHE_HEADERS }
      );
    }

    // Always return the production origin so firmware allow-list validation is
    // identical for the live site and Cloudflare preview deployments.
    const imageUrl = new URL("https://cyd-satellite-tracker.pages.dev/api/satellite-image");
    imageUrl.searchParams.set("norad", String(norad));
    imageUrl.searchParams.set("download", "1");
    const metadata = Response.json(
      { ok: true, norad_id: norad, image: imageUrl.toString(), sources: sources.length, source: new URL(selectedSource).hostname },
      { headers: CACHE_HEADERS }
    );
    await edgeCache.put(cacheKey, metadata.clone());
    return metadata;
  } catch (error) {
    return Response.json(
      { ok: false, error: error.message || "satellite_image_unavailable" },
      { status: 502, headers: { "Cache-Control": "no-store" } }
    );
  }
}