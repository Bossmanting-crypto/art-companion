cmake_minimum_required(VERSION 3.20)
project(ArtCompanion LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT WIN32)
    message(FATAL_ERROR "ArtCompanion targets Windows only (Win32 API, GDI+, layered windows).")
endif()

add_executable(ArtCompanion WIN32
    src/main.cpp
    src/TransparentWindow.cpp
    src/InputWatcher.cpp
    src/AnimationController.cpp
)

target_include_directories(ArtCompanion PRIVATE src)

# gdiplus: per-pixel alpha sprite rendering
# user32/gdi32: window + layered window + hooks
# shell32: process/module name lookups
target_link_libraries(ArtCompanion PRIVATE
    gdiplus
    user32
    gdi32
    shell32
    comctl32
)

# Static-link the MSVC/mingw runtime so the .exe is portable to test machines
# without redistributables (optional but convenient for a v1 prototype).
if(MSVC)
    set_property(TARGET ArtCompanion PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set_target_properties(ArtCompanion PROPERTIES
    OUTPUT_NAME "ArtCompanion"
)
