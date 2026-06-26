<#
  build-installer.ps1 - builds the standalone InternetGuard installer .exe

  Produces installer\install-internet-guard-<suffix>.exe with the plugin DLL embedded.
  The installer itself has no Qt dependency (pure Win32, statically linked).

  Usage (from the repo root or the installer folder):
      # Qt 5 build:
      pwsh -File installer\build-installer.ps1 -PluginDll build-qt5\internet-guard-qt5.dll
      # Qt 6 build:
      pwsh -File installer\build-installer.ps1 -PluginDll build-qt6\internet-guard-qt6.dll

  Parameters:
      -PluginDll   Path to the plugin DLL to embed (determines the embedded name automatically)
      -Mingw       Path to the MinGW bin folder (defaults to MSYS2 MinGW64, then Qt 5.12 MinGW)
      -OutputExe   Output exe name (defaults to install-<DLL basename without .dll>.exe)
#>
param(
    [string]$PluginDll = "$PSScriptRoot\..\build-qt5\internet-guard-qt5.dll",
    [string]$Mingw     = "",
    [string]$OutputExe = ""
)

$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

if (-not (Test-Path $PluginDll)) {
    throw "Plugin DLL non trovata: $PluginDll`nCompilare prima il plugin (vedi README)."
}

# Derive the DLL name (e.g. "internet-guard-qt6.dll") and installer exe name.
$DllName = [System.IO.Path]::GetFileName($PluginDll)
if (-not $OutputExe) {
    $base    = [System.IO.Path]::GetFileNameWithoutExtension($DllName)  # e.g. internet-guard-qt6
    $OutputExe = "install-$base.exe"
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

Write-Host "DLL da incorporare : $DllName"
Write-Host "Exe di output      : $OutputExe"
Write-Host "Toolchain MinGW    : $Mingw"

# windres reads the RCDATA payload relative to this folder.
Copy-Item $PluginDll ".\internet-guard.dll" -Force

Write-Host "Compilazione risorse (windres)..."
& $windres installer.rc -O coff -o installer_res.o
if ($LASTEXITCODE -ne 0) { throw "windres fallito." }

Write-Host "Compilazione e link installer..."
& $gpp -std=c++14 -O2 -municode -mwindows `
    "-DPLUGIN_NAME=L\`"$DllName\`"" `
    installer.cpp installer_res.o `
    -o $OutputExe `
    -static -static-libgcc -static-libstdc++ `
    -lshell32 -lole32 -ladvapi32 -luser32
if ($LASTEXITCODE -ne 0) { throw "g++ fallito." }

Remove-Item installer_res.o, ".\internet-guard.dll" -ErrorAction SilentlyContinue
Write-Host "Fatto: $(Join-Path $PSScriptRoot $OutputExe)"
