#include "TermWidget.hpp"
#include "ConfigManager.hpp"
#include <QPainter>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QUrl>
#include <QProcess>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <sys/utsname.h>
#include <cstdlib>

int TermWidget::sbPushline(int cols, const VTermScreenCell* cells, void* user) {
    auto* self = static_cast<TermWidget*>(user);
    std::vector<VTermScreenCell> line(cells, cells + cols);
    self->history_.push_back(std::move(line));
    if (self->history_.size() > self->maxHistory_) {
        self->history_.pop_front();
    }
    return 1;
}

int TermWidget::sbPopline(int cols, VTermScreenCell* cells, void* user) {
    auto* self = static_cast<TermWidget*>(user);
    if (self->history_.empty()) return 0;

    const auto& line = self->history_.back();
    int count = std::min((int)line.size(), cols);
    for (int i = 0; i < count; ++i) {
        cells[i] = line[i];
    }
    self->history_.pop_back();
    return 1;
}

int TermWidget::onSetTermProp(VTermProp prop, VTermValue* val, void* user) {
    auto* self = static_cast<TermWidget*>(user);
    if (prop == VTERM_PROP_MOUSE) {
        self->mouseProtocol_ = val->number;
    }
    return 1;
}

int TermWidget::onBell(void* /*user*/) {
    // Звуковой сигнал отключен
    return 1;
}

QString TermWidget::getProcessIcon(const std::string& proc) {
    static const std::unordered_map<std::string, QString> icons = {
        {"bash", ""}, {"zsh", ""}, {"fish", ""}, {"sh", ""},
        {"python", ""}, {"python3", ""}, {"git", ""}, {"nvim", ""},
        {"vim", ""}, {"nano", "✎"}, {"htop", ""}, {"btop", ""},
        {"top", ""}, {"pacman", ""}, {"yay", ""}, {"paru", ""},
        {"cargo", "󱘗"}, {"rustc", ""}, {"gcc", ""}, {"g++", ""},
        {"clang", ""}, {"make", ""}, {"docker", "󰡨"}, {"ssh", "󰣀"}
    };
    auto it = icons.find(proc);
    return (it != icons.end()) ? it->second : QStringLiteral("");
}

TermWidget::TermWidget(const std::string& cwd, QWidget* parent) : QWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setCursor(Qt::IBeamCursor);

    updateFontMetrics();

    targetRegex_ = QRegularExpression(
        QStringLiteral(R"((https?://[^\s<>"'\(\)\[\]\{\}]+|(?:\~|\.|\/|[a-zA-Z0-9_\-])[^\s<>"'\(\)\[\]\{\}]*\/[^\s<>"'\(\)\[\]\{\}]+|[a-zA-Z0-9_\-\.]+\.[a-zA-Z0-9_]{1,10}))")
    );
    targetRegex_.optimize();

    setupVTerm();

    pty_.start("/bin/bash", cwd, cols_, rows_);

    notifier_ = new QSocketNotifier(pty_.getMasterFd(), QSocketNotifier::Read, this);
    connect(notifier_, &QSocketNotifier::activated, this, &TermWidget::onPtyRead);

    procPollTimer_ = new QTimer(this);
    connect(procPollTimer_, &QTimer::timeout, this, &TermWidget::pollActiveProcess);
    procPollTimer_->start(500);

    cursorBlinkTimer_ = new QTimer(this);
    connect(cursorBlinkTimer_, &QTimer::timeout, this, [this]() {
        cursorBlinkState_ = !cursorBlinkState_;
        update();
    });
    cursorBlinkTimer_->start(500);

    // Загружаем историю bash для подсказок
    const char* home = getenv("HOME");
    if (home) {
        std::ifstream hFile(std::string(home) + "/.bash_history");
        std::string hLine;
        while (std::getline(hFile, hLine)) {
            if (!hLine.empty() && hLine[0] != '#') {
                cmdHistory_.push_back(QString::fromStdString(hLine));
            }
        }
    }

    QTimer::singleShot(80, this, &TermWidget::displayWelcomeBanner);
}

TermWidget::~TermWidget() {
    if (vt_) vterm_free(vt_);
}

void TermWidget::updateFontMetrics() {
    QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    monoFont.setPointSize(fontSize_);
    setFont(monoFont);

    QFontMetrics fm(monoFont);
    charW_ = std::max(1, fm.horizontalAdvance('M'));
    charH_ = std::max(1, fm.height());
    charAscent_ = fm.ascent();
}

void TermWidget::zoomIn() {
    if (fontSize_ < 36) {
        fontSize_ += 1;
        updateFontMetrics();
        resizeEvent(nullptr);
        emit stateChanged(this);
        update();
    }
}

