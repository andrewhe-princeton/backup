# Project
cmake_minimum_required(VERSION 3.13)
project(Outliner)

# Dependences
include(${CMAKE_CURRENT_SOURCE_DIR}/../../scripts/DependencesCMake.txt)

# Pass
add_subdirectory(src)

# Install
install(
  FILES
  include/noelle/core/Outliner.hpp
  include/noelle/core/OutlinerPass.hpp
  DESTINATION 
  include/noelle/core
  )
