#!/usr/bin/env node
/**
 * Build a repository-hosted satellite photo library.
 * Run via: node scripts/sync-satellite-images.mjs
 * Only downloads public image files from an explicit host allow-list.
 */
import fs from "node:fs/promises";
import path from "node:path";
import sharp from "sharp";

const ROOT = process.cwd();
const OUT = path.join(ROOT, "satellite-images");
const MANIFEST_PATH = path.join(OUT, "manifest.json");
const GROUPS = ["stations", "weather", "amateur", "gnss", "science", "cubesat", "visual"];
const MAX_CHECKS = Math.max(1, Number(process.env.MAX_IMAGE_CHECKS || 600));
const MAX_BYTES = 5_000_000;
const MAX_STORED_BYTES = 250_000;
const MISS_TTL = 30 * 24 * 60 * 60 * 1000;
const ALLOWED_HOSTS = new Set([
  "db.satnogs.org",
  "db-satnogs.freetls.fastly.net",
  "upload.wikimedia.org",
  "www.amsat.org",
  "images-assets.nasa.gov"
]);

const sleep = ms => new Promise(resolve => setTimeout(resolve, ms));
const now = () => new Date().toISOString();

async function json(url) {
  const response = await fetch(url, {
    headers: { "User-Agent": "CYD-Satellite-Tracker-image-library/1.0", Accept: "application/json" },
    signal: AbortSignal.timeout(20000)
  });
  if (!response.ok) throw new Error("HTTP " + response.status + " for " + url);
  return response.json();
}

