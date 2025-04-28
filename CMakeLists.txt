cmake_minimum_required(VERSION 3.10)
project(DeribitTradingSystem VERSION 1.0)

# Specify C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set compiler flags for optimization and warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O3")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
elseif(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /O2")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Od /Zi")
endif()

# Find required packages
find_package(Threads REQUIRED)

# External dependencies (will be downloaded and built)
include(FetchContent)

# Boost for networking and asynchronous operations
find_package(Boost 1.70 COMPONENTS system thread REQUIRED)

# JSON library for API communication
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
)

# WebSocket++ for WebSocket client and server
FetchContent_Declare(
    websocketpp
    GIT_REPOSITORY https://github.com/zaphoyd/websocketpp.git
    GIT_TAG 0.8.2
)

# OpenSSL for secure connections
find_package(OpenSSL REQUIRED)

# Make dependencies available
FetchContent_MakeAvailable(nlohmann_json websocketpp)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${Boost_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
    ${nlohmann_json_SOURCE_DIR}/include
    ${websocketpp_SOURCE_DIR}
)

# Define source files
file(GLOB_RECURSE SOURCES "src/*.cpp")

# Create main executable
add_executable(deribit_trading_system ${SOURCES})

# Link libraries
target_link_libraries(deribit_trading_system
    PRIVATE
    ${Boost_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    Threads::Threads
)

# Add tests directory
add_subdirectory(tests)

# Install targets
install(TARGETS deribit_trading_system
    RUNTIME DESTINATION bin
)