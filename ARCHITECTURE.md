# Architecture notes

Technical/architecture summary for contributors. User-facing documentation
(Italian) lives in `README.md`.

## What this is

A [Veyon](https://veyon.io/) teacher-side plugin that adds a "Block / Allow Internet" toggle to the Veyon Master UI. When the teacher activates the feature, the Veyon Server running on each student machine adds Windows Firewall rules (via `netsh advfirewall`) that block outbound internet traffic while keeping the local subnet (LAN) reachable. Deactivating removes those rules.

## Version compatibility (important)

The Veyon plugin interfaces used here — `PluginInterface`, `FeatureProviderInterface` (`controlFeature`, `handleFeatureMessage`, `sendFeatureMessage`), `Feature` (9-arg constructor + `Flag` enum), `FeatureMessage` — are **stable across every Veyon release from 4.7.5 through 4.10.x**. The source needs **no per-version `#if` branches for the API itself**. The only axis of variation is the **Qt major version** the target Veyon was built with:

- Veyon 4.7.5 – 4.9.x → Qt 5 (default, `WITH_QT6=OFF`) → output: `internet-guard-qt5.dll`
- Veyon 4.10.x → Qt 6 (`-DWITH_QT6=ON`) → output: `internet-guard-qt6.dll`

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
| Qt (Core, Widgets, Network) | Qt 5.12 at `C:/Qt/5.12.12/mingw73_64` | Qt **6.10.x** MinGW (e.g. `C:/Qt-aqt/6.10.3/mingw_64`) |
| MinGW toolchain | `C:/Qt/Tools/mingw730_64` (g++ 7.3) | MinGW **13.1.0** that ships with Qt 6.10 (`tools_mingw1310`) |
| Veyon source tree | `../veyon-src/core/src` (4.7.5 headers — see note) | `../veyon-src/core/src` (4.7.5 headers — see note) |
| Veyon import library | `libveyon-core.dll.a` (in repo root) | `libveyon-core-qt6.dll.a` (in repo root) |
| C++ standard | C++14 | C++14 |

> Qt Svg is **not** a dependency: the toolbar icon is a PNG (see "Toolbar icon"
> below), so `QIcon` needs no SVG icon-engine plugin.

**ABI note (Qt 5):** build with the **same compiler/Qt that the installed Veyon uses**. The official Windows Veyon 4.7.5–4.9.x builds use MinGW g++ 7.3 + Qt 5.12; using a different MinGW (e.g. MSYS2 GCC) can produce a DLL that fails to load.

**ABI note (Qt 6 / GCC 16):** GCC 16 emits both a local vtable and `__imp__ZTV` references for `dllimport` base classes (`FeatureProviderInterface`, `PluginInterface`). Both definitions are identical (same header), so the linker flag `-Wl,--allow-multiple-definition` is added for WIN32 builds to silently resolve the conflict. This is harmless and expected with this toolchain combination.

**Configure and build (Qt 5 / Veyon 4.7.5–4.9.x)**

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

**Configure and build (Qt 6 / Veyon 4.10.x — verified against Veyon 4.10.3 / Qt 6.10.3)**

> ⚠️ **Critical: match the target Veyon's Qt 6 *minor* version (or build older).**
> Qt's plugin loader rejects — *silently*, with no error and no toolbar button —
> any plugin built with a Qt **newer** than the host application
> (rule: plugin minor ≤ host minor, same major). Veyon 4.10.3 ships Qt **6.10.3**,
> so the plugin must be built with Qt ≤ 6.10. An earlier attempt built with
> MSYS2's rolling **Qt 6.11** produced a DLL tagged `qt_version_tag_6_11` that
> 4.10.3 refused to load. Check the tag of a built DLL with:
> `strings internet-guard-qt6.dll | grep qt_version_tag` — it must be ≤ the
> target's Qt. To find the target's Qt: read the `ProductVersion` of
> `Qt6Core.dll` in the Veyon install folder.

Get a matching Qt 6.10 MinGW toolchain non-interactively with
[`aqtinstall`](https://github.com/miurahr/aqtinstall) (no Qt account needed):

```powershell
py -m pip install aqtinstall
py -m aqt install-qt   windows desktop 6.10.3 win64_mingw --outputdir C:\Qt-aqt
py -m aqt install-tool windows desktop tools_mingw1310     --outputdir C:\Qt-aqt
```

Then configure and build with **MinGW Makefiles** (not Ninja — keeps the toolchain explicit):

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

Output: `build-qt6/internet-guard-qt6.dll`.

The repo ships `libveyon-core-qt6.dll.a` (generated from Veyon 4.10.x's
`veyon-core.dll` via `objdump` + `dlltool`) and `libveyon-core-qt6.def`
(the full export list). They are selected automatically when `WITH_QT6=ON`.

**Why `VEYON_TARGET_VERSION=4.7.5` even when targeting Veyon 4.10.3.**
The repo ships only the 4.7.5 `core/src` headers, whose `FeatureMessage` exposes
`command()` as a plain `qint32`. That is fine against a 4.10.3 `veyon-core.dll`:
`FeatureMessage`'s member layout is **byte-for-byte identical** between 4.7.5 and
4.10.3 (`QUuid` + 4-byte command + `QVariantMap`, same order) and its wire format
serialises the command as `qint32` in both. So a plugin compiled with the 4.7.5
headers and the `<4.10` decode path (`static_cast<Commands>(msg.command())`)
inter-operates correctly with Veyon 4.10.3 — confirmed by inspecting the 4.10.3
`FeatureMessage.h`. Only if the repo is ever updated to *actual* 4.10+ headers
must `VEYON_TARGET_VERSION` be set to ≥ 4.10 so `VEYON_DECODE_COMMAND` switches to
the `command<Commands>()` template overload.

For the CMake flags (`WITH_QT6`, `VEYON_TARGET_VERSION`, `VEYON_SOURCE_DIR`, `VEYON_CORE_LIBRARY`), see `README.md` §5–§6.

## Architecture

The plugin is a single shared library (`internet-guard-qt5.dll` or `internet-guard-qt6.dll`) built from:

- **`InternetGuardPlugin.h/.cpp`** — the entire plugin logic. Inherits from both `FeatureProviderInterface` and `PluginInterface`. Two roles depending on which side of Veyon loads it:
  - **Master side** (`controlFeature`): on `Operation::Start`/`Stop` sends a `BlockInternetCommand` / `AllowInternetCommand` `FeatureMessage` to the targeted computers via `sendFeatureMessage`.
  - **Server side** (`handleFeatureMessage`): receives the message and calls `blockInternet()` / `allowInternet()`. These run `netsh` via `runNetshBatch()`, which launches all commands as parallel `QProcess` instances and then waits — total wall time ≈ max(individual) instead of sum. `blockInternet()` first calls `ensureFirewallEnabled()` (`netsh advfirewall set allprofiles state on`) because block rules have no effect while a profile is off — the typical cause of a single client not being blocked. The destructor calls `allowInternet()` so stale rules are never left on unload.

  **Features exposed** (`featureList()`): a `Mode` toggle (`Block/Allow Internet`, toolbar) plus two `Action` sub-features (`Block Internet` / `Allow Internet`) whose `parentUid` is the toggle. The toggle gives a one-click global block/allow; the sub-features appear in the toolbar dropdown and the right-click context menu and act on **exactly the selected computers** — Veyon already passes the selection to `controlFeature`, but a `Mode` toggle tracks a single global state, so explicit per-selection `Action` features are what make independent per-client control reliable. All commands are dispatched by command code in `handleFeatureMessage`, independent of which feature UID carried them.

- **`VeyonCompat.h`** — single point of contact with the Veyon API + version macros + the `VEYON_DECODE_COMMAND` compatibility macro (see Version compatibility above).

- **`resources.qrc`** — embeds `network-offline.png` as the toolbar icon (`:/internet-guard/network-offline.png`). See "Toolbar icon" below for why it is a PNG, not an SVG.

- **`installer/`** — standalone native Win32 installer (`installer.cpp`, `installer.rc`, `installer.manifest`, `build-installer.ps1`). Statically linked, no Qt dependency; embeds the plugin DLL as an RCDATA resource. It suggests the Veyon folder (registry/Program Files), lets the user pick it, copies the DLL into `…\plugins\`, reports permission errors, and self-elevates (UAC `runas`) on access-denied. Build with `pwsh -File installer\build-installer.ps1 -PluginDll build-qt5\internet-guard-qt5.dll` (or `-qt6` variant).

### Firewall rule names

All rules use the prefix `VeyonIG_` so they can be reliably deleted. `blockInternet()` always calls `allowInternet()` first to avoid duplicate accumulation.

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
| `VeyonIG_AllowLAN` | any → `localsubnet` | Keeps the LAN reachable (more-specific `remoteip` rule wins) |

### Plugin identity

The plugin's UUID `a4b3c2d1-e5f6-7890-abcd-ef1234567890` is the identity of the **toggle** feature and must stay consistent across `uid()` and the toggle's `Feature::Uid` (the metadata IID `io.veyon.Veyon.Plugins.InternetGuard` is a separate, stable string). The two `Action` sub-features have their own constant UUIDs; only uniqueness matters for those.

### Qt plugin metadata export

Qt 5 exports `qt_plugin_query_metadata`; Qt 6 exports `qt_plugin_query_metadata_v2`. Both are generated automatically by `moc` / `AUTOMOC` — no manual action needed.

### Toolbar icon (PNG, not SVG)

Veyon renders a feature's icon with `QIcon(feature.iconUrl())`. Two constraints, both learned the hard way:

1. **No SVG at runtime.** Veyon's Windows deployment does not ship Qt's SVG icon-engine plugin (`iconengines/qsvgicon.dll`), so `QIcon(":/…svg")` yields an *empty* icon — the toolbar button appears but blank (same background as its neighbours). Every built-in Veyon plugin uses PNG icons; so does this one (`:/internet-guard/network-offline.png`). This also removes any dependency on Qt Svg.
2. **`network-offline.svg` is kept only as the editable source.** Regenerate the PNG after editing it. Note that Qt's SVG module is "SVG Tiny" and does **not** parse `rgba(r,g,b,a)` fill notation — it renders such a shape as opaque black (this is what originally hid the globe). Use `fill="#rrggbb" fill-opacity="0.x"` instead. Rasterize with any Qt build that has the Svg module (a ~20-line `QSvgRenderer` → `QPainter` on a transparent `QImage` → `img.save()` program at 128×128 is enough).

## Lessons learned (read before changing build/icon/version code)

- **Qt 6 plugin version gate.** Build the Qt 6 plugin with a Qt **≤ the target Veyon's Qt minor** (4.10.3 ⇒ Qt ≤ 6.10). A newer Qt makes Veyon reject the plugin silently (no button). Verify with `strings … | grep qt_version_tag`. See the Qt 6 build section.
- **Icon must be PNG**, not SVG, and the source SVG must avoid `rgba()`. See "Toolbar icon".
- **`VEYON_TARGET_VERSION=4.7.5` is correct for *both* Qt5 and Qt6 builds** while the repo ships 4.7.5 headers; the `FeatureMessage` layout/wire format is identical up to 4.10.3. See "Why `VEYON_TARGET_VERSION=4.7.5`…".
- **ABI:** match the target Veyon's MinGW/Qt — Qt5 ⇒ g++ 7.3 + Qt 5.12; Qt6 ⇒ the MinGW (13.1.0) bundled with the matching Qt 6.10.
- **Do not commit `*_instructions*.txt` or any chatbot-instruction file** (already covered by `.gitignore`).
- **Per-IP / per-domain blocking is intentionally NOT attempted (don't re-add it).** An "AI-sites only" feature (block ChatGPT/Claude/… while leaving the rest of the internet up) was prototyped via the `hosts` file (+ blocking public DoH/DoT resolvers to defeat browser "Secure DNS") and then **removed** because it cannot be made reliable or safe from the client with `netsh`/`hosts`:
  1. The `hosts` file and DoH-blocking only affect **new** name resolutions. Browsers keep an **internal DNS cache and persistent HTTP/2-3 connections**; once a site is open it keeps working (real requests included) regardless — `ipconfig /flushdns` does not clear the browser's own cache.
  2. Cutting an already-open connection requires blocking the destination **IP**, but the major AI sites sit on **shared CDNs (Cloudflare/Google)**, so per-IP firewall rules cause massive collateral damage.
  3. Windows Firewall cannot filter by hostname/SNI, so it cannot target a single domain at the network layer.
  Reliable per-site filtering belongs at the **network level** (DNS filtering on the router/firewall, or a filtering proxy), outside this plugin. The **full** internet block stays reliable because it filters by *port* and therefore also kills existing connections.
