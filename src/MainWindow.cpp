#include "MainWindow.hpp"
#include "TermWidget.hpp"
#include "SearchBar.hpp"
#include "SettingsDialog.hpp"
#include "ConfigManager.hpp"
#include <QTabBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QPushButton>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("Goatlyn Terminal");
    resize(1050, 650);

    setAttribute(Qt::WA_TranslucentBackground);
    setStyleSheet("QMainWindow { background: transparent; }");

    auto* central = new QWidget(this);
    central->setStyleSheet("background: transparent;");
    auto* mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Верхняя панель табов
    topHeaderBar_ = new QWidget(this);
    topHeaderBar_->setFixedHeight(32);

    auto* topLayout = new QHBoxLayout(topHeaderBar_);
    topLayout->setContentsMargins(0, 0, 8, 0);
    topLayout->setSpacing(0);

    tabBar_ = new QTabBar(topHeaderBar_);
    tabBar_->setTabsClosable(true);
    tabBar_->setMovable(true);
    tabBar_->setDrawBase(false);

    connect(tabBar_, &QTabBar::currentChanged, this, [this](int idx) {
        if (stackWidget_ && idx >= 0 && idx < stackWidget_->count()) {
            stackWidget_->setCurrentIndex(idx);
            updateActiveTerminalFromTab();
        }
    });

    connect(tabBar_, &QTabBar::tabCloseRequested, this, &MainWindow::onTabCloseRequested);

    topLayout->addWidget(tabBar_);
    topLayout->addStretch(1);

    btnNewTab_ = new QPushButton("+", topHeaderBar_);
    btnNewTab_->setFixedSize(24, 22);
    btnNewTab_->setToolTip("Новая вкладка");
    btnNewTab_->setCursor(Qt::PointingHandCursor);
    connect(btnNewTab_, &QPushButton::clicked, this, [this]() { addNewTab(); });

    btnSettings_ = new QPushButton("⚙", topHeaderBar_);
    btnSettings_->setFixedSize(24, 22);
    btnSettings_->setToolTip("Настройки");
    btnSettings_->setCursor(Qt::PointingHandCursor);
    connect(btnSettings_, &QPushButton::clicked, this, [this]() {
        SettingsDialog dlg(this);
        dlg.exec();
        if (activeTerm_) activeTerm_->setFocus();
    });

    topLayout->addWidget(btnNewTab_);
    topLayout->addSpacing(4);
    topLayout->addWidget(btnSettings_);

    stackWidget_ = new QStackedWidget(this);
    stackWidget_->setStyleSheet("background: transparent;");

    searchBar_ = new SearchBar(this);
    searchBar_->hide();

    // Статусбар
    statusBarWidget_ = new QWidget(this);
    statusBarWidget_->setFixedHeight(22);

    auto* sbLayout = new QHBoxLayout(statusBarWidget_);
    sbLayout->setContentsMargins(8, 0, 8, 0);
    sbLayout->setSpacing(12);

    lblCwd_ = new QLabel("~", statusBarWidget_);
    lblProc_ = new QLabel(" bash", statusBarWidget_);
    lblSize_ = new QLabel("80x24", statusBarWidget_);
    lblZoom_ = new QLabel("12pt", statusBarWidget_);

    sbLayout->addWidget(lblCwd_);
    sbLayout->addWidget(lblProc_);
    sbLayout->addStretch();
    sbLayout->addWidget(lblZoom_);
    sbLayout->addWidget(lblSize_);

    mainLayout->addWidget(topHeaderBar_);
    mainLayout->addWidget(stackWidget_, 1);
    mainLayout->addWidget(searchBar_);
    mainLayout->addWidget(statusBarWidget_);
    setCentralWidget(central);

    initActions();
    applyShortcuts();
    updateThemeStyles();

    connect(&ConfigManager::instance(), &ConfigManager::configChanged, this, [this]() {
        applyShortcuts();
        updateThemeStyles();
    });

    connect(searchBar_, &SearchBar::searchChanged, this, [this](const QString& q) {
        if (activeTerm_) activeTerm_->setSearchQuery(q);
    });
    connect(searchBar_, &SearchBar::navigateNext, this, [this]() {
        if (activeTerm_) activeTerm_->findNextMatch();
    });
    connect(searchBar_, &SearchBar::navigatePrev, this, [this]() {
        if (activeTerm_) activeTerm_->findPrevMatch();
    });
    connect(searchBar_, &SearchBar::hideSearch, this, [this]() {
        searchBar_->hide();
        if (activeTerm_) {
            activeTerm_->clearSearch();
            activeTerm_->setFocus();
        }
    });

    addNewTab();
}

