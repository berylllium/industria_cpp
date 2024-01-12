cmake_minimum_required(VERSION 3.21)

project(industria)

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
target_link_libraries(industria_standalone_server PRIVATE industria_server)

# Add client executable.
add_executable(industria_client client/main.cpp)
target_link_libraries(industria_client PRIVATE industria_server)
