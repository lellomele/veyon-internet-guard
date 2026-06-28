/*
 * VeyonCompat.h
 * Copyright (C) 2025-26  prof. Ing. Raffaele Mele
 *
 * Single point of contact with the Veyon plugin API.
 *
 * --------------------------------------------------------------------------
 * Compatibility notes (analisi di compatibilità)
 * --------------------------------------------------------------------------
 * SOURCE vs BINARY compatibility — read this before changing the build.
 *
 * At *source* level the plugin API (PluginInterface, FeatureProviderInterface,
 * Feature, FeatureMessage) compiles unchanged against every Veyon release from
 * 4.7.5 to 4.10.x, so no per-version #if branches are needed in the .cpp.
 *
 * The *binary* ABI is a different matter: it is tied to the Veyon **core**
 * version the plugin is compiled against — the class/vtable layout of those
 * interfaces and the symbols exported by veyon-core.dll. Building against the
 * wrong core version makes the Veyon Server *crash on load* (this actually
 * happened: a Qt6 plugin built against 4.7.5 headers crashed Veyon 4.10.x).
 * Two axes drive the build matrix:
 *
 *   1. Qt branch + version-gate. Veyon ships Qt5 up to 4.8.x and Qt6 from
 *      4.9.0. Qt's plugin loader also rejects a plugin built with a Qt *minor*
 *      newer than the host's. The lowest Qt in the 4.9-4.10 range is *6.7.2*
 *      (shipped by 4.9.0; later 4.9.x moved to 6.8, 4.10.x to 6.10), so a Qt6
 *      plugin must be built with Qt 6.7 to load on the whole range.
 *   2. Core ABI. The interfaces this plugin uses are binary-compatible across
 *      4.9.0-4.10.x: the PluginInterface/FeatureProviderInterface headers are
 *      identical, the FeatureMessage data layout is unchanged (`m_command` stays a
 *      4-byte field), and the imported core symbols exist in every release —
 *      verified by diffing core/src and by loading the very same DLL on 4.9.0,
 *      4.9.8 and 4.10.4. So one Qt6 build, against the 4.9.0 core, covers the range.
 *
 * Resulting builds (selected in CMakeLists.txt):
 *
 *   - WITH_QT6=OFF -> Qt5,   core 4.7.5 -> Veyon 4.7.5-4.8.x
 *   - WITH_QT6=ON  -> Qt 6.7, core 4.9.0 -> Veyon 4.9.0-4.10.x
 *
 * The C++ is restricted to C++14 (as the Veyon core itself), so the same
 * translation unit compiles unchanged against either branch. This header is the
 * single place where any future version shim should live.
 * --------------------------------------------------------------------------
 */

#pragma once

// Target Veyon version, injected by CMake (VEYON_TARGET_VERSION).
// Defaults keep the header usable if compiled outside the CMake project.
#ifndef VEYON_TARGET_VERSION_MAJOR
#define VEYON_TARGET_VERSION_MAJOR 4
#endif
#ifndef VEYON_TARGET_VERSION_MINOR
#define VEYON_TARGET_VERSION_MINOR 7
#endif
#ifndef VEYON_TARGET_VERSION_PATCH
#define VEYON_TARGET_VERSION_PATCH 5
#endif

#define VEYON_VERSION_NUM(major, minor, patch) \
	(((major) << 16) | ((minor) << 8) | (patch))

#define VEYON_TARGET_VERSION \
	VEYON_VERSION_NUM(VEYON_TARGET_VERSION_MAJOR, \
	                  VEYON_TARGET_VERSION_MINOR, \
	                  VEYON_TARGET_VERSION_PATCH)

// True when building against Veyon >= the given version. Reserved for future
// version-specific shims; currently no call site needs it.
#define VEYON_VERSION_AT_LEAST(major, minor, patch) \
	(VEYON_TARGET_VERSION >= VEYON_VERSION_NUM(major, minor, patch))

// The full Veyon plugin API surface the plugin relies on. Including this one
// header (instead of the individual Veyon headers) keeps every dependency on
// the Veyon core in a single, auditable place.
#include "PluginInterface.h"
#include "FeatureProviderInterface.h"
#include "FeatureMessage.h"
#include "VeyonServerInterface.h"

// FeatureMessage::command() API change in 4.10.0:
//   ≤4.9.x  command() → int (directly comparable to user enums)
//   ≥4.10.0 command() → FeatureMessage::Command (class enum);
//           use the template overload command<YourEnum>() to decode.
#if VEYON_VERSION_AT_LEAST(4, 10, 0)
#  define VEYON_DECODE_COMMAND(msg, EnumType) ((msg).command<EnumType>())
#else
#  define VEYON_DECODE_COMMAND(msg, EnumType) (static_cast<EnumType>((msg).command()))
#endif
