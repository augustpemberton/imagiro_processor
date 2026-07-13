#pragma once

#include <cstdint>

namespace imagiro {

// JUCE-free, toolkit-free abstraction of a plugin editor window. ClapProcessor
// owns one of these (created by a plugin hook) and routes the clap_plugin_gui
// vtable into it. A concrete plugin implements this over its own UI toolkit
// (novo: a visage ApplicationWindow hosting an ivl ViewRoot).
//
// All calls happen on the host's main/UI thread.
class IPluginView {
public:
    virtual ~IPluginView() = default;

    // Attach the view to a foreign parent window and make it live. parentHandle
    // is the platform native handle passed by the host: X11 Window on Linux,
    // NSView* on macOS, HWND on Windows.
    virtual void embed(void* parentHandle, uint32_t width, uint32_t height) = 0;

    virtual void setScale(double scale) = 0;
    virtual void getSize(uint32_t& width, uint32_t& height) = 0;
    virtual bool canResize() const = 0;
    virtual void resize(uint32_t width, uint32_t height) = 0;

    virtual void show() = 0;
    virtual void hide() = 0;
    virtual void destroy() = 0;

    // Linux only: the file descriptor of the view's plugin-window connection.
    // ClapProcessor registers it with the host's posix-fd support so the host
    // pumps the render loop. Returns < 0 when not applicable (macOS/Windows,
    // where the OS run loop drives repaint itself).
    virtual int posixFd() const { return -1; }
    virtual void onPosixFd() {}

    // Per-frame logical tick (host-timer driven): advances animations and syncs
    // audio-thread snapshots into the UI. Separate from toolkit repaint.
    virtual void tick() {}
};

} // namespace imagiro
