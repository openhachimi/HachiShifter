// Existing Main-owned pronunciation picker, shared by insert and replacement.
void MainComponent::presentMaterialPronunciationInput(const std::shared_ptr<MaterialInputSession>& session)
{
    if(closingUI||materialInputSession!=session||!session->active)return;
    if(auto* old=materialPronunciationPopup.getComponent())
    {
        ++session->previewSerial;if(session->picker)stopSourcePreview(session->picker.getComponent());
        if(auto* previous=session->picker.getComponent();previous&&!session->resolving)session->query=previous->queryEditor().getText();
        old->removeComponentListener(this);materialPronunciationObserved=nullptr;materialPronunciationPopup=nullptr;old->setVisible(false);old->dismiss();
    }
    std::vector<MaterialPronunciationCandidate> candidates;
    for(const auto& chain:session->sequences)if(chain.members.size()>1)
    {
        const auto& first=chain.members.front();MaterialPronunciationCandidate c{first.audio,first.manifest,first.audioId,first.regionId,first.alias,first.row.regionStartSeconds,chain.members.back().row.regionEndSeconds,first.row.pronunciationKind};
        for(const auto& member:chain.members){c.regionIds.add(member.regionId);c.aliases.add(member.alias);}candidates.push_back(std::move(c));
    }
    for(const auto& entry:session->entries)candidates.push_back({entry.audio,entry.manifest,entry.audioId,entry.regionId,entry.alias,entry.row.regionStartSeconds,entry.row.regionEndSeconds,entry.row.pronunciationKind});
    auto box=std::make_shared<juce::Component::SafePointer<juce::CallOutBox>>();juce::Component::SafePointer<MainComponent> safe(this);
    auto picker=std::make_unique<PronunciationCandidateBox>(strings,std::move(candidates),
        [safe,session,box](const auto& candidate){if(safe&&!safe->closingUI&&session->active&&safe->materialInputSession==session&&box->getComponent()==safe->materialPronunciationPopup.getComponent())safe->chooseMaterialPronunciation(session,candidate);},
        [safe,session,box]{if(safe&&safe->materialInputSession==session&&box->getComponent()==safe->materialPronunciationPopup.getComponent())safe->finishMaterialPronunciationInput(false);});
    juce::Component::SafePointer<PronunciationCandidateBox> previewOwner(picker.get());
    picker->onPreview=[safe,session,previewOwner](const MaterialPronunciationCandidate& candidate)
    {
        if(!safe||!previewOwner||safe->closingUI||safe->materialInputSession!=session||!session->active||session->resolving)return;
        const auto request=++session->previewSerial;
        const auto previewKey=candidate.audio.getFullPathName()+"\n"+candidate.audioId+"\n"+(candidate.regionIds.isEmpty()?candidate.regionId:candidate.regionIds.joinIntoString("\n"));
        if(safe->sourcePreviewActive&&safe->sourcePreviewOwner==previewOwner.getComponent()&&session->previewKey==previewKey)
        {safe->stopSourcePreview(previewOwner.getComponent());session->previewKey.clear();return;}
        session->previewKey=previewKey;
        juce::String error;auto entries=session->candidateEntries(candidate,error);
        if(entries.empty()){safe->showError(error);return;}
        const auto worker=safe->ownedAnnotationExecutor();
        worker([safe,session,previewOwner,request,entries=std::move(entries)]
        {
            juce::String error;std::vector<MaterialPronunciationCatalog::Entry> current;
            for(const auto& entry:entries)
            {
                if(!session->active)return;MaterialPronunciationCatalog::Entry resolved;
                if(!MaterialPronunciationCatalog::resolve(entry,resolved,error))break;
                current.push_back(std::move(resolved));
            }
            juce::MessageManager::callAsync([safe,session,previewOwner,request,current=std::move(current),error,expected=entries.size()]
            {
                if(!safe||!previewOwner||!session->active||safe->materialInputSession!=session||session->previewSerial!=request)return;
                if(current.size()!=expected){safe->showError(error);return;}
                safe->startSourcePreview(previewOwner.getComponent(),current.front().audio,current.front().row.regionStartSeconds,current.back().row.regionEndSeconds);
            });
        });
    };
    picker->setQuery(session->query);
    if(session->scoreSequence){juce::StringArray remaining;for(auto i=session->nextNote;i<session->noteIds.size();++i)remaining.add(session->lyrics[int(i)]);picker->setSequenceContext(remaining);}
    picker->setName(session->action==MaterialInputAction::replace?"Replace note material source":"Insert material pronunciation");
    const auto anchor=pianoViewport.localAreaToGlobal(pianoViewport.getLocalBounds().removeFromTop(24));juce::Component::SafePointer<PronunciationCandidateBox> pickerSafe(picker.get());session->picker=pickerSafe;
    auto& callout=juce::CallOutBox::launchAsynchronously(std::move(picker),anchor,nullptr);*box=&callout;materialPronunciationPopup=&callout;materialPronunciationObserved=&callout;callout.addComponentListener(this);
    juce::MessageManager::callAsync([pickerSafe]{if(pickerSafe&&pickerSafe->isShowing())pickerSafe->queryEditor().grabKeyboardFocus();});
}
void MainComponent::chooseMaterialPronunciation(const std::shared_ptr<MaterialInputSession>& session,const MaterialPronunciationCandidate& candidate)
{
    if(!session->ready||session->resolving||materialInputSession!=session||!session->active)return;
    if(session->action==MaterialInputAction::replace&&project.revisionNumber()!=session->expectedRevision)
    {finishMaterialPronunciationInput(false,"The score changed while choosing material sources. Reopen the picker; no source was changed.");return;}
    juce::String identityError;auto picked=session->candidateEntries(candidate,identityError);
    if(identityError.isNotEmpty()){finishMaterialPronunciationInput(false,identityError);return;}
    ++session->previewSerial;if(session->picker)stopSourcePreview(session->picker.getComponent());
    if(picked.empty()||(session->action==MaterialInputAction::replace&&session->chosenEntries.size()+picked.size()>session->noteIds.size()))
    {finishMaterialPronunciationInput(false,"The requested source rows no longer match the remaining selected notes");return;}
    auto all=session->chosenEntries;all.insert(all.end(),picked.begin(),picked.end());session->resolving=true;
    const bool final=session->action==MaterialInputAction::insert||all.size()==session->noteIds.size();const auto executor=ownedAnnotationExecutor();juce::Component::SafePointer<MainComponent> safe(this);
    executor([safe,session,all=std::move(all),final]()mutable
    {
        std::vector<MaterialPronunciationCatalog::Entry> current;std::vector<ProjectModel::MaterialSourceChoice> choices;std::vector<MaterialPronunciationCatalog::Sequence> nextSequences;
        struct Rows{std::shared_ptr<const std::vector<SampleRegionSetting>> value;juce::String canonical;};std::map<juce::String,Rows> cache;juce::String error;bool valid=true;
        for(std::size_t i=0;i<all.size();++i)
        {
            if(!session->active){valid=false;break;}MaterialPronunciationCatalog::Entry entry;std::vector<SampleRegionSetting> rows;
            if(!MaterialPronunciationCatalog::resolve(all[i],entry,error,&rows)){valid=false;break;}
            OttoMaterialIO::Document document;document.audio={{entry.audioId,entry.audio.getFileName(),rows}};juce::String canonical;
            if(!OttoMaterialIO::encode(document,canonical,error)){valid=false;break;}
            const auto key=entry.manifest.getFullPathName()+"\n"+entry.audio.getFullPathName()+"\n"+entry.audioId;
            auto found=cache.find(key);
            if(found==cache.end())found=cache.emplace(key,Rows{std::make_shared<const std::vector<SampleRegionSetting>>(std::move(rows)),canonical}).first;
            else if(found->second.canonical!=canonical){error="Source annotations changed while revalidating the chosen chain";valid=false;break;}
            if(session->action==MaterialInputAction::replace)choices.push_back({session->noteIds[i],entry.audio,found->second.value,entry.audioId,entry.regionId,entry.alias});
            else choices.push_back({{},entry.audio,found->second.value,entry.audioId,entry.regionId,entry.alias});
            current.push_back(std::move(entry));
        }
        if(valid&&!final&&session->scoreSequence)
        {
            juce::StringArray remaining;for(std::size_t i=current.size();i<session->noteIds.size();++i)remaining.add(session->lyrics[int(i)]);
            valid=MaterialPronunciationCatalog::matchSequences(session->entries,remaining,nextSequences,error,[session]{return !session->active.load();});
        }
        juce::MessageManager::callAsync([safe,session,current=std::move(current),choices=std::move(choices),nextSequences=std::move(nextSequences),error,valid,final]()mutable
        {
            if(!safe||safe->closingUI||!session->active||safe->materialInputSession!=session)return;
            if(!valid){safe->finishMaterialPronunciationInput(false,error);return;}
            juce::String failure;
            if(session->action==MaterialInputAction::insert)
            {
                const auto& c=choices.front();const auto clip=safe->project.addMaterialPronunciations(c.audio,*c.completeRows,{c.regionId},session->atSeconds,session->trackId,failure,c.alias);
                if(clip.isEmpty()){safe->finishMaterialPronunciationInput(false,failure);return;}safe->scheduleAnalysis(c.audio,clip);safe->focusClip(clip);safe->finishMaterialPronunciationInput(true);return;
            }
            const auto data=safe->project.snapshot();
            if(safe->project.revisionNumber()!=session->expectedRevision||std::any_of(session->bindingKeys.begin(),session->bindingKeys.end(),[&](const auto& binding){return materialBindingKey(data,binding.first)!=binding.second;}))
            {safe->finishMaterialPronunciationInput(false,"The score/source binding changed while choosing. No source was changed; reopen the picker.");return;}
            if(!final)
            {
                session->chosenEntries=std::move(current);session->nextNote=session->chosenEntries.size();session->noteId=session->noteIds[session->nextNote];session->bindingKey=session->bindingKeys.at(session->noteId);session->query=session->lyrics[int(session->nextNote)];session->sequences=std::move(nextSequences);
                // Preserve the new remaining-note query instead of the completed
                // picker's previous text when recreating this SAME picker type.
                safe->presentMaterialPronunciationInput(session);session->resolving=false;return;
            }
            ProjectModel::MaterialSourceReplacementReport report;
            if(session->scoreSequence)report=safe->project.replaceNotesMaterialSources(choices,session->expectedRevision,failure);
            else
            {
                const auto& c=choices.front();report=safe->project.replaceNoteMaterialSource(c.noteId,c.audio,*c.completeRows,c.audioId,c.regionId,c.alias,failure);
            }
            if(report.rejected){safe->finishMaterialPronunciationInput(false,failure);return;}
            if(!report.tickets.empty())for(const auto& ticket:report.tickets)safe->scheduleAnalysis(ticket.sourceFile,{},ticket,true);
            else if(report.pendingAnalysis)safe->scheduleAnalysis(report.ticket.sourceFile,{},report.ticket,true);
            safe->focusNote(session->aliasNoteId.isNotEmpty()?session->aliasNoteId:session->noteIds.front());safe->pianoRoll.setSelectedNoteIds(session->noteIds);safe->finishMaterialPronunciationInput(true);
        });
    });
}