void MainWindow::updateThemeStyles() {
    const auto& th = ConfigManager::instance().currentTheme();
    QColor baseBg = th.background;
    QColor fg = th.foreground;

    // Вычисляем гармоничные контрастные оттенки шапки и статусбара
    bool isDark = baseBg.lightness() < 128;
    QColor headerBg = isDark ? baseBg.darker(120) : baseBg.darker(108);
    QColor borderCol = isDark ? baseBg.lighter(130) : baseBg.darker(125);
    QColor tabSelBg = baseBg;
    QColor tabHoverBg = isDark ? baseBg.lighter(115) : baseBg.darker(104);
    QColor textMuted = isDark ? fg.darker(140) : fg.lighter(140);
    QColor accent = th.cursor;

    topHeaderBar_->setStyleSheet(QString(
        "QWidget { background-color: %1; border-bottom: 1px solid %2; }"
    ).arg(headerBg.name(QColor::HexRgb), borderCol.name(QColor::HexRgb)));

    tabBar_->setStyleSheet(QString(
        "QTabBar { background: transparent; border: none; font-weight: normal; }"
        "QTabBar::tab {"
        "  background: transparent;"
        "  color: %1;"
        "  padding: 4px 16px;"
        "  height: 24px;"
        "  border: none;"
        "  border-right: 1px solid %2;"
        "  font-size: 11px;"
        "  font-weight: normal;"
        "}"
        "QTabBar::tab:selected {"
        "  background: %3;"
        "  color: %4;"
        "  border-bottom: 2px solid %5;"
        "  font-weight: normal;"
        "}"
        "QTabBar::tab:hover:!selected {"
        "  background: %6;"
        "  color: %4;"
        "}"
        "QTabBar::close-button { subcontrol-position: right; margin-left: 8px; }"
    ).arg(textMuted.name(QColor::HexRgb), borderCol.name(QColor::HexRgb),
         tabSelBg.name(QColor::HexRgb), fg.name(QColor::HexRgb),
         accent.name(QColor::HexRgb), tabHoverBg.name(QColor::HexRgb)));

    QString btnStyle = QString(
        "QPushButton { background: transparent; color: %1; border: none; border-radius: 3px; font-size: 13px; font-weight: normal; }"
        "QPushButton:hover { background: %2; color: %3; }"
    ).arg(textMuted.name(QColor::HexRgb), tabHoverBg.name(QColor::HexRgb), fg.name(QColor::HexRgb));

    btnNewTab_->setStyleSheet(btnStyle);
    btnSettings_->setStyleSheet(btnStyle);

    statusBarWidget_->setStyleSheet(QString(
        "background-color: %1; border-top: 1px solid %2;"
    ).arg(headerBg.name(QColor::HexRgb), borderCol.name(QColor::HexRgb)));

    lblCwd_->setStyleSheet(QString("color: %1; font-size: 11px; background: transparent; font-weight: normal;").arg(accent.name(QColor::HexRgb)));
    lblProc_->setStyleSheet(QString("color: %1; font-size: 11px; background: transparent; font-weight: normal;").arg(fg.name(QColor::HexRgb)));
    lblSize_->setStyleSheet(QString("color: %1; font-size: 11px; background: transparent; font-weight: normal;").arg(textMuted.name(QColor::HexRgb)));
    lblZoom_->setStyleSheet(QString("color: %1; font-size: 11px; background: transparent; font-weight: normal;").arg(accent.name(QColor::HexRgb)));
}

void MainWindow::initActions() {
    auto addAct = [this](const QString& id, auto slot) {
        auto* act = new QAction(this);
        connect(act, &QAction::triggered, this, slot);
        addAction(act);
        actions_[id] = act;
    };

    addAct("new_tab", [this]() { addNewTab(); });
    addAct("split_vertical", [this]() { splitActive(); });
    addAct("close_tab", [this]() { closeActiveTab(); });
    addAct("find", [this]() { searchBar_->activate(); });
    addAct("settings", [this]() {
        SettingsDialog dlg(this);
        dlg.exec();
        if (activeTerm_) activeTerm_->setFocus();
    });
    addAct("zoom_in", [this]() { if (activeTerm_) activeTerm_->zoomIn(); });
    addAct("zoom_out", [this]() { if (activeTerm_) activeTerm_->zoomOut(); });
    addAct("zoom_reset", [this]() { if (activeTerm_) activeTerm_->resetZoom(); });
}

void MainWindow::applyShortcuts() {
    auto& cfg = ConfigManager::instance();
    for (const auto& [id, act] : actions_) {
        QString seq = cfg.getKeybinding(id, "");
        if (!seq.isEmpty()) act->setShortcut(QKeySequence(seq));
    }
}

