#pragma once
namespace hachi
{
inline bool MainComponent::diagnosticMld5Picker(const juce::File& output)
{
    output.createDirectory();audio.stop();bool ok=true;juce::String log,error;
    const auto check=[&](const char* name,bool pass){ok&=pass;log<<name<<"="<<(pass?"PASS":"FAIL")<<" "<<error<<"\n";std::cout<<name<<"="<<pass<<std::endl;error.clear();};
    const auto other=project.addTypedTrack(TrackType::hachi,"Other"),selected=project.addTypedTrack(TrackType::hachi,"MLD5 selection");project.dispatchPendingMessages();diagnosticSelectTrack(selected);
    check("MLD5_is_an_enabled_native_algorithm_choice",pitchAlgorithm.indexOfItemId(1)>=0&&pitchAlgorithm.isItemEnabled(1));
    pitchAlgorithm.setSelectedId(1,juce::sendNotificationSync);project.dispatchPendingMessages();refreshProjectControls();
    bool chosen=false,unchanged=false;for(const auto& track:project.snapshot().tracks){if(track.id==selected)chosen=track.pitchAlgorithm==PitchAlgorithm::mld5;if(track.id==other)unchanged=track.pitchAlgorithm==defaultPitchAlgorithm();}
    check("real_combo_selects_MLD5_only_on_selected_track",chosen&&unchanged&&pitchAlgorithm.getSelectedId()==1);
    diagnosticSelectTrack(other);diagnosticSelectTrack(selected);refreshProjectControls();check("MLD5_remains_selected_after_track_switch",pitchAlgorithm.getSelectedId()==1);
    check("MLD5_selection_undo",project.undo());project.dispatchPendingMessages();refreshProjectControls();check("undo_restores_default_NSF",pitchAlgorithm.getSelectedId()==2);
    check("MLD5_selection_redo",project.redo());project.dispatchPendingMessages();refreshProjectControls();check("redo_restores_MLD5_item1",pitchAlgorithm.getSelectedId()==1);
    ProjectModel cold;const auto saved=output.getChildFile("mld5-selected.hjpx");bool persisted=project.save(saved,error)&&cold.load(saved,error);bool found=false;for(const auto& track:cold.snapshot().tracks)if(track.id==selected){found=true;persisted&=track.pitchAlgorithm==PitchAlgorithm::mld5;}
    check("MLD5_selection_save_cold_reopen",found&&persisted);check("MLD3_remains_unavailable_in_native_picker",pitchAlgorithm.indexOfItemId(5)<0);
    output.getChildFile("verification.txt").replaceWithText(log);return ok;
}
}
