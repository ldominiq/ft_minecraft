cmake_minimum_required(VERSION 3.22)
project(ft_minecraft LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
include(FetchContent)

# Glad
FetchContent_Declare(
  glad
  GIT_REPOSITORY https://github.com/ldominiq/GLAD-4.6.git
  GIT_TAG main
)
FetchContent_MakeAvailable(glad)


# GLFW
FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG latest
)
FetchContent_MakeAvailable(glfw)

# GLM
FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG master
)
FetchContent_MakeAvailable(glm)

# FastNoise
FetchContent_Declare(
  fastnoise
  GIT_REPOSITORY https://github.com/Auburn/FastNoiseLite.git
  GIT_TAG master
)
FetchContent_MakeAvailable(fastnoise)

# stb_image
FetchContent_Declare(
  stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
  GIT_TAG master
)
FetchContent_MakeAvailable(stb)

# imgui
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG master
)
FetchContent_MakeAvailable(imgui)

# ZSTD
set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  zstd
  GIT_REPOSITORY https://github.com/facebook/zstd.git
  GIT_TAG v1.5.6   # stable release
  SOURCE_SUBDIR build/cmake
)
FetchContent_MakeAvailable(zstd)

# Freetype
FetchContent_Declare(
  freetype
  GIT_REPOSITORY https://github.com/freetype/freetype.git
  GIT_TAG VER-2-13-3  # latest stable release
)
FetchContent_MakeAvailable(freetype)

# SoLoud
FetchContent_Declare(
  soloud
  GIT_REPOSITORY https://github.com/jarikomppa/soloud.git
  GIT_TAG RELEASE_20200207 # latest stable release
)
FetchContent_MakeAvailable(soloud)

if(ASAN)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -g -O1")
    set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fsanitize=address")
endif()

if(EXISTS ${CMAKE_SOURCE_DIR}/assets)
  file(COPY ${CMAKE_SOURCE_DIR}/assets DESTINATION ${CMAKE_BINARY_DIR})
endif()

if(EXISTS ${CMAKE_SOURCE_DIR}/shaders)
  file(COPY ${CMAKE_SOURCE_DIR}/shaders DESTINATION ${CMAKE_BINARY_DIR})
endif()

if(EXISTS ${CMAKE_SOURCE_DIR}/fonts)
  file(COPY ${CMAKE_SOURCE_DIR}/fonts DESTINATION ${CMAKE_BINARY_DIR})
endif()

# ---------------------------------------------------------------------------
# ImGui integration
# ---------------------------------------------------------------------------
set(IMGUI_DIR   ${imgui_SOURCE_DIR})

set(IMGUI_SOURCES
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_demo.cpp
        ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
        ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

# Create a static library for ImGui
add_library(imgui STATIC ${IMGUI_SOURCES})

target_include_directories(imgui PUBLIC
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
)

# Link ImGui to GLFW (it needs GLFW for its backend)
# Also link against dl on Linux for dynamic loading (glad/glfw handle OpenGL loader)
target_link_libraries(imgui PUBLIC glfw ${CMAKE_DL_LIBS})

# ---------------------------------------------------------------------------
# SoLoud integration
# ---------------------------------------------------------------------------
add_library(soloud_static STATIC
  # Core engine
  ${soloud_SOURCE_DIR}/src/core/soloud.cpp
  ${soloud_SOURCE_DIR}/src/core/soloud_audiosource.cpp
  ${soloud_SOURCE_DIR}/src/core/soloud_bus.cpp
  ${soloud_SOURCE_DIR}/src/core/soloud_core_3d.cpp
  ${soloud_SOURCE_DIR}/src/core/soloud_core_basicops.cpp
  ${soloud_SOURCE_DIR}/src/core/soloud_core_faderops.cpp
  ${soloud_SOURCE_DIR}/src/core/soloud_core_filterops.cpp
  ${soloud_SOURCE_DIR}/src/core/soloud_core_getters.cpp
  ${soloud_SOURCE_DIR}/src/core/soloud_core_setters.cpp
  ${soloud_SOURCE_DIR}/src/core/soloud_core_voicegroup.cpp
  ${soloud_SOURCE_DIR}/src/core/soloud_core_voiceops.cpp
  ${soloud_SOURCE_DIR}/src/core/soloud_fader.cpp
  ${soloud_SOURCE_DIR}/src/core/soloud_fft.cpp
  ${soloud_SOURCE_DIR}/src/core/soloud_fft_lut.cpp
  ${soloud_SOURCE_DIR}/src/core/soloud_file.cpp
  ${soloud_SOURCE_DIR}/src/core/soloud_filter.cpp
  ${soloud_SOURCE_DIR}/src/core/soloud_misc.cpp
  ${soloud_SOURCE_DIR}/src/core/soloud_queue.cpp
  ${soloud_SOURCE_DIR}/src/core/soloud_thread.cpp

  # Audio sources: WAV (+ WavStream/OGG) and Speech/TTS
  ${soloud_SOURCE_DIR}/src/audiosource/wav/soloud_wav.cpp
  ${soloud_SOURCE_DIR}/src/audiosource/wav/soloud_wavstream.cpp
  ${soloud_SOURCE_DIR}/src/audiosource/wav/dr_impl.cpp
  ${soloud_SOURCE_DIR}/src/audiosource/wav/stb_vorbis.c
  ${soloud_SOURCE_DIR}/src/audiosource/speech/darray.cpp
  ${soloud_SOURCE_DIR}/src/audiosource/speech/klatt.cpp
  ${soloud_SOURCE_DIR}/src/audiosource/speech/resonator.cpp
  ${soloud_SOURCE_DIR}/src/audiosource/speech/soloud_speech.cpp
  ${soloud_SOURCE_DIR}/src/audiosource/speech/tts.cpp

  # Backend: MiniAudio (header-only, cross-platform, no extra deps)
  ${soloud_SOURCE_DIR}/src/backend/miniaudio/soloud_miniaudio.cpp
)

target_include_directories(soloud_static PUBLIC
  ${soloud_SOURCE_DIR}/include
)

target_compile_definitions(soloud_static PUBLIC
  WITH_MINIAUDIO
)

# MiniAudio needs threads + (on Linux) dl/m
if(UNIX AND NOT APPLE)
  target_link_libraries(soloud_static PUBLIC pthread dl m)
endif()

# ---------------------------------------------------------------------------
# Copy shaders so they also get added to build directory
# ---------------------------------------------------------------------------

file(GLOB_RECURSE SHADER_FILES
        "${CMAKE_SOURCE_DIR}/shaders/*.vert"
        "${CMAKE_SOURCE_DIR}/shaders/*.frag"
        "${CMAKE_SOURCE_DIR}/shaders/*.glsl"
)

add_custom_target(update_shaders DEPENDS ${SHADER_FILES})

add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/shaders_timestamp
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        ${CMAKE_BINARY_DIR}/shaders
        COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_BINARY_DIR}/shaders_timestamp
        DEPENDS ${SHADER_FILES}
        COMMENT "Force copy shaders if any were modified"
)

