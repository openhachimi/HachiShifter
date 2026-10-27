#pragma once
#include "../LegacyNativeSourceUsage.h"
#include "../ProjectFileIO.h"
#include "../PianoRollComponent.h"
#include "../NativeVirtualRenderPlan.h"
#include <iostream>
namespace hachi
{
inline bool runLegacyNativeUsageSmoke(const juce::File& folder,const juce::File& original)
{
    if(folder.createDirectory().failed())return false;bool ok=true;juce::String error,log;
    const auto check=[&](const juce::String& name,bool pass){ok&=pass;log<<name<<"="<<(pass?"PASS":"FAIL")<<" | "<<error<<"\n";error.clear();std::cout<<name<<"="<<pass<<std::endl;};
    const auto originalHash=juce::SHA256(original).toHexString();juce::MemoryBlock bytes;
    check("real_old_file_read_only",original.loadFileAsData(bytes));const auto tree=projectio::validatedTree(bytes);check("real_old_tree_valid",tree.isValid());
    int legacyFields=0;const auto count=[&](auto&& self,juce::ValueTree node)->void{if(node.hasProperty("materialSourceUsageGroupId"))++legacyFields;for(auto c:node)self(self,c);};count(count,tree);
    ProjectModel model;check("old_file_loads",model.load(original,error));const auto base=model.snapshot();std::vector<std::pair<juce::String,juce::String>> edges;
    for(const auto& track:base.tracks)for(const auto& a:track.clips)for(const auto& b:track.clips)
        if(&a!=&b&&a.notes.size()==1&&b.notes.size()==1&&a.materialSourceUsageGroupId.isNotEmpty()&&a.materialSourceUsageGroupId==b.materialSourceUsageGroupId&&std::abs(a.startSeconds+a.notes[0].startSeconds+a.notes[0].durationSeconds-b.startSeconds-b.notes[0].startSeconds)<1.e-6)
            edges.emplace_back(a.notes[0].id,b.notes[0].id);
    log<<"saved_usage_fields="<<legacyFields<<" restored_edges="<<int(edges.size())<<"\n";
    check("real_legacy_source_components_are_restored",legacyFields==0&&!edges.empty());
    const auto gap=[&](const ProjectData& data,const auto& edge){double left=0,right=1.e9;for(const auto& track:data.tracks)for(const auto& clip:track.clips)for(const auto& note:clip.notes){if(note.id==edge.first)left=clip.startSeconds+note.startSeconds+note.durationSeconds;if(note.id==edge.second)right=clip.startSeconds+note.startSeconds;}return right-left;};
    int cases=0;
    for(const auto& edge:edges)for(double delta:{-.02,.02})
    {
        model.resetDocument(base);const auto fingerprint=model.contentFingerprint();const auto changed=model.moveNativeNotes({edge.second},delta,0,&error);const auto after=model.snapshot();bool connected=changed;for(const auto& pair:edges)connected&=std::abs(gap(after,pair))<1.e-6;
        check("real_full_project_move_"+juce::String(++cases),connected);model.undo();check("real_full_project_undo_"+juce::String(cases),model.contentFingerprint()==fingerprint);
    }
    {
        I18n strings;PianoRollComponent roll(model,strings);roll.setFocusedTrack(base.tracks.front().id);roll.setTool(PianoRollComponent::Tool::note);roll.setShowNativeWaveforms(false);roll.setPixelsPerSecond(1600);roll.setRowHeight(26);roll.setSize(18000,3800);
        const auto event=[&](juce::Point<float> at,juce::Point<float> down){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),at,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier|juce::ModifierKeys::altModifier),1,0,0,0,0,&roll,&roll,juce::Time::getCurrentTime(),down,juce::Time::getCurrentTime(),1,at!=down);};
        int tested=0;for(const auto& edge:edges)
        {
            if(tested>=3)break;
            for(const auto& id:{edge.first,edge.second})for(double delta:{-.02,.02})
            {
                model.resetDocument(base);model.dispatchPendingMessages();roll.diagnosticRefresh();
                juce::Image image(juce::Image::ARGB,18000,120,true,juce::SoftwareImageType{});juce::Graphics graphics(image);float midi=60;for(const auto& clip:base.tracks.front().clips)for(const auto& note:clip.notes)if(note.id==id)midi=note.midiNote;graphics.addTransform(juce::AffineTransform::translation(0,-roll.diagnosticYForMidi(midi)+40));roll.paintEntireComponent(graphics,true);
                std::optional<juce::Point<float>> down;for(std::size_t hit=0;hit<roll.diagnosticHitCount();++hit)if(roll.diagnosticHitNoteId(hit)==id){auto body=roll.diagnosticHitNominalBounds(hit).getIntersection(roll.diagnosticHitBounds(hit));if(body.isEmpty())body=roll.diagnosticHitBounds(hit);down=juce::Point<float>(body.getCentreX(),body.getBottom()-4.f);}
                ProjectModel reference;reference.resetDocument(base);juce::String reason;const bool shouldMove=reference.moveNativeNotes({id},delta,0,&reason);
                const auto fingerprint=model.contentFingerprint();if(down){const auto up=down->translated(float(delta*1600),0);roll.mouseDown(event(*down,*down));roll.mouseDrag(event(*down+(up-*down)*.5f,*down));roll.mouseDrag(event(up,*down));check("legacy_mouse_preview_is_not_commit_"+id+juce::String(delta),model.contentFingerprint()==fingerprint);roll.mouseUp(event(up,*down));}
                bool connected=down&&((model.contentFingerprint()!=fingerprint)==shouldMove);for(const auto& pair:edges)connected&=std::abs(gap(model.snapshot(),pair))<1.e-6;
                if(!connected){log<<"mouse_debug hit="<<int(bool(down))<<" changed="<<int(model.contentFingerprint()!=fingerprint)<<" x="<<(down?down->x:0)<<" y="<<(down?down->y:0)<<"\n";for(const auto& pair:edges)if(std::abs(gap(model.snapshot(),pair))>=1.e-6)log<<"broken="<<pair.first<<" -> "<<pair.second<<" gap="<<gap(model.snapshot(),pair)<<"\n";}
                check("legacy_actual_mouse_"+id+juce::String(delta),connected);
            }++tested;
        }
    }
    model.resetDocument(base);const auto saved=folder.getChildFile("compatibility-roundtrip.hjpx");check("compatibility_save_to_new_file",model.save(saved,error));ProjectModel cold;check("cold_reload_preserves_restored_ids",cold.load(saved,error));bool identities=cold.snapshot().tracks.size()==base.tracks.size();if(identities)for(std::size_t t=0;t<base.tracks.size();++t)for(std::size_t c=0;c<base.tracks[t].clips.size();++c)identities&=cold.snapshot().tracks[t].clips[c].materialSourceUsageGroupId==base.tracks[t].clips[c].materialSourceUsageGroupId;
    check("load_migration_is_idempotent",identities);
    auto independent=base;for(auto& track:independent.tracks)for(auto& clip:track.clips){clip.materialSourceUsageGroupId.clear();clip.glideConnectedToNext=clip.glideConnectedFromPrevious=false;for(auto& note:clip.notes)note.connectedToNext=note.connectedToPrevious=false;}
    check("explicit_disconnects_are_not_recovered",restoreLegacyNativeSourceUsages(independent,tree)==0);
    auto explicitEmpty=tree.createCopy();const auto markEmpty=[&](auto&& self,juce::ValueTree node)->void{if(node.hasType("Clip"))node.setProperty("materialSourceUsageGroupId","",nullptr);for(auto c:node)self(self,c);};markEmpty(markEmpty,explicitEmpty);auto protectedData=base;for(auto& track:protectedData.tracks)for(auto& clip:track.clips)clip.materialSourceUsageGroupId.clear();
    check("explicit_empty_field_is_not_guessed",restoreLegacyNativeSourceUsages(protectedData,explicitEmpty)==0);
    check("original_user_project_not_modified",juce::SHA256(original).toHexString()==originalHash);
    folder.getChildFile("verification.txt").replaceWithText(log);return ok;
}
}
