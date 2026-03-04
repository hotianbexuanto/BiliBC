cmake_minimum_required(VERSION 3.21)
project(BiliBC VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# --- Find packages via vcpkg ---
find_package(SDL2 CONFIG REQUIRED)
find_package(freetype CONFIG REQUIRED)
find_package(cJSON CONFIG REQUIRED)

# --- mpv: prefer MPV_DEV_DIR, fall back to system ---
if(DEFINED MPV_DEV_DIR)
    set(MPV_INCLUDE_DIR "${MPV_DEV_DIR}/include")
    set(MPV_LIBRARY     "${MPV_DEV_DIR}/lib/mpv.lib")
    set(MPV_DLL         "${MPV_DEV_DIR}/bin/libmpv-2.dll")
    if(NOT EXISTS "${MPV_LIBRARY}")
        message(FATAL_ERROR "mpv.lib not found at ${MPV_LIBRARY}")
    endif()
else()
    # Try to find mpv in standard locations
    find_path(MPV_INCLUDE_DIR NAMES mpv/client.h)
    find_library(MPV_LIBRARY NAMES mpv mpv.dll.lib libmpv.dll.lib)
    if(NOT MPV_INCLUDE_DIR OR NOT MPV_LIBRARY)
        message(FATAL_ERROR
            "mpv not found. Set -DMPV_DEV_DIR=<path-to-mpv-dev> "
            "or install mpv development files.")
    endif()
endif()
message(STATUS "mpv include: ${MPV_INCLUDE_DIR}")
message(STATUS "mpv library: ${MPV_LIBRARY}")

# --- GLAD (embedded) ---
add_library(glad STATIC
    third_party/glad/src/glad.c
)
target_include_directories(glad PUBLIC third_party/glad/include)

# --- yxml (embedded) ---
add_library(yxml STATIC
    third_party/yxml/yxml.c
)
target_include_directories(yxml PUBLIC third_party/yxml)

# --- Main executable ---
set(BILIBC_SOURCES
    src/main.c
    src/player.c
    src/shader.c
    src/danmaku_parser.c
    src/danmaku.c
    src/text.c
    src/text_renderer.c
    src/utf8.c
    src/blur.c
    src/ui.c
    src/i18n.c
    src/config.c
    src/platform/platform_win32.c
)

add_executable(BiliBC WIN32 ${BILIBC_SOURCES})

target_include_directories(BiliBC PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${MPV_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/third_party/glad/include
    ${CMAKE_SOURCE_DIR}/third_party/yxml
)

target_link_libraries(BiliBC PRIVATE
    SDL2::SDL2
    SDL2::SDL2main
    freetype
    cjson
    glad
    yxml
    ${MPV_LIBRARY}
    opengl32
    comdlg32
    shell32
    ole32
)

# Windows manifest for UTF-8 + DPI awareness
if(MSVC)
    target_sources(BiliBC PRIVATE app.manifest)
    target_compile_options(BiliBC PRIVATE /utf-8 /W3)
    target_compile_definitions(BiliBC PRIVATE
        _CRT_SECURE_NO_WARNINGS
        UNICODE
        _UNICODE
    )
endif()

# --- Copy shaders and assets to build output ---
add_custom_command(TARGET BiliBC POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/shaders"
        "$<TARGET_FILE_DIR:BiliBC>/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:BiliBC>/assets"
    COMMENT "Copying shaders and assets to output directory"
)

# Copy mpv DLL if available
if(DEFINED MPV_DLL AND EXISTS "${MPV_DLL}")
    add_custom_command(TARGET BiliBC POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MPV_DLL}" "$<TARGET_FILE_DIR:BiliBC>"
        COMMENT "Copying libmpv-2.dll"
    )
endif()

# --- Install ---
install(TARGETS BiliBC RUNTIME DESTINATION .)
install(DIRECTORY shaders/ DESTINATION shaders)
install(DIRECTORY assets/ DESTINATION assets)
if(DEFINED MPV_DLL AND EXISTS "${MPV_DLL}")
    install(FILES "${MPV_DLL}" DESTINATION .)
endif()
