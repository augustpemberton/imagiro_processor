//
// Created by August Pemberton on 12/09/2022.
//

#include "ModulationMatrix.h"
#include "Modulator.h"

namespace imagiro {
    float ModulationMatrix::getModOnly(Parameter* p) {
        const int paramId = p->getModIndex();

        auto &info = parameters.getReference(paramId);
        auto mods = 0.f;

        for (auto &src: info.sources) {
            mods += getModValue(src.id, ModDstId(p->getModIndex())) * src.depth;
        }

        auto &smoother = smoothers.getReference(paramId);
        smoother.setValue(mods);
        mods = smoother.getCurrentValue();

        return mods;
    }

    float ModulationMatrix::getValue(Parameter *p, bool useConversion) {
        auto mods = getModOnly(p);
        auto v = juce::jlimit(0.f, 1.f, mods + p->getValue());

        v = p->convertFrom0to1(v);

        if (p->getConfig()->conversionFunction && useConversion)
            v = p->getConfig()->conversionFunction(v);

        return v;
    }

    void ModulationMatrix::setMonoValue(const ModSrcId& id, float value) {
        auto& info = sources.getReference (id.id);
        info.value = value;
    }

    ModSrcId ModulationMatrix::addModSource(const juce::String &id, const juce::String &name) {
        SourceInfo si {id, name, ModSrcId(sources.size())};
        sources.add (si);
        return {si.index};
    }

    void ModulationMatrix::addModSource(Modulator& mod, const juce::String &id, const juce::String &name) {
        SourceInfo si {id, name, ModSrcId(sources.size())};
        si.mod = &mod;
        sources.add (si);
        mod.id = si.index;
    }

    void ModulationMatrix::addParameter(Parameter *p) {
        p->setModMatrix (this);
        p->setModIndex (parameters.size());

        ParamInfo pi;
        pi.parameter = p;
        parameters.add (pi);

        smoothers.resize (parameters.size());
    }

    void ModulationMatrix::setSampleRate(double sr) {
        sampleRate = sr;

        for (auto& s : smoothers) {
            s.setSampleRate (sr);
            s.setTime (0.02);
        }
    }

    juce::Array<ModSrcId> ModulationMatrix::getModSources(Parameter* param) {
        juce::Array<ModSrcId> srcs;

        auto idx = param->getModIndex();
        if (idx >= 0) {
            auto& pi = parameters.getReference (idx);
            for (auto& si : pi.sources)
                srcs.add (si.id);
        }

        return srcs;
    }

    bool ModulationMatrix::isModulated(ModDstId param) {
        auto& pi = parameters.getReference (param.id);
        if (pi.sources.size() > 0)
            return true;
        return false;
    }

    ModSrcId ModulationMatrix::getSourceAtIndex(ModDstId param, int index) {
        auto& pi = parameters.getReference (param.id);
        if (index >= pi.sources.size()) return {};
        return pi.sources[index].id;
    }

    bool ModulationMatrix::getModBipolar(ModSrcId src, ModDstId param) {
        auto& pi = parameters.getReference (param.id);
        for (auto& si : pi.sources)
            if (si.id == src)
                return si.bipolar;

        return false;
    }

    float ModulationMatrix::getModDepth(ModSrcId src, ModDstId param) {
        auto& pi = parameters.getReference (param.id);
        for (auto& si : pi.sources)
            if (si.id == src)
                return si.depth;

        return 0;
    }

    void ModulationMatrix::setModBipolar(ModSrcId src, ModDstId param, bool bipolar) {
        setModDepth(src, param, getModDepth(src, param), bipolar);
    }

    void ModulationMatrix::setModDepth(ModSrcId src, ModDstId param, float depth) {
        setModDepth(src, param, depth, getModBipolar(src, param));
    }

    void ModulationMatrix::addModAtNextIndex(ModSrcId src, ModDstId param, float depth, bool bipolar, int max) {
        int i=0;
        auto& pi = parameters.getReference (param.id);
        while (i < pi.sources.size() && (pi.sources[i].id.isValid() && pi.sources[i].depth != 0)) i++;
        i = std::min(max-1, i);
        addModAtIndex(src, param, i, depth, bipolar);
    }

