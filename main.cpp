#include "main.hpp"

#include "windows_lean.h"

#include <string>
#include <sstream>

#include "fose/PluginAPI.h"
#include "fose_common/fose_version.h"

#include "utils.hpp"

static const int TryFindWindowIntervalMs = 1000;
static const char* ModName = "fo3_clipcursor";
static const char* GameClassName = "Fallout3";

static HWND GameWindow = NULL;
static WNDPROC GameWndProc = NULL;

BOOL WINAPI DllMain(HANDLE, DWORD, LPVOID)
{
	return true;
}

extern "C" {

EXPORT bool FOSEPlugin_Query(
	const FOSEInterface* fose,
	PluginInfo* info
) {
	info->infoVersion = PluginInfo::kInfoVersion;
	info->name = ModName;
	info->version = 2;

	std::string log_filename(ModName);
	log_filename.append(".log");
	gLog.Open(log_filename.c_str());

	if (!version_check(fose))
		return false;

	return true;
}

EXPORT bool FOSEPlugin_Load(
	const FOSEInterface* fose
) {
	if (
		SetTimer(
			NULL,
			0,
			TryFindWindowIntervalMs,
			find_game_window_timer) == 0
	) {
		fatal_error("Failed to set timer");
		return false;
	}

	return true;
}

}

bool version_check(const FOSEInterface* fose) {
	if (fose->isEditor)
		return false;

	if (fose->foseVersion < FOSE_VERSION_INTEGER) {
		std::stringstream ss;
		ss
			<< "FOSE version is too old ("
			<< fose->foseVersion
			<< "), expected at least "
			<< FOSE_VERSION_INTEGER;

		std::string str = ss.str();
		fatal_error(str.c_str(), false);
		return false;
	}

	return true;
}

void CALLBACK find_game_window_timer(
	HWND window,
	UINT wm_timer,
	UINT_PTR timer_id,
	DWORD tick_count
) {
	GameWindow = FindWindowA(GameClassName, NULL);

	if (GameWindow == NULL)
		return;

	KillTimer(window, timer_id);
	inject_wndproc();
}

void inject_wndproc() {
	GameWndProc = reinterpret_cast<WNDPROC>(
		GetWindowLongPtrA(GameWindow, GWLP_WNDPROC));

	if (GameWndProc == NULL) {
		fatal_error("Failed to get game wndproc");
		return;
	}

	if (
		SetWindowLongPtrA(
			GameWindow,
			GWLP_WNDPROC,
			reinterpret_cast<LONG>(&injected_wndproc)) == 0
	) {
		fatal_error("Failed to set injected wndproc");
		return;
	}

	setup_clipcursor(GameWindow);

	_MESSAGE("fo3_clipcursor setup correctly");
}

LRESULT CALLBACK injected_wndproc(
	HWND window,
	UINT message,
	WPARAM w_param,
	LPARAM l_param
) {
	switch (message) {
		case WM_ACTIVATE: {
			if (w_param == WA_INACTIVE)
				unset_clipcursor();
			else
				setup_clipcursor(window);

			break;
		}
		case WM_MOUSEMOVE: {
			setup_clipcursor(window);
			break;
		}
		case WM_SETFOCUS: {
			setup_clipcursor(window);
			break;
		}
		case WM_KILLFOCUS: {
			unset_clipcursor();
			break;
		}
	}

	return GameWndProc(window, message, w_param, l_param);
}

void setup_clipcursor(HWND window) {
	RECT rect;
	if (!GetWindowRect(window, &rect)) {
		fatal_error("Failed to get window rect");
		return;
	}

	if (!ClipCursor(&rect))
		fatal_error("Failed to setup clip cursor");
}

void unset_clipcursor() {
	if (!ClipCursor(NULL))
		fatal_error("Failed to unset clip cursor");
}
