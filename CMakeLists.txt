cmake_minimum_required(VERSION 2.6)

SET(CMAKE_CXX_FLAGS -std=c++0x)

SET(SRCs 
    computesi.c
    compute_interface.cpp
    main.cpp
)


include_directories(/usr/include/libdrm)


add_executable(test ${SRCs})
target_link_libraries(test drm_radeon drm)