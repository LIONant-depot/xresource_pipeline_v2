# The library
cmake_minimum_required(VERSION 3.10)

# Set policy CMP0169 to OLD to allow FetchContent_Populate
# FetchContent_MakeAvailable will automatically execute root cmakes which will break the build
if(POLICY CMP0169)
  cmake_policy(SET CMP0169 OLD)
endif()

#
# Populate xtextfile
#
FetchContent_Declare(
  xerr
  GIT_REPOSITORY https://github.com/LIONant-depot/xtextfile.git
  GIT_TAG        main
  SOURCE_DIR     "${CMAKE_SOURCE_DIR}/dependencies/xtextfile"  # Adjusted to go up one level
)

FetchContent_GetProperties(xtextfile)
if(NOT xtextfile_POPULATED)
  message(STATUS "Populating xtextfile...")
  FetchContent_Populate(xtextfile)
  
  # Include CMakeLists.txt
  add_subdirectory("${CMAKE_SOURCE_DIR}/dependencies/xtextfile/build/dependency" "${CMAKE_CURRENT_BINARY_DIR}/xtextfile")
endif()


#
# Populate xcmdline
#
FetchContent_Declare(
  xerr
  GIT_REPOSITORY https://github.com/LIONant-depot/xcmdline.git
  GIT_TAG        main
  SOURCE_DIR     "${CMAKE_SOURCE_DIR}/dependencies/xcmdline"  # Adjusted to go up one level
)

FetchContent_GetProperties(xcmdline)
if(NOT xcmdline_POPULATED)
  message(STATUS "Populating xcmdline...")
  FetchContent_Populate(xcmdline)
  
  # Include CMakeLists.txt
  add_subdirectory("${CMAKE_SOURCE_DIR}/dependencies/xcmdline/build/dependency" "${CMAKE_CURRENT_BINARY_DIR}/xcmdline")
endif()

#
# Populate xresource
#
FetchContent_Declare(
  xerr
  GIT_REPOSITORY https://github.com/LIONant-depot/xresource.git
  GIT_TAG        main
  SOURCE_DIR     "${CMAKE_SOURCE_DIR}/dependencies/xresource"  # Adjusted to go up one level
)

FetchContent_GetProperties(xresource)
if(NOT xresource_POPULATED)
  message(STATUS "Populating xresource...")
  FetchContent_Populate(xresource)
  
  # Include CMakeLists.txt
  add_subdirectory("${CMAKE_SOURCE_DIR}/dependencies/xresource/build/dependency" "${CMAKE_CURRENT_BINARY_DIR}/xresource")
endif()

#
# Populate xproperty
#
FetchContent_Declare(
  xerr
  GIT_REPOSITORY https://github.com/LIONant-depot/xproperty.git
  GIT_TAG        main
  SOURCE_DIR     "${CMAKE_SOURCE_DIR}/dependencies/xproperty"  # Adjusted to go up one level
)

FetchContent_GetProperties(xproperty)
if(NOT xproperty_POPULATED)
  message(STATUS "Populating xproperty...")
  FetchContent_Populate(xproperty)
  
  # Include CMakeLists.txt
  add_subdirectory("${CMAKE_SOURCE_DIR}/dependencies/xproperty/build/dependency" "${CMAKE_CURRENT_BINARY_DIR}/xproperty")
endif()


#
# Define as an interface library only if not already defined
#
if(NOT TARGET xresource_pipeline_v2)

  # Define xbitmap as a proper static library
  add_library(xresource_pipeline_v2 INTERFACE)

  # determine the root of our library
  if("${TARGET_PROJECT}" STREQUAL "xresource_pipeline_v2_unit_test")
    set(ROOT ".")
  else()
    set(ROOT "dependencies/xresource_pipeline_v2")

    # set the include path
    set(XRESOURCE_PIPELINE_V2_INCLUDE_LIST "${ROOT}")

  endif()

  # set the files required
  set(XRESOURCE_PIPELINE_V2_FILES_LIST
    "${ROOT}/source/xresource_pipeline_version.h"
    "${ROOT}/source/xresource_pipeline_tag.h"
    "${ROOT}/source/xresource_pipeline_info.h"
    "${ROOT}/source/xresource_pipeline_factory.h"
    "${ROOT}/source/xresource_pipeline_descriptor_base.h"
    "${ROOT}/source/xresource_pipeline_dependencies.h"
    "${ROOT}/source/xresource_pipeline_config.h"
    "${ROOT}/source/xresource_pipeline_compiler_base.h"
    "${ROOT}/source/xresource_pipeline.h"
    "${ROOT}/source/xresource_pipeline.cpp"
  )

  # Set global properties
  set_property(GLOBAL PROPERTY XRESOURCE_PIPELINE_V2_GROUP "dependencies/xcore/xresource_pipeline_v2")
  set_property(GLOBAL PROPERTY XRESOURCE_PIPELINE_V2_FILES "${XRESOURCE_PIPELINE_V2_FILES_LIST}")
  set_property(GLOBAL PROPERTY XRESOURCE_PIPELINE_V2_INCLUDES "${XRESOURCE_PIPELINE_V2_INCLUDE_LIST}")
  set_property(GLOBAL APPEND PROPERTY COMPONENT_REGISTRY "XRESOURCE_PIPELINE_V2")

endif()