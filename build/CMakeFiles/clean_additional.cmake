# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Release")
  file(REMOVE_RECURSE
  "CMakeFiles\\test_runner_ui_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\test_runner_ui_autogen.dir\\ParseCache.txt"
  "test_runner_ui_autogen"
  )
endif()
