#include "SearchBar.hpp"
#include "ConfigManager.hpp"
#include <QHBoxLayout>
#include <QKeyEvent>

SearchBar::SearchBar(QWidget* parent) : QWidget(parent) {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(6);

    input_ = new QLineEdit(this);
    input_->setPlaceholderText("Найти в терминале...");

    matchCountLabel_ = new QLabel("0/0", this);

    // Теперь это поля класса (btnPrev_ и т.д.), а не локальные переменные
    btnPrev_ = new QPushButton("▲", this);
    btnNext_ = new QPushButton("▼", this);
    btnClose_ = new QPushButton("✕", this);

    layout->addWidget(input_);
    layout->addWidget(matchCountLabel_);
    layout->addWidget(btnPrev_);
    layout->addWidget(btnNext_);
    layout->addWidget(btnClose_);

    connect(input_, &QLineEdit::textChanged, this, &SearchBar::searchChanged);
    connect(input_, &QLineEdit::returnPressed, this, [this]() { emit navigateNext(); });
    connect(btnPrev_, &QPushButton::clicked, this, &SearchBar::navigatePrev);
    connect(btnNext_, &QPushButton::clicked, this, &SearchBar::navigateNext);
    connect(btnClose_, &QPushButton::clicked, this, &SearchBar::hideSearch);

    // Перекрашиваем панель при смене темы в настройках
    connect(&ConfigManager::instance(), &ConfigManager::configChanged,
            this, &SearchBar::applyTheme);

    applyTheme(); // Применяем текущую тему сразу при создании
}

void SearchBar::applyTheme() {
    const auto& th = ConfigManager::instance().currentTheme();
    QColor bg = th.background;
    QColor fg = th.foreground;
    bool isDark = bg.lightness() < 128;

    QColor surfaceBg = isDark ? bg.darker(118) : bg.darker(106);
    QColor borderCol = isDark ? bg.lighter(130) : bg.darker(125);
    QColor textMuted = isDark ? fg.darker(140) : fg.lighter(140);
    QColor accent = th.cursor;

    input_->setStyleSheet(QString(
        "QLineEdit { background: %1; color: %2; border: 1px solid %3; "
        "border-radius: 4px; padding: 4px 8px; font-size: 13px; min-width: 200px; }"
        "QLineEdit:focus { border: 1px solid %4; }"
    ).arg(surfaceBg.name(QColor::HexRgb),
          fg.name(QColor::HexRgb),
          borderCol.name(QColor::HexRgb),
          accent.name(QColor::HexRgb)));

    matchCountLabel_->setStyleSheet(QString("color: %1; font-size: 12px;")
        .arg(textMuted.name(QColor::HexRgb)));

    QString btnStyle = QString(
        "QPushButton { background: %1; color: %2; border: none; border-radius: 3px; padding: 4px 8px; }"
        "QPushButton:hover { background: %3; color: %4; }"
        "QPushButton:pressed { background: %5; }"
    ).arg(surfaceBg.name(QColor::HexRgb),
          fg.name(QColor::HexRgb),
          isDark ? bg.lighter(110).name(QColor::HexRgb) : bg.darker(105).name(QColor::HexRgb),
          accent.name(QColor::HexRgb),
          borderCol.name(QColor::HexRgb));

    btnPrev_->setStyleSheet(btnStyle);
    btnNext_->setStyleSheet(btnStyle);
    btnClose_->setStyleSheet(btnStyle);

    setStyleSheet(QString(
        "SearchBar { background-color: %1; border-top: 1px solid %2; }"
    ).arg(surfaceBg.name(QColor::HexRgb), borderCol.name(QColor::HexRgb)));
}

void SearchBar::activate() {
    show();
    input_->setFocus();
    input_->selectAll();
}

void SearchBar::setMatchInfo(int current, int total) {
    if (total == 0) {
        matchCountLabel_->setText("Не найдено");
    } else {
        matchCountLabel_->setText(QString("%1/%2").arg(current).arg(total));
    }
}

void SearchBar::clearQuery() {
    input_->clear();
}

void SearchBar::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        emit hideSearch();
    } else {
        QWidget::keyPressEvent(event);
    }
}