<#
.SYNOPSIS
    Build a static Qt 6 (qtbase only) for tableview on Windows (MSVC).

.DESCRIPTION
    Mirrors scripts/build-static-qt.sh. Must run inside an MSVC developer
    environment (cl.exe and ninja on PATH); CI sets this up with
    ilammy/msvc-dev-cmd before invoking this script.

    Environment variables:
      QT_VERSION   Qt version to build (default 6.8.3)
      QT_PREFIX    install prefix (default <repo>\qt6-static)
#>
$ErrorActionPreference = "Stop"

$QtVersion = if ($env:QT_VERSION) { $env:QT_VERSION } else { "6.8.3" }
$QtMajorMinor = $QtVersion -replace '\.\d+$', ''

$RepoDir = (Resolve-Path "$PSScriptRoot\..").Path
$QtPrefix = if ($env:QT_PREFIX) { $env:QT_PREFIX } else { Join-Path $RepoDir "qt6-static" }
$QtSrcDir = Join-Path $RepoDir "qt6-src"
$QtBuildDir = Join-Path $RepoDir "qt6-build"

if (Test-Path (Join-Path $QtPrefix "lib\cmake\Qt6\Qt6Config.cmake")) {
    Write-Host "Static Qt $QtVersion already installed at $QtPrefix; skipping."
    exit 0
}

$Tarball = "qtbase-everywhere-src-$QtVersion.tar.xz"
$TarballPath = Join-Path $RepoDir $Tarball
if (-not (Test-Path $TarballPath)) {
    Write-Host "Downloading $Tarball ..."
    $base = "https://download.qt.io/archive/qt/$QtMajorMinor/$QtVersion/submodules"
    curl.exe -fSL "$base/$Tarball" -o "$TarballPath"
    if ($LASTEXITCODE -ne 0) { throw "Download failed" }
}

if (Test-Path $QtSrcDir) { Remove-Item -Recurse -Force $QtSrcDir }
New-Item -ItemType Directory -Force -Path $QtSrcDir | Out-Null
Write-Host "Extracting sources ..."
tar.exe -xf "$TarballPath" -C "$QtSrcDir" --strip-components=1
if ($LASTEXITCODE -ne 0) { throw "Extract failed" }

if (Test-Path $QtBuildDir) { Remove-Item -Recurse -Force $QtBuildDir }
New-Item -ItemType Directory -Force -Path $QtBuildDir | Out-Null

Write-Host "Configuring Qt $QtVersion (static) ..."
Push-Location $QtBuildDir
try {
    & "$QtSrcDir\configure.bat" `
        -static `
        -release `
        -opensource -confirm-license `
        -prefix "$QtPrefix" `
        -no-opengl `
        -no-icu `
        -no-dbus `
        -qt-zlib `
        -qt-libpng `
        -qt-libjpeg `
        -qt-pcre `
        -qt-harfbuzz `
        -qt-freetype `
        -nomake examples `
        -nomake tests `
        -nomake benchmarks
    if ($LASTEXITCODE -ne 0) { throw "configure failed" }
}
finally {
    Pop-Location
}

Write-Host "Building Qt $QtVersion ..."
cmake --build "$QtBuildDir" --parallel
if ($LASTEXITCODE -ne 0) { throw "build failed" }

Write-Host "Installing Qt $QtVersion to $QtPrefix ..."
cmake --install "$QtBuildDir"
if ($LASTEXITCODE -ne 0) { throw "install failed" }

Write-Host "Static Qt $QtVersion ready at $QtPrefix"
