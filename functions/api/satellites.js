const ALLOWED_GROUPS = new Set([
  "stations", "weather", "amateur", "gnss", "science", "cubesat", "visual"
]);

export async function onRequest(context) {
  const { request } = context;
  const group = (new URL(request.url).searchParams.get("group") || "").toLowerCase();
  const cors = {
    "Access-Control-Allow-Origin": "*",
    "Access-Control-Allow-Methods": "GET, OPTIONS",
    "Access-Control-Allow-Headers": "Content-Type"
  };
  if (request.method === "OPTIONS") return new Response(null, { headers: cors });
  if (request.method !== "GET") {
    return new Response("Method not allowed", { status: 405, headers: cors });
  }
  if (!ALLOWED_GROUPS.has(group)) {
    return new Response("Unsupported satellite group", { status: 400, headers: cors });
  }

  const cache = caches.default;
  const cacheUrl = new URL("/api/satellites?group=" + group, request.url);
  const cacheKey = new Request(cacheUrl.toString(), { method: "GET" });
  const cached = await cache.match(cacheKey);
  if (cached) {
    const headers = new Headers(cached.headers);
    headers.set("X-CYD-Cache", "HIT");
    return new Response(cached.body, { status: cached.status, headers });
  }

  const upstreamUrl = "https://celestrak.org/NORAD/elements/gp.php?GROUP=" +
    encodeURIComponent(group.toUpperCase()) + "&FORMAT=JSON";
  try {
    const upstream = await fetch(upstreamUrl, {
      headers: { "User-Agent": "CYD-Satellite-Tracker/1.0 (+https://github.com/OlympicBearBoss/CYD-Satellite-Tracker)" }
    });
    if (!upstream.ok) {
      return new Response("CelesTrak returned HTTP " + upstream.status, {
        status: upstream.status,
        headers: { ...cors, "Content-Type": "text/plain; charset=utf-8", "Cache-Control": "no-store" }
      });
    }
    const data = await upstream.json();
    if (!Array.isArray(data)) {
      return new Response("Unexpected CelesTrak OMM response", {
        status: 502,
        headers: { ...cors, "Content-Type": "text/plain; charset=utf-8" }
      });
    }
    const response = new Response(JSON.stringify(data), {
      headers: {
        ...cors,
        "Content-Type": "application/json; charset=utf-8",
        "Cache-Control": "public, max-age=900",
        "X-CYD-Data-Source": "CelesTrak GP OMM",
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
