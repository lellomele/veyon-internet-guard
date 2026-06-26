/*
 * VeyonCompat.h
 * Copyright (C) 2025-26  prof. Ing. Raffaele Mele
 *
 * Single point of contact with the Veyon plugin API.
 *
 * --------------------------------------------------------------------------
 * Compatibility notes (analisi di compatibilità)
 * --------------------------------------------------------------------------
 * The Veyon feature-plugin API used by InternetGuard is *stable* across every
 * release from 4.7.5 up to and including the 4.10.x line:
 *
 *   - PluginInterface            uid()/version()/name()/... : unchanged
 *   - FeatureProviderInterface   controlFeature(), the 3-arg
 *                                handleFeatureMessage(VeyonServerInterface&,
 *                                MessageContext&, FeatureMessage&) and the
 *                                protected sendFeatureMessage() helper :
 *                                unchanged (master only *adds* the optional
 *                                handleFeatureMessageFromWorker() virtual,
 *                                which we do not need to override).
 *   - Feature                    9-argument constructor + Flag enum : unchanged
 *   - FeatureMessage             FeatureMessage(uid, command) : unchanged
 *
 * Therefore the plugin source needs *no* per-version #if branches for the API
 * itself.  The only real axis of variation is the Qt major version that the
 * target Veyon build was compiled with:
 *
 *   - Veyon 4.7.x .. 4.9.x   -> Qt 5   (CMake default, WITH_QT6=OFF)
 *   - Veyon 4.10.x / Qt6 pkg -> Qt 6   (configure with -DWITH_QT6=ON)
 *
 * The Qt5/Qt6 difference is handled entirely in CMakeLists.txt (which Qt
 * package is found and linked).  The C++ used here is restricted to C++14 —
 * the standard Veyon core itself is built with — so the same translation unit
 * compiles unchanged against either Qt branch.
 *
 * This header exists so that, should a future Veyon release ever break source
 * compatibility, the shim lives here and nowhere else.
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
