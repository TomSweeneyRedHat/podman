#include <windows.h>
#include <MsiQuery.h>

BOOL isWSLEnabled();
BOOL isHyperVEnabled();
LPCWSTR boolToNStr(BOOL value);
LPCSTR szSvcNameHyperv = TEXT("vmms");

/**
 * CheckWSL is a custom action loaded by the Podman Windows installer
 * to determine whether the system already has WSL installed.
 *
 * The intention is that this action is compiled for x86_64, which
 * can be ran on both Intel and Arm based systems (the latter through
 * emulation). While the code should build fine on MSVC and clang, the
 * intended usage is MingW-W64 (cross-compiling gcc targeting Windows).
 *
 * Previously this was implemented as a Golang c-shared cgo library,
 * however, the WoW x86_64 emulation layer struggled with dynamic
 * hot-loaded transformation of the goruntime into an existing process
 * (required by MSI custom actions). In the future this could be
 * converted back, should the emulation issue be resolved.
 */

 __declspec(dllexport) UINT __cdecl CheckWSL(MSIHANDLE hInstall) {
	BOOL hasWSL = isWSLEnabled();
	// Set a property with the WSL state for the installer to operate on
	MsiSetPropertyW(hInstall, L"HAS_WSLFEATURE", boolToNStr(hasWSL));

	return 0;
}

/**
 * CheckHyperV is a custom action loaded by the Podman Windows installer
 * to determine whether the system already has Hyper-V installed.
 */

 __declspec(dllexport) UINT __cdecl CheckHyperV(MSIHANDLE hInstall) {
	BOOL hasHyperV = isHyperVEnabled();
	// Set a property with the HyperV state for the installer to operate on
	MsiSetPropertyW(hInstall, L"HAS_HYPERVFEATURE", boolToNStr(hasHyperV));

	return 0;
}

LPCWSTR boolToNStr(BOOL value) {
	return value ? L"1" : L"0";
}

BOOL isWSLEnabled() {
	/*
	 * The simplest, and most reliable check across all variants and versions
	 * of WSL appears to be changing the default version to WSL 2 and check
	 * for errors, which we need to do anyway.
	 */
	STARTUPINFOW startup;
	PROCESS_INFORMATION process;

	ZeroMemory(&startup, sizeof(STARTUPINFOW));
	startup.cb = sizeof(STARTUPINFOW);

	// These settings hide the console window, so there is no annoying flash
	startup.dwFlags = STARTF_USESHOWWINDOW;
	startup.wShowWindow = SW_HIDE;

	// CreateProcessW requires lpCommandLine to be mutable
	wchar_t cmd[] = L"wsl --set-default-version 2";
	if (! CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NEW_CONSOLE,
						 NULL, NULL, &startup, &process)) {

		return FALSE;
	}

	DWORD exitCode;
	WaitForSingleObject(process.hProcess, INFINITE);
	if (! GetExitCodeProcess(process.hProcess, &exitCode)) {
		return FALSE;
	}

	return exitCode == 0;
}

BOOL isHyperVEnabled() {
	/*
	 * Checks if the Windows service `vmms` is running to
	 * determine if Hyper-V is enabled.
	 */
	SC_HANDLE schSCManager;
	SC_HANDLE schService;
	SERVICE_STATUS_PROCESS ssStatus;
	DWORD dwBytesNeeded;

	// Get a handle to the SCM database.
	schSCManager = OpenSCManager(
		NULL,                    // local computer
		NULL,                    // servicesActive database
		SERVICE_QUERY_STATUS);   // service query access rights

	if (NULL == schSCManager) {
		return FALSE;
	}

	// Get a handle to the service.
	schService = OpenService(
		schSCManager,
		szSvcNameHyperv,
		SERVICE_QUERY_STATUS);

	if (schService == NULL) {
		CloseServiceHandle(schSCManager);
		return FALSE;
	}

	// Check the status
	if (!QueryServiceStatusEx(
			schService,                     // handle to service
			SC_STATUS_PROCESS_INFO,         // information level
			(LPBYTE) &ssStatus,             // address of structure
			sizeof(SERVICE_STATUS_PROCESS), // size of structure
			&dwBytesNeeded ) ) {
		CloseServiceHandle(schService);
		CloseServiceHandle(schSCManager);
		return FALSE;
	}

	CloseServiceHandle(schService);
	CloseServiceHandle(schSCManager);

	return ssStatus.dwCurrentState == SERVICE_RUNNING;
}
