cmake_minimum_required(VERSION 2.6)

SET(CMAKE_CXX_FLAGS "-std=c++0x -g3 -O0 -Wall")
SET(CMAKE_C_FLAGS "-g3 -O0 -Wall")

SET(SRCs 
    computesi.c
    compute_interface.cpp
)

ADD_CUSTOM_TARGET(run
      DEPENDS   test
      COMMAND   ./remote_run
    )

include_directories(/usr/include/libdrm)


add_executable(test ${SRCs} main.cpp)
add_executable(fma_test ${SRCs} fma_test.cpp)
add_executable(write_test ${SRCs} write_test.cpp)
add_executable(read_test ${SRCs} read_test.cpp)
add_executable(mandelbrot ${SRCs} mandelbrot.cpp)

target_link_libraries(test drm_radeon drm)
target_link_libraries(fma_test drm_radeon drm)
target_link_libraries(write_test drm_radeon drm)
target_link_libraries(read_test drm_radeon drm)
target_link_libraries(mandelbrot drm_radeon drm)