void TermWidget::zoomOut() {
    if (fontSize_ > 6) {
        fontSize_ -= 1;
        updateFontMetrics();
        resizeEvent(nullptr);
        emit stateChanged(this);
        update();
    }
}

void TermWidget::resetZoom() {
    if (fontSize_ != defaultFontSize_) {
        fontSize_ = defaultFontSize_;
        updateFontMetrics();
        resizeEvent(nullptr);
        emit stateChanged(this);
        update();
    }
}

void TermWidget::setSearchQuery(const QString& query) {
    searchQuery_ = query;
    updateSearchResults();
}

void TermWidget::clearSearch() {
    searchQuery_.clear();
    searchMatches_.clear();
    currentMatchIdx_ = -1;
    emit searchMatchesUpdated(0, 0);
    update();
}

void TermWidget::findNextMatch() {
    if (searchMatches_.empty()) return;
    currentMatchIdx_ = (currentMatchIdx_ + 1) % static_cast<int>(searchMatches_.size());
    const auto& match = searchMatches_[currentMatchIdx_];
    scrollOffset_ = (match.absRow < 0) ? -match.absRow : 0;
    emit searchMatchesUpdated(currentMatchIdx_ + 1, static_cast<int>(searchMatches_.size()));
    update();
}

void TermWidget::findPrevMatch() {
    if (searchMatches_.empty()) return;
    currentMatchIdx_ = (currentMatchIdx_ - 1 + static_cast<int>(searchMatches_.size())) % static_cast<int>(searchMatches_.size());
    const auto& match = searchMatches_[currentMatchIdx_];
    scrollOffset_ = (match.absRow < 0) ? -match.absRow : 0;
    emit searchMatchesUpdated(currentMatchIdx_ + 1, static_cast<int>(searchMatches_.size()));
    update();
}

void TermWidget::updateSearchResults() {
    searchMatches_.clear();
    currentMatchIdx_ = -1;

    if (searchQuery_.isEmpty()) {
        emit searchMatchesUpdated(0, 0);
        update();
        return;
    }

    int historyCount = static_cast<int>(history_.size());
    int minRow = -historyCount;
    int maxRow = rows_ - 1;

    for (int r = minRow; r <= maxRow; ++r) {
        QString lineStr = getRowString(r);
        int pos = 0;
        while ((pos = lineStr.indexOf(searchQuery_, pos, Qt::CaseInsensitive)) != -1) {
            searchMatches_.push_back({r, pos, static_cast<int>(searchQuery_.length())});
            pos += std::max(1, static_cast<int>(searchQuery_.length()));
        }
    }

    if (!searchMatches_.empty()) {
        currentMatchIdx_ = 0;
        emit searchMatchesUpdated(1, static_cast<int>(searchMatches_.size()));
    } else {
        emit searchMatchesUpdated(0, 0);
    }
    update();
}

void TermWidget::updateSuggestion() {
    currentSuggestion_.clear();
    if (currentInputBuffer_.length() >= 2) {
        for (auto it = cmdHistory_.rbegin(); it != cmdHistory_.rend(); ++it) {
            if (it->startsWith(currentInputBuffer_) && it->length() > currentInputBuffer_.length()) {
                currentSuggestion_ = it->mid(currentInputBuffer_.length());
                break;
            }
        }
    }
    update();
}

