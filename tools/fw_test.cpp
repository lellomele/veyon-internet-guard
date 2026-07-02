/*
 * tools/fw_test.cpp
 * Copyright (C) 2025-26  prof. Ing. Raffaele Mele
 *
 * Standalone manual test for the WindowsFirewall COM backend — no Veyon, no
 * Qt (qtshim/QtGlobal maps qWarning to stderr), no runtime dependencies.
 *
 * Build (any MinGW, from the repo root):
 *
 *   g++ -std=c++14 -I tools/qtshim -I . tools/fw_test.cpp WindowsFirewall.cpp ^
 *       -o fw_test.exe -static -static-libgcc -static-libstdc++ ^
 *       -lole32 -loleaut32 -ladvapi32
 *
 * Run it from an *elevated* prompt to exercise the real rule creation
 * (expected: block=true with no warnings; the VeyonIG_* rules exist between
 * step 2 and step 3, then the test removes them itself). From a normal
 * prompt it verifies the graceful-failure path instead (block=false with
 * one hr=0x80070005 warning per rule, no crash).
 * NOTE: while step 2 is active the machine's internet access is really
 * blocked for an instant; the test always cleans up before exiting.
 */

#include <cstdio>
#include <windows.h>

#include "WindowsFirewall.h"

// True when the process token is member of the Administrators group
// (i.e. the prompt was started elevated).
static bool isElevated()
{
	BOOL admin = FALSE;
	PSID adminGroup = nullptr;
	SID_IDENTIFIER_AUTHORITY ntAuthority = { SECURITY_NT_AUTHORITY };
	if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
	                             DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup))
	{
		if (!CheckTokenMembership(nullptr, adminGroup, &admin))
			admin = FALSE;
		FreeSid(adminGroup);
	}
	return admin != FALSE;
}

int main()
{
	const bool elevated = isElevated();
	std::printf("running %s\n", elevated
	            ? "ELEVATED: rules will really be created (and removed at the end)"
	            : "WITHOUT admin rights: expect block=false with clean per-rule warnings");

	std::printf("--- step 1: allowInternet (initial cleanup, should be silent) ---\n");
	std::printf("allowInternet -> %s\n", WindowsFirewall::allowInternet() ? "true" : "false");

	std::printf("--- step 2: blockInternet ---\n");
	std::printf("blockInternet -> %s\n", WindowsFirewall::blockInternet() ? "true" : "false");

	std::printf("--- step 3: allowInternet (cleanup) ---\n");
	std::printf("allowInternet -> %s\n", WindowsFirewall::allowInternet() ? "true" : "false");

	return 0;
}
