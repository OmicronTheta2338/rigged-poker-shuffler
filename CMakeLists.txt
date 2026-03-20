cmake_minimum_required(VERSION 3.20)
project(rigged-poker-shuffler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(rigged-poker-shuffler
  src/main.cpp
  src/card.cpp
  src/poker_hand.cpp
  src/rigging.cpp
)

target_compile_options(rigged-poker-shuffler PRIVATE -O3)

