"use strict";

const FPS = new Set([24, 25, 30, 50, 60]);

function positiveInt(value, fallback) {
  const n = Number(value);
  return Number.isInteger(n) && n > 0 ? n : fallback;
}

function normalizeOutputSpec(input = {}, project = {}) {
  const width = positiveInt(input.width, positiveInt(project.width, 1280));
  const height = positiveInt(input.height, positiveInt(project.height, 720));
  const fps = positiveInt(input.fps, positiveInt(project.fps, 30));
  if (!FPS.has(fps)) throw new Error(`unsupported export fps: ${fps}`);
  const pixelRatio = Number(input.pixelRatio ?? 1);
  if (!Number.isFinite(pixelRatio) || pixelRatio <= 0) throw new Error("invalid export pixelRatio");
  const crop = input.crop ?? "none";
  if (crop !== "none") throw new Error("implicit export crop is disabled");
  const format = input.format ?? "mp4";
  if (format !== "mp4") throw new Error(`unsupported export format: ${format}`);
  const quality = ["high", "medium", "low"].includes(input.quality) ? input.quality : "high";
  return { width, height, fps, pixelRatio, crop, format, quality };
}

function createExportSnapshot(project) {
  if (!project || typeof project !== "object") throw new Error("project is required");
  return JSON.parse(JSON.stringify(project));
}

async function renderFrameAt(snapshot, frame, outputSpec) {
  if (!Number.isInteger(frame) || frame < 0) throw new Error("frame must be a non-negative integer");
  const spec = normalizeOutputSpec(outputSpec, snapshot);
  if (typeof globalThis.__fablecutRenderFrame !== "function")
    throw new Error("browser compositor is not installed");
  return globalThis.__fablecutRenderFrame(snapshot, frame, spec);
}

module.exports = { normalizeOutputSpec, createExportSnapshot, renderFrameAt };
