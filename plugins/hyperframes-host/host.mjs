import fs from "node:fs/promises";
import path from "node:path";
import readline from "node:readline";
import { fileURLToPath } from "node:url";
import { closeCaptureSession, createCaptureSession, createFileServer, initializeSession } from "@hyperframes/producer";

const projectDir = fileURLToPath(new URL("./composition", import.meta.url));
const adapterDir = fileURLToPath(new URL(".", import.meta.url));
let server;
let session;
let sessionSize;
const runtimeSource = await fs.readFile(path.join(adapterDir, "node_modules/@hyperframes/producer/dist/hyperframe.runtime.iife.js"), "utf8");
const gsapSource = await fs.readFile(path.join(adapterDir, "node_modules/gsap/dist/gsap.min.js"), "utf8");
const component = { version: "1", root: { id: "root", type: "container", children: [{ id: "card", type: "shape", properties: { color: "#151515", borderColor: "#ff9966", borderWidth: 3 }, transform: { x: 0, y: 0, width: 520, height: 320 } }] } };

function response(id, result) { return { jsonrpc: "2.0", id, result }; }
function failure(id, message) { return { jsonrpc: "2.0", id, error: { code: "adapter_error", message } }; }

async function ensureSession(width, height) {
  if (session && sessionSize?.width === width && sessionSize?.height === height) return;
  if (session) { await closeCaptureSession(session); session = undefined; }
  server ??= await createFileServer({ projectDir, headScripts: [gsapSource, runtimeSource] });
  session = await createCaptureSession(server.url, projectDir, {
    width, height, fps: { num: 30, den: 1 }, format: "png", captureBeyondViewport: true,
    config: { forceScreenshot: true },
  });
  await initializeSession(session);
  sessionSize = { width, height };
}

async function handle(request) {
  if (!request || request.jsonrpc !== "2.0" || typeof request.id !== "string" || typeof request.method !== "string" || typeof request.params !== "object") throw new Error("invalid_rpc_request");
  if (request.method === "describe") {
    if (request.params.compositionId !== "EdwardCard") throw new Error("composition_not_found");
    return response(request.id, { compositionId: "EdwardCard", component, editableProps: ["x", "y", "opacity", "title", "fontSize", "borderWidth"] });
  }
  if (request.method !== "renderFrame") throw new Error("unsupported_rpc_method");
  const { compositionId, frame, width, height } = request.params;
  if (compositionId !== "EdwardCard" || !Number.isInteger(frame) || frame < 0 || !Number.isInteger(width) || !Number.isInteger(height) || width < 1 || height < 1) throw new Error("invalid_render_frame_request");
  await ensureSession(width, height);
  await session.page.evaluate((time) => window.__hf?.seek?.(time), frame / 30);
  const png = await session.page.screenshot({ type: "png", omitBackground: true });
  return response(request.id, { frame, pngBase64: png.toString("base64") });
}

const input = readline.createInterface({ input: process.stdin, crlfDelay: Infinity });
for await (const line of input) {
  if (!line.trim()) continue;
  let request;
  try { request = JSON.parse(line); process.stdout.write(`${JSON.stringify(await handle(request))}\n`); }
  catch (error) { process.stdout.write(`${JSON.stringify(failure(request?.id ?? "unknown", String(error?.message ?? error)))}\n`); }
}
if (session) await closeCaptureSession(session);
if (server) server.close();
