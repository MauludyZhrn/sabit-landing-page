add_library(sabit_detect STATIC
    detection.h
    sha256.h
    onnx_detector.h
    onnx_detector.cpp
    detection_log.h
    detection_log.cpp
)

target_include_directories(sabit_detect PUBLIC
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(sabit_detect PUBLIC
    sabit_core
)

target_compile_features(sabit_detect PUBLIC cxx_std_20)
