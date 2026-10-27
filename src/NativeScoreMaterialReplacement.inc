// Included inside namespace hachi in ProjectModel.cpp. This is the score-native
// adapter of the existing replacement authority, not a second project/writer.
ProjectModel::MaterialSourceReplacementReport ProjectModel::replaceNotesMaterialSources(
    const std::vector<MaterialSourceChoice>& choices,std::uint64_t expectedRevision,juce::String& error,bool apply)
{
    MaterialSourceReplacementReport report;error.clear();
    const auto reject=[&](const juce::String& why){report.rejected=true;report.changed=false;error=why;report.diagnostics.add(why);return report;};
    if(choices.empty())return reject("No score notes were selected for source replacement");
    const auto scoreFile=[](const juce::File& file){return file==juce::File{}||file.hasFileExtension("ust;ustx;mid;midi");};
    struct Snapshot{juce::String trackId;ClipData original;int velocity=100;};std::vector<Snapshot> snapshots;bool legacySingle=false;
    {
        const juce::ScopedLock guard(lock);if(revision!=expectedRevision)return reject("The project changed while choosing material sources");
        std::set<juce::String> ids;
        for(const auto& choice:choices)
        {
            if(choice.noteId.isEmpty()||!ids.insert(choice.noteId).second)return reject("Duplicate or missing score-note identity");
            const TrackData* track=nullptr;const ClipData* parent=nullptr;const NoteData* note=nullptr;int count=0;
            for(const auto& t:project.tracks)for(const auto& c:t.clips)for(const auto& n:c.notes)if(n.id==choice.noteId){track=&t;parent=&c;note=&n;++count;}
            if(count!=1||!track||!trackShowsAllNativeRegions(*track)||track->pitchAlgorithm==PitchAlgorithm::utau)return reject("Source replacement requires one exact native score-note adapter");
            const ClipData* owner=parent;if(!parent->parts.empty()){owner=nullptr;for(const auto& part:parent->parts)if(part.id==note->clipPartId){if(owner)return reject("Ambiguous source owner");owner=&part;}}
            if(!owner)return reject("The score note has no explicit source owner");
            if(!scoreFile(owner->sourceFile)&&!note->nativeScoreMaterialAligned)
            {if(choices.size()==1){legacySingle=true;break;}return reject("A multi-note score assignment cannot silently repartition legacy physical-source tuning");}
            if(note->materialLocalOverride)return reject("Clear the note-local material override explicitly before source replacement");
            if(std::none_of(snapshots.begin(),snapshots.end(),[&](const auto& item){return item.trackId==track->id&&item.original.id==parent->id;}))snapshots.push_back({track->id,*parent,track->utauConsonantVelocity});
        }
    }
    if(legacySingle){const auto& c=choices.front();if(!c.completeRows)return reject("Complete source annotations are required");return replaceNoteMaterialSource(c.noteId,c.audio,*c.completeRows,c.audioId,c.regionId,c.alias,error,apply);}
    struct Media{double duration=0;juce::String fingerprint;};std::map<juce::String,Media> media;
    juce::AudioFormatManager formats;formats.registerBasicFormats();
    const auto loadMedia=[&](const juce::File& file)->bool
    {
        const auto key=file.getFullPathName();if(media.count(key))return true;
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));if(!reader||reader->sampleRate<=0||reader->lengthInSamples<=0){error="A chosen physical recording cannot be read: "+key;return false;}
        const auto size=file.getSize();const auto modified=file.getLastModificationTime();Media facts{reader->lengthInSamples/reader->sampleRate,juce::SHA256(file).toHexString()};
        if(file.getSize()!=size||file.getLastModificationTime()!=modified){error="Recording changed during source preflight";return false;}media.emplace(key,std::move(facts));return true;
    };
    std::map<juce::String,SampleRegionSetting> requestedRows;
    for(const auto& c:choices)
    {
        if(!c.completeRows||c.audioId.isEmpty()||c.regionId.isEmpty()||!loadMedia(c.audio))return reject(error.isEmpty()?juce::String("Complete stable pronunciation identities are required"):error);
        OttoMaterialIO::Document doc;doc.audio={{c.audioId,c.audio.getFileName(),*c.completeRows}};if(!OttoMaterialIO::validate(doc,error))return reject(error);
        const auto row=std::find_if(c.completeRows->begin(),c.completeRows->end(),[&](const auto& r){return r.materialAudioId==c.audioId&&r.materialRegionId==c.regionId;});
        if(row==c.completeRows->end()||(!c.alias.isEmpty()&&c.alias!=row->name&&!row->additionalAliases.contains(c.alias)))return reject("Chosen alias/source row no longer belongs to the exact annotation variant");
        for(const auto& r:*c.completeRows)if(r.materialAudioId!=c.audioId||r.regionEndSeconds>media.at(c.audio.getFullPathName()).duration+1.e-8)return reject("Source annotations disagree with physical recording coverage");
        requestedRows.emplace(c.noteId,*row);
    }
    std::vector<ClipData> candidates;snapshots.shrink_to_fit();
    for(const auto& snapshot:snapshots)
    {
        const auto& original=snapshot.original;auto candidate=original;
        const auto ownerFor=[&](const NoteData& note)->const ClipData*{if(original.parts.empty())return &original;const ClipData* found=nullptr;for(const auto& part:original.parts)if(part.id==note.clipPartId){if(found)return nullptr;found=&part;}return found;};
        std::vector<NativeScoreMaterialSource> nodes;std::vector<juce::String> nodeIds;
        for(const auto& note:original.notes)
        {
            const auto choice=std::find_if(choices.begin(),choices.end(),[&](const auto& item){return item.noteId==note.id;});const bool selected=choice!=choices.end();
            if(!selected&&!note.nativeScoreMaterialAligned)continue;const auto* oldOwner=ownerFor(note);if(!oldOwner)return reject("An existing score-aligned note lost its exact source owner");
            NativeScoreMaterialSource node;node.noteId=note.id;node.scoreStart=note.startSeconds;node.scoreDuration=note.durationSeconds;node.preutteranceKnown=note.utauPreutteranceOverrideEnabled;node.targetPreutterance=note.utauPreutteranceSeconds;node.consonantVelocity=note.utauConsonantVelocity==inheritedUtauConsonantVelocity?snapshot.velocity:note.utauConsonantVelocity;
            if(selected){node.audio=choice->audio;node.audioId=choice->audioId;node.regionId=choice->regionId;node.row=requestedRows.at(note.id);node.completeRows=choice->completeRows;}
            else
            {
                if(!loadMedia(oldOwner->sourceFile))return reject(error);
                if(oldOwner->materialSourceFingerprint.isNotEmpty()&&oldOwner->materialSourceFingerprint!=media.at(oldOwner->sourceFile.getFullPathName()).fingerprint)return reject("An existing material recording changed; refresh its binding first");
                auto local=note;if(!original.parts.empty())local.startSeconds-=oldOwner->startSeconds;
                node.row=nativeNoteSourceTiming(local,*oldOwner,nativeClipClock(*oldOwner));node.row.name=note.materialBoundAlias.isEmpty()?note.label:note.materialBoundAlias;node.row.materialAudioId=note.materialAudioId;node.row.materialRegionId=note.materialRegionId;node.row.additionalAliases=note.materialAdditionalAliases;node.row.nativePronunciationEdges=note.nativePronunciationEdges;node.row.nativeTransition=note.nativeTransition;node.row.role=note.nativeRole;node.row.provenance=note.nativeProvenance;node.row.confidence=note.nativeConfidence;node.row.nativeUnpitched=note.nativeUnpitched;
                node.audio=oldOwner->sourceFile;node.audioId=note.materialAudioId;node.regionId=note.materialRegionId;node.retainedNextRegion=note.materialConnectedNextRegionId;
                for(const auto& c:choices)if(c.audio==node.audio&&c.audioId==node.audioId){node.completeRows=c.completeRows;break;}
            }
            nodes.push_back(std::move(node));nodeIds.push_back(note.id);
        }
        std::vector<NativeScoreMaterialGroup> groups;if(!planNativeScoreMaterialClocks(nodes,groups,error))return reject(error);
        if(candidate.parts.empty())
        {
            auto score=original;score.id=makeId("score-owner");score.startSeconds=0;score.notes.clear();score.parts.clear();score.sourceFile=juce::File{};score.gain=1;score.gainEnvelope.clear();score.inheritedGainEnvelopes.clear();score.fadeInSeconds=score.fadeOutSeconds=score.crossfadeInSeconds=score.crossfadeOutSeconds=0;
            candidate.parts={score};for(auto& note:candidate.notes)note.clipPartId=score.id;
        }
        else
        {
            for(const auto& part:original.parts)if(!scoreFile(part.sourceFile))
            {
                bool rebuilding=false,retaining=false;for(const auto& note:original.notes)if(note.clipPartId==part.id){const bool planned=std::find(nodeIds.begin(),nodeIds.end(),note.id)!=nodeIds.end();rebuilding|=planned;retaining|=!planned;}
                if(rebuilding&&retaining)return reject("Mixed legacy and score-aligned notes share a source owner; separate that ownership explicitly first");
            }
            std::erase_if(candidate.parts,[&](const auto& part){return std::any_of(original.notes.begin(),original.notes.end(),[&](const auto& note){return note.clipPartId==part.id&&std::find(nodeIds.begin(),nodeIds.end(),note.id)!=nodeIds.end();})&&std::none_of(original.notes.begin(),original.notes.end(),[&](const auto& note){return note.clipPartId==part.id&&std::find(nodeIds.begin(),nodeIds.end(),note.id)==nodeIds.end();});});
        }
        for(const auto& group:groups)
        {
            const auto& first=nodes[group.members.front()];const auto& facts=media.at(first.audio.getFullPathName());
            ClipData owner;owner.id=makeId("score-material");owner.sourceFile=first.audio;owner.startSeconds=group.targetBegin;owner.durationSeconds=group.targetEnd-group.targetBegin;owner.sourceOffsetSeconds=group.sourceBegin;owner.sourceDurationSeconds=group.sourceEnd-group.sourceBegin;owner.materialSourceFingerprint=facts.fingerprint;
            for(const auto& p:group.clock)owner.sourceTimeMap.push_back({p.targetSeconds-group.targetBegin,p.sourceSeconds-group.sourceBegin});
            bool allUnpitched=true;for(const auto index:group.members)allUnpitched&=nodes[index].row.nativeUnpitched;
            for(const auto& old:original.parts)if(old.sourceFile==owner.sourceFile&&old.materialSourceFingerprint==facts.fingerprint&&old.nativeSourcePitchComplete&&old.nativeSourcePitch){owner.nativeSourcePitch=old.nativeSourcePitch;owner.nativeSourcePitchComplete=true;break;}
            const bool chosenInGroup=std::any_of(group.members.begin(),group.members.end(),[&](const auto index){return std::any_of(choices.begin(),choices.end(),[&](const auto& choice){return choice.noteId==nodes[index].noteId;});});
            // Rebinding a note resets its source contour even when its owner can
            // reuse measured file facts. A ticket reapplies those facts from the
            // existing cache; never leave a new note unmeasured in a ready owner.
            owner.materialSourceAnalysisPending=!allUnpitched&&(!owner.nativeSourcePitchComplete||chosenInGroup);owner.nativeSourcePitchPending=owner.materialSourceAnalysisPending;owner.materialSourceResumeMuted=false;owner.muted=owner.materialSourceAnalysisPending;
            for(std::size_t member=0;member<group.members.size();++member)
            {
                const auto& node=nodes[group.members[member]];auto target=std::find_if(candidate.notes.begin(),candidate.notes.end(),[&](const auto& note){return note.id==node.noteId;});const auto before=std::find_if(original.notes.begin(),original.notes.end(),[&](const auto& note){return note.id==node.noteId;});if(target==candidate.notes.end()||before==original.notes.end())return reject("Score-note identity changed during pure source planning");
                target->clipPartId=owner.id;target->nativeScoreMaterialAligned=true;
                const auto choice=std::find_if(choices.begin(),choices.end(),[&](const auto& c){return c.noteId==node.noteId;});
                if(choice!=choices.end())
                {
                    const auto& row=node.row;const auto alias=choice->alias.isEmpty()?row.name:choice->alias;
                    const bool keepAuthored=before->nativeScoreMaterialAligned&&(before->nativePitchHandlesPlaced
                        ||(before->nativeSourceTrendTranspose&&hasAuthoredNativeSourceTarget(*before)));
                    if(keepAuthored)for(auto& p:target->contour){p.manualTargetCents=renderedPitchCents(*before,p);p.hasManualTarget=true;}
                    else
                    {
                        // A protocol PB/PBW/PBY/VBR curve is not this source's
                        // singing. Source analysis supplies the measured detail;
                        // only its centre moves to the unchanged score pitch.
                        target->contour.clear();target->pitchControlPoints.clear();target->nativePitchHandlesPlaced=false;
                        target->drift=target->modulation=1.0f;target->vibratoEnabled=false;target->utauAutoPitchTransition=false;
                    }
                    target->nativeSourceTrendTranspose=true;
                    target->materialTargetPitchLocked=true;target->sourceMidiCenter=-1;target->sourcePitchMeasured=false;
                    target->materialAudioId=node.audioId;target->materialRegionId=node.regionId;target->materialBoundAlias=alias;target->materialAliasFollowsPrimary=alias==row.name;target->materialLabelFollowsAlias=before->materialLabelFollowsAlias&&before->label==alias;
                    target->materialSourceStartSeconds=target->nativeSourceStartSeconds=row.regionStartSeconds;target->materialSourceEndSeconds=target->nativeSourceEndSeconds=row.regionEndSeconds;target->nativeSegments=SampleSettings::nativeSegmentsFor(row);target->nativeRole=row.role;target->nativeProvenance=row.provenance;target->nativeConfidence=row.confidence;target->nativeUnpitched=row.nativeUnpitched;target->nativePronunciationEdges=row.nativePronunciationEdges;target->nativeTransition=row.nativeTransition;target->materialAdditionalAliases=row.additionalAliases;target->pronunciationKind=row.pronunciationKind;target->simplifiedConsonant=row.simplifiedConsonant;target->vowelSplitsExplicit=row.vowelSplitsExplicit;target->materialReferenceUnresolved=false;
                }
                target->materialConnectedNextRegionId=member+1<group.members.size()?nodes[group.members[member+1]].regionId:juce::String{};
            }
            if(owner.materialSourceAnalysisPending){const auto note=nodes[group.members.front()].noteId;report.tickets.push_back({note,original.id,owner.id,owner.sourceFile,facts.fingerprint});}
            candidate.parts.push_back(std::move(owner));
        }
        std::erase_if(candidate.parts,[&](const auto& owner){return scoreFile(owner.sourceFile)&&std::none_of(candidate.notes.begin(),candidate.notes.end(),[&](const auto& note){return note.clipPartId==owner.id;});});
        candidate.nativeAudioLinked=true;candidates.push_back(std::move(candidate));
    }
    {
        const juce::ScopedLock guard(lock);if(revision!=expectedRevision)return reject("The score changed while material source files were being validated");
        std::vector<ClipData*> destinations;for(const auto& snapshot:snapshots){ClipData* found=nullptr;for(auto& track:project.tracks)if(track.id==snapshot.trackId)for(auto& parent:track.clips)if(parent.id==snapshot.original.id){if(found)return reject("Ambiguous destination source parent");found=&parent;}if(!found)return reject("The destination score parent disappeared");destinations.push_back(found);}
        report.changed=!candidates.empty();report.pendingAnalysis=!report.tickets.empty();if(!candidates.empty())report.clipId=candidates.front().id;if(!report.tickets.empty()){report.ticket=report.tickets.front();report.sourcePartId=report.ticket.sourcePartId;}
        if(apply&&report.changed){pushUndoLocked();for(std::size_t i=0;i<candidates.size();++i)*destinations[i]=std::move(candidates[i]);}
    }
    if(apply&&report.changed)sendChangeMessage();return report;
}
