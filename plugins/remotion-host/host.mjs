import fs from "node:fs/promises";
import path from "node:path";
import readline from "node:readline";
import { fileURLToPath } from "node:url";
import { bundle } from "@remotion/bundler";
import { getCompositions, renderFrames, renderMedia } from "@remotion/renderer";

const entryPoint = fileURLToPath(new URL("./index.jsx", import.meta.url));
let serveUrl;

const component = {
  version: "1",
  root: {
    id: "root", type: "container", children: [
      { id: "label", type: "text", properties: { text: "代码动画", color: "#f7ffff", fontSize: 32, background: "#08bdcc" }, transform: { x: 0, y: 0 } }
    ]
  }
};

function response(id, result) { return { jsonrpc: "2.0", id, result }; }
function failure(id, message) { return { jsonrpc: "2.0", id, error: { code: "adapter_error", message } }; }

function safeOutputPath(outputPath) {
  if (typeof outputPath !== "string" || !outputPath || path.isAbsolute(outputPath)) return null;
  const resolved = path.resolve(process.cwd(), outputPath);
  const relative = path.relative(process.cwd(), resolved);
  return relative && !relative.startsWith("..") && !path.isAbsolute(relative) ? resolved : null;
}

function validExportRequest(params) {
  return params.compositionId === "EdwardAnimation" &&
    safeOutputPath(params.outputPath) &&
    Number.isInteger(params.width) && params.width > 0 &&
    Number.isInteger(params.height) && params.height > 0 &&
    Number.isInteger(params.frameCount) && params.frameCount > 0 &&
    Number.isInteger(params.fpsNumerator) && params.fpsNumerator > 0 &&
    Number.isInteger(params.fpsDenominator) && params.fpsDenominator > 0;
}

function chromiumExecutable() {
  const executable = process.env.EDWARD_CHROMIUM_EXECUTABLE;
  if (!executable) return undefined;
  if (!path.isAbsolute(executable)) throw new Error("chromium_runtime_unavailable");
  return executable;
}

async function renderFrame(params) {
  const { compositionId, frame, width, height } = params;
  if (compositionId !== "EdwardAnimation" || !Number.isInteger(frame) || frame < 0 || !Number.isInteger(width) || !Number.isInteger(height) || width < 1 || height < 1) {
    throw new Error("invalid_render_frame_request");
  }
  serveUrl ??= await bundle({ entryPoint });
  const compositions = await getCompositions(serveUrl, { inputProps: {}, browserExecutable: chromiumExecutable() });
  const composition = compositions.find((item) => item.id === compositionId);
  if (!composition) throw new Error("composition_not_found");
  const outputDirectory = await fs.mkdtemp(path.join(process.cwd(), ".edward-remotion-"));
  try {
    await renderFrames({ composition: { ...composition, width, height, durationInFrames: Math.max(composition.durationInFrames, frame + 1) }, serveUrl,
      frameRange: [frame, frame], outputDir: outputDirectory, imageFormat: "png", concurrency: 1,
      browserExecutable: chromiumExecutable() });
    const files = await fs.readdir(outputDirectory);
    const imageName = files.find((file) => file.endsWith(".png"));
    if (!imageName) throw new Error("rendered_frame_missing");
    return { frame, pngBase64: (await fs.readFile(path.join(outputDirectory, imageName))).toString("base64") };
  } finally {
    await fs.rm(outputDirectory, { recursive: true, force: true });
  }
}

async function renderExport(params) {
  if (!validExportRequest(params)) throw new Error("invalid_render_export_request");
  const outputLocation = safeOutputPath(params.outputPath);
  serveUrl ??= await bundle({ entryPoint });
  const compositions = await getCompositions(serveUrl, { inputProps: {}, browserExecutable: chromiumExecutable() });
  const composition = compositions.find((item) => item.id === params.compositionId);
  if (!composition) throw new Error("composition_not_found");
  await fs.mkdir(path.dirname(outputLocation), { recursive: true });
  await renderMedia({
    composition: {
      ...composition,
      width: params.width,
      height: params.height,
      fps: params.fpsNumerator / params.fpsDenominator,
      durationInFrames: params.frameCount,
    },
    serveUrl,
    outputLocation,
    codec: "prores",
    proResProfile: "4444",
    pixelFormat: "yuva444p10le",
    imageFormat: "png",
    frameRange: [0, params.frameCount - 1],
    browserExecutable: chromiumExecutable(),
    overwrite: true,
    concurrency: 1,
    inputProps: {},
  });
  const output = await fs.stat(outputLocation);
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
  if (!request || request.jsonrpc !== "2.0" || typeof request.id !== "string" || typeof request.method !== "string" || typeof request.params !== "object") {
    throw new Error("invalid_rpc_request");
  }
  if (request.method === "describe") {
    if (request.params.compositionId !== "EdwardAnimation") throw new Error("composition_not_found");
    return response(request.id, { compositionId: "EdwardAnimation", component, editableProps: ["x", "y", "opacity", "title", "fontSize", "borderWidth"] });
  }
  if (request.method === "renderFrame") return response(request.id, await renderFrame(request.params));
  if (request.method === "renderExport") return response(request.id, await renderExport(request.params));
  throw new Error("unsupported_rpc_method");
}

const input = readline.createInterface({ input: process.stdin, crlfDelay: Infinity });
for await (const line of input) {
  if (!line.trim()) continue;
  let request;
  try { request = JSON.parse(line); process.stdout.write(`${JSON.stringify(await handle(request))}\n`); }
  catch (error) { process.stdout.write(`${JSON.stringify(failure(request?.id ?? "unknown", String(error?.message ?? error)))}\n`); }
}
