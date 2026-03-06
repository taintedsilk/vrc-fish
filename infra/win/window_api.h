#pragma once

#include <string>

#include <windows.h>

namespace infra {
namespace win {

HWND findWindowByClassAndTitleContains(const std::string& windowClass, const std::string& titleContains);

}  // namespace win
}  // namespace infra
