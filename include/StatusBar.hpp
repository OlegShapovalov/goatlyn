#pragma once
#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>

class StatusBar : public QWidget {
    Q_OBJECT
public:
    explicit StatusBar(QWidget* parent = nullptr);

    void updateInfo(const QString& cwd, int cols, int rows, int fontSize, const QString& procName, int pid);

private:
    QLabel* cwdLabel_ = nullptr;
    QLabel* gridLabel_ = nullptr;
    QLabel* fontLabel_ = nullptr;
    QLabel* procLabel_ = nullptr;

    static QString formatCwd(const QString& rawPath);
};
