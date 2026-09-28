#include <QMessageBox>
#include "SettingsDialog.hpp"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QLabel>
#include <QGroupBox>
#include <QColorDialog>
#include <QInputDialog>
#include <QHeaderView>

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Настройки Goatlyn");
    resize(700, 500);

    const auto& th = ConfigManager::instance().currentTheme();
    QColor bg = th.background;
    QColor fg = th.foreground;
    bool isDark = bg.lightness() < 128;
    QColor surfaceBg = isDark ? bg.darker(118) : bg.darker(106);
    QColor borderCol = isDark ? bg.lighter(130) : bg.darker(120);
    QColor textMuted = isDark ? fg.darker(135) : fg.lighter(135);
    QColor activeBg  = isDark ? bg.lighter(120) : bg.darker(110);
    QColor accent    = th.cursor;

    setStyleSheet(QString(
        "QDialog { background-color: %1; color: %2; font-weight: normal; }"
        "QLabel { color: %2; font-size: 13px; font-weight: normal; }"
        "QCheckBox, QRadioButton { color: %2; font-size: 13px; spacing: 8px; font-weight: normal; }"
        "QListWidget {"
        "  background-color: %3;"
        "  color: %4;"
        "  border: 1px solid %5;"
        "  border-radius: 6px;"
        "  padding: 4px;"
        "  font-size: 13px;"
        "  font-weight: normal;"
        "}"
        "QListWidget::item { padding: 8px 10px; border-radius: 4px; font-weight: normal; }"
        "QListWidget::item:selected { background-color: %6; color: %7; font-weight: normal; }"
        "QListWidget::item:hover:!selected { background-color: %8; color: %2; }"
        "QComboBox, QSpinBox, QFontComboBox {"
        "  background-color: %3;"
        "  color: %2;"
        "  border: 1px solid %5;"
        "  border-radius: 4px;"
        "  padding: 5px 8px;"
        "  font-weight: normal;"
        "}"
        "QGroupBox {"
        "  color: %2;"
        "  border: 1px solid %5;"
        "  border-radius: 6px;"
        "  margin-top: 10px;"
        "  padding-top: 10px;"
        "  font-weight: normal;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin;"
        "  subcontrol-position: top left;"
        "  left: 10px;"
        "  padding: 0 4px;"
        "  color: %7;"
        "  font-weight: normal;"
        "}"
        "QDialogButtonBox QPushButton {"
        "  background-color: %6;"
        "  color: %2;"
        "  border: 1px solid %5;"
        "  border-radius: 4px;"
        "  padding: 6px 16px;"
        "  font-weight: normal;"
        "}"
        "QDialogButtonBox QPushButton:hover { background-color: %8; color: %7; border-color: %7; }"
    ).arg(bg.name(QColor::HexRgb), fg.name(QColor::HexRgb),
         surfaceBg.name(QColor::HexRgb), textMuted.name(QColor::HexRgb),
         borderCol.name(QColor::HexRgb), activeBg.name(QColor::HexRgb),
         accent.name(QColor::HexRgb), isDark ? bg.lighter(110).name(QColor::HexRgb) : bg.darker(105).name(QColor::HexRgb)));

    auto* mainLayout = new QVBoxLayout(this);
    auto* centerLayout = new QHBoxLayout();

    sidebar_ = new QListWidget(this);
    sidebar_->setFixedWidth(180);
    sidebar_->addItem(" Внешний вид");
    sidebar_->addItem(" Шрифт и курсор");
    sidebar_->addItem(" Шелл и окружение");
    sidebar_->addItem(" Горячие клавиши");

    pages_ = new QStackedWidget(this);
    pages_->addWidget(createAppearancePage());
    pages_->addWidget(createFontPage());
    pages_->addWidget(createBehaviorPage());
    pages_->addWidget(createShortcutsPage());

    connect(sidebar_, &QListWidget::currentRowChanged, pages_, &QStackedWidget::setCurrentIndex);
    sidebar_->setCurrentRow(0);

    centerLayout->addWidget(sidebar_);
    centerLayout->addWidget(pages_);
    mainLayout->addLayout(centerLayout);

    auto* btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(btnBox, &QDialogButtonBox::accepted, this, &SettingsDialog::applyAndSave);
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(btnBox);
}

