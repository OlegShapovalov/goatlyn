#pragma once
#include <QWidget>
#include <QSocketNotifier>
#include <QTimer>
#include <QRegularExpression>
#include <vterm.h>
#include <deque>
#include <vector>
#include <optional>
#include <string>
#include "PtyProcess.hpp"

struct CellPoint {
    int col = 0;
    int row = 0;

    bool operator==(const CellPoint& other) const {
        return col == other.col && row == other.row;
    }
    bool operator<(const CellPoint& other) const {
        if (row != other.row) return row < other.row;
        return col < other.col;
    }
    bool operator>(const CellPoint& other) const {
        if (row != other.row) return row > other.row;
        return col > other.col;
    }
};

struct HoveredTarget {
    QString text;
    int absRow = 0;
    int startCol = 0;
    int endCol = 0;
};

struct SearchMatch {
    int absRow = 0;
    int startCol = 0;
    int length = 0;
};

class TermWidget : public QWidget {
    Q_OBJECT
public:
    explicit TermWidget(const std::string& cwd = "", QWidget* parent = nullptr);
    ~TermWidget() override;

    std::string getCurrentCwd() const;
    std::string getActiveProcessName() const { return currentProcName_; }
    int getCols() const { return cols_; }
    int getRows() const { return rows_; }
    int getFontSize() const { return fontSize_; }

    void zoomIn();
    void zoomOut();
    void resetZoom();

    void setSearchQuery(const QString& query);
    void clearSearch();
    void findNextMatch();
    void findPrevMatch();

signals:
    void termExited(TermWidget* widget);
    void focused(TermWidget* widget);
    void stateChanged(TermWidget* widget);
    void titleChanged(TermWidget* widget, const QString& title, const QString& icon);
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

private slots:
    void onPtyRead();
    void pollActiveProcess();

private:
    PtyProcess pty_;
    VTerm* vt_ = nullptr;
    VTermScreen* vts_ = nullptr;
    VTermState* state_ = nullptr;
    QSocketNotifier* notifier_ = nullptr;
    QTimer* procPollTimer_ = nullptr;
    QTimer* cursorBlinkTimer_ = nullptr;

    int cols_ = 80;
    int rows_ = 24;
    int fontSize_ = 12;
    const int defaultFontSize_ = 12;
    int charW_ = 9;
    int charH_ = 18;
    int charAscent_ = 14;

    bool cursorBlinkState_ = true;
    int mouseProtocol_ = 0;
    std::string currentProcName_ = "bash";

    std::deque<std::vector<VTermScreenCell>> history_;
    const size_t maxHistory_ = 3000;
    int scrollOffset_ = 0;

    bool isSelecting_ = false;
    std::optional<CellPoint> selStart_;
    std::optional<CellPoint> selEnd_;

    QRegularExpression targetRegex_;
    std::optional<HoveredTarget> hoveredTarget_;

    QString searchQuery_;
    std::vector<SearchMatch> searchMatches_;
    int currentMatchIdx_ = -1;

    QString currentInputBuffer_;
    QString currentSuggestion_;
    std::vector<QString> cmdHistory_;

    void setupVTerm();
    void updateFontMetrics();
    void displayWelcomeBanner();
    void updateSuggestion();
    void updateSearchResults();
    void scrollToBottom();
    QColor resolveColor(const VTermColor& color, bool isBg);
    void sendMouseEvent(int button, int col, int row, bool isRelease);

    CellPoint pixelToCell(const QPoint& pos) const;
    bool isCellSelected(int col, int absRow) const;
    void copySelectionToClipboard();

    QString getRowString(int absRow) const;
    void checkTargetHover(const QPoint& pos, bool ctrlHeld);
    void openTarget(const QString& target);
    static QString getProcessIcon(const std::string& proc);

    static int sbPushline(int cols, const VTermScreenCell* cells, void* user);
    static int sbPopline(int cols, VTermScreenCell* cells, void* user);
    static int onSetTermProp(VTermProp prop, VTermValue* val, void* user);
    static int onBell(void* user);
};
