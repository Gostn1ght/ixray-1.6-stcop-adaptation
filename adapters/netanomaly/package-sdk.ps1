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
Put a separate working copy of your game data here, or configure a dedicated writable workspace.
Do not point an editor session at a game checkout that you need to keep unchanged.
"@ | Set-Content -LiteralPath (Join-Path $gameDataDirectory "README-ASSETS.txt") -Encoding utf8

@"
NetAnomaly SDK portable package

Launch Start-LevelEditor.cmd. The Level Editor and utility executables are built from the
IX-Ray SDK source. NetAnomaly Python tools are installed in plugins/netanomaly/ and are
available through the Level Editor Plugins menu.

The archive contains no game assets. fs.ltx maps the editor to this package's own gamedata
folder; it does not point into your NetAnomaly checkout. To edit game data, prepare a separate
writable workspace and configure the filesystem paths for that workspace first.

The Python plugins can also be run directly with Python 3.9 or newer; see
README-NetAnomaly-SDK.md for commands and behavior.
"@ | Set-Content -LiteralPath (Join-Path $packageRoot "README-QUICKSTART-RU.txt") -Encoding utf8

@'
@echo off
setlocal
cd /d "%~dp0"
start "" "%~dp0LevelEditor.exe"
'@ | Set-Content -LiteralPath (Join-Path $packageRoot "Start-LevelEditor.cmd") -Encoding ascii

Write-Output "Packaged NetAnomaly SDK: $packageRoot"
Write-Output "Runtime files: $($runtimeFiles.Count)"