void SettingsDialog::updateColorButtons() {
    btnBgColor_->setStyleSheet(QString("background-color: %1; color: %2; border: 1px solid #666; border-radius: 4px; padding: 4px; font-weight: normal;")
        .arg(currentBg_.name(), currentFg_.name()));
    btnBgColor_->setText(currentBg_.name().toUpper());

    btnFgColor_->setStyleSheet(QString("background-color: %1; color: %2; border: 1px solid #666; border-radius: 4px; padding: 4px; font-weight: normal;")
        .arg(currentFg_.name(), currentBg_.name()));
    btnFgColor_->setText(currentFg_.name().toUpper());

    btnCursorColor_->setStyleSheet(QString("background-color: %1; color: #000; border: 1px solid #666; border-radius: 4px; padding: 4px; font-weight: normal;")
        .arg(currentCur_.name()));
    btnCursorColor_->setText(currentCur_.name().toUpper());
}

void SettingsDialog::onThemeSelected(const QString& name) {
    for (const auto& th : ConfigManager::instance().allThemes()) {
        if (th.name == name) {
            currentBg_ = th.background;
            currentFg_ = th.foreground;
            currentCur_ = th.cursor;
            updateColorButtons();
            break;
        }
    }
}

void SettingsDialog::pickBgColor() {
    QColor c = QColorDialog::getColor(currentBg_, this, "Выберите цвет фона");
    if (c.isValid()) {
        currentBg_ = c;
        updateColorButtons();
    }
}

void SettingsDialog::pickFgColor() {
    QColor c = QColorDialog::getColor(currentFg_, this, "Выберите цвет текста");
    if (c.isValid()) {
        currentFg_ = c;
        updateColorButtons();
    }
}

void SettingsDialog::pickCursorColor() {
    QColor c = QColorDialog::getColor(currentCur_, this, "Выберите цвет курсора");
    if (c.isValid()) {
        currentCur_ = c;
        updateColorButtons();
    }
}

void SettingsDialog::deleteCurrentTheme() {
    if (!currentIsCustom_) return;

    QString themeName = themeCombo_->currentText();
    int res = QMessageBox::question(
        this,
        "Удаление темы",
        QString("Удалить пользовательскую тему «%1»?").arg(themeName),
        QMessageBox::Yes | QMessageBox::No
    );

    if (res == QMessageBox::Yes) {
        int idx = themeCombo_->currentIndex();
        ConfigManager::instance().removeCustomTheme(themeName);
        themeCombo_->removeItem(idx);
    }
}

void SettingsDialog::createNewTheme() {
    bool ok = false;
    QString name = QInputDialog::getText(this, "Новая тема", "Введите имя темы:", QLineEdit::Normal, "My Custom Theme", &ok);
    if (ok && !name.trimmed().isEmpty()) {
        themeCombo_->addItem(name.trimmed());
        themeCombo_->setCurrentText(name.trimmed());
    }
}