add_custom_target(copy_shaders ALL DEPENDS ${CMAKE_BINARY_DIR}/shaders_timestamp)

# ---------------------------------------------------------------------------
# Glad
# ---------------------------------------------------------------------------

add_library(glad STATIC
  ${glad_SOURCE_DIR}/src/glad.c
)

target_include_directories(glad PUBLIC
  ${glad_SOURCE_DIR}/include
)

set_target_properties(glad PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ---------------------------------------------------------------------------
# FFMPEG
# ---------------------------------------------------------------------------

# find_package(PkgConfig REQUIRED)

# pkg_check_modules(FFMPEG
#     libavformat
#     libavcodec
#     libavutil
#     libswscale
# )

# if(NOT FFMPEG_FOUND)
#     message(FATAL_ERROR
#         "FFmpeg not found. Please install:\n"
#         "Ubuntu: sudo apt install libavformat-dev libavcodec-dev libavutil-dev libswscale-dev\n"
#         "Arch: sudo pacman -S ffmpeg\n"
#         "MacOS: brew install ffmpeg"
#     )
# endif()

# ---------------------------------------------------------------------------
# Source & Include setup for Client and Server
# ---------------------------------------------------------------------------

file(GLOB_RECURSE CLIENT_SRC
    "${CMAKE_SOURCE_DIR}/src/client/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/shared/*.cpp"
)

file(GLOB_RECURSE SERVER_SRC
    "${CMAKE_SOURCE_DIR}/src/server/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/shared/*.cpp"
)


# Client executable
add_executable(ft_minecraft ${CLIENT_SRC})
target_include_directories(ft_minecraft PRIVATE
    ${CMAKE_SOURCE_DIR}/include/client
	${CMAKE_SOURCE_DIR}/include/client/characters
	${CMAKE_SOURCE_DIR}/include/client/items
	${CMAKE_SOURCE_DIR}/include/client/menus
    ${CMAKE_SOURCE_DIR}/include/shared
	${CMAKE_SOURCE_DIR}/include/shared/Mobs
	${CMAKE_SOURCE_DIR}/include/shared/Inventories
    ${glm_SOURCE_DIR}
    ${fastnoise_SOURCE_DIR}/Cpp
    ${glad_SOURCE_DIR}/include
    ${stb_SOURCE_DIR}
	${zstd_SOURCE_DIR}/lib
    ${imgui_SOURCE_DIR}
	${freetype_SOURCE_DIR}/include
	# ${FFMPEG_INCLUDE_DIRS}
)

# target_compile_options(ft_minecraft PRIVATE ${FFMPEG_CFLAGS_OTHER})

find_package(OpenGL REQUIRED)

if(NOT WIN32)
    target_link_libraries(ft_minecraft PRIVATE
        dl pthread X11 Xi Xrandr Xxf86vm z
    )
endif()

target_link_libraries(ft_minecraft PRIVATE
    glfw glad imgui OpenGL::GL
	libzstd freetype soloud_static
    # ${FFMPEG_LIBRARIES}
)
add_dependencies(ft_minecraft update_shaders copy_shaders)

# Server executable
add_executable(ft_minecraft_server ${SERVER_SRC})
target_include_directories(ft_minecraft_server PRIVATE
    ${CMAKE_SOURCE_DIR}/include/server
    ${CMAKE_SOURCE_DIR}/include/shared
	${CMAKE_SOURCE_DIR}/include/shared/Mobs
	${CMAKE_SOURCE_DIR}/include/shared/Inventories
    ${glm_SOURCE_DIR}
    ${fastnoise_SOURCE_DIR}/Cpp
	${zstd_SOURCE_DIR}/lib
)
# Should look into restructure stuff so we can remove glad from server
target_link_libraries(ft_minecraft_server PRIVATE
	libzstd
)

if(WIN32)
    # Link Winsock library for networking on Windows
    target_link_libraries(ft_minecraft PRIVATE Ws2_32)
    target_link_libraries(ft_minecraft_server PRIVATE Ws2_32)
endif()

# TODO : LOOK INTO COMPILING WITH FLAGS. ONLY OUR CODE NO LIBS
