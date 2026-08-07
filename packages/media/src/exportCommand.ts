export type ExportRequest = { input: string; output: string; width: number; height: number; transparent: boolean; crf?: number };

export function buildExportCommand(request: ExportRequest): string[] {
  const args = ['-y', '-i', request.input, '-vf', `scale=${request.width}:${request.height}`];
  if (request.transparent) args.push('-c:v', 'libx265', '-pix_fmt', 'yuva420p', '-tag:v', 'hvc1');
  else args.push('-c:v', 'libx265', '-pix_fmt', 'yuv420p', '-tag:v', 'hvc1', '-c:a', 'aac');
  args.push('-crf', String(request.crf ?? 22), request.output);
  return args;
}

export type ExportProgress = {
  frame?: number;
  time?: string;
  speed?: string;
  status?: 'completed' | 'failed';
  error?: string;
};
export function parseFfmpegProgress(line: string): ExportProgress {
  const result: ExportProgress = {};
  const frame = line.match(/frame=\s*(\d+)/); if (frame) result.frame = Number(frame[1]);
  const time = line.match(/time=(\S+)/); if (time) result.time = time[1];
  const speed = line.match(/speed=\s*(\S+)/); if (speed) result.speed = speed[1];
  return result;
}
