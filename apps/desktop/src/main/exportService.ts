import { spawn, type ChildProcess } from 'node:child_process';
import { buildExportCommand, parseFfmpegProgress, type ExportRequest, type ExportProgress } from '@ai-video/media';

export function startExport(request: ExportRequest, onProgress: (progress: ExportProgress) => void): { cancel: () => void; done: Promise<void> } {
  const child: ChildProcess = spawn('ffmpeg', buildExportCommand(request), { stdio: ['ignore', 'ignore', 'pipe'] });
  child.stderr?.on('data', (data) => String(data).split(/\r?\n/).forEach((line) => onProgress(parseFfmpegProgress(line))));
  const done = new Promise<void>((resolve, reject) => child.once('close', (code) => code === 0 ? resolve() : reject(new Error(`ffmpeg exited with ${code}`))));
  return { cancel: () => child.kill('SIGTERM'), done };
}