void TermWidget::displayWelcomeBanner() {
    std::string osName = "Arch Linux";
    std::ifstream osFile("/etc/os-release");
    std::string line;
    while (std::getline(osFile, line)) {
        if (line.rfind("PRETTY_NAME=", 0) == 0) {
            osName = line.substr(12);
            osName.erase(std::remove(osName.begin(), osName.end(), '"'), osName.end());
            break;
        }
    }

    struct utsname u;
    std::string kernel = (uname(&u) == 0) ? u.release : "Linux";

    std::string cpuName = "x86_64";
    std::ifstream cpuFile("/proc/cpuinfo");
    while (std::getline(cpuFile, line)) {
        if (line.find("model name") != std::string::npos) {
            size_t colon = line.find(':');
            if (colon != std::string::npos) {
                cpuName = line.substr(colon + 2);
                break;
            }
        }
    }

    std::string ramStr = "";
    std::ifstream memFile("/proc/meminfo");
    long long memTotal = 0, memAvail = 0;
    while (std::getline(memFile, line)) {
        if (line.rfind("MemTotal:", 0) == 0) sscanf(line.c_str(), "MemTotal: %lld", &memTotal);
        else if (line.rfind("MemAvailable:", 0) == 0) sscanf(line.c_str(), "MemAvailable: %lld", &memAvail);
    }
    if (memTotal > 0) {
        double totalGb = memTotal / 1048576.0;
        double usedGb = (memTotal - memAvail) / 1048576.0;
        char ramBuf[64];
        snprintf(ramBuf, sizeof(ramBuf), "%.1f GiB / %.1f GiB", usedGb, totalGb);
        ramStr = ramBuf;
    }

    const char* user = getenv("USER") ? getenv("USER") : "user";
    char host[256] = "archlinux";
    gethostname(host, sizeof(host));

    // Слева оставляем 20 пробелов под отрисовку PNG-аватара
    std::string pad = "                                      ";
    std::vector<std::string> info = {
        std::string("\033[1;32m") + user + "\033[0m@\033[1;32m" + host + "\033[0m",
        std::string("\033[0;37m") + std::string(strlen(user) + strlen(host) + 1, '-') + "\033[0m",
        std::string("\033[1;34mOS:      \033[0m") + osName,
        std::string("\033[1;34mKernel:  \033[0m") + kernel,
        std::string("\033[1;34mShell:   \033[0m") + "bash",
        std::string("\033[1;34mCPU:     \033[0m") + cpuName,
        std::string("\033[1;34mMemory:  \033[0m") + ramStr,
        "\033[40m   \033[41m   \033[42m   \033[43m   \033[44m   \033[45m   \033[46m   \033[47m   \033[0m"
    };

    std::ostringstream out;
    out << "\r\n";
    for (const auto& lineText : info) {
        out << pad << lineText << "\r\n";
    }
    out << "\r\n";

    std::string s = out.str();
    vterm_input_write(vt_, s.data(), s.size());
    vterm_screen_flush_damage(vts_);
    update();
}

void TermWidget::pollActiveProcess() {
    pid_t mainPid = pty_.getPid();
    if (mainPid <= 0) return;

    std::string activeName = "bash";
    try {
        std::string taskPath = "/proc/" + std::to_string(mainPid) + "/task/" + std::to_string(mainPid) + "/children";
        std::ifstream taskFile(taskPath);
        std::vector<pid_t> children;
        pid_t p;
        while (taskFile >> p) {
            children.push_back(p);
        }

        pid_t targetPid = children.empty() ? mainPid : children.back();
        std::string commPath = "/proc/" + std::to_string(targetPid) + "/comm";
        std::ifstream commFile(commPath);
        if (commFile >> activeName) {
            activeName.erase(std::remove(activeName.begin(), activeName.end(), '\n'), activeName.end());
        }
    } catch (...) {}

    if (activeName != currentProcName_) {
        currentProcName_ = activeName;
        QString qName = QString::fromStdString(activeName);
        QString icon = getProcessIcon(activeName);
        emit titleChanged(this, qName, icon);
        emit stateChanged(this);
    }
}

std::string TermWidget::getCurrentCwd() const {
    if (pty_.getPid() > 0) {
        std::string procPath = "/proc/" + std::to_string(pty_.getPid()) + "/cwd";
        std::error_code ec;
        auto target = std::filesystem::read_symlink(procPath, ec);
        if (!ec) return target.string();
    }
    return "";
}

void TermWidget::focusInEvent(QFocusEvent*) {
    emit focused(this);
    emit stateChanged(this);
    update();
}

void TermWidget::setupVTerm() {
    vt_ = vterm_new(rows_, cols_);
    vterm_set_utf8(vt_, 1);
    vts_ = vterm_obtain_screen(vt_);
    state_ = vterm_obtain_state(vt_);

    static VTermScreenCallbacks cb = {};
    cb.sb_pushline = &TermWidget::sbPushline;
    cb.sb_popline = &TermWidget::sbPopline;
    cb.settermprop = &TermWidget::onSetTermProp;
    cb.bell = &TermWidget::onBell;
    vterm_screen_set_callbacks(vts_, &cb, this);

    vterm_screen_reset(vts_, 1);
    vterm_screen_enable_altscreen(vts_, 1);
}

void TermWidget::scrollToBottom() {
    if (scrollOffset_ != 0) {
        scrollOffset_ = 0;
        update();
    }
}

QColor TermWidget::resolveColor(const VTermColor& color, bool isBg) {
    if (VTERM_COLOR_IS_DEFAULT_BG(&color)) {
        return ConfigManager::instance().currentTheme().background;
    }
    if (VTERM_COLOR_IS_DEFAULT_FG(&color)) {
        return ConfigManager::instance().currentTheme().foreground;
    }

    VTermColor rgb = color;
    vterm_screen_convert_color_to_rgb(vts_, &rgb);

    if (rgb.type == VTERM_COLOR_RGB) {
        return QColor(rgb.rgb.red, rgb.rgb.green, rgb.rgb.blue);
    }
    const auto& th = ConfigManager::instance().currentTheme();
    return isBg ? th.background : th.foreground;
}

