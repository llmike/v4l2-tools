Webcam Library and Tools
========================

Introduction
------------

The webcam-tools package contains the following two components:
- libwebcam    Webcam Library
- uvcdynctrl   Manage dynamic controls in uvcvideo

This README file contains information that is common to all of the above
components. For component specific information please refer to the README
files in the libwebcam/ and uvcdynctrl/ subdirectories.


Versions
--------

Note that the latest versions of the Webcam Library, its API documentation, and
the tools can always be found at the following address:

  http://sourceforge.net/p/libwebcam


Dependencies
------------

The Webcam Tools have the following external dependencies:

- kernel headers (linux/videodev2.h and linux/uvcvideo.h)
- LibXML 2 development files (usually from the libxml2-dev package)
- gengetopt (optional)

gengetopt (optional):

  The uvcdynctrl directory ships with the two files cmdline.[ch]. As long as
  those are present, gnugetopt is not required. If they are missing, the build
  process will try to create them with the help of gnugetopt.

V4L2 2.6.32 (optional):

  To take advantage of the raw/string control support version 2.6.32 of V4L2
  is required. To be more specific: V4L2_CTRL_TYPE_STRING needs to be supported.
  Whether this is the case is determined at build time and if string control
  support is not present in your version of V4L2 the corresponding feature
  in libwebcam is disabled. During the build process one of the following
  messages is displayed indicating the result of the test:

  ** Your V4L2 has V4L2_CTRL_TYPE_STRING support. Enabling raw control support.
  ** Your V4L2 does not have V4L2_CTRL_TYPE_STRING support. Compiling without
     raw control support.

  Note that the uvcvideo version that comes with 2.6.32 does not yet support
  string controls. At the time of this writing support for it is being added
  to the UVC driver, so future versions should support it, therefore allowing
  V4L2 control values to be more than 4 bytes long.


Building
--------

The build system is based on CMake. Because of the way CMake works, Makefiles
do not ship with libwebcam, so you have to have CMake installed. Don't worry,
though, you are likely to have much fewer illegible error messages than with
the previous GNU Automake.

Once you have CMake installed you can build and install it by running the
following commands in the webcam-tools top level source directory:

mkdir build
cd build
cmake ..
make
make install (as root)

You need at least CMake 2.6, so if you are running Ubuntu 8.04 you need to
enable the hardy-backports repository in /etc/apt/sources.list and update
the 'cmake' package.

CMake defaults to installing binaries and libraries to /usr/local. To use a
different install prefix, you can add a command line parameter to 'cmake' like
this:

cmake .. -DCMAKE_INSTALL_PREFIX=/usr

If you prefer to leave the prefix alone, you should make sure that
/usr/local/bin is in your path and LD_LIBRARY_PATH contains the /usr/local/lib
directory.


Questions and feedback
----------------------

Please feel free to post your questions and comments at the sourceforge
project page:

  http://sourceforge.net/p/libwebcam

