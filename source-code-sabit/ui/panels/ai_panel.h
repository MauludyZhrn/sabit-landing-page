#ifndef SABIT_UI_AI_PANEL_H
#define SABIT_UI_AI_PANEL_H

#include <QWidget>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <vector>
#include "src/detect/detection.h"

namespace sabit::ui {

enum class AiButtonState {
    Off,
    Loading,
    On
};

class AiPanel : public QWidget {
    Q_OBJECT
public:
    explicit AiPanel(QWidget* parent = nullptr);
    ~AiPanel() override = default;

    void update_detection_status(bool is_active, const std::vector<detect::Detection>& dets);
    void set_ai_state(AiButtonState state);
    void retranslate_ui();
    [[nodiscard]] AiButtonState ai_state() const { return current_state_; }

signals:
    void ai_toggled(bool enabled);
    void ai_state_requested(AiButtonState target_state);
    void conf_threshold_changed(float conf);
    void nms_threshold_changed(float nms);

private:
    void apply_button_style();

    QPushButton* ai_toggle_btn_{nullptr};
    QLabel* conf_title_lbl_{nullptr};
    QSlider* conf_slider_{nullptr};
    QLabel* conf_val_lbl_{nullptr};
    QLabel* nms_title_lbl_{nullptr};
    QSlider* nms_slider_{nullptr};
    QLabel* nms_val_lbl_{nullptr};

    QLabel* status_pill_{nullptr};
    QLabel* latest_conf_lbl_{nullptr};

    AiButtonState current_state_{AiButtonState::Off};
    std::string last_status_text_;
    std::string last_pill_text_;
};

} // namespace sabit::ui

#endif // SABIT_UI_AI_PANEL_H
