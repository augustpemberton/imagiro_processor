//
// Created by August Pemberton on 30/05/2025.
//

#pragma once

#include "EnvelopeFollowerSource.h"


class EnvelopeFollowerSources {
public:
    EnvelopeFollowerSource *getSource(const std::string &id) {
        return sourcesMap.contains(id) ? sourcesMap[id] : nullptr;
    }

    EnvelopeFollowerSource& addSource(const std::string& uid, const std::string& name) {
        if (sourcesMap.contains(uid)) return *sourcesMap[uid];

        sources.push_back(std::make_unique<EnvelopeFollowerSource>(name));
        sourcesMap.insert({uid, sources.back().get()});

        return *sources.back();
    }

    void prepare(const int maxSamplePerBlock) {
        for (auto& source : sources) {
            source->prepare(maxSamplePerBlock);
        }
    }

    auto& getSources() { return sources; }

private:
    std::unordered_map<std::string, EnvelopeFollowerSource*> sourcesMap;
    std::vector<std::unique_ptr<EnvelopeFollowerSource>> sources;
};
