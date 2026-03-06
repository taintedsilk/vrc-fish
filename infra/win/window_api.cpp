#include "window_api.h"

namespace {

std::wstring toWStringSimple(const std::string& s) {
	return std::wstring(s.begin(), s.end());
}

struct FindWindowCtx {
	std::wstring className;
	std::wstring titleContains;
	HWND hwnd = nullptr;
};

BOOL CALLBACK enumWindowsFindProc(HWND hwnd, LPARAM lParam) {
	auto* ctx = reinterpret_cast<FindWindowCtx*>(lParam);
	if (!IsWindowVisible(hwnd)) {
		return TRUE;
	}
	if (!ctx->className.empty()) {
		wchar_t cls[256]{};
		GetClassNameW(hwnd, cls, 256);
		if (ctx->className != cls) {
			return TRUE;
		}
	}
	if (!ctx->titleContains.empty()) {
		wchar_t title[512]{};
		GetWindowTextW(hwnd, title, 512);
		std::wstring t = title;
		if (t.find(ctx->titleContains) == std::wstring::npos) {
			return TRUE;
		}
	}
	ctx->hwnd = hwnd;
	return FALSE;
}

}  // namespace

namespace infra {
namespace win {

HWND findWindowByClassAndTitleContains(const std::string& windowClass, const std::string& titleContains) {
	FindWindowCtx ctx{};
	ctx.className = toWStringSimple(windowClass);
	ctx.titleContains = toWStringSimple(titleContains);
	EnumWindows(enumWindowsFindProc, reinterpret_cast<LPARAM>(&ctx));
	return ctx.hwnd;
}

}  // namespace win
}  // namespace infra
