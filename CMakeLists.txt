cmake_minimum_required(VERSION 3.0...3.25)
project(
    LearnWebGPU # name of the project, which will also be the name of the visual studio solution if you use it
    VERSION 0.1.0 # any version number
    LANGUAGES CXX C # programming languages used by the project
)



add_executable(App main.cpp webgpu-utils.h webgpu-utils.cpp)

set_target_properties(App PROPERTIES
    CXX_STANDARD 17
    CXX_EXTENSIONS OFF
    COMPILE_WARNING_AS_ERROR ON
)

if (MSVC)
    target_compile_options(App PRIVATE /W3)
else()
    target_compile_options(App PRIVATE -Wall -Wextra -pedantic)
endif()


add_subdirectory(ext/glfw)
add_subdirectory(ext/webgpu)
add_subdirectory(ext/glfw3webgpu)

target_link_libraries(App PRIVATE webgpu glfw glfw3webgpu)

target_copy_webgpu_binaries(App)

target_compile_definitions(App PRIVATE
    DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
)

# add_custom_command(TARGET App POST_BUILD
    # COMMAND ${CMAKE_COMMAND} -E copy_directory
        # ${CMAKE_SOURCE_DIR}/data
        # $<TARGET_FILE_DIR:App>/data
# )