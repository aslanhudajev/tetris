cmake_minimum_required(VERSION 3.16)
project(puzzie C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(RAYLIB REQUIRED raylib)

add_executable(puzzie
    src/main.c
    src/game.c
    src/render.c
    src/menu.c
    src/scores.c
    src/theme.c
    src/platform.c
    src/ui.c
)

target_include_directories(puzzie PRIVATE src ${RAYLIB_INCLUDE_DIRS})
target_link_libraries(puzzie ${RAYLIB_LIBRARIES})
target_link_directories(puzzie PRIVATE ${RAYLIB_LIBRARY_DIRS})
target_compile_options(puzzie PRIVATE ${RAYLIB_CFLAGS_OTHER})

if(APPLE)
    set_target_properties(puzzie PROPERTIES
        MACOSX_RPATH ON
    )
    target_link_libraries(puzzie
        "-framework IOKit"
        "-framework Cocoa"
        "-framework OpenGL"
    )
endif()
