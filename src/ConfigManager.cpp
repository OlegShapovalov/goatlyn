#include "ConfigManager.hpp"
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <fstream>
#include <cstdlib>

void ConfigManager::initDefaultThemes() {
    themes_ = {
        {
            "Catppuccin Mocha",
            QColor("#1e1e2e"), QColor("#cdd6f4"), QColor("#a6e3a1"),
            {
                QColor("#45475a"), QColor("#f38ba8"), QColor("#a6e3a1"), QColor("#f9e2af"),
                QColor("#89b4fa"), QColor("#f5c2e7"), QColor("#94e2d5"), QColor("#bac2de"),
                QColor("#585b70"), QColor("#f38ba8"), QColor("#a6e3a1"), QColor("#f9e2af"),
                QColor("#89b4fa"), QColor("#f5c2e7"), QColor("#94e2d5"), QColor("#a6adc8")
            },
            false
        },
        {
            "Dracula",
            QColor("#282a36"), QColor("#f8f8f2"), QColor("#50fa7b"),
            {
                QColor("#21222c"), QColor("#ff5555"), QColor("#50fa7b"), QColor("#f1fa8c"),
                QColor("#bd93f9"), QColor("#ff79c6"), QColor("#8be9fd"), QColor("#f8f8f2"),
                QColor("#6272a4"), QColor("#ff6e6e"), QColor("#69ff94"), QColor("#ffffa5"),
                QColor("#d6acff"), QColor("#ff92df"), QColor("#a4ffff"), QColor("#ffffff")
            },
            false
        },
        {
            "Tokyo Night",
            QColor("#1a1b26"), QColor("#c0caf5"), QColor("#7aa2f7"),
            {
                QColor("#15161e"), QColor("#f7768e"), QColor("#9ece6a"), QColor("#e0af68"),
                QColor("#7aa2f7"), QColor("#bb9af7"), QColor("#7dcfff"), QColor("#a9b1d6"),
                QColor("#414868"), QColor("#f7768e"), QColor("#9ece6a"), QColor("#e0af68"),
                QColor("#7aa2f7"), QColor("#bb9af7"), QColor("#7dcfff"), QColor("#c0caf5")
            },
            false
        },
        {
            "Nord",
            QColor("#2e3440"), QColor("#eceff4"), QColor("#88c0d0"),
            {
                QColor("#3b4252"), QColor("#bf616a"), QColor("#a3be8c"), QColor("#ebcb8b"),
                QColor("#81a1c1"), QColor("#b48ead"), QColor("#88c0d0"), QColor("#e5e9f0"),
                QColor("#4c566a"), QColor("#bf616a"), QColor("#a3be8c"), QColor("#ebcb8b"),
                QColor("#81a1c1"), QColor("#b48ead"), QColor("#8fbcbb"), QColor("#eceff4")
            },
            false
        },
        {
            "Gruvbox Dark",
            QColor("#282828"), QColor("#ebdbb2"), QColor("#b8bb26"),
            {
                QColor("#282828"), QColor("#cc241d"), QColor("#98971a"), QColor("#d79921"),
                QColor("#458588"), QColor("#b16286"), QColor("#689d6a"), QColor("#a89984"),
                QColor("#928374"), QColor("#fb4934"), QColor("#b8bb26"), QColor("#fabd2f"),
                QColor("#83a598"), QColor("#d3869b"), QColor("#8ec07c"), QColor("#ebdbb2")
            },
            false
        },
        {
            "One Dark",
            QColor("#21252b"), QColor("#abb2bf"), QColor("#98c379"),
            {
                QColor("#1e2127"), QColor("#e06c75"), QColor("#98c379"), QColor("#d19a66"),
                QColor("#61afef"), QColor("#c678dd"), QColor("#56b6c2"), QColor("#abb2bf"),
                QColor("#5c6370"), QColor("#e06c75"), QColor("#98c379"), QColor("#d19a66"),
                QColor("#61afef"), QColor("#c678dd"), QColor("#56b6c2"), QColor("#ffffff")
            },
            false
        }
    };
}

void ConfigManager::initDefaultKeybindings() {
    keybindings_["new_tab"] = "Ctrl+Shift+T";
    keybindings_["split_vertical"] = "Ctrl+Shift+D";
    keybindings_["close_tab"] = "Ctrl+Shift+W";
    keybindings_["find"] = "Ctrl+Shift+F";
    keybindings_["settings"] = "Ctrl+,";
    keybindings_["zoom_in"] = "Ctrl+=";
    keybindings_["zoom_out"] = "Ctrl+-";
    keybindings_["zoom_reset"] = "Ctrl+0";
}

ConfigManager::ConfigManager() {
    const char* sysShell = getenv("SHELL");
    if (sysShell && strlen(sysShell) > 0) {
        shell_ = QString::fromLocal8Bit(sysShell);
    }
    initDefaultThemes();
    initDefaultKeybindings();
    load();
}

ConfigManager& ConfigManager::instance() {
    static ConfigManager inst;
    return inst;
}

const ThemePalette& ConfigManager::currentTheme() const {
    for (const auto& th : themes_) {
        if (th.name == themeName_) return th;
    }
    return themes_[0];
}

