#include "src/ui/dialogs/configure_capture_dialog.h"
#include "src/ui/i18n/translator.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>

namespace sabit::ui {

ConfigureCaptureDialog::ConfigureCaptureDialog(
    const QString& current_target,
    const QString& captures_dir,
    int current_limit_idx,
    int current_limit_val,
    QWidget* parent
) : QDialog(parent) {
    setWindowTitle(QString("Sabit - %1").arg(tr_str("configure_capture")));
    setMinimumWidth(440);
    setModal(true);

    setup_ui(current_target, captures_dir, current_limit_idx, current_limit_val);
}

void ConfigureCaptureDialog::setup_ui(
    const QString& current_target,
    const QString& captures_dir,
    int current_limit_idx,
    int current_limit_val
) {
    auto root_layout = new QVBoxLayout(this);
    root_layout->setContentsMargins(14, 14, 14, 14);
    root_layout->setSpacing(12);

    // 1. Target Name (Top row ala SharpCap)
    auto target_box = new QWidget(this);
    auto target_layout = new QVBoxLayout(target_box);
    target_layout->setContentsMargins(0, 0, 0, 0);
    target_layout->setSpacing(4);

    auto target_lbl = new QLabel(tr_str("target_name"), target_box);
    target_lbl->setObjectName("FieldLabel");

    target_combo_ = new QComboBox(target_box);
    target_combo_->setEditable(true);
    target_combo_->setMinimumHeight(28);

    const QStringList presets = {
        "Hilal",
        "Moon",
        "Sun",
        "Venus",
        "Jupiter",
        "Saturn",
        "Mercury",
        "Mars",
        "Daylight Sky",
        "Calibration",
        "Test Object"
    };

    target_combo_->addItems(presets);

    QString initial = current_target.trimmed();
    if (initial.isEmpty()) initial = "Hilal";

    int exist_idx = target_combo_->findText(initial, Qt::MatchFixedString);
    if (exist_idx >= 0) {
        target_combo_->setCurrentIndex(exist_idx);
    } else {
        target_combo_->setEditText(initial);
    }

    target_layout->addWidget(target_lbl);
    target_layout->addWidget(target_combo_);
    root_layout->addWidget(target_box);

    // 2. Select Capture Limit (Group Box)
    grp_limit_ = new QGroupBox(tr_str("capture_limit"), this);
    auto limit_grid = new QGridLayout(grp_limit_);
    limit_grid->setContentsMargins(10, 10, 10, 10);
    limit_grid->setHorizontalSpacing(10);
    limit_grid->setVerticalSpacing(8);

    rb_single_ = new QRadioButton(tr_str("single_frame"), grp_limit_);
    rb_unlimited_ = new QRadioButton(tr_str("unlimited"), grp_limit_);
    rb_frames_ = new QRadioButton(tr_str("frames"), grp_limit_);
    rb_time_ = new QRadioButton(tr_str("time_limit"), grp_limit_);

    spin_frames_ = new QSpinBox(grp_limit_);
    spin_frames_->setRange(1, 100000);
    spin_frames_->setValue(current_limit_val > 0 ? current_limit_val : 100);
    spin_frames_->setMinimumWidth(90);

    time_limit_ = new QTimeEdit(grp_limit_);
    time_limit_->setDisplayFormat("HH:mm:ss");
    int def_secs = (current_limit_idx == 1 && current_limit_val > 0) ? current_limit_val : 30;
    time_limit_->setTime(QTime(0, 0, 0).addSecs(def_secs));
    time_limit_->setMinimumWidth(90);

    limit_grid->addWidget(rb_single_, 0, 0, 1, 2);
    limit_grid->addWidget(rb_unlimited_, 1, 0, 1, 2);
    limit_grid->addWidget(rb_frames_, 2, 0);
    limit_grid->addWidget(spin_frames_, 2, 1);
    limit_grid->addWidget(rb_time_, 3, 0);
    limit_grid->addWidget(time_limit_, 3, 1);

    root_layout->addWidget(grp_limit_);

    // Set initial radio state
    if (current_limit_idx == 0) {
        rb_frames_->setChecked(true);
    } else if (current_limit_idx == 1) {
        rb_time_->setChecked(true);
    } else {
        rb_unlimited_->setChecked(true);
    }

    connect(rb_single_, &QRadioButton::toggled, this, &ConfigureCaptureDialog::on_limit_radio_changed);
    connect(rb_unlimited_, &QRadioButton::toggled, this, &ConfigureCaptureDialog::on_limit_radio_changed);
    connect(rb_frames_, &QRadioButton::toggled, this, &ConfigureCaptureDialog::on_limit_radio_changed);
    connect(rb_time_, &QRadioButton::toggled, this, &ConfigureCaptureDialog::on_limit_radio_changed);
    on_limit_radio_changed();

    // 3. Simple Sequence (Group Box)
    grp_sequence_ = new QGroupBox(tr_str("simple_sequence"), this);
    auto seq_layout = new QVBoxLayout(grp_sequence_);
    seq_layout->setContentsMargins(10, 10, 10, 10);
    seq_layout->setSpacing(8);

    chk_sequence_ = new QCheckBox(tr_str("seq_captures"), grp_sequence_);
    chk_sequence_->setChecked(false);
    seq_layout->addWidget(chk_sequence_);

    auto seq_form = new QGridLayout();
    seq_form->setContentsMargins(18, 0, 0, 0);
    seq_form->setHorizontalSpacing(10);
    seq_form->setVerticalSpacing(6);

    lbl_seq_len_ = new QLabel(tr_str("seq_length"), grp_sequence_);
    spin_seq_len_ = new QSpinBox(grp_sequence_);
    spin_seq_len_->setRange(1, 1000);
    spin_seq_len_->setValue(1);
    spin_seq_len_->setEnabled(false);
    seq_form->addWidget(lbl_seq_len_, 0, 0);
    seq_form->addWidget(spin_seq_len_, 0, 1);

    lbl_seq_interval_ = new QLabel(tr_str("seq_interval"), grp_sequence_);
    time_interval_ = new QTimeEdit(grp_sequence_);
    time_interval_->setDisplayFormat("HH:mm:ss");
    time_interval_->setTime(QTime(0, 5, 0));
    time_interval_->setEnabled(false);
    seq_form->addWidget(lbl_seq_interval_, 1, 0);
    seq_form->addWidget(time_interval_, 1, 1);

    seq_layout->addLayout(seq_form);
    root_layout->addWidget(grp_sequence_);

    connect(chk_sequence_, &QCheckBox::toggled, this, &ConfigureCaptureDialog::on_sequence_toggled);

    // 4. Capture Storage Directory
    grp_folder_ = new QGroupBox(tr_str("capture_folder"), this);
    auto f_layout = new QHBoxLayout(grp_folder_);
    f_layout->setContentsMargins(10, 8, 10, 8);
    f_layout->setSpacing(6);

    chosen_folder_ = captures_dir.isEmpty() ? "Captures" : captures_dir;
    folder_edit_ = new QLineEdit(chosen_folder_, grp_folder_);
    folder_edit_->setReadOnly(true);

    browse_folder_btn_ = new QPushButton(tr_str("browse"), grp_folder_);
    browse_folder_btn_->setFixedWidth(75);

    f_layout->addWidget(folder_edit_, 1);
    f_layout->addWidget(browse_folder_btn_);
    root_layout->addWidget(grp_folder_);

    connect(browse_folder_btn_, &QPushButton::clicked, this, &ConfigureCaptureDialog::on_browse_folder);

    // 5. Action Buttons (Start & Cancel)
    auto btn_box = new QHBoxLayout();
    btn_box->setContentsMargins(0, 4, 0, 0);
    btn_box->setSpacing(10);
    btn_box->addStretch(1);

    btn_start_ = new QPushButton(tr_str("start_capture"), this);
    btn_start_->setDefault(true);
    btn_start_->setMinimumWidth(100);
    btn_start_->setMinimumHeight(28);

    btn_cancel_ = new QPushButton(tr_str("cancel"), this);
    btn_cancel_->setMinimumWidth(80);
    btn_cancel_->setMinimumHeight(28);

    btn_box->addWidget(btn_start_);
    btn_box->addWidget(btn_cancel_);
    root_layout->addLayout(btn_box);

    connect(btn_start_, &QPushButton::clicked, this, &QDialog::accept);
    connect(btn_cancel_, &QPushButton::clicked, this, &QDialog::reject);
}

void ConfigureCaptureDialog::on_limit_radio_changed() {
    spin_frames_->setEnabled(rb_frames_->isChecked());
    time_limit_->setEnabled(rb_time_->isChecked());
}

void ConfigureCaptureDialog::on_sequence_toggled(bool checked) {
    spin_seq_len_->setEnabled(checked);
    time_interval_->setEnabled(checked);
}

void ConfigureCaptureDialog::on_browse_folder() {
    QString dir = QFileDialog::getExistingDirectory(
        this,
        tr_str("select_capture_folder"),
        folder_edit_->text(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );
    if (!dir.isEmpty()) {
        chosen_folder_ = dir;
        folder_edit_->setText(dir);
    }
}

QString ConfigureCaptureDialog::target_name() const {
    QString t = target_combo_->currentText().trimmed();
    return t.isEmpty() ? "Hilal" : t;
}

int ConfigureCaptureDialog::limit_type() const {
    if (rb_single_->isChecked()) return 0;
    if (rb_unlimited_->isChecked()) return 1;
    if (rb_frames_->isChecked()) return 2;
    if (rb_time_->isChecked()) return 3;
    return 2;
}

int ConfigureCaptureDialog::frame_count() const {
    if (rb_single_->isChecked()) return 1;
    if (rb_unlimited_->isChecked()) return -1;
    if (rb_frames_->isChecked()) return spin_frames_->value();
    if (rb_time_->isChecked()) {
        int secs = time_limit_seconds();
        return std::max(1, secs * 30); // ~30 fps equivalent
    }
    return 100;
}

int ConfigureCaptureDialog::time_limit_seconds() const {
    QTime t = time_limit_->time();
    return t.hour() * 3600 + t.minute() * 60 + t.second();
}

bool ConfigureCaptureDialog::sequence_enabled() const {
    return chk_sequence_->isChecked();
}

int ConfigureCaptureDialog::sequence_length() const {
    return spin_seq_len_->value();
}

int ConfigureCaptureDialog::sequence_interval_seconds() const {
    QTime t = time_interval_->time();
    return t.hour() * 3600 + t.minute() * 60 + t.second();
}

QString ConfigureCaptureDialog::selected_captures_dir() const {
    return chosen_folder_;
}

} // namespace sabit::ui
