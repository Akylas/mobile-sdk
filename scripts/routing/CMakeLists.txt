cmake_minimum_required(VERSION 3.10)
project(valhalla_routing)

enable_language(C CXX ASM)

# Directories (mirroring scripts/build/CMakeLists.txt)
set(SDK_BASE_DIR "${PROJECT_SOURCE_DIR}/../..")
set(SDK_EXTERNAL_LIBS_DIR "${SDK_BASE_DIR}/libs-external")
set(SDK_CARTO_LIBS_DIR "${SDK_BASE_DIR}/libs-carto")
set(ROUTING_SRC_DIR "${SDK_BASE_DIR}/routing-lib")

# Library type
if(ANDROID)
  set(ROUTING_LIBRARY_TYPE SHARED)
elseif(APPLE)
  set(ROUTING_LIBRARY_TYPE STATIC)
else()
  set(ROUTING_LIBRARY_TYPE STATIC)
endif()

# C++ defines (matching main CMakeLists.txt)
add_definitions("${SDK_CPP_DEFINES}" "-DBOOST_ALL_NO_LIB" "-DHAVE_ZSTD")

if(CMAKE_BUILD_TYPE MATCHES "Release|RELEASE")
  add_definitions("-DNDEBUG")
endif()

# Platform compiler flags (mirroring scripts/build/CMakeLists.txt exactly)
if(APPLE)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fobjc-arc -fmodules -fvisibility=hidden")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fobjc-arc -fmodules -stdlib=libc++ -std=c++17 -ftemplate-depth=1024 -fexceptions -frtti -fvisibility=hidden -fvisibility-inlines-hidden -fno-aligned-allocation -Wno-unqualified-std-cast-call -Wno-shorten-64-to-32")
  set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Os")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Os")
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
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -ftemplate-depth=1024 -fexceptions -frtti -fvisibility=hidden -fvisibility-inlines-hidden")
  set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Os -flto=thin -fomit-frame-pointer")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Os -flto=thin -fomit-frame-pointer")
  set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} -s -fuse-ld=lld -Wl,-plugin-opt=O3 -Wl,--gc-sections -Wl,-icf=all -Wl,--as-needed -Wl,--build-id")
endif(ANDROID)

# Boost header-only (required by valhalla)
include_directories("${SDK_EXTERNAL_LIBS_DIR}/boost")
include_directories("${SDK_EXTERNAL_LIBS_DIR}/boost/libs/any/include")

# External subprojects required by routing-lib
# (same mechanism as scripts/build/CMakeLists.txt)
set(ROUTING_EXTERNAL_SUBPROJECTS "sqlite" "sqlite3pp" "zstd")

list(APPEND ROUTING_EXTERNAL_SUBPROJECTS "date" "protobuf" "valhalla")

foreach(SDK_SUBPROJECT IN ITEMS ${ROUTING_EXTERNAL_SUBPROJECTS})
  add_subdirectory("${SDK_EXTERNAL_LIBS_DIR}/${SDK_SUBPROJECT}" "${SDK_SUBPROJECT}")
endforeach()

foreach(SDK_HEADER_LIB IN ITEMS "rapidjson/include")
  include_directories("${SDK_EXTERNAL_LIBS_DIR}/${SDK_HEADER_LIB}")
endforeach()

# Collect include dirs from subprojects (each exposes ${name}_INCLUDE_DIRS)
set(ROUTING_SUBPROJECT_INCLUDE_DIRS "")
foreach(SDK_SUBPROJECT IN ITEMS ${ROUTING_EXTERNAL_SUBPROJECTS})
  if(${SDK_SUBPROJECT}_INCLUDE_DIRS)
    list(APPEND ROUTING_SUBPROJECT_INCLUDE_DIRS "${${SDK_SUBPROJECT}_INCLUDE_DIRS}")
  endif()
endforeach()


# Routing-lib C++ sources
file(GLOB_RECURSE ROUTING_SOURCES
  "${ROUTING_SRC_DIR}/native/core/*.cpp"
  "${ROUTING_SRC_DIR}/native/exceptions/*.cpp"
  "${ROUTING_SRC_DIR}/native/log/*.cpp"
  "${ROUTING_SRC_DIR}/native/network/HTTPClient.cpp"
  "${ROUTING_SRC_DIR}/native/routing/*.cpp"
  "${ROUTING_SRC_DIR}/native/utils/*.cpp"
)

# Platform-specific sources
if(ANDROID)
  list(APPEND ROUTING_SOURCES
    "${ROUTING_SRC_DIR}/android/src/main/cpp/ValhallaRoutingJNI.cpp"
  )
  list(APPEND ROUTING_SOURCES
    "${ROUTING_SRC_DIR}/android/src/main/cpp/HTTPClientAndroidImpl.cpp"
  )
elseif(APPLE)
  list(APPEND ROUTING_SOURCES
    "${ROUTING_SRC_DIR}/ios/NTValhallaRoutingService.mm"
  )
  list(APPEND ROUTING_SOURCES
    "${ROUTING_SRC_DIR}/ios/HTTPClientIOSImpl.mm"
  )
endif()

# Collect compiled objects from all subprojects (same pattern as main SDK)
set(ROUTING_OBJECTS "")
foreach(SDK_SUBPROJECT IN ITEMS ${ROUTING_EXTERNAL_SUBPROJECTS})
  list(APPEND ROUTING_OBJECTS "$<TARGET_OBJECTS:${SDK_SUBPROJECT}>")
endforeach()

# Build the routing library (single library – all subproject objects embedded)
add_library(ValhallaRouting ${ROUTING_LIBRARY_TYPE}
  ${ROUTING_SOURCES}
  ${ROUTING_OBJECTS}
)

# Output name: libvalhalla_routing.so on Android, libValhallaRouting.a on Apple
if(ANDROID)
  set_target_properties(ValhallaRouting PROPERTIES OUTPUT_NAME "valhalla_routing")
endif()

# Include directories
target_include_directories(ValhallaRouting PRIVATE
  "${ROUTING_SRC_DIR}/native"
  "${ROUTING_SRC_DIR}/assets"
  ${ROUTING_SUBPROJECT_INCLUDE_DIRS}
)

# Platform link libraries
if(ANDROID)
  find_library(log-lib log)
  target_link_libraries(ValhallaRouting ${log-lib} z android)
elseif(APPLE)
  target_link_libraries(ValhallaRouting
    "-framework Foundation"
    "-framework CoreFoundation"
  )
endif()
