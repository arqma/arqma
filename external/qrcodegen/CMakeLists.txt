cmake_minimum_required(VERSION 3.5)

project(qrcodegen CXX)

add_library(qrcodegen QrCode.cpp)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")

target_include_directories(qrcodegen PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
