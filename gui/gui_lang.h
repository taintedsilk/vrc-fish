#pragma once
#include <string>
#include <unordered_map>
#include <fstream>
#include <vector>
#include <algorithm>
#include <windows.h>

class Lang {
public:
	static Lang& instance() {
		static Lang s;
		return s;
	}

	// Get translated string (returns key if not found)
	const char* get(const char* key) const {
		auto it = m_strings.find(key);
		if (it != m_strings.end()) return it->second.c_str();
		return key;
	}

	// Load a language file (ini-style: key=value, # comments, UTF-8)
	bool load(const std::string& path) {
		std::ifstream f(path);
		if (!f.is_open()) return false;
		m_strings.clear();
		std::string line;
		while (std::getline(f, line)) {
			// Trim \r
			if (!line.empty() && line.back() == '\r') line.pop_back();
			// Skip empty/comment
			if (line.empty() || line[0] == '#' || line[0] == ';') continue;
			auto eq = line.find('=');
			if (eq == std::string::npos) continue;
			std::string key = line.substr(0, eq);
			std::string val = line.substr(eq + 1);
			// Trim spaces
			while (!key.empty() && key.back() == ' ') key.pop_back();
			while (!val.empty() && val.front() == ' ') val.erase(val.begin());
			if (!key.empty()) m_strings[key] = val;
		}
		return true;
	}

	// Scan lang/ directory and return list of available language names
	std::vector<std::string> listLanguages(const std::string& langDir = "lang") const {
		std::vector<std::string> langs;
		WIN32_FIND_DATAA fd;
		std::string pattern = langDir + "\\*.ini";
		HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
		if (h == INVALID_HANDLE_VALUE) return langs;
		do {
			std::string name = fd.cFileName;
			// Strip .ini extension
			if (name.size() > 4) name = name.substr(0, name.size() - 4);
			langs.push_back(name);
		} while (FindNextFileA(h, &fd));
		FindClose(h);
		std::sort(langs.begin(), langs.end());
		return langs;
	}

	std::string currentLang;

private:
	Lang() = default;
	std::unordered_map<std::string, std::string> m_strings;
};

// Shorthand translation macro
#define T(key) Lang::instance().get(key)
