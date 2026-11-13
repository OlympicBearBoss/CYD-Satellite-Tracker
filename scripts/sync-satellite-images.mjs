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
const MAX_CHECKS = Math.max(1, Number(process.env.MAX_IMAGE_CHECKS || 300));
const MAX_BYTES = 5_000_000;
const MAX_STORED_BYTES = 250_000;
const MISS_TTL = 30 * 24 * 60 * 60 * 1000;
const ALLOWED_HOSTS = new Set([
  "db.satnogs.org",
  "db-satnogs.freetls.fastly.net",
  "images-assets.nasa.gov",
  "upload.wikimedia.org"
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


function normalizedText(value) {
  return String(value || "").normalize("NFKD").toLowerCase()
    .replace(/[\\u0300-\\u036f]/g, "").replace(/[^a-z0-9]+/g, " ").trim();
}

function matchesSatellite(text, name, id) {
  const haystack = " " + normalizedText(text) + " ";
  const needle = normalizedText(name);
  if (needle.length >= 4 && haystack.includes(" " + needle + " ")) return true;
  return String(id).length >= 4 && haystack.includes(" " + String(id) + " ");
}

async function searchNasaImage(name, id) {
  if (!name || normalizedText(name).length < 4) return null;
  const url = "https://images-api.nasa.gov/search?q=" + encodeURIComponent(name) + "&media_type=image&page_size=30";
  const result = await json(url);
  for (const item of result?.collection?.items || []) {
    const meta = (item.data || [])[0] || {};
    const text = [meta.title, meta.description, ...(meta.keywords || [])].join(" ");
    if (!matchesSatellite(text, name, id)) continue;
    const imageLink = (item.links || []).find(link => link.render === "image" || /\\.(jpe?g|png|webp)(\\?|$)/i.test(link.href || ""));
    if (!imageLink?.href) continue;
    const source = normalizeImageUrl(imageLink.href);
    if (!source) continue;
    return {
      url: source,
      provider: "NASA Image and Video Library",
      sourcePage: "https://images.nasa.gov/details/" + encodeURIComponent(meta.nasa_id || ""),
      title: meta.title || "",
      description: String(meta.description || "").slice(0, 700),
      credit: meta.center || meta.photographer || "NASA",
      license: "NASA media usage guidelines; retain source credit"
    };
  }
  return null;
}

async function searchCommonsImage(name, id) {
  if (!name || normalizedText(name).length < 4) return null;
  const params = new URLSearchParams({
    action: "query", generator: "search", gsrsearch: name + " satellite",
    gsrnamespace: "6", gsrlimit: "30", prop: "imageinfo",
    iiprop: "url|extmetadata", iiurlwidth: "1200", format: "json"
  });
  const result = await json("https://commons.wikimedia.org/w/api.php?" + params.toString());
  const pages = Object.values(result?.query?.pages || {});
  for (const page of pages) {
    const info = (page.imageinfo || [])[0];
    if (!info) continue;
    const ext = info.extmetadata || {};
    const description = ext.ImageDescription?.value || "";
    if (!matchesSatellite([page.title, description].join(" "), name, id)) continue;
    const license = ext.LicenseShortName?.value || "";
    if (!/^(CC0|Public domain|PD|CC BY(?:-SA)?)/i.test(license)) continue;
    const imageUrl = info.thumburl || info.url;
    const source = normalizeImageUrl(imageUrl);
    if (!source) continue;
    return {
      url: source,
      provider: "Wikimedia Commons",
      sourcePage: info.descriptionurl || ("https://commons.wikimedia.org/wiki/" + encodeURIComponent(page.title.replace(/ /g, "_"))),
      title: page.title || "",
      description: String(description).replace(/<[^>]*>/g, "").slice(0, 700),
      credit: ext.Artist?.value || ext.Credit?.value || "",
      license
    };
  }
  return null;
}

async function findExternalImage(name, id) {
  for (const finder of [searchNasaImage, searchCommonsImage]) {
    try {
      const result = await finder(name, id);
      if (result) return result;
    } catch (error) {
      console.warn("External image search failed for " + id + " (" + finder.name + "): " + error.message);
    }
  }
  return null;
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
  return "";
}

async function fetchPhoto(url) {
  const response = await fetch(url, {
    headers: { "User-Agent": "CYD-Satellite-Tracker-image-library/1.0", Accept: "image/jpeg,image/png,image/webp" },
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

await fs.mkdir(OUT, { recursive: true });
let manifest = { version: 1, generatedAt: now(), images: {} };
try {
  manifest = JSON.parse(await fs.readFile(MANIFEST_PATH, "utf8"));
  if (!manifest.images || typeof manifest.images !== "object") manifest.images = {};
} catch {}

const ids = new Set();
const objectNames = new Map();
for (const group of GROUPS) {
  try {
    const feed = await json("https://celestrak.org/NORAD/elements/gp.php?GROUP=" + encodeURIComponent(group) + "&FORMAT=JSON");
    if (Array.isArray(feed)) {
      for (const item of feed) {
        const id = String(item.NORAD_CAT_ID || "");
        if (/^\d{1,6}$/.test(id)) ids.add(id);
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
  return true;
}).slice(0, MAX_CHECKS);

console.log("Unique catalog objects: " + ids.size + "; image records to check this run: " + due.length + "/" + MAX_CHECKS);
let downloaded = 0, missing = 0, failed = 0;
for (const id of due) {
  try {
    const payload = await json("https://db.satnogs.org/api/satellites/?format=json&norad_cat_id=" + encodeURIComponent(id));
    const record = Array.isArray(payload) ? payload[0] : (payload.results || [])[0];
    const name = record?.name || record?.satellite_name || objectNames.get(id) || manifest.images[id]?.name || "";
    let source = normalizeImageUrl(imageValue(record));
    let provenance = source ? {
      provider: "SatNOGS DB",
      sourcePage: "https://db.satnogs.org/satellite/" + encodeURIComponent(id) + "/",
      title: name,
      credit: "SatNOGS DB",
      license: "See source record for image rights"
    } : null;
    if (!source) {
      const external = await findExternalImage(name, id);
      if (external) {
        source = external.url;
        provenance = external;
      }
    }
    if (!source) {
      manifest.images[id] = { status: "not-found", checkedAt: now(), name };
      missing++;
    } else {
      const photo = await fetchPhoto(source);
      const filename = id + "." + photo.ext;
      await fs.writeFile(path.join(OUT, filename), photo.bytes);
      manifest.images[id] = {
        status: "available",
        path: "/satellite-images/" + filename,
        source,
        sourcePage: provenance.sourcePage,
        provider: provenance.provider,
        title: provenance.title || name,
        description: provenance.description || "",
        credit: provenance.credit || "",
        license: provenance.license || "",
        contentType: photo.type,
        bytes: photo.bytes.length,
        checkedAt: now(),
        name
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
