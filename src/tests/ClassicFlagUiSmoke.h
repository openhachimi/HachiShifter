#pragma once
#include "../MainComponent.h"
#include <iostream>
namespace hachi
{
inline bool MainComponent::diagnosticClassicFlagLane(const juce::File& output)
{
    setSize(1400,860);
    const auto track=project.addTrack("WCSNDM - ordinary UTAU",true);
    project.setTrackPitchAlgorithm(track,PitchAlgorithm::utau);
    const auto clip=project.addClip(track,0,3);
    const auto id=project.addNote(clip,.4,1.4,60);
    project.setNoteLabel(id,"a"); project.dispatchPendingMessages();
    diagnosticSelectTrack(track); pianoRoll.setFocusedClip(clip);
    pianoRoll.diagnosticRefresh(); pianoRoll.setSelectedNoteIds({id});
    refreshSelectedNoteParameter();
    bool ok=flagCurveButton.isEnabled() && !flagEnvelopeButton.isEnabled();
    flagCurveButton.setToggleState(true,juce::dontSendNotification);flagCurveButton.onClick();
    ok=flagEnvelopeButton.isEnabled() && ok;
    project.setNoteUtauFlagCurve(id,"g",{{0,-20},{1.4,30}});
    project.setNoteUtauFlagCurve(id,"Mb",{{0,0},{.5,0},{1.4,100}});
    project.dispatchPendingMessages(); pianoRoll.diagnosticRefresh();
    flagEnvelopeButton.onClick(); pianoRoll.setFlagLaneFlag("Mb"); resized();
    ok=ok && pianoRoll.currentTool()==PianoRollComponent::Tool::flagCurve
        && pianoRoll.flagCurveActiveFor(id) && pianoRoll.flagLaneFlag()=="Mb";
    const auto note=project.snapshot().tracks.back().clips.front().notes.front();
    ok=ok && note.utauFlagCurves.size()==2;
    if(output!=juce::File{})
    {
        output.getParentDirectory().createDirectory();
        const auto image=createComponentSnapshot(getLocalBounds());
        if(auto stream=output.createOutputStream()) juce::PNGImageFormat().writeImageToStream(image,*stream);
        else ok=false;
    }
    std::cout << "classic_WCSNDM_flag_buttons_and_lane=" << ok << std::endl;
    return ok;
}
}
