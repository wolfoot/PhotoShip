param(
    [Parameter(Mandatory = $true)][string]$QtPrefix,
    [switch]$Installer,
    [string]$ZlibToolchain = $env:CMAKE_TOOLCHAIN_FILE
)
$ErrorActionPreference = "Stop"
$projectDir = Split-Path -Parent $PSScriptRoot
$QtPrefix = (Resolve-Path $QtPrefix).Path
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsDir = & $vswhere -latest -version '[17.0,18.0)' -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsDir) { throw "Visual Studio 2022 C++ tools are required." }
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    $env:PATH = "$vsDir\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;$env:PATH"
}
$crtDir = Get-ChildItem "$vsDir\VC\Redist\MSVC\*\x64\Microsoft.VC143.CRT" -Directory |
    Sort-Object FullName -Descending | Select-Object -First 1
if (-not $crtDir) { throw "Visual C++ x64 redistributable DLLs were not found." }
$env:PATH = "$QtPrefix\bin;$env:PATH"
$env:QT_PLUGIN_PATH = "$QtPrefix\plugins"
Push-Location $projectDir
try {
    $zlibOptions = @()
    if ($ZlibToolchain) { $zlibOptions += "-DCMAKE_TOOLCHAIN_FILE=$ZlibToolchain", "-DVCPKG_TARGET_TRIPLET=x64-windows-static-md" }
    cmake -S . -B build-windows -G "Visual Studio 17 2022" -A x64 "-DCMAKE_PREFIX_PATH=$QtPrefix" @zlibOptions
    if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed" }
    cmake --build build-windows --config Release --parallel 4
    if ($LASTEXITCODE -ne 0) { throw "Compilation failed" }
    Copy-Item "$($crtDir.FullName)\*.dll" "build-windows\Release" -Force
    $env:QT_QPA_PLATFORM = "offscreen"
    ctest --test-dir build-windows -C Release --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw "Editor tests failed" }
    Remove-Item Env:\QT_QPA_PLATFORM -ErrorAction SilentlyContinue
    $deployDir = Join-Path $projectDir "dist\windows"
    if (Test-Path -LiteralPath $deployDir) {
        if ((Resolve-Path -LiteralPath $deployDir).Path -ne "$projectDir\dist\windows") { throw "Unexpected deployment directory" }
        Remove-Item -LiteralPath $deployDir -Recurse -Force
    }
    New-Item -ItemType Directory -Force $deployDir | Out-Null
    Copy-Item "build-windows\Release\photoship.exe" $deployDir -Force
    & "$QtPrefix\bin\windeployqt.exe" --release --no-translations --no-compiler-runtime "$deployDir\photoship.exe"
    if ($LASTEXITCODE -ne 0) { throw "Qt deployment failed" }
    Copy-Item "$($crtDir.FullName)\*.dll" $deployDir -Force
    Copy-Item LICENSE,THIRD_PARTY_NOTICES.md,README*.md $deployDir -Force
    Copy-Item third_party\compositor\LICENSE "$deployDir\Compositor-LICENSE" -Force
    $licenseDir = Join-Path $deployDir "licenses"
    New-Item -ItemType Directory -Force $licenseDir | Out-Null
    Copy-Item "packaging\qt-licenses\*" $licenseDir -Force
    Copy-Item "packaging\zlib-LICENSE" $licenseDir -Force
    foreach ($source in @("$QtPrefix\licenses", "$QtPrefix\..\..\Licenses", "$QtPrefix\doc\global")) {
        if (Test-Path $source) {
            Get-ChildItem $source -Recurse -File | Where-Object { $_.Name -match 'LICENSE|COPYING|LGPL|GPL|license' } | ForEach-Object {
                Copy-Item $_.FullName $licenseDir -Force
            }
        }
    }
    if (-not (Get-ChildItem $licenseDir -File)) { throw "Qt license files were not found. Install/copy the Qt license notices before distribution." }
    Compress-Archive -Path "$deployDir\*" -DestinationPath "dist\PhotoShip-0.2.1-windows-x64.zip" -Force
    if ($Installer) {
        $compiler = Get-Command ISCC.exe -ErrorAction SilentlyContinue
        $compilerPath = if ($compiler) { $compiler.Source } else { $null }
        if (-not $compiler) {
            $candidate = "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe"
            if (Test-Path $candidate) { $compilerPath = $candidate }
        }
        if (-not $compilerPath) { throw "Inno Setup 6 is required to build the installer; the portable ZIP is ready." }
        & $compilerPath packaging\windows.iss
        if ($LASTEXITCODE -ne 0) { throw "Installer compilation failed" }
    }
} finally { Pop-Location }
