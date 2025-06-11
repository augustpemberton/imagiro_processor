//
// Created by August Pemberton on 30/05/2025.
//

#pragma once

#include <MacTypes.h>
#include <unordered_map>

struct NamedMultichannelValue {
    const MultichannelValue<MAX_MOD_VOICES>& value;
    std::string name;
};

class MultichannelValueSources {
public:
    explicit MultichannelValueSources(const FixedHashSet<size_t, MAX_MOD_VOICES> &activeVoices)
        : activeVoices(activeVoices) {
    }

    NamedMultichannelValue *getSource(const std::string &id) {
        return sourcesMap.contains(id) ? sourcesMap[id] : nullptr;
    }

    NamedMultichannelValue& addSource(const MultichannelValue<MAX_MOD_VOICES>& v, const std::string& uid, const std::string& name) {
        if (sourcesMap.contains(uid)) return *sourcesMap[uid];

        const NamedMultichannelValue namedValue {v, name};
        sources.push_back(namedValue);
        sourcesMap.insert({uid, &sources.back()});

        return sources.back();
    }

    const auto& getSources() { return sources; }
    const auto& getActiveVoices() const { return activeVoices; }

private:
    std::unordered_map<std::string, NamedMultichannelValue*> sourcesMap;
    std::vector<NamedMultichannelValue> sources;

    const FixedHashSet<size_t, MAX_MOD_VOICES>& activeVoices;
};
