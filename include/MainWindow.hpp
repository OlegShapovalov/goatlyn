#pragma once
#include <QMainWindow>
#include <QTabBar>
#include <QStackedWidget>
#include <QLabel>
#include <QAction>
#include <QPushButton>
#include <string>
#include <map>
#include "TermWidget.hpp"

class SearchBar;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

public slots:
    void addNewTab(const std::string& cwd = "");
    void splitActive();
    void closeActiveTab();
    void onTabCloseRequested(int idx);
    void applyShortcuts();
    void updateThemeStyles();

private slots:
    void onTermExited(TermWidget* term);
    void updateStatusBar();

private:
    QWidget* topHeaderBar_ = nullptr;
    QTabBar* tabBar_ = nullptr;
    QPushButton* btnNewTab_ = nullptr;
    QPushButton* btnSettings_ = nullptr;
    QStackedWidget* stackWidget_ = nullptr;
    SearchBar* searchBar_ = nullptr;
    QWidget* statusBarWidget_ = nullptr;

    QLabel* lblCwd_ = nullptr;
    QLabel* lblProc_ = nullptr;
    QLabel* lblSize_ = nullptr;
    QLabel* lblZoom_ = nullptr;

    TermWidget* activeTerm_ = nullptr;
    std::map<QString, QAction*> actions_;

    TermWidget* createTermWidget(const std::string& cwd, QWidget* parent);
    void updateActiveTerminalFromTab();
    void initActions();
};
