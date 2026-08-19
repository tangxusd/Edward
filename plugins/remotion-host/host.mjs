import fs from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import readline from "node:readline";
import { fileURLToPath } from "node:url";
import { bundle } from "@remotion/bundler";
import { getCompositions, renderFrames } from "@remotion/renderer";

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

async function renderFrame(params) {
  const { compositionId, frame, width, height } = params;
  if (compositionId !== "EdwardAnimation" || !Number.isInteger(frame) || frame < 0 || !Number.isInteger(width) || !Number.isInteger(height) || width < 1 || height < 1) {
    throw new Error("invalid_render_frame_request");
  }
  serveUrl ??= await bundle({ entryPoint });
  const compositions = await getCompositions(serveUrl, { inputProps: {} });
  const composition = compositions.find((item) => item.id === compositionId);
  if (!composition) throw new Error("composition_not_found");
  const outputDirectory = await fs.mkdtemp(path.join(os.tmpdir(), "edward-remotion-"));
  try {
    await renderFrames({ composition: { ...composition, width, height, durationInFrames: Math.max(composition.durationInFrames, frame + 1) }, serveUrl,
      frameRange: [frame, frame], outputDir: outputDirectory, imageFormat: "png", concurrency: 1 });
    const files = await fs.readdir(outputDirectory);
    const imageName = files.find((file) => file.endsWith(".png"));
    if (!imageName) throw new Error("rendered_frame_missing");
    return { frame, pngBase64: (await fs.readFile(path.join(outputDirectory, imageName))).toString("base64") };
  } finally {
    await fs.rm(outputDirectory, { recursive: true, force: true });
  }
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
  throw new Error("unsupported_rpc_method");
}

const input = readline.createInterface({ input: process.stdin, crlfDelay: Infinity });
for await (const line of input) {
  if (!line.trim()) continue;
  let request;
  try { request = JSON.parse(line); process.stdout.write(`${JSON.stringify(await handle(request))}\n`); }
  catch (error) { process.stdout.write(`${JSON.stringify(failure(request?.id ?? "unknown", String(error?.message ?? error)))}\n`); }
}
