#!/bin/sh
#
# This shell script creates a fat XPlane-plugin 
# (see http://www.xsquawkbox.net/xpsdk/mediawiki/BuildInstall#Fat_Plugins)
# for Windows, Linux and MacOS, all in 32 and 64 bits
#
# The script runs on a Linux host and cross-compiles the plugin for
# all the mentioned architectures. It has been tested on a Fedora 22
# x86_64 system.
#
# There is quite a number of prerequisites that must be installed
# in order for this script to run:
#
# Prerequisite to build X-Plane plugins:
#   - X-Plane SDK available at http://www.xsquawkbox.net/xpsdk/mediawiki/Download
#     [Make sure to use a version >=2.1.1 for 64 bit compatibility!,
#      I used 2.1.3. Download it and unzip it in THIS folder,
#      which will actually populate the SDK directory.]
#
# Build system for Linux target:
#   - gcc (tested with 5.1.1)
#   - libudev-devel (x86_64 AND i686)
#     [libudev-devel is now part of systemd-devel]
#
# Build system for Windows32 target:
#   - mingw32 [in Fedora 22, this is split into various
#              packages, like mingw32-gcc, mingw32-cpp,
#              mingw32-binutils etc.]
#   - mingw32-headers
#   - mingw32-crt
#
# Build system for Windows64 target:
#   - mingw64 [see comment above]
#   - mingw64-headers
#   - mingw64-crt
#
# Build system for MACOS target:
#   - osxcross [(https://github.com/tpoechtrager/osxcross) 
#               with OSX SDK installed (see osxcross README file)
#               This will give you a bit of work, since it will
#               recompile the clang compiler on linux. But it 
#               actually works!]
#

# CHANGE THIS: define location and version of OSX SDK installed
# with osxcross. 
OSXSDK=~/src/osxcross/target/SDK/MacOSX10.10.sdk

# echo commands
set -x

# set name and location of output directory
TARGETDIR=target/TeensyPlugin

# create output directory structure
rm -rf $TARGETDIR
mkdir -p $TARGETDIR
mkdir $TARGETDIR/32
mkdir $TARGETDIR/64

# build Linux32 version
make clean
OS=LINUX CFLAGS=-m32 LDFLAGS=-m32 TARGET=$TARGETDIR/32/lin.xpl make

# build Linux64 version
make clean
OS=LINUX CFLAGS=-m64 LDFLAGS=-m64 TARGET=$TARGETDIR/64/lin.xpl make

# build Windows32 version
make clean
OS=WINDOWS CC=i686-w64-mingw32-gcc TARGET=$TARGETDIR/32/win.xpl make

# build Windows64 version
make clean
OS=WINDOWS64 CC=x86_64-w64-mingw32-gcc TARGET=$TARGETDIR/64/win.xpl make

# build hybrid OSX version
make clean
OS=MACOSX CC=o64-clang SDK=$OSXSDK TARGET=$TARGETDIR/mac.xpl make

# clean up
make clean

rm -f target/TeensyPlugin.zip
zip -r target/TeensyPlugin target/TeensyPlugin/*
