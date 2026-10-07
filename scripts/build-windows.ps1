param(
    [Parameter(Mandatory = $true)][string]$QtPrefix,
    [switch]$Installer
)
$ErrorActionPreference = "Stop"
$projectDir = Split-Path -Parent $PSScriptRoot
$QtPrefix = (Resolve-Path $QtPrefix).Path
$env:PATH = "$QtPrefix\bin;$env:PATH"
$env:QT_PLUGIN_PATH = "$QtPrefix\plugins"
Push-Location $projectDir
try {
    cmake -S . -B build-windows -G "Visual Studio 17 2022" -A x64 "-DCMAKE_PREFIX_PATH=$QtPrefix"
    if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed" }
    cmake --build build-windows --config Release --parallel 4
    if ($LASTEXITCODE -ne 0) { throw "Compilation failed" }
    $env:QT_QPA_PLATFORM = "offscreen"
    ctest --test-dir build-windows -C Release --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw "Editor tests failed" }
    Remove-Item Env:\QT_QPA_PLATFORM -ErrorAction SilentlyContinue
    $deployDir = Join-Path $projectDir "dist\windows"
    New-Item -ItemType Directory -Force $deployDir | Out-Null
    Copy-Item "build-windows\Release\pixelstudio.exe" $deployDir -Force
    & "$QtPrefix\bin\windeployqt.exe" --release --no-translations "$deployDir\pixelstudio.exe"
    if ($LASTEXITCODE -ne 0) { throw "Qt deployment failed" }
    Copy-Item LICENSE,THIRD_PARTY_NOTICES.md,README.md $deployDir -Force
    Copy-Item third_party\compositor\LICENSE "$deployDir\Compositor-LICENSE" -Force
    $licenseDir = Join-Path $deployDir "licenses"
    New-Item -ItemType Directory -Force $licenseDir | Out-Null
    Copy-Item "packaging\qt-licenses\*" $licenseDir -Force
    foreach ($source in @("$QtPrefix\licenses", "$QtPrefix\..\..\Licenses", "$QtPrefix\doc\global")) {
        if (Test-Path $source) {
            Get-ChildItem $source -Recurse -File | Where-Object { $_.Name -match 'LICENSE|COPYING|LGPL|GPL|license' } | ForEach-Object {
                Copy-Item $_.FullName $licenseDir -Force
            }
        }
    }
    if (-not (Get-ChildItem $licenseDir -File)) { throw "Qt license files were not found. Install/copy the Qt license notices before distribution." }
    Compress-Archive -Path "$deployDir\*" -DestinationPath "dist\PixelStudio-0.1.0-windows-x64.zip" -Force
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
