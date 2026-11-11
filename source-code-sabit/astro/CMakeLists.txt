add_library(sabit_astro STATIC
    site.h
    ephemeris.h
    ephemeris.cpp
    criteria.h
    criteria.cpp
    hijri.h
    hijri.cpp
    plate_solver.h
    plate_solver.cpp
)

target_include_directories(sabit_astro PUBLIC
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(sabit_astro PUBLIC
    sabit_core
)

target_compile_features(sabit_astro PUBLIC cxx_std_20)
