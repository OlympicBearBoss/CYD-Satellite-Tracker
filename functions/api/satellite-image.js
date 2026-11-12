const KNOWN_IMAGES = {
  // NASA image library, public-domain ISS photograph (small rendition).
  "25544": "https://images-assets.nasa.gov/image/iss056e201248/iss056e201248~small.jpg",
  // Wikimedia Commons thumbnail of NOAA-19.
  "33591": "https://upload.wikimedia.org/wikipedia/commons/thumb/4/4c/NOAA-19.jpg/320px-NOAA-19.jpg",
  // AMSAT-NA's public Fox-1B mission image.
  "43017": "https://www.amsat.org/wordpress/wp-content/uploads/2017/11/Fox-1B.jpg"
};

const MAX_JPEG_BYTES = 120000;
const CACHE_HEADERS = {
  "Cache-Control": "public, max-age=300, s-maxage=3600",
  "X-Content-Type-Options": "nosniff"
};

function normalizeSource(value) {
  if (typeof value !== "string" || !value.trim()) return "";
  try {
    const candidate = new URL(value.trim(), "https://db.satnogs.org");
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
      cf: { cacheTtl: 3600, cacheEverything: true }
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
    cf: { cacheTtl: 3600, cacheEverything: true }
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

  try {
    const sources = await resolveImageSources(norad);
    if (!sources.length) {
      return Response.json(
        { ok: true, norad_id: norad, image: "", error: "photo_not_found" },
        { headers: CACHE_HEADERS }
      );
    }

    if (url.searchParams.get("download") === "1") {
      let bytes = null;
      let selectedSource = "";
      for (const source of sources) {
        try {
          bytes = await fetchCompactJpeg(source);
          selectedSource = source;
          break;
        } catch (_) {
          // Try the next approved source, e.g. SatNOGS when a known image is too large.
        }
      }
      if (!bytes) throw new Error("no_usable_jpeg_source");
      return new Response(bytes, {
        status: 200,
        headers: {
          ...CACHE_HEADERS,
          "Content-Type": "image/jpeg",
          "Content-Length": String(bytes.byteLength),
          "Content-Disposition": "inline; filename=satellite.jpg",
          "X-Satellite-Image-Source": new URL(selectedSource).hostname
        }
      });
    }

    // Always return the production origin so firmware allow-list validation is
    // identical for the live site and Cloudflare preview deployments.
    const imageUrl = new URL("https://cyd-satellite-tracker.pages.dev/api/satellite-image");
    imageUrl.searchParams.set("norad", String(norad));
    imageUrl.searchParams.set("download", "1");
    return Response.json(
      { ok: true, norad_id: norad, image: imageUrl.toString(), sources: sources.length },
      { headers: CACHE_HEADERS }
    );
  } catch (error) {
    return Response.json(
      { ok: false, error: error.message || "satellite_image_unavailable" },
      { status: 502, headers: { "Cache-Control": "no-store" } }
    );
  }
}
