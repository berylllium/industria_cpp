cmake_minimum_required(VERSION 3.21)

project(industria)

# CMake Settings.
set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}")
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_BINARY_DIR}")

set(CMAKE_CXX_STANDARD 20)

# Find Vulkan
find_package(Vulkan REQUIRED)

# Include root directory.
include_directories(src/)

# Include simple logger subdirectory.
add_subdirectory(deps/simple-logger)

# Add client executable.
add_executable(industria
    src/clock.cpp
    src/event.cpp
    src/input.cpp
    src/main.cpp
    src/platform/platform_linux.cpp
    src/platform/platform_windows.cpp
    src/renderer/command_buffer.cpp
    src/renderer/device.cpp
    src/renderer/fence.cpp
    src/renderer/pipeline.cpp
    src/renderer/render_pass.cpp
    src/renderer/renderer.cpp
    src/renderer/shader_stage.cpp
    src/renderer/swapchain.cpp
    src/renderer/voxel_shader.cpp
    src/renderer/vulkan_image.cpp
	src/server/network/net_message.cpp
)
target_link_libraries(industria PUBLIC Vulkan::Vulkan PUBLIC simple-logger)
target_include_directories(industria PUBLIC deps/asio/asio/include)
target_compile_definitions(industria PUBLIC VULKAN_HPP_NO_EXCEPTIONS)

if (WIN32)
    target_compile_definitions(industria PRIVATE I_ISWIN)

    message(STATUS "Generating client build files specifically for windows.")

    # Create symlink to assets folder in build path
    if(NOT EXISTS "${CMAKE_BINARY_DIR}/assets")
    	message(STATUS "Creating symlink for assets.")
    
    	file(TO_NATIVE_PATH "${CMAKE_BINARY_DIR}/assets" SYM_SHADER_LINK)
    	file(TO_NATIVE_PATH "${CMAKE_CURRENT_LIST_DIR}/assets" SYM_SHADER_TARGET)
    
    	execute_process(
    		COMMAND cmd.exe /c mklink /J "${SYM_SHADER_LINK}" "${SYM_SHADER_TARGET}"
    	)
    endif()

endif(WIN32)
if (UNIX)
    target_compile_definitions(industria PRIVATE I_ISLINUX)

    message(STATUS "Generating build files specifically for linux.")

    # Create symlink to assets folder in build path
    if(NOT EXISTS "${CMAKE_BINARY_DIR}/assets")
    	message(STATUS "Creating symlink for assets.")
    
    	execute_process(
    		COMMAND ln -s ${CMAKE_CURRENT_LIST_DIR}/assets ${CMAKE_BINARY_DIR}/assets
    	)
    endif()

endif (UNIX)
