#pragma once

namespace hachi
{
inline bool MainComponent::diagnosticNsfPicker()
{
    const auto configured = preferences != nullptr
        ? juce::File(preferences->getValue("algorithm.hifiganPath")) : juce::File{};
    if (!backend::NsfHifiganRenderer::modelAvailable(configured))
    {
        std::cout << "model_available=0\n";
        return false;
    }
    std::cout << "model_available=1\n";
    const auto other = project.addTrack("Other track", true);
    project.setTrackPitchAlgorithm(other, PitchAlgorithm::world);
    const auto selected = project.addTrack("NSF picker", true);
    project.dispatchPendingMessages();
    diagnosticSelectTrack(selected);
    bool ok = true;
    for (const auto id : { 3, 2, 6, 2 })
    {
        pitchAlgorithm.setSelectedId(id, juce::sendNotificationSync);
        project.dispatchPendingMessages();
        refreshProjectControls();
        const auto expected = id == 2 ? PitchAlgorithm::nsfHifigan
            : id == 3 ? PitchAlgorithm::world : PitchAlgorithm::llsm2;
        bool trackChanged = false, otherUnchanged = false;
        for (const auto& track : project.snapshot().tracks)
        {
            if (track.id == selected) trackChanged = track.pitchAlgorithm == expected;
            if (track.id == other) otherUnchanged = track.pitchAlgorithm == PitchAlgorithm::world;
        }
        const auto passed = trackChanged && otherUnchanged && pitchAlgorithm.getSelectedId() == id;
        std::cout << "picker_" << id << "_persists_without_changing_other_track=" << passed << '\n';
        ok = ok && passed;
    }
    return ok;
}
}
