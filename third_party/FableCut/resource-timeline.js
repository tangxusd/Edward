"use strict";

(function registerResourceTimeline(root) {
  const EPSILON = 1e-4;

  function trackNumber(track) {
    return Number.parseInt(String(track.id || "").slice(1), 10) || 0;
  }

  function canPlace(clips, trackId, start, duration) {
    const end = start + duration;
    return !clips.some((clip) => clip.track === trackId && start < Number(clip.start || 0) + Number(clip.duration || 0) - EPSILON && end > Number(clip.start || 0) + EPSILON);
  }

  function resolveResourceInsertionTrack(tracks, clips, start, duration, maxTracks) {
    const videoTracks = (tracks || []).filter((track) => track.kind === "video").sort((a, b) => trackNumber(a) - trackNumber(b));
    const byId = new Map(videoTracks.map((track) => [track.id, track]));
    const highest = Math.max(0, ...videoTracks.map(trackNumber));
    for (let number = 1; number <= highest + 1; number++) {
      const id = `V${number}`;
      const track = byId.get(id);
      if (!track) {
        if (videoTracks.length >= maxTracks) return { trackId: null, createdTrack: null };
        return { trackId: id, createdTrack: { id, kind: "video" } };
      }
      if (canPlace(clips || [], track.id, start, duration)) return { trackId: track.id, createdTrack: null };
    }
    return { trackId: null, createdTrack: null };
  }

  const api = { resolveResourceInsertionTrack, resolveTextInsertionTrack: resolveResourceInsertionTrack };
  if (typeof module !== "undefined" && module.exports) module.exports = api;
  root.fablecutResourceTimeline = api;
})(typeof window !== "undefined" ? window : globalThis);