void TermWidget::onPtyRead() {
    std::vector<char> buffer(16384);
    ssize_t bytes = read(pty_.getMasterFd(), buffer.data(), buffer.size());
    if (bytes > 0) {
        vterm_input_write(vt_, buffer.data(), bytes);
        vterm_screen_flush_damage(vts_);
        update();
    } else if (bytes <= 0) {
        notifier_->setEnabled(false);
        procPollTimer_->stop();
        cursorBlinkTimer_->stop();
        emit termExited(this);
    }
}

CellPoint TermWidget::pixelToCell(const QPoint& pos) const {
    int c = std::clamp(pos.x() / charW_, 0, cols_ - 1);
    int r = std::clamp(pos.y() / charH_, 0, rows_ - 1);
    int absRow = r - scrollOffset_;
    return {c, absRow};
}

bool TermWidget::isCellSelected(int col, int absRow) const {
    if (!selStart_ || !selEnd_ || *selStart_ == *selEnd_) return false;
    CellPoint p1 = *selStart_;
    CellPoint p2 = *selEnd_;
    if (p1 > p2) std::swap(p1, p2);

    if (absRow < p1.row || absRow > p2.row) return false;
    if (p1.row == p2.row) return col >= p1.col && col <= p2.col;
    if (absRow == p1.row) return col >= p1.col;
    if (absRow == p2.row) return col <= p2.col;
    return true;
}

QString TermWidget::getRowString(int absRow) const {
    int historyCount = static_cast<int>(history_.size());
    QString line;
    line.reserve(cols_);

    for (int c = 0; c < cols_; ++c) {
        VTermScreenCell cell = {};
        if (absRow < 0) {
            int histIdx = historyCount + absRow;
            if (histIdx >= 0 && histIdx < historyCount && c < (int)history_[histIdx].size()) {
                cell = history_[histIdx][c];
            }
        } else {
            vterm_screen_get_cell(vts_, {absRow, c}, &cell);
        }

        if (cell.chars[0]) {
            auto cp = static_cast<char32_t>(cell.chars[0]);
            line.append(QString::fromUcs4(&cp, 1));
        } else {
            line.append(' ');
        }
    }
    return line;
}

void TermWidget::checkTargetHover(const QPoint& pos, bool ctrlHeld) {
    if (!ctrlHeld) {
        if (hoveredTarget_.has_value()) {
            hoveredTarget_.reset();
            setCursor(Qt::IBeamCursor);
            update();
        }
        return;
    }

    CellPoint cp = pixelToCell(pos);
    QString lineStr = getRowString(cp.row);

    auto matchIterator = targetRegex_.globalMatch(lineStr);
    while (matchIterator.hasNext()) {
        auto match = matchIterator.next();
        int start = match.capturedStart();
        QString rawMatched = match.captured();

        while (!rawMatched.isEmpty() && QStringLiteral("'\")>]};,:").contains(rawMatched.back())) {
            rawMatched.chop(1);
        }
        int end = start + rawMatched.length();

        if (cp.col >= start && cp.col < end) {
            bool isUrl = rawMatched.startsWith("http://", Qt::CaseInsensitive) ||
                         rawMatched.startsWith("https://", Qt::CaseInsensitive);

            bool isFile = false;
            if (!isUrl) {
                std::filesystem::path p;
                std::string rawStr = rawMatched.toStdString();

                if (rawStr.rfind("~", 0) == 0) {
                    const char* home = getenv("HOME");
                    if (home) p = std::filesystem::path(home) / rawStr.substr(1);
                } else if (rawStr.rfind("/", 0) == 0) {
                    p = std::filesystem::path(rawStr);
                } else {
                    std::string cwd = getCurrentCwd();
                    p = cwd.empty() ? std::filesystem::path(rawStr) : (std::filesystem::path(cwd) / rawStr);
                }

                std::error_code ec;
                if (std::filesystem::exists(p, ec) && !ec) {
                    isFile = true;
                }
            }

            if (isUrl || isFile) {
                hoveredTarget_ = HoveredTarget{rawMatched, cp.row, start, end};
                setCursor(Qt::PointingHandCursor);
                update();
                return;
            }
        }
    }

    if (hoveredTarget_.has_value()) {
        hoveredTarget_.reset();
        setCursor(Qt::IBeamCursor);
        update();
    }
}

