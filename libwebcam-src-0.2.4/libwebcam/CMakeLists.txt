#
# GENERAL
#

# Require CMake 2.6 because of the Debian packaging.
cmake_minimum_required (VERSION 2.6)

project (LIBWEBCAM)



#
# TARGETS
#

add_library (webcam SHARED libwebcam.c dynctrl.c)
add_library (webcam_static STATIC libwebcam.c dynctrl.c)

set_target_properties (webcam PROPERTIES VERSION 0.2.4 SOVERSION 0 CLEAN_DIRECT_OUTPUT 1 OUTPUT_NAME "webcam")
set_target_properties (webcam_static PROPERTIES VERSION 0.2.4 SOVERSION 0 CLEAN_DIRECT_OUTPUT 1 OUTPUT_NAME "webcam")



#
# COMPILATION AND LINKING
#

# Require LibXML
#include (FindLibXml2)
include (FindPkgConfig)
pkg_check_modules (LIBXML2 REQUIRED libxml-2.0)

# Includes
include_directories (include ${LIBXML2_INCLUDE_DIRS} ../common/include)

# Libraries
target_link_libraries (webcam ${LIBXML2_LIBRARIES})
target_link_libraries (webcam_static ${LIBXML2_LIBRARIES})

# Check V4L2
try_compile (HAS_V4L2_STRING_CONTROLS ${CMAKE_BINARY_DIR}/ ${CMAKE_CURRENT_SOURCE_DIR}/../common/build/cmake_try_v4l2_ctrl_type_string.c)
if (HAS_V4L2_STRING_CONTROLS)
	message("** Your V4L2 has V4L2_CTRL_TYPE_STRING support. Enabling raw control support.")
	add_definitions (-DENABLE_RAW_CONTROLS)
else (HAS_V4L2_STRING_CONTROLS)
	message("** Your V4L2 does not have V4L2_CTRL_TYPE_STRING support. Compiling without raw control support.")
endif (HAS_V4L2_STRING_CONTROLS)

# Compiler flags
set_target_properties (webcam PROPERTIES
	COMPILE_FLAGS "-Wall"
)

set_target_properties (webcam_static PROPERTIES
	COMPILE_FLAGS "-Wall ${EXTRA_COMPILE_FLAGS}"
)

# create libwebcam.pc (for pkg-config)
execute_process (
	COMMAND			sh -c "sed -e 's_<--PREFIX--!>_${CMAKE_INSTALL_PREFIX}_g' ${CMAKE_CURRENT_SOURCE_DIR}/pkgconfig/libwebcam.pc_base  > ${CMAKE_CURRENT_BINARY_DIR}/libwebcam.pc"
	RESULT_VARIABLE		PC_EXECUTE_RESULT
	ERROR_QUIET
)
 
if (PC_EXECUTE_RESULT)
	message(ERROR " Unable to parse ${CMAKE_CURRENT_SOURCE_DIR}/pkgconfig/libwebcam.pc_base")
else (PC_EXECUTE_RESULT)
	message("** created libwebcam.pc")
endif (PC_EXECUTE_RESULT)



#
# INSTALLATION
#

if (NOT DEFINED CMAKE_INSTALL_LIBDIR)
	if (DEFINED LIB_INSTALL_DIR)
		SET(CMAKE_INSTALL_LIBDIR ${LIB_INSTALL_DIR})
	else (DEFINED LIB_INSTALL_DIR)
		SET(CMAKE_INSTALL_LIBDIR "lib")
	endif (DEFINED LIB_INSTALL_DIR)
endif (NOT DEFINED CMAKE_INSTALL_LIBDIR)

if (${CMAKE_INSTALL_LIBDIR} MATCHES "^/")
	message("** Installation directory for libwebcam: ${CMAKE_INSTALL_LIBDIR}")
else (${CMAKE_INSTALL_LIBDIR} MATCHES "^/")
	message("** Installation directory for libwebcam: ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")
endif (${CMAKE_INSTALL_LIBDIR} MATCHES "^/")
install (
	TARGETS					webcam webcam_static
	LIBRARY DESTINATION		${CMAKE_INSTALL_LIBDIR}
	ARCHIVE DESTINATION		${CMAKE_INSTALL_LIBDIR}
	COMPONENT				LIBWEBCAM
)

# Default permissions for DIRECTORY files: rw-r--r--
# Default permissions for PROGRAMS files:  rwxr-xr-x
# Note that having absolute paths here requires CPACK_SET_DESTDIR to be set to "ON".
install (
	FILES	${CMAKE_CURRENT_BINARY_DIR}/libwebcam.pc
	DESTINATION 	${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

install (
	FILES	../common/include/webcam.h
	DESTINATION 	${CMAKE_INSTALL_PREFIX}/include
)

