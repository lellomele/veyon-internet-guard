/*
 * installer.cpp - self-contained Windows installer for the InternetGuard
 * Veyon plugin.
 *
 * Copyright (C) 2025-26  prof. Ing. Raffaele Mele
 *
 * What it does:
 *   1. Suggests a default Veyon installation folder (registry / Program Files).
 *   2. Lets the user pick/confirm the folder with a native browse dialog.
 *   3. Detects whether that Veyon uses Qt 5 or Qt 6 (Qt5Core.dll / Qt6Core.dll)
 *      and automatically installs the matching plugin variant - so the wrong
 *      library can never be installed.
 *   4. Reads the Veyon version (from the bundled executables, falling back to
 *      veyon-core.dll) and warns if it is below the supported floor (4.7.5) or
 *      cannot be read at all - two distinct, non-blocking messages.
 *   5. Copies the matching internet-guard-qt{5,6}.dll into <folder>\plugins\ and
 *      removes the other-Qt variant if it was left there by a previous install.
 *   6. Uses modern TaskDialog dialogs (large heading text, native icons).
 *   7. On "access denied", offers to relaunch itself elevated (UAC).
 *
 * No Qt dependency: pure Win32 + statically linked libstdc++/libgcc, so the
 * installer is a single standalone .exe carrying both plugin variants.
 *
 * All user-facing text is bilingual: Italian first, then English.
 */

#define WINVER       0x0600
#define _WIN32_WINNT 0x0600

#include <windows.h>
#include <shlobj.h>
#include <commctrl.h>
#include <string>
#include <vector>

// Resource ids of the embedded plugin DLLs (see installer.rc).
#define IDR_PLUGIN_QT5 101
#define IDR_PLUGIN_QT6 102

static const wchar_t* kAppTitle   = L"Installazione plugin Veyon / InternetGuard - Veyon plugin setup";
static const wchar_t* kPluginQt5  = L"internet-guard-qt5.dll";
static const wchar_t* kPluginQt6  = L"internet-guard-qt6.dll";

// Supported Veyon floor: 4.7.5 (warn-and-proceed below this).
static const WORD kMinMajor = 4, kMinMinor = 7, kMinPatch = 5;

enum QtKind { QtNone, Qt5, Qt6 };

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------

static bool dirExists(const std::wstring& path)
{
	const DWORD attr = GetFileAttributesW(path.c_str());
	return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
}

