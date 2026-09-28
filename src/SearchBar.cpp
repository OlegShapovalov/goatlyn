#include "SearchBar.hpp"
#include <QHBoxLayout>
#include <QKeyEvent>

SearchBar::SearchBar(QWidget* parent) : QWidget(parent) {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(6);

    input_ = new QLineEdit(this);
    input_->setPlaceholderText("Найти в терминале...");
    input_->setStyleSheet(
        "QLineEdit { background: #313244; color: #cdd6f4; border: 1px solid #45475a; "
        "border-radius: 4px; padding: 4px 8px; font-size: 13px; min-width: 200px; }"
        "QLineEdit:focus { border: 1px solid #a6e3a1; }"
    );

    matchCountLabel_ = new QLabel("0/0", this);
    matchCountLabel_->setStyleSheet("color: #a6adc8; font-size: 12px;");

    auto* btnPrev = new QPushButton("▲", this);
    auto* btnNext = new QPushButton("▼", this);
    auto* btnClose = new QPushButton("✕", this);

    QString btnStyle = 
        "QPushButton { background: #313244; color: #cdd6f4; border: none; border-radius: 3px; padding: 4px 8px; }"
        "QPushButton:hover { background: #45475a; }"
        "QPushButton:pressed { background: #585b70; }";

    btnPrev->setStyleSheet(btnStyle);
    btnNext->setStyleSheet(btnStyle);
    btnClose->setStyleSheet(btnStyle);

    layout->addWidget(input_);
    layout->addWidget(matchCountLabel_);
    layout->addWidget(btnPrev);
    layout->addWidget(btnNext);
    layout->addWidget(btnClose);

    setStyleSheet("SearchBar { background-color: #181825; border-top: 1px solid #313244; }");

    connect(input_, &QLineEdit::textChanged, this, &SearchBar::searchChanged);
    connect(input_, &QLineEdit::returnPressed, this, [this]() { emit navigateNext(); });
    connect(btnPrev, &QPushButton::clicked, this, &SearchBar::navigatePrev);
    connect(btnNext, &QPushButton::clicked, this, &SearchBar::navigateNext);
    connect(btnClose, &QPushButton::clicked, this, &SearchBar::hideSearch);
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
