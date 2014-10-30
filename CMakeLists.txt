cmake_minimum_required(VERSION 2.6)

if(CXX_BUILD_TYPE MATCHES NONE)
	SET(CXX_BUILD_TYPE DEBUG)
endif()

if(CXX_BUILD_TYPE MATCHES DEBUG)
	SET(CMAKE_CXX_FLAGS "-std=c++11 -g3 -O0 -Wall")
	SET(CMAKE_C_FLAGS "-g3 -O0 -Wall")
else()
	SET(CMAKE_CXX_FLAGS "-std=c++11")
endif()

SET(SRCs
    computesi.c
    compute_interface.cpp
)

ADD_CUSTOM_TARGET(run
      DEPENDS   test
      COMMAND   ./remote_run
    )

include_directories(/usr/include/libdrm)

add_library(radeonsi_compute SHARED ${SRCs})
target_link_libraries(radeonsi_compute drm_radeon drm)

install(TARGETS radeonsi_compute LIBRARY DESTINATION lib)
install(FILES computesi.h compute_interface.hpp code_helper.h sid.h DESTINATION include)

add_executable(test ${SRCs} main.cpp)
add_executable(fma_test ${SRCs} fma_test.cpp)
add_executable(write_test ${SRCs} write_test.cpp)
add_executable(read_test ${SRCs} read_test.cpp)
add_executable(mandelbrot ${SRCs} mandelbrot.cpp)
add_executable(divide_precision ${SRCs} divide_precision.cpp)

target_link_libraries(test drm_radeon drm)
target_link_libraries(fma_test drm_radeon drm)
target_link_libraries(write_test drm_radeon drm)
target_link_libraries(read_test drm_radeon drm)
target_link_libraries(mandelbrot drm_radeon drm)
target_link_libraries(divide_precision drm_radeon drm)