find_program(EDWARD_FFMPEG_EXECUTABLE ffmpeg REQUIRED)
set(EDWARD_MEDIA_FIXTURE ${CMAKE_CURRENT_BINARY_DIR}/fixture.mp4)
add_custom_command(OUTPUT ${EDWARD_MEDIA_FIXTURE}
  COMMAND ${EDWARD_FFMPEG_EXECUTABLE} -hide_banner -loglevel error
    -f lavfi -i color=c=black:s=16x16:r=25:d=1
    -c:v libx264 -pix_fmt yuv420p -y ${EDWARD_MEDIA_FIXTURE}
  VERBATIM)
add_custom_target(media_fixture ALL DEPENDS ${EDWARD_MEDIA_FIXTURE})
add_executable(test_media_probe test_media_probe.cpp)
target_link_libraries(test_media_probe PRIVATE edward_media)
add_test(NAME media.media_probe COMMAND test_media_probe)
add_test(NAME media.media_probe_fixture COMMAND test_media_probe ${EDWARD_MEDIA_FIXTURE})

add_executable(test_mlt_adapter test_mlt_adapter.cpp)
target_link_libraries(test_mlt_adapter PRIVATE edward_media)
add_test(NAME media.mlt_adapter COMMAND test_mlt_adapter ${EDWARD_MEDIA_FIXTURE})

add_executable(test_render_graph test_render_graph.cpp)
target_link_libraries(test_render_graph PRIVATE edward_media)
add_test(NAME media.render_graph COMMAND test_render_graph ${EDWARD_MEDIA_FIXTURE})

add_executable(test_export_job test_export_job.cpp)
target_link_libraries(test_export_job PRIVATE edward_media)
add_test(NAME media.export_job COMMAND test_export_job ${EDWARD_MEDIA_FIXTURE} ${CMAKE_CURRENT_BINARY_DIR}/export.mp4)
