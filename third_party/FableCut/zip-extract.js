"use strict";

const fs = require("fs");
const path = require("path");
const zlib = require("zlib");
const { safeRelativePath } = require("./resource-cache");

const EOCD = 0x06054b50;
const CENTRAL = 0x02014b50;
const LOCAL = 0x04034b50;

function findEndOfCentralDirectory(buffer) {
  const first = Math.max(0, buffer.length - 0xffff - 22);
  for (let offset = buffer.length - 22; offset >= first; offset--) if (buffer.readUInt32LE(offset) === EOCD) return offset;
  throw new Error("zip_eocd_missing");
}

function readZipBuffer(buffer, { maxFiles = 256, maxEntryBytes = 16 * 1024 * 1024, maxTotalBytes = 64 * 1024 * 1024 } = {}) {
  const eocd = findEndOfCentralDirectory(buffer);
  const entryCount = buffer.readUInt16LE(eocd + 10);
  const centralOffset = buffer.readUInt32LE(eocd + 16);
  if (entryCount > maxFiles || centralOffset >= buffer.length) throw new Error("zip_limit_exceeded");
  let cursor = centralOffset;
  let totalBytes = 0;
  const seen = new Set();
  const files = [];
  for (let index = 0; index < entryCount; index++) {
    if (cursor + 46 > buffer.length || buffer.readUInt32LE(cursor) !== CENTRAL) throw new Error("zip_central_directory_invalid");
    const flags = buffer.readUInt16LE(cursor + 8);
    const method = buffer.readUInt16LE(cursor + 10);
    const compressedSize = buffer.readUInt32LE(cursor + 20);
    const uncompressedSize = buffer.readUInt32LE(cursor + 24);
    const nameLength = buffer.readUInt16LE(cursor + 28);
    const extraLength = buffer.readUInt16LE(cursor + 30);
    const commentLength = buffer.readUInt16LE(cursor + 32);
    const externalAttributes = buffer.readUInt32LE(cursor + 38);
    const localOffset = buffer.readUInt32LE(cursor + 42);
    const end = cursor + 46 + nameLength + extraLength + commentLength;
    if (end > buffer.length || flags & 1 || (method !== 0 && method !== 8)) throw new Error("zip_entry_unsupported");
    const name = buffer.subarray(cursor + 46, cursor + 46 + nameLength).toString("utf8");
    cursor = end;
    const normalizedName = name.endsWith("/") ? name.slice(0, -1) : name;
    const unixType = (externalAttributes >>> 16) & 0o170000;
    if (!safeRelativePath(normalizedName) || unixType === 0o120000 || seen.has(normalizedName)) throw new Error("zip_entry_path_invalid");
    if (name.endsWith("/")) { seen.add(normalizedName); continue; }
    if (uncompressedSize > maxEntryBytes || totalBytes + uncompressedSize > maxTotalBytes) throw new Error("zip_limit_exceeded");
    if (localOffset + 30 > buffer.length || buffer.readUInt32LE(localOffset) !== LOCAL) throw new Error("zip_local_header_invalid");
    const localNameLength = buffer.readUInt16LE(localOffset + 26);
    const localExtraLength = buffer.readUInt16LE(localOffset + 28);
    const dataStart = localOffset + 30 + localNameLength + localExtraLength;
    const dataEnd = dataStart + compressedSize;
    if (dataStart > buffer.length || dataEnd > buffer.length) throw new Error("zip_entry_data_invalid");
    const source = buffer.subarray(dataStart, dataEnd);
    const bytes = method === 0 ? source : zlib.inflateRawSync(source);
    if (bytes.length !== uncompressedSize) throw new Error("zip_entry_size_invalid");
    seen.add(name);
    totalBytes += bytes.length;
    files.push({ name, bytes });
  }
  return files;
}

function extractZipBuffer(buffer, outputDir, options = {}) {
  const files = readZipBuffer(buffer, options);
  for (const { name, bytes } of files) {
    const target = path.join(outputDir, name);
    if (!target.startsWith(outputDir + path.sep)) throw new Error("zip_entry_path_invalid");
    fs.mkdirSync(path.dirname(target), { recursive: true });
    fs.writeFileSync(target, bytes, { flag: "wx" });
  }
  return files.map(({ name }) => name);
}

module.exports = { extractZipBuffer, readZipBuffer };
