#pragma once

#include "IPluginView.h"

#include <ivl/ViewRoot.h>
#include <visage/app.h>

#include <cstdint>
#include <memory>

namespace imagiro {

// Generic CLAP editor view: a visage ApplicationWindow embedded into the host's
// parent window, hosting any ivl::ViewRoot. Header-only and toolkit-facing, so
// it is compiled only by a plugin's GUI translation units (which already have
// ivl + visage on their include path) — no link edge from imagiro_processor to
// ivl is added. A plugin's createPluginView() constructs one over its ViewRoot.
class IvlPluginView : public IPluginView {
public:
    explicit IvlPluginView(std::unique_ptr<ivl::ViewRoot> view)
        : view_(std::move(view))
    {
        app_.addChild(view_->hostFrame());
        app_.onResize().add([this] { layoutRoot(); });
    }

    ~IvlPluginView() override { app_.removeFromWindow(); }

    void embed(void* parentHandle, uint32_t width, uint32_t height) override {
        setPluginDimensions(width, height);
        app_.show(parentHandle);
        layoutRoot();
    }

    void setScale(double scale) override {
        if (app_.window())
            app_.window()->setDpiScale(static_cast<float>(scale));
    }

    void getSize(uint32_t& width, uint32_t& height) override {
        width = width_;
        height = height_;
    }

    bool canResize() const override { return true; }

    void resize(uint32_t width, uint32_t height) override {
        setPluginDimensions(width, height);
        layoutRoot();
    }

    void show() override {
        if (app_.window()) app_.window()->show();
    }

    void hide() override {
        if (app_.window()) app_.window()->hide();
    }

    void destroy() override { app_.close(); }

    int posixFd() const override {
        return app_.window() ? app_.window()->posixFd() : -1;
    }

    void onPosixFd() override {
        if (app_.window()) app_.window()->processPluginFdEvents();
    }

    void tick() override {
        // Host-timer fallback: only pump when the vsync render loop isn't already
        // driving tickFrame() (hidden or unpainted window), to avoid double ticks.
        if (!view_->tickedWithin(100.0))
            view_->tickFrame();
    }

private:
    void setPluginDimensions(uint32_t width, uint32_t height) {
        width_ = width;
        height_ = height;
#if defined(__APPLE__)
        app_.setWindowDimensions(static_cast<int>(width), static_cast<int>(height));
#else
        app_.setNativeWindowDimensions(static_cast<int>(width), static_cast<int>(height));
#endif
    }

    void layoutRoot() {
        view_->hostFrame().setBounds(0, 0, app_.width(), app_.height());
    }

    visage::ApplicationWindow app_;
    std::unique_ptr<ivl::ViewRoot> view_;
    uint32_t width_ = 1000;
    uint32_t height_ = 600;
};

} // namespace imagiro
