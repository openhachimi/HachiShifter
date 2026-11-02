#pragma once
#include "../MainComponent.h"
#include <algorithm>
#include <iostream>

namespace hachi
{
// Included once by MainComponent.cpp. Uses the real ComboBox notification,
// current-track writer, production refresh and binary save/load; no DSP or NN.
bool MainComponent::diagnosticMld5RenderOrderUi(const juce::File& output)
{
    if (output.createDirectory().failed()) return false;
    stopTimer(); audio.stop(); pianoRoll.setReadsVoicebankInBackground(false);
    bool ok = true;
    juce::String log, error;
    const auto check = [&](const juce::String& name, bool pass)
    {
        ok &= pass;
        log << name << '=' << (pass ? "PASS" : "FAIL") << ' ' << error << '\n';
        std::cout << name << '=' << pass << std::endl;
        error.clear();
    };
    project.resetDocument(ProjectData{});
    const auto other = project.addTypedTrack(TrackType::hachi, "Untouched NSF");
    const auto selected = project.addTypedTrack(TrackType::hachi, "MLD5 render order");
    project.dispatchPendingMessages();
    setSize(1280, 760); setTrackSettingsVisible(true);
    const auto findTrack = [](const ProjectData& data, const juce::String& id) -> const TrackData*
    {
        const auto found = std::find_if(data.tracks.begin(), data.tracks.end(),
            [&](const auto& track) { return track.id == id; });
        return found == data.tracks.end() ? nullptr : &*found;
    };
    const auto hasOrder = [&](const ProjectData& data, const juce::String& id, int item)
    {
        const auto* track = findTrack(data, id);
        return track != nullptr
            && track->renderOrder == (item == 2 ? RenderOrder::stretchSpliceThenPitch : RenderOrder::processThenSplice)
            && track->mergeContiguousSource == (item == 3);
    };
    const auto initial = project.snapshot();
    const auto* otherInitial = findTrack(initial, other);
    const auto untouched = [&](const ProjectData& data)
    {
        const auto* track = findTrack(data, other);
        return track != nullptr && otherInitial != nullptr
            && track->pitchAlgorithm == otherInitial->pitchAlgorithm
            && track->stretchAlgorithm == otherInitial->stretchAlgorithm
            && track->renderOrder == otherInitial->renderOrder
            && track->mergeContiguousSource == otherInitial->mergeContiguousSource;
    };
    diagnosticSelectTrack(other);
    check("NSF_default_remains_item2", hasOrder(project.snapshot(), other, 2)
        && renderOrder.getSelectedId() == 2);
    diagnosticSelectTrack(selected);
    check("MLD5_engine_item_is_available", pitchAlgorithm.indexOfItemId(1) >= 0 && pitchAlgorithm.isItemEnabled(1));
    pitchAlgorithm.setSelectedId(1, juce::sendNotificationSync);
    project.dispatchPendingMessages(); refreshProjectControls();
    check("MLD5_default_is_source_continuity_item3", hasOrder(project.snapshot(), selected, 3)
        && renderOrder.getSelectedId() == 3);
    check("MLD5_order_picker_and_all_three_choices_are_visible", renderOrder.isVisible()
        && trackSettingsPanel.isVisible() && renderOrderLabel.isVisible() && renderOrder.getWidth() > 0
        && renderOrder.getNumItems() == 3 && renderOrder.indexOfItemId(1) >= 0
        && renderOrder.indexOfItemId(2) >= 0 && renderOrder.indexOfItemId(3) >= 0);
    check("engine_selection_keeps_other_NSF_track_unchanged", untouched(project.snapshot()));

    for (const auto item : { 1, 2, 3 })
    {
        const auto tag = "MLD5_item" + juce::String(item);
        renderOrder.setSelectedId(item, juce::sendNotificationSync);
        project.dispatchPendingMessages();
        const auto chosen = project.snapshot();
        check(tag + "_real_combo_updates_only_current_model_track", hasOrder(chosen, selected, item) && untouched(chosen));
        refreshProjectControls();
        check(tag + "_refresh_preserves_choice", renderOrder.getSelectedId() == item);
        refreshTexts(); refreshProjectControls();
        check(tag + "_text_refresh_preserves_choice", renderOrder.getSelectedId() == item);
        pitchAlgorithm.setSelectedId(3, juce::sendNotificationSync);
        project.dispatchPendingMessages(); refreshProjectControls();
        check(tag + "_WORLD_hides_native_order", !renderOrder.isVisible());
        pitchAlgorithm.setSelectedId(1, juce::sendNotificationSync);
        project.dispatchPendingMessages(); refreshProjectControls();
        check(tag + "_engine_roundtrip_preserves_explicit_choice", hasOrder(project.snapshot(), selected, item)
            && renderOrder.getSelectedId() == item && untouched(project.snapshot()));
        diagnosticSelectTrack(other);
        check(tag + "_switch_to_other_preserves_NSF_item2", renderOrder.getSelectedId() == 2 && untouched(project.snapshot()));
        diagnosticSelectTrack(selected);
        check(tag + "_switch_back_preserves_choice", renderOrder.getSelectedId() == item);
        ProjectModel cold;
        const auto file = output.getChildFile("mld5-order-item" + juce::String(item) + ".hjpx");
        const auto loaded = project.save(file, error) && cold.load(file, error);
        const auto reopened = cold.snapshot();
        const auto* reopenedTrack = findTrack(reopened, selected);
        check(tag + "_save_cold_load_preserves_exact_order_and_other_track", loaded
            && reopenedTrack != nullptr && reopenedTrack->pitchAlgorithm == PitchAlgorithm::mld5
            && hasOrder(reopened, selected, item) && untouched(reopened));
        if (loaded)
        {
            project.resetDocument(reopened); project.dispatchPendingMessages();
            diagnosticSelectTrack(selected); refreshProjectControls();
            check(tag + "_cold_snapshot_refresh_preserves_combo", renderOrder.getSelectedId() == item
                && renderOrder.isVisible() && untouched(project.snapshot()));
        }
    }

    // Invalid/stale events cannot cross semantic track domains or edit all tracks.
    for (const auto type : { TrackType::utau, TrackType::diffSinger })
    {
        const auto id = project.addTypedTrack(type, trackTypeName(type));
        project.dispatchPendingMessages(); diagnosticSelectTrack(id);
        check(trackTypeName(type) + "_hides_native_processing_order", !renderOrder.isVisible() && !renderOrderLabel.isVisible());
        const auto before = project.contentFingerprint();
        renderOrder.setSelectedId(renderOrder.getSelectedId() == 1 ? 2 : 1, juce::sendNotificationSync);
        project.dispatchPendingMessages();
        check(trackTypeName(type) + "_stale_order_event_is_noop", project.contentFingerprint() == before && untouched(project.snapshot()));
    }
    const auto world = project.addTypedTrack(TrackType::hachi, "WORLD order hidden");
    project.setTrackPitchAlgorithm(world, PitchAlgorithm::world);
    project.dispatchPendingMessages(); diagnosticSelectTrack(world);
    check("WORLD_hides_processing_order", !renderOrder.isVisible() && !renderOrderLabel.isVisible());
    auto before = project.contentFingerprint();
    renderOrder.setSelectedId(renderOrder.getSelectedId() == 1 ? 2 : 1, juce::sendNotificationSync);
    project.dispatchPendingMessages();
    check("WORLD_stale_order_event_is_noop", project.contentFingerprint() == before);
    selectedTrackId.clear(); refreshProjectControls();
    check("no_current_track_hides_processing_order", !renderOrder.isVisible());
    before = project.contentFingerprint();
    renderOrder.setSelectedId(renderOrder.getSelectedId() == 1 ? 2 : 1, juce::sendNotificationSync);
    project.dispatchPendingMessages();
    check("no_current_track_never_changes_all_tracks", project.contentFingerprint() == before && untouched(project.snapshot()));
    diagnosticSelectTrack(selected); refreshProjectControls();
    check("final_MLD5_choice_and_NSF_default_are_retained", renderOrder.getSelectedId() == 3
        && hasOrder(project.snapshot(), selected, 3) && untouched(project.snapshot()));
    return output.getChildFile("verification.txt").replaceWithText(log) && ok;
}
}
