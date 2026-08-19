import fs from "node:fs/promises";
import path from "node:path";
import readline from "node:readline";
import { fileURLToPath } from "node:url";
import { closeCaptureSession, createCaptureSession, createFileServer, createRenderJob, executeRenderJob, initializeSession } from "@hyperframes/producer";

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

function safeOutputPath(outputPath) {
  if (typeof outputPath !== "string" || !outputPath || path.isAbsolute(outputPath)) return null;
  const resolved = path.resolve(process.cwd(), outputPath);
  const relative = path.relative(process.cwd(), resolved);
  return relative && !relative.startsWith("..") && !path.isAbsolute(relative) ? resolved : null;
}

function validExportRequest(params) {
  return params.compositionId === "EdwardCard" &&
    safeOutputPath(params.outputPath) &&
    Number.isInteger(params.width) && params.width > 0 &&
    Number.isInteger(params.height) && params.height > 0 &&
    Number.isInteger(params.frameCount) && params.frameCount > 0 &&
    Number.isInteger(params.fpsNumerator) && params.fpsNumerator > 0 &&
    Number.isInteger(params.fpsDenominator) && params.fpsDenominator > 0;
}


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

async function renderExport(params) {
  if (!validExportRequest(params)) throw new Error("invalid_render_export_request");
  const outputPath = safeOutputPath(params.outputPath);
  const fps = { num: params.fpsNumerator, den: params.fpsDenominator };
  const duration = params.frameCount * params.fpsDenominator / params.fpsNumerator;
  const renderProject = await fs.mkdtemp(path.join(process.cwd(), ".edward-hyperframes-export-"));
  try {
    const source = await fs.readFile(path.join(projectDir, "index.html"), "utf8");
    const responsiveSource = source
      .replace('data-duration="5"', `data-duration="${duration}"`)
      .replace('data-composition-id="EdwardCard"', `data-composition-id="EdwardCard" data-width="${params.width}" data-height="${params.height}"`);
    await fs.writeFile(path.join(renderProject, "index.html"), responsiveSource);
    await fs.mkdir(path.dirname(outputPath), { recursive: true });
    const job = createRenderJob({
      fps,
      quality: "high",
      format: "mov",
      workers: 1,
      useGpu: false,
      debug: false,
      strictness: "strict",
      entryFile: "index.html",
    });
    await executeRenderJob(job, renderProject, outputPath);
  } finally {
    await fs.rm(renderProject, { recursive: true, force: true });
  }
  const output = await fs.stat(outputPath);
  if (output.size < 1) throw new Error("rendered_export_missing");
  return {
    outputPath: params.outputPath,
    width: params.width,
    height: params.height,
    frameCount: params.frameCount,
    hasAlpha: true,
  };
}

async function handle(request) {
  if (!request || request.jsonrpc !== "2.0" || typeof request.id !== "string" || typeof request.method !== "string" || typeof request.params !== "object") throw new Error("invalid_rpc_request");
  if (request.method === "describe") {
    if (request.params.compositionId !== "EdwardCard") throw new Error("composition_not_found");
    return response(request.id, { compositionId: "EdwardCard", component, editableProps: ["x", "y", "opacity", "title", "fontSize", "borderWidth"] });
  }
  if (request.method === "renderExport") return response(request.id, await renderExport(request.params));
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
