#include "StatusBar.hpp"
#include <cstdlib>

StatusBar::StatusBar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(24);
    setStyleSheet(
        "StatusBar { background-color: #11111b; border-top: 1px solid #1e1e2e; font-family: monospace; font-size: 11px; }"
        "QLabel { color: #a6adc8; padding: 0 6px; }"
    );

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 0, 10, 0);
    layout->setSpacing(8);

    cwdLabel_ = new QLabel(" ~", this);
    cwdLabel_->setStyleSheet("color: #89b4fa; font-weight: bold;");

    procLabel_ = new QLabel(" bash", this);
    procLabel_->setStyleSheet("color: #a6e3a1;");

    gridLabel_ = new QLabel("80x24", this);
    gridLabel_->setStyleSheet("color: #fab387;");

    fontLabel_ = new QLabel("12pt", this);
    fontLabel_->setStyleSheet("color: #f9e2af;");

    layout->addWidget(cwdLabel_);
    layout->addWidget(procLabel_);
    layout->addStretch();
    layout->addWidget(fontLabel_);
    layout->addWidget(gridLabel_);
}

QString StatusBar::formatCwd(const QString& rawPath) {
    const char* home = getenv("HOME");
    if (home && rawPath.startsWith(home)) {
        return QString("~") + rawPath.mid(static_cast<int>(strlen(home)));
    }
    return rawPath.isEmpty() ? "~" : rawPath;
}

void StatusBar::updateInfo(const QString& cwd, int cols, int rows, int fontSize, const QString& procName, int pid) {
    cwdLabel_->setText(QString(" %1").arg(formatCwd(cwd)));
    gridLabel_->setText(QString("󰹑 %1x%2").arg(cols).arg(rows));
    fontLabel_->setText(QString(" %1pt").arg(fontSize));
    
    if (pid > 0) {
        procLabel_->setText(QString(" %1 (%2)").arg(procName).arg(pid));
    } else {
        procLabel_->setText(QString(" %1").arg(procName));
    }
}
