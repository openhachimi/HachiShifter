#pragma once
#include "../backend/AnalysisService.h"
#include <iostream>

namespace hachi
{
inline bool gameDefaultsSmoke(const juce::File& folder)
{
    using namespace backend;
    folder.createDirectory();
    juce::Array<juce::var> checks;
    bool ok = true;
    const auto check = [&](const juce::String& name, bool pass)
    {
        auto* row = new juce::DynamicObject();
        row->setProperty("name", name); row->setProperty("ok", pass);
        checks.add(juce::var(row)); ok &= pass;
        std::cout << name << "=" << pass << std::endl;
    };
    AnalysisConfig defaults;
    const auto portable = AnalysisService::status(defaults);
    check("default_medium", defaults.gameModel == "medium");
    check("portable_game_and_fcpe_ready", portable.gameModelReady && portable.fcpeModelReady);
    check("portable_runtime_ready", portable.onnxRuntimeReady);
    check("portable_actual_variant", portable.gameModel == "medium"
        && portable.activeBackend == "GAME+FCPE");
    const auto besideExe = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
        .getParentDirectory().getChildFile("models");
    check("portable_paths_beside_exe", portable.gameModelDirectory.isAChildOf(besideExe)
        && portable.fcpeModelPath.isAChildOf(besideExe));

    juce::PropertiesFile::Options options;
    options.storageFormat = juce::PropertiesFile::storeAsXML;
    options.millisecondsBeforeSaving = -1;
    const auto settingsFile = folder.getNonexistentChildFile("analysis-settings", ".xml");
    juce::PropertiesFile properties(settingsFile, options);
    check("new_user_medium", AnalysisService::configFromProperties(&properties).gameModel == "medium");
    properties.setValue("algorithm.gameModel", "large");
    check("legacy_missing_large_adopts_medium",
        AnalysisService::configFromProperties(&properties).gameModel == "medium");
    properties.setValue("algorithm.gameModel", "small");
    check("explicit_small_preserved",
        AnalysisService::configFromProperties(&properties).gameModel == "small");
    properties.setValue("algorithm.gameModel", "large");
    const auto custom = folder.getChildFile("custom-large"); custom.createDirectory();
    properties.setValue("algorithm.gamePath", custom.getFullPathName());
    check("custom_model_choice_preserved",
        AnalysisService::configFromProperties(&properties).gameModel == "large");
    check("custom_model_path_preserved",
        AnalysisService::configFromProperties(&properties).gameModelDirectory == custom);
    properties.setValue("algorithm.gamePath", "");
    properties.setValue("algorithm.gameModel", "medium");
    check("save_medium", properties.saveIfNeeded());
    juce::PropertiesFile reopened(settingsFile, options);
    check("reopen_medium", AnalysisService::configFromProperties(&reopened).gameModel == "medium");

    auto explicitRoot = defaults;
    explicitRoot.gameModelDirectory = besideExe;
    check("models_root_resolves_medium", AnalysisService::status(explicitRoot).gameModelDirectory
        == portable.gameModelDirectory);
    explicitRoot.gameModelDirectory = portable.gameModelDirectory;
    explicitRoot.gameModel = "large";
    check("explicit_folder_reports_actual_medium", AnalysisService::status(explicitRoot).gameModel == "medium");
    auto unavailable = defaults; unavailable.gameModel = "large";
    check("missing_model_keeps_native_fallback", AnalysisService::status(unavailable).activeBackend == "native-hq");
    auto* report = new juce::DynamicObject();
    report->setProperty("ok", ok); report->setProperty("checks", checks);
    folder.getChildFile("game-defaults.json").replaceWithText(juce::JSON::toString(juce::var(report), true));
    return ok;
}
}
