#
# GENERAL
#

# Require CMake 2.6 because of the Debian packaging.
cmake_minimum_required (VERSION 2.6)

# uninstall target
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)


#
# SUBDIRECTORIES
#

add_subdirectory (libwebcam)
add_subdirectory (uvcdynctrl)