function normalizeImageUrl(value) {
  if (typeof value !== "string" || !value.trim()) return "";
  const raw = value.trim();
  const normalized = /^\/?satellites\//.test(raw)
    ? "https://db.satnogs.org/media/" + raw.replace(/^\//, "")
    : raw;
  try {
    const url = new URL(normalized, "https://db.satnogs.org");
    return url.protocol === "https:" && ALLOWED_HOSTS.has(url.hostname) ? url.href : "";
  } catch {
    return "";
  }
}

function imageValue(record) {
  const value = record?.image || record?.image_url || record?.photo;
  if (typeof value === "string") return value;
  return value && typeof value === "object" ? (value.url || value.image || value.original || "") : "";
}

function extensionFor(contentType) {
  const type = contentType.split(";")[0].trim().toLowerCase();
  if (type === "image/jpeg" || type === "image/jpg") return "jpg";
  if (type === "image/png") return "png";
  if (type === "image/webp") return "webp";
  if (type === "image/gif") return "gif";
  return "";
}

async function fetchPhoto(url) {
  const response = await fetch(url, {
    headers: { "User-Agent": "CYD-Satellite-Tracker-image-library/1.0", Accept: "image/jpeg,image/png,image/webp,image/gif" },
    signal: AbortSignal.timeout(20000)
  });
  if (!response.ok) throw new Error("image HTTP " + response.status);
  const type = response.headers.get("content-type") || "";
  const ext = extensionFor(type);
  if (!ext) throw new Error("unsupported image type: " + type);
  const original = Buffer.from(await response.arrayBuffer());
  if (!original.length || original.length > MAX_BYTES) throw new Error("source image size outside allowed range");
  if (ext === "jpg" && (original[0] !== 0xff || original[1] !== 0xd8)) throw new Error("invalid JPEG");
  if (ext === "png" && !(original[0] === 0x89 && original.toString("ascii", 1, 4) === "PNG")) throw new Error("invalid PNG");
  if (ext === "webp" && original.toString("ascii", 0, 4) !== "RIFF") throw new Error("invalid WebP");
  // Store compact, consistently sized JPEG thumbnails to keep the Git repository small.
  let bytes = await sharp(original, { limitInputPixels: 40000000 })
    .rotate()
    .resize({ width: 640, height: 360, fit: "inside", withoutEnlargement: true })
    .jpeg({ quality: 72, mozjpeg: true })
    .toBuffer();
  if (bytes.length > MAX_STORED_BYTES) {
    bytes = await sharp(original, { limitInputPixels: 40000000 })
      .rotate()
      .resize({ width: 640, height: 360, fit: "inside", withoutEnlargement: true })
      .jpeg({ quality: 55, mozjpeg: true })
      .toBuffer();
  }
  if (!bytes.length || bytes.length > MAX_STORED_BYTES) throw new Error("optimized image exceeds repository size budget");
  return { bytes, ext: "jpg", type: "image/jpeg" };
}


const SEARCH_VERSION = 2;
const CLASS_QUERIES = {
  stations: "space station orbital spacecraft",
  weather: "weather satellite spacecraft",
  amateur: "amateur radio CubeSat satellite",
  gnss: "navigation GNSS satellite spacecraft",
  science: "scientific research satellite spacecraft",
  cubesat: "CubeSat small satellite spacecraft",
  visual: "Earth observation satellite spacecraft"
};
// Require evidence for the requested spacecraft class; generic "satellite" hits are not enough.
const CLASS_EVIDENCE = {
  stations: /\b(space station|orbital station|international space station|tiangong|mir space station|iss)\b/i,
  weather: /\b(weather satellite|meteorological satellite|weather observatory|noaa[- ]?\d*|goes[- ]?\d*|meteosat|himawari|fengyun|metop|jpss)\b/i,
  amateur: /\b(amateur radio satellite|ham radio satellite|amsat|cubesat|cube.?sat|1u cubesat|2u cubesat|3u cubesat)\b/i,
  gnss: /\b(gnss|navigation satellite|gps satellite|galileo satellite|glonass|beidou|navic)\b/i,
  science: /\b(science satellite|scientific satellite|research satellite|space telescope|space observatory|x[- ]ray observatory|astronomy satellite)\b/i,
  cubesat: /\b(cubesat|cube.?sat|nanosatellite|nano.?satellite|1u|2u|3u|6u)\b/i,
  visual: /\b(earth observation|earth imaging|remote sensing satellite|imaging satellite|landsat|sentinel[- ]?\d|terra satellite|aqua satellite|spot satellite|pleiades satellite)\b/i
};
const clearlyUnrelated = /\b(miranda|uranus|neptune|jupiter|saturn|mars|venus|mercury|moon|lunar|planet|asteroid|comet|nebula|galaxy|rocket|launch vehicle|astronaut portrait)\b/i;
const representativePhotoCache = new Map();

function cleanSearchText(value) {
  return String(value || "").replace(/[^\p{L}\p{N} ._()-]/gu, " ").replace(/\s+/g, " ").trim().slice(0, 100);
}
function candidateScore(text, name, group, matchType) {
  const haystack = String(text || "");
  const rule = CLASS_EVIDENCE[group] || CLASS_EVIDENCE.visual;
  if (clearlyUnrelated.test(haystack) || !rule.test(haystack)) return null;
  const tokens = cleanSearchText(name).toLowerCase().split(/[\s_-]+/)
    .filter(token => token.length >= 3 && !["satellite", "spacecraft", "weather", "orbital"].includes(token));
  const normalized = haystack.toLowerCase();
  const nameHits = tokens.filter(token => normalized.includes(token)).length;
  if (matchType === "name-related" && (!tokens.length || nameHits === 0)) return null;
  return nameHits * 10 + 5;
}
async function fetchAlternativePhoto(name, group) {
  const safeName = cleanSearchText(name);
  const queries = safeName
    ? [safeName + " satellite spacecraft", CLASS_QUERIES[group] || CLASS_QUERIES.visual]
    : [CLASS_QUERIES[group] || CLASS_QUERIES.visual];
  for (let queryIndex = 0; queryIndex < queries.length; queryIndex++) {
    const query = queries[queryIndex];
    const matchType = queryIndex === 0 && safeName ? "name-related" : "class-representative";
    const cacheKey = group || "visual";
    if (matchType === "class-representative" && representativePhotoCache.has(cacheKey)) {
      return { ...representativePhotoCache.get(cacheKey), matchType };
    }
    const candidates = [];
    try {
      const nasa = await json("https://images-api.nasa.gov/search?q=" + encodeURIComponent(query) + "&media_type=image&page_size=12");
      for (const item of nasa?.collection?.items || []) {
        const data = item.data?.[0] || {};
        const text = (data.title || "") + " " + (data.description || "");
        const href = (item.links || []).find(link => link.rel === "preview" && link.href)?.href || item.href;
        const score = candidateScore(text, safeName, group, matchType);
        // NASA item.href is commonly a JSON manifest; accept preview image URLs only.
        if (href && score !== null) {
          candidates.push({ href: href.replace(/^http:/, "https:"), title: data.title || "", score, provider: "NASA" });
        }
      }
    } catch (error) {
      console.warn("NASA image search unavailable: " + error.message);
    }
    try {
      const params = new URLSearchParams({
        action: "query", generator: "search", gsrsearch: "filetype:bitmap " + query,
        gsrnamespace: "6", gsrlimit: "12", prop: "imageinfo",
        iiprop: "url|mime", iiurlwidth: "1200", format: "json"
      });
      const commons = await json("https://commons.wikimedia.org/w/api.php?" + params.toString());
      for (const page of Object.values(commons?.query?.pages || {})) {
        const info = page.imageinfo?.[0] || {};
        const title = String(page.title || "").replace(/^File:/, "");
        const href = info.thumburl || info.url;
        const score = candidateScore(title, safeName, group, matchType);
        if (href && /^image\/(jpeg|png|webp|gif)$/i.test(info.mime || "") && score !== null) {
          candidates.push({ href, title, score, provider: "Wikimedia Commons" });
        }
      }
    } catch (error) {
      console.warn("Wikimedia image search unavailable: " + error.message);
    }
    candidates.sort((a, b) => b.score - a.score);
    for (const candidate of candidates.slice(0, 8)) {
      try {
        const photo = await fetchPhoto(candidate.href);
        const result = { ...photo, source: candidate.href, provider: candidate.provider,
          matchedTitle: candidate.title.slice(0, 180), matchType };
        if (matchType === "class-representative") representativePhotoCache.set(cacheKey, result);
        return result;
      } catch (error) {
        console.warn("Alternative image rejected (" + candidate.provider + "): " + error.message);
      }
    }
  }
  return null;
}

await fs.mkdir(OUT, { recursive: true });
let manifest = { version: 1, generatedAt: now(), images: {} };
try {
  manifest = JSON.parse(await fs.readFile(MANIFEST_PATH, "utf8"));
  if (!manifest.images || typeof manifest.images !== "object") manifest.images = {};
} catch {}

const ids = new Set();
const idGroups = new Map();
for (const group of GROUPS) {
  try {
    const feed = await json("https://celestrak.org/NORAD/elements/gp.php?GROUP=" + encodeURIComponent(group) + "&FORMAT=JSON");
    if (Array.isArray(feed)) {
      for (const item of feed) {
        const id = String(item.NORAD_CAT_ID || "");
        if (/^\d{1,6}$/.test(id)) { ids.add(id); idGroups.set(id, group); }
      }
    }
    console.log(group + ": " + (Array.isArray(feed) ? feed.length : 0) + " objects");
  } catch (error) {
    console.warn("Catalog group unavailable (" + group + "): " + error.message);
  }
}

if (!ids.size) throw new Error("No catalog records were retrieved; refusing to write an empty image manifest.");

const due = [...ids].filter(id => {
  const entry = manifest.images[id];
  if (entry?.path && entry.checkedAt && Date.now() - Date.parse(entry.checkedAt) < 180 * 86400000) return false;
  if (entry?.status === "not-found" && entry.searchVersion >= SEARCH_VERSION && entry.checkedAt && Date.now() - Date.parse(entry.checkedAt) < MISS_TTL) return false;
  return true;
}).slice(0, MAX_CHECKS);

console.log("Unique catalog objects: " + ids.size + "; image records to check this run: " + due.length + "/" + MAX_CHECKS);
let downloaded = 0, missing = 0, failed = 0;
for (const id of due) {
  try {
    const payload = await json("https://db.satnogs.org/api/satellites/?format=json&norad_cat_id=" + encodeURIComponent(id));
    const record = Array.isArray(payload) ? payload[0] : (payload.results || [])[0];
    const name = record?.name || record?.satellite_name || "";
    const source = normalizeImageUrl(imageValue(record));
    let photo = null;
    let provider = "SatNOGS";
    let matchType = "catalog-record";
    let matchedTitle = name;
    if (source) {
      try { photo = await fetchPhoto(source); }
      catch (error) { console.warn("SatNOGS image rejected for " + id + ": " + error.message); }
    }
    if (!photo) {
      const alternative = await fetchAlternativePhoto(name, idGroups.get(id) || "visual");
      if (alternative) {
        photo = alternative;
        provider = alternative.provider;
        matchType = alternative.matchType;
        matchedTitle = alternative.matchedTitle;
      }
    }
    if (!photo) {
      manifest.images[id] = { status: "not-found", checkedAt: now(), name, searchVersion: SEARCH_VERSION };
      missing++;
    } else {
      const filename = id + ".jpg";
      await fs.writeFile(path.join(OUT, filename), photo.bytes);
      manifest.images[id] = {
        status: "available", path: "/satellite-images/" + filename,
        source: photo.source || source, provider, matchType, matchedTitle,
        contentType: "image/jpeg", bytes: photo.bytes.length,
        checkedAt: now(), name, searchVersion: SEARCH_VERSION
      };
      downloaded++;
    }
  } catch (error) {
    manifest.images[id] = { ...(manifest.images[id] || {}), status: "error", checkedAt: now(), error: error.message.slice(0, 180) };
    failed++;
  }
  if ((downloaded + missing + failed) % 25 === 0) {
    console.log("Progress: " + (downloaded + missing + failed) + "/" + due.length);
  }
  await sleep(180);
}

manifest.generatedAt = now();
manifest.catalogObjectsSeen = ids.size;
manifest.lastRun = { checked: due.length, downloaded, missing, failed, maxChecks: MAX_CHECKS };
await fs.writeFile(MANIFEST_PATH, JSON.stringify(manifest, null, 2) + "\n");
const available = Object.values(manifest.images).filter(entry => entry.status === "available" && entry.path).length;
console.log(JSON.stringify({ catalogObjectsSeen: ids.size, checkedThisRun: due.length, downloadedThisRun: downloaded, notFoundThisRun: missing, failedThisRun: failed, totalStoredImages: available }, null, 2));
