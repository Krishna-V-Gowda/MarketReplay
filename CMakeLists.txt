cmake_minimum_required(VERSION 3.20)
project(MarketReplay VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MARKETREPLAY_ENABLE_SANITIZERS "Enable address and undefined-behaviour sanitizers" OFF)

add_library(marketreplay_lib src/itch.cpp)
target_include_directories(marketreplay_lib PUBLIC include)
target_compile_options(marketreplay_lib PRIVATE -Wall -Wextra -Wpedantic -Werror)

if(MARKETREPLAY_ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(marketreplay_lib PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(marketreplay_lib PRIVATE -fsanitize=address,undefined)
endif()

add_executable(marketreplay src/main.cpp)
target_link_libraries(marketreplay PRIVATE marketreplay_lib)
target_compile_options(marketreplay PRIVATE -Wall -Wextra -Wpedantic -Werror)

include(CTest)
if(BUILD_TESTING)
  add_executable(marketreplay_tests tests/test_main.cpp)
  target_link_libraries(marketreplay_tests PRIVATE marketreplay_lib)
  target_compile_options(marketreplay_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
  add_test(NAME marketreplay_tests COMMAND marketreplay_tests)
endif()
