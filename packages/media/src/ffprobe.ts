import { execFile } from 'node:child_process';
import { promisify } from 'node:util';

const execFileAsync = promisify(execFile);
export type MediaInfo = { kind: 'audio' | 'video'; durationMs: number; width?: number; height?: number; frameRate?: number; hasAudio: boolean };

export async function probeMedia(path: string): Promise<MediaInfo> {
  const { stdout } = await execFileAsync('ffprobe', ['-v','error','-show_streams','-show_format','-of','json',path]);
  const data = JSON.parse(stdout) as { streams: Array<{ codec_type: string; width?: number; height?: number; r_frame_rate?: string }>; format: { duration?: string } };
  const video = data.streams.find((stream) => stream.codec_type === 'video');
  const frameRate = video?.r_frame_rate ? (() => { const [a,b] = video.r_frame_rate.split('/').map(Number); return b ? a / b : a; })() : undefined;
  return { kind: video ? 'video' : 'audio', durationMs: Math.round(Number(data.format.duration ?? 0) * 1000), width: video?.width, height: video?.height, frameRate, hasAudio: data.streams.some((stream) => stream.codec_type === 'audio') };
}