QWidget* SettingsDialog::createAppearancePage() {
    auto* w = new QWidget(this);
    auto* form = new QFormLayout(w);

    auto* themeHBox = new QHBoxLayout();
    themeCombo_ = new QComboBox(w);
    for (const auto& th : ConfigManager::instance().allThemes()) {
        themeCombo_->addItem(th.name);
    }
    themeCombo_->setCurrentText(ConfigManager::instance().themeName());
    connect(themeCombo_, &QComboBox::currentTextChanged, this, &SettingsDialog::onThemeSelected);

    btnNewTheme_ = new QPushButton("+ Своя тема", w);
    btnNewTheme_->setStyleSheet("QPushButton { background-color: rgba(128, 128, 128, 40); border: 1px solid rgba(128, 128, 128, 80); border-radius: 4px; padding: 5px 12px; font-weight: normal; }");
    connect(btnNewTheme_, &QPushButton::clicked, this, &SettingsDialog::createNewTheme);

    themeHBox->addWidget(themeCombo_, 1);
    themeHBox->addWidget(btnNewTheme_);
    form->addRow("Цветовая тема:", themeHBox);

    btnBgColor_ = new QPushButton(w);
    connect(btnBgColor_, &QPushButton::clicked, this, &SettingsDialog::pickBgColor);
    form->addRow("Цвет фона окна:", btnBgColor_);

    btnFgColor_ = new QPushButton(w);
    connect(btnFgColor_, &QPushButton::clicked, this, &SettingsDialog::pickFgColor);
    form->addRow("Цвет текста:", btnFgColor_);

    btnCursorColor_ = new QPushButton(w);
    connect(btnCursorColor_, &QPushButton::clicked, this, &SettingsDialog::pickCursorColor);
    form->addRow("Цвет курсора:", btnCursorColor_);

    onThemeSelected(themeCombo_->currentText());

    opacitySlider_ = new QSlider(Qt::Horizontal, w);
    opacitySlider_->setRange(40, 100);
    opacitySlider_->setValue(static_cast<int>(ConfigManager::instance().backgroundOpacity() * 100));
    form->addRow("Прозрачность фона (%):", opacitySlider_);

    showBannerCheck_ = new QCheckBox("Отображать баннер при запуске", w);
    showBannerCheck_->setChecked(ConfigManager::instance().showBanner());
    form->addRow("", showBannerCheck_);

    return w;
}

QWidget* SettingsDialog::createFontPage() {
    auto* w = new QWidget(this);
    auto* form = new QFormLayout(w);

    fontCombo_ = new QFontComboBox(w);
    fontCombo_->setFontFilters(QFontComboBox::MonospacedFonts);
    fontCombo_->setCurrentFont(QFont(ConfigManager::instance().fontFamily()));
    form->addRow("Шрифт:", fontCombo_);

    fontSizeSpin_ = new QSpinBox(w);
    fontSizeSpin_->setRange(8, 36);
    fontSizeSpin_->setValue(ConfigManager::instance().fontSize());
    form->addRow("Размер кегля (pt):", fontSizeSpin_);

    auto* cursorBox = new QGroupBox("Форма курсора", w);
    auto* cursorLayout = new QHBoxLayout(cursorBox);
    cursorBlock_ = new QRadioButton("Блок █", cursorBox);
    cursorBeam_ = new QRadioButton("Луч |", cursorBox);
    cursorUnderline_ = new QRadioButton("Подчёркивание _", cursorBox);

    QString cur = ConfigManager::instance().cursorShape();
    if (cur == "beam") cursorBeam_->setChecked(true);
    else if (cur == "underline") cursorUnderline_->setChecked(true);
    else cursorBlock_->setChecked(true);

    cursorLayout->addWidget(cursorBlock_);
    cursorLayout->addWidget(cursorBeam_);
    cursorLayout->addWidget(cursorUnderline_);
    form->addRow(cursorBox);

    cursorBlinkCheck_ = new QCheckBox("Мигание курсора", w);
    cursorBlinkCheck_->setChecked(ConfigManager::instance().cursorBlink());
    form->addRow("", cursorBlinkCheck_);

    return w;
}

QWidget* SettingsDialog::createBehaviorPage() {
    auto* w = new QWidget(this);
    auto* form = new QFormLayout(w);

    shellCombo_ = new QComboBox(w);
    shellCombo_->setEditable(true);

    auto available = ConfigManager::instance().getAvailableShells();
    for (const auto& sh : available) {
        shellCombo_->addItem(sh);
    }

    QString currentShell = ConfigManager::instance().shell();
    if (shellCombo_->findText(currentShell) == -1) {
        shellCombo_->addItem(currentShell);
    }
    shellCombo_->setCurrentText(currentShell);

    form->addRow("Оболочка (Shell):", shellCombo_);

    auto* lblDesc = new QLabel("Укажите путь к исполняемому файлу шелла (например /bin/zsh или /bin/fish).", w);
    lblDesc->setStyleSheet("color: rgba(180, 180, 180, 180); font-size: 11px;");
    form->addRow("", lblDesc);

    return w;
}

