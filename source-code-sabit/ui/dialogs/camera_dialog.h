#ifndef SABIT_UI_CAMERA_DIALOG_H
#define SABIT_UI_CAMERA_DIALOG_H

#include <QDialog>
#include <QTreeWidget>
#include <QPushButton>
#include <vector>
#include "src/camera/icamera.h"

namespace sabit::ui {

class CameraDialog : public QDialog {
    Q_OBJECT
public:
    explicit CameraDialog(const std::vector<camera::CameraId>& cameras, QWidget* parent = nullptr);
    ~CameraDialog() override = default;

    [[nodiscard]] std::optional<camera::CameraId> selected_camera() const;

private:
    std::vector<camera::CameraId> cameras_;
    QTreeWidget* tree_widget_{nullptr};
    QPushButton* setup_btn_{nullptr};
    QPushButton* connect_btn_{nullptr};
    QPushButton* cancel_btn_{nullptr};
};

} // namespace sabit::ui

#endif // SABIT_UI_CAMERA_DIALOG_H
