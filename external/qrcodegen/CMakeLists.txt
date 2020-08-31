cmake_minimum_required(VERSION 3.12)
project(libqrcodegen)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(qrcodegen STATIC QrCode.cpp)

target_include_directories(qrcodegen PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR})
