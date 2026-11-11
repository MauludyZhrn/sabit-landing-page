#include "src/ui/panels/ai_panel.h"
#include "src/ui/i18n/translator.h"
#include "src/ui/theme/theme_manager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>

namespace sabit::ui {

AiPanel::AiPanel(QWidget* parent) : QWidget(parent) {
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    // AI Main 3-State Toggle Button (OFF / MEMUAT / ON)
    ai_toggle_btn_ = new QPushButton(this);
    ai_toggle_btn_->setMinimumHeight(32);
    layout->addWidget(ai_toggle_btn_);

    // Thresholds
    auto grid = new QGridLayout();
    grid->setContentsMargins(0, 4, 0, 0);

    bool is_night_red = (ThemeManager::instance().current_theme() == ThemeType::NightRed);

    auto c_title = new QLabel(tr_str("conf_threshold") + ":", this);
    c_title->setStyleSheet(QString("color: %1;").arg(is_night_red ? "#991B1B" : "#94A3B8"));
    conf_title_lbl_ = c_title;
    conf_val_lbl_ = new QLabel("25%", this);
    conf_val_lbl_->setStyleSheet(QString("color: %1; font-weight: bold; font-family: 'Consolas', monospace;").arg(is_night_red ? "#FF2A2A" : "#67E8F9"));

    conf_slider_ = new QSlider(Qt::Horizontal, this);
    conf_slider_->setRange(10, 95); // 10% to 95% (FR-20)
    conf_slider_->setValue(25);

    auto n_title = new QLabel(tr_str("nms_threshold") + ":", this);
    n_title->setStyleSheet(QString("color: %1;").arg(is_night_red ? "#991B1B" : "#94A3B8"));
    nms_title_lbl_ = n_title;
    nms_val_lbl_ = new QLabel("45%", this);
    nms_val_lbl_->setStyleSheet(QString("color: %1; font-weight: bold; font-family: 'Consolas', monospace;").arg(is_night_red ? "#FF2A2A" : "#67E8F9"));

    nms_slider_ = new QSlider(Qt::Horizontal, this);
    nms_slider_->setRange(20, 70); // 20% to 70% (FR-20)
    nms_slider_->setValue(45);

    grid->addWidget(conf_title_lbl_, 0, 0);
    grid->addWidget(conf_val_lbl_, 0, 1, Qt::AlignRight);
    grid->addWidget(conf_slider_, 1, 0, 1, 2);

    grid->addWidget(nms_title_lbl_, 2, 0);
    grid->addWidget(nms_val_lbl_, 2, 1, Qt::AlignRight);
    grid->addWidget(nms_slider_, 3, 0, 1, 2);

    layout->addLayout(grid);

    // Status Indicator
    status_pill_ = new QLabel(this);
    status_pill_->setAlignment(Qt::AlignCenter);
    layout->addWidget(status_pill_);

    latest_conf_lbl_ = new QLabel(tr_str("hilal_not_detected"), this);
    latest_conf_lbl_->setStyleSheet(QString("color: %1; font-size: 11px;").arg(is_night_red ? "#991B1B" : "#94A3B8"));
    layout->addWidget(latest_conf_lbl_);

    // Initial Button & Pill State
    set_ai_state(AiButtonState::Off);

    connect(ai_toggle_btn_, &QPushButton::clicked, this, [this]() {
        if (current_state_ == AiButtonState::Off) {
            set_ai_state(AiButtonState::Loading);
            emit ai_state_requested(AiButtonState::Loading);
            emit ai_toggled(true);
        } else {
            set_ai_state(AiButtonState::Off);
            emit ai_state_requested(AiButtonState::Off);
            emit ai_toggled(false);
        }
    });

    connect(conf_slider_, &QSlider::valueChanged, this, [this](int v) {
        conf_val_lbl_->setText(QString("%1%").arg(v));
        emit conf_threshold_changed(v / 100.0f);
    });

    connect(nms_slider_, &QSlider::valueChanged, this, [this](int v) {
        nms_val_lbl_->setText(QString("%1%").arg(v));
        emit nms_threshold_changed(v / 100.0f);
    });
}

void AiPanel::set_ai_state(AiButtonState state) {
    current_state_ = state;
    apply_button_style();
}

void AiPanel::apply_button_style() {
    bool is_night_red = (ThemeManager::instance().current_theme() == ThemeType::NightRed);

    switch (current_state_) {
        case AiButtonState::Off:
            ai_toggle_btn_->setText(tr_str("ai_off"));
            if (is_night_red) {
                ai_toggle_btn_->setStyleSheet(
                    "QPushButton { background-color: #120202; color: #991B1B; "
                    "border: 1px solid #380606; border-radius: 4px; font-weight: bold; } "
                    "QPushButton:hover { border-color: #991B1B; color: #FF2A2A; }"
                );
                status_pill_->setText(QString("AI: %1").arg(tr_str("ai_standby")));
                status_pill_->setStyleSheet("background-color: #120202; color: #991B1B; border: 1px solid #380606; border-radius: 4px; padding: 4px; font-weight: bold; font-size: 11px;");
            } else {
                ai_toggle_btn_->setStyleSheet(
                    "QPushButton { background-color: #0F172A; color: #94A3B8; "
                    "border: 1px solid #1E293B; border-radius: 4px; font-weight: bold; } "
                    "QPushButton:hover { border-color: #06B6D4; color: #F8FAFC; }"
                );
                status_pill_->setText(QString("AI: %1").arg(tr_str("ai_standby")));
                status_pill_->setStyleSheet("background-color: #1E293B; color: #94A3B8; border-radius: 4px; padding: 4px; font-weight: bold; font-size: 11px;");
            }
            break;

        case AiButtonState::Loading:
            ai_toggle_btn_->setText(tr_str("ai_loading"));
            if (is_night_red) {
                ai_toggle_btn_->setStyleSheet(
                    "QPushButton { background-color: #2D0808; color: #FF2A2A; "
                    "border: 1px solid #B91C1C; border-radius: 4px; font-weight: bold; }"
                );
                status_pill_->setText(QString("AI: %1").arg(tr_str("ai_loading_onnx")));
                status_pill_->setStyleSheet("background-color: #2D0808; color: #FF2A2A; border: 1px solid #B91C1C; border-radius: 4px; padding: 4px; font-weight: bold; font-size: 11px;");
            } else {
                ai_toggle_btn_->setStyleSheet(
                    "QPushButton { background-color: #1E293B; color: #F59E0B; "
                    "border: 1px solid #D97706; border-radius: 4px; font-weight: bold; }"
                );
                status_pill_->setText(QString("AI: %1").arg(tr_str("ai_loading_onnx")));
                status_pill_->setStyleSheet("background-color: #1E293B; color: #F59E0B; border: 1px solid #D97706; border-radius: 4px; padding: 4px; font-weight: bold; font-size: 11px;");
            }
            break;

        case AiButtonState::On:
            ai_toggle_btn_->setText(tr_str("ai_on"));
            if (is_night_red) {
                ai_toggle_btn_->setStyleSheet(
                    "QPushButton { background-color: #FF2A2A; color: #000000; "
                    "border: 1px solid #FF2A2A; border-radius: 4px; font-weight: bold; } "
                    "QPushButton:hover { background-color: #CC1111; border-color: #CC1111; }"
                );
                status_pill_->setText(QString("AI: %1 (3 FPS)").arg(tr_str("ai_active")));
                status_pill_->setStyleSheet("background-color: #2D0808; color: #FF2A2A; border: 1px solid #FF2A2A; border-radius: 4px; padding: 4px; font-weight: bold; font-size: 11px;");
            } else {
                ai_toggle_btn_->setStyleSheet(
                    "QPushButton { background-color: #06B6D4; color: #070B14; "
                    "border: 1px solid #06B6D4; border-radius: 4px; font-weight: bold; } "
                    "QPushButton:hover { background-color: #0891B2; border-color: #0891B2; }"
                );
                status_pill_->setText(QString("AI: %1 (3 FPS)").arg(tr_str("ai_active")));
                status_pill_->setStyleSheet("background-color: #0F291E; color: #10B981; border: 1px solid #059669; border-radius: 4px; padding: 4px; font-weight: bold; font-size: 11px;");
            }
            break;
    }
}

void AiPanel::retranslate_ui() {
    if (conf_title_lbl_) conf_title_lbl_->setText(tr_str("conf_threshold") + ":");
    if (nms_title_lbl_) nms_title_lbl_->setText(tr_str("nms_threshold") + ":");
    apply_button_style();
    last_status_text_.clear();
}

void AiPanel::update_detection_status(bool is_active, const std::vector<detect::Detection>& dets) {
    bool is_night_red = (ThemeManager::instance().current_theme() == ThemeType::NightRed);

    if (is_active) {
        if (current_state_ != AiButtonState::On) {
            set_ai_state(AiButtonState::On);
        }
    } else {
        if (current_state_ != AiButtonState::Off && current_state_ != AiButtonState::Loading) {
            set_ai_state(AiButtonState::Off);
        }
    }

    if (!is_active) {
        std::string txt = tr_str("hilal_standby").toStdString();
        if (last_status_text_ != txt) {
            last_status_text_ = txt;
            latest_conf_lbl_->setText(QString::fromStdString(txt));
            latest_conf_lbl_->setStyleSheet(QString("color: %1; font-size: 11px;").arg(is_night_red ? "#991B1B" : "#94A3B8"));
        }
        return;
    }

    if (!dets.empty()) {
        float max_c = 0.0f;
        for (const auto& d : dets) {
            if (d.confidence > max_c) max_c = d.confidence;
        }
        QString txt = tr_str("hilal_detected").arg(QString::number(max_c * 100.0f, 'f', 1));
        if (last_status_text_ != txt.toStdString()) {
            last_status_text_ = txt.toStdString();
            latest_conf_lbl_->setText(txt);
            latest_conf_lbl_->setStyleSheet(QString("color: %1; font-weight: bold; font-size: 12px;").arg(is_night_red ? "#FF2A2A" : "#06B6D4"));
        }
    } else {
        std::string txt = tr_str("hilal_not_detected").toStdString();
        if (last_status_text_ != txt) {
            last_status_text_ = txt;
            latest_conf_lbl_->setText(QString::fromStdString(txt));
            latest_conf_lbl_->setStyleSheet(QString("color: %1; font-size: 11px;").arg(is_night_red ? "#991B1B" : "#94A3B8"));
        }
    }
}

} // namespace sabit::ui
