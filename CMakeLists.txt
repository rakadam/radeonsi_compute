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
	amd_abi.cpp
	ati_chip.cpp
	computesi.c
	compute_interface.cpp
)

ADD_CUSTOM_TARGET(run
      DEPENDS   test
      COMMAND   ./remote_run
    )

include_directories(/usr/include/libdrm)

add_library(radeonsi_compute SHARED ${SRCs})
target_link_libraries(radeonsi_compute drm_radeon drm pciaccess va-x11 va udev)

install(TARGETS radeonsi_compute LIBRARY DESTINATION lib)
install(FILES ati_chip.h ati_pciids_gen.def computesi.h compute_interface.hpp code_helper.h sid.h DESTINATION include)

add_executable(drm_master drm_master.cpp)
add_executable(test main.cpp)
add_executable(fma_test fma_test.cpp)
add_executable(write_test write_test.cpp)
add_executable(read_test read_test.cpp)
add_executable(mandelbrot mandelbrot.cpp)
add_executable(divide_precision divide_precision.cpp)
add_executable(amd_abi_test amd_abi_test.cpp)

target_link_libraries(drm_master radeonsi_compute)
target_link_libraries(test radeonsi_compute)
target_link_libraries(fma_test radeonsi_compute)
target_link_libraries(write_test radeonsi_compute)
target_link_libraries(read_test radeonsi_compute)
target_link_libraries(mandelbrot radeonsi_compute)
target_link_libraries(divide_precision radeonsi_compute)
target_link_libraries(amd_abi_test radeonsi_compute)
