#include "JapaneseAnnotationBatch.h"
#include <juce_cryptography/juce_cryptography.h>
#include <set>

namespace hachi
{
bool JapaneseAnnotationBatch::preview(const Input& input,Plan& output,juce::String& error,
    const backend::AnnotationCancel& cancel,const std::function<void(const juce::String&)>& progress)
{
    error.clear();Plan plan;plan.root=input.root;plan.mode=input.mode;
    if(!JapaneseAutoAnnotation::planMode(input.mode,plan.stagePlan,error))return false;
    if(!plan.root.isDirectory()){error="Choose an existing material folder";return false;}
    plan.manifest=plan.root.getChildFile("otto.ini");
    // Recognition-only never constructs a publication candidate and must not
    // require a valid/migrated annotation document merely to obtain text.
    if(plan.stagePlan.canPublish)
    {
        if(plan.manifest.existsAsFile())
        {if(!OttoMaterialIO::loadDocument(plan.manifest,plan.candidate,error))return false;}
        else for(const auto& entry:juce::RangedDirectoryIterator(plan.root,true,"*.ini",juce::File::findFiles))
        {
            const auto name=entry.getFile().getFileName().toLowerCase();
            if(name=="oto.ini"||name=="oto.jie.ini"||name=="oto4.ini"||name=="otomou.ini")
            {error="Migrate the legacy OTO folder with the existing migration action before Japanese annotation; original annotations will not be shadowed";return false;}
        }
    }
    auto files=input.recordings;
    if(files.empty())
    {
        for(const auto& entry:juce::RangedDirectoryIterator(plan.root,true,"*",juce::File::findFiles))
        {
            if(cancel&&cancel()){error="Annotation cancelled";return false;}
            const auto file=entry.getFile();
            if(file.hasFileExtension("wav;flac;aif;aiff"))files.push_back(file);
            if(files.size()>10000){error="Annotation folder contains too many recordings";return false;}
        }
        std::sort(files.begin(),files.end(),[](const auto& a,const auto& b){return a.getFullPathName()<b.getFullPathName();});
    }
    if(files.empty()){error="No supported recordings in the selected folder";return false;}
    const bool manualMode=input.mode==JapaneseAutoAnnotation::InferenceMode::alignOnly;
    if(manualMode&&!input.correctedTokens.isEmpty()&&files.size()!=1)
    {error="Explicit transcript correction is only available for the current recording";return false;}
    if(input.focusAudio!=juce::File{})
    {
        if(std::find(files.begin(),files.end(),input.focusAudio)==files.end())
        {error="The focused recording is outside this inference request";return false;}
        plan.focusAudio=input.focusAudio;
    }
    else if(files.size()==1)plan.focusAudio=files.front();
    std::set<juce::String> visited;
    for(std::size_t i=0;i<files.size();++i)
    {
        if(cancel&&cancel()){error="Annotation cancelled";return false;}
        const auto& file=files[i];
        if(!file.existsAsFile()||!file.isAChildOf(plan.root))
        {error="Recording is missing or outside the selected material folder";return false;}
        if(!visited.insert(file.getFullPathName().toLowerCase()).second)
        {error="Duplicate recording in annotation request";return false;}
        juce::StringArray manual;
        if(manualMode)
        {
            const auto found=input.manualInputs.find(file.getFullPathName());
            if(found!=input.manualInputs.end())manual=found->second;
            else if(files.size()==1)manual=input.correctedTokens;
        }
        JapaneseAutoAnnotation::Result result,lastStages;
        const auto recognized=[&](const JapaneseAutoAnnotation::Result& snapshot)
        {
            if(snapshot.normalizedSyllables.isEmpty())return;
            const auto found=std::find_if(plan.recognitions.begin(),plan.recognitions.end(),[&](const auto& old){return old.audio==snapshot.audio;});
            if(found==plan.recognitions.end())plan.recognitions.push_back(snapshot);else *found=snapshot;
        };
        const bool inferred=JapaneseAutoAnnotation::infer(file,input.modelRoot,input.execution,input.mode,manual,result,error,cancel,
            [&](const juce::String& stage){if(progress)progress(juce::String(int(i+1))+"/"+juce::String(int(files.size()))+" "+file.getFileName()+": "+stage);},
            recognized,[&](const JapaneseAutoAnnotation::Result& snapshot){lastStages=snapshot;});
        plan.audioLoadCount+=lastStages.audioLoadCount;plan.asrCallCount+=lastStages.asrCallCount;plan.alignmentCallCount+=lastStages.alignmentCallCount;
        if(!inferred)
        {
            if(cancel&&cancel()){error="Annotation cancelled";return false;}
            plan.failures.add(file.getRelativePathFrom(plan.root)+": "+error);error.clear();continue;
        }
        plan.report+=file.getRelativePathFrom(plan.root)+"\nASR raw: "+result.recognizedTokens.joinIntoString(" ")
            +"\nSyllables: "+result.normalizedSyllables.joinIntoString(" ")+"\nPhones: "+result.sequence.phones.joinIntoString(" ")+"\n";
        if(!plan.stagePlan.canPublish)
        {
            plan.report+="Recognition-only: no alignment, rows or OTTO publication candidate. ASR="+result.asrBackend+"\n\n";
            continue;
        }
        if(!result.ready||result.rows.empty()){error="Inference did not produce validated aligned annotation rows";return false;}
        // Explicit aligned operations add siblings, never overwrite authored rows.
        if(std::any_of(plan.candidate.audio.begin(),plan.candidate.audio.end(),[&](const auto& old){return old.id==result.audioId;}))
            result.audioId+="_"+juce::Uuid().toString().removeCharacters("-").substring(0,12);
        for(auto& row:result.rows)row.materialAudioId=result.audioId;
        if(!OttoMaterialIO::replaceAudio(plan.candidate,plan.manifest,file,result.rows,error,result.audioId))return false;
        for(const auto& row:result.rows)
            plan.report+=row.name+" ["+juce::String(row.regionStartSeconds,3)+", "+juce::String(row.regionEndSeconds,3)
                +"] P "+juce::String(row.alignmentSeconds,3)+" C "+juce::String(row.regionStartSeconds+row.fixedDurationSeconds,3)
                +" overlap "+juce::String(row.overlapSeconds,3)+"\n";
        plan.report+="ASR="+result.asrBackend+"; TIFA="+result.alignmentBackend+"\n\n";
        plan.results.push_back(std::move(result));
    }
    if(!plan.failures.isEmpty())plan.report+="NOT PUBLISHED / NEEDS REVIEW:\n"+plan.failures.joinIntoString("\n")+"\n";
    if(plan.results.empty()&&plan.recognitions.empty())
    {error=plan.failures.isEmpty()?juce::String("No recognition/alignment results were produced"):plan.failures.joinIntoString("\n");return false;}
    if(!plan.results.empty())
    {if(!OttoMaterialIO::validate(plan.candidate,error))return false;plan.ready=true;}
    // Useful normalized text survives TIFA failure, but cannot enable Publish.
    if(plan.results.empty()&&!plan.recognitions.empty()&&plan.stagePlan.runAlignment)
        plan.report+="Recognized/normalized text is available for correction; alignment has not produced publishable rows.\n";
    output=std::move(plan);return true;
}

bool JapaneseAnnotationBatch::publish(Plan& plan,juce::String& error,const backend::AnnotationCancel& cancel)
{
    if(plan.mode==JapaneseAutoAnnotation::InferenceMode::recognizeOnly
        ||std::any_of(plan.results.begin(),plan.results.end(),[](const auto& result){return result.mode==JapaneseAutoAnnotation::InferenceMode::recognizeOnly;}))
    {error="Recognition-only results cannot publish OTTO annotations; run alignment first";return false;}
    if(!plan.ready||plan.results.empty()){error="Create a fresh aligned annotation preview first";return false;}
    for(const auto& result:plan.results)
    {
        if(cancel&&cancel()){error="Annotation cancelled";return false;}
        if(!result.audio.existsAsFile()||juce::SHA256(result.audio).toHexString()!=result.sourceFingerprint)
        {error="Recording changed since preview; no annotations were published";return false;}
    }
    if(cancel&&cancel()){error="Annotation cancelled";return false;}
    if(!OttoMaterialIO::saveDocument(plan.manifest,plan.candidate,error))return false;
    plan.ready=false;return true;
}
}
