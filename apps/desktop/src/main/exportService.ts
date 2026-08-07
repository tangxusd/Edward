import { spawn, type ChildProcess } from 'node:child_process';
import { rename, rm } from 'node:fs/promises';
import { buildExportCommand, calculateExportPercent, createPartialOutputPath, parseFfmpegProgress, type ExportRequest, type ExportProgress } from '@ai-video/media';

export function startExport(request: ExportRequest, onProgress: (progress: ExportProgress) => void): { cancel: () => void; done: Promise<void> } {
  const partialOutput = createPartialOutputPath(request.output);
  const child: ChildProcess = spawn('ffmpeg', buildExportCommand({ ...request, output: partialOutput }), { stdio: ['ignore', 'ignore', 'pipe'] });
  child.stderr?.on('data', (data) => String(data).split(/\r?\n/).forEach((line) => {
    const progress = parseFfmpegProgress(line);
    const percent = progress.timeMs === undefined || request.durationMs === undefined ? undefined : calculateExportPercent(progress.timeMs, request.durationMs);
    onProgress(percent === undefined ? progress : { ...progress, percent });
  }));
  const done = new Promise<void>((resolve, reject) => {
    child.once('error', (error) => reject(error instanceof Error && (error as NodeJS.ErrnoException).code === 'ENOENT' ? new Error('未找到 FFmpeg：请安装媒体运行时后重试') : error));
    child.once('close', (code) => {
    void (async () => {
      if (code === 0) {
        try {
          await rename(partialOutput, request.output);
          resolve();
        } catch (error) {
          await rm(partialOutput, { force: true });
          reject(error);
        }
        return;
      }
      await rm(partialOutput, { force: true });
      reject(new Error(`ffmpeg exited with ${code}`));
    })();
    });
  });
  return { cancel: () => child.kill('SIGTERM'), done };
}
