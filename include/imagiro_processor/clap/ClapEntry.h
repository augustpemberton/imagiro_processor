#pragma once

#include <clap/clap.h>
#include <cstring>

// CLAP factory + entry glue, generic to every imagiro plugin. A plugin emits it
// with two macros and provides nothing else:
//
//   // in the plugin's ClapProcessor-subclass TU (compiled into the static core)
//   IMAGIRO_CLAP_PLUGIN(myns::MyClapPlugin)
//
//   // in the tiny per-format entry TU (recompiled into the .clap and each
//   // clap-wrapper format, so only this shim carries the exported symbol)
//   IMAGIRO_CLAP_ENTRY()
//
// The single CLAP_EXPORT clap_entry lives only in the IMAGIRO_CLAP_ENTRY() TU;
// IMAGIRO_CLAP_PLUGIN emits the factory plus the three externally-linked entry
// hooks the export shim points at. One plugin class per binary.

extern "C" {
bool imagiroClapEntryInit(const char* pluginPath);
void imagiroClapEntryDeinit();
const void* imagiroClapEntryGetFactory(const char* factoryId);
}

// PluginClass must expose `static const clap_plugin_descriptor* descriptor()`,
// a `PluginClass(const clap_host*)` constructor, and `clapPlugin()` (from the
// clap-helpers Plugin base ClapProcessor derives from).
#define IMAGIRO_CLAP_PLUGIN(PluginClass)                                        \
    namespace {                                                                 \
    uint32_t imagiroClapFactoryGetPluginCount(const clap_plugin_factory*) {     \
        return 1;                                                               \
    }                                                                           \
    const clap_plugin_descriptor* imagiroClapFactoryGetDescriptor(             \
        const clap_plugin_factory*, uint32_t index) {                          \
        return index == 0 ? PluginClass::descriptor() : nullptr;               \
    }                                                                           \
    const clap_plugin* imagiroClapFactoryCreatePlugin(                         \
        const clap_plugin_factory*, const clap_host* host, const char* id) {   \
        if (!host || !id) return nullptr;                                       \
        if (std::strcmp(id, PluginClass::descriptor()->id) != 0)               \
            return nullptr;                                                     \
        auto* plugin = new PluginClass(host);                                   \
        return plugin->clapPlugin();                                            \
    }                                                                           \
    const clap_plugin_factory kImagiroClapFactory = {                          \
        imagiroClapFactoryGetPluginCount,                                       \
        imagiroClapFactoryGetDescriptor,                                        \
        imagiroClapFactoryCreatePlugin};                                        \
    }                                                                           \
    extern "C" bool imagiroClapEntryInit(const char*) { return true; }         \
    extern "C" void imagiroClapEntryDeinit() {}                                \
    extern "C" const void* imagiroClapEntryGetFactory(const char* factoryId) { \
        if (std::strcmp(factoryId, CLAP_PLUGIN_FACTORY_ID) == 0)               \
            return &kImagiroClapFactory;                                        \
        return nullptr;                                                         \
    }

#define IMAGIRO_CLAP_ENTRY()                                                    \
    extern "C" {                                                                \
    _Pragma("GCC diagnostic push")                                             \
    _Pragma("GCC diagnostic ignored \"-Wattributes\"")                        \
    const CLAP_EXPORT clap_plugin_entry clap_entry = {                        \
        CLAP_VERSION, imagiroClapEntryInit, imagiroClapEntryDeinit,           \
        imagiroClapEntryGetFactory};                                          \
    _Pragma("GCC diagnostic pop")                                             \
    }
