const ALLOWED_GROUPS = new Set([
  "stations", "weather", "amateur", "gnss", "science", "cubesat", "visual", "starlink"
]);

export async function onRequest(context) {
  const { request } = context;
  const params = new URL(request.url).searchParams;
  const group = (params.get("group") || "").toLowerCase();
  const norad = (params.get("norad") || "").trim();
  const cors = {
    "Access-Control-Allow-Origin": "*",
    "Access-Control-Allow-Methods": "GET, OPTIONS",
    "Access-Control-Allow-Headers": "Content-Type"
  };
  if (request.method === "OPTIONS") return new Response(null, { headers: cors });
  if (request.method !== "GET") {
    return new Response("Method not allowed", { status: 405, headers: cors });
  }
  if (norad && !/^\d{1,6}$/.test(norad)) {
    return new Response("Invalid NORAD catalog number", { status: 400, headers: cors });
  }
  if (!norad && !ALLOWED_GROUPS.has(group)) {
    return new Response("Unsupported satellite group", { status: 400, headers: cors });
  }

  const cache = caches.default;
  const cacheUrl = new URL(norad ? "/api/satellites?v=3&norad=" + norad : "/api/satellites?v=3&group=" + group, request.url);
  const cacheKey = new Request(cacheUrl.toString(), { method: "GET" });
  const cached = await cache.match(cacheKey);
  if (cached) {
    const headers = new Headers(cached.headers);
    headers.set("X-CYD-Cache", "HIT");
    return new Response(cached.body, { status: cached.status, headers });
  }

  const upstreamUrl = norad
    ? "https://celestrak.org/NORAD/elements/gp.php?CATNR=" + encodeURIComponent(norad) + "&FORMAT=JSON"
    : "https://celestrak.org/NORAD/elements/gp.php?GROUP=" + encodeURIComponent(group.toUpperCase()) + "&FORMAT=JSON";
  const jsonHeaders = { ...cors, "Content-Type": "application/json; charset=utf-8" };
  const tleExponent = (field) => {
    const value = String(field || "").trim();
    if (!value) return 0;
    const sign = value[0] === "-" ? -1 : 1;
    const digits = value.replace(/[+-]/g, "").slice(0, 5);
    const exponent = Number(value.slice(-2));
    return sign * Number("0." + digits) * Math.pow(10, Number.isFinite(exponent) ? exponent : 0);
  };
  const tleToOmm = (tle) => {
    if (!tle || !tle.line1 || !tle.line2) throw new Error("Backup TLE response is incomplete");
    const l1 = tle.line1, l2 = tle.line2;
    const yy = Number(l1.slice(18, 20));
    const day = Number(l1.slice(20, 32));
    const year = yy >= 57 ? 1900 + yy : 2000 + yy;
    const epoch = new Date(Date.UTC(year, 0, 1) + (day - 1) * 86400000).toISOString().replace(/Z$/, "");
    const ageDays = (Date.now() - Date.parse(epoch + "Z")) / 86400000;
    if (!Number.isFinite(ageDays) || ageDays < -1 || ageDays > 14) {
      throw new Error("Backup TLE is stale (" + Math.round(ageDays) + " days); refusing inaccurate orbit");
    }
    return {
      OBJECT_NAME: tle.name || "NORAD " + norad,
      OBJECT_ID: l1.slice(9, 17).trim(),
      EPOCH: epoch,
      MEAN_MOTION: Number(l2.slice(52, 63)),
      ECCENTRICITY: Number("0." + l2.slice(26, 33).trim()),
      INCLINATION: Number(l2.slice(8, 16)),
      RA_OF_ASC_NODE: Number(l2.slice(17, 25)),
      ARG_OF_PERICENTER: Number(l2.slice(34, 42)),
      MEAN_ANOMALY: Number(l2.slice(43, 51)),
      EPHEMERIS_TYPE: Number(l1.slice(62, 63)) || 0,
      CLASSIFICATION_TYPE: l1.slice(7, 8),
      NORAD_CAT_ID: Number(tle.satelliteId || norad),
      ELEMENT_SET_NO: Number(l1.slice(64, 68)) || 0,
      REV_AT_EPOCH: Number(l2.slice(63, 68)) || 0,
      BSTAR: tleExponent(l1.slice(53, 61)),
      MEAN_MOTION_DOT: Number(l1.slice(33, 43)),
      MEAN_MOTION_DDOT: tleExponent(l1.slice(44, 52))
    };
  };
  try {
    let data = null;
    let source = "CelesTrak GP OMM";
    let primaryError = null;
    try {
      const upstream = await fetch(upstreamUrl, {
        headers: { "User-Agent": "CYD-Satellite-Tracker/1.0 (+https://github.com/OlympicBearBoss/CYD-Satellite-Tracker)" },
        signal: AbortSignal.timeout(12000)
      });
      if (!upstream.ok) throw new Error("CelesTrak HTTP " + upstream.status);
      const parsed = await upstream.json();
      if (!Array.isArray(parsed)) throw new Error("Unexpected CelesTrak OMM response");
      data = parsed;
    } catch (error) {
      primaryError = error;
    }

    // Secondary daily TLE mirror: use only for individual NORAD requests and only
    // when its element epoch is no more than 14 days old.
    if (!data && norad) {
      const backup = await fetch("https://tle.ivanstanojevic.me/api/tle/" + encodeURIComponent(norad), {
        headers: { "Accept": "application/ld+json, application/json" },
        signal: AbortSignal.timeout(12000)
      });
      if (!backup.ok) throw new Error("CelesTrak: " + primaryError.message + "; TLE backup HTTP " + backup.status);
      data = [tleToOmm(await backup.json())];
      source = "Ivan Stanojevic TLE mirror (converted to OMM)";
    }
    if (!data) throw new Error("CelesTrak: " + (primaryError ? primaryError.message : "unavailable") + "; no backup for group " + group);

    const response = new Response(JSON.stringify(data), {
      headers: {
        ...jsonHeaders,
        "Cache-Control": source.startsWith("CelesTrak") ? "public, max-age=7200, s-maxage=7200" : "public, max-age=1800, s-maxage=1800",
        "X-CYD-Data-Source": source,
        "X-CYD-Cache": "MISS"
      }
    });
    await cache.put(cacheKey, response.clone());
    return response;
  } catch (error) {
    return new Response("Satellite data source unavailable: " + error.message, {
      status: 502,
      headers: { ...cors, "Content-Type": "text/plain; charset=utf-8", "Cache-Control": "no-store" }
    });
  }
}
