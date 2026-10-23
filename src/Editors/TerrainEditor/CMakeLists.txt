project(TerrainEditor)

# Files import
file(GLOB_RECURSE ETE_SOURCE_ALL_FILES CONFIGURE_DEPENDS
    "*.*"
)

set(ETE_SRC_FILES)

source_group("src" FILES ${ETE_SOURCE_ALL_FILES})

# Apply list
list(APPEND ETE_SRC_FILES ${ETE_SOURCE_ALL_FILES})

# TerrainEditor project
add_executable(TerrainEditor WIN32 ${ETE_SRC_FILES})
target_include_directories(TerrainEditor PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(TerrainEditor PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../xrCore/")
target_precompile_headers(TerrainEditor PRIVATE "stdafx.h")

# Project defines
target_compile_definitions(TerrainEditor PRIVATE _TERRAIN_EDITOR)
target_compile_definitions(TerrainEditor PRIVATE _WINDOWS)
target_compile_definitions(TerrainEditor PRIVATE _USRDLL)
target_compile_definitions(TerrainEditor PUBLIC XR_EDITOR_NEW)

add_compile_options(/fp:fast)
target_compile_options(TerrainEditor PRIVATE /W0)
target_compile_definitions(TerrainEditor PRIVATE "$<$<CONFIG:Debug>:DEBUG>")

# Linker list
target_link_libraries(TerrainEditor PUBLIC xrCore)
target_link_libraries(TerrainEditor PUBLIC xrEUI)
target_link_libraries(TerrainEditor PUBLIC xrEProps)
target_link_libraries(TerrainEditor PUBLIC xrECore)

# Move precompiled header
set_target_properties(TerrainEditor PROPERTIES DISABLE_PRECOMPILE_HEADERS ON)

file(GLOB_RECURSE ETE_SOURCE_PCH_FILES "stdafx.*")
source_group("pch" FILES ${ETE_SOURCE_PCH_FILES})
