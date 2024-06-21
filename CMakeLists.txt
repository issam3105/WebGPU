cmake_minimum_required(VERSION 3.0...3.25)
project(
    IssamWebGPU 
    VERSION 0.1.0 # any version number
    LANGUAGES CXX C 
)

list(APPEND sources
   main.cpp 
   webgpu-utils.cpp
   uniformsBuffer.cpp
)

list(APPEND sources
    tiny_obj_loader.h 
    context.h 
	shader.h 
	pipeline.h 
	pass.h 
	managers.h 
	imgui_wrapper.h 
	utils.h 
	renderer.h 
	mesh.h 
	node.h
	uniformsBuffer.h
	material.h
	attributed.h
)

add_executable(App ${sources})

set_target_properties(App PROPERTIES
    CXX_STANDARD 17
    CXX_EXTENSIONS OFF
    COMPILE_WARNING_AS_ERROR OFF
)

if (MSVC)
    target_compile_options(App PRIVATE /W3)
else()
    target_compile_options(App PRIVATE -Wall -Wextra -pedantic)
endif()


add_subdirectory(ext/glfw)
add_subdirectory(ext/webgpu)
add_subdirectory(ext/glfw3webgpu)
add_subdirectory(ext/glm)
add_subdirectory(ext/imgui)
add_subdirectory(ext/tinygltf)


target_link_libraries(App PRIVATE webgpu glfw glfw3webgpu glm imgui tinygltf)


target_compile_definitions(App PRIVATE
    DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
)

# add_custom_command(TARGET App POST_BUILD
    # COMMAND ${CMAKE_COMMAND} -E copy_directory
        # ${CMAKE_SOURCE_DIR}/data
        # $<TARGET_FILE_DIR:App>/data
# )