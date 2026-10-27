#pragma once
#include "NativeConsonantMergeDetection.h"
#include "NativeVirtualAutoConnect.h"
namespace hachi
{
struct MelodyneImportOptimizationOptions
{
    bool mergeConsonants=false,virtualConnections=false,virtualPitch=false;
    bool registerTransitions=true; // existing annotation policy, not an audio optimization
};
inline bool readMelodyneImportOptimizationOptions(const juce::var& args,MelodyneImportOptimizationOptions& output,juce::String& error)
{
    error.clear();MelodyneImportOptimizationOptions options;
    for(const auto& [key,target]:std::initializer_list<std::pair<const char*,bool*>>{
        {"auto_merge_consonants",&options.mergeConsonants},{"auto_virtual_connections",&options.virtualConnections},
        {"auto_virtual_pitch",&options.virtualPitch},{"auto_register_transitions",&options.registerTransitions}})
    {
        if(!args.hasProperty(key))continue;
        const auto value=args.getProperty(key,{});if(!value.isBool()){error=juce::String(key)+" must be a boolean";return false;}*target=bool(value);
    }
    output=options;return true;
}
// Shared entry stages: capture original P before conversion; route connections
// only after the caller has finalized track configuration. All commits still
// delegate to the existing Model authorities. No material publication here.
inline NativeConsonantAutoMergeReport prepareMelodyneImportOptimization(ProjectData& project,
    const std::vector<NativeSplitTransitionEvidence>& joins,const MelodyneImportOptimizationOptions& options)
{
    if(options.registerTransitions)annotateMelodyneSplitTransitions(project,detectMelodyneSplitTransitions(project,joins));
    return autoMergeMelodyneConsonants(project,joins,options.mergeConsonants);
}
inline NativeVirtualAutoConnectReport finishMelodyneImportOptimization(ProjectData& project,
    const std::vector<NativeSplitTransitionEvidence>& joins,const MelodyneImportOptimizationOptions& options)
{
    if(options.registerTransitions)annotateMelodyneSplitTransitions(project,detectMelodyneSplitTransitions(project,joins));
    return autoConnectNativeVirtualImport(project,{options.virtualConnections,options.virtualPitch},joins);
}
}
