add_library(sabit_app STATIC
    session.h
    session.cpp
    app_controller.h
    app_controller.cpp
)

target_include_directories(sabit_app PUBLIC
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(sabit_app PUBLIC
    sabit_camera
    sabit_imaging
    sabit_detect
    sabit_astro
    sabit_core
)

target_compile_features(sabit_app PUBLIC cxx_std_20)
