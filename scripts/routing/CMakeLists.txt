cmake_minimum_required(VERSION 3.10)
project(valhalla_routing)

enable_language(C CXX ASM)

# General options
option(SINGLE_LIBRARY "Compile as single library" OFF)

# Directories (mirroring scripts/build/CMakeLists.txt)
set(SDK_BASE_DIR "${PROJECT_SOURCE_DIR}/../..")
set(SDK_EXTERNAL_LIBS_DIR "${SDK_BASE_DIR}/libs-external")
set(SDK_CARTO_LIBS_DIR "${SDK_BASE_DIR}/libs-carto")
set(ROUTING_SRC_DIR "${SDK_BASE_DIR}/routing-lib")

# Library type
if(ANDROID)
  set(ROUTING_LIBRARY_TYPE SHARED)
else()
  set(ROUTING_LIBRARY_TYPE STATIC)
endif()

# C++ compiler flags
add_definitions("${SDK_CPP_DEFINES}" "-DBOOST_ALL_NO_LIB" "-DHAVE_ZSTD")

if(CMAKE_BUILD_TYPE MATCHES "Release|RELEASE")
  add_definitions("-DNDEBUG")
endif()


if(APPLE)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fobjc-arc -fmodules -fvisibility=hidden -ffunction-sections -fdata-sections")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fobjc-arc -fmodules -stdlib=libc++ -std=c++17 -ftemplate-depth=1024 -fexceptions -frtti -fvisibility=hidden -fvisibility-inlines-hidden -ffunction-sections -fdata-sections -fno-aligned-allocation -Wno-unqualified-std-cast-call -Wno-shorten-64-to-32 -Wconfig-macros")
  set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Oz")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Oz")
  set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -ObjC")

  set(CMAKE_XCODE_ATTRIBUTE_CLANG_ENABLE_CODE_COVERAGE NO)
  set(CMAKE_XCODE_ATTRIBUTE_GCC_INSTRUMENT_PROGRAM_FLOW_ARCS NO)

  if(SDK_DEV_TEAM)
    set(CMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM ${SDK_DEV_TEAM})
  endif()

  if(CMAKE_BUILD_TYPE MATCHES "Release|RELEASE")
    set(CMAKE_XCODE_ATTRIBUTE_DEPLOYMENT_POSTPROCESSING YES)
    set(CMAKE_XCODE_ATTRIBUTE_DEAD_CODE_STRIPPING YES)
    set(CMAKE_XCODE_ATTRIBUTE_SEPARATE_STRIP YES)
    set(CMAKE_XCODE_ATTRIBUTE_STRIP_STYLE "non-global")
    set(CMAKE_XCODE_ATTRIBUTE_GENERATE_MASTER_OBJECT_FILE YES)
  endif()

endif(APPLE)

if(ANDROID)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden -ffunction-sections -fdata-sections")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -ftemplate-depth=1024 -fexceptions -frtti -fvisibility=hidden -fvisibility-inlines-hidden -ffunction-sections -fdata-sections")
  set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Oz -flto=thin -fomit-frame-pointer")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Oz -flto=thin -fomit-frame-pointer")
  set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} -s -fuse-ld=lld -Wl,-plugin-opt=O3 -Wl,--gc-sections -Wl,-icf=all -Wl,--as-needed -Wl,--build-id -Wl,--version-script=${PROJECT_SOURCE_DIR}/../android/version-script")
endif(ANDROID)

# Make common libraries available to all subprojects
include_directories("${SDK_EXTERNAL_LIBS_DIR}/boost")
include_directories("${SDK_EXTERNAL_LIBS_DIR}/boost/libs/any/include")

if(APPLE)
  # include_directories("${SDK_BASE_DIR}/ios/stdlib")
endif(APPLE)

# External subprojects required by routing-lib
# (same mechanism as scripts/build/CMakeLists.txt)
set(ROUTING_EXTERNAL_SUBPROJECTS sqlite sqlite3pp zstd date protobuf valhalla)

foreach(SDK_SUBPROJECT IN ITEMS ${ROUTING_EXTERNAL_SUBPROJECTS})
  add_subdirectory("${SDK_EXTERNAL_LIBS_DIR}/${SDK_SUBPROJECT}" "${SDK_SUBPROJECT}")
endforeach()

