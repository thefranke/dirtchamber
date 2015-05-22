# Find the Microsoft Kinect SDK

include(FindPackageHandleStandardArgs)

if (CMAKE_CL_64)
    set(LIBPATH "amd64;lib/amd64")
else()
    set(LIBPATH "x86;lib/x86")
endif()

find_library(KinectSDK_LIBRARY
    NAMES Kinect10
    PATHS $ENV{KINECTSDK10_DIR}/lib
    PATH_SUFFIXES ${LIBPATH})
    
find_path(KinectSDK_INCLUDE_DIR
    NAMES NuiApi.h
    PATHS $ENV{KINECTSDK10_DIR}/inc
    PATH_SUFFIXES inc)
    
FIND_PACKAGE_HANDLE_STANDARD_ARGS(KinectSDK 
    DEFAULT_MSG 
    KinectSDK_LIBRARY 
    KinectSDK_INCLUDE_DIR)
