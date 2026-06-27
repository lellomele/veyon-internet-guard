# InternetGuard — Veyon "Block / Allow Internet" plugin

**English** | [Italiano](#italiano)

Teacher-side plugin for [Veyon](https://veyon.io/) that adds a button to the
**Veyon Master** console toolbar. When the teacher activates the feature, every
**Veyon Server** on the student computers adds a few Windows Firewall rules
(via `netsh advfirewall`) that block outbound Internet traffic while keeping the
local network (LAN) working. Deactivating the feature removes the rules.

## ⬇️ Download (pre-built)

No need to compile anything: grab the ready-made files from
**[v1.1.0](https://github.com/lellomele/veyon-internet-guard/releases/tag/v1.1.1)**
(Windows only — after installing, restart Veyon Master and Veyon Server).

| File | Veyon | What it is |
|------|-------|------------|
| **[install-internet-guard-qt6.exe](https://github.com/lellomele/veyon-internet-guard/releases/download/v1.1.1/install-internet-guard-qt6.exe)** | 4.10.x (Qt 6) | Recommended installer — picks the Veyon folder and copies the plugin, with self-elevation if needed. |
| **[internet-guard-qt6.dll](https://github.com/lellomele/veyon-internet-guard/releases/download/v1.1.1/internet-guard-qt6.dll)** | 4.10.x (Qt 6) | DLL only — copy manually into `<Veyon folder>\plugins\`. |
| **[install-internet-guard-qt5.exe](https://github.com/lellomele/veyon-internet-guard/releases/download/v1.1.1/install-internet-guard-qt5.exe)** | 4.7.5 – 4.9.x (Qt 5) | Recommended installer — picks the Veyon folder and copies the plugin, with self-elevation if needed. |
| **[internet-guard-qt5.dll](https://github.com/lellomele/veyon-internet-guard/releases/download/v1.1.1/internet-guard-qt5.dll)** | 4.7.5 – 4.9.x (Qt 5) | DLL only — copy manually into `<Veyon folder>\plugins\`. |

---

## 1. What the plugin does

- Adds a **Block Internet / Allow Internet** toggle button to the Veyon Master
  console.
- Sends a message (`FeatureMessage`) to the connected computers.
- On the student computer, the Veyon Server applies or removes the firewall
  rules.

### Blocked ports and protocols

| Firewall rule            | Port / Protocol               | Purpose                     |
|--------------------------|-------------------------------|-----------------------------|
| `VeyonIG_BlockHTTP`      | TCP 80                        | HTTP                        |
| `VeyonIG_BlockHTTPS`     | TCP 443                       | HTTPS                       |
| `VeyonIG_BlockDNS_UDP`   | UDP 53                        | DNS                         |
| `VeyonIG_BlockDNS_TCP`   | TCP 53                        | DNS                         |
| `VeyonIG_BlockQUIC`      | UDP 443                       | QUIC / HTTP-3               |
| `VeyonIG_BlockDoT_TCP`   | TCP 853                       | DNS over TLS                |
| `VeyonIG_BlockDoT_UDP`   | UDP 853                       | DNS over TLS                |
| `VeyonIG_BlockProxy`     | TCP 8080, 8443, 3128          | Proxy / alternate HTTP      |
| `VeyonIG_AllowLAN`       | any → `localsubnet`           | Keeps the LAN reachable     |

The `VeyonIG_AllowLAN` rule allows all traffic toward the local subnet: Windows
Firewall prefers the more-specific rule (`remoteip`), so Veyon and local network
resources keep working even with Internet blocked. `blockInternet()` always
removes existing rules before recreating them, to avoid duplicates.

---

## 2. Supported Veyon versions

The plugin interfaces used by InternetGuard
(`PluginInterface`, `FeatureProviderInterface`, `Feature`, `FeatureMessage`)
are **identical** across every Veyon release from **4.7.5** through **4.10.x**.
The plugin source is therefore compatible, with no `#if` branches, with:

| Veyon            | Qt        | Build configuration     | Output DLL                  | Status                      |
|------------------|-----------|-------------------------|-----------------------------|-----------------------------|
| 4.7.5 – 4.9.x    | Qt 5      | default (`WITH_QT6=OFF`)| `internet-guard-qt5.dll`    | ✅ built and verified        |
| 4.10.x           | Qt 6      | `-DWITH_QT6=ON`         | `internet-guard-qt6.dll`    | ✅ built and verified        |

The only real difference between versions is the **Qt branch** (Qt 5 up to Veyon
4.9.x, Qt 6 from Veyon 4.10.x), handled entirely by CMake. All compatibility
notes are centralized in [`VeyonCompat.h`](VeyonCompat.h).

> **Important (ABI).** The plugin must be compiled with the **same toolchain**
> (MinGW compiler + Qt version) used to build the Veyon it will be loaded into.
> For the official Veyon packages on Windows that means MinGW and the same Qt
> major as the installed version.

---

## 3. Software requirements

**To use the plugin (installation)**

- Windows with Veyon installed (Master and Server).
- No additional requirement: the installer is self-contained.

**To build the plugin**

| Dependency                  | Qt 5 build                                          | Qt 6 build                                          |
|-----------------------------|-----------------------------------------------------|-----------------------------------------------------|
| CMake                       | ≥ 3.16                                              | ≥ 3.16                                              |
| Qt (Core, Widgets, Network) | Qt 5.12 at `C:/Qt/5.12.12/mingw73_64`              | Qt **6.10.x** MinGW (must be ≤ the target Veyon's Qt — see §5.2) |
| MinGW toolchain             | `C:/Qt/Tools/mingw730_64` (g++ 7.3, Qt 5.12 ABI)   | MinGW **13.1.0** bundled with Qt 6.10               |
| Veyon sources               | `../veyon-src/core/src` (4.7.5 headers)            | `../veyon-src/core/src` (4.7.5 headers — see §5.2)  |
| Veyon import library        | `libveyon-core.dll.a` (in repo root)                | `libveyon-core-qt6.dll.a` (in repo root)            |
| C++ standard                | C++14                                               | C++14                                               |

> Qt Svg is **not** required: the toolbar icon is a PNG, so `QIcon` needs no SVG
> icon-engine plugin (Veyon's Windows build does not ship one anyway).

---

## 4. Configuration

Behavior is configured through the CMake variables listed in §6. In particular,
to target a different Veyon version just set `VEYON_SOURCE_DIR`,
`VEYON_CORE_LIBRARY` and possibly `WITH_QT6`, without touching the source code.

The plugin identity (UUID `a4b3c2d1-e5f6-7890-abcd-ef1234567890`) appears in
three places that must stay consistent: `Q_PLUGIN_METADATA`, `uid()` and the
`Feature::Uid` passed to the `Feature` constructor.

---

## 5. Build instructions

### 5.1 Veyon 4.7.5 – 4.9.x (Qt 5)

From PowerShell, in the repository root:

```powershell
$env:PATH = "C:\Qt\Tools\mingw730_64\bin;$env:PATH"

cmake -S . -B build-qt5 -G "MinGW Makefiles" `
  -DCMAKE_CXX_COMPILER="C:/Qt/Tools/mingw730_64/bin/g++.exe" `
  -DCMAKE_MAKE_PROGRAM="C:/Qt/Tools/mingw730_64/bin/mingw32-make.exe" `
  -DVEYON_TARGET_VERSION=4.7.5 `
  -DVEYON_SOURCE_DIR="C:/path/to/veyon-4.9-src"

cmake --build build-qt5
```

Output: `build-qt5/internet-guard-qt5.dll`.

> The g++ 7.3 shipped with Qt 5.12.12 is used explicitly: it is the only one
> ABI-compatible with the Qt 5.12 libraries and the official `veyon-core.dll`.
> Newer MinGW compilers (e.g. MSYS2 ones) produce a DLL that may fail to load
> correctly in Veyon.

### 5.2 Veyon 4.10.x (Qt 6) — verified against Veyon 4.10.3 / Qt 6.10.3

> ⚠️ **Build with a Qt 6 *minor* version ≤ the one your Veyon ships.** Qt's plugin
> loader silently rejects a plugin built with a *newer* Qt than the host (rule:
> plugin minor ≤ host minor) — the result is exactly "no toolbar button", with no
> error. Veyon 4.10.3 ships **Qt 6.10.3**, so build with Qt ≤ 6.10. To read your
> Veyon's Qt version, check the `ProductVersion` of `Qt6Core.dll` in the Veyon
> install folder.

Get a matching Qt 6.10 MinGW toolchain non-interactively with
[`aqtinstall`](https://github.com/miurahr/aqtinstall) (no Qt account needed):

```powershell
py -m pip install aqtinstall
py -m aqt install-qt   windows desktop 6.10.3 win64_mingw --outputdir C:\Qt-aqt
py -m aqt install-tool windows desktop tools_mingw1310     --outputdir C:\Qt-aqt
```

Then configure and build:

```powershell
$mingw = "C:\Qt-aqt\Tools\mingw1310_64\bin"
$qt6   = "C:\Qt-aqt\6.10.3\mingw_64"
$env:PATH = "$mingw;$qt6\bin;$env:PATH"

cmake -S . -B build-qt6 -G "MinGW Makefiles" `
  -DWITH_QT6=ON `
  -DCMAKE_CXX_COMPILER="$mingw/g++.exe" `
  -DCMAKE_MAKE_PROGRAM="$mingw/mingw32-make.exe" `
  -DCMAKE_PREFIX_PATH="$qt6" `
  -DVEYON_TARGET_VERSION=4.7.5

cmake --build build-qt6
```

Output: `build-qt6/internet-guard-qt6.dll`. Verify its Qt tag is ≤ your Veyon's:
`strings build-qt6\internet-guard-qt6.dll | Select-String qt_version_tag`.

`VEYON_TARGET_VERSION=4.7.5` is intentional even for Veyon 4.10: the repo ships
the 4.7.5 `core/src` headers, and `FeatureMessage`'s memory layout and wire format
are identical through 4.10.3, so the plugin inter-operates correctly. The repo
already contains `libveyon-core-qt6.dll.a` and `libveyon-core-qt6.def`, selected
automatically when `WITH_QT6=ON`.

### 5.3 Building the installer

After building the plugin, run from the repository root:

```powershell
# Qt 5 installer (embeds internet-guard-qt5.dll)
pwsh -File installer\build-installer.ps1 -PluginDll build-qt5\internet-guard-qt5.dll

# Qt 6 installer (embeds internet-guard-qt6.dll)
pwsh -File installer\build-installer.ps1 -PluginDll build-qt6\internet-guard-qt6.dll
```

Produces `installer\install-internet-guard-qt5.exe` (or `…-qt6.exe`) — the name
follows the embedded DLL. Each is a self-contained executable with no Qt dependency.

---

## 6. Available CMake flags

| Variable              | Default                              | Description                                            |
|-----------------------|--------------------------------------|--------------------------------------------------------|
| `WITH_QT6`            | `OFF`                                | Build with Qt 6 instead of Qt 5.                       |
| `VEYON_TARGET_VERSION`| `4.7.5`                              | Target Veyon version (compatibility macros).           |
| `VEYON_SOURCE_DIR`    | `../veyon-src`                       | Veyon sources providing the `core/src` headers.        |
| `VEYON_CORE_LIBRARY`  | auto (`libveyon-core.dll.a` or `-qt6`)| `veyon-core` import library; auto-selected by `WITH_QT6`. |

`VEYON_TARGET_VERSION` is split by CMake and passed to the code as
`VEYON_TARGET_VERSION_MAJOR/MINOR/PATCH`; `VeyonCompat.h` derives the
`VEYON_VERSION_AT_LEAST(maj,min,patch)` macro from it, ready for any future
adaptations.

---

## 7. Running the tests

The plugin has no automated unit-test suite (it is a thin adapter between the
Veyon API and `netsh`). Verification is functional:

1. **Build check (Qt 5)** — inspect the produced DLL:

   ```powershell
   & "C:\Qt\Tools\mingw730_64\bin\objdump.exe" -p build-qt5\internet-guard-qt5.dll | Select-String "qt_plugin|DLL Name"
   ```

   Expected: exports `qt_plugin_instance` and `qt_plugin_query_metadata`,
   dependencies `Qt5Core.dll`, `Qt5Gui.dll`, `veyon-core.dll`.

2. **Build check (Qt 6)** — inspect the produced DLL:

   ```powershell
   & "C:\Qt-aqt\Tools\mingw1310_64\bin\objdump.exe" -p build-qt6\internet-guard-qt6.dll | Select-String "qt_plugin|DLL Name"
   ```

   Expected: exports `qt_plugin_instance` and `qt_plugin_query_metadata_v2`
   (Qt 6 uses `_v2`), dependencies `Qt6Core.dll`, `Qt6Gui.dll`, `veyon-core.dll`.
   Also check `qt_version_tag` is ≤ your Veyon's Qt (see §5.2).

3. **Installer check** — a "dry run" install into a temporary folder, with no
   GUI:

   ```powershell
   $t = Join-Path $env:TEMP "ig_test"
   .\installer\install-internet-guard-qt5.exe --elevated "$t"
   # expected: $t\plugins\internet-guard-qt5.dll (or -qt6.dll) identical to the compiled DLL
   ```

4. **Runtime check in Veyon**:
   - Install the plugin (§9) on a student PC and on the teacher PC.
   - Restart Veyon Master and Veyon Server.
   - The **Block Internet** button must appear in the Master toolbar.
   - When activated, on the student computer `netsh advfirewall firewall show rule name=all`
     must list the `VeyonIG_*` rules; Internet browsing must stop while Veyon
     keeps working.
   - When deactivated, the `VeyonIG_*` rules must disappear.

---

## 8. Known limitations

- **Windows only.** Blocking uses `netsh advfirewall` (a Windows command), so
  the student computers must run Windows.
- **ABI / Qt-version compatibility.** The DLL must be built with a compiler/Qt
  compatible with the installed Veyon (see §2). Use g++ 7.3 (Qt 5.12 toolchain)
  for the Qt5 build; for the Qt6 build use a Qt **≤ the target Veyon's Qt minor**
  with the matching MinGW (e.g. Qt 6.10 + MinGW 13.1.0) — a newer Qt makes Veyon
  silently ignore the plugin (see §5.2).
- **Permissions.** The Veyon Server runs as a service (system account) and has
  the privileges to modify the firewall; no extra action is required.
- The installer needs write access to the Veyon folder (usually under
  `C:\Program Files`): if missing, it offers to restart as administrator.

---

## 9. Installing the plugin

1. Run the installer for your Veyon (`install-internet-guard-qt6.exe` for 4.10.x,
   `install-internet-guard-qt5.exe` for 4.7.5–4.9.x).
2. Confirm/select the Veyon installation folder (the installer suggests a
   default detected from the registry or from `Program Files`).
3. The installer copies the plugin DLL (`internet-guard-qt5.dll` or
   `internet-guard-qt6.dll`, depending on the build embedded in the installer)
   into Veyon's `plugins\` subfolder. If permissions are insufficient, it offers
   to restart with administrator privileges.
4. Repeat on every student PC (for the actual blocking) and on the teacher PC
   (for the toolbar button). Restart Veyon after installing.

Alternatively, copy the DLL manually into `<Veyon folder>\plugins\`.

---

## 10. Project structure

```
InternetGuard/
├─ InternetGuardPlugin.h          Plugin declaration (PluginInterface + FeatureProviderInterface)
├─ InternetGuardPlugin.cpp        Logic: Master side (sending commands) and Server side (netsh rules)
├─ VeyonCompat.h                  Single point of contact with the Veyon API + version macros
├─ CMakeLists.txt                 Multi-version build (WITH_QT6, VEYON_* flags)
├─ resources.qrc                  Embeds the toolbar icon (PNG)
├─ network-offline.png            Toolbar icon (embedded; rendered by QIcon)
├─ network-offline.svg            Editable source for the PNG (not used at runtime)
├─ libveyon-core.dll.a            veyon-core import library (Qt 5, Veyon 4.7.5–4.9.x)
├─ veyon-core.def                 Symbol list for the Qt 5 veyon-core.dll
├─ libveyon-core-qt6.dll.a        veyon-core import library (Qt 6, Veyon 4.10.x)
├─ libveyon-core-qt6.def          Symbol list for the Qt 6 veyon-core.dll
├─ installer/
│  ├─ installer.cpp               Self-contained Win32 installer (folder picker, copy, self-elevation)
│  ├─ installer.rc                Resources: manifest + embedded plugin DLL
│  ├─ installer.manifest          UAC manifest (asInvoker) + common controls
│  └─ build-installer.ps1         Installer build script
├─ build-qt5/                     Qt 5 build output (internet-guard-qt5.dll)
└─ build-qt6/                     Qt 6 build output (internet-guard-qt6.dll)
```

---

## 11. License

This plugin is distributed under the **GNU General Public License v2.0 or later**
(see [`LICENSE`](LICENSE)). The GPL is required because the plugin includes the
Veyon headers and links against `veyon-core`.

The repository also redistributes `libveyon-core.dll.a`, `veyon-core.def`,
`libveyon-core-qt6.dll.a` and `libveyon-core-qt6.def`, derived from Veyon:

> **Veyon** © Veyon Community — GNU GPL v2.0 or later —
> <https://github.com/veyon/veyon>

---

*Third-party plugin, not part of the official Veyon distribution.*
*Copyright © 2025-26 prof. Ing. Raffaele Mele — GPL-2.0-or-later.*

<br>

---
---

<a name="italiano"></a>

# InternetGuard — plugin Veyon "Blocca / Consenti Internet"

[English](#internetguard--veyon-block--allow-internet-plugin) | **Italiano**

Plugin lato insegnante per [Veyon](https://veyon.io/) che aggiunge un pulsante
alla toolbar della console **Veyon Master**. Quando l'insegnante attiva la
funzione, ogni **Veyon Server** sui computer degli studenti aggiunge alcune
regole del Firewall di Windows (tramite `netsh advfirewall`) che bloccano il
traffico Internet in uscita, lasciando però funzionare la rete locale (LAN).
Disattivando la funzione le regole vengono rimosse.

## ⬇️ Download (versione compilata)

Non serve compilare nulla: scarica i file già pronti dalla release
**[v1.1.0](https://github.com/lellomele/veyon-internet-guard/releases/tag/v1.1.1)**
(solo Windows — dopo l'installazione riavviare Veyon Master e Veyon Server).

| File | Veyon | A cosa serve |
|------|-------|--------------|
| **[install-internet-guard-qt6.exe](https://github.com/lellomele/veyon-internet-guard/releases/download/v1.1.1/install-internet-guard-qt6.exe)** | 4.10.x (Qt 6) | Installer consigliato — seleziona la cartella di Veyon e copia il plugin, con auto-elevazione se necessario. |
| **[internet-guard-qt6.dll](https://github.com/lellomele/veyon-internet-guard/releases/download/v1.1.1/internet-guard-qt6.dll)** | 4.10.x (Qt 6) | Solo la DLL — da copiare manualmente in `<cartella Veyon>\plugins\`. |
| **[install-internet-guard-qt5.exe](https://github.com/lellomele/veyon-internet-guard/releases/download/v1.1.1/install-internet-guard-qt5.exe)** | 4.7.5 – 4.9.x (Qt 5) | Installer consigliato — seleziona la cartella di Veyon e copia il plugin, con auto-elevazione se necessario. |
| **[internet-guard-qt5.dll](https://github.com/lellomele/veyon-internet-guard/releases/download/v1.1.1/internet-guard-qt5.dll)** | 4.7.5 – 4.9.x (Qt 5) | Solo la DLL — da copiare manualmente in `<cartella Veyon>\plugins\`. |

---

## 1. Scopo del plugin

- Aggiunge alla console Veyon Master un pulsante **Blocca Internet / Consenti
  Internet** (interruttore acceso/spento).
- Invia ai computer collegati un messaggio (`FeatureMessage`).
- Sul computer dello studente, il Veyon Server applica o rimuove le regole del
  firewall.

### Porte e protocolli bloccati

| Regola firewall          | Porta / Protocollo            | Scopo                       |
|--------------------------|-------------------------------|-----------------------------|
| `VeyonIG_BlockHTTP`      | TCP 80                        | HTTP                        |
| `VeyonIG_BlockHTTPS`     | TCP 443                       | HTTPS                       |
| `VeyonIG_BlockDNS_UDP`   | UDP 53                        | DNS                         |
| `VeyonIG_BlockDNS_TCP`   | TCP 53                        | DNS                         |
| `VeyonIG_BlockQUIC`      | UDP 443                       | QUIC / HTTP-3               |
| `VeyonIG_BlockDoT_TCP`   | TCP 853                       | DNS over TLS                |
| `VeyonIG_BlockDoT_UDP`   | UDP 853                       | DNS over TLS                |
| `VeyonIG_BlockProxy`     | TCP 8080, 8443, 3128          | Proxy / HTTP alternativi    |
| `VeyonIG_AllowLAN`       | qualsiasi → `localsubnet`     | Mantiene attiva la LAN      |

La regola `VeyonIG_AllowLAN` consente tutto il traffico verso la sottorete
locale: Windows Firewall preferisce la regola più specifica (`remoteip`), quindi
Veyon e le risorse di rete locali continuano a funzionare anche con Internet
bloccato. `blockInternet()` rimuove sempre le regole esistenti prima di
ricrearle, per evitare duplicati.

---

## 2. Versioni di Veyon supportate

Le interfacce dei plugin usate da InternetGuard
(`PluginInterface`, `FeatureProviderInterface`, `Feature`, `FeatureMessage`)
sono **identiche** in tutte le versioni di Veyon dalla **4.7.5** fino alla
**4.10.x**. Il codice sorgente del plugin è quindi compatibile, senza alcun
ramo `#if`, con:

| Veyon            | Qt        | Configurazione di build        | DLL prodotta                | Stato                       |
|------------------|-----------|--------------------------------|-----------------------------|-----------------------------|
| 4.7.5 – 4.9.x    | Qt 5      | predefinita (`WITH_QT6=OFF`)   | `internet-guard-qt5.dll`    | ✅ compilato e verificato    |
| 4.10.x           | Qt 6      | `-DWITH_QT6=ON`                | `internet-guard-qt6.dll`    | ✅ compilato e verificato    |

L'unica vera differenza tra le versioni è il **ramo di Qt** (Qt 5 fino a Veyon
4.9.x, Qt 6 da Veyon 4.10.x), gestito interamente da CMake. Tutte le note di
compatibilità sono centralizzate in [`VeyonCompat.h`](VeyonCompat.h).

> **Importante (ABI).** Il plugin va compilato con lo **stesso toolchain**
> (compilatore MinGW + versione di Qt) usato per la build di Veyon su cui verrà
> caricato. Per i pacchetti ufficiali Veyon su Windows ciò significa MinGW e la
> stessa major di Qt della versione installata.

---

## 3. Requisiti software

**Per usare il plugin (installazione)**

- Windows con Veyon installato (Master e Server).
- Nessun requisito aggiuntivo: l'installer è autonomo.

**Per compilare il plugin**

| Dipendenza                  | Build Qt 5                                           | Build Qt 6                                           |
|-----------------------------|------------------------------------------------------|------------------------------------------------------|
| CMake                       | ≥ 3.16                                               | ≥ 3.16                                               |
| Qt (Core, Widgets, Network) | Qt 5.12 in `C:/Qt/5.12.12/mingw73_64`               | Qt **6.10.x** MinGW (deve essere ≤ al Qt del Veyon di destinazione — vedi §5.2) |
| Toolchain MinGW             | `C:/Qt/Tools/mingw730_64` (g++ 7.3, ABI di Qt 5.12) | MinGW **13.1.0** fornito con Qt 6.10                 |
| Sorgenti Veyon              | `../veyon-src/core/src` (header 4.7.5)              | `../veyon-src/core/src` (header 4.7.5 — vedi §5.2)   |
| Import library Veyon        | `libveyon-core.dll.a` (nella root del repo)          | `libveyon-core-qt6.dll.a` (nella root del repo)      |
| Standard C++                | C++14                                                | C++14                                                |

> Qt Svg **non** è richiesto: l'icona della toolbar è un PNG, quindi `QIcon` non
> ha bisogno dell'icon-engine SVG (che comunque la build Windows di Veyon non include).

---

## 4. Configurazione

Il comportamento si configura tramite le variabili CMake elencate in § 6. In
particolare, per puntare a una diversa versione di Veyon basta impostare
`VEYON_SOURCE_DIR`, `VEYON_CORE_LIBRARY` ed eventualmente `WITH_QT6`, senza
toccare il codice sorgente.

L'identità del plugin (UUID `a4b3c2d1-e5f6-7890-abcd-ef1234567890`) compare in
tre punti che devono restare coerenti: `Q_PLUGIN_METADATA`, `uid()` e il
`Feature::Uid` passato al costruttore di `Feature`.

---

## 5. Istruzioni di compilazione

### 5.1 Veyon 4.7.5 – 4.9.x (Qt 5)

Da PowerShell, nella root del repository:

```powershell
$env:PATH = "C:\Qt\Tools\mingw730_64\bin;$env:PATH"

cmake -S . -B build-qt5 -G "MinGW Makefiles" `
  -DCMAKE_CXX_COMPILER="C:/Qt/Tools/mingw730_64/bin/g++.exe" `
  -DCMAKE_MAKE_PROGRAM="C:/Qt/Tools/mingw730_64/bin/mingw32-make.exe" `
  -DVEYON_TARGET_VERSION=4.7.5 `
  -DVEYON_SOURCE_DIR="C:/path/to/veyon-4.9-src"

cmake --build build-qt5
```

Output: `build-qt5/internet-guard-qt5.dll`.

> Si usa esplicitamente il g++ 7.3 fornito con Qt 5.12.12: è l'unico ABI-compatibile
> con le librerie Qt 5.12 e con la `veyon-core.dll` ufficiale. Compilatori MinGW
> più recenti (es. quelli di MSYS2) producono una DLL che potrebbe non caricarsi
> correttamente in Veyon.

### 5.2 Veyon 4.10.x (Qt 6) — verificato su Veyon 4.10.3 / Qt 6.10.3

> ⚠️ **Compilare con un Qt 6 di versione *minor* ≤ a quella del tuo Veyon.** Il
> caricatore di plugin di Qt rifiuta silenziosamente un plugin compilato con un Qt
> più recente dell'host (regola: minor del plugin ≤ minor dell'host) — il risultato
> è esattamente «nessun pulsante nella toolbar», senza errori. Veyon 4.10.3 include
> **Qt 6.10.3**, quindi compilare con Qt ≤ 6.10. Per leggere la versione di Qt del
> tuo Veyon, controlla il `ProductVersion` di `Qt6Core.dll` nella cartella di Veyon.

Procurarsi un toolchain Qt 6.10 MinGW in modo non interattivo con
[`aqtinstall`](https://github.com/miurahr/aqtinstall) (senza account Qt):

```powershell
py -m pip install aqtinstall
py -m aqt install-qt   windows desktop 6.10.3 win64_mingw --outputdir C:\Qt-aqt
py -m aqt install-tool windows desktop tools_mingw1310     --outputdir C:\Qt-aqt
```

Poi configurare e compilare:

```powershell
$mingw = "C:\Qt-aqt\Tools\mingw1310_64\bin"
$qt6   = "C:\Qt-aqt\6.10.3\mingw_64"
$env:PATH = "$mingw;$qt6\bin;$env:PATH"

cmake -S . -B build-qt6 -G "MinGW Makefiles" `
  -DWITH_QT6=ON `
  -DCMAKE_CXX_COMPILER="$mingw/g++.exe" `
  -DCMAKE_MAKE_PROGRAM="$mingw/mingw32-make.exe" `
  -DCMAKE_PREFIX_PATH="$qt6" `
  -DVEYON_TARGET_VERSION=4.7.5

cmake --build build-qt6
```

Output: `build-qt6/internet-guard-qt6.dll`. Verifica che il tag Qt sia ≤ a quello
del tuo Veyon: `strings build-qt6\internet-guard-qt6.dll | Select-String qt_version_tag`.

`VEYON_TARGET_VERSION=4.7.5` è voluto anche per Veyon 4.10: il repo contiene gli
header 4.7.5 di `core/src`, e il layout in memoria e il formato su filo di
`FeatureMessage` sono identici fino alla 4.10.3, quindi il plugin interopera
correttamente. Il repo contiene già `libveyon-core-qt6.dll.a` e
`libveyon-core-qt6.def`, selezionate automaticamente quando `WITH_QT6=ON`.

### 5.3 Creazione dell'installer

Dopo aver compilato il plugin:

```powershell
# Installer Qt 5 (incorpora internet-guard-qt5.dll)
pwsh -File installer\build-installer.ps1 -PluginDll build-qt5\internet-guard-qt5.dll

# Installer Qt 6 (incorpora internet-guard-qt6.dll)
pwsh -File installer\build-installer.ps1 -PluginDll build-qt6\internet-guard-qt6.dll
```

Produce `installer\install-internet-guard-qt5.exe` (o `…-qt6.exe`) — il nome segue
la DLL incorporata. Ciascuno è un eseguibile autonomo senza dipendenze da Qt.

---

## 6. Flag CMake disponibili

| Variabile             | Default                                | Descrizione                                              |
|-----------------------|----------------------------------------|----------------------------------------------------------|
| `WITH_QT6`            | `OFF`                                  | Compila con Qt 6 invece di Qt 5.                         |
| `VEYON_TARGET_VERSION`| `4.7.5`                                | Versione Veyon di destinazione (macro di compatibilità). |
| `VEYON_SOURCE_DIR`    | `../veyon-src`                         | Sorgenti Veyon che forniscono gli header `core/src`.     |
| `VEYON_CORE_LIBRARY`  | auto (`libveyon-core.dll.a` o `-qt6`)  | Import library di `veyon-core`; selezionata automaticamente da `WITH_QT6`. |

`VEYON_TARGET_VERSION` viene scomposta da CMake e passata al codice come
`VEYON_TARGET_VERSION_MAJOR/MINOR/PATCH`; `VeyonCompat.h` ne deriva la macro
`VEYON_VERSION_AT_LEAST(maj,min,patch)`, pronta per eventuali adattamenti futuri.

---

## 7. Esecuzione dei test

Il plugin non ha una suite di unit test automatici (è un sottile adattatore tra
l'API di Veyon e `netsh`). La verifica è funzionale:

1. **Verifica di compilazione (Qt 5)** — controllo della DLL prodotta:

   ```powershell
   & "C:\Qt\Tools\mingw730_64\bin\objdump.exe" -p build-qt5\internet-guard-qt5.dll | Select-String "qt_plugin|DLL Name"
   ```

   Devono comparire gli export `qt_plugin_instance` e `qt_plugin_query_metadata`
   e le dipendenze `Qt5Core.dll`, `Qt5Gui.dll`, `veyon-core.dll`.

2. **Verifica di compilazione (Qt 6)** — controllo della DLL prodotta:

   ```powershell
   & "C:\Qt-aqt\Tools\mingw1310_64\bin\objdump.exe" -p build-qt6\internet-guard-qt6.dll | Select-String "qt_plugin|DLL Name"
   ```

   Devono comparire gli export `qt_plugin_instance` e `qt_plugin_query_metadata_v2`
   (Qt 6 usa `_v2`) e le dipendenze `Qt6Core.dll`, `Qt6Gui.dll`, `veyon-core.dll`.
   Verificare inoltre che `qt_version_tag` sia ≤ al Qt del proprio Veyon (vedi §5.2).

3. **Verifica dell'installer** — installazione "a secco" in una cartella
   temporanea, senza GUI:

   ```powershell
   $t = Join-Path $env:TEMP "ig_test"
   .\installer\install-internet-guard-qt5.exe --elevated "$t"
   # atteso: $t\plugins\internet-guard-qt5.dll (o -qt6.dll) identico alla DLL compilata
   ```

4. **Verifica a runtime in Veyon**:
   - Installare il plugin (§ 9) su un PC studente e sul PC insegnante.
   - Riavviare Veyon Master e Veyon Server.
   - Nella toolbar di Master deve comparire il pulsante **Blocca Internet**.
   - Attivandolo, sul computer dello studente `netsh advfirewall firewall show rule name=all`
     deve elencare le regole `VeyonIG_*`; la navigazione Internet deve
     interrompersi mentre Veyon continua a funzionare.
   - Disattivandolo, le regole `VeyonIG_*` devono sparire.

---

## 8. Limitazioni note

- **Funziona solo su Windows.** Il blocco usa `netsh advfirewall` (comando di
  Windows), quindi i computer degli studenti devono avere Windows.
- **Compatibilità ABI / versione di Qt.** La DLL va compilata con un compilatore/Qt
  compatibile con il Veyon installato (vedi § 2). Usare g++ 7.3 (toolchain Qt 5.12)
  per la build Qt 5; per la build Qt 6 usare un Qt **≤ al minor del Qt del Veyon di
  destinazione** con il MinGW corrispondente (es. Qt 6.10 + MinGW 13.1.0) — un Qt
  più recente fa ignorare silenziosamente il plugin (vedi §5.2).
- **Permessi.** Il Veyon Server gira come servizio (account di sistema) e ha i
  privilegi per modificare il firewall; nessuna azione aggiuntiva è richiesta.
- L'installer richiede i diritti di scrittura nella cartella di Veyon (di norma
  in `C:\Program Files`): se mancano, propone il riavvio come amministratore.

---

## 9. Installazione del plugin

1. Eseguire l'installer adatto al proprio Veyon (`install-internet-guard-qt6.exe`
   per 4.10.x, `install-internet-guard-qt5.exe` per 4.7.5–4.9.x).
2. Confermare/selezionare la cartella di installazione di Veyon (l'installer ne
   propone una predefinita rilevandola dal registro o da `Program Files`).
3. L'installer copia la DLL del plugin (`internet-guard-qt5.dll` o
   `internet-guard-qt6.dll`, a seconda della build incorporata nell'installer)
   nella sottocartella `plugins\` di Veyon. In caso di permessi insufficienti,
   propone il riavvio con privilegi di amministratore.
4. Ripetere su ogni PC studente (per il blocco effettivo) e sul PC insegnante
   (per il pulsante nella console). Riavviare Veyon dopo l'installazione.

In alternativa, copiare manualmente la DLL in `<cartella di Veyon>\plugins\`.

---

## 10. Struttura del progetto

```
InternetGuard/
├─ InternetGuardPlugin.h          Dichiarazione del plugin (PluginInterface + FeatureProviderInterface)
├─ InternetGuardPlugin.cpp        Logica: lato Master (invio comandi) e lato Server (regole netsh)
├─ VeyonCompat.h                  Punto unico di contatto con l'API Veyon + macro di versione
├─ CMakeLists.txt                 Build multi-versione (flag WITH_QT6, VEYON_*)
├─ resources.qrc                  Incorpora l'icona della toolbar (PNG)
├─ network-offline.png            Icona della toolbar (incorporata; resa da QIcon)
├─ network-offline.svg            Sorgente modificabile del PNG (non usato a runtime)
├─ libveyon-core.dll.a            Import library di veyon-core (Qt 5, Veyon 4.7.5–4.9.x)
├─ veyon-core.def                 Elenco simboli esportati da veyon-core.dll (Qt 5)
├─ libveyon-core-qt6.dll.a        Import library di veyon-core (Qt 6, Veyon 4.10.x)
├─ libveyon-core-qt6.def          Elenco simboli esportati da veyon-core.dll (Qt 6)
├─ installer/
│  ├─ installer.cpp               Installer Win32 autonomo (selezione cartella, copia, auto-elevazione)
│  ├─ installer.rc                Risorse: manifest + DLL del plugin incorporata
│  ├─ installer.manifest          Manifest UAC (asInvoker) + common controls
│  └─ build-installer.ps1         Script di build dell'installer
├─ build-qt5/                     Output della build Qt 5 (internet-guard-qt5.dll)
└─ build-qt6/                     Output della build Qt 6 (internet-guard-qt6.dll)
```

---

## 11. Licenza

Questo plugin è distribuito sotto **GNU General Public License v2.0 or later**
(vedi [`LICENSE`](LICENSE)). La GPL è richiesta perché il plugin include gli
header di Veyon e si collega a `veyon-core`.

Il repository ridistribuisce anche `libveyon-core.dll.a`, `veyon-core.def`,
`libveyon-core-qt6.dll.a` e `libveyon-core-qt6.def`, derivati da Veyon:

> **Veyon** © Veyon Community — GNU GPL v2.0 or later —
> <https://github.com/veyon/veyon>

---

*Plugin di terze parti, non parte della distribuzione ufficiale di Veyon.*
*Copyright © 2025-26 prof. Ing. Raffaele Mele — GPL-2.0-or-later.*
