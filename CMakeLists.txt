cmake_minimum_required(VERSION 3.20)
project(distributed_rate_limiter VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
FetchContent_Declare(httplib URL https://github.com/yhirose/cpp-httplib/archive/refs/tags/v0.18.3.tar.gz)
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
FetchContent_MakeAvailable(httplib json)

add_library(ratelimiter INTERFACE)
target_include_directories(ratelimiter INTERFACE include)
target_compile_features(ratelimiter INTERFACE cxx_std_20)

add_executable(rate-limiter src/main.cpp)
target_link_libraries(rate-limiter PRIVATE ratelimiter httplib::httplib nlohmann_json::nlohmann_json)

enable_testing()
add_executable(rate-limiter-tests tests/unit/algorithms_test.cpp)
target_link_libraries(rate-limiter-tests PRIVATE ratelimiter)
add_test(NAME algorithms COMMAND rate-limiter-tests)

if(MSVC)
  target_compile_options(rate-limiter PRIVATE /W4 /permissive-)
else()
  target_compile_options(rate-limiter PRIVATE -Wall -Wextra -Wpedantic)
endif()
