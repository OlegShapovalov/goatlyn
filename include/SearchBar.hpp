#pragma once
#include <QWidget>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>

class SearchBar : public QWidget {
    Q_OBJECT
public:
    explicit SearchBar(QWidget* parent = nullptr);

    void activate();
    void setMatchInfo(int current, int total);
    void clearQuery();

signals:
    void searchChanged(const QString& query);
    void navigateNext();
    void navigatePrev();
    void hideSearch();

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    QLineEdit* input_ = nullptr;
    QLabel* matchCountLabel_ = nullptr;
};
