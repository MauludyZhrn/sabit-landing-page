add_library(sabit_camera STATIC
    icamera.h
    sim/simulator_camera.h
    sim/simulator_camera.cpp
    zwo/zwo_camera.h
    zwo/zwo_camera.cpp
    qhy/qhy_camera.h
    qhy/qhy_camera.cpp
    ascom/ascom_camera.h
    ascom/ascom_camera.cpp
    uvc/uvc_camera.h
    uvc/uvc_camera.cpp
)

target_include_directories(sabit_camera PUBLIC
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(sabit_camera PUBLIC
    sabit_core
    mf
    mfplat
    mfreadwrite
    mfuuid
    ole32
    oleaut32
    uuid
    strmiids
    advapi32
)

target_compile_features(sabit_camera PUBLIC cxx_std_20)
