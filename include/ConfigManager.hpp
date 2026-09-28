#pragma once
#include <QObject>
#include <QString>
#include <QColor>
#include <map>
#include <vector>

struct ThemePalette {
    QString name;
    QColor background;
    QColor foreground;
    QColor cursor;
    std::vector<QColor> ansiColors;
    bool isCustom = false;
};

class ConfigManager : public QObject {
    Q_OBJECT
public:
    static ConfigManager& instance();

    void load();
    void save();

    QString fontFamily() const { return fontFamily_; }
    int fontSize() const { return fontSize_; }
    double backgroundOpacity() const { return opacity_; }
    QString cursorShape() const { return cursorShape_; }
    bool cursorBlink() const { return cursorBlink_; }
    bool showBanner() const { return showBanner_; }
    QString shell() const { return shell_; }
    QString themeName() const { return themeName_; }

    const ThemePalette& currentTheme() const;
    const std::vector<ThemePalette>& allThemes() const { return themes_; }

    QString getKeybinding(const QString& actionId, const QString& defaultSeq) const;
    void setKeybinding(const QString& actionId, const QString& seq);
    const std::map<QString, QString>& allKeybindings() const { return keybindings_; }

    std::vector<QString> getAvailableShells() const;

    void setFontFamily(const QString& fam);
    void setFontSize(int sz);
    void setBackgroundOpacity(double op);
    void setCursorShape(const QString& shape);
    void setCursorBlink(bool blink);
    void setShowBanner(bool show);
    void setShell(const QString& sh);
    void setTheme(const QString& name);
    void addOrUpdateCustomTheme(const ThemePalette& theme);
    bool removeCustomTheme(const QString& name);

signals:
    void configChanged();

private:
    ConfigManager();
    ~ConfigManager() override = default;

    QString fontFamily_ = "Monospace";
    int fontSize_ = 12;
    double opacity_ = 1.0;
    QString cursorShape_ = "block";
    bool cursorBlink_ = true;
    bool showBanner_ = true;
    QString shell_ = "/bin/bash";
    QString themeName_ = "Catppuccin Mocha";

    std::vector<ThemePalette> themes_;
    std::map<QString, QString> keybindings_;

    void initDefaultThemes();
    void initDefaultKeybindings();
    QString getConfigPath() const;
};
