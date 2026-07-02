# Architecture notes

Technical/architecture summary for contributors. User-facing documentation
(Italian) lives in `README.md`.

## What this is

A [Veyon](https://veyon.io/) teacher-side plugin that adds a "Block / Allow Internet" toggle to the Veyon Master UI. When the teacher activates the feature, the Veyon Server running on each student machine adds Windows Firewall rules (via the native firewall COM API, `INetFwPolicy2`) that block outbound internet traffic while Veyon itself keeps working. Deactivating removes those rules.

## Version compatibility (important)

At *source* level the Veyon plugin interfaces used here — `PluginInterface`, `FeatureProviderInterface` (`controlFeature`, `handleFeatureMessage`, `sendFeatureMessage`), `Feature` (9-arg constructor + `Flag` enum), `FeatureMessage` — compile unchanged across every Veyon release from 4.7.5 through 4.10.x, with **no per-version `#if` branches**. The **binary ABI**, however, is tied to the Veyon *core* version the plugin is compiled against (see the **Binary ABI** lesson below — getting it wrong crashes the Veyon Server on load). Together with the Qt branch this gives two builds:

- Veyon 4.7.5 – 4.8.x → Qt 5, **core 4.7.5** (default, `WITH_QT6=OFF`) → output: `internet-guard-qt5.dll`
- Veyon 4.9.0 – 4.10.x → **Qt 6.7, core 4.9.0** (`WITH_QT6=ON`) → output: `internet-guard-qt6.dll` (one binary covers the whole 4.9–4.10 range)

All version handling is centralized in `VeyonCompat.h` (single include point for the Veyon API + `VEYON_TARGET_VERSION_*` macros and `VEYON_VERSION_AT_LEAST()`).

### API change in Veyon 4.10.0: `FeatureMessage::command()`

In Veyon ≤4.9.x, `FeatureMessage::command()` returned `int`.  
In Veyon ≥4.10.0, it returns `FeatureMessage::Command` (a class enum), with a
template overload `command<YourEnum>()` to decode into a user-defined enum type.

`VeyonCompat.h` provides the `VEYON_DECODE_COMMAND(msg, EnumType)` macro to
abstract this difference; `InternetGuardPlugin.cpp` uses it exclusively so the
same source compiles against both API versions.

## Build

**Prerequisites**

| Dependency | Qt 5 build | Qt 6 build |
|---|---|---|
| CMake | ≥ 3.16 | ≥ 3.16 |
| Qt (Core, Widgets, Network) | Qt 5.12 at `C:/Qt/5.12.12/mingw73_64` | Qt **6.7.x** MinGW (e.g. `C:/Qt-aqt/6.7.2/mingw_64`) |
| MinGW toolchain | `C:/Qt/Tools/mingw730_64` (g++ 7.3) | MinGW **13.1.0** (`tools_mingw1310`) |
| Veyon source tree | a 4.7.5 checkout's `core/src` | a **4.9.0** checkout's `core/src` |
| Veyon import library | `libveyon-core.dll.a` (in repo root) | `libveyon-core-qt6.dll.a` (from 4.9.0, in repo root) |
| C++ standard | C++14 | C++14 |

> Qt Svg is **not** a dependency: the toolbar icon is a PNG (see "Toolbar icon"
> below), so `QIcon` needs no SVG icon-engine plugin.

**ABI note (Qt 5):** build with the **same compiler/Qt that the installed Veyon uses**. The official Windows Veyon 4.7.5–4.8.x builds use MinGW g++ 7.3 + Qt 5.12; using a different MinGW (e.g. MSYS2 GCC) can produce a DLL that fails to load.

**ABI note (Qt 6 / GCC 16):** GCC 16 emits both a local vtable and `__imp__ZTV` references for `dllimport` base classes (`FeatureProviderInterface`, `PluginInterface`). Both definitions are identical (same header), so the linker flag `-Wl,--allow-multiple-definition` is added for WIN32 builds to silently resolve the conflict. This is harmless and expected with this toolchain combination.

**Configure and build (Qt 5 / Veyon 4.7.5–4.8.x)**

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

**Configure and build (Qt 6 / Veyon 4.9.x–4.10.x — one binary, verified on Veyon 4.9.0 and 4.10.4)**

> ⚠️ **Two things must match: the Qt minor *and* the Veyon core version.**
> - *Version gate.* Qt's plugin loader rejects — *silently*, no error, no toolbar
>   button — a plugin built with a Qt **newer** than the host (plugin minor ≤ host
>   minor, same major). The lowest Qt in the range is **6.7.2** (Veyon 4.9.0;
>   later 4.9.x moved to 6.8, 4.10.x to 6.10), so the plugin is built with Qt
>   **6.7** and loads on every host up to 4.10.x. An earlier attempt with MSYS2's
>   rolling Qt 6.11 produced `qt_version_tag_6_11`, refused even by 4.10.x. Check
>   with `strings internet-guard-qt6.dll | grep qt_version_tag`.
> - *Core ABI.* Build against the headers **and** import library of the Veyon
>   **core** the plugin loads into, or the Server **crashes on load**. The
>   interfaces this plugin uses are binary-compatible across 4.9.0–4.10.x, so the
>   **4.9.0** core (the floor) is the single target; the resulting DLL loads on
>   4.9.0, 4.9.8 and 4.10.4. Read the host's Qt from the `ProductVersion` of
>   `Qt6Core.dll` in the Veyon folder.

Get a Qt 6.7 MinGW toolchain non-interactively with
[`aqtinstall`](https://github.com/miurahr/aqtinstall) and check out the 4.9.0 headers:

```powershell
py -m pip install aqtinstall
py -m aqt install-qt   windows desktop 6.7.2 win64_mingw --outputdir C:\Qt-aqt
py -m aqt install-tool windows desktop tools_mingw1310    --outputdir C:\Qt-aqt
git clone --depth 1 --branch v4.9.0 https://github.com/veyon/veyon.git ..\veyon-src-490
```

Then configure and build with **MinGW Makefiles** (not Ninja — keeps the toolchain explicit):

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

Output: `build-qt6/internet-guard-qt6.dll` (`qt_version_tag_6_7`). `VEYON_TARGET_VERSION`
defaults to 4.9.0 when `WITH_QT6=ON`, so `VEYON_DECODE_COMMAND` uses the `<4.10`
`static_cast<Commands>(msg.command())` path — correct, since 4.9.0's `FeatureMessage`
exposes `command()` as `qint32`.

The repo ships `libveyon-core-qt6.dll.a` (generated from Veyon **4.9.0**'s
`veyon-core.dll` via `gendef` + `dlltool`) and `libveyon-core-qt6.def` (its export
list), selected automatically when `WITH_QT6=ON`. To target a different core,
regenerate the import lib from that Veyon's `veyon-core.dll` and point
`VEYON_SOURCE_DIR` at the matching source checkout.

For the CMake flags (`WITH_QT6`, `VEYON_TARGET_VERSION`, `VEYON_SOURCE_DIR`, `VEYON_CORE_LIBRARY`), see `README.md` §5–§6.

## Architecture

The plugin is a single shared library (`internet-guard-qt5.dll` or `internet-guard-qt6.dll`) built from:

- **`InternetGuardPlugin.h/.cpp`** — the entire plugin logic. Inherits from both `FeatureProviderInterface` and `PluginInterface`. Two roles depending on which side of Veyon loads it:
  - **Master side** (`controlFeature`): on `Operation::Start`/`Stop` sends a `BlockInternetCommand` / `AllowInternetCommand` `FeatureMessage` to the targeted computers via `sendFeatureMessage`.
  - **Server side** (`handleFeatureMessage`): receives the message and calls `blockInternet()` / `allowInternet()` — thin `Q_OS_WIN`-guarded wrappers around the **`WindowsFirewall`** backend (no-ops with a warning elsewhere). The destructor calls `allowInternet()` so stale rules are never left on unload.

- **`WindowsFirewall.h/.cpp`** — the firewall backend (compiled on Windows only, see `CMakeLists.txt`). Talks to the Windows Firewall through the **`INetFwPolicy2` / `INetFwRule` COM API** instead of spawning `netsh`: no external process on PATH (the Server runs as SYSTEM — PATH lookup was a hijacking risk), no locale-dependent output, no process timeouts, per-call `HRESULT` logging, and removing absent rules is silent (with `netsh`, every `delete rule` on a missing rule produced a spurious warning). The GUIDs of the firewall COM objects are defined locally so no uuid import library is needed and both MinGW toolchains link identically. `blockInternet()` first removes leftovers (idempotence), then enables all firewall profiles — block rules have no effect while a profile is off, the typical cause of a single client not being blocked — then adds the block rules; `put_Protocol` must be called **before** `put_RemotePorts` or the latter fails. Rule names are not unique, so removal probes with `Item()` and loops (`Remove()` reports success even when nothing matched).

  **Features exposed** (`featureList()`): a `Mode` toggle (`Block/Allow Internet`, toolbar) plus two `Action` sub-features (`Block Internet` / `Allow Internet`) whose `parentUid` is the toggle. The toggle gives a one-click global block/allow; the sub-features appear in the toolbar dropdown and the right-click context menu and act on **exactly the selected computers** — Veyon already passes the selection to `controlFeature`, but a `Mode` toggle tracks a single global state, so explicit per-selection `Action` features are what make independent per-client control reliable. All commands are dispatched by command code in `handleFeatureMessage`, independent of which feature UID carried them.

- **`VeyonCompat.h`** — single point of contact with the Veyon API + version macros + the `VEYON_DECODE_COMMAND` compatibility macro (see Version compatibility above).

- **`resources.qrc`** — embeds `network-offline.png` as the toolbar icon (`:/internet-guard/network-offline.png`). See "Toolbar icon" below for why it is a PNG, not an SVG.

- **`installer/`** — standalone native Win32 installer (`installer.cpp`, `installer.rc`, `installer.manifest`, `build-installer.ps1`). Statically linked, no Qt dependency; embeds **both** plugin DLLs (Qt5 + Qt6) as RCDATA resources. It suggests the Veyon folder (registry/Program Files), lets the user pick it, **detects whether that Veyon uses Qt 5 or Qt 6** (`Qt5Core.dll`/`Qt6Core.dll`) and installs the matching variant, **reads the Veyon version** (from the bundled executables, falling back to `veyon-core.dll`) and warns — without blocking — if it is below 4.7.5 or unreadable, copies the DLL into `…\plugins\` (removing the other-Qt variant), uses modern `TaskDialog` dialogs, reports permission errors, and self-elevates (UAC `runas`) on access-denied. Build with `pwsh -File installer\build-installer.ps1 -PluginDllQt5 build-qt5\internet-guard-qt5.dll -PluginDllQt6 build-qt6\internet-guard-qt6.dll`.

- **`tools/fw_test.cpp`** — standalone manual test of the `WindowsFirewall` backend (`tools/qtshim/QtGlobal` maps `qWarning` to stderr so it builds without Qt; static exe, no runtime dependencies). Run elevated to exercise real rule creation/removal; unelevated it verifies the graceful-failure path. Build command in the file header.

### Firewall rule names

All rules use the prefix `VeyonIG_` so they can be reliably deleted. `blockInternet()` always removes existing rules first to avoid duplicate accumulation. All rules are outbound, all profiles, IPv4+IPv6 (no remote-address filter).

| Rule constant | Port / Protocol | Purpose |
|---|---|---|
| `VeyonIG_BlockHTTP` | TCP 80 | HTTP |
| `VeyonIG_BlockHTTPS` | TCP 443 | HTTPS |
| `VeyonIG_BlockDNS_UDP` | UDP 53 | DNS |
| `VeyonIG_BlockDNS_TCP` | TCP 53 | DNS |
| `VeyonIG_BlockQUIC` | UDP 443 | QUIC / HTTP-3 |
| `VeyonIG_BlockDoT_TCP` | TCP 853 | DNS over TLS |
| `VeyonIG_BlockDoT_UDP` | UDP 853 | DNS over TLS |
| `VeyonIG_BlockProxy` | TCP 8080,8443,3128 | Proxy / alternate HTTP |

**No LAN exemption — by design (don't re-add it).** Plugin ≤ 1.1 shipped a `VeyonIG_AllowLAN` rule (`allow any → localsubnet`) meant to keep LAN web servers reachable. It never worked: Windows Firewall gives **Block rules precedence over Allow rules**, so an allow rule cannot punch through port blocks. Scoping the block rules to public IP ranges instead was considered and rejected: it would leave an internal proxy (LAN address, port 8080/3128) as an internet escape route. The blocked ports therefore apply to the LAN too; Veyon's own ports, file sharing and printing are unaffected. `allowInternet()` still deletes the legacy `VeyonIG_AllowLAN` name to clean up upgrades from ≤ 1.1.

### Plugin identity

The plugin's UUID `a4b3c2d1-e5f6-7890-abcd-ef1234567890` is the identity of the **toggle** feature and must stay consistent across `uid()` and the toggle's `Feature::Uid` (the metadata IID `io.veyon.Veyon.Plugins.InternetGuard` is a separate, stable string). The two `Action` sub-features have their own constant UUIDs; only uniqueness matters for those.

### Qt plugin metadata export

Qt 5 exports `qt_plugin_query_metadata`; Qt 6 exports `qt_plugin_query_metadata_v2`. Both are generated automatically by `moc` / `AUTOMOC` — no manual action needed.

### Toolbar icon (PNG, not SVG)

Veyon renders a feature's icon with `QIcon(feature.iconUrl())`. Two constraints, both learned the hard way:

1. **No SVG at runtime.** Veyon's Windows deployment does not ship Qt's SVG icon-engine plugin (`iconengines/qsvgicon.dll`), so `QIcon(":/…svg")` yields an *empty* icon — the toolbar button appears but blank (same background as its neighbours). Every built-in Veyon plugin uses PNG icons; so does this one (`:/internet-guard/network-offline.png`). This also removes any dependency on Qt Svg.
2. **`network-offline.svg` is kept only as the editable source.** Regenerate the PNG after editing it. Note that Qt's SVG module is "SVG Tiny" and does **not** parse `rgba(r,g,b,a)` fill notation — it renders such a shape as opaque black (this is what originally hid the globe). Use `fill="#rrggbb" fill-opacity="0.x"` instead. Rasterize with any Qt build that has the Svg module (a ~20-line `QSvgRenderer` → `QPainter` on a transparent `QImage` → `img.save()` program at 128×128 is enough).

## Lessons learned (read before changing build/icon/version code)

- **Binary ABI ≠ source compatibility — this one crashes the Server.** The plugin must be compiled against the Veyon **core** version it loads into (interface headers + `veyon-core` import library). The source compiles fine against any 4.7.x–4.10.x headers, but a Qt6 DLL built against **4.7.5** headers **crashed Veyon 4.10.x at load** (vtable / class-layout mismatch) even though its Qt matched (both 6.10.3). Fix: build the Qt6 plugin against the **4.9.0** core (the floor of the Qt6 range). The interfaces this plugin uses are binary-compatible across 4.9.0–4.10.x — verified by `diff`-ing `core/src` (PluginInterface/FeatureProviderInterface headers identical; the `FeatureMessage` data layout — `m_command`, a 4-byte field — is unchanged; the 3 imported core symbols exist in every release) and by loading the very same DLL on 4.9.0, 4.9.8 and 4.10.4.
- **Qt 6 plugin version gate.** Qt rejects — silently, no button — a plugin built with a Qt **newer** than the host's minor. The lowest Qt across 4.9.0–4.10.x is **6.7.2** (Veyon 4.9.0; later 4.9.x use 6.8, 4.10.x uses 6.10), so build the Qt6 plugin with **Qt 6.7** to cover the whole range. Verify with `strings … | grep qt_version_tag` (must read `…_6_7`).
- **Icon must be PNG**, not SVG, and the source SVG must avoid `rgba()`. See "Toolbar icon".
- **`VEYON_TARGET_VERSION` tracks the core:** 4.7.5 for the Qt5 build, 4.9.0 for the Qt6 build (auto-selected by `WITH_QT6` in CMake); it drives the `VEYON_DECODE_COMMAND` branch.
- **ABI (toolchain):** Qt5 ⇒ g++ 7.3 + Qt 5.12. Qt6 ⇒ Qt 6.7 + any recent MinGW (we use 13.1.0): the plugin imports only base C++ ABI symbols from libstdc++ (`operator new`/`delete`, `__cxxabiv1` type_info vtables), present in every Veyon's runtime, so the MinGW version need not match Qt 6.7's bundled 11.2.0.
- **Do not commit `*_instructions*.txt` or any chatbot-instruction file** (already covered by `.gitignore`).
- **Per-IP / per-domain blocking is intentionally NOT attempted (don't re-add it).** An "AI-sites only" feature (block ChatGPT/Claude/… while leaving the rest of the internet up) was prototyped via the `hosts` file (+ blocking public DoH/DoT resolvers to defeat browser "Secure DNS") and then **removed** because it cannot be made reliable or safe from the client with `netsh`/`hosts`:
  1. The `hosts` file and DoH-blocking only affect **new** name resolutions. Browsers keep an **internal DNS cache and persistent HTTP/2-3 connections**; once a site is open it keeps working (real requests included) regardless — `ipconfig /flushdns` does not clear the browser's own cache.
  2. Cutting an already-open connection requires blocking the destination **IP**, but the major AI sites sit on **shared CDNs (Cloudflare/Google)**, so per-IP firewall rules cause massive collateral damage.
  3. Windows Firewall cannot filter by hostname/SNI, so it cannot target a single domain at the network layer.
  Reliable per-site filtering belongs at the **network level** (DNS filtering on the router/firewall, or a filtering proxy), outside this plugin. The **full** internet block stays reliable because it filters by *port* and therefore also kills existing connections.
