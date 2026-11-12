#!/usr/bin/env node
/**
 * Build a repository-hosted satellite photo library.
 * Run via: node scripts/sync-satellite-images.mjs
 * Only downloads public image files from an explicit host allow-list.
 */
import fs from "node:fs/promises";
import path from "node:path";

const ROOT = process.cwd();
const OUT = path.join(ROOT, "satellite-images");
const MANIFEST_PATH = path.join(OUT, "manifest.json");
const GROUPS = ["stations", "weather", "amateur", "gnss", "science", "cubesat", "visual"];
const MAX_CHECKS = Math.max(1, Number(process.env.MAX_IMAGE_CHECKS || 300));
const MAX_BYTES = 1_200_000;
const MISS_TTL = 30 * 24 * 60 * 60 * 1000;
const ALLOWED_HOSTS = new Set([
  "db.satnogs.org",
  "db-satnogs.freetls.fastly.net",
  "images-assets.nasa.gov",
  "upload.wikimedia.org",
  "www.amsat.org"
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
  const bytes = Buffer.from(await response.arrayBuffer());
  if (!bytes.length || bytes.length > MAX_BYTES) throw new Error("image size outside allowed range");
  if (ext === "jpg" && (bytes[0] !== 0xff || bytes[1] !== 0xd8)) throw new Error("invalid JPEG");
  if (ext === "png" && !(bytes[0] === 0x89 && bytes.toString("ascii", 1, 4) === "PNG")) throw new Error("invalid PNG");
  if (ext === "webp" && bytes.toString("ascii", 0, 4) !== "RIFF") throw new Error("invalid WebP");
  return { bytes, ext, type };
}

await fs.mkdir(OUT, { recursive: true });
let manifest = { version: 1, generatedAt: now(), images: {} };
try {
  manifest = JSON.parse(await fs.readFile(MANIFEST_PATH, "utf8"));
  if (!manifest.images || typeof manifest.images !== "object") manifest.images = {};
} catch {}

const ids = new Set();
for (const group of GROUPS) {
  try {
    const feed = await json("https://celestrak.org/NORAD/elements/gp.php?GROUP=" + encodeURIComponent(group.toUpperCase()) + "&FORMAT=JSON");
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
  if (entry?.status === "not-found" && entry.checkedAt && Date.now() - Date.parse(entry.checkedAt) < MISS_TTL) return false;
  return true;
}).slice(0, MAX_CHECKS);

console.log("Unique catalog objects: " + ids.size + "; image records to check this run: " + due.length + "/" + MAX_CHECKS);
let downloaded = 0, missing = 0, failed = 0;
for (const id of due) {
  try {
    const payload = await json("https://db.satnogs.org/api/satellites/?format=json&norad_cat_id=" + encodeURIComponent(id));
    const record = Array.isArray(payload) ? payload[0] : (payload.results || [])[0];
    const source = normalizeImageUrl(imageValue(record));
    if (!source) {
      manifest.images[id] = { status: "not-found", checkedAt: now(), name: record?.name || record?.satellite_name || "" };
      missing++;
    } else {
      const photo = await fetchPhoto(source);
      const filename = id + "." + photo.ext;
      await fs.writeFile(path.join(OUT, filename), photo.bytes);
      manifest.images[id] = {
        status: "available",
        path: "/satellite-images/" + filename,
        source,
        contentType: photo.type,
        bytes: photo.bytes.length,
        checkedAt: now(),
        name: record?.name || record?.satellite_name || ""
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
