//
// Created by August Pemberton on 07/07/2024.
//

#pragma once
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct ValueDataEntry {
    json value;
    bool saveInPreset {false};

    json getState() const {
        json val = json::object();
        val["value"] = value;
        return val;
    }

    static ValueDataEntry fromState(const json& v, const bool isPreset) {
        ValueDataEntry val;
        if (v.is_object() && v.contains("value")) {
            val.value = v["value"];
            val.saveInPreset = isPreset;
        }
        return val;
    }
};

class ValueData {
public:
    struct Listener {
        virtual ~Listener() = default;
        virtual void OnValueDataUpdated(ValueData& s, const std::string& key, const json& newValue) {}
    };
    void addListener(Listener* l) { listeners.add(l); }
    void removeListener(Listener* l) { listeners.remove(l); }

    ValueData() = default;
    ValueData (const ValueData& other) {
        this->values = other.values;
    }

    void set(const std::string& key, const json& value, const bool saveInPreset) {
        if (values.contains(key)) values[key] = {value, saveInPreset};
        values.insert({key, {value, saveInPreset}});

        listeners.call(&Listener::OnValueDataUpdated, *this, key, value);
    }

    std::optional<json> get(const std::string &key) {
        if (!values.contains(key)) return {};
        return values[key].value;
    }

    json getState(const bool onlyPresetData) {
        auto vals = json::object();
        for (auto& [key, val] : values) {
            if (onlyPresetData && !val.saveInPreset) continue;
            vals[key] = val.getState();
        }

        return vals;
    }

    void loadState(const json& v, const bool isPreset) {
        for (auto& [name, value] : v.items()) {
            auto data = ValueDataEntry::fromState(value, isPreset);
            if (values.contains(name)) {
                values[name] = data;
            } else {
                values.insert({name, data});
            }
            listeners.call(&Listener::OnValueDataUpdated, *this, name, data.value);
        }
    }

    std::map<std::string, ValueDataEntry>& getValues() { return values; }

private:
    std::map<std::string, ValueDataEntry> values;
    juce::ListenerList<Listener> listeners;
};