void TermWidget::openTarget(const QString& target) {
    if (target.isEmpty()) return;

    if (target.startsWith("http://", Qt::CaseInsensitive) || 
        target.startsWith("https://", Qt::CaseInsensitive)) {
        QDesktopServices::openUrl(QUrl(target));
        return;
    }

    std::filesystem::path p;
    std::string rawStr = target.toStdString();

    if (rawStr.rfind("~", 0) == 0) {
        const char* home = getenv("HOME");
        if (home) p = std::filesystem::path(home) / rawStr.substr(1);
    } else if (rawStr.rfind("/", 0) == 0) {
        p = std::filesystem::path(rawStr);
    } else {
        std::string cwd = getCurrentCwd();
        p = cwd.empty() ? std::filesystem::path(rawStr) : (std::filesystem::path(cwd) / rawStr);
    }

    QString finalPath = QString::fromStdString(p.string());
    QString editorProg;
    const char* envEditor = getenv("EDITOR");
    if (!envEditor) envEditor = getenv("VISUAL");

    if (envEditor) {
        editorProg = QString::fromLocal8Bit(envEditor);
    } else {
        for (const char* candidate : {"cosmic-edit", "pluma", "xed", "mousepad", "gedit", "kate", "kwrite"}) {
            if (std::filesystem::exists(std::string("/usr/bin/") + candidate)) {
                editorProg = candidate;
                break;
            }
        }
    }

    if (editorProg.isEmpty()) {
        editorProg = QStringLiteral("xdg-open");
    }

    QString shellCmd = QStringLiteral("exec %1 \"%2\" >/dev/null 2>&1 &").arg(editorProg, finalPath);
    QProcess::startDetached(QStringLiteral("/bin/sh"), {QStringLiteral("-c"), shellCmd});
}

void TermWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setFont(font());

    const auto& th = ConfigManager::instance().currentTheme();
    painter.fillRect(rect(), th.background);

    // Neofetch PNG-логотип слева от системной информации (увеличен в 2 раза)
    static QPixmap goatImg(":/goatlyn.png");
    if (!goatImg.isNull() && history_.empty() && scrollOffset_ == 0) {
        int avatarH = charH_ * 16;
        int avatarW = charW_ * 34;
        QPixmap scaled = goatImg.scaled(avatarW, avatarH, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        int imgY = charH_; 
        int imgX = charW_ * 2;
        painter.drawPixmap(imgX, imgY, scaled);
    }



    int historyCount = static_cast<int>(history_.size());

    for (int r = 0; r < rows_; ++r) {
        int absRow = r - scrollOffset_;
        int y = r * charH_;

        for (int c = 0; c < cols_; ++c) {
            VTermScreenCell cell = {};
            bool cellFound = false;

            if (absRow < 0) {
                int histIdx = historyCount + absRow;
                if (histIdx >= 0 && histIdx < historyCount) {
                    if (c < (int)history_[histIdx].size()) {
                        cell = history_[histIdx][c];
                        cellFound = true;
                    }
                }
            } else {
                vterm_screen_get_cell(vts_, {absRow, c}, &cell);
                cellFound = true;
            }

            int x = c * charW_;
            bool selected = isCellSelected(c, absRow);

            QColor bg = cellFound ? resolveColor(cell.bg, true) : th.background;
            QColor fg = cellFound ? resolveColor(cell.fg, false) : th.foreground;

            if (cell.attrs.reverse) {
                std::swap(fg, bg);
            }

            if (bg != th.background) {
                painter.fillRect(x, y, charW_, charH_, bg);
            }
            if (selected) {
                // Полупрозрачный акцентный слой выделения поверх фона
                painter.fillRect(x, y, charW_, charH_, QColor(137, 180, 250, 110));
            }

            if (cellFound && cell.chars[0] && cell.chars[0] != ' ') {
                painter.setPen(fg);
                auto cp = static_cast<char32_t>(cell.chars[0]);
                QString text = QString::fromUcs4(&cp, 1);
                painter.drawText(x, y + charAscent_, text);
            }
        }

        if (!searchMatches_.empty()) {
            for (size_t i = 0; i < searchMatches_.size(); ++i) {
                const auto& m = searchMatches_[i];
                if (m.absRow == absRow) {
                    int matchX = m.startCol * charW_;
                    int matchW = m.length * charW_;
                    QColor hlColor = (static_cast<int>(i) == currentMatchIdx_) 
                        ? QColor(249, 226, 175, 160)
                        : QColor(137, 180, 250, 100);
                    painter.fillRect(matchX, y, matchW, charH_, hlColor);
                }
            }
        }

        if (hoveredTarget_.has_value() && hoveredTarget_->absRow == absRow) {
            int lineY = y + charH_ - 1;
            int x1 = hoveredTarget_->startCol * charW_;
            int x2 = hoveredTarget_->endCol * charW_;
            painter.setPen(QPen(QColor("#38bdf8"), 2));
            painter.drawLine(x1, lineY, x2, lineY);
        }
    }

    if (scrollOffset_ == 0) {
        VTermPos cursor;
        vterm_state_get_cursorpos(state_, &cursor);
        if (cursor.row < rows_ && cursor.col < cols_) {
            if (cursorBlinkState_ || !hasFocus()) {
                QString shape = ConfigManager::instance().cursorShape();
                int cx = cursor.col * charW_;
                int cy = cursor.row * charH_;
                QColor curColor = th.cursor;
                curColor.setAlpha(180);

                if (shape == "beam") {
                    painter.fillRect(cx, cy, 2, charH_, curColor);
                } else if (shape == "underline") {
                    painter.fillRect(cx, cy + charH_ - 2, charW_, 2, curColor);
                } else {
                    painter.fillRect(cx, cy, charW_, charH_, curColor);
                }
            }

            if (!currentSuggestion_.isEmpty()) {
                QColor ghostColor = th.foreground;
                ghostColor.setAlpha(140);
                painter.setPen(ghostColor);
                int ghostX = cursor.col * charW_;
                int ghostY = cursor.row * charH_ + charAscent_;
                painter.drawText(ghostX, ghostY, currentSuggestion_);
            }
        }
    }
}

