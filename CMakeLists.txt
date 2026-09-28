cmake_minimum_required(VERSION 3.16)
project(Goatlyn LANGUAGES CXX C VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
find_package(PkgConfig REQUIRED)
pkg_check_modules(VTERM REQUIRED vterm)

include_directories(include ${VTERM_INCLUDE_DIRS})

add_executable(goatlyn
    include/MainWindow.hpp
    include/TermWidget.hpp
    include/PtyProcess.hpp
    include/SearchBar.hpp
    include/SettingsDialog.hpp
    include/ConfigManager.hpp
    src/main.cpp
    src/PtyProcess.cpp
    src/TermWidget.cpp
    src/MainWindow.cpp
    src/SearchBar.cpp
    src/SettingsDialog.cpp
    src/ConfigManager.cpp
    resources/resources.qrc
)

target_link_libraries(goatlyn PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    ${VTERM_LIBRARIES}
    util
)

# Правила установки для сборки пакета в Linux / Arch (PKGBUILD)
include(GNUInstallDirs)

install(TARGETS goatlyn
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES goatlyn.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)

install(FILES icons/goatlyn.png
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
)
