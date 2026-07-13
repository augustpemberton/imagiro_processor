#pragma once

#include <imagiro_util/fs.h>
#include <cstdlib>
#include <string>

#if defined(_WIN32)
  #include <shlobj.h>
#endif

namespace imagiro {

// JUCE-free equivalent of Resources::getDataFolder(): the per-user data folder
// for <company>/<plugin>, created if missing. Mirrors the platform locations
// JUCE uses (userApplicationDataDirectory + "Application Support" on macOS).
//   Windows : %APPDATA%\<company>\<plugin>
//   macOS   : ~/Library/Application Support/<company>/<plugin>
//   Linux   : $XDG_CONFIG_HOME/<company>/<plugin>  (fallback ~/.config)
inline fs::path getUserDataFolder(const std::string& company,
                                  const std::string& plugin) {
    fs::path base;

#if defined(_WIN32)
    if (const char* appdata = std::getenv("APPDATA"))
        base = fs::path(appdata);
    else
        base = fs::path(".");
#elif defined(__APPLE__)
    const char* home = std::getenv("HOME");
    base = fs::path(home ? home : ".") / "Library" / "Application Support";
#else
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg) {
        base = fs::path(xdg);
    } else if (const char* home = std::getenv("HOME")) {
        base = fs::path(home) / ".config";
    } else {
        base = fs::path(".");
    }
#endif

    fs::path folder = base / company / plugin;
    std::error_code ec;
    fs::create_directories(folder, ec);
    return folder;
}

} // namespace imagiro
