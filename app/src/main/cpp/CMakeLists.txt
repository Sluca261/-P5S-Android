cmake_minimum_required(VERSION 3.22.1)
project(p5s_android_native)

add_library(native-lib SHARED native-lib.cpp)

find_library(log-lib log)
find_library(vulkan-lib vulkan)
find_library(android-lib android)

target_link_libraries(native-lib ${log-lib} ${vulkan-lib} ${android-lib})
