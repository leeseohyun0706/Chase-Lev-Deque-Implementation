cmake_minimum_required(VERSION 4.2)
project(Concal)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_library(Concal STATIC main.cpp
        deque.h++
)
