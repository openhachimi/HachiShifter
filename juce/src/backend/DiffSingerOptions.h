#pragma once
#include <cmath>
#include <juce_data_structures/juce_data_structures.h>

namespace hachi::backend
{
struct DiffSingerOptions
{
    int backend = 1; // auto, CPU, DirectML
    int device = 0;
    int preview = 2, exportQuality = 3; // fast, standard, high, custom
    int acousticSteps = 20, pitchSteps = 20, varianceSteps = 20;
    double depth = 1.0;

    static DiffSingerOptions read(const juce::PropertiesFile& p)
    {
        DiffSingerOptions v;
        v.backend = juce::jlimit(1, 3, p.getIntValue("ds.backend", 1));
        v.device = juce::jlimit(0, 31, p.getIntValue("ds.device", 0));
        v.preview = juce::jlimit(1, 4, p.getIntValue("ds.preview", 2));
        v.exportQuality = juce::jlimit(1, 4, p.getIntValue("ds.export", 3));
        v.acousticSteps = juce::jlimit(1, 1000, p.getIntValue("ds.acousticSteps", 20));
        v.pitchSteps = juce::jlimit(1, 1000, p.getIntValue("ds.pitchSteps", 20));
        v.varianceSteps = juce::jlimit(1, 1000, p.getIntValue("ds.varianceSteps", 20));
        const auto d = p.getDoubleValue("ds.depth", 1.0);
        v.depth = std::isfinite(d) ? juce::jlimit(0.01, 1.0, d) : 1.0;
        return v;
    }
    void write(juce::PropertiesFile& p) const
    {
        p.setValue("ds.backend", backend); p.setValue("ds.device", device);
        p.setValue("ds.preview", preview); p.setValue("ds.export", exportQuality);
        p.setValue("ds.acousticSteps", acousticSteps); p.setValue("ds.pitchSteps", pitchSteps);
        p.setValue("ds.varianceSteps", varianceSteps); p.setValue("ds.depth", depth);
    }
    juce::var request(bool exporting = false) const
    {
        const char* backends[] { "auto", "cpu", "directml" };
        const char* qualities[] { "fast", "standard", "high", "custom" };
        const auto q = juce::jlimit(1, 4, exporting ? exportQuality : preview);
        const int steps[][3] { {8, 8, 8}, {20, 20, 20}, {50, 40, 40} };
        auto* value = new juce::DynamicObject();
        value->setProperty("backend", backends[juce::jlimit(1, 3, backend)-1]);
        value->setProperty("device", juce::jlimit(0, 31, device));
        value->setProperty("quality", qualities[q-1]);
        value->setProperty("acoustic_steps", q == 4 ? acousticSteps : steps[q-1][0]);
        value->setProperty("pitch_steps", q == 4 ? pitchSteps : steps[q-1][1]);
        value->setProperty("variance_steps", q == 4 ? varianceSteps : steps[q-1][2]);
        value->setProperty("depth", q == 4 ? depth : 1.0);
        return juce::var(value);
    }
};
}
