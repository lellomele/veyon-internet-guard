/*
 * installer.cpp - self-contained Windows installer for the InternetGuard
 * Veyon plugin.
 *
 * Copyright (C) 2025-26  prof. Ing. Raffaele Mele
 *
 * What it does:
 *   1. Suggests a default Veyon installation folder (registry / Program Files).
 *   2. Lets the user pick/confirm the folder with a native browse dialog.
 *   3. Copies the embedded internet-guard.dll into <folder>\plugins\.
 *   4. Reports any error (including permission errors) via message boxes.
 *   5. On "access denied", offers to relaunch itself elevated (UAC) and
 *      install straight into the previously chosen folder.
 *
 * No Qt dependency: pure Win32 + statically linked libstdc++/libgcc, so the
 * installer is a single standalone .exe.
 */

#include <windows.h>
#include <shlobj.h>
#include <string>

// Resource id of the embedded plugin DLL (see installer.rc).
#define IDR_PLUGIN_DLL 101

static const wchar_t* kAppTitle   = L"InternetGuard - Veyon plugin setup / Installazione plugin Veyon";
#ifndef PLUGIN_NAME
#  define PLUGIN_NAME L"internet-guard.dll"
#endif
static const wchar_t* kPluginName = PLUGIN_NAME;

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------

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

