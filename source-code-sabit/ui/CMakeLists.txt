find_package(Qt6 COMPONENTS Widgets Core Gui Svg REQUIRED)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

add_library(sabit_ui STATIC
    theme/theme_manager.h
    theme/theme_manager.cpp
    i18n/translator.h
    i18n/translator.cpp
    fluent/fluent_toggle_switch.h
    fluent/fluent_toggle_switch.cpp
    fluent/fluent_badge.h
    fluent/fluent_badge.cpp
    fluent/fluent_card.h
    fluent/fluent_card.cpp
    widgets/video_widget.h
    widgets/video_widget.cpp
    widgets/histogram_widget.h
    widgets/histogram_widget.cpp
    widgets/collapsible_section.h
    widgets/collapsible_section.cpp
    widgets/marquee_label.h
    widgets/marquee_label.cpp
    widgets/themed_icon.h
    widgets/themed_icon.cpp
    panels/ephemeris_panel.h
    panels/ephemeris_panel.cpp
    panels/camera_panel.h
    panels/camera_panel.cpp
    panels/processing_panel.h
    panels/processing_panel.cpp
    panels/ai_panel.h
    panels/ai_panel.cpp
    panels/live_log_panel.h
    panels/live_log_panel.cpp
    status_bar.h
    status_bar.cpp
    dialogs/camera_dialog.h
    dialogs/camera_dialog.cpp
    dialogs/session_dialog.h
    dialogs/session_dialog.cpp
    dialogs/settings_dialog.h
    dialogs/settings_dialog.cpp
    dialogs/ephemeris_detail_dialog.h
    dialogs/ephemeris_detail_dialog.cpp
    dialogs/calibration_dialog.h
    dialogs/calibration_dialog.cpp
    dialogs/session_summary_dialog.h
    dialogs/session_summary_dialog.cpp
    dialogs/export_dialog.h
    dialogs/export_dialog.cpp
    dialogs/configure_capture_dialog.h
    dialogs/configure_capture_dialog.cpp
    main_window.h
    main_window.cpp
)

target_include_directories(sabit_ui PUBLIC
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(sabit_ui PUBLIC
    oclero::qlementine
    sabit_app
    sabit_camera
    sabit_imaging
    sabit_detect
    sabit_astro
    sabit_core
    Qt6::Widgets
    Qt6::Core
    Qt6::Gui
    Qt6::Svg
)

target_compile_features(sabit_ui PUBLIC cxx_std_20)
