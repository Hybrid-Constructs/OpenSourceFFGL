cmake_minimum_required(VERSION 3.15)

foreach(p
    GameOfLife
    ParamToOsc
    PulseControl
    Rainbow
    Wipe
)
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${p}/CMakeLists.txt")
        add_subdirectory(${p})
    endif()
endforeach()