# InternetGuard — Veyon "Block / Allow Internet" plugin

**English** | [Italiano](#italiano)

Teacher-side plugin for [Veyon](https://veyon.io/) that adds a button to the
**Veyon Master** console toolbar. When the teacher activates the feature, every
**Veyon Server** on the student computers adds a few Windows Firewall rules
(via `netsh advfirewall`) that block outbound Internet traffic while keeping the
local network (LAN) working. Deactivating the feature removes the rules.

## ⬇️ Download (pre-built)

No need to compile anything: grab the ready-made files from
**[v1.1.5](https://github.com/lellomele/veyon-internet-guard/releases/tag/v1.1.5)**
(Windows only — after installing, restart Veyon Master and Veyon Server).

> The **installer auto-detects** whether your Veyon uses Qt 5 or Qt 6 and copies
> the matching plugin — so the wrong library can never be installed. It also
> reads the Veyon version and warns (without blocking) if it is below the
> supported floor (4.7.5). Veyon ships **Qt 6 since version 4.9.0** and Qt 5 up
> to 4.8.x.

| File | Veyon | What it is |
|------|-------|------------|
| **[install-internet-guard.exe](https://github.com/lellomele/veyon-internet-guard/releases/download/v1.1.5/install-internet-guard.exe)** | 4.7.5 – 4.10.x (Qt 5 & Qt 6) | **Recommended.** Single installer: detects Qt 5/6 automatically, picks the Veyon folder, copies the right plugin, self-elevates if needed. |
| **[internet-guard-qt6.dll](https://github.com/lellomele/veyon-internet-guard/releases/download/v1.1.5/internet-guard-qt6.dll)** | 4.9.0 – 4.10.x (Qt 6) | DLL only — copy manually into `<Veyon folder>\plugins\`. |
| **[internet-guard-qt5.dll](https://github.com/lellomele/veyon-internet-guard/releases/download/v1.1.5/internet-guard-qt5.dll)** | 4.7.5 – 4.8.x (Qt 5) | DLL only — copy manually into `<Veyon folder>\plugins\`. |

---

## 1. What the plugin does

- Adds a **Block Internet / Allow Internet** toggle button to the Veyon Master
  console toolbar.
- For finer control, **right-click one or more selected computers** and choose
  **Block Internet** / **Allow Internet** from the context menu — the command
  is sent only to the selected computers.
- Sends a message (`FeatureMessage`) to the targeted computers.
- On each student computer the Veyon Server applies or removes the firewall
  rules. The Windows Firewall is automatically switched on first, since block
  rules have no effect while it is disabled.

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

| Veyon            | Qt        | Build configuration     | Output DLL                  | Status                          |
|------------------|-----------|-------------------------|-----------------------------|---------------------------------|
| 4.7.5 – 4.8.x    | Qt 5      | default (`WITH_QT6=OFF`)| `internet-guard-qt5.dll`    | ✅ built and verified            |
| 4.9.0 – 4.10.x   | Qt 6.7    | `-DWITH_QT6=ON`         | `internet-guard-qt6.dll`    | ✅ verified on 4.9.0 and 4.10.4 |

At *source* level the plugin interfaces (`PluginInterface`,
`FeatureProviderInterface`, `Feature`, `FeatureMessage`) are unchanged from
4.7.5 to 4.10.x, so a single source builds every variant with no `#if` branches.

> **Important (binary ABI).** What the plugin is *compiled against* matters more
> than the source. The binary ABI is tied to the Veyon **core** version, so:
> - the **Qt 5** build targets the 4.7.5 core → use it on Veyon 4.7.5–4.8.x;
> - the **Qt 6** build targets the 4.9.0 core and is built with **Qt 6.7** → one
>   file works on Veyon 4.9.0–4.10.x (the interfaces this plugin uses are
>   binary-compatible across that range, and Qt 6.7 — the lowest Qt in it, from
>   4.9.0 — loads on the Qt 6.10 of 4.10.x).
>
> Getting this wrong makes Veyon either ignore the plugin or **crash on load**.
> Use the official Windows (MinGW) Veyon packages. Full analysis in
> [`VeyonCompat.h`](VeyonCompat.h) and §5.2.

---

## 3. Software requirements

**To use the plugin (installation)**

- Windows with Veyon installed (Master and Server).
- No additional requirement: the installer is self-contained.

**To build the plugin**

| Dependency                  | Qt 5 build                                          | Qt 6 build                                          |
|-----------------------------|-----------------------------------------------------|-----------------------------------------------------|
| CMake                       | ≥ 3.16                                              | ≥ 3.16                                              |
| Qt (Core, Widgets, Network) | Qt 5.12 at `C:/Qt/5.12.12/mingw73_64`              | Qt **6.7.x** MinGW (build with 6.7 to cover 4.9.0–4.10.x — see §5.2) |
| MinGW toolchain             | `C:/Qt/Tools/mingw730_64` (g++ 7.3, Qt 5.12 ABI)   | MinGW **13.1.0** (or any recent MinGW — the plugin needs only base libstdc++ symbols; see §5.2) |
| Veyon sources               | a **4.7.5** checkout's `core/src` headers          | a **4.9.0** checkout's `core/src` headers (see §5.2) |
| Veyon import library        | `libveyon-core.dll.a` (in repo root)                | `libveyon-core-qt6.dll.a` (from 4.9.0, in repo root) |
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

### 5.1 Veyon 4.7.5 – 4.8.x (Qt 5)

From PowerShell, in the repository root:

```powershell
$env:PATH = "C:\Qt\Tools\mingw730_64\bin;$env:PATH"

cmake -S . -B build-qt5 -G "MinGW Makefiles" `
  -DCMAKE_CXX_COMPILER="C:/Qt/Tools/mingw730_64/bin/g++.exe" `
  -DCMAKE_MAKE_PROGRAM="C:/Qt/Tools/mingw730_64/bin/mingw32-make.exe" `
  -DVEYON_TARGET_VERSION=4.7.5 `
  -DVEYON_SOURCE_DIR="C:/path/to/veyon-4.8-src"

cmake --build build-qt5
```

Output: `build-qt5/internet-guard-qt5.dll`.

> The g++ 7.3 shipped with Qt 5.12.12 is used explicitly: it is the only one
> ABI-compatible with the Qt 5.12 libraries and the official `veyon-core.dll`.
> Newer MinGW compilers (e.g. MSYS2 ones) produce a DLL that may fail to load
> correctly in Veyon.

### 5.2 Veyon 4.9.x – 4.10.x (Qt 6) — one binary, verified on Veyon 4.9.0 and 4.10.4

A single Qt 6 DLL covers the whole 4.9.0–4.10.x range. Two rules make it work:

> ⚠️ **Build with Qt 6.7 and against the 4.9.0 core.**
> - *Version-gate:* Qt's plugin loader silently rejects a plugin built with a Qt
>   *minor* newer than the host's (plugin minor ≤ host minor, otherwise "no
>   toolbar button" with no error). The lowest Qt in the range is **6.7.2**
>   (Veyon 4.9.0; later 4.9.x use 6.8, 4.10.x uses 6.10), so building with **Qt
>   6.7** loads on the whole range. (Read your Veyon's Qt from the
>   `ProductVersion` of `Qt6Core.dll` in its folder.)
> - *Core ABI:* the plugin must be compiled against headers and an import library
>   matching the Veyon **core** it loads into, or the Server **crashes on load**.
>   The interfaces this plugin uses are binary-compatible across 4.9.0–4.10.x, so
>   the **4.9.0** core (the floor) is the right single target.

Get a Qt 6.7 MinGW toolchain non-interactively with
[`aqtinstall`](https://github.com/miurahr/aqtinstall) (no Qt account needed):

```powershell
py -m pip install aqtinstall
py -m aqt install-qt   windows desktop 6.7.2 win64_mingw --outputdir C:\Qt-aqt
py -m aqt install-tool windows desktop tools_mingw1310    --outputdir C:\Qt-aqt
```

Check out the Veyon **4.9.0** sources for the headers:

```powershell
git clone --depth 1 --branch v4.9.0 https://github.com/veyon/veyon.git ..\veyon-src-490
```

Then configure and build. `VEYON_TARGET_VERSION` defaults to 4.9.0 when
`WITH_QT6=ON`, and the repo already ships the matching `libveyon-core-qt6.dll.a`:

```powershell
$mingw = "C:\Qt-aqt\Tools\mingw1310_64\bin"
$qt6   = "C:\Qt-aqt\6.7.2\mingw_64"
$env:PATH = "$mingw;$qt6\bin;$env:PATH"

cmake -S . -B build-qt6 -G "MinGW Makefiles" `
  -DWITH_QT6=ON `
  -DCMAKE_CXX_COMPILER="$mingw/g++.exe" `
  -DCMAKE_MAKE_PROGRAM="$mingw/mingw32-make.exe" `
  -DCMAKE_PREFIX_PATH="$qt6" `
  -DVEYON_SOURCE_DIR="..\veyon-src-490"

cmake --build build-qt6
```

Output: `build-qt6/internet-guard-qt6.dll` — its Qt tag is `qt_version_tag_6_8`
(`strings build-qt6\internet-guard-qt6.dll | Select-String qt_version_tag`).

> **Targeting a different Veyon core?** Regenerate the import library from that
> Veyon's installed `veyon-core.dll` and point `VEYON_SOURCE_DIR` at the matching
> source checkout:
> ```powershell
> gendef veyon-core.dll
> dlltool -d veyon-core.def -l libveyon-core-qt6.dll.a -D veyon-core.dll
> ```

### 5.3 Building the installer

After building **both** plugin variants, run from the repository root:

```powershell
pwsh -File installer\build-installer.ps1 `
    -PluginDllQt5 build-qt5\internet-guard-qt5.dll `
    -PluginDllQt6 build-qt6\internet-guard-qt6.dll
```

Produces a single `installer\install-internet-guard.exe` that embeds **both** the
Qt 5 and Qt 6 plugins and picks the right one at install time by detecting the
target Veyon's Qt version. It is a self-contained executable with no Qt dependency.

---

## 6. Available CMake flags

| Variable              | Default                              | Description                                            |
|-----------------------|--------------------------------------|--------------------------------------------------------|
| `WITH_QT6`            | `OFF`                                | Build with Qt 6 instead of Qt 5.                       |
| `VEYON_TARGET_VERSION`| auto: `4.7.5` (Qt5) / `4.9.0` (Qt6)  | Target Veyon version (compatibility macros).           |
| `VEYON_SOURCE_DIR`    | `../veyon-src`                       | Veyon `core/src` headers — a 4.7.5 checkout for Qt5, a 4.9.0 one for Qt6. |
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
   $t = Join-Path $env:TEMP "ig_test"; New-Item -ItemType Directory -Force $t | Out-Null
   New-Item -ItemType File -Force "$t\Qt6Core.dll" | Out-Null   # simulate a Qt 6 Veyon (use Qt5Core.dll for Qt 5)
   .\installer\install-internet-guard.exe --elevated "$t"
   # expected: $t\plugins\internet-guard-qt6.dll (the variant matching the detected Qt)
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
- **ABI / Qt-version compatibility.** The DLL must match the installed Veyon's
  core and Qt (see §2). Use g++ 7.3 (Qt 5.12 toolchain) for the Qt5 build; for the
  Qt6 build use **Qt 6.7 + MinGW 13.1.0** against the 4.9.0 core — that one file
  works on Veyon 4.9.x and 4.10.x. A newer Qt makes Veyon silently ignore the
  plugin, and the wrong core version makes the Server crash on load (see §5.2).
- **Permissions.** The Veyon Server runs as a service (system account) and has
  the privileges to modify the firewall; no extra action is required.
- The installer needs write access to the Veyon folder (usually under
  `C:\Program Files`): if missing, it offers to restart as administrator.

---

## 9. Installing the plugin

1. Run `install-internet-guard.exe` (the single installer works for every
   supported Veyon — it detects Qt 5 vs Qt 6 automatically).
2. Confirm/select the Veyon installation folder (the installer suggests a
   default detected from the registry or from `Program Files`). It then shows the
   detected Veyon version and Qt, and warns (without blocking) if the version is
   unsupported or unreadable.
3. The installer copies the matching plugin DLL (`internet-guard-qt5.dll` or
   `internet-guard-qt6.dll`) into Veyon's `plugins\` subfolder and removes the
   other-Qt variant if a previous install left it there. If permissions are
   insufficient, it offers to restart with administrator privileges.
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
├─ libveyon-core.dll.a            veyon-core import library (Qt 5, Veyon 4.7.5–4.8.x)
├─ veyon-core.def                 Symbol list for the Qt 5 veyon-core.dll
├─ libveyon-core-qt6.dll.a        veyon-core import library (Qt 6, Veyon 4.9.x–4.10.x)
├─ libveyon-core-qt6.def          Symbol list for the Qt 6 veyon-core.dll
├─ installer/
│  ├─ installer.cpp               Self-contained Win32 installer (Qt auto-detect, version check, TaskDialog UI, copy, self-elevation)
│  ├─ installer.rc                Resources: manifest + both embedded plugin DLLs (Qt5 + Qt6)
│  ├─ installer.manifest          UAC manifest (asInvoker) + common controls
│  └─ build-installer.ps1         Installer build script (embeds both Qt variants)
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
**[v1.1.5](https://github.com/lellomele/veyon-internet-guard/releases/tag/v1.1.5)**
(solo Windows — dopo l'installazione riavviare Veyon Master e Veyon Server).

> L'**installer rileva automaticamente** se il tuo Veyon usa Qt 5 o Qt 6 e copia
> il plugin corretto — così non è possibile installare la libreria sbagliata.
> Legge inoltre la versione di Veyon e avvisa (senza bloccare) se è inferiore al
> minimo supportato (4.7.5). Veyon usa **Qt 6 dalla versione 4.9.0** e Qt 5 fino
> alla 4.8.x.

| File | Veyon | A cosa serve |
|------|-------|--------------|
| **[install-internet-guard.exe](https://github.com/lellomele/veyon-internet-guard/releases/download/v1.1.5/install-internet-guard.exe)** | 4.7.5 – 4.10.x (Qt 5 e Qt 6) | **Consigliato.** Installer unico: rileva automaticamente Qt 5/6, seleziona la cartella di Veyon, copia il plugin giusto, con auto-elevazione se necessario. |
| **[internet-guard-qt6.dll](https://github.com/lellomele/veyon-internet-guard/releases/download/v1.1.5/internet-guard-qt6.dll)** | 4.9.0 – 4.10.x (Qt 6) | Solo la DLL — da copiare manualmente in `<cartella Veyon>\plugins\`. |
| **[internet-guard-qt5.dll](https://github.com/lellomele/veyon-internet-guard/releases/download/v1.1.5/internet-guard-qt5.dll)** | 4.7.5 – 4.8.x (Qt 5) | Solo la DLL — da copiare manualmente in `<cartella Veyon>\plugins\`. |

---

## 1. Scopo del plugin

- Aggiunge alla toolbar della console Veyon Master un pulsante **Blocca Internet /
  Consenti Internet** (interruttore acceso/spento).
- Per un controllo più fine, **clic destro su uno o più computer selezionati** e
  scegliere **Block Internet** / **Allow Internet** dal menù contestuale: il
  comando viene inviato solo ai computer selezionati.
- Invia ai computer interessati un messaggio (`FeatureMessage`).
- Su ogni computer studente il Veyon Server applica o rimuove le regole del
  firewall. Il Firewall di Windows viene prima riacceso automaticamente, perché
  con il firewall disattivato le regole di blocco non avrebbero effetto.

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

| Veyon            | Qt        | Configurazione di build        | DLL prodotta                | Stato                           |
|------------------|-----------|--------------------------------|-----------------------------|---------------------------------|
| 4.7.5 – 4.8.x    | Qt 5      | predefinita (`WITH_QT6=OFF`)   | `internet-guard-qt5.dll`    | ✅ compilato e verificato        |
| 4.9.0 – 4.10.x   | Qt 6.7    | `-DWITH_QT6=ON`                | `internet-guard-qt6.dll`    | ✅ verificato su 4.9.0 e 4.10.4 |

A livello *sorgente* le interfacce dei plugin (`PluginInterface`,
`FeatureProviderInterface`, `Feature`, `FeatureMessage`) sono invariate dalla
4.7.5 alla 4.10.x, quindi un unico sorgente costruisce ogni variante senza rami `#if`.

> **Importante (ABI binaria).** Conta più *contro cosa* viene compilato il plugin
> che il sorgente. L'ABI binaria dipende dalla versione del **core** di Veyon, perciò:
> - la build **Qt 5** punta al core 4.7.5 → usala su Veyon 4.7.5–4.8.x;
> - la build **Qt 6** punta al core 4.9.0 ed è costruita con **Qt 6.7** → un unico
>   file funziona su Veyon 4.9.0–4.10.x (le interfacce usate dal plugin sono
>   compatibili a livello binario in tutto l'arco, e Qt 6.7 — il Qt più basso, di
>   4.9.0 — si carica sul Qt 6.10 della 4.10.x).
>
> Sbagliare questo fa sì che Veyon ignori il plugin o **crashi al caricamento**.
> Usare i pacchetti Veyon ufficiali per Windows (MinGW). Analisi completa in
> [`VeyonCompat.h`](VeyonCompat.h) e § 5.2.

---

## 3. Requisiti software

**Per usare il plugin (installazione)**

- Windows con Veyon installato (Master e Server).
- Nessun requisito aggiuntivo: l'installer è autonomo.

**Per compilare il plugin**

| Dipendenza                  | Build Qt 5                                           | Build Qt 6                                           |
|-----------------------------|------------------------------------------------------|------------------------------------------------------|
| CMake                       | ≥ 3.16                                               | ≥ 3.16                                               |
| Qt (Core, Widgets, Network) | Qt 5.12 in `C:/Qt/5.12.12/mingw73_64`               | Qt **6.7.x** MinGW (compilare con 6.7 per coprire 4.9.0–4.10.x — vedi § 5.2) |
| Toolchain MinGW             | `C:/Qt/Tools/mingw730_64` (g++ 7.3, ABI di Qt 5.12) | MinGW **13.1.0** (o qualunque MinGW recente — al plugin servono solo simboli libstdc++ di base; vedi § 5.2) |
| Sorgenti Veyon              | header `core/src` di un checkout **4.7.5**          | header `core/src` di un checkout **4.9.0** (vedi § 5.2) |
| Import library Veyon        | `libveyon-core.dll.a` (nella root del repo)          | `libveyon-core-qt6.dll.a` (da 4.9.0, nella root del repo) |
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

### 5.1 Veyon 4.7.5 – 4.8.x (Qt 5)

Da PowerShell, nella root del repository:

```powershell
$env:PATH = "C:\Qt\Tools\mingw730_64\bin;$env:PATH"

cmake -S . -B build-qt5 -G "MinGW Makefiles" `
  -DCMAKE_CXX_COMPILER="C:/Qt/Tools/mingw730_64/bin/g++.exe" `
  -DCMAKE_MAKE_PROGRAM="C:/Qt/Tools/mingw730_64/bin/mingw32-make.exe" `
  -DVEYON_TARGET_VERSION=4.7.5 `
  -DVEYON_SOURCE_DIR="C:/path/to/veyon-4.8-src"

cmake --build build-qt5
```

Output: `build-qt5/internet-guard-qt5.dll`.

> Si usa esplicitamente il g++ 7.3 fornito con Qt 5.12.12: è l'unico ABI-compatibile
> con le librerie Qt 5.12 e con la `veyon-core.dll` ufficiale. Compilatori MinGW
> più recenti (es. quelli di MSYS2) producono una DLL che potrebbe non caricarsi
> correttamente in Veyon.

### 5.2 Veyon 4.9.x – 4.10.x (Qt 6) — un solo binario, verificato su Veyon 4.9.0 e 4.10.4

Un'unica DLL Qt 6 copre tutto l'arco 4.9.0–4.10.x. Due regole lo rendono possibile:

> ⚠️ **Compilare con Qt 6.7 e contro il core 4.9.0.**
> - *Version-gate:* il caricatore di plugin di Qt rifiuta silenziosamente un plugin
>   compilato con un Qt *minor* più recente dell'host (minor del plugin ≤ minor
>   dell'host, altrimenti «nessun pulsante nella toolbar», senza errori). Il Qt più
>   basso dell'arco è **6.7.2** (Veyon 4.9.0; le 4.9.x successive usano 6.8, la
>   4.10.x usa 6.10), quindi compilando con **Qt 6.7** si carica su tutto l'arco.
>   (Leggi il Qt del tuo Veyon dal `ProductVersion` di `Qt6Core.dll` nella sua cartella.)
> - *ABI del core:* il plugin va compilato contro header e import library
>   corrispondenti al **core** di Veyon su cui si carica, altrimenti il Server
>   **crasha al caricamento**. Le interfacce usate dal plugin sono compatibili a
>   livello binario in tutto 4.9.0–4.10.x, perciò il core **4.9.0** (il pavimento)
>   è il giusto target unico.

Procurarsi un toolchain Qt 6.7 MinGW in modo non interattivo con
[`aqtinstall`](https://github.com/miurahr/aqtinstall) (senza account Qt):

```powershell
py -m pip install aqtinstall
py -m aqt install-qt   windows desktop 6.7.2 win64_mingw --outputdir C:\Qt-aqt
py -m aqt install-tool windows desktop tools_mingw1310    --outputdir C:\Qt-aqt
```

Scaricare i sorgenti Veyon **4.9.0** per gli header:

```powershell
git clone --depth 1 --branch v4.9.0 https://github.com/veyon/veyon.git ..\veyon-src-490
```

Poi configurare e compilare. `VEYON_TARGET_VERSION` vale 4.9.0 di default quando
`WITH_QT6=ON`, e il repo include già la `libveyon-core-qt6.dll.a` corrispondente:

```powershell
$mingw = "C:\Qt-aqt\Tools\mingw1310_64\bin"
$qt6   = "C:\Qt-aqt\6.7.2\mingw_64"
$env:PATH = "$mingw;$qt6\bin;$env:PATH"

cmake -S . -B build-qt6 -G "MinGW Makefiles" `
  -DWITH_QT6=ON `
  -DCMAKE_CXX_COMPILER="$mingw/g++.exe" `
  -DCMAKE_MAKE_PROGRAM="$mingw/mingw32-make.exe" `
  -DCMAKE_PREFIX_PATH="$qt6" `
  -DVEYON_SOURCE_DIR="..\veyon-src-490"

cmake --build build-qt6
```

Output: `build-qt6/internet-guard-qt6.dll` — il tag Qt è `qt_version_tag_6_8`
(`strings build-qt6\internet-guard-qt6.dll | Select-String qt_version_tag`).

> **Puntare a un core Veyon diverso?** Rigenera l'import library dal `veyon-core.dll`
> installato di quel Veyon e imposta `VEYON_SOURCE_DIR` sul checkout sorgente
> corrispondente:
> ```powershell
> gendef veyon-core.dll
> dlltool -d veyon-core.def -l libveyon-core-qt6.dll.a -D veyon-core.dll
> ```

### 5.3 Creazione dell'installer

Dopo aver compilato **entrambe** le varianti del plugin:

```powershell
pwsh -File installer\build-installer.ps1 `
    -PluginDllQt5 build-qt5\internet-guard-qt5.dll `
    -PluginDllQt6 build-qt6\internet-guard-qt6.dll
```

Produce un unico `installer\install-internet-guard.exe` che incorpora **entrambi**
i plugin (Qt 5 e Qt 6) e sceglie quello giusto al momento dell'installazione
rilevando la versione di Qt del Veyon di destinazione. È un eseguibile autonomo
senza dipendenze da Qt.

---

## 6. Flag CMake disponibili

| Variabile             | Default                                | Descrizione                                              |
|-----------------------|----------------------------------------|----------------------------------------------------------|
| `WITH_QT6`            | `OFF`                                  | Compila con Qt 6 invece di Qt 5.                         |
| `VEYON_TARGET_VERSION`| auto: `4.7.5` (Qt5) / `4.9.0` (Qt6)    | Versione Veyon di destinazione (macro di compatibilità). |
| `VEYON_SOURCE_DIR`    | `../veyon-src`                         | Header `core/src` di Veyon — un checkout 4.7.5 per Qt5, uno 4.9.0 per Qt6. |
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
   Verificare inoltre che `qt_version_tag` sia ≤ al Qt del proprio Veyon (vedi § 5.2).

3. **Verifica dell'installer** — installazione "a secco" in una cartella
   temporanea, senza GUI:

   ```powershell
   $t = Join-Path $env:TEMP "ig_test"; New-Item -ItemType Directory -Force $t | Out-Null
   New-Item -ItemType File -Force "$t\Qt6Core.dll" | Out-Null   # simula un Veyon Qt 6 (usa Qt5Core.dll per Qt 5)
   .\installer\install-internet-guard.exe --elevated "$t"
   # atteso: $t\plugins\internet-guard-qt6.dll (la variante corrispondente al Qt rilevato)
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
- **Compatibilità ABI / versione di Qt.** La DLL deve corrispondere al core e al Qt
  del Veyon installato (vedi § 2). Usare g++ 7.3 (toolchain Qt 5.12) per la build
  Qt 5; per la build Qt 6 usare **Qt 6.7 + MinGW 13.1.0** contro il core 4.9.0 —
  questo unico file funziona su Veyon 4.9.x e 4.10.x. Un Qt più recente fa ignorare
  silenziosamente il plugin, e una versione errata del core fa crashare il Server al
  caricamento (vedi § 5.2).
- **Permessi.** Il Veyon Server gira come servizio (account di sistema) e ha i
  privilegi per modificare il firewall; nessuna azione aggiuntiva è richiesta.
- L'installer richiede i diritti di scrittura nella cartella di Veyon (di norma
  in `C:\Program Files`): se mancano, propone il riavvio come amministratore.

---

## 9. Installazione del plugin

1. Eseguire `install-internet-guard.exe` (l'installer unico va bene per ogni
   Veyon supportato — rileva automaticamente Qt 5 o Qt 6).
2. Confermare/selezionare la cartella di installazione di Veyon (l'installer ne
   propone una predefinita rilevandola dal registro o da `Program Files`). Mostra
   poi la versione di Veyon e il Qt rilevati e avvisa (senza bloccare) se la
   versione non è supportata o non è leggibile.
3. L'installer copia la DLL del plugin corrispondente (`internet-guard-qt5.dll` o
   `internet-guard-qt6.dll`) nella sottocartella `plugins\` di Veyon e rimuove
   l'altra variante Qt se lasciata da un'installazione precedente. In caso di
   permessi insufficienti, propone il riavvio con privilegi di amministratore.
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
├─ libveyon-core.dll.a            Import library di veyon-core (Qt 5, Veyon 4.7.5–4.8.x)
├─ veyon-core.def                 Elenco simboli esportati da veyon-core.dll (Qt 5)
├─ libveyon-core-qt6.dll.a        Import library di veyon-core (Qt 6, Veyon 4.9.x–4.10.x)
├─ libveyon-core-qt6.def          Elenco simboli esportati da veyon-core.dll (Qt 6)
├─ installer/
│  ├─ installer.cpp               Installer Win32 autonomo (auto-rilevamento Qt, controllo versione, UI TaskDialog, copia, auto-elevazione)
│  ├─ installer.rc                Risorse: manifest + entrambe le DLL del plugin incorporate (Qt5 + Qt6)
│  ├─ installer.manifest          Manifest UAC (asInvoker) + common controls
│  └─ build-installer.ps1         Script di build dell'installer (incorpora entrambe le varianti Qt)
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
