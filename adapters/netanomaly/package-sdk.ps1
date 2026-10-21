param(
    [Parameter(Mandatory = $true)]
    [string]$BuildRoot,

    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory,

    [ValidateSet("Debug", "RelWithDebInfo", "Release")]
    [string]$Configuration = "RelWithDebInfo"
)

$ErrorActionPreference = "Stop"

$repositoryRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
$buildBin = Join-Path ([System.IO.Path]::GetFullPath($BuildRoot)) "bin\$Configuration"
$outputRoot = [System.IO.Path]::GetFullPath($OutputDirectory)
$packageRoot = Join-Path $outputRoot "NetAnomaly-SDK"

if (-not (Test-Path -LiteralPath (Join-Path $buildBin "LevelEditor.exe") -PathType Leaf)) {
    throw "LevelEditor.exe was not found under $buildBin. Build the NetAnomaly SDK preset first."
}
if (Test-Path -LiteralPath $packageRoot) {
    throw "Package directory already exists; choose a fresh output directory: $packageRoot"
}

New-Item -ItemType Directory -Path $packageRoot -Force | Out-Null
$runtimeFiles = Get-ChildItem -LiteralPath $buildBin -File | Where-Object {
    $_.Extension -in @(".exe", ".dll")
}
if (-not $runtimeFiles) {
    throw "No executable or DLL files found under $buildBin."
}
Copy-Item -LiteralPath $runtimeFiles.FullName -Destination $packageRoot

Get-ChildItem -LiteralPath $repositoryRoot -File -Filter "fs*.ltx" | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination $packageRoot
}
$supportFiles = @("ActorEditorLevel.cform")
foreach ($name in $supportFiles) {
    $source = Join-Path $repositoryRoot $name
    if (Test-Path -LiteralPath $source -PathType Leaf) {
        Copy-Item -LiteralPath $source -Destination $packageRoot
    }
}

$pluginDirectory = Join-Path $packageRoot "plugins\netanomaly"
New-Item -ItemType Directory -Path $pluginDirectory -Force | Out-Null
Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot "plugins") -File -Filter "*.py" |
    Copy-Item -Destination $pluginDirectory
Copy-Item -LiteralPath (Join-Path $PSScriptRoot "README.md") -Destination (Join-Path $packageRoot "README-NetAnomaly-SDK.md")

$gameDataDirectory = Join-Path $packageRoot "gamedata"
New-Item -ItemType Directory -Path $gameDataDirectory -Force | Out-Null
@"
This folder is intentionally empty. No NetAnomaly or third-party game assets are included.
Put a separate writable copy of editor-compatible Call of Pripyat data here.
Do not point an editor session at a game checkout that you need to keep unchanged.
"@ | Set-Content -LiteralPath (Join-Path $gameDataDirectory "README-ASSETS.txt") -Encoding utf8

@"
NetAnomaly SDK portable package

Запуск редактора: Start-LevelEditor.cmd. Исполняемые файлы редактора и утилит собраны из
исходников IX-Ray SDK. Инструменты NetAnomaly на Python находятся в plugins/netanomaly/.

В архиве нет игровых данных. Перед запуском редактор проверяет наличие
gamedata/shaders_xrlc.xr, gamedata/configs/defines.ltx и gamedata/configs/system.ltx.
Поместите сюда отдельную рабочую копию совместимых данных Call of Pripyat. Не запускайте
редактор напрямую на исходной папке NetAnomaly. Конфиги Anomaly/GAMMA могут быть несовместимы
с редактором CoP и вызывать ошибки DLTX.

Python-плагины можно запускать отдельно через Python 3.9 или новее. Команды и описание
поведения см. в README-NetAnomaly-SDK.md.
"@ | Set-Content -LiteralPath (Join-Path $packageRoot "README-QUICKSTART-RU.txt") -Encoding utf8

@'
@echo off
setlocal
cd /d "%~dp0"
set "MISSING=0"
for %%F in ("gamedata\shaders_xrlc.xr" "gamedata\configs\defines.ltx" "gamedata\configs\system.ltx") do (
    if not exist "%%~F" (
        echo Missing required editor data: %%~F
        set "MISSING=1"
    )
)
if "%MISSING%"=="1" (
    echo.
    echo Add a separate, writable copy of compatible Call of Pripyat editor data under this SDK's gamedata folder.
    echo Keep the original game folder unchanged. Anomaly/GAMMA configuration sets may not be compatible with this editor.
    echo.
    pause
    exit /b 2
)
start "" "%~dp0LevelEditor.exe"
'@ | Set-Content -LiteralPath (Join-Path $packageRoot "Start-LevelEditor.cmd") -Encoding ascii

Write-Output "Packaged NetAnomaly SDK: $packageRoot"
Write-Output "Runtime files: $($runtimeFiles.Count)"
