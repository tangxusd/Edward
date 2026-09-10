set(CMAKE_AUTOMOC ON)
add_library(edward_desktop STATIC
  include/edward/desktop/workbench_runtime.hpp
  include/edward/desktop/diagnostics_reporter.hpp
  src/diagnostics_reporter.cpp
  src/timeline_controller.cpp
  src/workbench_runtime.cpp
)
target_include_directories(edward_desktop PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(edward_desktop PUBLIC edward_core edward_media edward_plugins edward_resources edward_resolve Qt6::Core Qt6::Concurrent Qt6::Widgets)
target_compile_definitions(edward_desktop PRIVATE EDWARD_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

# Qt SVG Image 不支持 CSS currentColor；为 Rail 选中态生成同源橙色 SVG，
# 保持原始 SVG 作为未选中态资源，不引入运行时滤镜或额外图形依赖。
set(EDWARD_RAIL_ICON_NAMES home media text audio card chart background annotation number)
set(EDWARD_RAIL_ACTIVE_ICON_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated-rail-icons")
file(MAKE_DIRECTORY "${EDWARD_RAIL_ACTIVE_ICON_DIR}")
foreach(icon_name IN LISTS EDWARD_RAIL_ICON_NAMES)
  file(READ "${CMAKE_CURRENT_SOURCE_DIR}/qml/icons/${icon_name}.svg" icon_svg)
  string(REPLACE "#a6a6aa" "#ff6e00" icon_svg "${icon_svg}")
  file(WRITE "${EDWARD_RAIL_ACTIVE_ICON_DIR}/${icon_name}-active.svg" "${icon_svg}")
endforeach()

add_executable(edward_app src/main.cpp)
target_compile_definitions(edward_app PRIVATE EDWARD_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(edward_app PRIVATE edward_desktop Qt6::Gui Qt6::Quick Qt6::QuickControls2 Qt6::Svg Qt6::Widgets Qt6::WebEngineQuick)
set_target_properties(edward_app PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
add_custom_command(TARGET edward_app POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_CURRENT_SOURCE_DIR}/../resources/data/resolve-api"
          "$<TARGET_FILE_DIR:edward_app>/../Resources/resolve-api"
  VERBATIM)
add_custom_command(TARGET edward_app POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_CURRENT_SOURCE_DIR}/../resolve/workflow-plugin"
          "$<TARGET_FILE_DIR:edward_app>/../Resources/workflow-plugin"
          VERBATIM)
add_custom_command(TARGET edward_app POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory
          "$<TARGET_FILE_DIR:edward_app>/../Resources/resolve-ograf"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_CURRENT_SOURCE_DIR}/../resolve/ograf/ograf_renderer.js"
          "$<TARGET_FILE_DIR:edward_app>/../Resources/resolve-ograf/ograf_renderer.js"
  VERBATIM)
qt_add_resources(edward_app desktop_qml
  PREFIX "/qml"
  BASE "${CMAKE_CURRENT_SOURCE_DIR}/qml"
  FILES
    qml/DesignTokens.qml
    qml/Workbench.qml
    qml/qmldir
    ${CMAKE_SOURCE_DIR}/video-editor-logo-indigo-pink.svg
    ${CMAKE_SOURCE_DIR}/video-editor-logo-indigo-pink.png
    ${CMAKE_SOURCE_DIR}/video-editor-logo-indigo-pink-rendered.png
    ${CMAKE_SOURCE_DIR}/video-editor-logo-indigo-pink-rendered-4x.png
    qml/icons/home.svg
    qml/icons/media.svg
    qml/icons/text.svg
    qml/icons/audio.svg
    qml/icons/card.svg
    qml/icons/chart.svg
    qml/icons/background.svg
    qml/icons/annotation.svg
    qml/icons/number.svg
    qml/icons/ai.svg
    qml/icons/settings.svg
    qml/icons/microphone.svg
    qml/icons/send.svg
    qml/icons/plus.svg
    qml/icons/plus-visible.svg
    qml/icons/microphone-visible.svg
    qml/icons/send-visible.svg
    qml/icons/attachment.svg
    qml/icons/action-split.svg
    qml/icons/action-delete.svg
    qml/icons/action-ripple.svg
    qml/icons/action-subtitle.svg
    qml/icons/action-fade-in.svg
    qml/icons/action-fade-out.svg
    qml/icons/action-keep-range.svg
    qml/icons/action-marker.svg
    qml/icons/action-color.svg
    qml/icons/action-io.svg
)

qt_add_resources(edward_app desktop_rail_active_icons
  PREFIX "/qml"
  BASE "${EDWARD_RAIL_ACTIVE_ICON_DIR}"
  FILES
    ${EDWARD_RAIL_ACTIVE_ICON_DIR}/home-active.svg
    ${EDWARD_RAIL_ACTIVE_ICON_DIR}/media-active.svg
    ${EDWARD_RAIL_ACTIVE_ICON_DIR}/text-active.svg
    ${EDWARD_RAIL_ACTIVE_ICON_DIR}/audio-active.svg
    ${EDWARD_RAIL_ACTIVE_ICON_DIR}/card-active.svg
    ${EDWARD_RAIL_ACTIVE_ICON_DIR}/chart-active.svg
    ${EDWARD_RAIL_ACTIVE_ICON_DIR}/background-active.svg
    ${EDWARD_RAIL_ACTIVE_ICON_DIR}/annotation-active.svg
    ${EDWARD_RAIL_ACTIVE_ICON_DIR}/number-active.svg
)

if(APPLE)
  execute_process(
    COMMAND qtpaths6 -query QT_INSTALL_PLUGINS
    OUTPUT_VARIABLE EDWARD_QT_PLUGIN_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
  if(EXISTS "${EDWARD_QT_PLUGIN_DIR}/imageformats/libqsvg.dylib")
    add_custom_command(TARGET edward_app POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory
              "$<TARGET_FILE_DIR:edward_app>/../PlugIns/imageformats"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              "${EDWARD_QT_PLUGIN_DIR}/imageformats/libqsvg.dylib"
              "$<TARGET_FILE_DIR:edward_app>/../PlugIns/imageformats/libqsvg.dylib"
      COMMAND ${CMAKE_COMMAND} -E remove
              "$<TARGET_FILE_DIR:edward_app>/../PlugIns/imageformats/._libqsvg.dylib"
              "$<TARGET_FILE_DIR:edward_app>/../PlugIns/._imageformats"
      VERBATIM)
  endif()
  set_target_properties(edward_app PROPERTIES
    MACOSX_BUNDLE TRUE
    OUTPUT_NAME Edward
    MACOSX_BUNDLE_BUNDLE_NAME Edward
    MACOSX_BUNDLE_GUI_IDENTIFIER com.edward.video
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
  )
endif()
