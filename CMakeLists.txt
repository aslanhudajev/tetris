cmake_minimum_required(VERSION 3.16)

# Distributable builds target both Mac architectures and the oldest macOS we
# support. Both must be set before project() or the toolchain is already fixed.
option(METRIS_BUNDLE "Build a self-contained universal .app for distribution" OFF)

if(METRIS_BUNDLE)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

project(metris C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(METRIS_SOURCES
    src/main.c
    src/game.c
    src/render.c
    src/menu.c
    src/scores.c
    src/theme.c
    src/platform.c
    src/ui.c
)

if(METRIS_BUNDLE)
    # Homebrew only ships raylib for the host architecture, so a universal
    # build has to compile it from source. Static, so the app carries no
    # dependency on the machine that built it.
    include(FetchContent)

    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES OFF CACHE BOOL "" FORCE)

    # Do not set CUSTOMIZE_BUILD. It makes raylib generate its SUPPORT_* macros
    # from CMake options whose defaults disagree with the shipped config.h, and
    # SUPPORT_CUSTOM_FRAME_CONTROL comes out ON. That strips SwapScreenBuffer()
    # and PollInputEvents() out of EndDrawing(), so the window opens and then
    # never paints or accepts input. Trimming unused modules is not worth
    # shipping a raylib that behaves differently from the one we develop against.

    FetchContent_Declare(
        raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG 6.0
        GIT_SHALLOW ON
    )
    FetchContent_MakeAvailable(raylib)

    add_executable(metris MACOSX_BUNDLE ${METRIS_SOURCES})
    target_link_libraries(metris raylib)

    set_target_properties(metris PROPERTIES
        OUTPUT_NAME "Metris"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/packaging/Info.plist.in"
        MACOSX_BUNDLE_BUNDLE_NAME "Metris"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.metris.game"
        MACOSX_BUNDLE_BUNDLE_VERSION "1.0"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
    )

    # Assets live in Contents/Resources so the app is one self-contained thing
    # to hand over. platform_asset_path already looks there first.
    add_custom_command(TARGET metris POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            "$<TARGET_BUNDLE_CONTENT_DIR:metris>/Resources/assets"
        COMMAND find "$<TARGET_BUNDLE_CONTENT_DIR:metris>/Resources" -name ".DS_Store" -delete
        COMMENT "Copying assets into the bundle"
    )
else()
    # Fast local build: link the system raylib, host architecture only.
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(RAYLIB REQUIRED raylib)

    add_executable(metris ${METRIS_SOURCES})

    target_include_directories(metris PRIVATE ${RAYLIB_INCLUDE_DIRS})
    target_link_libraries(metris ${RAYLIB_LIBRARIES})
    target_link_directories(metris PRIVATE ${RAYLIB_LIBRARY_DIRS})
    target_compile_options(metris PRIVATE ${RAYLIB_CFLAGS_OTHER})

    set_target_properties(metris PROPERTIES MACOSX_RPATH ON)
endif()

target_include_directories(metris PRIVATE src)

if(APPLE)
    target_link_libraries(metris
        "-framework IOKit"
        "-framework Cocoa"
        "-framework OpenGL"
    )
endif()
