cmake_minimum_required(VERSION 3.21)

project(industria)

# CMake Settings.
set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}")
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_BINARY_DIR}")

set(CMAKE_CXX_STANDARD 20)

# Include root directory.
include_directories(.)

# Include simple logger subdirectory.
add_subdirectory(deps/simple-logger)

# Add server ""shared"" library.
set(SERVER_SOURCES
    network/net_message.cpp
)
list(TRANSFORM SERVER_SOURCES PREPEND "server/")

add_library(industria_server STATIC ${SERVER_SOURCES})
target_include_directories(industria_server PUBLIC deps/asio/asio/include PUBLIC simple-logger)

# Add server standalone executable.
add_executable(industria_standalone_server server/main.cpp)
target_link_libraries(industria_standalone_server PUBLIC industria_server PUBLIC simple-logger)

# Add client executable.
add_executable(industria_client
	client/clock.cpp
    client/event.cpp
    client/input.cpp
    client/main.cpp
    client/platform/platform_linux.cpp
	client/renderer/command_buffer.cpp
    client/renderer/device.cpp
	client/renderer/fence.cpp
	client/renderer/pipeline.cpp
	client/renderer/render_pass.cpp
    client/renderer/renderer.cpp
	client/renderer/shader_stage.cpp
	client/renderer/swapchain.cpp
	client/renderer/voxel_shader.cpp
	client/renderer/vulkan_image.cpp
)
target_link_libraries(industria_client PUBLIC industria_server PUBLIC simple-logger)
target_compile_definitions(industria_client PUBLIC VULKAN_HPP_NO_EXCEPTIONS)

if (WIN32)

	target_compile_definitions(industria_client PRIVATE I_ISWIN)

	message(STATUS "Generating client build files specifically for windows.")

	message(STATUS "Attempting to locate Vulkan SDK using manual path...")
	set(VULKAN_SDK "C:/VulkanSDK")
	set(VULKAN_VERSION "1.3.231.1")
	set(VULKAN_PATH "${VULKAN_SDK}/${VULKAN_VERSION}")
	message(STATUS "Using manually specified path: ${VULKAN_PATH}")

	# Check if manually set path exists
	if(NOT EXISTS "${VULKAN_PATH}")
		message("Error: Unable to locate this Vulkan SDK path VULKAN_PATH: ${VULKAN_PATH}, please specify correct path.")
		return()
	endif()

	target_include_directories(industria_client PRIVATE ${VULKAN_PATH}/Include)
	target_link_libraries(industria_client PRIVATE ${VULKAN_PATH}/Lib/vulkan-1.lib)

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

	target_compile_definitions(industria_client PRIVATE I_ISLINUX)

	message(STATUS "Generating build files specifically for linux.")

	target_link_libraries(industria_client PRIVATE vulkan PRIVATE xcb PRIVATE X11-xcb PRIVATE X11)
	
	# Create symlink to assets folder in build path
	if(NOT EXISTS "${CMAKE_BINARY_DIR}/assets")
		message(STATUS "Creating symlink for assets.")
	
		execute_process(
			COMMAND ln -s ${CMAKE_CURRENT_LIST_DIR}/assets ${CMAKE_BINARY_DIR}/assets
		)
	endif()

endif (UNIX)
