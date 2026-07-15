#pragma once

#include <imagiro_processor/parameter/ParamController.h>
#include <imagiro_processor/parameter/HostParamBridge.h>

#include <string>
#include <vector>

namespace imagiro {

// Generic editor-host services every imagiro plugin editor needs, independent of
// any one plugin's DSP: parameter access/binding, the preset browser, standalone
// audio-settings, and a CPU-load readout. A plugin's view-host interface derives
// from this and adds its plugin-specific surface; UI that only needs these
// generic services (e.g. a params panel, the toolbar) depends on IEditorHost.
class IEditorHost {
public:
    virtual ~IEditorHost() = default;

    // ---- parameters / binding ----
    virtual ParamController& params() = 0;
    virtual HostParamBridge* paramBridge() = 0;

    // Smoothed DSP load in [0,1] (processing time / block duration).
    virtual float getCpuLoad() const { return 0.f; }

    // ---- preset browser (toolbar) ----
    virtual std::vector<std::string> listPresetNames() { return {}; }
    virtual std::string currentPresetName() { return "Init"; }
    virtual void loadPresetIndex(int index) {}
    virtual void savePresetInteractive() {}
    virtual bool hasPresetBrowser() const { return false; }

    // ---- standalone audio/MIDI settings (only meaningful for the standalone) ----
    virtual void openAudioSettings() {}
    virtual bool hasAudioSettings() const { return false; }
};

} // namespace imagiro
