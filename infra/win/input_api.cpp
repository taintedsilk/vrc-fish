// input_api.cpp  --  Window activation, mouse/keyboard input, and OSC helpers
// Extracted from vrc-fish.cpp so other translation units can share them.

// Winsock headers must come before windows.h to avoid winsock.h conflict
#include <winsock2.h>
#include <ws2tcpip.h>

#include "input_api.h"

#include "../../runtime/params.h"
#include "../../gui/gui_config_def.h"
#include "../../gui/gui_logger.h"

#include <cstring>
#include <vector>

// --- Window activation (foreground mode only) ---
void activateGameWindowInner(bool forceCursorCenter) {
	if (config.background_input) return;
	HWND hwnd = params.hwnd;
	if (!hwnd) return;
	if (IsIconic(hwnd)) {
		ShowWindow(hwnd, SW_RESTORE);
	}
	SetForegroundWindow(hwnd);
	BringWindowToTop(hwnd);
	if (config.vr_debug) {
		HWND fg = GetForegroundWindow();
		if (fg != hwnd) {
			static unsigned long long lastWarnMs = 0;
			unsigned long long t = GetTickCount64();
			if (t - lastWarnMs > 2000) {
				LOG_WARN("[vrchat_fish] foreground hwnd mismatch (fg=%p vrchat=%p)", (void*)fg, (void*)hwnd);
				lastWarnMs = t;
			}
		}
	}
	RECT r = params.rect;
	if (r.right > r.left && r.bottom > r.top) {
		POINT p{};
		if (GetCursorPos(&p)) {
			if (forceCursorCenter || p.x < r.left || p.x >= r.right || p.y < r.top || p.y >= r.bottom) {
				SetCursorPos((r.left + r.right) / 2, (r.top + r.bottom) / 2);
			}
		}
	}
}

void activateGameWindow() {
	activateGameWindowInner(false);
}

// --- Helper: get client center as LPARAM for PostMessage ---
LPARAM getClientCenterLParam() {
	HWND hwnd = params.hwnd;
	if (!hwnd) return MAKELPARAM(0, 0);
	RECT cr;
	GetClientRect(hwnd, &cr);
	return MAKELPARAM((cr.right - cr.left) / 2, (cr.bottom - cr.top) / 2);
}

// --- Mouse / keyboard input ---
void sendMouseLeftRaw(DWORD flags, const char* phaseTag) {
	INPUT input{};
	input.type = INPUT_MOUSE;
	input.mi.dwFlags = flags;
	input.mi.time = NULL;
	if (SendInput(1, &input, sizeof(INPUT)) != 1 && config.vr_debug) {
		LOG_ERROR("[vrchat_fish] SendInput %s failed err=%lu", phaseTag, GetLastError());
	}
}

void mouseLeftDown() {
	if (config.background_input) {
		HWND hwnd = params.hwnd;
		if (hwnd) PostMessage(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, getClientCenterLParam());
		return;
	}
	activateGameWindow();
	sendMouseLeftRaw(MOUSEEVENTF_LEFTDOWN, "leftdown");
}

void mouseLeftUp() {
	if (config.background_input) {
		HWND hwnd = params.hwnd;
		if (hwnd) PostMessage(hwnd, WM_LBUTTONUP, 0, getClientCenterLParam());
		return;
	}
	activateGameWindow();
	sendMouseLeftRaw(MOUSEEVENTF_LEFTUP, "leftup");
}

void mouseLeftClickCentered(int delayMs) {
	if (config.background_input) {
		HWND hwnd = params.hwnd;
		if (!hwnd) return;
		LPARAM lp = getClientCenterLParam();
		PostMessage(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, lp);
		Sleep(delayMs);
		PostMessage(hwnd, WM_LBUTTONUP, 0, lp);
		return;
	}
	activateGameWindowInner(true);
	sendMouseLeftRaw(MOUSEEVENTF_LEFTDOWN, "leftdown");
	Sleep(delayMs);
	sendMouseLeftRaw(MOUSEEVENTF_LEFTUP, "leftup");
}