static bool fileExists(const std::wstring& path)
{
	const DWORD attr = GetFileAttributesW(path.c_str());
	return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

static std::wstring readRegString(HKEY root, const wchar_t* subKey, const wchar_t* value)
{
	HKEY key{};
	// Query both 64-bit and 32-bit views.
	for (DWORD view : {KEY_WOW64_64KEY, KEY_WOW64_32KEY})
	{
		if (RegOpenKeyExW(root, subKey, 0, KEY_READ | view, &key) == ERROR_SUCCESS)
		{
			wchar_t buffer[MAX_PATH]{};
			DWORD size = sizeof(buffer);
			DWORD type = 0;
			const LONG r = RegQueryValueExW(key, value, nullptr, &type,
			                                reinterpret_cast<LPBYTE>(buffer), &size);
			RegCloseKey(key);
			if (r == ERROR_SUCCESS && (type == REG_SZ || type == REG_EXPAND_SZ))
			{
				return std::wstring(buffer);
			}
		}
	}
	return {};
}

// Best guess for the Veyon installation directory.
static std::wstring guessVeyonDir()
{
	// 1. Uninstall entry written by the official Veyon installer.
	std::wstring p = readRegString(HKEY_LOCAL_MACHINE,
	    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Veyon",
	    L"InstallLocation");
	if (!p.empty() && dirExists(p)) return p;

	// 2. Common Program Files locations.
	for (const wchar_t* env : {L"ProgramW6432", L"ProgramFiles", L"ProgramFiles(x86)"})
	{
		wchar_t base[MAX_PATH]{};
		if (GetEnvironmentVariableW(env, base, MAX_PATH))
		{
			std::wstring candidate = std::wstring(base) + L"\\Veyon";
			if (dirExists(candidate)) return candidate;
		}
	}
	// 3. Fallback default even if it does not exist yet.
	return L"C:\\Program Files\\Veyon";
}

// Detect which Qt major version this Veyon folder ships. Qt 6 takes precedence
// (an installation never carries both, but if it somehow did, Qt 6 is current).
static QtKind detectQt(const std::wstring& veyonDir)
{
	if (fileExists(veyonDir + L"\\Qt6Core.dll")) return Qt6;
	if (fileExists(veyonDir + L"\\Qt5Core.dll")) return Qt5;
	return QtNone;
}

// ---------------------------------------------------------------------------
// Veyon version detection
// ---------------------------------------------------------------------------
//
// The version is read from the binary VS_FIXEDFILEINFO. Important: on Veyon
// 4.7.5 the *DLL* (veyon-core.dll) carries NO version resource, while every
// bundled *executable* does - and newer builds (e.g. 4.10.4) embed it in the
// DLL too. So we try the executables first and fall back to the DLL, taking the
// first file that yields a valid version.

struct VeyonVersion { WORD major, minor, patch; bool valid; };

static VeyonVersion readVersionFromFile(const std::wstring& file)
{
	VeyonVersion v{0, 0, 0, false};

	DWORD ignored = 0;
	const DWORD size = GetFileVersionInfoSizeW(file.c_str(), &ignored);
	if (size == 0) return v;

	std::vector<BYTE> buffer(size);
	if (!GetFileVersionInfoW(file.c_str(), 0, size, buffer.data())) return v;

	VS_FIXEDFILEINFO* ffi = nullptr;
	UINT len = 0;
	if (!VerQueryValueW(buffer.data(), L"\\", reinterpret_cast<LPVOID*>(&ffi), &len) || !ffi)
		return v;

	v.major = HIWORD(ffi->dwFileVersionMS);
	v.minor = LOWORD(ffi->dwFileVersionMS);
	v.patch = HIWORD(ffi->dwFileVersionLS);
	// A file with an empty version resource reports 0.0.0.0 - treat as invalid.
	v.valid = (v.major != 0 || v.minor != 0 || v.patch != 0);
	return v;
}

static VeyonVersion readVeyonVersion(const std::wstring& veyonDir)
{
	// Executables first (always versioned), DLL last (versioned only on newer
	// builds). veyon-master/-server are the most likely to be present.
	const wchar_t* candidates[] = {
		L"\\veyon-master.exe",
		L"\\veyon-server.exe",
		L"\\veyon-service.exe",
		L"\\veyon-configurator.exe",
		L"\\veyon-worker.exe",
		L"\\veyon-cli.exe",
		L"\\veyon-core.dll",
	};
	for (const wchar_t* tail : candidates)
	{
		const VeyonVersion v = readVersionFromFile(veyonDir + tail);
		if (v.valid) return v;
	}
	return VeyonVersion{0, 0, 0, false};
}

// True if version >= 4.7.5 (no hard upper bound; future versions are allowed).
static bool versionSupported(const VeyonVersion& v)
{
	if (!v.valid) return false;
	if (v.major != kMinMajor) return v.major > kMinMajor;
	if (v.minor != kMinMinor) return v.minor > kMinMinor;
	return v.patch >= kMinPatch;
}

static std::wstring versionString(const VeyonVersion& v)
{
	return std::to_wstring(v.major) + L"." + std::to_wstring(v.minor) +
	       L"." + std::to_wstring(v.patch);
}

// ---------------------------------------------------------------------------
// TaskDialog wrappers (modern look, large heading text, native icons)
// ---------------------------------------------------------------------------

// Generic message dialog. Returns the pressed button id (e.g. IDOK, IDYES).
static int showDialog(PCWSTR mainInstruction, PCWSTR content, PCWSTR icon,
                      TASKDIALOG_COMMON_BUTTON_FLAGS buttons, int defaultButton = 0)
{
	TASKDIALOGCONFIG cfg{};
	cfg.cbSize           = sizeof(cfg);
	cfg.dwFlags          = TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW;
	cfg.pszWindowTitle   = kAppTitle;
	cfg.pszMainIcon      = icon;
	cfg.pszMainInstruction = mainInstruction;
	cfg.pszContent       = content;
	cfg.dwCommonButtons  = buttons;
	cfg.nDefaultButton   = defaultButton;

	int pressed = 0;
	if (TaskDialogIndirect(&cfg, &pressed, nullptr, nullptr) != S_OK)
		return 0;
	return pressed;
}

// Final confirmation with a big "Install now" command-link button + Cancel.
// Returns true if the user chose to install.
static bool confirmInstall(PCWSTR mainInstruction, PCWSTR content)
{
	const TASKDIALOG_BUTTON buttons[] = {
		{ 1001, L"Installa adesso\nInstall now" },
	};

	TASKDIALOGCONFIG cfg{};
	cfg.cbSize           = sizeof(cfg);
	cfg.dwFlags          = TDF_ALLOW_DIALOG_CANCELLATION | TDF_USE_COMMAND_LINKS
	                     | TDF_POSITION_RELATIVE_TO_WINDOW;
	cfg.pszWindowTitle   = kAppTitle;
	cfg.pszMainIcon      = TD_INFORMATION_ICON;
	cfg.pszMainInstruction = mainInstruction;
	cfg.pszContent       = content;
	cfg.pButtons         = buttons;
	cfg.cButtons         = ARRAYSIZE(buttons);
	cfg.dwCommonButtons  = TDCBF_CANCEL_BUTTON;
	cfg.nDefaultButton   = 1001;

	int pressed = 0;
	if (TaskDialogIndirect(&cfg, &pressed, nullptr, nullptr) != S_OK)
		return false;
	return pressed == 1001;
}

// ---------------------------------------------------------------------------
// Folder picker
// ---------------------------------------------------------------------------

static int CALLBACK browseCallback(HWND hwnd, UINT msg, LPARAM, LPARAM data)
{
	if (msg == BFFM_INITIALIZED && data)
		SendMessageW(hwnd, BFFM_SETSELECTIONW, TRUE, data);
	return 0;
}

static std::wstring pickFolder(const std::wstring& suggested)
{
	BROWSEINFOW bi{};
	bi.hwndOwner = nullptr;
	bi.lpszTitle = L"Seleziona la cartella di installazione di Veyon (quella che contiene veyon-core.dll)\n"
	               L"Select the Veyon installation folder (the one containing veyon-core.dll)";
	bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
	bi.lpfn = browseCallback;
	bi.lParam = reinterpret_cast<LPARAM>(suggested.c_str());

	LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
	if (!pidl) return {};

	wchar_t path[MAX_PATH]{};
	const BOOL ok = SHGetPathFromIDListW(pidl, path);
	CoTaskMemFree(pidl);
	return ok ? std::wstring(path) : std::wstring();
}

// ---------------------------------------------------------------------------
// Install
// ---------------------------------------------------------------------------

// Write an embedded DLL resource to disk. Returns 0 on success or a Win32
// error code on failure.
static DWORD extractPluginTo(int resourceId, const std::wstring& fullPath)
{
	HRSRC res = FindResourceW(nullptr, MAKEINTRESOURCEW(resourceId), RT_RCDATA);
	if (!res) return GetLastError() ? GetLastError() : ERROR_RESOURCE_DATA_NOT_FOUND;

	HGLOBAL handle = LoadResource(nullptr, res);
	if (!handle) return GetLastError();

	const void* data = LockResource(handle);
	const DWORD size = SizeofResource(nullptr, res);
	if (!data || size == 0) return ERROR_RESOURCE_DATA_NOT_FOUND;

	HANDLE file = CreateFileW(fullPath.c_str(), GENERIC_WRITE, 0, nullptr,
	                          CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == INVALID_HANDLE_VALUE) return GetLastError();

	DWORD written = 0;
	const BOOL ok = WriteFile(file, data, size, &written, nullptr);
	const DWORD err = ok && written == size ? 0 : GetLastError();
	CloseHandle(file);
	if (err != 0) DeleteFileW(fullPath.c_str());
	return err;
}

// Relaunch this executable elevated. Only the folder is passed; the elevated
// instance re-detects the Qt variant from it, so no extra state is needed.
static bool relaunchElevated(const std::wstring& targetDir)
{
	wchar_t exePath[MAX_PATH]{};
	GetModuleFileNameW(nullptr, exePath, MAX_PATH);

	std::wstring params = L"--elevated \"" + targetDir + L"\"";

	SHELLEXECUTEINFOW sei{};
	sei.cbSize = sizeof(sei);
	sei.fMask = SEE_MASK_NOCLOSEPROCESS;
	sei.lpVerb = L"runas";          // triggers the UAC elevation prompt
	sei.lpFile = exePath;
	sei.lpParameters = params.c_str();
	sei.nShow = SW_SHOWNORMAL;

	if (!ShellExecuteExW(&sei)) return false;        // user declined UAC, etc.
	if (sei.hProcess)
	{
		WaitForSingleObject(sei.hProcess, INFINITE);
		CloseHandle(sei.hProcess);
	}
	return true;
}

// Core install routine for an already-chosen folder and detected Qt variant.
// Returns true on success; on access-denied sets *accessDenied so the caller
// can offer elevation.
static bool installInto(const std::wstring& veyonDir, QtKind qt, bool* accessDenied)
{
	*accessDenied = false;

	const wchar_t* pluginName  = (qt == Qt6) ? kPluginQt6 : kPluginQt5;
	const int      resourceId  = (qt == Qt6) ? IDR_PLUGIN_QT6 : IDR_PLUGIN_QT5;
	const wchar_t* otherName   = (qt == Qt6) ? kPluginQt5 : kPluginQt6;

	const std::wstring pluginsDir = veyonDir + L"\\plugins";
	const std::wstring target     = pluginsDir + L"\\" + pluginName;

	// Ensure <veyonDir>\plugins exists.
	if (!dirExists(pluginsDir))
	{
		const int rc = SHCreateDirectoryExW(nullptr, pluginsDir.c_str(), nullptr);
		if (rc != ERROR_SUCCESS && rc != ERROR_ALREADY_EXISTS)
		{
			if (rc == ERROR_ACCESS_DENIED) { *accessDenied = true; return false; }
			std::wstring msg = L"Impossibile creare la cartella dei plugin:\n" + pluginsDir +
			                   L"\n\nCannot create the plugins folder (see above)." +
			                   L"\n\nCodice di errore / Error code: " + std::to_wstring(rc);
			showDialog(L"Installazione non riuscita / Installation failed", msg.c_str(),
			           TD_ERROR_ICON, TDCBF_OK_BUTTON);
			return false;
		}
	}

	const DWORD err = extractPluginTo(resourceId, target);
	if (err == ERROR_ACCESS_DENIED) { *accessDenied = true; return false; }
	if (err != 0)
	{
		std::wstring msg = L"Errore durante la copia del plugin in:\n" + target +
		                   L"\n\nError while copying the plugin (see above)." +
		                   L"\n\nCodice di errore / Error code: " + std::to_wstring(err);
		showDialog(L"Installazione non riuscita / Installation failed", msg.c_str(),
		           TD_ERROR_ICON, TDCBF_OK_BUTTON);
		return false;
	}

	// Remove the other-Qt variant if a previous install left it behind, so the
	// wrong library can never be loaded. Best effort - ignore failures.
	DeleteFileW((pluginsDir + L"\\" + otherName).c_str());

	std::wstring ok = L"Installato in:\n" + target +
	                  L"\n\nRiavviare Veyon Master e Veyon Server affinche' il plugin venga caricato." +
	                  L"\n\nInstalled to the path above. Restart Veyon Master and Veyon Server "
	                  L"so the plugin is loaded.";
	showDialog(L"Plugin installato correttamente / Plugin installed successfully",
	           ok.c_str(), TD_INFORMATION_ICON, TDCBF_OK_BUTTON);
	return true;
}

// ---------------------------------------------------------------------------
// Command line
// ---------------------------------------------------------------------------

// Parse "--elevated <dir>" from the command line, if present.
static bool parseElevated(std::wstring* targetDir)
{
	int argc = 0;
	LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	bool elevated = false;
	if (argv)
	{
		for (int i = 1; i < argc; ++i)
		{
			if (lstrcmpiW(argv[i], L"--elevated") == 0 && i + 1 < argc)
			{
				elevated = true;
				*targetDir = argv[i + 1];
				break;
			}
		}
		LocalFree(argv);
	}
	return elevated;
}

// Build the "ready to install" summary for the confirmation dialog.
static std::wstring describeTarget(const std::wstring& veyonDir, QtKind qt,
                                   const VeyonVersion& ver)
{
	std::wstring s = L"Cartella / Folder:\n" + veyonDir + L"\n\n";

	s += L"Versione Veyon / Veyon version: ";
	s += ver.valid ? versionString(ver) : std::wstring(L"sconosciuta / unknown");
	s += L"\n";

	s += L"Qt: ";
	s += (qt == Qt6) ? L"6" : L"5";
	s += L"  \x2192  ";
	s += (qt == Qt6) ? kPluginQt6 : kPluginQt5;
	return s;
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int)
{
	INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_STANDARD_CLASSES };
	InitCommonControlsEx(&icc);
	CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

	std::wstring targetDir;
	const bool elevated = parseElevated(&targetDir);

	QtKind qt = QtNone;
	VeyonVersion ver{0, 0, 0, false};

	if (!elevated)
	{
		// First run: greet, then let the user choose the folder.
		const int greet = showDialog(
		    L"Plugin InternetGuard per Veyon",
		    L"Questo programma installa il plugin InternetGuard per Veyon.\n"
		    L"Sceglierai la cartella in cui Veyon e' installato; la libreria "
		    L"corretta (Qt 5 o Qt 6) viene rilevata e copiata automaticamente.\n\n"
		    L"This program installs the InternetGuard plugin for Veyon.\n"
		    L"You will choose the folder where Veyon is installed; the matching "
		    L"plugin (Qt 5 or Qt 6) is detected and copied automatically.",
		    TD_INFORMATION_ICON, TDCBF_OK_BUTTON | TDCBF_CANCEL_BUTTON, IDOK);
		if (greet != IDOK) { CoUninitialize(); return 1; }

		targetDir = pickFolder(guessVeyonDir());
		if (targetDir.empty())
		{
			showDialog(L"Installazione annullata / Installation cancelled", nullptr,
			           TD_WARNING_ICON, TDCBF_OK_BUTTON);
			CoUninitialize();
			return 1;
		}

		// The Qt variant is the core safety check: without it we cannot pick the
		// right library, so we must stop.
		qt = detectQt(targetDir);
		if (qt == QtNone)
		{
			showDialog(
			    L"Impossibile determinare la versione di Qt / Could not determine the Qt version",
			    L"Nella cartella selezionata non sono stati trovati ne' Qt5Core.dll ne' "
			    L"Qt6Core.dll. Non sembra una cartella di Veyon valida, quindi non e' "
			    L"possibile scegliere il plugin corretto. Installazione annullata.\n\n"
			    L"Neither Qt5Core.dll nor Qt6Core.dll was found in the selected folder. "
			    L"This does not look like a valid Veyon installation, so the correct "
			    L"plugin cannot be chosen. Installation aborted.",
			    TD_ERROR_ICON, TDCBF_OK_BUTTON);
			CoUninitialize();
			return 1;
		}

		// The Veyon version is advisory: warn but let the user proceed. Two
		// clearly distinct cases - version unreadable vs. version below floor.
		ver = readVeyonVersion(targetDir);
		if (!ver.valid)
		{
			const int r = showDialog(
			    L"Versione di Veyon non rilevata / Veyon version not detected",
			    L"Non e' stato possibile leggere la versione di Veyon dai file "
			    L"dell'installazione. Verifica di aver scelto la cartella giusta.\n"
			    L"Il plugin potrebbe funzionare comunque. Continuare?\n\n"
			    L"The Veyon version could not be read from the installation files. "
			    L"Make sure you picked the correct folder.\n"
			    L"The plugin may still work. Continue?",
			    TD_WARNING_ICON, TDCBF_YES_BUTTON | TDCBF_NO_BUTTON, IDNO);
			if (r != IDYES) { CoUninitialize(); return 1; }
		}
		else if (!versionSupported(ver))
		{
			std::wstring content =
			    L"Rilevato Veyon " + versionString(ver) + L", inferiore al minimo "
			    L"supportato (4.7.5). Il plugin potrebbe funzionare comunque, ma "
			    L"questa versione non e' stata testata. Continuare lo stesso?\n\n"
			    L"Detected Veyon " + versionString(ver) + L", below the supported "
			    L"minimum (4.7.5). The plugin may still work, but this version has "
			    L"not been tested. Continue anyway?";
			const int r = showDialog(
			    L"Versione di Veyon non supportata / Unsupported Veyon version",
			    content.c_str(), TD_WARNING_ICON,
			    TDCBF_YES_BUTTON | TDCBF_NO_BUTTON, IDNO);
			if (r != IDYES) { CoUninitialize(); return 1; }
		}

		// Final confirmation with the resolved target summary.
		if (!confirmInstall(L"Pronto per l'installazione / Ready to install",
		                    describeTarget(targetDir, qt, ver).c_str()))
		{
			CoUninitialize();
			return 1;
		}
	}
	else
	{
		// Elevated re-run: re-detect the Qt variant from the chosen folder.
		qt = detectQt(targetDir);
		if (qt == QtNone)
		{
			showDialog(L"Impossibile determinare la versione di Qt / Could not determine the Qt version",
			           nullptr, TD_ERROR_ICON, TDCBF_OK_BUTTON);
			CoUninitialize();
			return 1;
		}
	}

	bool accessDenied = false;
	const bool ok = installInto(targetDir, qt, &accessDenied);

	if (!ok && accessDenied)
	{
		if (elevated)
		{
			// Already elevated and still denied: nothing more we can do.
			showDialog(
			    L"Accesso negato / Access denied",
			    L"Permessi insufficienti anche con privilegi di amministratore.\n"
			    L"Verificare i permessi sulla cartella di Veyon.\n\n"
			    L"Access denied even with administrator privileges.\n"
			    L"Check the permissions on the Veyon folder.",
			    TD_ERROR_ICON, TDCBF_OK_BUTTON);
			CoUninitialize();
			return 2;
		}

		const int r = showDialog(
		    L"Privilegi di amministratore necessari / Administrator privileges required",
		    L"Permessi insufficienti per scrivere nella cartella di Veyon.\n"
		    L"Rilanciare l'installazione con privilegi di amministratore?\n\n"
		    L"Insufficient permissions to write to the Veyon folder.\n"
		    L"Restart the installation with administrator privileges?",
		    TD_SHIELD_ICON, TDCBF_YES_BUTTON | TDCBF_NO_BUTTON, IDYES);
		if (r == IDYES)
		{
			if (!relaunchElevated(targetDir))
			{
				showDialog(
				    L"Elevazione annullata / Elevation cancelled",
				    L"Elevazione annullata o non riuscita. Installazione non completata.\n\n"
				    L"Elevation cancelled or failed. Installation not completed.",
				    TD_ERROR_ICON, TDCBF_OK_BUTTON);
			}
		}
		CoUninitialize();
		return 3;
	}

	CoUninitialize();
	return ok ? 0 : 3;
}
