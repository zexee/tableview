# tableview

Cross-platform DBF / CSV / Excel lightweight viewer built with C++ and Qt Widgets.

## Features

- Open `.dbf`, `.csv`, and `.xlsx` files.
- Edit table cells in memory.
- Save DBF files with Visual FoxPro signature `0x30`.
- Toggle DBF delete marks with `Ctrl+Delete`.
- Switch GBK / UTF-8 decoding for DBF and CSV.
- Configure CSV delimiter and first-row header mode.
- Filter rows with expressions such as `AGE>=18`, `NAME LIKE '张%'`, and `A=1 OR B=2`.
- Export visible rows to CSV.
- Native Qt file dialogs for opening and exporting files.

## Development

Install Qt 6 Widgets development files and zlib first. On Debian/Ubuntu:

```bash
sudo apt install qt6-base-dev zlib1g-dev cmake ninja-build
```

Configure, build, and run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -G Ninja
cmake --build build
./build/tableview
./build/tableview path/to/file.dbf
```

## Release Build

Build one app per target platform:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -G Ninja
cmake --build build
```

The output is under `build/tableview` on Linux or `build/tableview.exe` on Windows.
Qt normally deploys as an executable plus Qt shared libraries and plugins. Use `windeployqt` on Windows, `macdeployqt` on macOS, or AppImage/Flatpak/system Qt packaging on Linux.

## Fonts

The UI labels are English, but file data may contain Chinese text. Qt uses system fonts and input methods, so Chinese content should render normally when the target OS has a CJK font installed.

## Project Layout

- `src/main.cpp` - application entry point.
- `src/MainWindow.*` - Qt Widgets UI, menus, toolbar, file dialogs, shortcuts.
- `src/TableModel.*` - `QAbstractTableModel` for table editing and visible-row filtering.
- `src/DbfReader.*` - DBF read/write.
- `src/CsvReader.*` - CSV read/export.
- `src/XlsxReader.*` and `src/ZipReader.*` - minimal `.xlsx` ZIP/OpenXML reader.
- `src/Codec.*` - GBK / UTF-8 detection and conversion.
- `src/Filter.*` - simple row filter expression evaluator.
