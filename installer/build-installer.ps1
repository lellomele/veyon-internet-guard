<#
  build-installer.ps1 - builds the standalone InternetGuard installer .exe

  Produces installer\install-internet-guard.exe with the plugin DLL embedded.
  Uses the MinGW 7.3 toolchain bundled with Qt 5.12.12 (matches the ABI of the
  DLL it embeds, though the installer itself has no Qt dependency).

  Usage (from the repo root or the installer folder):
      pwsh -File installer\build-installer.ps1
      pwsh -File installer\build-installer.ps1 -PluginDll path\to\internet-guard.dll
#>
param(
    [string]$Mingw     = "C:\Qt\Tools\mingw730_64\bin",
    [string]$PluginDll = "$PSScriptRoot\..\build-qt5\internet-guard.dll"
)

$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

if (-not (Test-Path $PluginDll)) {
    throw "Plugin DLL non trovata: $PluginDll`nCompilare prima il plugin (vedi README)."
}

# windres reads the RCDATA payload relative to this folder.
Copy-Item $PluginDll ".\internet-guard.dll" -Force

$gpp     = Join-Path $Mingw "g++.exe"
$windres = Join-Path $Mingw "windres.exe"

Write-Host "Compilazione risorse (windres)..."
& $windres installer.rc -O coff -o installer_res.o

Write-Host "Compilazione e link installer..."
& $gpp -std=c++14 -O2 -municode -mwindows `
    installer.cpp installer_res.o `
    -o install-internet-guard.exe `
    -static -static-libgcc -static-libstdc++ `
    -lshell32 -lole32 -ladvapi32 -luser32

Remove-Item installer_res.o -ErrorAction SilentlyContinue
Write-Host "Fatto: $(Join-Path $PSScriptRoot 'install-internet-guard.exe')"
