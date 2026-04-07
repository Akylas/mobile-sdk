cmake_minimum_required(VERSION 3.18)
project(valhalla_routing)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Root of the routing-lib
set(ROUTING_LIB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../..")

# Collect routing-lib C++ sources
file(GLOB_RECURSE ROUTING_SOURCES
    "${ROUTING_LIB_ROOT}/native/core/*.cpp"
    "${ROUTING_LIB_ROOT}/native/datasource/*.cpp"
    "${ROUTING_LIB_ROOT}/native/routing/*.cpp"
    "${ROUTING_LIB_ROOT}/native/exceptions/*.cpp"
    "${ROUTING_LIB_ROOT}/native/log/*.cpp"
)

add_library(valhalla_routing SHARED
    ValhallaRoutingJNI.cpp
    ${ROUTING_SOURCES}
)

target_include_directories(valhalla_routing PRIVATE
    ${ROUTING_LIB_ROOT}/native
    ${ROUTING_LIB_ROOT}/assets
)

# Link against valhalla and SQLite3 (must be provided by the build environment)
find_library(log-lib log)
target_link_libraries(valhalla_routing
    ${log-lib}
    sqlite3
    z      # zlib for gzip decompression
)

# Optional: zstd for zstd-compressed MBTiles
find_library(zstd-lib zstd)
if(zstd-lib)
    target_compile_definitions(valhalla_routing PRIVATE HAVE_ZSTD)
    target_link_libraries(valhalla_routing ${zstd-lib})
endif()

# Valhalla support (define HAVE_VALHALLA and link when the valhalla lib is available)
find_library(valhalla-lib valhalla)
if(valhalla-lib)
    target_compile_definitions(valhalla_routing PRIVATE HAVE_VALHALLA)
    target_link_libraries(valhalla_routing ${valhalla-lib})
endif()
