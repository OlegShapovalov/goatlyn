#pragma once
#include <QDialog>
#include <QListWidget>
#include <QStackedWidget>
#include <QFontComboBox>
#include <QSpinBox>
#include <QSlider>
#include <QComboBox>
#include <QRadioButton>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QKeyEvent>
#include <map>
#include "ConfigManager.hpp"

class KeySequenceButton : public QPushButton {
    Q_OBJECT
public:
    explicit KeySequenceButton(const QString& sequence, QWidget* parent = nullptr)
        : QPushButton(sequence, parent), currentSequence_(sequence) {
        setCheckable(true);
        setStyleSheet(
            "QPushButton { background: rgba(128, 128, 128, 30); color: inherit; border: 1px solid rgba(128, 128, 128, 70); border-radius: 4px; padding: 4px 10px; font-weight: normal; }"
            "QPushButton:checked { background: #f38ba8; color: #11111b; border-color: #f38ba8; }"
        );
        connect(this, &QPushButton::toggled, this, [this](bool checked) {
            setText(checked ? "Нажмите клавиши..." : currentSequence_);
        });
    }

    QString sequence() const { return currentSequence_; }
    void setSequence(const QString& seq) {
        currentSequence_ = seq;
        setText(seq);
    }

protected:
    void keyPressEvent(QKeyEvent* event) override {
        if (!isChecked()) {
            QPushButton::keyPressEvent(event);
            return;
        }

        int key = event->key();
        if (key == Qt::Key_Control || key == Qt::Key_Shift || key == Qt::Key_Alt || key == Qt::Key_Meta) {
            return;
        }

        QKeySequence seq(event->modifiers() | key);
        currentSequence_ = seq.toString(QKeySequence::NativeText);
        setChecked(false);
        setText(currentSequence_);
    }

private:
    QString currentSequence_;
};

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);

private slots:
    void applyAndSave();
    void onThemeSelected(const QString& themeName);
    void pickBgColor();
    void pickFgColor();
    void pickCursorColor();
    void createNewTheme();
    void deleteCurrentTheme();

private:
    QListWidget* sidebar_ = nullptr;
    QStackedWidget* pages_ = nullptr;

    // Внешний вид
    QComboBox* themeCombo_ = nullptr;
    QPushButton* btnBgColor_ = nullptr;
    QPushButton* btnFgColor_ = nullptr;
    QPushButton* btnCursorColor_ = nullptr;
    QPushButton* btnNewTheme_ = nullptr;
    QPushButton* btnDeleteTheme_ = nullptr;

    QColor currentBg_;
    QColor currentFg_;
    QColor currentCur_;
    bool currentIsCustom_ = false;

    QSlider* opacitySlider_ = nullptr;
    QCheckBox* showBannerCheck_ = nullptr;

    // Шрифт и курсор
    QFontComboBox* fontCombo_ = nullptr;
    QSpinBox* fontSizeSpin_ = nullptr;
    QRadioButton* cursorBlock_ = nullptr;
    QRadioButton* cursorBeam_ = nullptr;
    QRadioButton* cursorUnderline_ = nullptr;
    QCheckBox* cursorBlinkCheck_ = nullptr;

    // Шелл
    QComboBox* shellCombo_ = nullptr;

    // Горячие клавиши
    std::map<QString, KeySequenceButton*> keyButtons_;

    QWidget* createAppearancePage();
    QWidget* createFontPage();
    QWidget* createBehaviorPage();
    QWidget* createShortcutsPage();

    bool ensureCustomThemeBeforeEdit();
    void updateColorButtons();
};
