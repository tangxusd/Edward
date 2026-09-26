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
add_executable(test_preview_session test_preview_session.cpp)
target_link_libraries(test_preview_session PRIVATE edward_media)
add_test(NAME media.preview_session COMMAND test_preview_session ${EDWARD_MEDIA_FIXTURE})
add_executable(test_loop_animation_evaluator test_loop_animation_evaluator.cpp)
target_link_libraries(test_loop_animation_evaluator PRIVATE edward_media)
add_test(NAME media.loop_animation_evaluator COMMAND test_loop_animation_evaluator)