QWidget* SettingsDialog::createShortcutsPage() {
    auto* w = new QWidget(this);
    auto* layout = new QVBoxLayout(w);

    auto* desc = new QLabel("Кликните на кнопку сочетания и нажмите новую комбинацию на клавиатуре:", w);
    desc->setStyleSheet("color: rgba(180, 180, 180, 180); font-size: 12px; margin-bottom: 6px;");
    layout->addWidget(desc);

    auto* table = new QTableWidget(w);
    table->setColumnCount(2);
    table->setHorizontalHeaderLabels({"Действие", "Горячая клавиша"});
    table->horizontalHeader()->setStretchLastSection(true);
    table->verticalHeader()->setVisible(false);
    table->setShowGrid(false);
    table->setSelectionMode(QAbstractItemView::NoSelection);

    table->setStyleSheet(
        "QTableWidget { background: rgba(0, 0, 0, 30); border: 1px solid rgba(128, 128, 128, 60); border-radius: 6px; }"
        "QHeaderView::section { background: rgba(0, 0, 0, 50); color: inherit; padding: 6px; border: none; font-weight: normal; }"
        "QTableWidget::item { padding-left: 8px; border-bottom: 1px solid rgba(128, 128, 128, 30); font-weight: normal; }"
    );

    struct ActionDesc {
        QString id;
        QString title;
        QString defaultSeq;
    };

    std::vector<ActionDesc> actions = {
        {"new_tab", "Новая вкладка", "Ctrl+Shift+T"},
        {"split_vertical", "Разделить по вертикали", "Ctrl+Shift+D"},
        {"close_tab", "Закрыть текущую вкладку", "Ctrl+Shift+W"},
        {"find", "Поиск текста", "Ctrl+Shift+F"},
        {"settings", "Открыть настройки", "Ctrl+,"},
        {"zoom_in", "Увеличить масштаб", "Ctrl+="},
        {"zoom_out", "Уменьшить масштаб", "Ctrl+-"},
        {"zoom_reset", "Сбросить масштаб", "Ctrl+0"}
    };

    table->setRowCount(static_cast<int>(actions.size()));

    for (int i = 0; i < (int)actions.size(); ++i) {
        const auto& act = actions[i];

        auto* itemTitle = new QTableWidgetItem(act.title);
        itemTitle->setFlags(Qt::ItemIsEnabled);
        table->setItem(i, 0, itemTitle);

        QString currentSeq = ConfigManager::instance().getKeybinding(act.id, act.defaultSeq);
        auto* btn = new KeySequenceButton(currentSeq, table);
        keyButtons_[act.id] = btn;
        table->setCellWidget(i, 1, btn);
        table->setRowHeight(i, 38);
    }

    table->setColumnWidth(0, 260);
    layout->addWidget(table);

    return w;
}

void SettingsDialog::applyAndSave() {
    auto& cfg = ConfigManager::instance();

    QString selectedName = themeCombo_->currentText();
    ThemePalette p;
    p.name = selectedName;
    p.background = currentBg_;
    p.foreground = currentFg_;
    p.cursor = currentCur_;
    p.isCustom = true;
    cfg.addOrUpdateCustomTheme(p);
    cfg.setTheme(selectedName);

    cfg.setBackgroundOpacity(opacitySlider_->value() / 100.0);
    cfg.setShowBanner(showBannerCheck_->isChecked());

    cfg.setFontFamily(fontCombo_->currentFont().family());
    cfg.setFontSize(fontSizeSpin_->value());

    if (cursorBeam_->isChecked()) cfg.setCursorShape("beam");
    else if (cursorUnderline_->isChecked()) cfg.setCursorShape("underline");
    else cfg.setCursorShape("block");

    cfg.setCursorBlink(cursorBlinkCheck_->isChecked());
    cfg.setShell(shellCombo_->currentText().trimmed());

    for (const auto& [actionId, btn] : keyButtons_) {
        cfg.setKeybinding(actionId, btn->sequence());
    }

    cfg.save();
    accept();
}
