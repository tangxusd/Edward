/* ═══════════════════════════════════════════════════════════════════════════
   Where things live.

   Two roots, because they have different lifetimes:

     APP_DIR   the checked-out code and the assets that ship with it. Read-only
               in practice. When FableCut is installed as a Claude Code plugin
               this directory is pinned to a commit and REPLACED on every update.

     DATA_DIR  the user's work — project.json, imported media, exports, cached
               analysis. Must outlive an update.

   Standalone (`node server.js`) they are the same directory, which is exactly
   how FableCut has always behaved. Set FABLECUT_DATA_DIR to split them; the
   plugin sets it to ${CLAUDE_PLUGIN_DATA}.
   ═══════════════════════════════════════════════════════════════════════════ */
"use strict";
const fs = require("fs");
const path = require("path");

const APP_DIR = __dirname;
const DATA_DIR = process.env.FABLECUT_DATA_DIR
  ? path.resolve(process.env.FABLECUT_DATA_DIR)
  : APP_DIR;
const SPLIT = DATA_DIR !== APP_DIR;

const userPath = (name, fallback) => process.env[name] ? path.resolve(process.env[name]) : fallback;
const CACHE_ROOT = userPath("FABLECUT_CACHE_ROOT", path.join(DATA_DIR, "cache"));
const MEDIA_DIR = userPath("FABLECUT_MEDIA_DIR", path.join(DATA_DIR, "media"));
const EXPORTS_DIR = userPath("FABLECUT_EXPORTS_DIR", path.join(DATA_DIR, "exports"));
const ANALYSIS_DIR = userPath("FABLECUT_ANALYSIS_DIR", path.join(CACHE_ROOT, "analysis"));
const RESOURCE_CACHE_DIR = userPath("FABLECUT_RESOURCE_CACHE_DIR", path.join(CACHE_ROOT, "resource-packages"));
const LIBRARY_DIR = path.join(DATA_DIR, "library");
const COMPONENTS_DIR = userPath("FABLECUT_COMPONENTS_DIR", path.join(DATA_DIR, "components"));
const PROJECT_FILE = path.join(DATA_DIR, "project.json");
const LIBRARY_SUBDIRS = ["sfx", "elements", "svg", "fonts"];

/* The asset library ships with the repo but users also drop their own files in
   (library/sfx is gitignored precisely for that). When the data dir is split
   off, copy the shipped assets across once so the library is populated, then
   leave it alone — a later update re-copies only what the user deleted or has
   never seen, and never clobbers a file they put there. */
function seedLibrary() {
  if (!SPLIT) return;
  const src = path.join(APP_DIR, "library");
  if (!fs.existsSync(src)) return;
  const walk = (from, to) => {
    fs.mkdirSync(to, { recursive: true });
    for (const e of fs.readdirSync(from, { withFileTypes: true })) {
      const a = path.join(from, e.name), b = path.join(to, e.name);
      if (e.isDirectory()) walk(a, b);
      else if (!fs.existsSync(b)) fs.copyFileSync(a, b);
    }
  };
  walk(src, LIBRARY_DIR);
}

/* Create the writable tree. Safe to call from both servers; whoever runs first
   wins and the other no-ops. */
function ensureDirs() {
  for (const d of [DATA_DIR, CACHE_ROOT, MEDIA_DIR, EXPORTS_DIR, ANALYSIS_DIR, RESOURCE_CACHE_DIR, COMPONENTS_DIR])
    fs.mkdirSync(d, { recursive: true });
  const legacyResourceCache = path.join(DATA_DIR, "resource-cache");
  if (legacyResourceCache !== RESOURCE_CACHE_DIR && fs.existsSync(legacyResourceCache)) {
    const targetHasEntries = fs.readdirSync(RESOURCE_CACHE_DIR).length > 0;
    if (!targetHasEntries) {
      fs.rmSync(RESOURCE_CACHE_DIR, { recursive: true, force: true });
      fs.renameSync(legacyResourceCache, RESOURCE_CACHE_DIR);
      fs.mkdirSync(RESOURCE_CACHE_DIR, { recursive: true });
    }
  }
  for (const d of LIBRARY_SUBDIRS)
    fs.mkdirSync(path.join(LIBRARY_DIR, d), { recursive: true });
  seedLibrary();
}

module.exports = {
  APP_DIR, DATA_DIR, SPLIT,
  CACHE_ROOT, MEDIA_DIR, EXPORTS_DIR, ANALYSIS_DIR, RESOURCE_CACHE_DIR, LIBRARY_DIR, COMPONENTS_DIR, PROJECT_FILE,
  LIBRARY_SUBDIRS, ensureDirs,
};
