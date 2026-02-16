#pragma once

#include <utility>
#include <nlohmann/json.hpp>
#include "ValueData.h"
#include <imagiro_util/readerwriterqueue/readerwriterqueue.h>

using json = nlohmann::json;

template<typename T>
struct Serializer {
    static json serialize(const T& value) { return json(value); }
    static T load(const json& j) { return j.get<T>(); }
};


// Bidirectional sync wrapper
template<typename T>
class SerializableValue : public ValueData::Listener {
private:
    T value;
    T realtimeValue;

    ValueData& valueData;
    std::string key;
    bool saveInPreset;

    // Prevent infinite recursion during sync
    std::atomic<bool> syncing{false};

    // Optional change callback
    std::function<void(const T&)> onChanged;

    moodycamel::ReaderWriterQueue<T> realtimeUpdateQueue {4};

public:
    SerializableValue(ValueData& valueData, std::string key,
                     const T& initialValue = T{}, const bool saveInPreset = false)
        : value(initialValue), valueData(valueData), key(std::move(key)), saveInPreset(saveInPreset) {

        valueData.addListener(this);

        // Load existing value from ValueData if it exists
        auto existing = valueData.get(key);
        if (existing.has_value()) {
            value = Serializer<T>::load(existing.value());
            realtimeValue = value;
        } else {
            // Store initial value
            syncToValueData();
        }
    }

    ~SerializableValue() override {
        valueData.removeListener(this);
    }

    // Access the value
    const T& get() const { return value; }
    T& get() { return value; }

    // Access the real-time value
    const T& getRT() const { return realtimeValue; }
    T& getRT() { return realtimeValue; }
    bool updateRT() {
        bool updated = false;
        while (realtimeUpdateQueue.try_dequeue(realtimeValue)) { updated = true; }
        return updated;
    }

    // Set the value and sync to string data
    void set(const T& newValue) {
        value = newValue;
        realtimeUpdateQueue.try_enqueue(newValue);
        syncToValueData();

        if (onChanged) {
            onChanged(value);
        }
    }

    // Operator overloads for convenience
    SerializableValue& operator=(const T& newValue) {
        set(newValue);
        return *this;
    }

    explicit operator const T&() const { return value; }

    // Direct access to members (for Point.x, Point.y etc.)
    T* operator->() { return &value; }
    const T* operator->() const { return &value; }

    T& operator*() { return value; }
    const T& operator*() const { return value; }

    // Set change callback
    void setOnChanged(std::function<void(const T&)> callback) {
        onChanged = std::move(callback);
    }

    // Manual sync methods
    void syncToValueData() {
        if (syncing.load()) return;

        syncing = true;
        auto serialized = Serializer<T>::serialize(value);
        valueData.set(key, serialized, saveInPreset);
        syncing = false;
    }

    void syncFromValueData() {
        if (syncing.load()) return;

        const auto value = valueData.get(key);
        if (value.has_value()) {
            syncing = true;
            set(Serializer<T>::load(value.value()));
            syncing = false;
        }
    }

    void OnValueDataUpdated(ValueData& s, const std::string& key_, const json& newValue) override {
        if (key_ == key) {
            syncFromValueData();
        }
    }
};
