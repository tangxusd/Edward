include_guard(GLOBAL)

find_package(PkgConfig REQUIRED)
find_package(Qt6 6.11.1 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Concurrent)
pkg_check_modules(EDWARD_FFMPEG REQUIRED IMPORTED_TARGET
  libavformat libavcodec libavfilter libavutil libswresample libswscale)
pkg_check_modules(EDWARD_SODIUM REQUIRED IMPORTED_TARGET libsodium)
pkg_check_modules(EDWARD_SDL3 REQUIRED IMPORTED_TARGET sdl3)

find_package(Mlt7 7.40 CONFIG REQUIRED)

if(NOT TARGET Mlt7::mlt)
  message(FATAL_ERROR "Edward requires the Mlt7::mlt target from MLT 7.40.")
endif()

set(EDWARD_MLT_MODULE_DIR "" CACHE PATH
  "Directory containing the MLT runtime modules")

add_library(edward_media_runtime INTERFACE)
target_link_libraries(edward_media_runtime INTERFACE
  Qt6::Core
  Qt6::Gui
  Qt6::Quick
  Qt6::QuickControls2
  Mlt7::mlt
  PkgConfig::EDWARD_FFMPEG
  PkgConfig::EDWARD_SDL3
)

message(STATUS "Edward dependency: FFmpeg ${EDWARD_FFMPEG_VERSION}")
message(STATUS "Edward dependency: MLT ${Mlt7_VERSION}")
message(STATUS "Edward dependency: MLT modules ${EDWARD_MLT_MODULE_DIR}")
