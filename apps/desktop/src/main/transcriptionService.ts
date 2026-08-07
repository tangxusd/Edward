import { spawn } from 'node:child_process';
import { createInterface } from 'node:readline';
import { join } from 'node:path';

export type TranscriptSegment = { start: number; end: number; text: string };

export async function transcribe(mediaPath: string, onSegment?: (segment: TranscriptSegment) => void, pythonExecutable = 'python3'): Promise<TranscriptSegment[]> {
  const script = join(__dirname, '../../python/transcribe.py');
  const process = spawn(pythonExecutable, [script, mediaPath], { stdio: ['ignore', 'pipe', 'pipe'] });
  const segments: TranscriptSegment[] = [];
  const errors: string[] = [];
  const lines = createInterface({ input: process.stdout });
  lines.on('line', (line) => {
    const value = JSON.parse(line) as { type: string } & TranscriptSegment;
    if (value.type === 'segment') { const segment = { start: value.start, end: value.end, text: value.text }; segments.push(segment); onSegment?.(segment); }
  });
  process.stderr.on('data', (chunk) => errors.push(String(chunk)));
  const code = await new Promise<number | null>((resolve, reject) => {
    process.once('error', (error) => reject(error instanceof Error && (error as NodeJS.ErrnoException).code === 'ENOENT' ? new Error('未找到 Python 运行时：请安装本地转写运行时后重试') : error));
    process.once('close', resolve);
  });
  if (code !== 0) throw new Error(errors.join('').trim() || `transcription failed with code ${code}`);
  return segments;
}
