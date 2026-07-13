#pragma once

#include <imagiro_util/fs.h>
#include <imagiro_processor/pfd/portable-file-dialogs.h>

#include <optional>
#include <string>

namespace imagiro {

// JUCE-free native file dialogs (via portable-file-dialogs). Blocking: they
// return once the user confirms or cancels. Intended for calls off the audio
// thread (UI click handlers).

// Native "open audio file" chooser. Returns the chosen path, or nullopt if the
// user cancelled or native dialogs are unavailable on this platform.
inline std::optional<fs::path> openAudioFileDialog(const fs::path& defaultDir = {}) {
    try {
        auto selection = pfd::open_file(
            "Choose audio file",
            defaultDir.string(),
            {"Audio Files", "*.wav *.aiff *.aif *.flac *.mp3 *.ogg *.m4a",
             "All Files", "*"},
            pfd::opt::none).result();

        if (selection.empty()) return std::nullopt;
        return fs::path(selection.front());
    } catch (...) {
        return std::nullopt;
    }
}

// Native "save file" chooser (used for presets). Returns the chosen path, or
// nullopt if cancelled/unavailable. Appends the given extension if missing.
inline std::optional<fs::path> saveFileDialog(const std::string& title,
                                              const fs::path& defaultPath,
                                              const std::string& filterLabel,
                                              const std::string& filterPattern,
                                              const std::string& extension = {}) {
    try {
        auto chosen = pfd::save_file(
            title, defaultPath.string(),
            {filterLabel, filterPattern, "All Files", "*"}).result();

        if (chosen.empty()) return std::nullopt;

        fs::path path(chosen);
        if (!extension.empty() && path.extension() != extension)
            path += extension;
        return path;
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace imagiro
