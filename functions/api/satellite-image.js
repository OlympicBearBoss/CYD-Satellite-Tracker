export async function onRequestGet({ request }) {
  const url = new URL(request.url);
  const norad = Number(url.searchParams.get("norad"));
  if (!Number.isInteger(norad) || norad < 1 || norad > 999999) {
    return Response.json({ ok: false, error: "invalid_norad_id" }, { status: 400 });
  }

  const upstreamUrl = new URL("https://db.satnogs.org/api/satellites/");
  upstreamUrl.searchParams.set("format", "json");
  upstreamUrl.searchParams.set("norad_cat_id", String(norad));

  try {
    const upstream = await fetch(upstreamUrl.toString(), {
      headers: { Accept: "application/json" },
      cf: { cacheTtl: 3600, cacheEverything: true }
    });
    if (!upstream.ok) {
      return Response.json(
        { ok: false, error: "satnogs_upstream_" + upstream.status },
        { status: 502, headers: { "Cache-Control": "no-store" } }
      );
    }

    const payload = await upstream.json();
    const satellite = Array.isArray(payload) ? payload[0] : (payload.results || [])[0];
    const image = satellite && (satellite.image || satellite.image_url);
    return Response.json(
      { ok: true, norad_id: norad, image: typeof image === "string" ? image : "" },
      { headers: { "Cache-Control": "public, max-age=300, s-maxage=3600" } }
    );
  } catch (error) {
    return Response.json(
      { ok: false, error: "satnogs_unavailable" },
      { status: 502, headers: { "Cache-Control": "no-store" } }
    );
  }
}
