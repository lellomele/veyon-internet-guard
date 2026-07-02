/*
 * WindowsFirewall.h
 * Copyright (C) 2025-26  prof. Ing. Raffaele Mele
 *
 * Windows Firewall backend for the InternetGuard plugin.
 *
 * Talks to the firewall through the native COM API (INetFwPolicy2 /
 * INetFwRule, "HNetCfg" objects) instead of spawning netsh processes:
 * no external tool on PATH, no locale-dependent output parsing, no
 * process timeouts, and precise per-call HRESULT error reporting.
 * The rules created are the same persistent Windows Firewall rules
 * netsh would create (visible in "Windows Defender Firewall with
 * Advanced Security").
 *
 * Compiled on Windows only (see CMakeLists.txt); the header itself has
 * no Windows dependency so it can be included unconditionally.
 */

#pragma once

namespace WindowsFirewall
{

// Enables the firewall on all profiles, then (re-)adds the outbound block
// rules (HTTP, HTTPS, DNS, QUIC, DoT, proxy ports). Idempotent: existing
// InternetGuard rules are removed first. Returns false if any rule could
// not be applied (details are logged via qWarning).
bool blockInternet();

// Removes every InternetGuard firewall rule (including legacy rule names
// from older plugin versions). Idempotent and cheap when nothing is
// installed. Returns false only on unexpected COM failures.
bool allowInternet();

} // namespace WindowsFirewall
