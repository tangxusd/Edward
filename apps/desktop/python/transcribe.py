#!/usr/bin/env python3
import json, sys

def main():
    if len(sys.argv) != 2:
        raise SystemExit('usage: transcribe.py <audio-path>')
    from faster_whisper import WhisperModel
    model = WhisperModel('small', device='cpu', compute_type='int8')
    segments, _ = model.transcribe(sys.argv[1], vad_filter=True)
    for segment in segments:
        text = segment.text.strip()
        if text:
            print(json.dumps({'type':'segment','start':segment.start,'end':segment.end,'text':text}, ensure_ascii=False), flush=True)

if __name__ == '__main__': main()
