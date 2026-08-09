import { useCallback, useEffect, useRef, useState } from 'react';
import { copyClip, deleteClip, markClipUserEdited, moveClip, resizeClip, type Project, type TimelineClip, type TrackId } from '@ai-video/domain';
import { PIXELS_PER_SECOND, timeToPixel, pixelToTime, getClipColor, generateTickMarks } from './timelineMath.js';

// ---------------------------------------------------------------------------
// Track display mapping — 4 visual tracks matching the approved spec (V3, V2, V1, A1)
// Domain tracks: graphics, cards, subtitles, mainMedia, background
// V3 shows graphics (and subtitles clips merged), V2 shows cards, V1 shows mainMedia, A1 shows background
// ---------------------------------------------------------------------------

const TRACK_DISPLAY: Array<{ id: TrackId; label: string }> = [
  { id: 'graphics', label: 'V3' },
  { id: 'cards', label: 'V2' },
  { id: 'mainMedia', label: 'V1' },
  { id: 'background', label: 'A1' },
];

/** Extra domain tracks we fold into the display (e.g. subtitles → V3) */
const FOLD_TRACKS: Partial<Record<TrackId, TrackId>> = {
  subtitles: 'graphics',
};

/** Total visible timeline duration in seconds (derived from project clips) */
function computeDuration(project: Project): number {
  let maxEnd = 10;
  for (const trackId of Object.keys(project.tracks) as TrackId[]) {
    for (const clip of project.tracks[trackId].clips) {
      maxEnd = Math.max(maxEnd, clip.start + clip.duration);
    }
  }
  return maxEnd + 2; // 2s padding
}

// ---------------------------------------------------------------------------
// Drag state
// ---------------------------------------------------------------------------

type ActiveDrag =
  | { kind: 'idle' }
  | {
      kind: 'move';
      clipId: string;
      sourceTrackId: TrackId;
      startMouseX: number;
      clipStart: number;
    }
  | {
      kind: 'resize-left';
      clipId: string;
      trackId: TrackId;
      startMouseX: number;
      clipStart: number;
      clipDuration: number;
    }
  | {
      kind: 'resize-right';
      clipId: string;
      trackId: TrackId;
      startMouseX: number;
      clipStart: number;
      clipDuration: number;
    };

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/** Find which display track a mouse y falls in, given the track elements */
function trackAtY(
  y: number,
  trackEls: Array<{ id: TrackId; top: number; bottom: number }>,
): { id: TrackId; isOutside: boolean } {
  for (const t of trackEls) {
    if (y >= t.top && y < t.bottom) return { id: t.id, isOutside: false };
  }
  // Below the last track → auto-track zone
  return { id: trackEls[trackEls.length - 1]?.id ?? 'background', isOutside: true };
}

/** Move a clip to a different track */
function moveClipToTrack(
  project: Project,
  clipId: string,
  fromTrack: TrackId,
  toTrack: TrackId,
): Project {
  const clip = project.tracks[fromTrack].clips.find((c) => c.id === clipId);
  if (!clip) return project;
  if (fromTrack === toTrack) return project;

  const fromClips = project.tracks[fromTrack].clips.filter((c) => c.id !== clipId);
  const toClips = [...project.tracks[toTrack].clips, clip];

  return {
    ...project,
    tracks: {
      ...project.tracks,
      [fromTrack]: { ...project.tracks[fromTrack], clips: fromClips },
      [toTrack]: { ...project.tracks[toTrack], clips: toClips },
    },
  };
}

// ---------------------------------------------------------------------------
// Component
// ---------------------------------------------------------------------------

