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
   gltfLoader.cpp
   utils.cpp
)

list(APPEND sources
    context.h 
	shader.h 
	pipeline.h 
	pass.h 
	managers.h 
	imgui_wrapper.h 
	utils.h 
	renderer.h 
	mesh.h 
	scene.h
	uniformsBuffer.h
	material.h
	attributed.h
	gltfLoader.h
)

add_executable(App ${sources})

set_target_properties(App PROPERTIES
    CXX_STANDARD 17
    CXX_EXTENSIONS OFF
    COMPILE_WARNING_AS_ERROR OFF
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(App PRIVATE -Wall -Wextra -pedantic)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    #   target_compile_options(App PRIVATE -Wall -Wextra -pedantic)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(App PRIVATE /W3)
endif()


if (NOT EMSCRIPTEN)
    add_subdirectory(ext/glfw)
endif()

add_subdirectory(ext/webgpu)
add_subdirectory(ext/glm)
add_subdirectory(ext/imgui)
add_subdirectory(ext/tinygltf)
add_subdirectory(ext/entt)


target_link_libraries(App PRIVATE webgpu glfw glm imgui tinygltf EnTT::EnTT)

if (NOT EMSCRIPTEN)
    target_link_libraries(App PRIVATE dawncpp dawn_utils dawn_glfw)
endif()

if (EMSCRIPTEN)
	 target_link_options(App PUBLIC
        "SHELL: -s USE_GLFW=3"
        "SHELL: -s USE_WEBGPU"
        "SHELL: -s ASYNCIFY=1"
		"SHELL: -s ALLOW_MEMORY_GROWTH"
		"SHELL:--preload-file ${CMAKE_CURRENT_SOURCE_DIR}/data@/data"
    )
	
	set_target_properties(App PROPERTIES SUFFIX ".html")
	
	add_custom_command(
		TARGET App POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			${CMAKE_SOURCE_DIR}/Scripts/launchInChrome.py
			${CMAKE_BINARY_DIR}
)
endif()

 target_compile_definitions(App
    PRIVATE
    DATA_DIR=$<IF:$<STREQUAL:$<PLATFORM_ID>,Emscripten>,"data","${CMAKE_CURRENT_SOURCE_DIR}/data">
)

