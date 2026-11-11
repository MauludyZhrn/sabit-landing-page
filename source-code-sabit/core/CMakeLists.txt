add_library(sabit_core STATIC
    error.h
    result.h
    clock.h
    clock.cpp
    frame.h
    frame.cpp
    ring_buffer.h
    latest_slot.h
    logger.h
    logger.cpp
)

target_include_directories(sabit_core PUBLIC
    ${CMAKE_SOURCE_DIR}
)

target_compile_features(sabit_core PUBLIC cxx_std_20)
