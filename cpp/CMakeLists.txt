cmake_minimum_required(VERSION 3.21)

project(VideoCutStudio VERSION 1.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Multimedia MultimediaWidgets Network)

qt_standard_project_setup()

# ---------------------------------------------------------------------------
# Version / update feed
# ---------------------------------------------------------------------------
set(APP_VERSION "${PROJECT_VERSION}")
string(REPLACE "." "," APP_VERSION_COMMA "${APP_VERSION}")
set(APP_MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/assets/app.manifest")
set(APP_ICON "${CMAKE_CURRENT_SOURCE_DIR}/assets/videocut.ico")
if (NOT DEFINED VIDEOCUT_UPDATE_REPO)
    set(VIDEOCUT_UPDATE_REPO "OWNER/REPO")
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/assets/app.rc.in
               ${CMAKE_CURRENT_BINARY_DIR}/app.rc @ONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/setup/setup.rc.in
               ${CMAKE_CURRENT_BINARY_DIR}/setup.rc @ONLY)

qt_add_executable(VideoCutStudio WIN32
    src/main.cpp
    src/Theme.cpp
    src/MediaUtils.cpp
    src/Renderer.cpp
    src/TimelineWidget.cpp
    src/PreviewPanel.cpp
    src/PreviewEngine.cpp
    src/MediaBin.cpp
    src/PropertiesPanel.cpp
    src/ExportDialog.cpp
    src/MainWindow.cpp
    src/Updater.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/app.rc
)

target_include_directories(VideoCutStudio PRIVATE src)

target_compile_definitions(VideoCutStudio PRIVATE
    VIDEOCUT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/.."
    VIDEOCUT_UPDATE_REPO="${VIDEOCUT_UPDATE_REPO}"
    VIDEOCUT_VERSION="${APP_VERSION}"
    QT_NO_CAST_FROM_ASCII_DISABLED
)

target_link_libraries(VideoCutStudio PRIVATE
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::Network
)

if (MSVC)
    target_compile_options(VideoCutStudio PRIVATE /W3 /utf-8)
else()
    target_compile_options(VideoCutStudio PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

# ---------------------------------------------------------------------------
# Custom installer (no MSI): branded setup app + tiny uninstaller
# ---------------------------------------------------------------------------
qt_add_executable(VideoCutSetup WIN32
    setup/SetupMain.cpp
    setup/SetupWindow.cpp
    setup/Payload.cpp
    setup/WinShortcuts.cpp
    setup/setup.qrc
    ${CMAKE_CURRENT_BINARY_DIR}/setup.rc
)
target_include_directories(VideoCutSetup PRIVATE setup)
target_compile_definitions(VideoCutSetup PRIVATE VIDEOCUT_VERSION="${APP_VERSION}")
target_link_libraries(VideoCutSetup PRIVATE Qt6::Widgets)
set_target_properties(VideoCutSetup PROPERTIES OUTPUT_NAME "VideoCutSetup")

add_executable(VideoCutUninstall WIN32 setup/uninstall/UninstallMain.cpp)
target_link_libraries(VideoCutUninstall PRIVATE shell32 advapi32 user32)
set_target_properties(VideoCutUninstall PROPERTIES OUTPUT_NAME "Uninstall")
if (MINGW)
    target_link_options(VideoCutUninstall PRIVATE -static-libgcc -static-libstdc++ -static)
endif()

# Native launcher that carries the whole installer in one file.
add_executable(VideoCutStub WIN32 setup/stub/StubMain.cpp)
target_link_libraries(VideoCutStub PRIVATE shell32)
set_target_properties(VideoCutStub PROPERTIES OUTPUT_NAME "SetupStub")
if (MINGW)
    target_link_options(VideoCutStub PRIVATE -static-libgcc -static-libstdc++ -static)
endif()

# Copy ffmpeg next to the binary so a dev build works out of the box.
add_custom_command(TARGET VideoCutStudio POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:VideoCutStudio>/ffmpeg"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/../vendor/ffmpeg/ffmpeg.exe"
        "$<TARGET_FILE_DIR:VideoCutStudio>/ffmpeg/ffmpeg.exe"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/../vendor/ffmpeg/ffprobe.exe"
        "$<TARGET_FILE_DIR:VideoCutStudio>/ffmpeg/ffprobe.exe"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/../legal"
        "$<TARGET_FILE_DIR:VideoCutStudio>/legal"
    COMMENT "Copying ffmpeg and legal documents next to VideoCutStudio.exe"
)
