project(pvr.ksys)

cmake_minimum_required(VERSION 2.6)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR})

enable_language(CXX)

include(FindOpenSSL)
#include(FindCURL)

SET( CMAKE_CXX_FLAGS  "-DANDROID_STL=c++_shared ${CMAKE_CXX_FLAGS}" )

find_package(Kodi REQUIRED)
find_package(kodiplatform REQUIRED)
find_package(p8-platform REQUIRED)
find_package(ZLIB REQUIRED)
find_package(CURL REQUIRED)

include_directories(${kodiplatform_INCLUDE_DIRS}
                    ${p8-platform_INCLUDE_DIRS}
                    ${KODI_INCLUDE_DIR}
                    ${PROJECT_SOURCE_DIR}/lib)

set(DEPLIBS ${p8-platform_LIBRARIES}
            ${ZLIB_LIBRARIES}
            ${CURL_LIBRARIES})

message(STATUS "ZLIB_LIBRARIES: ${ZLIB_LIBRARIES}")
message(STATUS "CURL_LIBRARIES: ${CURL_LIBRARIES}")
message(STATUS "KODI_INCLUDE_DIR: ${KODI_INCLUDE_DIR}")
message(STATUS "p8-platform_INCLUDE_DIRS: ${p8-platform_INCLUDE_DIRS}")
message(STATUS "OPENSSL_FOUND: ${OPENSSL_FOUND}")

set(KSYS_SOURCES src/client.cpp
				 src/PVRKsysAPI.cpp
                 src/PVRIptvData.cpp
                 src/md5.cpp
                 src/PVRRecorder.cpp
                 src/PVRRecorderJob.cpp
                 src/PVRKauth.cpp
                 src/dialogs/GUIDialogPinCode.cpp)

set(KSYS_HEADERS src/client.h
				 src/PVRKsysAPI.h
                 src/PVRIptvData.h
                 src/md5.h
                 src/PVRRecorder.h
                 src/PVRRecorderJob.h
                 src/PVRKauth.h
                 src/dialogs/GUIDialogPinCode.h)

build_addon(pvr.ksys KSYS DEPLIBS)

include(CPack)