    void ModulationMatrix::addModAtIndex(ModSrcId src, ModDstId param, int index, float depth, bool bipolar) {
        if (!src.isValid() || !param.isValid()) return;

        if (getModDepth(src, param) != 0) return;

        auto& pi = parameters.getReference (param.id);
        pi.sources.resize(std::max(pi.sources.size(), index+1));

        auto& si = pi.sources.getReference(index);
        si.id = src;
        si.depth = depth;
        si.bipolar = bipolar;

        listeners.call ([&] (Listener& l) { l.modMatrixChanged(); });
    }

    void ModulationMatrix::setModDepth(ModSrcId src, ModDstId param, float depth, bool bipolar) {
        if (!src.isValid() || !param.isValid()) return;

        auto& pi = parameters.getReference (param.id);
        for (auto& si : pi.sources) {
            if (si.id == src) {
                si.depth = depth;
                si.bipolar = bipolar;
                listeners.call ([&] (Listener& l) { l.modMatrixChanged(); });
                return;
            }
        }

        // add modulation if doesnt exist
        Source s {src, depth, bipolar};
        pi.sources.add (s);
        listeners.call ([&] (Listener& l) { l.modMatrixChanged(); });
    }

    bool ModulationMatrix::modExists (ModSrcId src, ModDstId param) {
        auto& pi = parameters.getReference (param.id);

        for (auto &si: pi.sources) if (si.id == src) return true;

        return false;
    }

    void ModulationMatrix::clearModDepth(ModSrcId src, ModDstId param) {
        auto& pi = parameters.getReference (param.id);
        for (auto& si : pi.sources) {
            if (si.id == src) {
                si.depth = 0;
                listeners.call ([&] (Listener& l) { l.modMatrixChanged(); });
                return;
            }
        }
    }

    float ModulationMatrix::getModValue(ModSrcId source, ModDstId dest) {
        if (!source.isValid()) return 0;
        auto& pi = parameters.getReference (dest.id);
        auto v = sources.getReference(source.id).value;
        for (auto& si : pi.sources) {
            if (si.id == source) {
                if (si.bipolar) {
                    v -= 0.5f;
                    v *= 2.f;
                }
            }
        }

        return v;

    }

    void ModulationMatrix::loadFromTree(const juce::ValueTree &vt) {
        auto lookupSrc = [&] (const juce::String& str)
        {
            int idx = 0;
            for (auto& s : sources)
            {
                if (s.id == str)
                    return ModSrcId (idx);
                idx++;
            }
            jassertfalse;
            return ModSrcId();
        };

        for (auto& pi : parameters)
            pi.sources.clear();

        auto mm = vt;
        if (mm.isValid() && mm.hasType("modmatrix"))
        {
            for (auto c : mm)
            {
                if (! c.hasType ("modulation")) continue;

                juce::String src = c.getProperty ("srcId");
                float f    = c.getProperty ("depth");
                juce::String dst = c.getProperty ("dstId");
                bool bipolar = c.getProperty ("bipolar");

                if (src.isNotEmpty() && dst.isNotEmpty())
                {
                    Source s;
                    s.id = lookupSrc (src);
                    s.depth = f;
                    s.bipolar = bipolar;

                    for (auto& pi : parameters)
                    {
                        if (pi.parameter->getUID() == dst)
                        {
                            pi.sources.add (s);
                            break;
                        }
                    }
                }
            }
        }
        listeners.call ([&] (Listener& l) { l.modMatrixChanged(); });
    }

    juce::ValueTree ModulationMatrix::getTree() {
        juce::ValueTree tree ("modmatrix");

        for (int i = 0; i < parameters.size(); i++)
        {
            auto& pi = parameters.getReference (i);
            for (auto src : pi.sources)
            {
                auto c = juce::ValueTree ("modulation");
                c.setProperty ("srcId", sources[src.id.id].id, nullptr);
                c.setProperty ("depth", src.depth, nullptr);
                c.setProperty ("dstId", pi.parameter->getUID(), nullptr);
                c.setProperty ("bipolar", src.bipolar, nullptr);

                tree.addChild (c, -1, nullptr);
            }
        }
        return tree;
    }

}
