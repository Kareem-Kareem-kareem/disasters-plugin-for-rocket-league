cmake_minimum_required(VERSION 3.15)
project(RLDisasters)

set(CMAKE_CXX_STANDARD 17)

if(NOT DEFINED BAKKESMOD_SDK)
    set(BAKKESMOD_SDK "$ENV{BAKKESMOD_SDK}" CACHE PATH "Path to BakkesMod SDK")
endif()

if(NOT EXISTS "${BAKKESMOD_SDK}/include/bakkesmod/plugin/bakkesmodplugin.h")
    message(FATAL_ERROR "BakkesMod SDK not found at '${BAKKESMOD_SDK}'.")
endif()

# ── Sources ──────────────────────────────────────────────────────────
add_library(RLDisasters SHARED
    src/RLDisasters.cpp
    ${BAKKESMOD_SDK}/include/IMGUI/imgui.cpp
    ${BAKKESMOD_SDK}/include/IMGUI/imgui_draw.cpp
    ${BAKKESMOD_SDK}/include/IMGUI/imgui_demo.cpp
    ${BAKKESMOD_SDK}/include/IMGUI/imgui_widgets.cpp
    ${BAKKESMOD_SDK}/include/IMGUI/imgui_tables.cpp
)

target_include_directories(RLDisasters PRIVATE
    ${BAKKESMOD_SDK}/include
    ${BAKKESMOD_SDK}/include/IMGUI
    src
)

target_link_directories(RLDisasters PRIVATE
    ${BAKKESMOD_SDK}/lib
)

target_link_libraries(RLDisasters PRIVATE
    pluginsdk
)

if(WIN32)
    set_target_properties(RLDisasters PROPERTIES
        SUFFIX ".dll"
        PREFIX ""
    )
    target_compile_definitions(RLDisasters PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
endif()

if(DEFINED ENV{APPDATA})
    set(BM_PLUGINS "$ENV{APPDATA}/bakkesmod/bakkesmod/plugins")
    set_target_properties(RLDisasters PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${BM_PLUGINS}"
        LIBRARY_OUTPUT_DIRECTORY "${BM_PLUGINS}"
    )
endif()
