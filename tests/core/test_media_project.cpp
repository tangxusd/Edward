#include <edward/core/media_project.hpp>

#include <cassert>

int main() {
  edward::core::MediaProject project(300);
  const auto track = project.addVideoTrack();
  assert(project.videoTrackCount() == 1);

  assert(!project.insertClip(track, edward::core::MediaClip{1, {}, 0, 30, 0}));
  assert(!project.insertClip(track, edward::core::MediaClip{1, "clip.mp4", 30, 30, 0}));
  assert(!project.insertClip(track, edward::core::MediaClip{1, "clip.mp4", 0, 301, 0}));
  assert(project.insertClip(track, edward::core::MediaClip{1, "clip.mp4", 0, 30, 0}));
  assert(project.clipCount(track) == 1);
  assert(!project.insertClip(track, edward::core::MediaClip{1, "other.mp4", 0, 30, 30}));
  return 0;
}
