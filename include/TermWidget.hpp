#pragma once

#include <QWidget>
#include <QTimer>
#include <QSocketNotifier>
#include <QRegularExpression>
#include <vterm.h>
#include <vector>
#include <deque>
#include <string>
#include <optional>

#include "PtyProcess.hpp"

struct CellPoint {
    int col = 0;
    int row = 0;

    auto operator<=>(const CellPoint&) const = default;
};

struct SearchMatch {
    int absRow = 0;
    int startCol = 0;
    int length = 0;
};

struct HoveredTarget {
    QString text;
    int absRow = 0;
    int startCol = 0;
    int endCol = 0;
};

class TermWidget : public QWidget {
    Q_OBJECT

public:
    explicit TermWidget(const std::string& cwd = "", QWidget* parent = nullptr);
    ~TermWidget() override;

    void zoomIn();
    void zoomOut();
    void resetZoom();

    void setSearchQuery(const QString& query);
    void clearSearch();
    void findNextMatch();
    void findPrevMatch();

    std::string getCurrentCwd() const;
    static QString getProcessIcon(const std::string& proc);

    // Геттеры для строки состояния MainWindow
    std::string getActiveProcessName() const { return currentProcName_; }
    int getCols() const { return cols_; }
    int getRows() const { return rows_; }
    int getFontSize() const { return fontSize_; }

signals:
    void titleChanged(TermWidget* widget, const QString& title, const QString& icon);
    void stateChanged(TermWidget* widget);
    void focused(TermWidget* widget);
    void termExited(TermWidget* widget);
    void searchMatchesUpdated(int current, int total);
    void openSettingsRequested();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;

private:
    void setupVTerm();
    void updateFontMetrics();
    void applyPendingResize();
    void applyAccumulatedZoom();
    void updateSearchResults();
    void updateSuggestion();
    void displayWelcomeBanner();
    void pollActiveProcess();
    void scrollToBottom();
    QColor resolveColor(const VTermColor& color, bool isBg);
    void onPtyRead();
    CellPoint pixelToCell(const QPoint& pos) const;
    bool isCellSelected(int col, int absRow) const;
    QString getRowString(int absRow) const;
    void checkTargetHover(const QPoint& pos, bool ctrlHeld);
    void openTarget(const QString& target);
    void copySelectionToClipboard();
    void sendMouseEvent(int button, int col, int row, bool isRelease);

    // Callbacks for libvterm
    static int sbPushline(int cols, const VTermScreenCell* cells, void* user);
    static int sbPopline(int cols, VTermScreenCell* cells, void* user);
    static int onSetTermProp(VTermProp prop, VTermValue* val, void* user);
    static int onBell(void* user);

private:
    PtyProcess pty_;
    VTerm* vt_ = nullptr;
    VTermScreen* vts_ = nullptr;
    VTermState* state_ = nullptr;

    QSocketNotifier* notifier_ = nullptr;
    QTimer* resizeTimer_ = nullptr;
    QTimer* zoomApplyTimer_ = nullptr;
    QTimer* procPollTimer_ = nullptr;
    QTimer* cursorBlinkTimer_ = nullptr;

    int cols_ = 80;
    int rows_ = 24;
    int charW_ = 8;
    int charH_ = 16;
    int charAscent_ = 12;
    int fontSize_ = 11;
    int defaultFontSize_ = 11;
    int accumulatedZoomDelta_ = 0;

    int scrollOffset_ = 0;
    size_t maxHistory_ = 2000;
    std::deque<std::vector<VTermScreenCell>> history_;

    bool isAltScreen_ = false;
    bool cursorBlinkState_ = true;
    bool bannerActive_ = false;
    bool handlingResize_ = false;
    bool pendingResize_ = false;
    int mouseProtocol_ = 0;

    bool isSelecting_ = false;
    std::optional<CellPoint> selStart_;
    std::optional<CellPoint> selEnd_;

    std::optional<HoveredTarget> hoveredTarget_;
    QRegularExpression targetRegex_;

    QString searchQuery_;
    std::vector<SearchMatch> searchMatches_;
    int currentMatchIdx_ = -1;

    std::string currentProcName_ = "bash";
    QString currentSuggestion_;
    QString currentInputBuffer_;
    std::vector<QString> cmdHistory_;
};
