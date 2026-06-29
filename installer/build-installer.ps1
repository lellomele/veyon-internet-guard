<#
  build-installer.ps1 - builds the standalone InternetGuard installer .exe

  Produces installer\install-internet-guard.exe with BOTH plugin variants
  (Qt 5 and Qt 6) embedded. At install time the installer detects whether the
  target Veyon uses Qt 5 or Qt 6 and copies the matching library automatically.
  The installer itself has no Qt dependency (pure Win32, statically linked).

  Usage (from the repo root or the installer folder):
      pwsh -File installer\build-installer.ps1 `
          -PluginDllQt5 build-qt5\internet-guard-qt5.dll `
          -PluginDllQt6 build-qt6\internet-guard-qt6.dll

  Parameters:
      -PluginDllQt5  Path to the Qt 5 plugin DLL to embed
      -PluginDllQt6  Path to the Qt 6 plugin DLL to embed
      -Mingw         Path to the MinGW bin folder (defaults to MSYS2 MinGW64, then Qt 5.12 MinGW)
      -OutputExe     Output exe name (defaults to install-internet-guard.exe)
#>
param(
    [string]$PluginDllQt5 = "$PSScriptRoot\..\build-qt5\internet-guard-qt5.dll",
    [string]$PluginDllQt6 = "$PSScriptRoot\..\build-qt6\internet-guard-qt6.dll",
    [string]$Mingw        = "",
    [string]$OutputExe    = "install-internet-guard.exe"
)

$ErrorActionPreference = "Stop"

# Resolve relative DLL paths against the caller's directory *before* we cd into
# the installer folder, otherwise the relative paths would break.
foreach ($name in 'PluginDllQt5', 'PluginDllQt6') {
    $val = Get-Variable -Name $name -ValueOnly
    if ($val -and -not [System.IO.Path]::IsPathRooted($val)) {
        Set-Variable -Name $name -Value (Join-Path (Get-Location).Path $val)
    }
}

Set-Location $PSScriptRoot

if (-not (Test-Path $PluginDllQt5)) {
    throw "Plugin DLL Qt5 non trovata: $PluginDllQt5`nCompilare prima il plugin (vedi README)."
}
if (-not (Test-Path $PluginDllQt6)) {
    throw "Plugin DLL Qt6 non trovata: $PluginDllQt6`nCompilare prima il plugin (vedi README)."
}

# Auto-detect MinGW: prefer MSYS2 MinGW64, fall back to Qt 5.12 toolchain.
if (-not $Mingw) {
    if (Test-Path "C:\msys64\mingw64\bin\g++.exe") {
        $Mingw = "C:\msys64\mingw64\bin"
    } elseif (Test-Path "C:\Qt\Tools\mingw730_64\bin\g++.exe") {
        $Mingw = "C:\Qt\Tools\mingw730_64\bin"
    } else {
        throw "MinGW non trovato. Installare MSYS2 o Qt 5.12 oppure passare -Mingw."
    }
}

$gpp     = Join-Path $Mingw "g++.exe"
$windres = Join-Path $Mingw "windres.exe"

Write-Host "DLL Qt5 da incorporare : $([System.IO.Path]::GetFileName($PluginDllQt5))"
Write-Host "DLL Qt6 da incorporare : $([System.IO.Path]::GetFileName($PluginDllQt6))"
Write-Host "Exe di output          : $OutputExe"
Write-Host "Toolchain MinGW        : $Mingw"

# windres reads the RCDATA payloads relative to this folder; the .rc references
# them by their canonical names, so copy each variant to the expected name.
Copy-Item $PluginDllQt5 ".\internet-guard-qt5.dll" -Force
Copy-Item $PluginDllQt6 ".\internet-guard-qt6.dll" -Force

Write-Host "Compilazione risorse (windres)..."
& $windres installer.rc -O coff -o installer_res.o
if ($LASTEXITCODE -ne 0) { throw "windres fallito." }

Write-Host "Compilazione e link installer..."
& $gpp -std=c++14 -O2 -municode -mwindows `
    installer.cpp installer_res.o `
    -o $OutputExe `
    -static -static-libgcc -static-libstdc++ `
    -lcomctl32 -lversion -lshell32 -lole32 -ladvapi32 -luser32
$gppExit = $LASTEXITCODE

Remove-Item installer_res.o, ".\internet-guard-qt5.dll", ".\internet-guard-qt6.dll" -ErrorAction SilentlyContinue
if ($gppExit -ne 0) { throw "g++ fallito." }
Write-Host "Fatto: $(Join-Path $PSScriptRoot $OutputExe)"
