# Repository Guidelines

## Project Structure & Module Organization

This is a C++/Qt Widgets desktop app for viewing DBF / CSV / XLSX files.

- `src/main.cpp` starts the Qt application.
- `src/MainWindow.*` contains menus, toolbar actions, file dialogs, and shortcuts.
- `src/TableModel.*` implements `QAbstractTableModel` for editable table data.
- `src/DataTypes.h` defines shared table, row, column, format, and encoding types.
- `src/DbfReader.*`, `src/CsvReader.*`, and `src/XlsxReader.*` contain format-specific I/O.
- `src/ZipReader.*` is a small ZIP reader used by `.xlsx` support.
- `src/Codec.*` handles GBK / UTF-8 detection and conversion.
- `src/Filter.*` evaluates row filter expressions.

Keep parsing and file-writing code outside `MainWindow` so it can be tested without launching the GUI.

## Build, Test, and Development Commands

Run commands from the repository root:

- `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/home/z/code/tv/qt6-static -G Ninja` - configure with static Qt6.
- `cmake --build build` - compile the app.
- `./build/tableview` - launch the app on Linux.
- `./build/tableview path/to/file.dbf` - launch and open a file.

Static Qt6 (6.8.3, qtbase only) is installed at `qt6-static/`. It was built with `-static -release -no-opengl -no-icu -no-glib` and bundled zlib, pcre2, libpng, libjpeg, harfbuzz. The project also statically links zlib, libgcc, and libstdc++.

## Coding Style & Naming Conventions

Use modern C++17 and Qt idioms. Keep modules small and name functions by behavior, for example `readDbfFile`, `exportCsvFile`, or `filterRows`.

Use `PascalCase` for classes and types, `camelCase` for functions and local variables, and `m_` prefixes for private member fields. Use enum classes for file formats and encodings instead of stringly typed state.

## Testing Guidelines

Add tests for readers and filters once a Qt test target is introduced. Prefer deterministic temp-file tests for DBF/CSV/XLSX parsing and DBF round-tripping.

Important coverage areas are DBF round-tripping, GBK / UTF-8 decoding, CSV quoting, XLSX cell parsing, numeric validation, delete marks, and filter expressions.

## Commit & Pull Request Guidelines

There is no local Git repository in this workspace, so no repository-specific commit history can be inferred. Use concise imperative commit messages such as `Add DBF writer` or `Improve row filtering`.

Pull requests should include a summary, validation commands, and screenshots or screen recordings for visible UI changes.

## Agent-Specific Instructions

Do not modify `../dbfview`; it is only a reference implementation. Develop this C++/Qt version in the current directory.
