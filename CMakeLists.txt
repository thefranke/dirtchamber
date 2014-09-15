project(dirtchamber)

cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

set(CMAKE_MODULE_PATH 
	${CMAKE_SOURCE_DIR}/cmake
	${CMAKE_MODULE_PATH})

find_package(Boost REQUIRED)
find_package(Assimp REQUIRED)
find_package(D3D REQUIRED)
find_package(DXUT REQUIRED)

find_package(OpenCV)
find_package(KinectSDK)
find_package(WMF)

# configure
set(D3D_LIBS 
	${D3D_D3D11_LIBRARY} 
	${D3D_D3DCompiler_LIBRARY}
	${WINDOWS_SDK_USP10_LIBRARY}
	${DXUT_LIBRARIES})

include_directories(
	${Boost_INCLUDE_DIR} 
	${D3D_INCLUDE_DIR} 
    ${WINDOWS_SDK_INCLUDE_DIR}
	${DXUT_INCLUDE_DIRS}
	${Assimp_INCLUDE_DIR}    
	${CMAKE_CURRENT_SOURCE_DIR}/src)
	
if(KINECTSDK_FOUND)
	add_definitions(-D MICROSOFT_KINECT_SDK)
	include_directories(${KinectSDK_INCLUDE_DIR})
endif()

if (OPENCV_FOUND)
    add_definitions(-D OPENCV)
    include_directories(${OpenCV_INCLUDE_DIR})
endif()
    
add_definitions(-D UNICODE)
add_definitions(-D NOMINMAX)
add_definitions(-D _WIN32_WINNT=0x0601)
          
# dune
file(GLOB_RECURSE ext_src src/ext/*)
source_group("Ext" FILES ${ext_src})
file(GLOB dune_src src/dune/* ${ext_src})

option(BUILD_WITH_WMF "Build with Windows Media Foundation support" ON)

set(dune_dir ${CMAKE_CURRENT_SOURCE_DIR}/src/dune)

if (NOT BUILD_WITH_WMF)
	list(REMOVE_ITEM dune_src
		${dune_dir}/video_gbuffer.h
		${dune_dir}/video_gbuffer.cpp)
endif()

if (NOT KINECTSDK_FOUND)
	list(REMOVE_ITEM dune_src 
		${dune_dir}/kinect_gbuffer.cpp
		${dune_dir}/kinect_gbuffer.h)
endif()

if (NOT OPENCV_FOUND)
	list(REMOVE_ITEM dune_src
		${dune_dir}/tracker.cpp
		${dune_dir}/tracker.h)
endif()

list(REMOVE_ITEM dune_src 
		${dune_dir}/sdk_mesh.cpp
		${dune_dir}/sdk_mesh.h)

add_library(dune ${dune_src})

# shader
file(GLOB hlsl_src shader/*.hlsl shader/common.h)
source_group("Shader Files" FILES ${hlsl_src})

# common files
set(common_src src/pppipe.cpp src/pppipe.h src/common_gui.cpp src/common_gui.h src/common_renderer.cpp src/common_renderer.h src/common_dxut.cpp src/common_dxut.h)

# lpv
add_executable(lpv WIN32 src/main_gi.cpp src/skydome.h ${common_src} ${hlsl_src})
set_target_properties (lpv PROPERTIES COMPILE_DEFINITIONS "LPV")
target_link_libraries(lpv 
    dune 
	${D3D_LIBS} 
	${Assimp_LIBRARY}
	comctl32.lib)

if(BUILD_WITH_WMF)
	target_link_libraries(lpv ${WMF_LIBRARIES})
endif()


# vct
add_executable(vct WIN32 src/main_gi.cpp src/skydome.h ${common_src} ${hlsl_src})
target_link_libraries(vct 
    dune 
	${D3D_LIBS} 
	${Assimp_LIBRARY}
	comctl32.lib)

	

if(OPENCV_FOUND)

    # dlpv kinect
	add_executable(delta_lpv WIN32 src/main_gi_mr.cpp ${common_src} ${hlsl_src})
	set_target_properties (delta_lpv PROPERTIES COMPILE_DEFINITIONS "DLPV")
	target_link_libraries(delta_lpv
		dune 
		${D3D_LIBS} 
		${Assimp_LIBRARY}
		${OpenCV_LIBRARIES}
		comctl32.lib)

    # dvct kinect
	add_executable(delta_vct WIN32 src/main_gi_mr.cpp ${common_src} ${hlsl_src})
	target_link_libraries(delta_vct
		dune 
		${D3D_LIBS} 
		${Assimp_LIBRARY}
		${OpenCV_LIBRARIES}
		comctl32.lib)
		
    if(BUILD_WITH_WMF)
        target_link_libraries(delta_vct ${WMF_LIBRARIES})
        target_link_libraries(delta_lpv ${WMF_LIBRARIES})
    endif()
    
    if (KINECTSDK_FOUND)
        target_link_libraries(delta_vct ${KinectSDK_LIBRARY})
        target_link_libraries(delta_lpv ${KinectSDK_LIBRARY})
    endif()
    
endif()