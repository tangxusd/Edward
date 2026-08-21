set(CMAKE_AUTOMOC ON)
add_library(edward_desktop STATIC
  include/edward/desktop/workbench_runtime.hpp
  src/timeline_controller.cpp
  src/workbench_runtime.cpp
)
target_include_directories(edward_desktop PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(edward_desktop PUBLIC edward_core edward_media edward_plugins edward_resources edward_resolve Qt6::Core Qt6::Concurrent)

add_executable(edward_app src/main.cpp)
target_link_libraries(edward_app PRIVATE edward_desktop Qt6::Gui Qt6::Quick Qt6::QuickControls2)
set_target_properties(edward_app PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
qt_add_resources(edward_app desktop_qml
  PREFIX "/qml"
  BASE "${CMAKE_CURRENT_SOURCE_DIR}/qml"
  FILES
    qml/DesignTokens.qml
    qml/EdwardPreview.qml
    qml/EdwardTimeline.qml
    qml/Workbench.qml
    qml/qmldir
)

if(APPLE)
  set_target_properties(edward_app PROPERTIES
    MACOSX_BUNDLE TRUE
    OUTPUT_NAME Edward
    MACOSX_BUNDLE_BUNDLE_NAME Edward
    MACOSX_BUNDLE_GUI_IDENTIFIER com.edward.video
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
  )
endif()
