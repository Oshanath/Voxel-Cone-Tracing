cmake_minimum_required(VERSION 3.8 FATAL_ERROR)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

add_definitions(-DDWSF_VULKAN)
add_definitions(-DDWSF_IMGUI)

if(CMAKE_SIZEOF_VOID_P EQUAL 8) 
    message("Using 64-bit glslangValidator")
    set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/Bin/glslangValidator.exe")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    message("Using 32-bit glslangValidator")
    set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/Bin32/glslangValidator.exe")
endif()

set(VCT_RENDERER_SOURCES
    ${PROJECT_SOURCE_DIR}/src/VCTRenderer.cpp
    ${PROJECT_SOURCE_DIR}/src/controls.cpp
    ${PROJECT_SOURCE_DIR}/src/ShadowMap.cpp
    ${PROJECT_SOURCE_DIR}/src/Voxelizer.cpp
    ${PROJECT_SOURCE_DIR}/src/util.cpp
    ${PROJECT_SOURCE_DIR}/src/ComputeVoxelizer.cpp
    ${PROJECT_SOURCE_DIR}/src/GeometryVoxelizer.cpp
    ${PROJECT_SOURCE_DIR}/src/RendererObject.cpp)

set(SHADER_SOURCES 
    ${PROJECT_SOURCE_DIR}/src/shader/mesh.vert 
    ${PROJECT_SOURCE_DIR}/src/shader/mesh.frag
    ${PROJECT_SOURCE_DIR}/src/shader/shadow.vert 
    ${PROJECT_SOURCE_DIR}/src/shader/shadow.frag
    ${PROJECT_SOURCE_DIR}/src/shader/geometry_voxelizer/geometry_voxelizer.vert
    ${PROJECT_SOURCE_DIR}/src/shader/geometry_voxelizer/geometry_voxelizer.geom
    ${PROJECT_SOURCE_DIR}/src/shader/geometry_voxelizer/geometry_voxelizer.frag
    ${PROJECT_SOURCE_DIR}/src/shader/compute_voxelizer_correct_texcoords.comp
    ${PROJECT_SOURCE_DIR}/src/shader/reset_compute_indirect.comp
    ${PROJECT_SOURCE_DIR}/src/shader/large_triangles.comp
    ${PROJECT_SOURCE_DIR}/src/shader/large_triangles_dda.comp
    ${PROJECT_SOURCE_DIR}/src/shader/compute_voxelizer_incorrect_texcoords.comp
    ${PROJECT_SOURCE_DIR}/src/shader/reset.comp
    ${PROJECT_SOURCE_DIR}/src/shader/reset_instance.comp
    ${PROJECT_SOURCE_DIR}/src/shader/voxel_vis.comp
    ${PROJECT_SOURCE_DIR}/src/shader/voxel_vis.vert 
    ${PROJECT_SOURCE_DIR}/src/shader/voxel_vis.frag
    ${PROJECT_SOURCE_DIR}/src/shader/generate_mip_maps.comp)

include_directories(${PROJECT_SOURCE_DIR}/include)

add_executable(VCTRenderer ${VCT_RENDERER_SOURCES} ${SHADER_SOURCES}) 
target_link_libraries(VCTRenderer dwSampleFramework)

foreach(GLSL ${SHADER_SOURCES})
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "${CMAKE_SOURCE_DIR}/bin/shaders/${FILE_NAME}.spv")
    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/bin/shaders"
        COMMAND ${GLSL_VALIDATOR} --target-env vulkan1.2 -V ${GLSL} -o ${SPIRV} -gVS
        DEPENDS ${GLSL})
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(VCTRenderer_Shaders DEPENDS ${SPIRV_BINARY_FILES})
add_dependencies(VCTRenderer VCTRenderer_Shaders)

set_property(TARGET VCTRenderer PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/")