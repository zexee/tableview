# tableview

轻量级跨平台表格查看器，使用 C++ 和 Qt Widgets 构建。

Lightweight cross-platform table viewer built with C++ and Qt Widgets.

## 功能 / Features

- 打开 `.dbf`、`.csv`、`.xlsx` 文件。 / Open `.dbf`, `.csv`, and `.xlsx` files.
- 直接编辑表格单元格。 / Edit table cells directly.
- 保存 DBF 文件，使用 Visual FoxPro 签名 `0x30`。 / Save DBF files with Visual FoxPro signature `0x30`.
- 使用 `Ctrl+Delete` 切换 DBF 删除标记。 / Toggle DBF delete marks with `Ctrl+Delete`.
- DBF/CSV 支持 GBK 与 UTF-8 手动切换。 / Switch GBK / UTF-8 decoding for DBF and CSV.
- CSV 支持分隔符和首行表头选项。 / Configure CSV delimiter and first-row header mode.
- 支持表达式过滤，例如 `AGE>=18`、`NAME LIKE '张%'`、`A=1 OR B=2`。 / Filter rows with expressions such as `AGE>=18`, `NAME LIKE '张%'`, and `A=1 OR B=2`.
- 导出当前可见行到 CSV。 / Export visible rows to CSV.
- 使用 Qt 原生文件打开/保存对话框。 / Use native Qt file dialogs for opening and exporting files.
- 单元格内容显示不下时显示 tooltip，状态栏显示当前单元格完整内容和当前文件路径。 / Show tooltips for truncated cells; the status bar shows the current cell value and full file path.

## 开发 / Development

先安装 Qt 6 Widgets 开发文件和 zlib。Debian/Ubuntu 示例：

Install Qt 6 Widgets development files and zlib first. Debian/Ubuntu example:

```bash
sudo apt install qt6-base-dev zlib1g-dev cmake ninja-build
```

配置、构建和运行：

Configure, build, and run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -G Ninja
cmake --build build
./build/tableview
./build/tableview path/to/file.dbf
```

## 发布构建 / Release Build

每个平台分别构建应用：

Build one app per target platform:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -G Ninja
cmake --build build
```

Linux 输出为 `build/tableview`，Windows 输出为 `build/tableview.exe`。

The output is `build/tableview` on Linux or `build/tableview.exe` on Windows.

Qt 通常以“可执行文件 + Qt 共享库 + plugins”的形式发布。Windows 使用 `windeployqt`，macOS 使用 `macdeployqt`，Linux 建议使用 AppImage/Flatpak 或依赖系统 Qt 包。

Qt normally deploys as an executable plus Qt shared libraries and plugins. Use `windeployqt` on Windows, `macdeployqt` on macOS, or AppImage/Flatpak/system Qt packaging on Linux.

## 持续集成 / CI

`.github/workflows/ci.yml` 在 Linux、Windows、macOS 上构建本项目。每个平台用 `scripts/build-static-qt.sh`（Windows 用 `scripts/build-static-qt.ps1`）从源码编译静态 Qt 6（仅 qtbase），并通过 GitHub Actions 缓存 `qt6-static`，缓存键包含 Qt 版本和构建脚本哈希，命中后跳过重复编译。

The workflow in `.github/workflows/ci.yml` builds on Linux, Windows, and macOS. Each runner builds a static Qt 6 (qtbase only) from source via `scripts/build-static-qt.sh` (or `scripts/build-static-qt.ps1` on Windows), cached as `qt6-static` so later runs skip the rebuild.

推送到 `master` 或开 PR 时触发构建并上传各平台二进制为 artifact。推送 `v*` 标签时额外创建 GitHub Release 并附带各平台压缩包。

Pushes to `master` and pull requests build and upload per-platform binaries as artifacts. Pushing a `v*` tag also creates a GitHub Release with per-platform archives.

Windows 上应用本身的 zlib 依赖通过 vcpkg 静态三元组 `x64-windows-static` 提供；若出现 CRT 不匹配的链接错误，需让静态 Qt 与 zlib 的运行时库设置保持一致。

On Windows the app's zlib dependency comes from the vcpkg `x64-windows-static` triplet; if you hit CRT mismatch link errors, align the static Qt and zlib runtime-library settings.

## 字体 / Fonts

界面文字使用英文，但文件数据可能包含中文。Qt 使用系统字体和输入法；只要目标系统安装了 CJK 字体，中文内容应正常显示。

The UI labels are English, but file data may contain Chinese text. Qt uses system fonts and input methods, so Chinese content should render normally when the target OS has a CJK font installed.

## 项目结构 / Project Layout

- `src/main.cpp` - 程序入口。 / Application entry point.
- `src/MainWindow.*` - Qt Widgets 界面、菜单、工具栏、文件对话框、快捷键。 / Qt Widgets UI, menus, toolbar, file dialogs, shortcuts.
- `src/TableModel.*` - 用于编辑和过滤显示的 `QAbstractTableModel`。 / `QAbstractTableModel` for table editing and visible-row filtering.
- `src/DbfReader.*` - DBF 读写。 / DBF read/write.
- `src/CsvReader.*` - CSV 读取和导出。 / CSV read/export.
- `src/XlsxReader.*` 与 `src/ZipReader.*` - 最小 `.xlsx` ZIP/OpenXML 读取实现。 / Minimal `.xlsx` ZIP/OpenXML reader.
- `src/Codec.*` - GBK / UTF-8 检测和转换。 / GBK / UTF-8 detection and conversion.
- `src/Filter.*` - 简单行过滤表达式求值。 / Simple row filter expression evaluator.