void mouseMoveRelative(int dx, int dy, const char* phaseTag) {
	if (dx == 0 && dy == 0) return;
	if (config.background_input) {
		HWND hwnd = params.hwnd;
		if (!hwnd) return;
		RECT cr;
		GetClientRect(hwnd, &cr);
		int cx = (cr.right - cr.left) / 2 + dx;
		int cy = (cr.bottom - cr.top) / 2 + dy;
		PostMessage(hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(cx, cy));
		return;
	}
	activateGameWindow();
	INPUT input{};
	input.type = INPUT_MOUSE;
	input.mi.dwFlags = MOUSEEVENTF_MOVE;
	input.mi.dx = dx;
	input.mi.dy = dy;
	input.mi.time = NULL;
	if (SendInput(1, &input, sizeof(INPUT)) != 1 && config.vr_debug) {
		LOG_ERROR("[vrchat_fish] SendInput %s failed err=%lu", phaseTag, GetLastError());
	}
}

// -- OSC UDP sender (VRChat listens on 127.0.0.1:9000) --

bool sendOscInt(const char* path, int value) {
	// Build minimal OSC message: path (4-byte aligned) + ",i\0\0" + int32 big-endian
	size_t pathLen = strlen(path) + 1; // include null
	size_t pathPad = (4 - (pathLen % 4)) % 4;
	size_t totalPathLen = pathLen + pathPad;
	const char typeTag[] = ",i\0\0";
	size_t msgLen = totalPathLen + 4 + 4; // path + type tag + int32

	std::vector<char> buf(msgLen, 0);
	memcpy(buf.data(), path, strlen(path));
	memcpy(buf.data() + totalPathLen, typeTag, 4);
	// Big-endian int32
	uint32_t be = htonl((uint32_t)value);
	memcpy(buf.data() + totalPathLen + 4, &be, 4);

	SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock == INVALID_SOCKET) return false;

	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(9000);
	inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

	int sent = sendto(sock, buf.data(), (int)msgLen, 0, (sockaddr*)&addr, sizeof(addr));
	closesocket(sock);
	return sent > 0;
}

void shakeHeadOSC() {
	int durationMs = config.osc_shake_duration_ms;
	if (durationMs <= 0) durationMs = 20;

	sendOscInt("/input/LookRight", 1);
	Sleep(durationMs);
	sendOscInt("/input/LookRight", 0);
	Sleep(50);

	sendOscInt("/input/LookLeft", 1);
	Sleep(durationMs);
	sendOscInt("/input/LookLeft", 0);
	Sleep(50);
}

void keyTapVk(WORD vk, int delayMs) {
	if (config.background_input) {
		HWND hwnd = params.hwnd;
		if (!hwnd) return;
		UINT scanCode = MapVirtualKey(vk, MAPVK_VK_TO_VSC);
		LPARAM downLParam = (LPARAM)(1 | (scanCode << 16));
		LPARAM upLParam   = (LPARAM)(1 | (scanCode << 16) | (1 << 30) | (1 << 31));
		PostMessage(hwnd, WM_KEYDOWN, vk, downLParam);
		Sleep(delayMs);
		PostMessage(hwnd, WM_KEYUP, vk, upLParam);
		return;
	}
	activateGameWindow();
	INPUT input{};
	input.type = INPUT_KEYBOARD;
	input.ki.wVk = vk;
	input.ki.wScan = (WORD)MapVirtualKey(vk, MAPVK_VK_TO_VSC);
	input.ki.dwFlags = 0;
	if (SendInput(1, &input, sizeof(INPUT)) != 1 && config.vr_debug) {
		LOG_ERROR("[vrchat_fish] SendInput keydown failed err=%lu", GetLastError());
	}
	Sleep(delayMs);
	input.ki.dwFlags = KEYEVENTF_KEYUP;
	if (SendInput(1, &input, sizeof(INPUT)) != 1 && config.vr_debug) {
		LOG_ERROR("[vrchat_fish] SendInput keyup failed err=%lu", GetLastError());
	}
}
