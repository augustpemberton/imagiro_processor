// StateRegistry.h
#pragma once

#include <string>
#include <memory>
#include <nlohmann/json.hpp>
#include <immer/map.hpp>
#include <immer/vector.hpp>

namespace imagiro {

using json = nlohmann::json;

struct Handle {
    uint32_t index{0xFFFF};

    bool operator==(const Handle&) const = default;
    bool operator!=(const Handle&) const = default;

    static Handle invalid() { return Handle{0xFFFF}; }
    bool isValid() const { return index != 0xFFFF; }
};

template<typename T>
class StateRegistry {
public:
    StateRegistry() = default;
    StateRegistry(const StateRegistry&) = default;
    StateRegistry(StateRegistry&&) = default;
    StateRegistry& operator=(const StateRegistry&) = default;
    StateRegistry& operator=(StateRegistry&&) = default;

    // Immutable add - returns new registry and handle via out param
    [[nodiscard]] StateRegistry add(std::string id, T value, Handle& outHandle) const {
        StateRegistry result = *this;
        outHandle = Handle{static_cast<uint32_t>(result.ids_.size())};
        result.idToHandle_ = result.idToHandle_.set(id, outHandle);
        result.ids_ = result.ids_.push_back(std::move(id));
        result.values_ = result.values_.set(outHandle.index, std::move(value));
        return result;
    }

    // Mutable version for setup phase - modifies in place, returns handle
    Handle add(std::string id, T value) {
        Handle handle{static_cast<uint32_t>(ids_.size())};
        idToHandle_ = idToHandle_.set(id, handle);
        ids_ = ids_.push_back(std::move(id));
        values_ = values_.set(handle.index, std::move(value));
        return handle;
    }

    Handle handle(const std::string& id) const {
        auto* h = idToHandle_.find(id);
        return h ? *h : Handle::invalid();
    }

    bool has(const std::string& id) const {
        return idToHandle_.find(id) != nullptr;
    }

    const std::string& id(Handle h) const {
        return ids_[h.index];
    }

    // Immutable set - returns new registry
    [[nodiscard]] StateRegistry set(Handle h, T value) const {
        StateRegistry result = *this;
        result.values_ = result.values_.set(h.index, std::move(value));
        return result;
    }

    const T& get(Handle h) const {
        return *values_.find(h.index);
    }

    const T* tryGet(Handle h) const {
        return values_.find(h.index);
    }

    size_t size() const { return ids_.size(); }

    template<typename Func>
    void forEach(Func&& fn) const {
        for (size_t i = 0; i < ids_.size(); i++) {
            Handle h{static_cast<uint32_t>(i)};
            fn(h, ids_[i], *values_.find(h.index));
        }
    }

    // Immutable fromJson - returns new registry
    [[nodiscard]] StateRegistry fromJson(const json& j) const {
        StateRegistry result = *this;
        for (auto& [id, value] : j.items()) {
            if (auto h = result.handle(id); h.isValid()) {
                result.values_ = result.values_.set(h.index, value.get<T>());
            }
        }
        return result;
    }

private:
    immer::vector<std::string> ids_;
    immer::map<std::string, Handle> idToHandle_;
    immer::map<uint32_t, T> values_;

    template<typename U>
    friend void to_json(json& j, const StateRegistry<U>& r);
};

template<typename T>
void to_json(json& j, const StateRegistry<T>& r) {
    j = json::object();
    r.forEach([&](Handle, const std::string& id, const T& value) {
        j[id] = value;
    });
}

// Type-erased registry wrapper for heterogeneous storage
class AnyRegistry {
public:
    AnyRegistry() = default;

    template<typename T>
    explicit AnyRegistry(StateRegistry<T> reg)
        : impl_(std::make_shared<Model<T>>(std::move(reg))) {}

    AnyRegistry(const AnyRegistry&) = default;
    AnyRegistry(AnyRegistry&&) = default;
    AnyRegistry& operator=(const AnyRegistry&) = default;
    AnyRegistry& operator=(AnyRegistry&&) = default;

    template<typename T>
    const StateRegistry<T>& as() const {
        return static_cast<const Model<T>&>(*impl_).registry;
    }

    json toJson() const {
        return impl_ ? impl_->toJson() : json::object();
    }

    explicit operator bool() const { return impl_ != nullptr; }

private:
    struct Concept {
        virtual ~Concept() = default;
        virtual json toJson() const = 0;
    };

    template<typename T>
    struct Model : Concept {
        StateRegistry<T> registry;
        explicit Model(StateRegistry<T> r) : registry(std::move(r)) {}
        json toJson() const override { return registry; }
    };

    std::shared_ptr<const Concept> impl_;
};

inline void to_json(json& j, const AnyRegistry& r) {
    j = r.toJson();
}

} // namespace imagiro