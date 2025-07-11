//
// Created by August Pemberton on 07/07/2024.
//

#pragma once

struct ValueDataEntry {
    choc::value::Value value;
    bool saveInPreset {false};

    choc::value::Value getState() const {
        auto val = choc::value::createObject("UIDataValue");
        val.setMember("value", value);
        return val;
    }

    static ValueDataEntry fromState(const choc::value::ValueView &v, const bool isPreset) {
        ValueDataEntry val;
        if (v.isObject() && v.hasObjectMember("value")) {
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
        virtual void OnValueDataUpdated(ValueData& s, const std::string& key, const choc::value::ValueView& newValue) {}
    };
    void addListener(Listener* l) { listeners.add(l); }
    void removeListener(Listener* l) { listeners.remove(l); }

    ValueData() = default;
    ValueData (const ValueData& other) {
        this->values = other.values;
    }

    void set(std::string key, choc::value::Value value, const bool saveInPreset) {
        if (values.contains(key)) values[key] = {value, saveInPreset};
        values.insert({key, {value, saveInPreset}});

        listeners.call(&Listener::OnValueDataUpdated, *this, key, value);
    }

    void set(const std::string& key, const choc::value::ValueView& valueView, const bool saveInPreset) {
        const auto v = choc::value::Value(valueView);
        set(key, v, saveInPreset);
    }

    void set(const std::string& key, const std::string& val, const bool saveInPreset) {
        const auto v = choc::value::Value(val);
        set(key, v, saveInPreset);
    }

    std::optional<choc::value::Value> get(const std::string &key) {
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
