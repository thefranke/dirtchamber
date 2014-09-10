# - Find KinectSDK
# Find the Microsoft Kinect SDK
#
#  This module defines the following variables:
#     KINECTSDK_FOUND
#     KINECTSDK_INCLUDE_DIR
#     KINECTSDK_LIBRARY
#

include(FindPackageHandleStandardArgs)

find_path(Assimp_INCLUDE_DIR 
	NAMES assimp.h)

find_library(Assimp_LIBRARY_RELEASE 
	NAMES assimp 
	PATHS ${Assimp_INCLUDE_DIR}/../lib)
	
find_library(Assimp_LIBRARY_DEBUG 
	NAMES assimpD 
	PATHS ${Assimp_INCLUDE_DIR}/../lib)
	
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Assimp 
	DEFAULT_MSG 
	Assimp_INCLUDE_DIR 
	Assimp_LIBRARY_RELEASE 
	Assimp_LIBRARY_DEBUG)
	
if(ASSIMP_FOUND)
	set(Assimp_LIBRARY optimized ${Assimp_LIBRARY_RELEASE} debug ${Assimp_LIBRARY_DEBUG} CACHE STRING "")
	mark_as_advanced(Assimp_LIBRARY_RELEASE Assimp_LIBRARY_DEBUG)
endif()