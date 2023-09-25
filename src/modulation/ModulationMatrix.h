//
// Created by August Pemberton on 12/09/2022.
//

#pragma once
#include <gin_plugin/gin_plugin.h>
#include "../processor/parameter/Parameter.h"

namespace imagiro {
    class Modulator;
//==============================================================================
    struct ModSrcId {
        ModSrcId() = default;

        explicit ModSrcId(int id_) : id(id_) {}

        ModSrcId(const ModSrcId &other) { id = other.id; }

        ModSrcId &operator=(const ModSrcId &other) {
            id = other.id;
            return *this;
        }

        bool operator==(const ModSrcId &other) const { return other.id == id; }

        bool isValid() const { return id >= 0; }

        int id = -1;
    };

//==============================================================================
    struct ModDstId {
        ModDstId() = default;

        explicit ModDstId(int id_) : id(id_) {}

        ModDstId(const ModDstId &other) { id = other.id; }

        ModDstId &operator=(const ModDstId &other) {
            id = other.id;
            return *this;
        }

        bool operator==(const ModDstId &other) const { return other.id == id; }

        bool isValid() const { return id >= 0; }

        int id = -1;
    };

    class ModulationMatrix {
    public:
        ModulationMatrix() = default;

        struct SourceInfo {
            juce::String id;
            juce::String name;
            ModSrcId index = {};
            float value = 0.0f;
            Modulator* mod {nullptr};
        };


        //==============================================================================
        void loadFromTree(const juce::ValueTree &vt);
        juce::ValueTree getTree();

        //==============================================================================
        float getValue (Parameter* p, bool useConversion = true);
        float getModOnly (Parameter* p);
        void setMonoValue (const ModSrcId& id, float value);

        void finishBlock (int numSamples) {
            for (auto && smoother : smoothers) {
                smoother.process (numSamples);
            }
        }

        ModSrcId addModSource (const juce::String& id, const juce::String& name);
        void addModSource (Modulator& mod, const juce::String& id, const juce::String& name);
        void addParameter (Parameter* p);

        void setSampleRate (double sampleRate);


        //==============================================================================
        int getNumModSources()                      { return sources.size();            }
        juce::String getModSrcName (ModSrcId src)   { return sources[src.id].name;      }
        float getModSrcValue (ModSrcId src)         { return sources[src.id].value;   }
        SourceInfo getModSrc (ModSrcId src)         { return sources[src.id];   }
        float getModValue(ModSrcId source, ModDstId dest);

        juce::Array<ModSrcId> getModSources (Parameter* param);

        bool isModulated (ModDstId param);

        ModSrcId getSourceAtIndex(ModDstId param, int index);
        bool getModBipolar (ModSrcId src, ModDstId param);
        float getModDepth (ModSrcId src, ModDstId param);
        void setModDepth (ModSrcId src, ModDstId param, float depth, bool bipolar);
        void setModDepth (ModSrcId src, ModDstId param, float depth);
        void addModAtIndex (ModSrcId src, ModDstId param, int index, float depth, bool bipolar = false);
        void addModAtNextIndex (ModSrcId src, ModDstId param, float depth, bool bipolar = false, int max = 2);
        void setModBipolar (ModSrcId src, ModDstId param, bool bipolar);
        void clearModDepth (ModSrcId src, ModDstId param);
        bool modExists (ModSrcId src, ModDstId param);

        class Listener
        {
        public:
            virtual ~Listener() = default;
            virtual void modMatrixChanged()             {}
        };

        void addListener (Listener* l)      { listeners.add (l);            }
        void removeListener (Listener* l)   { listeners.remove (l);         }

    private:

        struct Source {
            ModSrcId id = {};
            float depth = 0.0f;
            bool bipolar = false;
        };

        struct ParamInfo {
            Parameter *parameter;
            juce::Array<Source> sources;
        };


        juce::Array<SourceInfo> sources;
        juce::Array<ParamInfo> parameters;
        juce::Array<gin::ValueSmoother<float>> smoothers;

        double sampleRate = 44100;

        juce::ListenerList<Listener> listeners;


    };


}