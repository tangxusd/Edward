add_library(edward_media STATIC
  src/audio_preview.cpp
  src/audio_waveform.cpp
  src/media_probe.cpp
  src/mlt_adapter.cpp
  src/render_graph.cpp
  src/render_storage.cpp
  src/preview_frame_cache.cpp
  src/proxy_manager.cpp
  src/preview_session.cpp
  src/export_job.cpp
)
target_include_directories(edward_media PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(edward_media PUBLIC edward_core edward_media_runtime)