void MainWindow::addNewTab(const std::string& cwd) {
    auto* splitter = new QSplitter(Qt::Horizontal, stackWidget_);
    splitter->setHandleWidth(2);
    splitter->setStyleSheet("QSplitter { background: transparent; } QSplitter::handle { background: rgba(128, 128, 128, 60); }");

    auto* term = createTermWidget(cwd, splitter);
    splitter->addWidget(term);

    int pageIdx = stackWidget_->addWidget(splitter);
    int tabIdx = tabBar_->addTab(" bash");
    tabBar_->setCurrentIndex(tabIdx);
    stackWidget_->setCurrentIndex(pageIdx);
    term->setFocus();
}

TermWidget* MainWindow::createTermWidget(const std::string& cwd, QWidget* parent) {
    auto* term = new TermWidget(cwd, parent);

    connect(term, &TermWidget::focused, this, [this](TermWidget* w) {
        activeTerm_ = w;
        updateStatusBar();
    });

    connect(term, &TermWidget::stateChanged, this, [this](TermWidget* w) {
        if (w == activeTerm_) updateStatusBar();
    });

    connect(term, &TermWidget::titleChanged, this, [this](TermWidget* w, const QString& title, const QString& icon) {
        for (int i = 0; i < stackWidget_->count(); ++i) {
            if (stackWidget_->widget(i)->isAncestorOf(w)) {
                tabBar_->setTabText(i, icon + " " + title);
                break;
            }
        }
        if (w == activeTerm_) updateStatusBar();
    });

    connect(term, &TermWidget::termExited, this, &MainWindow::onTermExited);

    connect(term, &TermWidget::searchMatchesUpdated, this, [this](int cur, int tot) {
        searchBar_->setMatchInfo(cur, tot);
    });

    connect(term, &TermWidget::openSettingsRequested, this, [this]() {
        SettingsDialog dlg(this);
        dlg.exec();
        if (activeTerm_) activeTerm_->setFocus();
    });

    return term;
}

void MainWindow::splitActive() {
    if (!activeTerm_) return;

    QWidget* parent = activeTerm_->parentWidget();
    auto* splitter = qobject_cast<QSplitter*>(parent);
    if (!splitter) return;

    std::string currentCwd = activeTerm_->getCurrentCwd();
    auto* newTerm = createTermWidget(currentCwd, splitter);
    splitter->addWidget(newTerm);

    QList<int> sizes;
    int share = splitter->width() / splitter->count();
    for (int i = 0; i < splitter->count(); ++i) sizes << share;
    splitter->setSizes(sizes);

    newTerm->setFocus();
}

void MainWindow::closeActiveTab() {
    onTabCloseRequested(tabBar_->currentIndex());
}

void MainWindow::onTabCloseRequested(int idx) {
    if (idx < 0 || idx >= tabBar_->count()) return;

    if (tabBar_->count() > 1) {
        auto* page = stackWidget_->widget(idx);
        tabBar_->removeTab(idx);
        stackWidget_->removeWidget(page);
        delete page;
        updateActiveTerminalFromTab();
    } else {
        close();
    }
}

void MainWindow::onTermExited(TermWidget* term) {
    QWidget* parent = term->parentWidget();
    auto* splitter = qobject_cast<QSplitter*>(parent);

    term->deleteLater();

    if (splitter) {
        if (splitter->count() <= 1) {
            for (int i = 0; i < stackWidget_->count(); ++i) {
                if (stackWidget_->widget(i) == splitter) {
                    onTabCloseRequested(i);
                    return;
                }
            }
        }
    }
    updateActiveTerminalFromTab();
}

void MainWindow::updateActiveTerminalFromTab() {
    QWidget* cur = stackWidget_->currentWidget();
    if (!cur) {
        activeTerm_ = nullptr;
        return;
    }

    auto terms = cur->findChildren<TermWidget*>();
    if (!terms.isEmpty()) {
        activeTerm_ = terms.first();
        activeTerm_->setFocus();
        updateStatusBar();
    }
}

void MainWindow::updateStatusBar() {
    if (!activeTerm_) return;

    std::string cwd = activeTerm_->getCurrentCwd();
    lblCwd_->setText(cwd.empty() ? "~" : QString::fromStdString(cwd));
    lblProc_->setText(QString::fromStdString(activeTerm_->getActiveProcessName()));
    lblSize_->setText(QString("%1x%2").arg(activeTerm_->getCols()).arg(activeTerm_->getRows()));
    lblZoom_->setText(QString("%1pt").arg(activeTerm_->getFontSize()));
}
