#include "src/ui/theme/theme_manager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPalette>
#include <QStyleFactory>

namespace sabit::ui {

ThemeManager& ThemeManager::instance() {
    static ThemeManager mgr;
    return mgr;
}

ThemeManager::ThemeManager() {
    set_fallback_tokens();
    load_tokens();
    current_colors_ = themes_[current_theme_];
}

void ThemeManager::set_fallback_tokens() {
    // 1. Dark Sabit (Obsidian Violet-Black & Electric Purple — FlowDash Dark Reference)
    themes_[ThemeType::Dark] = ThemeColors{
        .bg = "#0B0B14",
        .surface = "#121220",
        .surface2 = "#1A1A2E",
        .hover = "#24223E",
        .border = "#222238",
        .border_subtle = "#19192B",
        .text = "#F8FAFC",
        .muted = "#94A3B8",
        .accent = "#8B5CF6",
        .accent_secondary = "#A78BFA",
        .on_accent = "#FFFFFF",
        .focus = "#A78BFA",
        .danger = "#EF4444",
        .danger_bg = "#3B1018",
        .success = "#10B981",
        .warn = "#F59E0B",
        .video_bg = "#07070E",
        .console_fg = "#A78BFA"
    };

    // 2. Night Vision Red (Zero blue/green light emission - NFR-12)
    themes_[ThemeType::NightRed] = ThemeColors{
        .bg = "#050000",
        .surface = "#120202",
        .surface2 = "#1C0404",
        .hover = "#2D0808",
        .border = "#380606",
        .border_subtle = "#240404",
        .text = "#FF2A2A",
        .muted = "#991B1B",
        .accent = "#DC2626",
        .accent_secondary = "#991B1B",
        .on_accent = "#050000",
        .focus = "#FF2A2A",
        .danger = "#FF2A2A",
        .danger_bg = "#240404",
        .success = "#DC2626",
        .warn = "#B91C1C",
        .video_bg = "#020000",
        .console_fg = "#FF2A2A"
    };

    // 3. Light Theme (Crisp White, Soft Lavender-Gray & Royal Indigo — FlowDash Light Reference)
    themes_[ThemeType::Light] = ThemeColors{
        .bg = "#F8F9FE",
        .surface = "#FFFFFF",
        .surface2 = "#F1F0FB",
        .hover = "#E4E2F5",
        .border = "#E2E8F0",
        .border_subtle = "#EDF0F7",
        .text = "#0F172A",
        .muted = "#64748B",
        .accent = "#6366F1",
        .accent_secondary = "#8B5CF6",
        .on_accent = "#FFFFFF",
        .focus = "#6366F1",
        .danger = "#EF4444",
        .danger_bg = "#FEE2E2",
        .success = "#10B981",
        .warn = "#F59E0B",
        .video_bg = "#0B0B14",
        .console_fg = "#6366F1"
    };
}

void ThemeManager::load_tokens(const std::string& json_path) {
    QString app_dir = QCoreApplication::applicationDirPath();
    QStringList candidate_paths = {
        QString::fromStdString(json_path),
        app_dir + "/" + QString::fromStdString(json_path),
        app_dir + "/tokens.json"
    };

    QFile file;
    bool opened = false;
    for (const auto& path : candidate_paths) {
        file.setFileName(path);
        if (file.open(QIODevice::ReadOnly)) {
            opened = true;
            break;
        }
    }
    if (!opened) return;

    QByteArray raw = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (!doc.isObject()) return;

    QJsonObject root = doc.object();
    QJsonObject themes_obj = root["themes"].toObject();

    auto parse_theme = [&](const QString& key, ThemeType type) {
        if (!themes_obj.contains(key)) return;
        QJsonObject c = themes_obj[key].toObject()["colors"].toObject();
        auto& dst = themes_[type];
        auto get_str = [&](const char* k, const QString& def) {
            return c.contains(k) ? c[k].toString(def) : def;
        };
        dst.bg = get_str("bg", dst.bg);
        dst.surface = get_str("surface", dst.surface);
        dst.surface2 = get_str("surface2", dst.surface2);
        dst.hover = get_str("hover", dst.hover);
        dst.border = get_str("border", dst.border);
        dst.border_subtle = get_str("border_subtle", dst.border_subtle);
        dst.text = get_str("text", dst.text);
        dst.muted = get_str("muted", dst.muted);
        dst.accent = get_str("accent", dst.accent);
        dst.accent_secondary = get_str("accent_secondary", dst.accent_secondary);
        dst.on_accent = get_str("on_accent", dst.on_accent);
        dst.focus = get_str("focus", dst.focus);
        dst.danger = get_str("danger", dst.danger);
        dst.danger_bg = get_str("danger_bg", dst.danger_bg);
        dst.success = get_str("success", dst.success);
        dst.warn = get_str("warn", dst.warn);
        dst.video_bg = get_str("video_bg", dst.video_bg);
        dst.console_fg = get_str("console_fg", dst.console_fg);
    };

    parse_theme("dark", ThemeType::Dark);
    parse_theme("night_red", ThemeType::NightRed);
    parse_theme("light", ThemeType::Light);
}

oclero::qlementine::Theme ThemeManager::to_qlementine_theme(ThemeType type) const {
    auto it = themes_.find(type);
    const ThemeColors& c = (it != themes_.end()) ? it->second : themes_.at(ThemeType::Dark);

    oclero::qlementine::Theme qt;
    qt.meta.name = (type == ThemeType::NightRed) ? "Night Vision Red"
                 : (type == ThemeType::Light)    ? "Light System"
                                                 : "Deep Space Fluent";
    qt.backgroundColorMain1 = QColor(c.bg);
    qt.backgroundColorMain2 = QColor(c.surface);
    qt.backgroundColorMain3 = QColor(c.surface2);
    qt.backgroundColorMain4 = QColor(c.hover);
    qt.backgroundColorWorkspace = QColor(c.video_bg);
    qt.neutralColor = QColor(c.surface2);
    qt.neutralColorHovered = QColor(c.hover);
    qt.neutralColorPressed = QColor(c.border);
    qt.primaryColor = QColor(c.accent);
    qt.primaryColorHovered = QColor(c.accent).lighter(115);
    qt.primaryColorPressed = QColor(c.accent).darker(115);
    qt.primaryColorForeground = QColor(c.on_accent);
    qt.secondaryColor = QColor(c.accent_secondary);
    qt.primaryTextColor = QColor(c.text);
    qt.secondaryTextColor = QColor(c.muted);
    qt.borderColor = QColor(c.border);
    qt.borderColorFocused = QColor(c.focus);
    qt.focusColor = QColor(c.focus);
    qt.statusColorSuccess = QColor(c.success);
    qt.statusColorWarning = QColor(c.warn);
    qt.statusColorError = QColor(c.danger);
    qt.borderRadius = 6.0;
    return qt;
}

QString ThemeManager::generate_stylesheet(ThemeType type) const {
    auto it = themes_.find(type);
    if (it == themes_.end()) return "";

    const ThemeColors& c = it->second;

    QString qss = QString::fromUtf8(R"(
        * {
            font-family: 'Segoe UI Variable', 'Inter', 'Space Grotesk', 'Segoe UI', sans-serif;
            font-size: 12px;
        }
        QMainWindow, QDialog {
            background-color: @BG@;
            color: @TEXT@;
        }
        QWidget {
            color: @TEXT@;
            selection-background-color: @ACCENT@;
            selection-color: @ON_ACCENT@;
            outline: none;
        }
        QLabel {
            background: transparent;
        }

        /* ---------- Fluent Menu Bar ---------- */
        QMenuBar {
            background-color: @BG@;
            color: @TEXT@;
            border-bottom: 1px solid @BORDER@;
            padding: 3px 6px;
        }
        QMenuBar::item {
            background: transparent;
            padding: 5px 10px;
            border-radius: 5px;
        }
        QMenuBar::item:selected, QMenuBar::item:pressed {
            background-color: @SURFACE2@;
            color: @ACCENT@;
        }
        QMenu {
            background-color: @SURFACE@;
            color: @TEXT@;
            border: 1px solid @BORDER@;
            border-radius: 6px;
            padding: 5px;
        }
        QMenu::item {
            padding: 6px 26px 6px 12px;
            border-radius: 5px;
        }
        QMenu::item:selected {
            background-color: @SURFACE2@;
            color: @ACCENT@;
        }
        QMenu::item:disabled {
            color: @MUTED@;
        }
        QMenu::separator {
            height: 1px;
            background: @BORDER@;
            margin: 4px 8px;
        }

        /* ---------- Fluent Command Toolbar (SharpCap-style Flat & Compact) ---------- */
        QToolBar {
            background-color: @SURFACE@;
            border: none;
            border-bottom: 1px solid @BORDER@;
            spacing: 2px;
            padding: 1px 4px;
        }
        QToolBar::separator {
            background: @BORDER@;
            width: 1px;
            margin: 3px 4px;
        }
        QToolButton {
            background-color: transparent;
            color: @TEXT@;
            border: 1px solid transparent;
            border-radius: 4px;
            padding: 2px 7px;
            font-size: 11px;
            font-weight: 500;
        }
        QToolButton:hover {
            background-color: @HOVER@;
            border: 1px solid @BORDER_SUBTLE@;
            color: @TEXT@;
        }
        QToolButton:pressed {
            background-color: @BORDER_SUBTLE@;
        }
        QToolButton:checked {
            background-color: @ACCENT_ALPHA_16@;
            border: 1px solid @ACCENT@;
            color: @ACCENT@;
            font-weight: 600;
        }
        QToolButton:disabled {
            color: @MUTED@;
            background-color: transparent;
            border-color: transparent;
        }
        QToolButton#RecordButton {
            background-color: transparent;
            color: @DANGER@;
            border: 1px solid transparent;
            font-weight: 600;
        }
        QToolButton#RecordButton:hover {
            background-color: @DANGER_BG@;
            border: 1px solid @DANGER@;
            color: @DANGER@;
        }
        QToolButton#RecordButton:checked {
            background-color: @DANGER@;
            color: #FFFFFF;
            border: 1px solid @DANGER@;
            font-weight: 700;
        }
        QLabel#ToolbarLabel {
            color: @MUTED@;
            font-size: 11px;
            font-weight: 500;
            padding: 0 1px;
        }
        QFrame#ToolbarDivider {
            background-color: @BORDER@;
            max-width: 1px;
            min-width: 1px;
            margin: 3px 4px;
        }

        /* ---------- AI 3-state Flat Toggle ---------- */
        #AiToggle[aiState="off"] {
            color: @MUTED@;
            border: 1px solid transparent;
            background-color: transparent;
            font-weight: 500;
            border-radius: 4px;
        }
        #AiToggle[aiState="off"]:hover {
            color: @TEXT@;
            border: 1px solid @BORDER_SUBTLE@;
            background-color: @HOVER@;
        }
        #AiToggle[aiState="loading"] {
            color: @WARN@;
            border: 1px solid @WARN@;
            background-color: rgba(245, 158, 11, 0.12);
            font-weight: 600;
            border-radius: 4px;
        }
        #AiToggle[aiState="on"] {
            background-color: @ACCENT_ALPHA_16@;
            color: @ACCENT@;
            border: 1px solid @ACCENT@;
            font-weight: 600;
            border-radius: 4px;
        }
        #AiToggle[aiState="on"]:hover {
            background-color: @ACCENT_ALPHA_26@;
        }

        /* ---------- Right Sidebar & Fluent Cards ---------- */
        QWidget#RightPanel, QWidget#RightContainer {
            background-color: @BG@;
        }
        QLabel#PanelTitle {
            font-weight: 700;
            font-size: 11px;
            letter-spacing: 0.6px;
            color: @MUTED@;
        }
        QPushButton#ResetButton {
            background-color: @SURFACE@;
            color: @MUTED@;
            border: 1px solid @BORDER@;
            border-radius: 5px;
            padding: 3px 10px;
            font-size: 11px;
            min-height: 18px;
        }
        QPushButton#ResetButton:hover {
            color: @ACCENT@;
            background-color: @SURFACE2@;
            border-color: @ACCENT@;
        }
        QFrame#SectionCard, QFrame#FluentCard {
            background-color: @SURFACE@;
            border: 1px solid @BORDER@;
            border-radius: 4px;
        }
        QPushButton#SectionHeader {
            background-color: @SURFACE2@;
            color: @TEXT@;
            border: none;
            border-bottom: 1px solid @BORDER@;
            border-top-left-radius: 4px;
            border-top-right-radius: 4px;
            border-bottom-left-radius: 0px;
            border-bottom-right-radius: 0px;
            text-align: left;
            padding: 4px 8px;
            min-height: 18px;
            font-size: 11px;
            font-weight: 600;
        }
        QPushButton#SectionHeader[collapsed="true"] {
            border-bottom: none;
            border-radius: 4px;
        }
        QPushButton#SectionHeader:hover {
            background-color: @HOVER@;
        }
        QWidget#SectionBody {
            background: transparent;
        }
        QLabel#FieldLabel {
            color: @MUTED@;
            font-size: 11px;
        }
        QLabel#FieldValue {
            color: @TEXT@;
            font-family: 'Cascadia Mono', 'Consolas', monospace;
            font-size: 12px;
        }
        QLabel[state="ok"] {
            color: @SUCCESS@;
            font-weight: 600;
        }
        QLabel[state="bad"] {
            color: @DANGER@;
            font-weight: 600;
        }
        QLabel[state="warn"] {
            color: @WARN@;
            font-weight: 600;
        }
        QLabel[state="idle"] {
            color: @MUTED@;
        }
        QFrame#HistogramFrame {
            background-color: @VIDEO_BG@;
            border: 1px solid @BORDER@;
            border-radius: 6px;
        }

        /* ---------- Video Viewport ---------- */
        QGraphicsView#VideoView {
            background-color: @VIDEO_BG@;
            border: 1px solid @BORDER@;
            border-radius: 6px;
        }

        /* ---------- Status Bar ---------- */
        QStatusBar {
            background-color: @BG@;
            color: @MUTED@;
            border-top: 1px solid @BORDER@;
            min-height: 28px;
        }
        QStatusBar::item {
            border: none;
        }
        QStatusBar QLabel {
            color: @MUTED@;
            font-family: 'Cascadia Mono', 'Consolas', monospace;
            font-size: 11px;
        }
        QStatusBar QLabel[state="ok"] { color: @SUCCESS@; }
        QStatusBar QLabel[state="bad"] { color: @DANGER@; }
        QStatusBar QLabel[state="warn"] { color: @WARN@; }
        QProgressBar#FrameProgress {
            background-color: @VIDEO_BG@;
            border: 1px solid @BORDER@;
            border-radius: 5px;
            max-height: 10px;
            min-height: 10px;
        }
        QProgressBar#FrameProgress::chunk {
            background-color: @ACCENT@;
            border-radius: 4px;
        }

        /* ---------- Fluent Controls ---------- */
        QPushButton {
            background-color: @SURFACE2@;
            color: @TEXT@;
            border: 1px solid @BORDER@;
            border-radius: 6px;
            padding: 5px 14px;
            min-height: 20px;
            font-weight: 500;
        }
        QPushButton:hover {
            background-color: @HOVER@;
            border-color: @FOCUS@;
        }
        QPushButton:pressed {
            background-color: @BORDER_SUBTLE@;
        }
        QPushButton:disabled {
            color: @MUTED@;
            background-color: @SURFACE@;
        }
        QPushButton[class="primary"] {
            background-color: @ACCENT@;
            color: @ON_ACCENT@;
            border-color: @ACCENT@;
            font-weight: 700;
        }
        QPushButton[class="primary"]:hover {
            background-color: @FOCUS@;
        }
        QLineEdit {
            background-color: @SURFACE2@;
            color: @TEXT@;
            border: 1px solid @BORDER@;
            border-radius: 4px;
            padding: 1px 6px;
            font-size: 11px;
            min-height: 18px;
        }
        QComboBox {
            background-color: @SURFACE2@;
            color: @TEXT@;
            border: 1px solid @BORDER@;
            border-radius: 4px;
            padding: 1px 18px 1px 6px;
            font-size: 11px;
            min-height: 18px;
        }
        QSpinBox, QDoubleSpinBox {
            background-color: @SURFACE2@;
            color: @TEXT@;
            border: 1px solid @BORDER@;
            border-radius: 4px;
            padding: 1px 16px 1px 6px;
            font-size: 11px;
            min-height: 18px;
        }
        QLineEdit:hover, QSpinBox:hover, QDoubleSpinBox:hover, QComboBox:hover {
            border-color: @MUTED@;
        }
        QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus {
            border-color: @FOCUS@;
            background-color: @SURFACE@;
        }
        QSpinBox:disabled, QDoubleSpinBox:disabled, QComboBox:disabled {
            color: @MUTED@;
            background-color: @SURFACE@;
        }
        QComboBox::drop-down {
            subcontrol-origin: border;
            subcontrol-position: top right;
            width: 16px;
            border-left: 1px solid @BORDER_SUBTLE@;
            border-top-right-radius: 4px;
            border-bottom-right-radius: 4px;
            background: transparent;
        }
        QComboBox::drop-down:hover {
            background-color: @HOVER@;
        }
        QComboBox::down-arrow {
            image: url(@CHEVRON_DOWN@);
            width: 9px;
            height: 9px;
        }
        QSpinBox::up-button, QDoubleSpinBox::up-button {
            subcontrol-origin: border;
            subcontrol-position: top right;
            width: 14px;
            border-left: 1px solid @BORDER_SUBTLE@;
            border-bottom: 1px solid @BORDER_SUBTLE@;
            border-top-right-radius: 4px;
            background: transparent;
        }
        QSpinBox::down-button, QDoubleSpinBox::down-button {
            subcontrol-origin: border;
            subcontrol-position: bottom right;
            width: 14px;
            border-left: 1px solid @BORDER_SUBTLE@;
            border-bottom-right-radius: 4px;
            background: transparent;
        }
        QSpinBox::up-button:hover, QDoubleSpinBox::up-button:hover,
        QSpinBox::down-button:hover, QDoubleSpinBox::down-button:hover {
            background-color: @HOVER@;
        }
        QSpinBox::up-arrow, QDoubleSpinBox::up-arrow {
            image: url(@CHEVRON_UP@);
            width: 7px;
            height: 7px;
        }
        QSpinBox::down-arrow, QDoubleSpinBox::down-arrow {
            image: url(@CHEVRON_DOWN@);
            width: 7px;
            height: 7px;
        }
        QComboBox QAbstractItemView {
            background-color: @SURFACE@;
            color: @TEXT@;
            border: 1px solid @BORDER@;
            border-radius: 6px;
            padding: 3px;
            selection-background-color: @SURFACE2@;
            selection-color: @ACCENT@;
        }
        QCheckBox {
            spacing: 8px;
            background: transparent;
        }
        QCheckBox::indicator {
            width: 15px;
            height: 15px;
            border: 1px solid @MUTED@;
            background-color: @SURFACE2@;
            border-radius: 4px;
        }
        QCheckBox::indicator:checked {
            background-color: @ACCENT@;
            border-color: @ACCENT@;
        }

        /* ---------- Fluent Sliders ---------- */
        QSlider {
            background: transparent;
            min-height: 20px;
        }
        QSlider::groove:horizontal {
            height: 6px;
            background: @SURFACE2@;
            border: 1px solid @BORDER@;
            border-radius: 3px;
        }
        QSlider::sub-page:horizontal {
            background: @ACCENT@;
            border-radius: 3px;
        }
        QSlider::handle:horizontal {
            background: @TEXT@;
            border: 2px solid @ACCENT@;
            width: 14px;
            margin: -5px 0;
            border-radius: 8px;
        }
        QSlider::handle:horizontal:hover {
            background: @ACCENT@;
        }
        QSlider::groove:horizontal:disabled {
            border-color: @BORDER_SUBTLE@;
        }
        QSlider::sub-page:horizontal:disabled {
            background: @BORDER@;
        }
        QSlider::handle:horizontal:disabled {
            background: @MUTED@;
            border-color: @BORDER@;
        }

        QPlainTextEdit, QTextEdit {
            background-color: @VIDEO_BG@;
            color: @CONSOLE_FG@;
            border: 1px solid @BORDER@;
            border-radius: 6px;
            padding: 6px;
        }
        QGroupBox {
            background-color: @SURFACE@;
            border: 1px solid @BORDER@;
            border-radius: 7px;
            margin-top: 14px;
            padding: 14px 12px 12px 12px;
            font-weight: 600;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 12px;
            padding: 0 6px;
            color: @ACCENT@;
        }
        QTabWidget::pane {
            border: 1px solid @BORDER@;
            border-radius: 6px;
            background-color: @SURFACE@;
        }
        QTabBar::tab {
            background-color: @BG@;
            color: @MUTED@;
            border: 1px solid @BORDER@;
            padding: 7px 16px;
            margin-right: 4px;
            border-top-left-radius: 6px;
            border-top-right-radius: 6px;
        }
        QTabBar::tab:selected {
            background-color: @SURFACE@;
            color: @ACCENT@;
            border-bottom-color: @SURFACE@;
            font-weight: 600;
        }
        QScrollArea {
            background-color: @BG@;
            border: none;
        }
        QScrollBar:vertical {
            background: @BG@;
            width: 8px;
            margin: 0;
        }
        QScrollBar::handle:vertical {
            background: @SURFACE2@;
            min-height: 24px;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical:hover {
            background: @MUTED@;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
        }
        QSplitter::handle {
            background-color: @BORDER@;
        }
        QSplitter::handle:hover {
            background-color: @ACCENT@;
        }
        QListWidget {
            background-color: @SURFACE2@;
            color: @TEXT@;
            border: 1px solid @BORDER@;
            border-radius: 6px;
            padding: 4px;
        }
        QListWidget::item {
            padding: 8px 10px;
            border-radius: 5px;
        }
        QListWidget::item:selected {
            background-color: @HOVER@;
            color: @ACCENT@;
            border: 1px solid @ACCENT@;
        }
    )");

    QString icon_dir = QCoreApplication::applicationDirPath() + "/assets/icons";
    if (!QDir(icon_dir).exists()) {
        if (QDir("assets/icons").exists()) {
            icon_dir = QDir("assets/icons").absolutePath();
        } else {
            QDir().mkpath(icon_dir);
        }
    }
    const QString down_path = QDir::fromNativeSeparators(icon_dir + "/chevron_down.svg");
    const QString up_path = QDir::fromNativeSeparators(icon_dir + "/chevron_up.svg");

    {
        QFile f_down(down_path);
        if (f_down.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QString svg = QString("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"12\" height=\"12\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"%1\" stroke-width=\"2.5\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"m6 9 6 6 6-6\"/></svg>\n").arg(c.text);
            f_down.write(svg.toUtf8());
        }
        QFile f_up(up_path);
        if (f_up.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QString svg = QString("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"10\" height=\"10\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"%1\" stroke-width=\"2.5\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"m18 15-6-6-6 6\"/></svg>\n").arg(c.text);
            f_up.write(svg.toUtf8());
        }
    }

    const QColor acc_col(c.accent);
    const QString accent_alpha_16 = QString("rgba(%1, %2, %3, 0.16)").arg(acc_col.red()).arg(acc_col.green()).arg(acc_col.blue());
    const QString accent_alpha_26 = QString("rgba(%1, %2, %3, 0.26)").arg(acc_col.red()).arg(acc_col.green()).arg(acc_col.blue());

    qss.replace("@CHEVRON_DOWN@", down_path);
    qss.replace("@CHEVRON_UP@", up_path);
    qss.replace("@ACCENT_ALPHA_16@", accent_alpha_16);
    qss.replace("@ACCENT_ALPHA_26@", accent_alpha_26);
    qss.replace("@BG@", c.bg);
    qss.replace("@SURFACE2@", c.surface2);
    qss.replace("@SURFACE@", c.surface);
    qss.replace("@HOVER@", c.hover);
    qss.replace("@BORDER_SUBTLE@", c.border_subtle);
    qss.replace("@BORDER@", c.border);
    qss.replace("@TEXT@", c.text);
    qss.replace("@MUTED@", c.muted);
    qss.replace("@ACCENT_SECONDARY@", c.accent_secondary);
    qss.replace("@ACCENT@", c.accent);
    qss.replace("@ON_ACCENT@", c.on_accent);
    qss.replace("@FOCUS@", c.focus);
    qss.replace("@DANGER_BG@", c.danger_bg);
    qss.replace("@DANGER@", c.danger);
    qss.replace("@SUCCESS@", c.success);
    qss.replace("@WARN@", c.warn);
    qss.replace("@VIDEO_BG@", c.video_bg);
    qss.replace("@CONSOLE_FG@", c.console_fg);

    return qss;
}

void ThemeManager::apply_theme(QApplication* app, ThemeType type) {
    current_theme_ = type;
    if (themes_.find(type) != themes_.end()) {
        current_colors_ = themes_[type];
    }

    if (!app) return;

    if (!qlementine_style_) {
        qlementine_style_ = new oclero::qlementine::QlementineStyle(app);
        qlementine_style_->setAutoIconColor(oclero::qlementine::AutoIconColorMode::TextColor);
        qlementine_style_->setAnimationsEnabled(true);
        app->setStyle(qlementine_style_);
    }

    const auto qTheme = to_qlementine_theme(type);
    qlementine_style_->setTheme(qTheme);

    QPalette pal = app->palette();
    qlementine_style_->polish(pal);
    app->setPalette(pal);

    app->setStyleSheet(generate_stylesheet(type));
}

} // namespace sabit::ui