void TermWidget::copySelectionToClipboard() {
    if (!selStart_ || !selEnd_ || *selStart_ == *selEnd_) return;

    CellPoint p1 = *selStart_;
    CellPoint p2 = *selEnd_;
    if (p1 > p2) std::swap(p1, p2);

    int historyCount = static_cast<int>(history_.size());
    QString result;

    for (int r = p1.row; r <= p2.row; ++r) {
        int cStart = (r == p1.row) ? p1.col : 0;
        int cEnd = (r == p2.row) ? p2.col : (cols_ - 1);

        QString lineText;
        for (int c = cStart; c <= cEnd; ++c) {
            VTermScreenCell cell = {};
            if (r < 0) {
                int histIdx = historyCount + r;
                if (histIdx >= 0 && histIdx < historyCount && c < (int)history_[histIdx].size()) {
                    cell = history_[histIdx][c];
                }
            } else {
                vterm_screen_get_cell(vts_, {r, c}, &cell);
            }

            if (cell.chars[0]) {
                auto cp = static_cast<char32_t>(cell.chars[0]);
                lineText.append(QString::fromUcs4(&cp, 1));
            } else {
                lineText.append(' ');
            }
        }
        result.append(lineText.trimmed());
        if (r < p2.row) result.append('\n');
    }

    if (!result.isEmpty()) {
        QGuiApplication::clipboard()->setText(result);
    }
}

void TermWidget::resizeEvent(QResizeEvent*) {
    int newCols = std::max(10, width() / charW_);
    int newRows = std::max(3, height() / charH_);
    if (newCols != cols_ || newRows != rows_) {
        cols_ = newCols;
        rows_ = newRows;
        vterm_set_size(vt_, rows_, cols_);
        vterm_screen_flush_damage(vts_);
        pty_.resize(cols_, rows_);
        emit stateChanged(this);
    }
}

void TermWidget::sendMouseEvent(int button, int col, int row, bool isRelease) {
    char flag = isRelease ? 'm' : 'M';
    char buf[64];
    int len = snprintf(buf, sizeof(buf), "\x1b[<%d;%d;%d%c", button, col + 1, row + 1, flag);
    pty_.writeData(buf, len);
}

void TermWidget::mousePressEvent(QMouseEvent* event) {
    setFocus();

    if ((event->modifiers() & Qt::ControlModifier) && event->button() == Qt::LeftButton) {
        if (hoveredTarget_.has_value()) {
            openTarget(hoveredTarget_->text);
            return;
        }
    }

    bool shiftHeld = event->modifiers() & Qt::ShiftModifier;
    if (mouseProtocol_ > 0 && !shiftHeld) {
        int col = std::clamp((int)(event->position().x() / charW_), 0, cols_ - 1);
        int row = std::clamp((int)(event->position().y() / charH_), 0, rows_ - 1);
        int btn = 0;
        if (event->button() == Qt::MiddleButton) btn = 1;
        else if (event->button() == Qt::RightButton) btn = 2;

        sendMouseEvent(btn, col, row, false);
        return;
    }

    if (event->button() == Qt::LeftButton) {
        isSelecting_ = true;
        selStart_ = pixelToCell(event->pos());
        selEnd_ = selStart_;
        update();
    }
}

