//
// Created by August Pemberton on 07/07/2024.
//

#pragma once

struct StringDataValue {
    std::string value;
    bool saveInPreset {false};

    choc::value::Value getState() const {
        auto val = choc::value::createObject("UIDataValue");
        val.setMember("value", value);
        return val;
    }

    static StringDataValue fromState(const choc::value::ValueView &v, const bool isPreset) {
        StringDataValue val;
        if (v.isString()) {
            val.value = v.getWithDefault("");
        } else if (v.isObject() && v.hasObjectMember("value")) {
            val.value = v["value"].getWithDefault("");
            val.saveInPreset = isPreset;
        }
        return val;
    }
};

class StringData {
public:

    struct Listener {
        virtual ~Listener() = default;
        virtual void OnStringDataUpdated(StringData& s, const std::string& key, const std::string& newValue) {}
    };
    void addListener(Listener* l) { listeners.add(l); }
    void removeListener(Listener* l) { listeners.remove(l); }

    StringData() = default;
    StringData (const StringData& other) {
        this->values = other.values;
    }

    void set(std::string key, std::string value, const bool saveInPreset) {
        if (values.contains(key)) values[key] = {value, saveInPreset};
        values.insert({key, {value, saveInPreset}});

        listeners.call(&Listener::OnStringDataUpdated, *this, key, value);
    }

    std::optional<std::string> get(const std::string &key) {
        if (!values.contains(key)) return {};
        return values[key].value;
    }

    choc::value::Value getState(const bool onlyPresetData) {
        auto vals = choc::value::createObject("UIData");
        for (auto& [key, val] : values) {
            if (onlyPresetData && !val.saveInPreset) continue;
            vals.addMember(key, val.getState());
        }

        return vals;
    }

    void loadState(const choc::value::ValueView& v, const bool isPreset) {
        for (int i=0; i<v.size(); i++) {
            auto [name, value] = v.getObjectMemberAt(i);
            auto data = StringDataValue::fromState(value, isPreset);
            values.insert({name, data});
            listeners.call(&Listener::OnStringDataUpdated, *this, name, data.value);
        }
    }

    std::map<std::string, StringDataValue>& getValues() { return values; }

private:
    std::map<std::string, StringDataValue> values;
    juce::ListenerList<Listener> listeners;
};
