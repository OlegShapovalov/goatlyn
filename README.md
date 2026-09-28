# 🐐 Goatlyn Terminal

Fast and lightweight Qt6/C++ terminal emulator.

## Features
- Tabs & Splits (Ctrl+Shift+T / Ctrl+Shift+D)
- Fish-style Autosuggestions
- Interactive links & files (Ctrl+Click)
- Fast search (Ctrl+Shift+F)
- Settings dialog (Ctrl+,)

## Build
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
sudo cmake --install build
```
