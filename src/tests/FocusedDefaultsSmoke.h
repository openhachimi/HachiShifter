#pragma once
#include "../ProjectModel.h"
#include "../SettingsComponent.h"
#include <iostream>

namespace hachi
{
// Call on the JUCE message thread with a GUI initialiser. No MainComponent,
// AudioEngine, model files, analysis, rendering or NN inference is required.
// Deliberately not wired into Main.cpp: the caller owns the test entry point.
inline bool runFocusedDefaultsSmoke(const juce::File& output)
{
    if (output.createDirectory().failed()) return false;
    const auto root = output.getChildFile("focused-defaults-" + juce::Uuid().toString());
    if (root.createDirectory().failed()) return false;
    bool ok = true;
    juce::Array<juce::var> checks;
    const auto check = [&](const char* name, bool pass)
    {
        ok &= pass;
        std::cout << name << '=' << pass << std::endl;
        auto* row = new juce::DynamicObject();
        row->setProperty("name", name); row->setProperty("passed", pass);
        checks.add(juce::var(row));
    };
    const TrackData defaults;
    check("DTO_engine_default_is_NSF_without_model", defaults.pitchAlgorithm == PitchAlgorithm::nsfHifigan
        && defaultPitchAlgorithm(root.getChildFile("missing-model")) == PitchAlgorithm::nsfHifigan);
    check("processing_order_default_is_splice_before_synthesis", defaults.renderOrder == RenderOrder::stretchSpliceThenPitch
        && renderOrderPickerItem(defaults.renderOrder) == 2);
    check("stretch_algorithm_default_is_unchanged", defaults.stretchAlgorithm == StretchAlgorithm::melodyneHybrid);
    check("toolbar_processing_order_preserves_old_options", renderOrderPickerItem(RenderOrder::processThenSplice) == 1
        && renderOrderPickerItem(RenderOrder::processThenSplice, true) == 3);

    ProjectModel model;
    model.addTrack("New native track");
    const auto created = model.snapshot();
    check("new_track_uses_default_authority", created.tracks.size() == 1
        && created.tracks[0].pitchAlgorithm == defaults.pitchAlgorithm
        && created.tracks[0].renderOrder == defaults.renderOrder
        && created.tracks[0].stretchAlgorithm == defaults.stretchAlgorithm);
    juce::String error;
    const auto seed = root.getChildFile("seed.hjpx");
    const auto saved = model.save(seed, error);
    check("fixture_saved", saved);
    juce::MemoryBlock bytes;
    const auto read = saved && seed.loadFileAsData(bytes);
    const auto original = read ? juce::ValueTree::readFromData(bytes.getData(), bytes.getSize()) : juce::ValueTree{};
    check("fixture_tree_is_readable", original.isValid());
    if (!original.isValid()) return false;
    ProjectModel modern;
    check("modern_explicit_NSF_and_processing_order_reopen", modern.load(seed, error)
        && modern.snapshot().tracks.size() == 1 && modern.snapshot().tracks[0].pitchAlgorithm == defaults.pitchAlgorithm
        && modern.snapshot().tracks[0].renderOrder == defaults.renderOrder);
    const auto writeTree = [](const juce::ValueTree& tree, const juce::File& file)
    {
        juce::MemoryOutputStream stream; tree.writeToStream(stream);
        return file.replaceWithData(stream.getData(), stream.getDataSize());
    };
    for (const auto legacy : { false, true })
    {
        auto tree = original.createCopy();
        auto track = tree.getChildWithName("Track");
        track.removeProperty("pitchAlgorithm", nullptr);
        track.removeProperty("renderOrder", nullptr);
        if (legacy) track.removeProperty("trackType", nullptr);
        const auto file = root.getChildFile(legacy ? "old-missing.hjpx" : "new-missing.hjpx");
        ProjectModel reopened;
        const auto loaded = writeTree(tree, file) && reopened.load(file, error);
        const auto data = reopened.snapshot();
        check(legacy ? "legacy_missing_fields_use_defaults" : "modern_missing_fields_use_defaults", loaded
            && data.tracks.size() == 1 && data.tracks[0].pitchAlgorithm == defaults.pitchAlgorithm
            && data.tracks[0].renderOrder == defaults.renderOrder
            && data.tracks[0].stretchAlgorithm == defaults.stretchAlgorithm);
    }
    for (const auto* engine : { "nsf-hifigan", "world", "llsm2", "mld5", "mld3" })
    {
        auto tree = original.createCopy();
        auto track = tree.getChildWithName("Track");
        track.removeProperty("trackType", nullptr);
        track.setProperty("pitchAlgorithm", engine, nullptr);
        track.setProperty("renderOrder", "process-then-splice", nullptr);
        track.setProperty("stretchAlgorithm", "loop", nullptr);
        track.setProperty("mergeContiguousSource", true, nullptr);
        const auto file = root.getChildFile(juce::String(engine) + "-explicit.hjpx");
        ProjectModel reopened;
        const auto loaded = writeTree(tree, file) && reopened.load(file, error);
        const auto data = reopened.snapshot();
        const auto expected = juce::String(engine) == "nsf-hifigan" ? PitchAlgorithm::nsfHifigan
            : juce::String(engine) == "world" ? PitchAlgorithm::world
            : juce::String(engine) == "llsm2" ? PitchAlgorithm::llsm2
            : juce::String(engine) == "mld5" ? PitchAlgorithm::mld5 : PitchAlgorithm::mld3;
        check((juce::String("explicit_saved_") + engine + "_and_old_order_are_preserved").toRawUTF8(), loaded
            && data.tracks.size() == 1 && data.tracks[0].pitchAlgorithm == expected
            && data.tracks[0].renderOrder == RenderOrder::processThenSplice
            && data.tracks[0].mergeContiguousSource && data.tracks[0].stretchAlgorithm == StretchAlgorithm::loop);
    }

    juce::PropertiesFile::Options options; options.applicationName = "focused-defaults-smoke";
    options.storageFormat = juce::PropertiesFile::storeAsXML;
    juce::PropertiesFile properties(root.getChildFile("isolated-settings.xml"), options);
    properties.setValue("algorithm.hifiganPath", root.getChildFile("missing-model").getFullPathName());
    check("missing_import_preference_defaults_to_NSF_without_writing_key", defaultImportedPitchAlgorithm(&properties) == PitchAlgorithm::nsfHifigan
        && !properties.containsKey("import.algorithm"));
    I18n strings;
    juce::AudioDeviceManager devices; // Not initialised: no device is opened.
    {
        SettingsComponent settings(strings, devices, properties, [] {});
        check("settings_engine_default_is_NSF_with_missing_model", settings.diagnosticImportedAlgorithmItem() == 2);
        check("settings_stretch_default_remains_one", settings.diagnosticImportedStretchAlgorithmItem() == 1);
    }
    for (const auto id : { 2, 3, 6 })
    {
        properties.setValue("import.algorithm", id);
        const auto stretch = id == 2 ? 2 : 3;
        properties.setValue("import.stretchAlgorithm", stretch);
        const auto expected = id == 2 ? PitchAlgorithm::nsfHifigan : id == 3 ? PitchAlgorithm::world : PitchAlgorithm::llsm2;
        SettingsComponent settings(strings, devices, properties, [] {});
        check((juce::String("explicit_settings_engine_") + juce::String(id) + "_is_preserved").toRawUTF8(),
            defaultImportedPitchAlgorithm(&properties) == expected && settings.diagnosticImportedAlgorithmItem() == id
            && properties.getIntValue("import.algorithm") == id);
        check((juce::String("explicit_stretch_preference_with_engine_") + juce::String(id) + "_is_preserved").toRawUTF8(),
            properties.getIntValue("import.stretchAlgorithm") == stretch
            && settings.diagnosticImportedStretchAlgorithmItem() == stretch);
    }
    auto* report = new juce::DynamicObject();
    report->setProperty("passed", ok); report->setProperty("checks", juce::var(checks));
    report->setProperty("fixtures", root.getFullPathName());
    const auto reported = output.getChildFile("focused-defaults-verification.json").replaceWithText(juce::JSON::toString(juce::var(report), true));
    return ok && reported;
}
}
