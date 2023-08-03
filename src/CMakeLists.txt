cmake_minimum_required(VERSION 3.8 FATAL_ERROR)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

add_definitions(-DDWSF_IMGUI)

set(VCT_RENDERER_SOURCES ${PROJECT_SOURCE_DIR}/src/main.cpp)
file(GLOB_RECURSE SHADER_SOURCES ${PROJECT_SOURCE_DIR}/src/*.glsl)

add_executable(VCTRenderer ${VCT_RENDERER_SOURCES}) 
target_link_libraries(VCTRenderer dwSampleFramework)

add_custom_command(TARGET VCTRenderer POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/src/shader $<TARGET_FILE_DIR:VCTRenderer>/shader)
add_custom_command(TARGET VCTRenderer POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/data/mesh $<TARGET_FILE_DIR:VCTRenderer>/mesh)
add_custom_command(TARGET VCTRenderer POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/data/texture $<TARGET_FILE_DIR:VCTRenderer>/texture)

set_property(TARGET VCTRenderer PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/$(Configuration)")