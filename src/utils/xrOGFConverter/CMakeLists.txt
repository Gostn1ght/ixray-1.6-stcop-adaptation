project(xrOGFConverter)

# The xray_re core lives with the DCC plugins; build it here when plugins are off
if (NOT TARGET PCore)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../plugins/PCore" "${CMAKE_CURRENT_BINARY_DIR}/PCore")
endif()

add_executable(xrOGFConverter "main.cpp")
target_link_libraries(xrOGFConverter PRIVATE PCore)
target_compile_definitions(xrOGFConverter PRIVATE "$<$<CONFIG:Debug>:DEBUG>")
