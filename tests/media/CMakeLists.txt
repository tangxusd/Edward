find_program(EDWARD_FFMPEG_EXECUTABLE ffmpeg REQUIRED)
set(EDWARD_MEDIA_FIXTURE ${CMAKE_CURRENT_BINARY_DIR}/fixture.mp4)
set(EDWARD_MEDIA_OVERLAY_FIXTURE ${CMAKE_CURRENT_BINARY_DIR}/overlay.mp4)
set(EDWARD_MEDIA_AUDIO_FIXTURE ${CMAKE_CURRENT_BINARY_DIR}/audio-fixture.mp4)
set(EDWARD_MEDIA_ALPHA_FIXTURE ${CMAKE_CURRENT_BINARY_DIR}/alpha-fixture.mov)
add_custom_command(OUTPUT ${EDWARD_MEDIA_FIXTURE}
  COMMAND ${EDWARD_FFMPEG_EXECUTABLE} -hide_banner -loglevel error
    -f lavfi -i color=c=black:s=16x16:r=25:d=1
    -c:v libx264 -pix_fmt yuv420p -y ${EDWARD_MEDIA_FIXTURE}
  VERBATIM)
add_custom_target(media_fixture ALL DEPENDS ${EDWARD_MEDIA_FIXTURE})
add_custom_command(OUTPUT ${EDWARD_MEDIA_OVERLAY_FIXTURE}
  COMMAND ${EDWARD_FFMPEG_EXECUTABLE} -hide_banner -loglevel error
    -f lavfi -i color=c=red:s=16x16:r=25:d=1
    -c:v libx264 -pix_fmt yuv420p -y ${EDWARD_MEDIA_OVERLAY_FIXTURE}
  VERBATIM)
add_custom_target(media_overlay_fixture ALL DEPENDS ${EDWARD_MEDIA_OVERLAY_FIXTURE})
add_custom_command(OUTPUT ${EDWARD_MEDIA_AUDIO_FIXTURE}
  COMMAND ${EDWARD_FFMPEG_EXECUTABLE} -hide_banner -loglevel error
    -f lavfi -i color=c=black:s=16x16:r=25:d=1
    -f lavfi -i sine=frequency=1000:sample_rate=48000:duration=1
    -shortest -c:v libx264 -pix_fmt yuv420p -c:a aac -y ${EDWARD_MEDIA_AUDIO_FIXTURE}
  VERBATIM)
add_custom_target(media_audio_fixture ALL DEPENDS ${EDWARD_MEDIA_AUDIO_FIXTURE})
add_custom_command(OUTPUT ${EDWARD_MEDIA_ALPHA_FIXTURE}
  COMMAND ${EDWARD_FFMPEG_EXECUTABLE} -hide_banner -loglevel error
    -f lavfi -i color=c=black@0.0:s=16x16:r=25:d=1
    -vf format=rgba -c:v prores_ks -profile:v 4 -pix_fmt yuva444p10le -an -y ${EDWARD_MEDIA_ALPHA_FIXTURE}
  VERBATIM)
add_custom_target(media_alpha_fixture ALL DEPENDS ${EDWARD_MEDIA_ALPHA_FIXTURE})
add_executable(test_media_probe test_media_probe.cpp)
target_link_libraries(test_media_probe PRIVATE edward_media)
add_test(NAME media.media_probe COMMAND test_media_probe)
add_test(NAME media.media_probe_fixture COMMAND test_media_probe ${EDWARD_MEDIA_FIXTURE})
add_dependencies(test_media_probe media_alpha_fixture)
add_test(NAME media.media_probe_alpha_fixture COMMAND test_media_probe ${EDWARD_MEDIA_FIXTURE} ${EDWARD_MEDIA_ALPHA_FIXTURE})

add_executable(test_mlt_adapter test_mlt_adapter.cpp)
target_link_libraries(test_mlt_adapter PRIVATE edward_media)
add_dependencies(test_mlt_adapter media_overlay_fixture)
add_test(NAME media.mlt_adapter COMMAND test_mlt_adapter ${EDWARD_MEDIA_FIXTURE} ${EDWARD_MEDIA_OVERLAY_FIXTURE})

add_executable(test_render_graph test_render_graph.cpp)
target_link_libraries(test_render_graph PRIVATE edward_media)
add_dependencies(test_render_graph media_overlay_fixture)
add_test(NAME media.render_graph COMMAND test_render_graph ${EDWARD_MEDIA_FIXTURE} ${EDWARD_MEDIA_OVERLAY_FIXTURE})

add_executable(test_export_job test_export_job.cpp)
target_link_libraries(test_export_job PRIVATE edward_media)
add_dependencies(test_export_job media_audio_fixture)
add_test(NAME media.export_job COMMAND test_export_job ${EDWARD_MEDIA_AUDIO_FIXTURE} ${CMAKE_CURRENT_BINARY_DIR}/export.mp4)

add_executable(test_audio_waveform test_audio_waveform.cpp)
target_link_libraries(test_audio_waveform PRIVATE edward_media)
add_dependencies(test_audio_waveform media_audio_fixture)
add_test(NAME media.audio_waveform COMMAND test_audio_waveform ${EDWARD_MEDIA_AUDIO_FIXTURE} ${EDWARD_MEDIA_FIXTURE})

add_executable(test_audio_preview test_audio_preview.cpp)
target_link_libraries(test_audio_preview PRIVATE edward_media)
add_dependencies(test_audio_preview media_audio_fixture)
add_test(NAME media.audio_preview COMMAND test_audio_preview ${EDWARD_MEDIA_AUDIO_FIXTURE} ${EDWARD_MEDIA_FIXTURE})

add_executable(test_component_renderer test_component_renderer.cpp)
target_link_libraries(test_component_renderer PRIVATE edward_media)
add_test(NAME media.component_renderer COMMAND test_component_renderer)
set_tests_properties(media.component_renderer PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
