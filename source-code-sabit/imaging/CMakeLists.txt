add_library(sabit_imaging STATIC
    calibration.h
    calibration.cpp
    stacker.h
    stacker.cpp
    stretch.h
    stretch.cpp
    io/fits_writer.h
    io/fits_writer.cpp
    io/image_writer.h
    io/image_writer.cpp
    io/avi_writer.h
    io/avi_writer.cpp
    io/mp4_writer.h
    io/mp4_writer.cpp
)

target_include_directories(sabit_imaging PUBLIC
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(sabit_imaging PUBLIC
    sabit_core
)

if(WIN32)
    target_link_libraries(sabit_imaging PUBLIC
        mf
        mfplat
        mfreadwrite
        mfuuid
        ole32
    )
endif()

target_compile_features(sabit_imaging PUBLIC cxx_std_20)
