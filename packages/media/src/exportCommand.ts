export type ExportRequest = { input: string; output: string; width: number; height: number; transparent: boolean; crf?: number; durationMs?: number; overlays?: Array<{ text: string; start: number; duration: number; color?: string; fontSize?: number }> };

export function createPartialOutputPath(output: string): string {
  const separator = Math.max(output.lastIndexOf('/'), output.lastIndexOf('\\'));
  const extension = output.lastIndexOf('.');
  return extension <= separator ? `${output}.partial` : `${output.slice(0, extension)}.partial${output.slice(extension)}`;
}

export function buildExportCommand(request: ExportRequest): string[] {
  const args = ['-y', '-i', request.input];
  if (request.overlays?.length) {
    const filters = [`scale=${request.width}:${request.height}`, ...request.overlays.map((overlay) => `drawtext=text='${overlay.text.replace(/([\\'])/g, '\\$1')}':fontcolor='${overlay.color ?? '#ffffff'}':fontsize=${overlay.fontSize ?? 48}:x=(w-text_w)/2:y=h*0.8:enable='between(t,${overlay.start},${overlay.start + overlay.duration})'`)].join(',');
    args.push('-filter_complex', `[0:v]${filters}[v]`, '-map', '[v]', '-map', '0:a?');
  } else {
    args.push('-vf', `scale=${request.width}:${request.height}`);
  }
  if (request.transparent) {
    args.push('-c:v', 'prores_ks', '-profile:v', '4444', '-pix_fmt', 'yuva444p10le', '-c:a', 'pcm_s16le');
  } else {
    args.push('-c:v', 'libx265', '-pix_fmt', 'yuv420p', '-tag:v', 'hvc1', '-c:a', 'aac', '-crf', String(request.crf ?? 22));
  }
  args.push(request.output);
  return args;
}

export type ExportProgress = {
  frame?: number;
  time?: string;
  timeMs?: number;
  percent?: number;
  speed?: string;
  status?: 'completed' | 'failed';
  error?: string;
};

export function calculateExportPercent(timeMs: number, durationMs: number): number | undefined {
  if (!Number.isFinite(timeMs) || !Number.isFinite(durationMs) || durationMs <= 0) return undefined;
  return Math.min(99, Math.max(0, Math.floor((timeMs / durationMs) * 100)));
}

export function parseFfmpegProgress(line: string): ExportProgress {
  const result: ExportProgress = {};
  const frame = line.match(/frame=\s*(\d+)/); if (frame) result.frame = Number(frame[1]);
  const time = line.match(/time=(\S+)/);
  if (time) {
    result.time = time[1];
    const [hours, minutes, seconds] = time[1].split(':').map(Number);
    if ([hours, minutes, seconds].every(Number.isFinite)) result.timeMs = Math.round((hours * 3600 + minutes * 60 + seconds) * 1000);
  }
  const speed = line.match(/speed=\s*(\S+)/); if (speed) result.speed = speed[1];
  return result;
}
