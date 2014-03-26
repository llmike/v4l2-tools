#
# GENERAL
#

# Require CMake 2.6 because of the Debian packaging.
cmake_minimum_required (VERSION 2.6)

project (UVCDYNCTRL)



#
# TARGETS
#

add_executable (uvcdynctrl main.c controls.c cmdline.c getopt.c getopt1.c)

set_target_properties (uvcdynctrl PROPERTIES VERSION 0.2.4)

if (NOT DYNCTRL_DATA_DIR)
	set_target_properties (uvcdynctrl PROPERTIES DATA_DIR "${CMAKE_INSTALL_PREFIX}/share/uvcdynctrl")
else (NOT DYNCTRL_DATA_DIR)
	set_target_properties (uvcdynctrl PROPERTIES DATA_DIR ${DYNCTRL_DATA_DIR})
endif (NOT DYNCTRL_DATA_DIR)



#
# SPECIAL FILES
#

# gengetopt for command line parsing
if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmdline.c" AND NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmdline.h")
	message("** Command line parser source files not found. Creating gengetopt rules ...")

	# Check if gengetopt is installed
	find_program(GENGETOPT gengetopt)
	if (NOT GENGETOPT)
		message(FATAL_ERROR "Unable to find gengetopt")
	endif (NOT GENGETOPT)

	add_custom_command (
		OUTPUT		${CMAKE_CURRENT_SOURCE_DIR}/cmdline.c ${CMAKE_CURRENT_SOURCE_DIR}/cmdline.h
		COMMAND		gengetopt -i ${CMAKE_CURRENT_SOURCE_DIR}/uvcdynctrl.ggo --output-dir ${CMAKE_CURRENT_SOURCE_DIR} --unamed-opts=VALUES
		DEPENDS 	${CMAKE_CURRENT_SOURCE_DIR}/uvcdynctrl.ggo
		COMMENT		"Generating getopt parser code (cmdline.*) ..."
		VERBATIM
	)
endif (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmdline.c" AND NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmdline.h")

# config.h
# Note how we define HAVE_CONFIG_H. This will let gengetopt pick up
# the VERSION constant from config.h.
get_target_property (UVCDYNCTRL_VERSION uvcdynctrl VERSION)
get_target_property (UVCDYNCTRL_DATA_DIR uvcdynctrl DATA_DIR)
configure_file (${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)
add_definitions (-DHAVE_CONFIG_H)

#create man pages
execute_process (
	COMMAND			sh -c "rm -f ${CMAKE_CURRENT_BINARY_DIR}/*.gz; cp ${CMAKE_CURRENT_SOURCE_DIR}/uvcdynctrl.1_ ${CMAKE_CURRENT_BINARY_DIR}/uvcdynctrl.1; cp ${CMAKE_CURRENT_SOURCE_DIR}/uvcdynctrl.1_ ${CMAKE_CURRENT_BINARY_DIR}/uvcdynctrl-${UVCDYNCTRL_VERSION}.1; gzip --best ${CMAKE_CURRENT_BINARY_DIR}/uvcdynctrl-${UVCDYNCTRL_VERSION}.1; gzip --best ${CMAKE_CURRENT_BINARY_DIR}/uvcdynctrl.1;"
	RESULT_VARIABLE		MAN_BUILD_RESULT
	
)

if (MAN_BUILD_RESULT)
	message(ERROR " Unable to build man pages")
else (MAN_BUILD_RESULT)
	message("** created man pages")
endif (MAN_BUILD_RESULT)

#
# COMPILATION AND LINKING
#

include_directories (../common/include)
include_directories (${CMAKE_CURRENT_BINARY_DIR})
link_directories (${LIBWEBCAM_BINARY_DIR}/webcam)

target_link_libraries (uvcdynctrl webcam)

# Compiler flags
set_target_properties (uvcdynctrl PROPERTIES
	COMPILE_FLAGS "-Wall"
)

#
# INSTALLATION
#

# uvcdynctrl binary
message("** Installation directory for uvcdynctrl: ${CMAKE_INSTALL_PREFIX}/bin")
install (
	TARGETS					uvcdynctrl
	RUNTIME DESTINATION		bin
	COMPONENT				UVCDYNCTRL
)

# udev rules, script, and data
# Default permissions for DIRECTORY files: rw-r--r--
# Default permissions for PROGRAMS files:  rwxr-xr-x
# Note that having absolute paths here requires CPACK_SET_DESTDIR to be set to "ON".
install (
	FILES	udev/rules/80-uvcdynctrl.rules
	DESTINATION /lib/udev/rules.d
)
install (
	PROGRAMS	udev/scripts/uvcdynctrl
	DESTINATION	/lib/udev
)
install (
	DIRECTORY	data
	DESTINATION ${UVCDYNCTRL_DATA_DIR}
	PATTERN ".git" EXCLUDE
)
install (
	FILES	${CMAKE_CURRENT_BINARY_DIR}/uvcdynctrl.1.gz ${CMAKE_CURRENT_BINARY_DIR}/uvcdynctrl-${UVCDYNCTRL_VERSION}.1.gz 
	DESTINATION share/man/man1
)