static bool dirExists(const std::wstring& path)
{
	const DWORD attr = GetFileAttributesW(path.c_str());
	return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
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

// Browse-dialog callback: preselect the suggested folder.
static int CALLBACK browseCallback(HWND hwnd, UINT msg, LPARAM, LPARAM data)
{
	if (msg == BFFM_INITIALIZED && data)
	{
		SendMessageW(hwnd, BFFM_SETSELECTIONW, TRUE, data);
	}
	return 0;
}

static std::wstring pickFolder(const std::wstring& suggested)
{
	BROWSEINFOW bi{};
	bi.hwndOwner = nullptr;
	bi.lpszTitle = L"Select the Veyon installation folder (the one containing veyon-core.dll)\n"
	               L"Seleziona la cartella di installazione di Veyon (quella che contiene veyon-core.dll)";
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

// Write the embedded DLL resource to disk. Returns 0 on success or a Win32
// error code on failure.
static DWORD extractPluginTo(const std::wstring& fullPath)
{
	HRSRC res = FindResourceW(nullptr, MAKEINTRESOURCEW(IDR_PLUGIN_DLL), RT_RCDATA);
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

// Relaunch this executable elevated, passing the chosen folder so the elevated
// instance installs without prompting again.
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

// Core install routine for an already-chosen folder. Returns true on success.
// On access-denied it returns false via *accessDenied so the caller can offer
// elevation.
static bool installInto(const std::wstring& veyonDir, bool* accessDenied)
{
	*accessDenied = false;

	const std::wstring pluginsDir = veyonDir + L"\\plugins";
	const std::wstring target     = pluginsDir + L"\\" + kPluginName;

	// Ensure <veyonDir>\plugins exists.
	if (!dirExists(pluginsDir))
	{
		const int rc = SHCreateDirectoryExW(nullptr, pluginsDir.c_str(), nullptr);
		if (rc != ERROR_SUCCESS && rc != ERROR_ALREADY_EXISTS)
		{
			if (rc == ERROR_ACCESS_DENIED) { *accessDenied = true; return false; }
			std::wstring msg = L"Cannot create the plugins folder:\n" + pluginsDir +
			                   L"\n\nImpossibile creare la cartella dei plugin (vedi sopra)." +
			                   L"\n\nError code / Codice di errore: " + std::to_wstring(rc);
			MessageBoxW(nullptr, msg.c_str(), kAppTitle, MB_ICONERROR);
			return false;
		}
	}

	const DWORD err = extractPluginTo(target);
	if (err == ERROR_ACCESS_DENIED) { *accessDenied = true; return false; }
	if (err != 0)
	{
		std::wstring msg = L"Error while copying the plugin to:\n" + target +
		                   L"\n\nErrore durante la copia del plugin (vedi sopra)." +
		                   L"\n\nError code / Codice di errore: " + std::to_wstring(err);
		MessageBoxW(nullptr, msg.c_str(), kAppTitle, MB_ICONERROR);
		return false;
	}

	std::wstring ok = L"Plugin installed successfully to:\n" + target +
	                  L"\n\nRestart Veyon Master and Veyon Server so the plugin is loaded." +
	                  L"\n\n--- IT ---\n"
	                  L"Plugin installato correttamente (vedi percorso sopra).\n"
	                  L"Riavviare Veyon Master e Veyon Server affinche' il plugin venga caricato.";
	MessageBoxW(nullptr, ok.c_str(), kAppTitle, MB_ICONINFORMATION);
	return true;
}

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

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int)
{
	CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

	std::wstring targetDir;
	const bool elevated = parseElevated(&targetDir);

	if (!elevated)
	{
		// First run: greet, then let the user choose the folder.
		MessageBoxW(nullptr,
		    L"This program installs the InternetGuard plugin for Veyon.\n"
		    L"You will be asked to select the folder where Veyon is installed; "
		    L"the plugin will be copied into the \"plugins\" subfolder.\n\n"
		    L"--- IT ---\n"
		    L"Questo programma installa il plugin InternetGuard per Veyon.\n"
		    L"Verra' chiesto di selezionare la cartella in cui Veyon e' "
		    L"installato; il plugin sara' copiato nella sottocartella \"plugins\".",
		    kAppTitle, MB_ICONINFORMATION | MB_OK);

		targetDir = pickFolder(guessVeyonDir());
		if (targetDir.empty())
		{
			MessageBoxW(nullptr, L"Installation cancelled.\nInstallazione annullata.", kAppTitle, MB_ICONWARNING);
			CoUninitialize();
			return 1;
		}

		// Soft sanity check: warn if veyon-core.dll is not present.
		if (!dirExists(targetDir) ||
		    GetFileAttributesW((targetDir + L"\\veyon-core.dll").c_str()) == INVALID_FILE_ATTRIBUTES)
		{
			const int r = MessageBoxW(nullptr,
			    L"veyon-core.dll was not found in the selected folder.\n"
			    L"This may not be the correct Veyon folder. Continue anyway?\n\n"
			    L"--- IT ---\n"
			    L"Nella cartella selezionata non e' stato trovato veyon-core.dll.\n"
			    L"Potrebbe non essere la cartella corretta di Veyon. Continuare comunque?",
			    kAppTitle, MB_ICONWARNING | MB_YESNO);
			if (r != IDYES) { CoUninitialize(); return 1; }
		}
	}

	bool accessDenied = false;
	const bool ok = installInto(targetDir, &accessDenied);

	if (!ok && accessDenied)
	{
		if (elevated)
		{
			// Already elevated and still denied: nothing more we can do.
			MessageBoxW(nullptr,
			    L"Access denied even with administrator privileges.\n"
			    L"Check the permissions on the Veyon folder.\n\n"
			    L"--- IT ---\n"
			    L"Permessi insufficienti anche con privilegi di amministratore.\n"
			    L"Verificare i permessi sulla cartella di Veyon.",
			    kAppTitle, MB_ICONERROR);
			CoUninitialize();
			return 2;
		}

		const int r = MessageBoxW(nullptr,
		    L"Insufficient permissions to write to the Veyon folder.\n"
		    L"Restart the installation with administrator privileges?\n\n"
		    L"--- IT ---\n"
		    L"Permessi insufficienti per scrivere nella cartella di Veyon.\n"
		    L"Rilanciare l'installazione con privilegi di amministratore?",
		    kAppTitle, MB_ICONQUESTION | MB_YESNO);
		if (r == IDYES)
		{
			if (!relaunchElevated(targetDir))
			{
				MessageBoxW(nullptr,
				    L"Elevation cancelled or failed. Installation not completed.\n"
				    L"Elevazione annullata o non riuscita. Installazione non completata.",
				    kAppTitle, MB_ICONERROR);
			}
		}
		CoUninitialize();
		return ok ? 0 : 3;
	}

	CoUninitialize();
	return ok ? 0 : 3;
}