QString ConfigManager::getKeybinding(const QString& actionId, const QString& defaultSeq) const {
    auto it = keybindings_.find(actionId);
    if (it != keybindings_.end() && !it->second.isEmpty()) {
        return it->second;
    }
    return defaultSeq;
}

void ConfigManager::setKeybinding(const QString& actionId, const QString& seq) {
    keybindings_[actionId] = seq;
}

std::vector<QString> ConfigManager::getAvailableShells() const {
    std::vector<QString> result;
    std::ifstream file("/etc/shells");
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line[0] == '/') {
            result.push_back(QString::fromStdString(line));
        }
    }
    if (result.empty()) {
        result.push_back("/bin/bash");
        result.push_back("/bin/sh");
    }
    return result;
}

QString ConfigManager::getConfigPath() const {
    const char* home = getenv("HOME");
    QString base = home ? QString::fromLocal8Bit(home) : QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    return base + "/.config/goatling/config.json";
}

void ConfigManager::load() {
    QFile file(getConfigPath());
    if (!file.open(QIODevice::ReadOnly)) return;

    QJsonObject obj = QJsonDocument::fromJson(file.readAll()).object();
    if (obj.contains("fontFamily")) fontFamily_ = obj["fontFamily"].toString();
    if (obj.contains("fontSize")) fontSize_ = obj["fontSize"].toInt();
    if (obj.contains("opacity")) opacity_ = obj["opacity"].toDouble();
    if (obj.contains("cursorShape")) cursorShape_ = obj["cursorShape"].toString();
    if (obj.contains("cursorBlink")) cursorBlink_ = obj["cursorBlink"].toBool();
    if (obj.contains("showBanner")) showBanner_ = obj["showBanner"].toBool();
    if (obj.contains("shell")) shell_ = obj["shell"].toString();
    if (obj.contains("theme")) themeName_ = obj["theme"].toString();

    if (obj.contains("keybindings")) {
        QJsonObject kbObj = obj["keybindings"].toObject();
        for (auto it = kbObj.begin(); it != kbObj.end(); ++it) {
            keybindings_[it.key()] = it.value().toString();
        }
    }

    if (obj.contains("customThemes")) {
        QJsonArray cThemes = obj["customThemes"].toArray();
        for (auto val : cThemes) {
            QJsonObject to = val.toObject();
            ThemePalette th;
            th.name = to["name"].toString();
            th.background = QColor(to["background"].toString());
            th.foreground = QColor(to["foreground"].toString());
            th.cursor = QColor(to["cursor"].toString());
            th.isCustom = true;

            bool exists = false;
            for (auto& item : themes_) {
                if (item.name == th.name && item.isCustom) {
                    item = th;
                    exists = true;
                    break;
                }
            }
            if (!exists) themes_.push_back(th);
        }
    }
}

void ConfigManager::save() {
    QFileInfo fi(getConfigPath());
    fi.dir().mkpath(".");

    QJsonObject obj;
    obj["fontFamily"] = fontFamily_;
    obj["fontSize"] = fontSize_;
    obj["opacity"] = opacity_;
    obj["cursorShape"] = cursorShape_;
    obj["cursorBlink"] = cursorBlink_;
    obj["showBanner"] = showBanner_;
    obj["shell"] = shell_;
    obj["theme"] = themeName_;

    QJsonObject kbObj;
    for (const auto& [action, seq] : keybindings_) {
        kbObj[action] = seq;
    }
    obj["keybindings"] = kbObj;

    QJsonArray cThemes;
    for (const auto& th : themes_) {
        if (th.isCustom) {
            QJsonObject to;
            to["name"] = th.name;
            to["background"] = th.background.name(QColor::HexRgb);
            to["foreground"] = th.foreground.name(QColor::HexRgb);
            to["cursor"] = th.cursor.name(QColor::HexRgb);
            cThemes.append(to);
        }
    }
    obj["customThemes"] = cThemes;

    QFile file(getConfigPath());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    }
    emit configChanged();
}

void ConfigManager::addOrUpdateCustomTheme(const ThemePalette& theme) {
    for (auto& th : themes_) {
        if (th.name == theme.name && th.isCustom) {
            th = theme;
            return;
        }
    }
    ThemePalette copy = theme;
    copy.isCustom = true;
    themes_.push_back(copy);
}

bool ConfigManager::removeCustomTheme(const QString& name) {
    for (auto it = themes_.begin(); it != themes_.end(); ++it) {
        if (it->name == name && it->isCustom) {
            themes_.erase(it);
            if (themeName_ == name) {
                themeName_ = "Catppuccin Mocha";
            }
            save();
            return true;
        }
    }
    return false;
}

void ConfigManager::setFontFamily(const QString& fam) { fontFamily_ = fam; }
void ConfigManager::setFontSize(int sz) { fontSize_ = sz; }
void ConfigManager::setBackgroundOpacity(double op) { opacity_ = op; }
void ConfigManager::setCursorShape(const QString& shape) { cursorShape_ = shape; }
void ConfigManager::setCursorBlink(bool blink) { cursorBlink_ = blink; }
void ConfigManager::setShowBanner(bool show) { showBanner_ = show; }
void ConfigManager::setShell(const QString& sh) { shell_ = sh; }
void ConfigManager::setTheme(const QString& name) { themeName_ = name; }
