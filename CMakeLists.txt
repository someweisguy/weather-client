cmake_minimum_required(VERSION 3.5)
list(APPEND EXTRA_COMPONENT_DIRS "sensors")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(weather)
