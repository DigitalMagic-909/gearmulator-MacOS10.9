cmake_minimum_required(VERSION 3.15)

# build a fat binary that runs on both intel and the new Apple M1 chip
if(APPLE)
	include(source/cmake/xcodeversion.cmake)

	set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "OS X Architectures")

	message("CMAKE_GENERATOR: " ${CMAKE_GENERATOR})
	message("XCODE_VERSION: " ${XCODE_VERSION})

	# Xcode 14+ can not build for anything < High Sierra anymore
	if(CMAKE_GENERATOR STREQUAL Xcode AND XCODE_VERSION VERSION_GREATER_EQUAL 14.0.0)
		set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum OS X deployment version")
	else()
		# 10.12 is now the minimum because we use std::shared_mutex in patch manager
		set(CMAKE_OSX_DEPLOYMENT_TARGET "10.12" CACHE STRING "Minimum OS X deployment version")
	endif()
	message("CMAKE_OSX_DEPLOYMENT_TARGET: " ${CMAKE_OSX_DEPLOYMENT_TARGET})
endif()

# Use VS 2022 toolset (v143) for Windows 7 compatibility. The VS 2026
# default toolset (v150) statically links CRT functions that import Win8+
# APIs (CopyFile2, CreateFile2), preventing DLL load on Win7.
# Override on command line with -T <toolset> if needed.
if(NOT CMAKE_GENERATOR_TOOLSET AND CMAKE_GENERATOR MATCHES "Visual Studio 18")
	set(CMAKE_GENERATOR_TOOLSET "v143")
endif()

project(gearmulator VERSION 2.2.28)

include(source/cmake/base.cmake)

# ----------------- MAC-LEGACY: Intel Macs on OS X 10.9 Mavericks (fork DigitalMagic-909/gearmulator-MacOS10.9)
#   -DCMAKE_OSX_DEPLOYMENT_TARGET=10.9 -DCMAKE_OSX_ARCHITECTURES=x86_64 [-DTUS_X64_MARCH=ivybridge]
# The DSP library needs packaging/macos-legacy/dsp56300-macos-legacy.patch (weak __ulock, guarded QoS).
set(TUS_X64_MARCH "" CACHE STRING "x86_64: optional -march value, e.g. ivybridge (empty = compiler default)")
if(TUS_X64_MARCH AND NOT MSVC)
	add_compile_options(-march=${TUS_X64_MARCH})
endif()
set(TUS_LEGACY_ALIGNED_NEW "")
if(APPLE AND CMAKE_OSX_DEPLOYMENT_TARGET AND CMAKE_OSX_DEPLOYMENT_TARGET VERSION_LESS 10.14)
	# libc++ of macOS < 10.14 has no aligned operator new/delete: our own replacements are linked into every
	# binary below, and an explicit -faligned-allocation keeps Apple Clang from refusing aligned new
	add_compile_options("$<$<COMPILE_LANGUAGE:CXX,OBJCXX>:-faligned-allocation>")
	set(TUS_LEGACY_ALIGNED_NEW ${CMAKE_SOURCE_DIR}/source/macos-legacy/aligned_new_compat.cpp)
endif()
if(APPLE AND CMAKE_OSX_DEPLOYMENT_TARGET AND CMAKE_OSX_DEPLOYMENT_TARGET VERSION_LESS 10.13)
	# std::bad_variant_access (vtable/typeinfo) is in the system libc++ from macOS 10.13 on
	list(APPEND TUS_LEGACY_ALIGNED_NEW ${CMAKE_SOURCE_DIR}/source/macos-legacy/libcxx_legacy_compat.cpp)
endif()
include(CTest)

option(${PROJECT_NAME}_BUILD_JUCEPLUGIN "Build Juce plugin" on)
option(${PROJECT_NAME}_BUILD_JUCEPLUGIN_CLAP "Build CLAP version of Juce plugin" on)

# Steinberg's ASIO SDK cannot be redistributed, so ASIO support is opt-in:
# point this at an extracted asiosdk directory (the one containing "common")
# and the standalones gain ASIO in their audio device list. Windows only;
# ignored everywhere else.
set(${PROJECT_NAME}_ASIO_SDK_PATH "" CACHE PATH "Path to an extracted Steinberg ASIO SDK. Enables ASIO in the standalone audio settings on Windows.")

# ----------------- CPack basic parameters

message("CMAKE_SYSTEM_NAME: " ${CMAKE_SYSTEM_NAME})
message("CMAKE_SYSTEM_PROCESSOR: " ${CMAKE_SYSTEM_PROCESSOR})

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	set(CPACK_SYSTEM_NAME ${CMAKE_SYSTEM_NAME}_${CMAKE_SYSTEM_PROCESSOR})
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
	set(CPACK_SYSTEM_NAME "MacOS")