export function Timeline({
  project,
  onChange,
  onSelect,
}: {
  project?: Project;
  onChange?: (project: Project) => void;
  onSelect?: (clipId: string) => void;
}): React.JSX.Element {
  const [draft, setDraft] = useState(project);
  const [drag, setDrag] = useState<ActiveDrag>({ kind: 'idle' });
  const [hoverTargetTrack, setHoverTargetTrack] = useState<TrackId | null>(null);
  const [showAutoTrack, setShowAutoTrack] = useState(false);

  const dragRef = useRef<ActiveDrag>({ kind: 'idle' });
  const draftRef = useRef(project);
  const containerRef = useRef<HTMLDivElement>(null);
  const trackElsRef = useRef<Array<{ id: TrackId; top: number; bottom: number }>>([]);

  useEffect(() => {
    draftRef.current = project;
    setDraft(project);
  }, [project]);

  // Re-read track elements each render (they shift with scroll)
  const updateTrackRects = useCallback(() => {
    if (!containerRef.current) return;
    const containerRect = containerRef.current.getBoundingClientRect();
    const els = containerRef.current.querySelectorAll<HTMLElement>('.timeline-track');
    trackElsRef.current = Array.from(els).map((el) => {
      const rect = el.getBoundingClientRect();
      return {
        id: (el.dataset.trackId ?? 'background') as TrackId,
        top: rect.top - containerRect.top,
        bottom: rect.bottom - containerRect.top,
      };
    });
  }, []);

  // -----------------------------------------------------------------------
  // Global mouse tracking
  // -----------------------------------------------------------------------

  useEffect(() => {
    const move = (event: MouseEvent) => {
      const current = dragRef.current;
      if (current.kind === 'idle') return;
      event.preventDefault();

      const deltaPx = event.clientX - current.startMouseX;
      const deltaSec = pixelToTime(deltaPx);
      const container = containerRef.current;
      if (!container) return;

      if (current.kind === 'move') {
        // Update clip position
        setDraft((prev) => {
          if (!prev) return prev;
          const newStart = Math.max(0, current.clipStart + deltaSec);
          const moved = moveClip(prev, current.clipId, newStart);
          return markClipUserEdited(moved, current.clipId);
        });

        // Detect hovered track for cross-track drag
        updateTrackRects();
        const containerRect = container.getBoundingClientRect();
        const localY = event.clientY - containerRect.top;
        const hit = trackAtY(localY, trackElsRef.current);
        if (hit.isOutside) {
          setShowAutoTrack(true);
          setHoverTargetTrack(null);
        } else {
          setShowAutoTrack(false);
          setHoverTargetTrack(hit.id !== current.sourceTrackId ? hit.id : null);
        }
      } else if (current.kind === 'resize-right') {
        setDraft((prev) => {
          if (!prev) return prev;
          const newDuration = Math.max(0.1, current.clipDuration + deltaSec);
          const resized = resizeClip(prev, current.clipId, newDuration);
          return markClipUserEdited(resized, current.clipId);
        });
      } else if (current.kind === 'resize-left') {
        setDraft((prev) => {
          if (!prev) return prev;
          const newDuration = Math.max(0.1, current.clipDuration - deltaSec);
          const newStart = current.clipStart + (current.clipDuration - newDuration);
          if (newStart < 0) return prev;
          const resized = resizeClip(prev, current.clipId, newDuration);
          const moved = moveClip(resized, current.clipId, newStart);
          return markClipUserEdited(moved, current.clipId);
        });
      }
    };

    const up = () => {
      const current = dragRef.current;
      if (current.kind === 'idle') return;

      // Commit
      if (draftRef.current && onChange) {
        let final = draftRef.current;

        // Cross-track move
        if (current.kind === 'move' && hoverTargetTrack && hoverTargetTrack !== current.sourceTrackId) {
          final = moveClipToTrack(final, current.clipId, current.sourceTrackId, hoverTargetTrack);
        }

        // Auto-track: if dragging outside, move to subtitles track (or the last track)
        if (current.kind === 'move' && showAutoTrack) {
          // Move to the subtitles track (folded into V3 display)
          final = moveClipToTrack(final, current.clipId, current.sourceTrackId, 'subtitles');
        }

        onChange(final);
      }

      dragRef.current = { kind: 'idle' };
      setDrag({ kind: 'idle' });
      setHoverTargetTrack(null);
      setShowAutoTrack(false);
    };

    window.addEventListener('mousemove', move);
    window.addEventListener('mouseup', up);
    return () => {
      window.removeEventListener('mousemove', move);
      window.removeEventListener('mouseup', up);
    };
  }, [onChange, hoverTargetTrack, showAutoTrack, updateTrackRects]);

  // -----------------------------------------------------------------------
  // Clip mutation helpers
  // -----------------------------------------------------------------------

  const mutateClip = (operation: (current: Project) => Project, clipId: string) => {
    if (!draft) return;
    const next = operation(draft);
    setDraft(next);
    onChange?.(next);
  };

  // -----------------------------------------------------------------------
  // Event handlers
  // -----------------------------------------------------------------------

  const handleClipMouseDown = (event: React.MouseEvent, clipId: string, trackId: TrackId, clip: TimelineClip) => {
    event.preventDefault();
    onSelect?.(clip.id);
    const next: ActiveDrag = {
      kind: 'move',
      clipId,
      sourceTrackId: trackId,
      startMouseX: event.clientX,
      clipStart: clip.start,
    };
    dragRef.current = next;
    setDrag(next);
    updateTrackRects();
  };

  const handleTrimLeftMouseDown = (event: React.MouseEvent, clipId: string, trackId: TrackId, clip: TimelineClip) => {
    event.stopPropagation();
    event.preventDefault();
    const next: ActiveDrag = {
      kind: 'resize-left',
      clipId,
      trackId,
      startMouseX: event.clientX,
      clipStart: clip.start,
      clipDuration: clip.duration,
    };
    dragRef.current = next;
    setDrag(next);
  };

  const handleTrimRightMouseDown = (event: React.MouseEvent, clipId: string, trackId: TrackId, clip: TimelineClip) => {
    event.stopPropagation();
    event.preventDefault();
    const next: ActiveDrag = {
      kind: 'resize-right',
      clipId,
      trackId,
      startMouseX: event.clientX,
      clipStart: clip.start,
      clipDuration: clip.duration,
    };
    dragRef.current = next;
    setDrag(next);
  };

  // -----------------------------------------------------------------------
  // Render helpers
  // -----------------------------------------------------------------------

  if (!draft) {
    return (
      <section className="timeline-section" aria-label="多轨时间线">
        <div className="timeline-ruler" />
      </section>
    );
  }

  const duration = computeDuration(draft);
  const totalWidth = timeToPixel(duration);

  // Collect clips for each display track (including folded tracks)
  function clipsForTrack(displayId: TrackId): TimelineClip[] {
    const p = draft!;
    const direct = p.tracks[displayId]?.clips ?? [];
    // Add folded tracks (e.g. subtitles → graphics)
    const folded = (Object.keys(FOLD_TRACKS) as TrackId[])
      .filter((ft) => FOLD_TRACKS[ft] === displayId)
      .flatMap((ft) => p.tracks[ft]?.clips ?? []);
    return [...direct, ...folded].sort((a, b) => a.start - b.start);
  }

  // Generate tick marks for the ruler
  const ticks = generateTickMarks(duration);

  // Current playhead position (show at 2s by default, or at the first clip's start)
  const playheadTime = (() => {
    let firstStart = 2;
    for (const t of TRACK_DISPLAY) {
      for (const clip of clipsForTrack(t.id)) {
        firstStart = Math.min(firstStart, clip.start);
      }
    }
    return firstStart;
  })();

  return (
    <section className="timeline-section" aria-label="多轨时间线" ref={containerRef}>
      {/* Time ruler */}
      <div className="timeline-ruler">
        {ticks.map((tick) => (
          <span
            key={tick.position}
            className={`timeline-ruler-label${tick.isMajor ? ' major' : ''}`}
            style={{ left: tick.position }}
          >
            {tick.label}
          </span>
        ))}
      </div>

      {/* Playhead */}
      <div
        className="timeline-playhead"
        style={{ left: timeToPixel(playheadTime) }}
      />

      {/* Tracks */}
      <div className="timeline-tracks-container">
        {TRACK_DISPLAY.map((displayTrack) => {
          const clips = clipsForTrack(displayTrack.id);
          const isDragOver = hoverTargetTrack === displayTrack.id;

          return (
            <div
              key={displayTrack.id}
              className={`timeline-track${isDragOver ? ' drag-over' : ''}`}
              data-track-id={displayTrack.id}
            >
              <span className="timeline-track-head">{displayTrack.label}</span>
              <div className="timeline-track-body">
                {isDragOver && (
                  <div className="timeline-track-drop-indicator">
                    释放到 {displayTrack.label}
                  </div>
                )}
                {clips.map((clip) => {
                  const clipColor = getClipColor(displayTrack.id);
                  const left = timeToPixel(clip.start);
                  const width = Math.max(
                    20,
                    timeToPixel(clip.duration),
                  );
                  const isSelected = drag.kind !== 'idle' && (drag as any).clipId === clip.id;

                  return (
                    <div
                      key={clip.id}
                      className="timeline-clip"
                      style={{
                        left,
                        width,
                        background: clipColor,
                        opacity: isSelected ? 0.8 : 1,
                        zIndex: isSelected ? 5 : 2,
                      }}
                      onMouseDown={(event) => handleClipMouseDown(event, clip.id, displayTrack.id, clip)}
                    >
                      <span style={{ overflow: 'hidden', textOverflow: 'ellipsis' }}>
                        {clip.id}
                      </span>
                      {/* Trim handles */}
                      <span
                        className="timeline-clip-trim-left"
                        onMouseDown={(event) => handleTrimLeftMouseDown(event, clip.id, displayTrack.id, clip)}
                      />
                      <span
                        className="timeline-clip-trim-right"
                        onMouseDown={(event) => handleTrimRightMouseDown(event, clip.id, displayTrack.id, clip)}
                      />
                    </div>
                  );
                })}
              </div>
            </div>
          );
        })}

        {/* Auto-track placeholder */}
        {showAutoTrack && (
          <div className="timeline-auto-track">
            <span className="timeline-track-head">A2</span>
            <span style={{ marginLeft: 18 }}>松开后新增轨道</span>
          </div>
        )}
      </div>
    </section>
  );
}