# When in single library mode, apply compilation flags to individual projects.
if(SINGLE_LIBRARY)
  foreach(SDK_SUBPROJECT IN ITEMS ${ROUTING_EXTERNAL_SUBPROJECTS})
    set(SUBPROJECT_FLAGS "${${SDK_SUBPROJECT}_FLAGS}")
    if(CMAKE_BUILD_TYPE MATCHES "Release|RELEASE")
      if(ANDROID OR APPLE)
        if(SDK_SUBPROJECT MATCHES "^(zstd)$")
          set(SUBPROJECT_FLAGS ${SUBPROJECT_FLAGS} "-O2")
          if (APPLE)
            set(SUBPROJECT_FLAGS ${SUBPROJECT_FLAGS} "-fno-modules")
          endif()
        else()
          set(SUBPROJECT_FLAGS ${SUBPROJECT_FLAGS} "-Oz")
        endif()
      elseif(WIN32)
        if(SDK_SUBPROJECT MATCHES "^(zstd)$")
          set(SUBPROJECT_FLAGS ${SUBPROJECT_FLAGS} "/Ox")
        else()
          set(SUBPROJECT_FLAGS ${SUBPROJECT_FLAGS} "/Os")
        endif()
      endif()
    endif()
    if(SUBPROJECT_FLAGS)
      set_source_files_properties(${${SDK_SUBPROJECT}_SRC_FILES} PROPERTIES COMPILE_OPTIONS "${SUBPROJECT_FLAGS}")
    endif()
  endforeach()
endif(SINGLE_LIBRARY)

# Source files and properties
file(GLOB_RECURSE SDK_SRC_FILES
  "${ROUTING_SRC_DIR}/native/*/*.cpp"
  "${ROUTING_SRC_DIR}/native/*/*.h"
)

# Platform-specific sources
if(ANDROID)
  file(GLOB SDK_ANDROID_SRC_FILES
    "${ROUTING_SRC_DIR}/android/*/*.cpp"
    "${ROUTING_SRC_DIR}/android/*/*.h"
  )
  set(SDK_SRC_FILES "${SDK_SRC_FILES}" "${SDK_ANDROID_SRC_FILES}")
elseif(APPLE)
  file(GLOB SDK_IOS_SRC_FILES
    "${ROUTING_SRC_DIR}/ios/*/*.cpp"
    "${ROUTING_SRC_DIR}/ios/*/*.mm"
    "${ROUTING_SRC_DIR}/ios/*/*.h"
  )
  set(SDK_SRC_FILES "${SDK_SRC_FILES}" "${SDK_IOS_SRC_FILES}")
endif()

# Include directories
include_directories(
  "${ROUTING_SRC_DIR}/native"
  "${ROUTING_SRC_DIR}/assets"
  ${ROUTING_SUBPROJECT_INCLUDE_DIRS}
)

foreach(SDK_HEADER_LIB IN ITEMS "rapidjson/include" "picojson")
  include_directories("${SDK_EXTERNAL_LIBS_DIR}/${SDK_HEADER_LIB}")
endforeach()

foreach(SDK_SUBPROJECT IN ITEMS ${ROUTING_EXTERNAL_SUBPROJECTS})
  set(SUBPROJECT_INCLUDE_DIRS ${${SDK_SUBPROJECT}_INCLUDE_DIRS})
  if(SUBPROJECT_INCLUDE_DIRS)
    include_directories("${SUBPROJECT_INCLUDE_DIRS}")
  endif()
endforeach()

# Collect compiled objects from all subprojects (same pattern as main SDK)
set(SDK_OBJECTS "")
if(SINGLE_LIBRARY)
  foreach(SDK_SUBPROJECT IN ITEMS ${ROUTING_EXTERNAL_SUBPROJECTS})
    set(SDK_SRC_FILES "${SDK_SRC_FILES}" "${${SDK_SUBPROJECT}_SRC_FILES}")
  endforeach()
else()
  foreach(SDK_SUBPROJECT IN ITEMS ${ROUTING_EXTERNAL_SUBPROJECTS})
    set(SDK_OBJECTS "${SDK_OBJECTS}" "$<TARGET_OBJECTS:${SDK_SUBPROJECT}>")
  endforeach()
endif()

add_library(valhalla_routing ${ROUTING_LIBRARY_TYPE}
  ${SDK_SRC_FILES}
  ${SDK_OBJECTS}
)

# Platform link libraries
if(ANDROID)
  target_link_libraries(valhalla_routing log z android)
elseif(APPLE)

  target_link_libraries(valhalla_routing
    "-framework Foundation"
    "-framework CoreFoundation"
    "-framework CFNetwork"
  )
endif()