endif()

if(NOT DEFINED CPACK_SYSTEM_NAME)
	if(WIN32)
		if(CMAKE_CXX_COMPILER_ARCHITECTURE_ID STREQUAL "ARM64")
			set(CPACK_SYSTEM_NAME "winArm64")
		elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
			set(CPACK_SYSTEM_NAME "win64")
		else()
			set(CPACK_SYSTEM_NAME "win32")
		endif()
	elseif(APPLE)
		set(CPACK_SYSTEM_NAME "MacOS")
	else()
		set(CPACK_SYSTEM_NAME "${CMAKE_SYSTEM_NAME}_${CMAKE_SYSTEM_PROCESSOR}")
	endif()
endif()
message("CPACK_SYSTEM_NAME: " ${CPACK_SYSTEM_NAME})

set(CPACK_PACKAGE_CONTACT "The Usual Suspects")
set(CPACK_PACKAGE_VENDOR "The Usual Suspects")
set(CPACK_PACKAGE_NAME "TheUsualSuspects")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "The Usual Suspects emulated audio plugin")

set(CPACK_DEBIAN_PACKAGE_MAINTAINER "The Usual Suspects")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://dsp56300.wordpress.com")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)

set(CPACK_RPM_PACKAGE_AUTOREQ "yes")

# Without the symbol version in what is required of libcurl. Built on Debian or Ubuntu, the plugin
# manager links a libcurl whose symbols are versioned CURL_OPENSSL_4, rpm writes that version into the
# requirement, and the libcurl of Fedora has none - so the package would not install there, although
# the program runs with it (BUG-10507). The requirement of libcurl.so.4 itself stays.
set(CPACK_RPM_SPEC_MORE_DEFINE "%global __requires_exclude ^libcurl[.]so[.]4[(]CURL_")
set(CPACK_RPM_PACKAGE_URL ${CPACK_DEBIAN_PACKAGE_HOMEPAGE})
set(CPACK_RPM_PACKAGE_DESCRIPTION ${CPACK_PACKAGE_DESCRIPTION_SUMMARY})

# ----------------- source

add_subdirectory(source)
add_subdirectory(doc)

# MAC-LEGACY: aligned new/delete and libc++ replacements into each binary (see above)
if(TUS_LEGACY_ALIGNED_NEW)
	foreach(t JE8086TestConsole jeJucePlugin_VST3 jeJucePlugin_AU jeJucePlugin_Standalone)
		if(TARGET ${t})
			target_sources(${t} PRIVATE ${TUS_LEGACY_ALIGNED_NEW})
		endif()
	endforeach()
endif()

# ----------------- CPack parameters based on source

get_cmake_property(CPACK_COMPONENTS_ALL COMPONENTS)
list(REMOVE_ITEM CPACK_COMPONENTS_ALL "Unspecified")

# set better archive names
foreach(comp IN LISTS CPACK_COMPONENTS_ALL)
	string(TOUPPER "${comp}" COMP_UPPER)

	set(archiveName "${CPACK_PACKAGE_NAME}-${comp}-${CMAKE_PROJECT_VERSION}-${CPACK_SYSTEM_NAME}")

	set(varName "CPACK_ARCHIVE_${COMP_UPPER}_FILE_NAME")
	set(${varName} ${archiveName})

	set(varName "CPACK_DEBIAN_${COMP_UPPER}_FILE_NAME")
	set(${varName} "${archiveName}.deb")

	set(varName "CPACK_RPM_${COMP_UPPER}_FILE_NAME")
	set(${varName} "${archiveName}.rpm")
endforeach()

# add list of all targets that we want to upload
get_property(CPACK_TUS_TARGETS GLOBAL PROPERTY "TUS_TARGETS")

# add target properties that we need for CPack / upload
foreach(target IN LISTS CPACK_TUS_TARGETS)
	get_target_property(comp "${target}" TUS_CMAKE_COMPONENT)
	if(comp)
		set(CPACK_TUS_${target}_CMAKE_COMPONENT ${comp})
	endif()
	get_target_property(prodName "${target}" TUS_PRODUCT_NAME)
	if(prodName)
		set(CPACK_TUS_${target}_PRODUCT_NAME ${prodName})
	endif()
endforeach()

set(CPACK_TUS_SOURCE_DIR ${CMAKE_SOURCE_DIR})
set(CPACK_TUS_BIN_DIR ${TUS_BIN_DIR})

set(CPACK_COMPONENTS_GROUPING IGNORE)

set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
set(CPACK_DEB_COMPONENT_INSTALL ON)
set(CPACK_RPM_COMPONENT_INSTALL ON)

if(UNIX AND NOT APPLE)
	set(CPACK_PACKAGING_INSTALL_PREFIX /usr/local)
endif()

include(CPack)