void TermWidget::mouseMoveEvent(QMouseEvent* event) {
    bool ctrlHeld = event->modifiers() & Qt::ControlModifier;
    checkTargetHover(event->pos(), ctrlHeld);

    bool shiftHeld = event->modifiers() & Qt::ShiftModifier;
    if (mouseProtocol_ > 0 && !shiftHeld && (event->buttons() & Qt::LeftButton)) {
        int col = std::clamp((int)(event->position().x() / charW_), 0, cols_ - 1);
        int row = std::clamp((int)(event->position().y() / charH_), 0, rows_ - 1);
        sendMouseEvent(32, col, row, false);
        return;
    }

    if (isSelecting_) {
        selEnd_ = pixelToCell(event->pos());
        update();
    }
}

void TermWidget::mouseReleaseEvent(QMouseEvent* event) {
    bool shiftHeld = event->modifiers() & Qt::ShiftModifier;
    if (mouseProtocol_ > 0 && !shiftHeld) {
        int col = std::clamp((int)(event->position().x() / charW_), 0, cols_ - 1);
        int row = std::clamp((int)(event->position().y() / charH_), 0, rows_ - 1);
        int btn = 0;
        if (event->button() == Qt::MiddleButton) btn = 1;
        else if (event->button() == Qt::RightButton) btn = 2;

        sendMouseEvent(btn, col, row, true);
        return;
    }

    if (event->button() == Qt::LeftButton && isSelecting_) {
        isSelecting_ = false;
        selEnd_ = pixelToCell(event->pos());
        update();
    }
}

void TermWidget::wheelEvent(QWheelEvent* event) {
    if (event->modifiers() & Qt::ControlModifier) {
        if (event->angleDelta().y() > 0) zoomIn();
        else if (event->angleDelta().y() < 0) zoomOut();
        return;
    }

    int delta = event->angleDelta().y();
    if (delta == 0) return;

    bool shiftHeld = event->modifiers() & Qt::ShiftModifier;
    if (mouseProtocol_ > 0 && !shiftHeld) {
        int col = std::clamp((int)(event->position().x() / charW_), 0, cols_ - 1);
        int row = std::clamp((int)(event->position().y() / charH_), 0, rows_ - 1);
        int btn = (delta > 0) ? 64 : 65;
        sendMouseEvent(btn, col, row, false);
        return;
    }

    int step = (delta > 0) ? 3 : -3;
    int maxScroll = static_cast<int>(history_.size());
    int newOffset = std::clamp(scrollOffset_ + step, 0, maxScroll);

    if (newOffset != scrollOffset_) {
        scrollOffset_ = newOffset;
        update();
    }
}

void TermWidget::keyPressEvent(QKeyEvent* event) {
    auto mods = event->modifiers();
    int key = event->key();

    // 1. Игнорируем нажатия чистых модификаторов (чтобы не сбивать выделение)
    if (key == Qt::Key_Control) {
        checkTargetHover(mapFromGlobal(QCursor::pos()), true);
        return;
    }
    if (key == Qt::Key_Shift || key == Qt::Key_Alt || key == Qt::Key_Meta) {
        return;
    }

    // 2. Копирование по Ctrl+Shift+C или просто Ctrl+C при наличии выделения
    bool isCtrlC = (mods & Qt::ControlModifier) && (key == Qt::Key_C);
    bool hasSelection = selStart_.has_value() && selEnd_.has_value() && (*selStart_ != *selEnd_);

    if (isCtrlC && hasSelection) {
        copySelectionToClipboard();
        selStart_.reset();
        selEnd_.reset();
        update();
        return;
    }

    // 3. Вставка по Ctrl+Shift+V
    if ((mods & Qt::ControlModifier) && (mods & Qt::ShiftModifier) && key == Qt::Key_V) {
        QClipboard* clipboard = QGuiApplication::clipboard();
        QByteArray text = clipboard->text().toUtf8();
        if (!text.isEmpty()) {
            scrollToBottom();
            pty_.writeData(text.data(), text.size());
        }
        return;
    }

    // 4. Скроллинг через Shift+PageUp / Shift+PageDown
    if (mods & Qt::ShiftModifier) {
        if (key == Qt::Key_PageUp) {
            scrollOffset_ = std::min(scrollOffset_ + rows_, static_cast<int>(history_.size()));
            update();
            return;
        } else if (key == Qt::Key_PageDown) {
            scrollOffset_ = std::max(0, scrollOffset_ - rows_);
            update();
            return;
        }
    }

    // 5. Принятие подсказки по Tab / Стрелке вправо
    if ((key == Qt::Key_Right || key == Qt::Key_Tab) && !currentSuggestion_.isEmpty()) {
        QByteArray text = currentSuggestion_.toUtf8();
        currentInputBuffer_ += currentSuggestion_;
        currentSuggestion_.clear();
        pty_.writeData(text.data(), text.size());
        update();
        return;
    }

    // Любой дальнейший ввод сбрасывает скролл и снимает выделение
    scrollToBottom();
    if (hasSelection) {
        selStart_.reset();
        selEnd_.reset();
        update();
    }

    // 6. Ведение буфера ввода для подсказок
    if (key == Qt::Key_Return || key == Qt::Key_Enter) {
        QString cmd = currentInputBuffer_.trimmed();
        if (!cmd.isEmpty()) {
            if (std::find(cmdHistory_.begin(), cmdHistory_.end(), cmd) == cmdHistory_.end()) {
                cmdHistory_.push_back(cmd);
            }
        }
        currentInputBuffer_.clear();
        currentSuggestion_.clear();
    } else if ((mods & Qt::ControlModifier) && key == Qt::Key_C) {
        currentInputBuffer_.clear();
        currentSuggestion_.clear();
    } else if (key == Qt::Key_Backspace) {
        if (!currentInputBuffer_.isEmpty()) {
            currentInputBuffer_.chop(1);
            updateSuggestion();
        }
    } else {
        QString txt = event->text();
        if (!txt.isEmpty() && txt.at(0).isPrint()) {
            currentInputBuffer_ += txt;
            updateSuggestion();
        }
    }

    // 7. Стандартные Ctrl комбинации терминала (Ctrl+A ... Ctrl+Z)
    if ((mods & Qt::ControlModifier) && !(mods & Qt::ShiftModifier)) {
        if (key >= Qt::Key_A && key <= Qt::Key_Z) {
            char ctrlByte = static_cast<char>(key - Qt::Key_A + 1);
            pty_.writeData(&ctrlByte, 1);
            return;
        }
        if (key == Qt::Key_BracketLeft) {
            pty_.writeData("\x1b", 1);
            return;
        }
    }

    // 8. Функциональные клавиши F1-F12
    if (key >= Qt::Key_F1 && key <= Qt::Key_F12) {
        const char* fkeys[] = {
            "\x1bOP", "\x1bOQ", "\x1bOR", "\x1bOS",
            "\x1b[15~", "\x1b[17~", "\x1b[18~", "\x1b[19~",
            "\x1b[20~", "\x1b[21~", "\x1b[23~", "\x1b[24~"
        };
        const char* seq = fkeys[key - Qt::Key_F1];
        pty_.writeData(seq, strlen(seq));
        return;
    }

    // 9. Навигация и спецклавиши
    switch (key) {
        case Qt::Key_Up:        pty_.writeData("\x1b[A", 3); return;
        case Qt::Key_Down:      pty_.writeData("\x1b[B", 3); return;
        case Qt::Key_Right:     pty_.writeData("\x1b[C", 3); return;
        case Qt::Key_Left:      pty_.writeData("\x1b[D", 3); return;
        case Qt::Key_Home:      pty_.writeData("\x1b[H", 3); return;
        case Qt::Key_End:       pty_.writeData("\x1b[F", 3); return;
        case Qt::Key_Insert:    pty_.writeData("\x1b[2~", 4); return;
        case Qt::Key_Delete:    pty_.writeData("\x1b[3~", 4); return;
        case Qt::Key_PageUp:    pty_.writeData("\x1b[5~", 4); return;
        case Qt::Key_PageDown:  pty_.writeData("\x1b[6~", 4); return;
        case Qt::Key_Return:
        case Qt::Key_Enter:     pty_.writeData("\r", 1); return;
        case Qt::Key_Backspace: pty_.writeData("\x7f", 1); return;
        case Qt::Key_Tab:       pty_.writeData("\t", 1); return;
        case Qt::Key_Escape:    pty_.writeData("\x1b", 1); return;
        default: break;
    }

    // 10. Обычный ввод текста
    QByteArray bytes = event->text().toUtf8();
    if (!bytes.isEmpty()) {
        pty_.writeData(bytes.data(), bytes.size());
    }
}

void TermWidget::keyReleaseEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Control) {
        checkTargetHover(mapFromGlobal(QCursor::pos()), false);
    }
    QWidget::keyReleaseEvent(event);
}
