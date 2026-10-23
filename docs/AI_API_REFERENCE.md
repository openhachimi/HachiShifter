# 实际接口声明清单（自动生成）

核对日期：2026-10-16。配套解读：[功能主入口与架构审计](AI_INTERFACE_ARCHITECTURE.md)。

这是源文件导航，不是承诺稳定的公共 SDK。仅枚举下列 11 个边界类的公开方法，不把 private 函数、UI 回调、纯计算辅助函数或每个第三方函数算作产品接口。重载各列一行；诊断接口单独标记。源码行号为生成时位置。

生成命令：`python tools/generate-interface-reference.py`。词法提取不是 C++ 编译器，不能用条目数证明所有业务路径正确；新增边界类时需更新脚本 HEADERS。

## ProjectModel（171 个声明）

| 实际签名 | 类型 | 定义入口 |
| --- | --- | --- |
| `ProjectModel()` | 生命周期 | [ProjectModel.h:827](../juce/src/ProjectModel.h#L827) |
| `ProjectData snapshot() const` | 业务／适配 | [ProjectModel.h:829](../juce/src/ProjectModel.h#L829) |
| `std::uint64_t revisionNumber() const` | 业务／适配 | [ProjectModel.h:830](../juce/src/ProjectModel.h#L830) |
| `void replace(ProjectData replacement)` | 业务／适配 | [ProjectModel.h:831](../juce/src/ProjectModel.h#L831) |
| `void resetDocument(ProjectData replacement = {})` | 业务／适配 | [ProjectModel.h:833](../juce/src/ProjectModel.h#L833) |
| `void clear()` | 业务／适配 | [ProjectModel.h:834](../juce/src/ProjectModel.h#L834) |
| `void setAssetManagerVisible(bool visible)` | 业务／适配 | [ProjectModel.h:836](../juce/src/ProjectModel.h#L836) |
| `bool setHamoodState(const juce::String& state, juce::String& error)` | 业务／适配 | [ProjectModel.h:837](../juce/src/ProjectModel.h#L837) |
| `bool undo()` | 业务／适配 | [ProjectModel.h:838](../juce/src/ProjectModel.h#L838) |
| `bool redo()` | 业务／适配 | [ProjectModel.h:839](../juce/src/ProjectModel.h#L839) |
| `bool canUndo() const` | 业务／适配 | [ProjectModel.h:840](../juce/src/ProjectModel.h#L840) |
| `bool canRedo() const` | 业务／适配 | [ProjectModel.h:841](../juce/src/ProjectModel.h#L841) |
| `juce::String addAudioFile(const juce::File& file, double durationSeconds, double startSeconds = 0.0, const juce::String& targetTrackId = {}, const VoicebankOtoEntry* oto = nullptr, UtauMode otoMode = UtauMode::classic)` | 业务／适配 | [ProjectModel.h:843](../juce/src/ProjectModel.h#L843) |
| `juce::String addTrack(const juce::String& name, bool compose = true, bool referenceOnly = false, bool accompaniment = false)` | 业务／适配 | [ProjectModel.h:848](../juce/src/ProjectModel.h#L848) |
| `void setTrackReferenceOnly(const juce::String& trackId, bool referenceOnly)` | 业务／适配 | [ProjectModel.h:850](../juce/src/ProjectModel.h#L850) |
| `void setTrackName(const juce::String& trackId, const juce::String& name)` | 业务／适配 | [ProjectModel.h:851](../juce/src/ProjectModel.h#L851) |
| `bool setClipNotesIfEmpty(const juce::String& clipId, std::vector<NoteData> notes)` | 业务／适配 | [ProjectModel.h:852](../juce/src/ProjectModel.h#L852) |
| `std::size_t markNativeUnpitchedRegions(const juce::String& clipId)` | 业务／适配 | [ProjectModel.h:853](../juce/src/ProjectModel.h#L853) |
| `bool setNativeTrimSourceAnalysis(const juce::File& file, const std::vector<NoteData>& sourceNotes)` | 业务／适配 | [ProjectModel.h:854](../juce/src/ProjectModel.h#L854) |
| `bool setClipAudioAnalysis(const juce::String& clipId, std::vector<NoteData> notes, const ClipData& importedClip = ClipData{})` | 业务／适配 | [ProjectModel.h:855](../juce/src/ProjectModel.h#L855) |
| `bool addMidiFile(const juce::File& file, juce::String& error)` | 业务／适配 | [ProjectModel.h:857](../juce/src/ProjectModel.h#L857) |
| `static std::vector<MidiTrackChoice> midiTrackChoices(const juce::File& file, juce::String& error)` | 业务／适配 | [ProjectModel.h:867](../juce/src/ProjectModel.h#L867) |
| `juce::String addMidiTrack(const juce::File& file, int trackIndex, juce::String& error)` | 业务／适配 | [ProjectModel.h:874](../juce/src/ProjectModel.h#L874) |
| `static bool writeMidiFile(const ProjectData& data, const juce::File& file, juce::String& error)` | 业务／适配 | [ProjectModel.h:880](../juce/src/ProjectModel.h#L880) |
| `bool addUstFile(const juce::File& file, juce::String& error, juce::StringArray& warnings, UstImportMode mode = UstImportMode::addTrack, juce::String* importedTrackId = nullptr, int encoding = 0)` | 业务／适配 | [ProjectModel.h:896](../juce/src/ProjectModel.h#L896) |
| `bool exportUst(const juce::File& file, const juce::String& trackId, juce::String& error, juce::StringArray& warnings, int encoding = 0) const` | 业务／适配 | [ProjectModel.h:900](../juce/src/ProjectModel.h#L900) |
| `void setTempo(double bpm, int numerator, int denominator = 4)` | 业务／适配 | [ProjectModel.h:902](../juce/src/ProjectModel.h#L902) |
| `void setTempoChange(double quarterPosition, double bpm, bool synchronizeNotes = true)` | 业务／适配 | [ProjectModel.h:905](../juce/src/ProjectModel.h#L905) |
| `juce::int64 contentFingerprint() const` | 业务／适配 | [ProjectModel.h:909](../juce/src/ProjectModel.h#L909) |
| `void setGridDivision(const juce::String& division)` | 业务／适配 | [ProjectModel.h:910](../juce/src/ProjectModel.h#L910) |
| `void setNoteEditDivision(int division)` | 业务／适配 | [ProjectModel.h:911](../juce/src/ProjectModel.h#L911) |
| `void setBaseScale(const juce::String& scale)` | 业务／适配 | [ProjectModel.h:912](../juce/src/ProjectModel.h#L912) |
| `void setTrackCompose(const juce::String& trackId, bool enabled)` | 业务／适配 | [ProjectModel.h:913](../juce/src/ProjectModel.h#L913) |
| `void setTrackMuted(const juce::String& trackId, bool muted)` | 业务／适配 | [ProjectModel.h:914](../juce/src/ProjectModel.h#L914) |
| `void setTrackSolo(const juce::String& trackId, bool solo)` | 业务／适配 | [ProjectModel.h:915](../juce/src/ProjectModel.h#L915) |
| `void setTrackVolume(const juce::String& trackId, float volume)` | 业务／适配 | [ProjectModel.h:916](../juce/src/ProjectModel.h#L916) |
| `void setClipGainEnvelope(const juce::String& clipId, std::vector<GainEnvelopePoint> points)` | 业务／适配 | [ProjectModel.h:917](../juce/src/ProjectModel.h#L917) |
| `void setTrackPan(const juce::String& trackId, float pan)` | 业务／适配 | [ProjectModel.h:918](../juce/src/ProjectModel.h#L918) |
| `void setTrackSmoothOverlaps(const juce::String& trackId, bool enabled)` | 业务／适配 | [ProjectModel.h:919](../juce/src/ProjectModel.h#L919) |
| `void setTrackNormalizeVolume(const juce::String& trackId, bool enabled)` | 业务／适配 | [ProjectModel.h:920](../juce/src/ProjectModel.h#L920) |
| `void setTrackNsfSmoothPitchTransitions(const juce::String& trackId, bool enabled)` | 业务／适配 | [ProjectModel.h:921](../juce/src/ProjectModel.h#L921) |
| `void setTrackNsfNoiseProtection(const juce::String& trackId, bool enabled)` | 业务／适配 | [ProjectModel.h:922](../juce/src/ProjectModel.h#L922) |
| `void setTrackVoicebankDirectory(const juce::String& trackId, const juce::File& directory)` | 业务／适配 | [ProjectModel.h:923](../juce/src/ProjectModel.h#L923) |
| `void setTrackUtauConsonantVelocity(const juce::String& trackId, int velocity)` | 业务／适配 | [ProjectModel.h:925](../juce/src/ProjectModel.h#L925) |
| `void setTrackUtauGlobalFlags(const juce::String& trackId, const juce::String& flags)` | 业务／适配 | [ProjectModel.h:926](../juce/src/ProjectModel.h#L926) |
| `void setUtauMode(UtauMode mode)` | 业务／适配 | [ProjectModel.h:927](../juce/src/ProjectModel.h#L927) |
| `void setTrackUtauMode(const juce::String& trackId, UtauMode mode)` | 业务／适配 | [ProjectModel.h:928](../juce/src/ProjectModel.h#L928) |
| `void setTrackChineseCvvc(const juce::String& trackId, bool enabled)` | 业务／适配 | [ProjectModel.h:929](../juce/src/ProjectModel.h#L929) |
| `void setTrackCvvcCurveCrossfade(const juce::String& trackId, bool curved)` | 业务／适配 | [ProjectModel.h:930](../juce/src/ProjectModel.h#L930) |
| `void setTrackOutputEngine(const juce::String& trackId, UtauOutputEngine engine, const juce::File& resampler = {}, const juce::File& wavtool = {})` | 业务／适配 | [ProjectModel.h:931](../juce/src/ProjectModel.h#L931) |
| `void setNotesVibrato(const std::vector<juce::String>& noteIds, const NoteData& parameters, bool enabled, std::optional<bool> realLine = std::nullopt)` | 业务／适配 | [ProjectModel.h:933](../juce/src/ProjectModel.h#L933) |
| `void setNotesVibratoRealLine(const std::vector<juce::String>& noteIds, bool enabled)` | 业务／适配 | [ProjectModel.h:936](../juce/src/ProjectModel.h#L936) |
| `bool bakeNoteVibratoIntoPitch(const juce::String& noteId)` | 业务／适配 | [ProjectModel.h:940](../juce/src/ProjectModel.h#L940) |
| `void setNotesUtauSplice(const std::vector<juce::String>& noteIds, bool enabled)` | 业务／适配 | [ProjectModel.h:941](../juce/src/ProjectModel.h#L941) |
| `bool setPointCurves(std::vector<PointCurveEdit> edits, std::uint64_t expectedRevision)` | 业务／适配 | [ProjectModel.h:954](../juce/src/ProjectModel.h#L954) |
| `bool resetCvvcBoundaries(const std::vector<juce::String>& noteIds, std::uint64_t expectedRevision)` | 业务／适配 | [ProjectModel.h:956](../juce/src/ProjectModel.h#L956) |
| `bool setCvvcBoundary(const juce::String& noteId, int rightPart, double position, std::uint64_t expectedRevision)` | 业务／适配 | [ProjectModel.h:957](../juce/src/ProjectModel.h#L957) |
| `bool setCvvcEnvelopes(std::vector<CvvcEnvelopeEdit>, std::uint64_t expectedRevision)` | 业务／适配 | [ProjectModel.h:958](../juce/src/ProjectModel.h#L958) |
| `bool setCvvcEnvelope(const juce::String& noteId, int part, const juce::String& flag, backend::CvvcEnvelopeCurve points, std::uint64_t expectedRevision)` | 业务／适配 | [ProjectModel.h:959](../juce/src/ProjectModel.h#L959) |
| `void setNotesUtauFlagCurveEnabled(const std::vector<juce::String>& noteIds, bool enabled)` | 业务／适配 | [ProjectModel.h:961](../juce/src/ProjectModel.h#L961) |
| `bool resetNotesUtauFlagCurves(const std::vector<juce::String>& noteIds)` | 业务／适配 | [ProjectModel.h:968](../juce/src/ProjectModel.h#L968) |
| `bool resetNotesUtauFlagCurve(const std::vector<juce::String>& noteIds, const juce::String& flag)` | 业务／适配 | [ProjectModel.h:970](../juce/src/ProjectModel.h#L970) |
| `bool setNoteUtauFlagCurves(const juce::String& flag, const std::map<juce::String, std::vector<FlagCurvePoint>>& edits, std::optional<std::uint64_t> expectedRevision = std::nullopt, bool enabledOnly = true)` | 业务／适配 | [ProjectModel.h:974](../juce/src/ProjectModel.h#L974) |
| `bool setNoteUtauFlagCurve(const juce::String& noteId, const juce::String& flag, std::vector<FlagCurvePoint> points)` | 业务／适配 | [ProjectModel.h:977](../juce/src/ProjectModel.h#L977) |
| `void setNotesRegionFlags(const std::vector<juce::String>& noteIds, bool split, const juce::String& first, const juce::String& second, const juce::String& third, const juce::String& fourth)` | 业务／适配 | [ProjectModel.h:979](../juce/src/ProjectModel.h#L979) |
| `void setNotesUtauJieSplitCleared(const std::vector<juce::String>& noteIds)` | 业务／适配 | [ProjectModel.h:983](../juce/src/ProjectModel.h#L983) |
| `void setNotesUtauJieSplit(const std::vector<juce::String>& noteIds, double first, double second, double third)` | 业务／适配 | [ProjectModel.h:984](../juce/src/ProjectModel.h#L984) |
| `void clearNotesUtauJieSplit(const std::vector<juce::String>& noteIds)` | 业务／适配 | [ProjectModel.h:986](../juce/src/ProjectModel.h#L986) |
| `void setTrackPitchAlgorithm(const juce::String& trackId, PitchAlgorithm algorithm)` | 业务／适配 | [ProjectModel.h:987](../juce/src/ProjectModel.h#L987) |
| `void setTrackStretchAlgorithm(const juce::String& trackId, StretchAlgorithm algorithm)` | 业务／适配 | [ProjectModel.h:988](../juce/src/ProjectModel.h#L988) |
| `void setPitchAlgorithm(PitchAlgorithm algorithm)` | 业务／适配 | [ProjectModel.h:989](../juce/src/ProjectModel.h#L989) |
| `void setStretchAlgorithm(StretchAlgorithm algorithm)` | 业务／适配 | [ProjectModel.h:990](../juce/src/ProjectModel.h#L990) |
| `void setTrackRenderOrder(const juce::String& trackId, RenderOrder order)` | 业务／适配 | [ProjectModel.h:991](../juce/src/ProjectModel.h#L991) |
| `void setRenderOrder(RenderOrder order)` | 业务／适配 | [ProjectModel.h:992](../juce/src/ProjectModel.h#L992) |
| `void moveClip(const juce::String& clipId, double startSeconds)` | 业务／适配 | [ProjectModel.h:993](../juce/src/ProjectModel.h#L993) |
| `bool canSplitClip(const juce::String& clipId, double absoluteSeconds) const` | 业务／适配 | [ProjectModel.h:994](../juce/src/ProjectModel.h#L994) |
| `juce::String splitClip(const juce::String& clipId, double absoluteSeconds)` | 业务／适配 | [ProjectModel.h:997](../juce/src/ProjectModel.h#L997) |
| `juce::String duplicateClip(const juce::String& clipId, double startSeconds = -1.0, const juce::String& targetTrackId = {})` | 业务／适配 | [ProjectModel.h:998](../juce/src/ProjectModel.h#L998) |
| `void resizeClip(const juce::String& clipId, double startSeconds, double durationSeconds)` | 业务／适配 | [ProjectModel.h:1001](../juce/src/ProjectModel.h#L1001) |
| `bool trimClip(const juce::String& clipId, double startSeconds, double durationSeconds)` | 业务／适配 | [ProjectModel.h:1003](../juce/src/ProjectModel.h#L1003) |
| `void setClipGain(const juce::String& clipId, float gain)` | 业务／适配 | [ProjectModel.h:1004](../juce/src/ProjectModel.h#L1004) |
| `void setClipFades(const juce::String& clipId, double fadeInSeconds, double fadeOutSeconds)` | 业务／适配 | [ProjectModel.h:1005](../juce/src/ProjectModel.h#L1005) |
| `void setClipMuted(const juce::String& clipId, bool muted)` | 业务／适配 | [ProjectModel.h:1007](../juce/src/ProjectModel.h#L1007) |
| `void setClipNoteHints(const juce::String& clipId, bool enabled)` | 业务／适配 | [ProjectModel.h:1008](../juce/src/ProjectModel.h#L1008) |
| `void setClipNormalDisplay(const juce::String& clipId, bool enabled)` | 业务／适配 | [ProjectModel.h:1009](../juce/src/ProjectModel.h#L1009) |
| `void removeClip(const juce::String& clipId)` | 业务／适配 | [ProjectModel.h:1010](../juce/src/ProjectModel.h#L1010) |
| `void removeClips(const std::vector<juce::String>& clipIds)` | 业务／适配 | [ProjectModel.h:1011](../juce/src/ProjectModel.h#L1011) |
| `bool canMergeClips(const std::vector<juce::String>& clipIds) const` | 业务／适配 | [ProjectModel.h:1012](../juce/src/ProjectModel.h#L1012) |
| `juce::String mergeClips(const std::vector<juce::String>& clipIds)` | 业务／适配 | [ProjectModel.h:1013](../juce/src/ProjectModel.h#L1013) |
| `void moveClips(const std::vector<juce::String>& clipIds, double deltaSeconds, int trackDelta = 0)` | 业务／适配 | [ProjectModel.h:1014](../juce/src/ProjectModel.h#L1014) |
| `void removeTrack(const juce::String& trackId)` | 业务／适配 | [ProjectModel.h:1015](../juce/src/ProjectModel.h#L1015) |
| `void transposeNote(const juce::String& noteId, float semitones)` | 业务／适配 | [ProjectModel.h:1016](../juce/src/ProjectModel.h#L1016) |
| `void transposeNotes(const std::vector<juce::String>& noteIds, float semitones)` | 业务／适配 | [ProjectModel.h:1017](../juce/src/ProjectModel.h#L1017) |
| `bool moveUtauNotes(const std::vector<juce::String>& noteIds, double deltaSeconds, float semitones, juce::String* error = nullptr)` | 业务／适配 | [ProjectModel.h:1021](../juce/src/ProjectModel.h#L1021) |
| `bool moveNativeNotes(const std::vector<juce::String>& noteIds, double deltaSeconds, float semitones, juce::String* error = nullptr)` | 业务／适配 | [ProjectModel.h:1025](../juce/src/ProjectModel.h#L1025) |
| `bool trimNativeNoteEdge(const juce::String& noteId, double deltaSeconds, bool leftEdge)` | 业务／适配 | [ProjectModel.h:1027](../juce/src/ProjectModel.h#L1027) |
| `bool resizeNativeNoteEdge(const juce::String& noteId, double deltaSeconds, bool leftEdge)` | 业务／适配 | [ProjectModel.h:1028](../juce/src/ProjectModel.h#L1028) |
| `void setNotesMidi(const std::vector<juce::String>& noteIds, float midiNote)` | 业务／适配 | [ProjectModel.h:1029](../juce/src/ProjectModel.h#L1029) |
| `void averageNotesMidi(const std::vector<juce::String>& noteIds)` | 业务／适配 | [ProjectModel.h:1030](../juce/src/ProjectModel.h#L1030) |
| `void quantizeNotesMidi(const std::vector<juce::String>& noteIds, float stepSemitones = 1.0f)` | 业务／适配 | [ProjectModel.h:1031](../juce/src/ProjectModel.h#L1031) |
| `std::vector<juce::String> notesOverlapping( const juce::String& clipId, double fromSeconds, double toSeconds) const` | 业务／适配 | [ProjectModel.h:1035](../juce/src/ProjectModel.h#L1035) |
| `std::vector<juce::String> insertNotes( const juce::String& clipId, const std::vector<NoteData>& noteTemplates, double absoluteStartSeconds)` | 业务／适配 | [ProjectModel.h:1037](../juce/src/ProjectModel.h#L1037) |
| `std::vector<juce::String> insertNativeAudioClips( const juce::String& trackId, const std::vector<ClipData>& templates, const std::vector<NativeConnection>& connections, double atSeconds)` | 业务／适配 | [ProjectModel.h:1040](../juce/src/ProjectModel.h#L1040) |
| `std::vector<juce::String> duplicateNotes( const std::vector<juce::String>& noteIds, const juce::String& targetClipId, double absoluteStartSeconds)` | 业务／适配 | [ProjectModel.h:1043](../juce/src/ProjectModel.h#L1043) |
| `void resizeNote(const juce::String& noteId, double newStart, double newDuration)` | 业务／适配 | [ProjectModel.h:1046](../juce/src/ProjectModel.h#L1046) |
| `juce::String splitNote(const juce::String& noteId, double localSeconds)` | 业务／适配 | [ProjectModel.h:1047](../juce/src/ProjectModel.h#L1047) |
| `juce::String mergeNotes(const std::vector<juce::String>& noteIds)` | 业务／适配 | [ProjectModel.h:1049](../juce/src/ProjectModel.h#L1049) |
| `bool canJoinNativeNotes(const juce::String&,const juce::String&) const` | 业务／适配 | [ProjectModel.h:1050](../juce/src/ProjectModel.h#L1050) |
| `juce::String joinNativeNotes(const juce::String&,const juce::String&)` | 业务／适配 | [ProjectModel.h:1051](../juce/src/ProjectModel.h#L1051) |
| `void setNotesConnection(const std::vector<juce::String>& noteIds, bool enabled)` | 业务／适配 | [ProjectModel.h:1054](../juce/src/ProjectModel.h#L1054) |
| `bool disconnectNativeAudio(const std::vector<juce::String>& noteIds)` | 业务／适配 | [ProjectModel.h:1055](../juce/src/ProjectModel.h#L1055) |
| `bool linkNativeAudio(const std::vector<juce::String>& noteIds)` | 业务／适配 | [ProjectModel.h:1056](../juce/src/ProjectModel.h#L1056) |
| `bool setNativeAudioOverlap(const juce::String& trackId, bool enabled)` | 业务／适配 | [ProjectModel.h:1057](../juce/src/ProjectModel.h#L1057) |
| `juce::String insertPrefixNote(const juce::String& noteId, double targetOverlapSeconds)` | 业务／适配 | [ProjectModel.h:1062](../juce/src/ProjectModel.h#L1062) |
| `void setNoteModulation(const juce::String& noteId, float modulation)` | 业务／适配 | [ProjectModel.h:1064](../juce/src/ProjectModel.h#L1064) |
| `void setNoteDrift(const juce::String& noteId, float drift)` | 业务／适配 | [ProjectModel.h:1065](../juce/src/ProjectModel.h#L1065) |
| `void setNoteTension(const juce::String& noteId, float tension)` | 业务／适配 | [ProjectModel.h:1066](../juce/src/ProjectModel.h#L1066) |
| `void setNoteBreath(const juce::String& noteId, float breath)` | 业务／适配 | [ProjectModel.h:1067](../juce/src/ProjectModel.h#L1067) |
| `void setNoteFormant(const juce::String& noteId, float semitones)` | 业务／适配 | [ProjectModel.h:1068](../juce/src/ProjectModel.h#L1068) |
| `void setNoteGain(const juce::String& noteId, float gain)` | 业务／适配 | [ProjectModel.h:1069](../juce/src/ProjectModel.h#L1069) |
| `bool setNotesTailFade(const std::vector<juce::String>& noteIds, int mode)` | 业务／适配 | [ProjectModel.h:1070](../juce/src/ProjectModel.h#L1070) |
| `bool setNotesNativeEnvelope(const std::vector<juce::String>& noteIds, const backend::NativeEnvelopeSettings& settings)` | 业务／适配 | [ProjectModel.h:1071](../juce/src/ProjectModel.h#L1071) |
| `bool setNotesTailFade(const std::vector<juce::String>& noteIds, int mode, const backend::TailFadeSettings& settings)` | 业务／适配 | [ProjectModel.h:1072](../juce/src/ProjectModel.h#L1072) |
| `void setNotesAmplitudeEnvelopeBase(const std::vector<juce::String>& noteIds, float basePercent)` | 业务／适配 | [ProjectModel.h:1074](../juce/src/ProjectModel.h#L1074) |
| `void setNoteAttack(const juce::String& noteId, double consonantSeconds, float attackSpeed)` | 业务／适配 | [ProjectModel.h:1076](../juce/src/ProjectModel.h#L1076) |
| `void setNoteAttackSpeed(const juce::String& noteId, float attackSpeed)` | 业务／适配 | [ProjectModel.h:1077](../juce/src/ProjectModel.h#L1077) |
| `void setNoteLabel(const juce::String& noteId, const juce::String& label)` | 业务／适配 | [ProjectModel.h:1078](../juce/src/ProjectModel.h#L1078) |
| `int convertTrackLyricsToPinyin(const juce::String& trackId)` | 业务／适配 | [ProjectModel.h:1082](../juce/src/ProjectModel.h#L1082) |
| `void setNoteLabels(const std::vector<std::pair<juce::String, juce::String>>& labels)` | 业务／适配 | [ProjectModel.h:1086](../juce/src/ProjectModel.h#L1086) |
| `void setNoteUtauFlags(const juce::String& noteId, const juce::String& flags)` | 业务／适配 | [ProjectModel.h:1090](../juce/src/ProjectModel.h#L1090) |
| `void setNotesUtauFlags(const std::vector<juce::String>& noteIds, const juce::String& flags)` | 业务／适配 | [ProjectModel.h:1091](../juce/src/ProjectModel.h#L1091) |
| `void setNotesUtauConsonantVelocity(const std::vector<juce::String>& noteIds, int velocity)` | 业务／适配 | [ProjectModel.h:1093](../juce/src/ProjectModel.h#L1093) |
| `void setNoteUtauOto(const juce::String& noteId, const backend::UtauOtoOverride& oto)` | 业务／适配 | [ProjectModel.h:1120](../juce/src/ProjectModel.h#L1120) |
| `void clearNotesUtauOto(const std::vector<juce::String>& noteIds)` | 业务／适配 | [ProjectModel.h:1124](../juce/src/ProjectModel.h#L1124) |
| `void setNotesUtauStp(const std::vector<juce::String>& noteIds, double seconds)` | 业务／适配 | [ProjectModel.h:1126](../juce/src/ProjectModel.h#L1126) |
| `void setNoteUtauTimingOverrides(const juce::String& noteId, bool enabled, double preutteranceSeconds, double overlapSeconds)` | 业务／适配 | [ProjectModel.h:1127](../juce/src/ProjectModel.h#L1127) |
| `void setNoteRobustPitchCurve(const juce::String& noteId, bool enabled)` | 业务／适配 | [ProjectModel.h:1130](../juce/src/ProjectModel.h#L1130) |
| `bool rememberDiffSingerParameters(const std::map<juce::String, std::vector<FlagCurve>>& curves)` | 业务／适配 | [ProjectModel.h:1131](../juce/src/ProjectModel.h#L1131) |
| `bool applyDiffSingerParameters(std::uint64_t expectedRevision, const std::map<juce::String, std::vector<FlagCurve>>& curves)` | 业务／适配 | [ProjectModel.h:1132](../juce/src/ProjectModel.h#L1132) |
| `bool setDiffSingerParameterCurve(const juce::String& id, const juce::String& key, std::vector<FlagCurvePoint> points)` | 业务／适配 | [ProjectModel.h:1134](../juce/src/ProjectModel.h#L1134) |
| `bool setDiffSingerPitchOffset(const juce::String& noteId, std::vector<PitchCurveEditPoint> points)` | 业务／适配 | [ProjectModel.h:1136](../juce/src/ProjectModel.h#L1136) |
| `bool setNotePitchCurve(const juce::String& noteId, std::vector<PitchCurveEditPoint> points, bool storeControlPoints = false)` | 业务／适配 | [ProjectModel.h:1138](../juce/src/ProjectModel.h#L1138) |
| `bool applyDiffSingerTiming(std::uint64_t expectedRevision, const std::vector<std::pair<juce::String, juce::String>>& timings)` | 业务／适配 | [ProjectModel.h:1141](../juce/src/ProjectModel.h#L1141) |
| `bool applyDiffSingerPronunciation(std::uint64_t expectedRevision, const juce::String& trackId, const std::vector<std::pair<juce::String, juce::String>>& readings, const juce::String& dictionary)` | 业务／适配 | [ProjectModel.h:1143](../juce/src/ProjectModel.h#L1143) |
| `bool restoreDiffSingerPitchPoint(std::uint64_t expectedRevision, const juce::String& noteId, const std::vector<PitchCurveEditPoint>& displayedPoints, int index)` | 业务／适配 | [ProjectModel.h:1145](../juce/src/ProjectModel.h#L1145) |
| `bool applyDiffSingerPitch(std::uint64_t expectedRevision, const std::vector<std::pair<juce::String, std::vector<PitchCurveEditPoint>>>& curves, bool preserveOutsideNoteHandles = false)` | 业务／适配 | [ProjectModel.h:1147](../juce/src/ProjectModel.h#L1147) |
| `void setDiffSingerOptions(const juce::String& trackId, const juce::String& language, const juce::String& speaker)` | 业务／适配 | [ProjectModel.h:1150](../juce/src/ProjectModel.h#L1150) |
| `bool setNoteAmplitudeEnvelope(const juce::String& noteId, std::vector<AmplitudeEnvelopePoint> points)` | 业务／适配 | [ProjectModel.h:1152](../juce/src/ProjectModel.h#L1152) |
| `int crossfadeNotes(std::vector<CrossfadeSpan> spans, std::uint64_t expectedRevision, bool curved = false)` | 业务／适配 | [ProjectModel.h:1164](../juce/src/ProjectModel.h#L1164) |
| `bool rememberCvvcPhonemes(const std::vector<std::pair<juce::String,juce::String>>& caches, std::uint64_t expectedRevision)` | 业务／适配 | [ProjectModel.h:1165](../juce/src/ProjectModel.h#L1165) |
| `bool setNotesAmplitudeEnvelopes( std::vector<std::pair<juce::String, std::vector<AmplitudeEnvelopePoint>>> envelopes)` | 业务／适配 | [ProjectModel.h:1167](../juce/src/ProjectModel.h#L1167) |
| `static PlannedNote plannedNoteFor(double startSeconds, double requestedSeconds, double clipSeconds, const std::vector<NoteSpan>& occupied)` | 业务／适配 | [ProjectModel.h:1193](../juce/src/ProjectModel.h#L1193) |
| `juce::String addClip(const juce::String& trackId, double startSeconds, double durationSeconds)` | 业务／适配 | [ProjectModel.h:1201](../juce/src/ProjectModel.h#L1201) |
| `bool canAddEmptyTuningClip(const juce::String& trackId, double startSeconds) const` | 业务／适配 | [ProjectModel.h:1203](../juce/src/ProjectModel.h#L1203) |
| `juce::String addEmptyTuningClip(const juce::String& trackId, double startSeconds, double durationSeconds = 10.0)` | 业务／适配 | [ProjectModel.h:1205](../juce/src/ProjectModel.h#L1205) |
| `static std::vector<std::size_t> regionsToImport( const std::vector<RegionSpan>& regions)` | 业务／适配 | [ProjectModel.h:1222](../juce/src/ProjectModel.h#L1222) |
| `void flattenNotePitch(const std::vector<juce::String>& noteIds, bool snapNativeToSemitone = false)` | 业务／适配 | [ProjectModel.h:1238](../juce/src/ProjectModel.h#L1238) |
| `bool restoreNativeSourcePitch(const std::vector<juce::String>& noteIds)` | 业务／适配 | [ProjectModel.h:1239](../juce/src/ProjectModel.h#L1239) |
| `static double firstFreeStartFrom(double startSeconds, const std::vector<NoteSpan>& occupied)` | 业务／适配 | [ProjectModel.h:1240](../juce/src/ProjectModel.h#L1240) |
| `juce::String addNote(const juce::String& preferredClipId, double absoluteStart, double duration, float midiNote)` | 业务／适配 | [ProjectModel.h:1243](../juce/src/ProjectModel.h#L1243) |
| `juce::String addNoteFrom(const juce::String& preferredClipId, double absoluteFromSeconds, double maximumDuration, float midiNote)` | 业务／适配 | [ProjectModel.h:1251](../juce/src/ProjectModel.h#L1251) |
| `void removeNote(const juce::String& noteId)` | 业务／适配 | [ProjectModel.h:1254](../juce/src/ProjectModel.h#L1254) |
| `void removeNotes(const std::vector<juce::String>& noteIds)` | 业务／适配 | [ProjectModel.h:1255](../juce/src/ProjectModel.h#L1255) |
| `void removeNotesRippling(const std::vector<juce::String>& noteIds)` | 业务／适配 | [ProjectModel.h:1259](../juce/src/ProjectModel.h#L1259) |
| `void insertGapBeforeNote(const juce::String& noteId, double seconds)` | 业务／适配 | [ProjectModel.h:1263](../juce/src/ProjectModel.h#L1263) |
| `void closeGapBeforeNote(const juce::String& noteId, double seconds)` | 业务／适配 | [ProjectModel.h:1266](../juce/src/ProjectModel.h#L1266) |
| `void toggleNoteConnection(const juce::String& noteId)` | 业务／适配 | [ProjectModel.h:1267](../juce/src/ProjectModel.h#L1267) |
| `void applySourceSettings(const juce::File& source, const std::vector<SampleRegionSetting>& rows)` | 业务／适配 | [ProjectModel.h:1268](../juce/src/ProjectModel.h#L1268) |
| `bool save(const juce::File& file, juce::String& error) const` | 业务／适配 | [ProjectModel.h:1271](../juce/src/ProjectModel.h#L1271) |
| `bool load(const juce::File& file, juce::String& error)` | 业务／适配 | [ProjectModel.h:1272](../juce/src/ProjectModel.h#L1272) |
| `static bool saveRecoverySnapshot(ProjectData data, const juce::File& recoveryFile, const juce::File& originalFile, juce::String& error)` | 业务／适配 | [ProjectModel.h:1273](../juce/src/ProjectModel.h#L1273) |

## AudioEngine（67 个声明）

| 实际签名 | 类型 | 定义入口 |
| --- | --- | --- |
| `AudioEngine()` | 生命周期 | [AudioEngine.h:58](../juce/src/AudioEngine.h#L58) |
| `~AudioEngine() override` | 生命周期 | [AudioEngine.h:59](../juce/src/AudioEngine.h#L59) |
| `void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override` | 业务／适配 | [AudioEngine.h:61](../juce/src/AudioEngine.h#L61) |
| `void releaseResources() override` | 业务／适配 | [AudioEngine.h:62](../juce/src/AudioEngine.h#L62) |
| `void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override` | 业务／适配 | [AudioEngine.h:63](../juce/src/AudioEngine.h#L63) |
| `void syncProject(const ProjectData& project, bool diffSingerExport = false, WavExportComponent component = WavExportComponent::full)` | 业务／适配 | [AudioEngine.h:65](../juce/src/AudioEngine.h#L65) |
| `static juce::String componentExportIssue(const ProjectData& project, const juce::File& engine, const juce::String& trackId = {}, const std::vector<juce::String>& noteIds = {}, juce::Range<double> range = {}, const juce::String& auditionTrack = {}, UtauOutputEngine defaultEngine = UtauOutputEngine::resampler)` | 业务／适配 | [AudioEngine.h:67](../juce/src/AudioEngine.h#L67) |
| `void setPlayUntil(double seconds)` | 业务／适配 | [AudioEngine.h:74](../juce/src/AudioEngine.h#L74) |
| `void setUtauRenderNoteSelection(const std::vector<juce::String>& noteIds)` | 业务／适配 | [AudioEngine.h:75](../juce/src/AudioEngine.h#L75) |
| `void setAuditionTrack(const juce::String& trackId)` | 业务／适配 | [AudioEngine.h:79](../juce/src/AudioEngine.h#L79) |
| `static bool trackIsAudible(bool muted, bool solo, bool anySolo, bool referenceOnly, bool beingWorkedOn)` | 业务／适配 | [AudioEngine.h:82](../juce/src/AudioEngine.h#L82) |
| `int selectEveryUtauNote(const ProjectData& project)` | 业务／适配 | [AudioEngine.h:88](../juce/src/AudioEngine.h#L88) |
| `bool selectAllRenderedUtauNotes()` | 业务／适配 | [AudioEngine.h:89](../juce/src/AudioEngine.h#L89) |
| `void setHifiganModelDirectory(const juce::File& directory)` | 业务／适配 | [AudioEngine.h:90](../juce/src/AudioEngine.h#L90) |
| `void setUtauResamplerFile(const juce::File& executable)` | 业务／适配 | [AudioEngine.h:95](../juce/src/AudioEngine.h#L95) |
| `void setUtauOutputDefaults(UtauOutputEngine engine, const juce::File& wavtool)` | 业务／适配 | [AudioEngine.h:96](../juce/src/AudioEngine.h#L96) |
| `UtauOutputEngine currentUtauOutputEngine() const` | 业务／适配 | [AudioEngine.h:97](../juce/src/AudioEngine.h#L97) |
| `juce::File currentUtauWavtoolFile() const` | 业务／适配 | [AudioEngine.h:98](../juce/src/AudioEngine.h#L98) |
| `juce::String outputEngineDisplayName(const TrackData& track) const` | 业务／适配 | [AudioEngine.h:99](../juce/src/AudioEngine.h#L99) |
| `static juce::File bundledUtauResampler( const juce::File& executableDirectory)` | 业务／适配 | [AudioEngine.h:104](../juce/src/AudioEngine.h#L104) |
| `static juce::File resolveUtauResampler( const juce::String& configured, const juce::File& executableDirectory)` | 业务／适配 | [AudioEngine.h:107](../juce/src/AudioEngine.h#L107) |
| `juce::File diagnosticUtauResamplerFile() const` | 诊断 | [AudioEngine.h:114](../juce/src/AudioEngine.h#L114) |
| `juce::File currentUtauResamplerFile() const` | 业务／适配 | [AudioEngine.h:118](../juce/src/AudioEngine.h#L118) |
| `void setInferenceConfiguration(backend::InferenceBackend inference, int deviceIndex)` | 业务／适配 | [AudioEngine.h:122](../juce/src/AudioEngine.h#L122) |
| `std::optional<double> probeDuration(const juce::File& file)` | 业务／适配 | [AudioEngine.h:123](../juce/src/AudioEngine.h#L123) |
| `bool setAuditionFile(const juce::File& file)` | 业务／适配 | [AudioEngine.h:124](../juce/src/AudioEngine.h#L124) |
| `void clearAuditionFile()` | 业务／适配 | [AudioEngine.h:125](../juce/src/AudioEngine.h#L125) |
| `void play()` | 业务／适配 | [AudioEngine.h:127](../juce/src/AudioEngine.h#L127) |
| `void stop()` | 业务／适配 | [AudioEngine.h:128](../juce/src/AudioEngine.h#L128) |
| `void setPosition(double seconds)` | 业务／适配 | [AudioEngine.h:129](../juce/src/AudioEngine.h#L129) |
| `double position() const` | 业务／适配 | [AudioEngine.h:130](../juce/src/AudioEngine.h#L130) |
| `float trackPeak(const juce::String& trackId) const` | 业务／适配 | [AudioEngine.h:131](../juce/src/AudioEngine.h#L131) |
| `std::optional<double> renderProgress() const` | 业务／适配 | [AudioEngine.h:132](../juce/src/AudioEngine.h#L132) |
| `bool playbackNeedsRender(double lookAheadSeconds = 0.75) const` | 业务／适配 | [AudioEngine.h:135](../juce/src/AudioEngine.h#L135) |
| `bool hasPlayableRenderedAudio() const` | 业务／适配 | [AudioEngine.h:136](../juce/src/AudioEngine.h#L136) |
| `bool hasCurrentRenderedAudio() const` | 业务／适配 | [AudioEngine.h:137](../juce/src/AudioEngine.h#L137) |
| `bool rewindToFirstPlayableRenderedAudio(double leadInSeconds = 0.03)` | 业务／适配 | [AudioEngine.h:138](../juce/src/AudioEngine.h#L138) |
| `juce::String activeRenderBackends() const` | 业务／适配 | [AudioEngine.h:139](../juce/src/AudioEngine.h#L139) |
| `juce::String activeRenderWarnings() const` | 业务／适配 | [AudioEngine.h:140](../juce/src/AudioEngine.h#L140) |
| `static juce::StringArray renderCapabilityWarnings( const ProjectData& project, UtauOutputEngine defaultEngine = UtauOutputEngine::resampler)` | 业务／适配 | [AudioEngine.h:146](../juce/src/AudioEngine.h#L146) |
| `static bool clipsJoinAt(const ClipData& clip, const ClipData& neighbour, bool asNext)` | 业务／适配 | [AudioEngine.h:158](../juce/src/AudioEngine.h#L158) |
| `static std::vector<std::vector<const ClipData*>> glideChains( const std::vector<const ClipData*>& orderedClips)` | 业务／适配 | [AudioEngine.h:166](../juce/src/AudioEngine.h#L166) |
| `static backend::Mld5FileRenderRequest mergedRequestFor( const std::vector<const ClipData*>& group, const TrackData& track, const juce::File& hifiganModelDirectory, const backend::OrtExecutionConfig& inference)` | 业务／适配 | [AudioEngine.h:172](../juce/src/AudioEngine.h#L172) |
| `int diagnosticMergedPhraseCount() const` | 诊断 | [AudioEngine.h:178](../juce/src/AudioEngine.h#L178) |
| `std::uint64_t diagnosticScheduledNsfUtauRenders() const` | 诊断 | [AudioEngine.h:179](../juce/src/AudioEngine.h#L179) |
| `double diagnosticRenderPriorityPosition() const` | 诊断 | [AudioEngine.h:180](../juce/src/AudioEngine.h#L180) |
| `static std::vector<float> diagnosticNativeTargetMidi( const ProjectData& project, int trackIndex, int clipIndex)` | 诊断 | [AudioEngine.h:186](../juce/src/AudioEngine.h#L186) |
| `static backend::Mld5FileRenderRequest diagnosticNativeRequest(const ClipData& clip,const TrackData& track)` | 诊断 | [AudioEngine.h:188](../juce/src/AudioEngine.h#L188) |
| `std::shared_ptr<const std::vector<UtauNoteWaveform>> utauNoteWaveforms() const` | 业务／适配 | [AudioEngine.h:194](../juce/src/AudioEngine.h#L194) |
| `void refreshUtauWaveforms()` | 业务／适配 | [AudioEngine.h:200](../juce/src/AudioEngine.h#L200) |
| `void refreshNativeWaveforms()` | 业务／适配 | [AudioEngine.h:201](../juce/src/AudioEngine.h#L201) |
| `std::shared_ptr<const std::vector<NativeRenderedWaveform>> nativeClipWaveforms() const` | 业务／适配 | [AudioEngine.h:202](../juce/src/AudioEngine.h#L202) |
| `static std::uint64_t nativeClipWaveformHash(const ClipData&, const TrackData&)` | 业务／适配 | [AudioEngine.h:203](../juce/src/AudioEngine.h#L203) |
| `static std::uint64_t utauNoteRenderHash(const NoteData& note)` | 业务／适配 | [AudioEngine.h:209](../juce/src/AudioEngine.h#L209) |
| `static std::uint64_t utauNoteAudioHash(const NoteData& note)` | 业务／适配 | [AudioEngine.h:212](../juce/src/AudioEngine.h#L212) |
| `static std::uint64_t utauNotePhonemeHash(const NoteData& note)` | 业务／适配 | [AudioEngine.h:213](../juce/src/AudioEngine.h#L213) |
| `static std::uint64_t utauPhonemeMixContextHash(const TrackData& track)` | 业务／适配 | [AudioEngine.h:214](../juce/src/AudioEngine.h#L214) |
| `static std::vector<backend::UtauNoteRenderSpec> diagnosticUtauRequestNotes( const ProjectData& project, const juce::String& clipId, const std::vector<juce::String>& selection = {})` | 诊断 | [AudioEngine.h:218](../juce/src/AudioEngine.h#L218) |
| `static std::vector<backend::UtauNoteRenderSpec> utauRequestNotesForClip( const ProjectData& project, const juce::String& clipId)` | 业务／适配 | [AudioEngine.h:222](../juce/src/AudioEngine.h#L222) |
| `static std::string diagnosticUtauRenderKey( const ProjectData& project, const juce::String& clipId, UtauOutputEngine defaultEngine = UtauOutputEngine::resampler, const juce::File& resampler = {}, const juce::File& wavtool = {})` | 诊断 | [AudioEngine.h:224](../juce/src/AudioEngine.h#L224) |
| `bool exportWav(const juce::File& file, juce::String& error, const juce::String& trackId = {}, double fromSeconds = 0.0, double toSeconds = 0.0, WavExportOptions options = {})` | 业务／适配 | [AudioEngine.h:229](../juce/src/AudioEngine.h#L229) |
| `double playbackSampleRate() const` | 业务／适配 | [AudioEngine.h:233](../juce/src/AudioEngine.h#L233) |
| `bool isPlaying() const` | 业务／适配 | [AudioEngine.h:234](../juce/src/AudioEngine.h#L234) |
| `juce::AudioDeviceManager& devices()` | 业务／适配 | [AudioEngine.h:235](../juce/src/AudioEngine.h#L235) |
| `bool ensureOutputDevice(juce::String& error)` | 业务／适配 | [AudioEngine.h:236](../juce/src/AudioEngine.h#L236) |
| `void restoreDeviceState(juce::PropertiesFile& properties)` | 业务／适配 | [AudioEngine.h:237](../juce/src/AudioEngine.h#L237) |
| `void saveDeviceState(juce::PropertiesFile& properties) const` | 业务／适配 | [AudioEngine.h:238](../juce/src/AudioEngine.h#L238) |

## SampleSettings（31 个声明）

| 实际签名 | 类型 | 定义入口 |
| --- | --- | --- |
| `static juce::File sidecarFor(const juce::File& audio)` | 业务／适配 | [SampleSettings.h:78](../juce/src/SampleSettings.h#L78) |
| `static std::vector<NativeSegment> nativeSegmentsFor( const SampleRegionSetting& row)` | 业务／适配 | [SampleSettings.h:82](../juce/src/SampleSettings.h#L82) |
| `static std::vector<SampleRegionSetting> loadOrDerive(const juce::File& audio, const ProjectData& project)` | 业务／适配 | [SampleSettings.h:84](../juce/src/SampleSettings.h#L84) |
| `static bool save(const juce::File& audio, const std::vector<SampleRegionSetting>& rows, juce::String& error)` | 业务／适配 | [SampleSettings.h:86](../juce/src/SampleSettings.h#L86) |
| `static bool convertMelodyneProject(ProjectData& project, juce::StringArray& warnings)` | 业务／适配 | [SampleSettings.h:91](../juce/src/SampleSettings.h#L91) |
| `static bool importOto(const juce::File& oto, const juce::File& audio, double audioDuration, std::vector<SampleRegionSetting>& rows, juce::String& error)` | 业务／适配 | [SampleSettings.h:92](../juce/src/SampleSettings.h#L92) |
| `static bool exportOto(const juce::File& oto, const juce::File& audio, const std::vector<SampleRegionSetting>& rows, double audioDuration, juce::String& error)` | 业务／适配 | [SampleSettings.h:95](../juce/src/SampleSettings.h#L95) |
| `static bool importVoicebank(const juce::File& root, juce::StringArray& audioFiles, int& sidecarsWritten, int& regionsWritten, juce::StringArray& warnings)` | 业务／适配 | [SampleSettings.h:98](../juce/src/SampleSettings.h#L98) |
| `static backend::UtauOtoOverride noteOtoFromEntry(const VoicebankOtoEntry& entry, bool withRegions)` | 业务／适配 | [SampleSettings.h:106](../juce/src/SampleSettings.h#L106) |
| `static VoicebankOtoEntry entryWithNoteOto(VoicebankOtoEntry entry, const backend::UtauOtoOverride& oto)` | 业务／适配 | [SampleSettings.h:123](../juce/src/SampleSettings.h#L123) |
| `static std::vector<VoicebankOtoEntry> loadVoicebankOto( const juce::File& root, juce::StringArray& warnings, bool jieMode = false, bool mouMode = false, const juce::File& onlyOtoFile = {})` | 业务／适配 | [SampleSettings.h:142](../juce/src/SampleSettings.h#L142) |
| `static std::vector<VoicebankOtoEntry> loadAudioOto(const juce::File& audio, const juce::File& root, UtauMode mode, juce::StringArray& warnings)` | 业务／适配 | [SampleSettings.h:147](../juce/src/SampleSettings.h#L147) |
| `static std::optional<ClipData> nativeAudioFromOto(const VoicebankOtoEntry& entry, UtauMode mode, double sourceDurationSeconds)` | 业务／适配 | [SampleSettings.h:150](../juce/src/SampleSettings.h#L150) |
| `static std::uint64_t voicebankFilesRevision()` | 业务／适配 | [SampleSettings.h:156](../juce/src/SampleSettings.h#L156) |
| `static bool updateVoicebankOtoEntry(const VoicebankOtoEntry& original, const VoicebankOtoEntry& updated, juce::String& error)` | 业务／适配 | [SampleSettings.h:157](../juce/src/SampleSettings.h#L157) |
| `static juce::File jieClassicOtoFileFor(const juce::File& otoFile)` | 业务／适配 | [SampleSettings.h:163](../juce/src/SampleSettings.h#L163) |
| `static bool updateJieVoicebankOtoEntry(const VoicebankOtoEntry& original, const VoicebankOtoEntry& updated, juce::String& error)` | 业务／适配 | [SampleSettings.h:164](../juce/src/SampleSettings.h#L164) |
| `static bool duplicateVoicebankOtoEntry(const VoicebankOtoEntry& source, const juce::String& newAlias, bool jieMode, juce::String& error)` | 业务／适配 | [SampleSettings.h:169](../juce/src/SampleSettings.h#L169) |
| `static juce::String entryLookupName(const VoicebankOtoEntry& entry)` | 业务／适配 | [SampleSettings.h:177](../juce/src/SampleSettings.h#L177) |
| `static int findEntryForAlias(const std::vector<VoicebankOtoEntry>& entries, const juce::String& alias)` | 业务／适配 | [SampleSettings.h:180](../juce/src/SampleSettings.h#L180) |
| `static juce::File jieOtoFileFor(const juce::File& otoFile)` | 业务／适配 | [SampleSettings.h:182](../juce/src/SampleSettings.h#L182) |
| `static void mergeJieOto(std::vector<VoicebankOtoEntry>& entries)` | 业务／适配 | [SampleSettings.h:184](../juce/src/SampleSettings.h#L184) |
| `static void mergeMouOto(std::vector<VoicebankOtoEntry>& entries)` | 业务／适配 | [SampleSettings.h:187](../juce/src/SampleSettings.h#L187) |
| `static int mouRegionCount(const juce::String& classes)` | 业务／适配 | [SampleSettings.h:190](../juce/src/SampleSettings.h#L190) |
| `static juce::String mouClassesForCount(const juce::String& classes, int count)` | 业务／适配 | [SampleSettings.h:194](../juce/src/SampleSettings.h#L194) |
| `static bool updateMouOtoEntry(const VoicebankOtoEntry& original, const VoicebankOtoEntry& updated, juce::String& error)` | 业务／适配 | [SampleSettings.h:198](../juce/src/SampleSettings.h#L198) |
| `static bool createMouOto(const juce::File& root, int& written, int& kept, int& merged, juce::String& error)` | 业务／适配 | [SampleSettings.h:210](../juce/src/SampleSettings.h#L210) |
| `static juce::File mouOtoFileFor(const juce::File& otoFile)` | 业务／适配 | [SampleSettings.h:212](../juce/src/SampleSettings.h#L212) |
| `static bool createJieOto(const juce::File& root, int& written, int& kept, juce::String& error)` | 业务／适配 | [SampleSettings.h:215](../juce/src/SampleSettings.h#L215) |
| `static bool createJieOtoFromMou(const juce::File& root, JieFromMouResult& result, juce::String& error)` | 业务／适配 | [SampleSettings.h:220](../juce/src/SampleSettings.h#L220) |
| `static bool updateJieOtoEntry(const VoicebankOtoEntry& original, const VoicebankOtoEntry& updated, juce::String& error)` | 业务／适配 | [SampleSettings.h:222](../juce/src/SampleSettings.h#L222) |

## AccompanimentTempoTask（8 个声明）

| 实际签名 | 类型 | 定义入口 |
| --- | --- | --- |
| `explicit AccompanimentTempoTask(ClipData region, juce::File runtime = {})` | 生命周期 | [AccompanimentTempo.h:12](../juce/src/AccompanimentTempo.h#L12) |
| `~AccompanimentTempoTask() override` | 生命周期 | [AccompanimentTempo.h:13](../juce/src/AccompanimentTempo.h#L13) |
| `void cancel()` | 业务／适配 | [AccompanimentTempo.h:14](../juce/src/AccompanimentTempo.h#L14) |
| `bool finished() const` | 业务／适配 | [AccompanimentTempo.h:15](../juce/src/AccompanimentTempo.h#L15) |
| `int stage() const` | 业务／适配 | [AccompanimentTempo.h:16](../juce/src/AccompanimentTempo.h#L16) |
| `juce::var result() const` | 业务／适配 | [AccompanimentTempo.h:17](../juce/src/AccompanimentTempo.h#L17) |
| `static bool canAnalyse(const ClipData&)` | 业务／适配 | [AccompanimentTempo.h:18](../juce/src/AccompanimentTempo.h#L18) |
| `static juce::File runtimeConfig()` | 业务／适配 | [AccompanimentTempo.h:19](../juce/src/AccompanimentTempo.h#L19) |

## AnalysisService（7 个声明）

| 实际签名 | 类型 | 定义入口 |
| --- | --- | --- |
| `static AnalysisConfig configFromProperties(const juce::PropertiesFile* properties)` | 业务／适配 | [backend/AnalysisService.h:49](../juce/src/backend/AnalysisService.h#L49) |
| `static AnalysisConfig configFromEnvironment()` | 业务／适配 | [backend/AnalysisService.h:50](../juce/src/backend/AnalysisService.h#L50) |
| `static AnalysisStatus status(const AnalysisConfig& config)` | 业务／适配 | [backend/AnalysisService.h:51](../juce/src/backend/AnalysisService.h#L51) |
| `static AnalysisResult analyse(const juce::File& file, const AnalysisConfig& config, juce::String& error, Progress progress = {})` | 业务／适配 | [backend/AnalysisService.h:52](../juce/src/backend/AnalysisService.h#L52) |
| `static std::size_t applySourcePitch(ClipData& clip, const std::vector<NoteData>& sourceNotes)` | 业务／适配 | [backend/AnalysisService.h:56](../juce/src/backend/AnalysisService.h#L56) |
| `static bool reanalyseProjectSourcePitch(ProjectData& project, const AnalysisConfig& config, juce::String& error, Progress progress = {}, AnalysisStatus* usedStatus = nullptr)` | 业务／适配 | [backend/AnalysisService.h:58](../juce/src/backend/AnalysisService.h#L58) |
| `static juce::String backendText(const AnalysisStatus& status)` | 业务／适配 | [backend/AnalysisService.h:63](../juce/src/backend/AnalysisService.h#L63) |

## RenderService（12 个声明）

| 实际签名 | 类型 | 定义入口 |
| --- | --- | --- |
| `RenderService()` | 生命周期 | [backend/RenderService.h:86](../juce/src/backend/RenderService.h#L86) |
| `~RenderService()` | 生命周期 | [backend/RenderService.h:87](../juce/src/backend/RenderService.h#L87) |
| `void renderMld5(Mld5RenderRequest request, Completion completion)` | 业务／适配 | [backend/RenderService.h:88](../juce/src/backend/RenderService.h#L88) |
| `void renderMld5File(Mld5FileRenderRequest request, FileCompletion completion, RenderSchedule schedule = {})` | 业务／适配 | [backend/RenderService.h:89](../juce/src/backend/RenderService.h#L89) |
| `void renderUtau(UtauRenderRequest request, FileCompletion completion, RenderSchedule schedule = {})` | 业务／适配 | [backend/RenderService.h:91](../juce/src/backend/RenderService.h#L91) |
| `void renderNsfUtau(UtauRenderRequest request, juce::File modelDirectory, OrtExecutionConfig execution, FileCompletion completion, RenderSchedule schedule = {})` | 业务／适配 | [backend/RenderService.h:97](../juce/src/backend/RenderService.h#L97) |
| `void cancelAll()` | 业务／适配 | [backend/RenderService.h:100](../juce/src/backend/RenderService.h#L100) |
| `bool hasActiveJobs()` | 业务／适配 | [backend/RenderService.h:101](../juce/src/backend/RenderService.h#L101) |
| `void setPlaybackPosition(double seconds) noexcept` | 业务／适配 | [backend/RenderService.h:102](../juce/src/backend/RenderService.h#L102) |
| `void beginUpdate()` | 业务／适配 | [backend/RenderService.h:103](../juce/src/backend/RenderService.h#L103) |
| `void endUpdate(const std::unordered_set<std::string>& keys)` | 业务／适配 | [backend/RenderService.h:104](../juce/src/backend/RenderService.h#L104) |
| `double playbackPriorityPosition() const` | 业务／适配 | [backend/RenderService.h:105](../juce/src/backend/RenderService.h#L105) |

## UtauRenderer（27 个声明）

| 实际签名 | 类型 | 定义入口 |
| --- | --- | --- |
| `static std::optional<UtauSampleTiming> mappedAliasTiming( const juce::File& directory, const juce::String& alias, float midiNote)` | 业务／适配 | [backend/UtauRenderer.h:224](../juce/src/backend/UtauRenderer.h#L224) |
| `static bool supportsComponentExport(const juce::File& engine, const juce::String& effectiveFlags)` | 业务／适配 | [backend/UtauRenderer.h:228](../juce/src/backend/UtauRenderer.h#L228) |
| `static juce::String diagnosticPitchbend( const UtauNoteRenderSpec& note, double bpm, double preutterance, double outputSeconds)` | 诊断 | [backend/UtauRenderer.h:230](../juce/src/backend/UtauRenderer.h#L230) |
| `static std::vector<UtauAmplitudePoint> fitAmplitudeEnvelope( const std::vector<UtauAmplitudePoint>& drawn, double preutteranceSeconds, double soundingEndSeconds)` | 业务／适配 | [backend/UtauRenderer.h:234](../juce/src/backend/UtauRenderer.h#L234) |
| `static void invalidateVoicebankCache()` | 业务／适配 | [backend/UtauRenderer.h:237](../juce/src/backend/UtauRenderer.h#L237) |
| `static void prewarmHfDaemon(const juce::File& resamplerExecutable)` | 业务／适配 | [backend/UtauRenderer.h:243](../juce/src/backend/UtauRenderer.h#L243) |
| `static bool startHfDaemonIfNeeded(const juce::File& resamplerExecutable, int port = 51765)` | 业务／适配 | [backend/UtauRenderer.h:246](../juce/src/backend/UtauRenderer.h#L246) |
| `static void shutdownHfDaemon()` | 业务／适配 | [backend/UtauRenderer.h:251](../juce/src/backend/UtauRenderer.h#L251) |
| `static juce::String hfDaemonInterpreter(const juce::File& engineDirectory)` | 业务／适配 | [backend/UtauRenderer.h:256](../juce/src/backend/UtauRenderer.h#L256) |
| `static bool voicebankIndexReady(const juce::File& voicebankDirectory, bool fourRegion, bool consonantClasses, std::function<void()> whenReady)` | 业务／适配 | [backend/UtauRenderer.h:262](../juce/src/backend/UtauRenderer.h#L262) |
| `static int diagnosticIndexBuilds()` | 诊断 | [backend/UtauRenderer.h:270](../juce/src/backend/UtauRenderer.h#L270) |
| `static int diagnosticMessageThreadWaits()` | 诊断 | [backend/UtauRenderer.h:273](../juce/src/backend/UtauRenderer.h#L273) |
| `static void diagnosticRecheckVoicebankFiles()` | 诊断 | [backend/UtauRenderer.h:274](../juce/src/backend/UtauRenderer.h#L274) |
| `static void recheckVoicebankFiles(const juce::File& root)` | 业务／适配 | [backend/UtauRenderer.h:276](../juce/src/backend/UtauRenderer.h#L276) |
| `static juce::String diagnosticResolve(const juce::File& voicebankDirectory, const juce::String& alias, float midiNote, bool fourRegion, bool consonantClasses, bool byScan)` | 诊断 | [backend/UtauRenderer.h:277](../juce/src/backend/UtauRenderer.h#L277) |
| `static double crossfadeEnd(double soundStart, double overlap, double nextNoteEnd)` | 业务／适配 | [backend/UtauRenderer.h:289](../juce/src/backend/UtauRenderer.h#L289) |
| `static bool crossfadesInto(double soundStart, double noteEnd)` | 业务／适配 | [backend/UtauRenderer.h:298](../juce/src/backend/UtauRenderer.h#L298) |
| `static bool sendsFlagCurves(bool noteFlagCurve)` | 业务／适配 | [backend/UtauRenderer.h:309](../juce/src/backend/UtauRenderer.h#L309) |
| `static bool readsOnlyFirstTwoRegions(bool fourRegion, bool consonantClasses, double durationSeconds)` | 业务／适配 | [backend/UtauRenderer.h:310](../juce/src/backend/UtauRenderer.h#L310) |
| `static std::array<double, 4> firstTwoRegions( const std::array<double, 4>& regionSeconds)` | 业务／适配 | [backend/UtauRenderer.h:313](../juce/src/backend/UtauRenderer.h#L313) |
| `static std::optional<UtauSampleTiming> sampleTiming( const juce::File& voicebankDirectory, const juce::String& alias, float midiNote, int consonantVelocity = 100, bool fourRegion = false, bool consonantClasses = false)` | 业务／适配 | [backend/UtauRenderer.h:315](../juce/src/backend/UtauRenderer.h#L315) |
| `static std::optional<UtauSampleTiming> sampleTiming( const juce::File& voicebankDirectory, const juce::String& alias, float midiNote, int consonantVelocity, bool fourRegion, bool consonantClasses, const UtauOtoOverride* noteOto)` | 业务／适配 | [backend/UtauRenderer.h:322](../juce/src/backend/UtauRenderer.h#L322) |
| `static ResolvedSample resolveVoiceSample( const juce::File& voicebankDirectory, const juce::String& alias, float midiNote, int consonantVelocity = 100, bool fourRegion = false, bool consonantClasses = false, double stpSeconds = 0.0, bool preutteranceOverrideEnabled = false, double preutteranceSeconds = 0.0, bool overlapOverrideEnabled = false, double overlapSeconds = 0.0, const UtauOtoOverride* noteOto = nullptr, bool aliasIsResolved = false)` | 业务／适配 | [backend/UtauRenderer.h:346](../juce/src/backend/UtauRenderer.h#L346) |
| `static UtauRenderResult render(const UtauRenderRequest& request)` | 业务／适配 | [backend/UtauRenderer.h:353](../juce/src/backend/UtauRenderer.h#L353) |
| `static std::optional<UtauTailFadeSpan> tailFadeSpan( const UtauSampleTiming& timing,double soundStart,double soundEnd, double nominalDuration,int velocity,bool fourRegion,bool mou, const std::array<double,3>* manual=nullptr)` | 业务／适配 | [backend/UtauRenderer.h:355](../juce/src/backend/UtauRenderer.h#L355) |
| `static std::optional<UtauTailFadeSpan> headEnvelopeSpan( const UtauSampleTiming& timing,double soundStart,double soundEnd, double nominalDuration,int velocity,bool fourRegion,bool mou, const std::array<double,3>* manual=nullptr)` | 业务／适配 | [backend/UtauRenderer.h:359](../juce/src/backend/UtauRenderer.h#L359) |
| `static UtauRegionSplit regionSplit( const std::array<double, 4>& sourceSeconds, double outputSeconds, int consonantVelocity, double leadInSeconds, const std::array<double, 3>* manualFractions, const juce::String* classes = nullptr)` | 业务／适配 | [backend/UtauRenderer.h:370](../juce/src/backend/UtauRenderer.h#L370) |

## NsfHifiganRenderer（3 个声明）

| 实际签名 | 类型 | 定义入口 |
| --- | --- | --- |
| `static void shutdown()` | 业务／适配 | [backend/NsfHifiganRenderer.h:66](../juce/src/backend/NsfHifiganRenderer.h#L66) |
| `static bool modelAvailable(const juce::File& configuredModelDirectory)` | 业务／适配 | [backend/NsfHifiganRenderer.h:67](../juce/src/backend/NsfHifiganRenderer.h#L67) |
| `static NsfHifiganRenderResult render( const juce::AudioBuffer<float>& source, double sampleRate, int targetSamples, double framePeriodMs, const std::vector<float>& targetMidi, const std::vector<float>& formantSemitones, const std::vector<NsfHifiganTimeMapPoint>& timeMap, const juce::File& configuredModelDirectory, const OrtExecutionConfig& execution, NsfHifiganStretchOrder stretchOrder, bool normalizeVolume = false, const NsfHifiganEdgeGuard& edgeGuard = NsfHifiganEdgeGuard{}, const std::function<bool()>& cancelled = {}, bool protectNativeNoise = false, bool smoothPitchTransitions = true, bool incrementalNativeRender = false, const std::vector<float>& sourceMidi = {})` | 业务／适配 | [backend/NsfHifiganRenderer.h:68](../juce/src/backend/NsfHifiganRenderer.h#L68) |

## DiffSingerRenderer（12 个声明）

| 实际签名 | 类型 | 定义入口 |
| --- | --- | --- |
| `static void openProjectCache(const juce::File& project = {})` | 业务／适配 | [backend/DiffSingerRenderer.h:10](../juce/src/backend/DiffSingerRenderer.h#L10) |
| `static bool saveProjectCache(const juce::File& project)` | 业务／适配 | [backend/DiffSingerRenderer.h:11](../juce/src/backend/DiffSingerRenderer.h#L11) |
| `static juce::String cacheSession()` | 业务／适配 | [backend/DiffSingerRenderer.h:12](../juce/src/backend/DiffSingerRenderer.h#L12) |
| `static void configure(const DiffSingerOptions&)` | 业务／适配 | [backend/DiffSingerRenderer.h:13](../juce/src/backend/DiffSingerRenderer.h#L13) |
| `static juce::var inferenceOptions(bool exporting = false)` | 业务／适配 | [backend/DiffSingerRenderer.h:14](../juce/src/backend/DiffSingerRenderer.h#L14) |
| `static juce::String inferenceStatus()` | 业务／适配 | [backend/DiffSingerRenderer.h:15](../juce/src/backend/DiffSingerRenderer.h#L15) |
| `static juce::var inferenceReport()` | 业务／适配 | [backend/DiffSingerRenderer.h:16](../juce/src/backend/DiffSingerRenderer.h#L16) |
| `static void shutdown()` | 业务／适配 | [backend/DiffSingerRenderer.h:17](../juce/src/backend/DiffSingerRenderer.h#L17) |
| `static bool isVoicebank(const juce::File& directory)` | 业务／适配 | [backend/DiffSingerRenderer.h:18](../juce/src/backend/DiffSingerRenderer.h#L18) |
| `static juce::var invoke(juce::var request, const std::function<bool()>& cancelled = {}, juce::AudioBuffer<float>* audio = nullptr, double* rate = nullptr)` | 业务／适配 | [backend/DiffSingerRenderer.h:19](../juce/src/backend/DiffSingerRenderer.h#L19) |
| `static juce::var requestJson(const UtauRenderRequest&, const juce::String& operation)` | 业务／适配 | [backend/DiffSingerRenderer.h:21](../juce/src/backend/DiffSingerRenderer.h#L21) |
| `static UtauRenderResult render(const UtauRenderRequest&)` | 业务／适配 | [backend/DiffSingerRenderer.h:22](../juce/src/backend/DiffSingerRenderer.h#L22) |

## McpServer（12 个声明）

| 实际签名 | 类型 | 定义入口 |
| --- | --- | --- |
| `explicit McpServer(const juce::StringArray& extraRoots = {})` | 生命周期 | [backend/McpServer.h:18](../juce/src/backend/McpServer.h#L18) |
| `McpServer(ProjectModel& model, AudioEngine* engine, const juce::StringArray& extraRoots = {})` | 生命周期 | [backend/McpServer.h:20](../juce/src/backend/McpServer.h#L20) |
| `juce::var processRequest(const juce::var& request, bool& shouldRespond)` | 业务／适配 | [backend/McpServer.h:22](../juce/src/backend/McpServer.h#L22) |
| `juce::var executeTool(const juce::String& name, const juce::var& args)` | 业务／适配 | [backend/McpServer.h:23](../juce/src/backend/McpServer.h#L23) |
| `juce::var snapshotJson() const` | 业务／适配 | [backend/McpServer.h:24](../juce/src/backend/McpServer.h#L24) |
| `static juce::var noteJson(const NoteData& note, bool includeCurves = true)` | 业务／适配 | [backend/McpServer.h:25](../juce/src/backend/McpServer.h#L25) |
| `void allowFile(const juce::File& file)` | 业务／适配 | [backend/McpServer.h:26](../juce/src/backend/McpServer.h#L26) |
| `static juce::var result(const juce::String& text, bool failed = false)` | 业务／适配 | [backend/McpServer.h:27](../juce/src/backend/McpServer.h#L27) |
| `int run()` | 业务／适配 | [backend/McpServer.h:28](../juce/src/backend/McpServer.h#L28) |
| `static bool pathWithinRoots(const std::vector<juce::File>& roots, const juce::File& target)` | 业务／适配 | [backend/McpServer.h:32](../juce/src/backend/McpServer.h#L32) |
| `static std::vector<juce::File> projectRoots(const ProjectData& data)` | 业务／适配 | [backend/McpServer.h:36](../juce/src/backend/McpServer.h#L36) |
| `static juce::var diagnosticTools()` | 诊断 | [backend/McpServer.h:38](../juce/src/backend/McpServer.h#L38) |

## LiveMcpBridge（5 个声明）

| 实际签名 | 类型 | 定义入口 |
| --- | --- | --- |
| `LiveMcpBridge(Handler handler, std::function<juce::var()> describe)` | 生命周期 | [backend/LiveMcpBridge.h:13](../juce/src/backend/LiveMcpBridge.h#L13) |
| `~LiveMcpBridge() override` | 生命周期 | [backend/LiveMcpBridge.h:14](../juce/src/backend/LiveMcpBridge.h#L14) |
| `const juce::String& sessionId() const` | 业务／适配 | [backend/LiveMcpBridge.h:15](../juce/src/backend/LiveMcpBridge.h#L15) |
| `static int runProxy(const juce::String& session = {}, bool listOnly = false)` | 业务／适配 | [backend/LiveMcpBridge.h:16](../juce/src/backend/LiveMcpBridge.h#L16) |
| `static juce::File sessionsDirectory()` | 业务／适配 | [backend/LiveMcpBridge.h:17](../juce/src/backend/LiveMcpBridge.h#L17) |

## MCP 命令目录

Headless 命令来自 `McpServer::diagnosticTools`；Live 复用它并叠加窗口命令，并非两套完整独立业务实现。Live 写操作另有 expected_revision、会话和草稿提交约束。字段类型／必填项以链接处 schema 为准；不是所有 GUI 功能都有 MCP 命令。

| 命令 | 注册位置 | 源码描述 |
| --- | --- | --- |
| `project_new` | [backend/McpServer.cpp:443](../juce/src/backend/McpServer.cpp#L443) | Create an empty HachiShifter project / 新建工程 |
| `project_open` | [backend/McpServer.cpp:444](../juce/src/backend/McpServer.cpp#L444) | Open HJPX, legacy HSPX, MPD, MIDI, or audio from path / 打开工程或素材 |
| `project_save` | [backend/McpServer.cpp:451](../juce/src/backend/McpServer.cpp#L451) | Save current project as HJPX / 保存工程 |
| `project_snapshot` | [backend/McpServer.cpp:455](../juce/src/backend/McpServer.cpp#L455) | Read every current track, clip, note, pitch and marker / 读取全部工程内容 |
| `import_audio` | [backend/McpServer.cpp:456](../juce/src/backend/McpServer.cpp#L456) | Import an audio file at start_seconds / 导入音频 |
| `analyse_audio` | [backend/McpServer.cpp:465](../juce/src/backend/McpServer.cpp#L465) | Run configured GAME+FCPE analysis with native fallback and return actual backend / 执行 GAME+FCPE 分析并报告实际后端 |
| `analysis_status` | [backend/McpServer.cpp:471](../juce/src/backend/McpServer.cpp#L471) | Inspect GAME large/small, FCPE and inference availability / 查看 GAME、FCPE 与推理状态 |
| `import_midi` | [backend/McpServer.cpp:473](../juce/src/backend/McpServer.cpp#L473) | Import MIDI notes and tempo / 导入 MIDI |
| `export_midi` | [backend/McpServer.cpp:477](../juce/src/backend/McpServer.cpp#L477) | Write the project out as a MIDI file with its tempo map and lyrics / 导出 MIDI |
| `export_ust` | [backend/McpServer.cpp:481](../juce/src/backend/McpServer.cpp#L481) | Export one monophonic track as UST, retaining imported opaque data / 导出单轨 UST |
| `import_ust` | [backend/McpServer.cpp:486](../juce/src/backend/McpServer.cpp#L486) | Import a UTAU project as a plain UTAU track / 导入 UST |
| `import_melodyne` | [backend/McpServer.cpp:491](../juce/src/backend/McpServer.cpp#L491) | Import Melodyne MPD edits; recursive_media, preserve_edits and source_pitch control import / 导入 Melodyne 工程并控制素材搜索、工程编辑与原始 F0 |
| `set_tempo` | [backend/McpServer.cpp:505](../juce/src/backend/McpServer.cpp#L505) | Set BPM and time signature / 设置速度与拍号 |
| `add_track` | [backend/McpServer.cpp:513](../juce/src/backend/McpServer.cpp#L513) | Create a melodic, audio or accompaniment track / 新建旋律、普通或伴奏轨道 |
| `set_track` | [backend/McpServer.cpp:521](../juce/src/backend/McpServer.cpp#L521) | Set compose, mute, solo, gain, pan and render algorithms / 设置轨道及算法 |
| `set_clip` | [backend/McpServer.cpp:563](../juce/src/backend/McpServer.cpp#L563) | Set clip gain, fades and mute state / 设置采样增益、淡入淡出和静音 |
| `move_clip` | [backend/McpServer.cpp:574](../juce/src/backend/McpServer.cpp#L574) | Move a clip on the timeline / 移动采样 |
| `resize_clip` | [backend/McpServer.cpp:580](../juce/src/backend/McpServer.cpp#L580) | Stretch a whole clip while preserving its source media / 整体拉伸采样并保留原始素材 |
| `duplicate_clip` | [backend/McpServer.cpp:588](../juce/src/backend/McpServer.cpp#L588) | Deep-copy a clip and its notes to a timeline position / 深度复制采样及其音符到指定位置 |
| `transpose_note` | [backend/McpServer.cpp:596](../juce/src/backend/McpServer.cpp#L596) | Move a note and its whole contour / 整体移动音高线 |
| `edit_notes_pitch` | [backend/McpServer.cpp:602](../juce/src/backend/McpServer.cpp#L602) | Batch transpose, set, average or quantize note pitches / 批量移调、设置、平均或量化音符 |
| `duplicate_notes` | [backend/McpServer.cpp:618](../juce/src/backend/McpServer.cpp#L618) | Deep-copy selected notes into a target clip / 深度复制所选音符到目标采样 |
| `resize_note` | [backend/McpServer.cpp:628](../juce/src/backend/McpServer.cpp#L628) | Change note time bounds / 修改音符时间 |
| `set_note` | [backend/McpServer.cpp:636](../juce/src/backend/McpServer.cpp#L636) | Set pitch, Robust Pitch Curve, tension, breath, formant, gain, Attack and consonant parameters / 设置稳健音高线及全部音符参数 |
| `set_pitch_curve` | [backend/McpServer.cpp:711](../juce/src/backend/McpServer.cpp#L711) | Draw an absolute target-pitch curve without replacing source F0 / 绘制目标音高线并保留原始 F0 |
| `add_note` | [backend/McpServer.cpp:718](../juce/src/backend/McpServer.cpp#L718) | Create a note in a clip / 在采样中创建音符 |
| `remove_note` | [backend/McpServer.cpp:728](../juce/src/backend/McpServer.cpp#L728) | Delete a note / 删除音符 |
| `toggle_note_connection` | [backend/McpServer.cpp:732](../juce/src/backend/McpServer.cpp#L732) | Connect or separate adjacent notes / 连接或分离相邻音符 |
| `remove_clip` | [backend/McpServer.cpp:737](../juce/src/backend/McpServer.cpp#L737) | Delete a clip / 删除采样 |
| `remove_track` | [backend/McpServer.cpp:741](../juce/src/backend/McpServer.cpp#L741) | Delete a track / 删除轨道 |
| `undo` | [backend/McpServer.cpp:745](../juce/src/backend/McpServer.cpp#L745) | Undo the last project edit / 撤销工程编辑 |
| `redo` | [backend/McpServer.cpp:746](../juce/src/backend/McpServer.cpp#L746) | Redo the last project edit / 重做工程编辑 |
| `utau_render_selection` | [backend/McpServer.cpp:747](../juce/src/backend/McpServer.cpp#L747) | Choose which UTAU notes render and play; empty selects every note / 选择参与 UTAU 渲染与试听的音符 |
| `set_utau_resampler` | [backend/McpServer.cpp:754](../juce/src/backend/McpServer.cpp#L754) | Point UTAU rendering at a resampler executable / 指定 UTAU 重采样器 |
| `render_prepare` | [backend/McpServer.cpp:759](../juce/src/backend/McpServer.cpp#L759) | Pre-render the current project with its selected algorithms / 按当前所选算法预渲染工程 |
| `render_status` | [backend/McpServer.cpp:765](../juce/src/backend/McpServer.cpp#L765) | Read pre-render progress and active backends / 读取预渲染进度与实际后端 |
| `export_wav` | [backend/McpServer.cpp:766](../juce/src/backend/McpServer.cpp#L766) | Render every note and export to WAV; track_id exports one track alone, from_seconds/to_seconds one stretch / 渲染全曲并导出 WAV，track_id 可单独导出一个轨道 |
| `transport_play` | [backend/McpServer.cpp:781](../juce/src/backend/McpServer.cpp#L781) | Render if needed and start playback; play_until_seconds stops where a selection ends / 必要时预渲染并开始播放 |
| `transport_stop` | [backend/McpServer.cpp:789](../juce/src/backend/McpServer.cpp#L789) | Stop transport playback / 停止播放 |
| `transport_seek` | [backend/McpServer.cpp:790](../juce/src/backend/McpServer.cpp#L790) | Seek transport to position_seconds / 跳转播放位置 |
| `transport_status` | [backend/McpServer.cpp:794](../juce/src/backend/McpServer.cpp#L794) | Read playback position and render state / 读取播放位置与渲染状态 |
| `sample_settings_read` | [backend/McpServer.cpp:795](../juce/src/backend/McpServer.cpp#L795) | Read or derive audio .hjm.csv regions / 读取或生成音频 .hjm.csv 分段 |
| `sample_settings_save` | [backend/McpServer.cpp:800](../juce/src/backend/McpServer.cpp#L800) | Save audio regions to .hjm.csv / 保存音频分段到 .hjm.csv |
| `oto_import` | [backend/McpServer.cpp:807](../juce/src/backend/McpServer.cpp#L807) | Import one audio file's regions from UTAU oto.ini / 从 UTAU oto.ini 导入单个音频分段 |
| `oto_export` | [backend/McpServer.cpp:816](../juce/src/backend/McpServer.cpp#L816) | Export one audio file's regions to UTAU oto.ini / 将单个音频分段导出为 UTAU oto.ini |
| `jie_oto_create` | [backend/McpServer.cpp:822](../juce/src/backend/McpServer.cpp#L822) | Seed a voicebank's four-region oto4.ini from its oto.ini / 按 oto.ini 生成界•OTO |
| `voicebank_import` | [backend/McpServer.cpp:826](../juce/src/backend/McpServer.cpp#L826) | Import an UTAU voicebank and create .hjm.csv sidecars / 导入 UTAU 音源并生成 .hjm.csv |
| `read_file` | [backend/McpServer.cpp:830](../juce/src/backend/McpServer.cpp#L830) | Read a byte range as base64 / 读取任意文件内容 |
| `list_directory` | [backend/McpServer.cpp:838](../juce/src/backend/McpServer.cpp#L838) | List a directory with type and size / 列出目录内容 |
| `editor_selection` | [MainComponentMcp.cpp:57](../juce/src/MainComponentMcp.cpp#L57) | Read ONLY notes currently selected in the editor / 读取当前选区。Use FIRST for 'these notes', 'this phrase', or 'what did I select'. Returns lyrics, note names, timing and tracks; empty selection returns no notes, NEVER the whole project. |
| `editor_status` | [MainComponentMcp.cpp:60](../juce/src/MainComponentMcp.cpp#L60) | Read the connected window, current file, revision, selection, playback and DS state |
| `editor_query` | [MainComponentMcp.cpp:61](../juce/src/MainComponentMcp.cpp#L61) | Read selected notes by default; range uses absolute project seconds; curves are opt-in |
| `editor_select` | [MainComponentMcp.cpp:66](../juce/src/MainComponentMcp.cpp#L66) | Change real GUI selection and focus; [] clears it |
| `editor_batch` | [MainComponentMcp.cpp:67](../juce/src/MainComponentMcp.cpp#L67) | Apply up to 128 project edit commands atomically with one Undo; imports, file and transport commands are excluded |
| `editor_job_status` | [MainComponentMcp.cpp:69](../juce/src/MainComponentMcp.cpp#L69) | Read a background job's state/result; completed does not imply success, check result.isError |
| `editor_cancel_job` | [MainComponentMcp.cpp:70](../juce/src/MainComponentMcp.cpp#L70) | Cancel a pending job; staged edits are discarded, external file writes may already have completed |
| `ds_generate_pitch` | [MainComponentMcp.cpp:71](../juce/src/MainComponentMcp.cpp#L71) | Regenerate DS pitch using GUI logic; selection required unless whole_track=true |
| `ds_generate_parameters` | [MainComponentMcp.cpp:72](../juce/src/MainComponentMcp.cpp#L72) | Regenerate DS prediction parameters using GUI logic; preserve offsets and unselected notes |
| `ds_query_phonemes` | [MainComponentMcp.cpp:73](../juce/src/MainComponentMcp.cpp#L73) | Predict phoneme timing in full phrase context, returning only selected notes by default; includes stable note IDs and timing context |
| `ds_capabilities` | [MainComponentMcp.cpp:74](../juce/src/MainComponentMcp.cpp#L74) | Inspect the focused DS voicebank and its parameter support asynchronously |
| `ds_set_pitch_offset` | [MainComponentMcp.cpp:75](../juce/src/MainComponentMcp.cpp#L75) | Set an independent additive semitone curve; preserves the original prediction reference |
| `ds_set_parameter` | [MainComponentMcp.cpp:77](../juce/src/MainComponentMcp.cpp#L77) | Set a DS actual parameter or offset curve; supports only parameters exposed by this voicebank |
| `ds_set_pronunciation` | [MainComponentMcp.cpp:80](../juce/src/MainComponentMcp.cpp#L80) | Set per-note pronunciation override; empty text restores automatic conversion |
| `ds_set_timing` | [MainComponentMcp.cpp:82](../juce/src/MainComponentMcp.cpp#L82) | Set phoneme onset overrides from an inspected timing context; null restores prediction |
| `hamood_analyse_audio` | [MainComponentMcp.cpp:84](../juce/src/MainComponentMcp.cpp#L84) | Analyse one accompaniment clip with Beat This! small0 + BTC; background job, cached by source audio/model hash. Read-only project operation. No pitch or tempo edits. |
| `hamood_alignment_preview` | [MainComponentMcp.cpp:86](../juce/src/MainComponentMcp.cpp#L86) | Preview a small beat-phase offset against the existing project tempo grid. Does NOT resolve whole-bar ambiguity or change audio/tempo. Inspect start/middle/end windows before applying move_clip. |
| `hamood_audio_context` | [MainComponentMcp.cpp:90](../juce/src/MainComponentMcp.cpp#L90) | Read cached timestamped chords, low-register chroma candidates, beats and downbeats on the CURRENT project timeline. Scores are not calibrated probabilities. Defaults to selected-note time range; explicit range or whole_clip required otherwise. |
| `hamood_get_context` | [MainComponentMcp.cpp:95](../juce/src/MainComponentMcp.cpp#L95) | Read project-persisted HAMOOD settings, manual key ranges, chord timeline, section labels and confirmation states. Timeline positions use quarter notes. |
| `hamood_set_context` | [MainComponentMcp.cpp:96](../juce/src/MainComponentMcp.cpp#L96) | Replace HAMOOD project metadata with one undo step; read hamood_get_context first and preserve unrelated entries. schema=1; settings object; chords, sections, key_predictions arrays. Chords require start_quarter/end_quarter, label, pitch_classes 0..11, score 0..1 and confirmed. Confirmed chords survive audio reanalysis. |
| `hamood_preview` | [MainComponentMcp.cpp:98](../juce/src/MainComponentMcp.cpp#L98) | 只读预览和声方案 |
| `hamood_generate` | [MainComponentMcp.cpp:98](../juce/src/MainComponentMcp.cpp#L98) | 生成独立和声轨道，一次撤销 |

## 启动参数字面量

仅来自 Main.cpp 的 `--…` 字符串去重；包括诊断、启动选项和帮助文字，不代表全是用户功能，也不等同于测试已执行。参数说明见原有 [命令索引](AI_SOURCE_INDEX.md)。

| 参数 | 首次出现位置 |
| --- | --- |
| `--audit-hamood-keys` | [Main.cpp:134](../juce/src/Main.cpp#L134) |
| `--smoke-hamood-utau` | [Main.cpp:153](../juce/src/Main.cpp#L153) |
| `--smoke-hamood` | [Main.cpp:159](../juce/src/Main.cpp#L159) |
| `--smoke-diffsinger-pitch-offset` | [Main.cpp:166](../juce/src/Main.cpp#L166) |
| `--smoke-accompaniment-tempo` | [Main.cpp:173](../juce/src/Main.cpp#L173) |
| `--smoke-accompaniment` | [Main.cpp:189](../juce/src/Main.cpp#L189) |
| `--smoke-wav-export-options` | [Main.cpp:196](../juce/src/Main.cpp#L196) |
| `--smoke-diffsinger-pitch-persistence` | [Main.cpp:206](../juce/src/Main.cpp#L206) |
| `--reopen` | [Main.cpp:210](../juce/src/Main.cpp#L210) |
| `--smoke-diffsinger-pitch-handles` | [Main.cpp:214](../juce/src/Main.cpp#L214) |
| `--smoke-diffsinger-pitch-restore` | [Main.cpp:220](../juce/src/Main.cpp#L220) |
| `--smoke-diffsinger-pitch-reference` | [Main.cpp:227](../juce/src/Main.cpp#L227) |
| `--smoke-continuous-flag` | [Main.cpp:233](../juce/src/Main.cpp#L233) |
| `--smoke-classic-flag-ui` | [Main.cpp:240](../juce/src/Main.cpp#L240) |
| `--smoke-diffsinger-inference` | [Main.cpp:249](../juce/src/Main.cpp#L249) |
| `--smoke-diffsinger-parameter-retake` | [Main.cpp:255](../juce/src/Main.cpp#L255) |
| `--smoke-diffsinger-cache` | [Main.cpp:262](../juce/src/Main.cpp#L262) |
| `--smoke-diffsinger-parameters` | [Main.cpp:267](../juce/src/Main.cpp#L267) |
| `--smoke-diffsinger-expressions` | [Main.cpp:272](../juce/src/Main.cpp#L272) |
| `--smoke-diffsinger-consonants` | [Main.cpp:279](../juce/src/Main.cpp#L279) |
| `--smoke-diffsinger-timing` | [Main.cpp:285](../juce/src/Main.cpp#L285) |
| `--smoke-diffsinger-ui` | [Main.cpp:292](../juce/src/Main.cpp#L292) |
| `--smoke-diffsinger` | [Main.cpp:301](../juce/src/Main.cpp#L301) |
| `--smoke-chinese-cvvc` | [Main.cpp:308](../juce/src/Main.cpp#L308) |
| `--smoke-output-engine` | [Main.cpp:316](../juce/src/Main.cpp#L316) |
| `--smoke-native-vibrato` | [Main.cpp:325](../juce/src/Main.cpp#L325) |
| `--smoke-native-vibrato-render` | [Main.cpp:336](../juce/src/Main.cpp#L336) |
| `--smoke-native-noise-option` | [Main.cpp:342](../juce/src/Main.cpp#L342) |
| `--smoke-native-incremental` | [Main.cpp:350](../juce/src/Main.cpp#L350) |
| `--smoke-native-shutdown` | [Main.cpp:358](../juce/src/Main.cpp#L358) |
| `--smoke-native-noise` | [Main.cpp:366](../juce/src/Main.cpp#L366) |
| `--smoke-wcs-nonloop` | [Main.cpp:373](../juce/src/Main.cpp#L373) |
| `--smoke-hn-cache` | [Main.cpp:380](../juce/src/Main.cpp#L380) |
| `--force` | [Main.cpp:383](../juce/src/Main.cpp#L383) |
| `--smoke-hifisampler` | [Main.cpp:387](../juce/src/Main.cpp#L387) |
| `--smoke-hifishifter-mel` | [Main.cpp:387](../juce/src/Main.cpp#L387) |
| `--smoke-nsf-regions` | [Main.cpp:396](../juce/src/Main.cpp#L396) |
| `--smoke-nsf-pitch-transitions` | [Main.cpp:404](../juce/src/Main.cpp#L404) |
| `--smoke-voicebank-import` | [Main.cpp:410](../juce/src/Main.cpp#L410) |
| `--smoke-voicebank-selection` | [Main.cpp:416](../juce/src/Main.cpp#L416) |
| `--smoke-nsf-picker` | [Main.cpp:425](../juce/src/Main.cpp#L425) |
| `--smoke-cross-region-editing` | [Main.cpp:434](../juce/src/Main.cpp#L434) |
| `--smoke-hamood-persistence` | [Main.cpp:434](../juce/src/Main.cpp#L434) |
| `--smoke-mou-to-jie` | [Main.cpp:441](../juce/src/Main.cpp#L441) |
| `--smoke-oto-overlap` | [Main.cpp:447](../juce/src/Main.cpp#L447) |
| `--smoke-modeless-oto` | [Main.cpp:453](../juce/src/Main.cpp#L453) |
| `--smoke-oto-continuity` | [Main.cpp:458](../juce/src/Main.cpp#L458) |
| `--smoke-native-envelope` | [Main.cpp:463](../juce/src/Main.cpp#L463) |
| `--smoke-cvvc-layout-cache` | [Main.cpp:468](../juce/src/Main.cpp#L468) |
| `--smoke-auto-crossfade` | [Main.cpp:473](../juce/src/Main.cpp#L473) |
| `--smoke-pitch-point-selection` | [Main.cpp:478](../juce/src/Main.cpp#L478) |
| `--smoke-point-group` | [Main.cpp:483](../juce/src/Main.cpp#L483) |
| `--smoke-envelope-point-selection` | [Main.cpp:488](../juce/src/Main.cpp#L488) |
| `--smoke-native-linked-envelope` | [Main.cpp:493](../juce/src/Main.cpp#L493) |
| `--smoke-advanced-envelope` | [Main.cpp:498](../juce/src/Main.cpp#L498) |
| `--smoke-advanced-envelope-panel` | [Main.cpp:498](../juce/src/Main.cpp#L498) |
| `--smoke-playback-priority` | [Main.cpp:503](../juce/src/Main.cpp#L503) |
| `--smoke-ust-fidelity` | [Main.cpp:504](../juce/src/Main.cpp#L504) |
| `--smoke-project-safety` | [Main.cpp:505](../juce/src/Main.cpp#L505) |
| `--smoke-normal-display` | [Main.cpp:513](../juce/src/Main.cpp#L513) |
| `--smoke-note-hints` | [Main.cpp:521](../juce/src/Main.cpp#L521) |
| `--smoke-clip-editor-scope` | [Main.cpp:529](../juce/src/Main.cpp#L529) |
| `--smoke-empty-tuning-clip` | [Main.cpp:537](../juce/src/Main.cpp#L537) |
| `--smoke-track-gain-envelope` | [Main.cpp:545](../juce/src/Main.cpp#L545) |
| `--smoke-native-rendered-waveform` | [Main.cpp:553](../juce/src/Main.cpp#L553) |
| `--smoke-native-note-move` | [Main.cpp:560](../juce/src/Main.cpp#L560) |
| `--smoke-native-source-pitch-restore` | [Main.cpp:566](../juce/src/Main.cpp#L566) |
| `--smoke-native-audio-overlap-focus` | [Main.cpp:572](../juce/src/Main.cpp#L572) |
| `--smoke-native-audio-overlap` | [Main.cpp:578](../juce/src/Main.cpp#L578) |
| `--smoke-native-linked-pitch` | [Main.cpp:584](../juce/src/Main.cpp#L584) |
| `--smoke-source-breathiness` | [Main.cpp:591](../juce/src/Main.cpp#L591) |
| `--smoke-native-unpitched-regions` | [Main.cpp:598](../juce/src/Main.cpp#L598) |
| `--smoke-native-consonant-display` | [Main.cpp:606](../juce/src/Main.cpp#L606) |
| `--smoke-native-pitch-visibility` | [Main.cpp:613](../juce/src/Main.cpp#L613) |
| `--smoke-native-audio-link` | [Main.cpp:619](../juce/src/Main.cpp#L619) |
| `--smoke-native-note-join` | [Main.cpp:625](../juce/src/Main.cpp#L625) |
| `--smoke-native-trim-expand` | [Main.cpp:630](../juce/src/Main.cpp#L630) |
| `--smoke-native-audio-trim` | [Main.cpp:635](../juce/src/Main.cpp#L635) |
| `--smoke-native-audio-disconnect` | [Main.cpp:641](../juce/src/Main.cpp#L641) |
| `--smoke-native-note-copy-paste` | [Main.cpp:647](../juce/src/Main.cpp#L647) |
| `--smoke-native-import-performance` | [Main.cpp:654](../juce/src/Main.cpp#L654) |
| `--smoke-native-source-pitch` | [Main.cpp:661](../juce/src/Main.cpp#L661) |
| `--smoke-native-waveform-preview` | [Main.cpp:669](../juce/src/Main.cpp#L669) |
| `--smoke-timeline-notes` | [Main.cpp:677](../juce/src/Main.cpp#L677) |
| `--smoke-clip-gain-knob` | [Main.cpp:685](../juce/src/Main.cpp#L685) |
| `--smoke-clip-merge` | [Main.cpp:693](../juce/src/Main.cpp#L693) |
| `--smoke-timeline-selection` | [Main.cpp:701](../juce/src/Main.cpp#L701) |
| `--smoke-clip-trim` | [Main.cpp:709](../juce/src/Main.cpp#L709) |
| `--smoke-clip-split` | [Main.cpp:718](../juce/src/Main.cpp#L718) |
| `--smoke-independent-zoom` | [Main.cpp:727](../juce/src/Main.cpp#L727) |
| `--smoke-scroll-zoom` | [Main.cpp:736](../juce/src/Main.cpp#L736) |
| `--preview-note-dance` | [Main.cpp:745](../juce/src/Main.cpp#L745) |
| `--smoke-tempo-change` | [Main.cpp:753](../juce/src/Main.cpp#L753) |
| `--smoke-timeline-pit` | [Main.cpp:762](../juce/src/Main.cpp#L762) |
| `--mcp-live` | [Main.cpp:768](../juce/src/Main.cpp#L768) |
| `--mcp-list-sessions` | [Main.cpp:768](../juce/src/Main.cpp#L768) |
| `--session` | [Main.cpp:771](../juce/src/Main.cpp#L771) |
| `--smoke-hifisampler-envelope` | [Main.cpp:776](../juce/src/Main.cpp#L776) |
| `--smoke-nsf-project-note` | [Main.cpp:782](../juce/src/Main.cpp#L782) |
| `--mcp` | [Main.cpp:788](../juce/src/Main.cpp#L788) |
| `--roots` | [Main.cpp:790](../juce/src/Main.cpp#L790) |
| `--smoke-oto-playback` | [Main.cpp:802](../juce/src/Main.cpp#L802) |
| `--smoke-oto-editor` | [Main.cpp:1002](../juce/src/Main.cpp#L1002) |
| `--click` | [Main.cpp:1113](../juce/src/Main.cpp#L1113) |
| `--smoke-splice-envelope` | [Main.cpp:1125](../juce/src/Main.cpp#L1125) |
| `--smoke-dialog-enter` | [Main.cpp:1235](../juce/src/Main.cpp#L1235) |
| `--smoke-source-edit-view` | [Main.cpp:1293](../juce/src/Main.cpp#L1293) |
| `--smoke-consonant-hold` | [Main.cpp:1355](../juce/src/Main.cpp#L1355) |
| `--smoke-mou-panel-counts` | [Main.cpp:1575](../juce/src/Main.cpp#L1575) |
| `--smoke-mou-consonant-ticks` | [Main.cpp:1666](../juce/src/Main.cpp#L1666) |
| `--smoke-mou-boundary-drag` | [Main.cpp:1819](../juce/src/Main.cpp#L1819) |
| `--smoke-prefix-map` | [Main.cpp:1908](../juce/src/Main.cpp#L1908) |
| `--repair-mou-oto` | [Main.cpp:1949](../juce/src/Main.cpp#L1949) |
| `--smoke-mou-oto-file` | [Main.cpp:1970](../juce/src/Main.cpp#L1970) |
| `--smoke-utau-mode` | [Main.cpp:2169](../juce/src/Main.cpp#L2169) |
| `--smoke-lane-marquee` | [Main.cpp:2309](../juce/src/Main.cpp#L2309) |
| `--smoke-flag-lane` | [Main.cpp:2410](../juce/src/Main.cpp#L2410) |
| `--smoke-keyboard-labels` | [Main.cpp:3271](../juce/src/Main.cpp#L3271) |
| `--smoke-lane-labels` | [Main.cpp:3325](../juce/src/Main.cpp#L3325) |
| `--smoke-export-targets` | [Main.cpp:3353](../juce/src/Main.cpp#L3353) |
| `--smoke-audition-position` | [Main.cpp:3459](../juce/src/Main.cpp#L3459) |
| `--smoke-lyric-envelope` | [Main.cpp:3501](../juce/src/Main.cpp#L3501) |
| `--smoke-lyric-timing` | [Main.cpp:3573](../juce/src/Main.cpp#L3573) |
| `--smoke-vibrato-bake` | [Main.cpp:3672](../juce/src/Main.cpp#L3672) |
| `--smoke-mou-note-regions` | [Main.cpp:3785](../juce/src/Main.cpp#L3785) |
| `--smoke-region-guides` | [Main.cpp:4071](../juce/src/Main.cpp#L4071) |
| `--smoke-rest-playback` | [Main.cpp:4145](../juce/src/Main.cpp#L4145) |
| `--smoke-rest-lyric` | [Main.cpp:4241](../juce/src/Main.cpp#L4241) |
| `--smoke-missing-alias-piano` | [Main.cpp:4404](../juce/src/Main.cpp#L4404) |
| `--smoke-flag-curve-mode` | [Main.cpp:4531](../juce/src/Main.cpp#L4531) |
| `--smoke-flag-point-value` | [Main.cpp:4716](../juce/src/Main.cpp#L4716) |
| `--smoke-flag-overlap` | [Main.cpp:4806](../juce/src/Main.cpp#L4806) |
| `--smoke-load-timing` | [Main.cpp:4921](../juce/src/Main.cpp#L4921) |
| `--smoke-flatten-pitch-line` | [Main.cpp:5034](../juce/src/Main.cpp#L5034) |
| `--smoke-note-across-tracks` | [Main.cpp:5194](../juce/src/Main.cpp#L5194) |
| `--smoke-clip-across-tracks` | [Main.cpp:5350](../juce/src/Main.cpp#L5350) |
| `--smoke-reference-track` | [Main.cpp:5499](../juce/src/Main.cpp#L5499) |
| `--smoke-native-pitch-seam` | [Main.cpp:5647](../juce/src/Main.cpp#L5647) |
| `--smoke-native-pitch-points` | [Main.cpp:5719](../juce/src/Main.cpp#L5719) |
| `--smoke-paste-at-pointer` | [Main.cpp:5943](../juce/src/Main.cpp#L5943) |
| `--smoke-note-menu-split` | [Main.cpp:6086](../juce/src/Main.cpp#L6086) |
| `--smoke-lyric-gate` | [Main.cpp:6268](../juce/src/Main.cpp#L6268) |
| `--smoke-lyric-tab` | [Main.cpp:6365](../juce/src/Main.cpp#L6365) |
| `--smoke-syllable-cuts` | [Main.cpp:6565](../juce/src/Main.cpp#L6565) |
| `--smoke-single-syllable` | [Main.cpp:6747](../juce/src/Main.cpp#L6747) |
| `--smoke-deleted-notes-silent` | [Main.cpp:6854](../juce/src/Main.cpp#L6854) |
| `--smoke-delete-clip` | [Main.cpp:7001](../juce/src/Main.cpp#L7001) |
| `--smoke-draw-unit` | [Main.cpp:7098](../juce/src/Main.cpp#L7098) |
| `--smoke-new-track-draw` | [Main.cpp:7191](../juce/src/Main.cpp#L7191) |
| `--smoke-draw-drag` | [Main.cpp:7298](../juce/src/Main.cpp#L7298) |
| `--smoke-draw-overlap` | [Main.cpp:7469](../juce/src/Main.cpp#L7469) |
| `--smoke-dropdown-arrow` | [Main.cpp:7653](../juce/src/Main.cpp#L7653) |
| `--smoke-pitch-line` | [Main.cpp:7744](../juce/src/Main.cpp#L7744) |
| `--smoke-view-menu` | [Main.cpp:8035](../juce/src/Main.cpp#L8035) |
| `--inspect-melodyne-tracks` | [Main.cpp:8148](../juce/src/Main.cpp#L8148) |
| `--smoke-melodyne-provider` | [Main.cpp:8159](../juce/src/Main.cpp#L8159) |
| `--probe-melodyne-vst3-instance` | [Main.cpp:8184](../juce/src/Main.cpp#L8184) |
| `--smoke-native-timing` | [Main.cpp:8196](../juce/src/Main.cpp#L8196) |
| `--smoke-integrated-ui` | [Main.cpp:8253](../juce/src/Main.cpp#L8253) |
| `--smoke-integrated` | [Main.cpp:8262](../juce/src/Main.cpp#L8262) |
| `--smoke-native-hjm` | [Main.cpp:8268](../juce/src/Main.cpp#L8268) |
| `--smoke-batch-lyrics` | [Main.cpp:8335](../juce/src/Main.cpp#L8335) |
| `--smoke-envelope-lanes` | [Main.cpp:8427](../juce/src/Main.cpp#L8427) |
| `--smoke-wheel` | [Main.cpp:8486](../juce/src/Main.cpp#L8486) |
| `--smoke-waveform-toggle` | [Main.cpp:8527](../juce/src/Main.cpp#L8527) |
| `--smoke-envelope-base` | [Main.cpp:8637](../juce/src/Main.cpp#L8637) |
| `--smoke-amplitude-waveform` | [Main.cpp:8958](../juce/src/Main.cpp#L8958) |
| `--smoke-vibrato-end` | [Main.cpp:9416](../juce/src/Main.cpp#L9416) |
| `--smoke-hanzi-pinyin` | [Main.cpp:9676](../juce/src/Main.cpp#L9676) |
| `--smoke-shared-pitch-line` | [Main.cpp:9815](../juce/src/Main.cpp#L9815) |
| `--smoke-waveform-alignment` | [Main.cpp:10473](../juce/src/Main.cpp#L10473) |
| `--smoke-note-waveform` | [Main.cpp:10672](../juce/src/Main.cpp#L10672) |
| `--smoke-utau-waveform` | [Main.cpp:10978](../juce/src/Main.cpp#L10978) |
| `--smoke-voicebank-index` | [Main.cpp:11152](../juce/src/Main.cpp#L11152) |
| `--smoke-ust-pitch` | [Main.cpp:11618](../juce/src/Main.cpp#L11618) |
| `--low` | [Main.cpp:11923](../juce/src/Main.cpp#L11923) |
| `--smoke-hf-daemon-owner` | [Main.cpp:12042](../juce/src/Main.cpp#L12042) |
| `--quit` | [Main.cpp:12050](../juce/src/Main.cpp#L12050) |
| `--smoke-hf-prewarm` | [Main.cpp:12055](../juce/src/Main.cpp#L12055) |
| `--smoke-ust-vibrato` | [Main.cpp:12188](../juce/src/Main.cpp#L12188) |
| `--smoke-mcp-roots` | [Main.cpp:12433](../juce/src/Main.cpp#L12433) |
| `--smoke-mcp-schema` | [Main.cpp:12540](../juce/src/Main.cpp#L12540) |
| `--smoke-dragged-transition` | [Main.cpp:12705](../juce/src/Main.cpp#L12705) |
| `--smoke-forward-bend` | [Main.cpp:12928](../juce/src/Main.cpp#L12928) |
| `--smoke-midi-track-import` | [Main.cpp:13100](../juce/src/Main.cpp#L13100) |
| `--smoke-midi-export` | [Main.cpp:13429](../juce/src/Main.cpp#L13429) |
| `--sounding` | [Main.cpp:13554](../juce/src/Main.cpp#L13554) |
| `--still` | [Main.cpp:13686](../juce/src/Main.cpp#L13686) |
| `--smoke-ust-import` | [Main.cpp:13722](../juce/src/Main.cpp#L13722) |
| `--smoke-ust-encoding` | [Main.cpp:13863](../juce/src/Main.cpp#L13863) |
| `--smoke-ust` | [Main.cpp:14015](../juce/src/Main.cpp#L14015) |
| `--smoke-active-resampler` | [Main.cpp:14220](../juce/src/Main.cpp#L14220) |
| `--smoke-bundled-resampler` | [Main.cpp:14265](../juce/src/Main.cpp#L14265) |
| `--smoke-anchor-frequency` | [Main.cpp:14323](../juce/src/Main.cpp#L14323) |
| `--smoke-note-oto` | [Main.cpp:14509](../juce/src/Main.cpp#L14509) |
| `--smoke-prefix-jie` | [Main.cpp:14939](../juce/src/Main.cpp#L14939) |
| `--smoke-prefix-note` | [Main.cpp:15093](../juce/src/Main.cpp#L15093) |
| `--smoke-utau-overlap` | [Main.cpp:15450](../juce/src/Main.cpp#L15450) |
| `--smoke-note-stp` | [Main.cpp:15765](../juce/src/Main.cpp#L15765) |
| `--smoke-track-toggle-tips` | [Main.cpp:16004](../juce/src/Main.cpp#L16004) |
| `--smoke-track-area-menu` | [Main.cpp:16251](../juce/src/Main.cpp#L16251) |
| `--smoke-stretch-items` | [Main.cpp:16352](../juce/src/Main.cpp#L16352) |
| `--smoke-render-order` | [Main.cpp:16404](../juce/src/Main.cpp#L16404) |
| `--smoke-llsm2-length` | [Main.cpp:16536](../juce/src/Main.cpp#L16536) |
| `--smoke-glide-seam` | [Main.cpp:16588](../juce/src/Main.cpp#L16588) |
| `--smoke-native-envelope-base` | [Main.cpp:16652](../juce/src/Main.cpp#L16652) |
| `--smoke-nsf-utau-phrase` | [Main.cpp:16736](../juce/src/Main.cpp#L16736) |
| `--smoke-nsf-utau-synth` | [Main.cpp:16768](../juce/src/Main.cpp#L16768) |
| `--smoke-nsf-utau-f0` | [Main.cpp:16816](../juce/src/Main.cpp#L16816) |
| `--smoke-nsf-utau-mix` | [Main.cpp:16861](../juce/src/Main.cpp#L16861) |
| `--smoke-nsf-utau-plan` | [Main.cpp:16926](../juce/src/Main.cpp#L16926) |
| `--smoke-render-capability` | [Main.cpp:16983](../juce/src/Main.cpp#L16983) |
| `--smoke-oto-hjm-discipline` | [Main.cpp:17034](../juce/src/Main.cpp#L17034) |
| `--smoke-asset-register-dir` | [Main.cpp:17104](../juce/src/Main.cpp#L17104) |
| `--smoke-oto-audio` | [Main.cpp:17148](../juce/src/Main.cpp#L17148) |
| `--smoke-asset-audio` | [Main.cpp:17161](../juce/src/Main.cpp#L17161) |
| `--smoke-project-voicebank-assets` | [Main.cpp:17175](../juce/src/Main.cpp#L17175) |
| `--smoke-asset-search` | [Main.cpp:17181](../juce/src/Main.cpp#L17181) |
| `--smoke-asset-manager` | [Main.cpp:17190](../juce/src/Main.cpp#L17190) |
| `--smoke-gap-menu` | [Main.cpp:17310](../juce/src/Main.cpp#L17310) |
| `--smoke-insert-gap` | [Main.cpp:17429](../juce/src/Main.cpp#L17429) |
| `--smoke-ripple-delete` | [Main.cpp:17545](../juce/src/Main.cpp#L17545) |
| `--smoke-paste-notes` | [Main.cpp:17634](../juce/src/Main.cpp#L17634) |
| `--smoke-consonant-edge` | [Main.cpp:17810](../juce/src/Main.cpp#L17810) |
| `--smoke-consonant-reset` | [Main.cpp:17966](../juce/src/Main.cpp#L17966) |
| `--smoke-region-split` | [Main.cpp:18073](../juce/src/Main.cpp#L18073) |
| `--smoke-vibrato-presets` | [Main.cpp:18165](../juce/src/Main.cpp#L18165) |
| `--smoke-follow-rule` | [Main.cpp:18235](../juce/src/Main.cpp#L18235) |
| `--smoke-play-until` | [Main.cpp:18307](../juce/src/Main.cpp#L18307) |
| `--smoke-flags-field` | [Main.cpp:18366](../juce/src/Main.cpp#L18366) |
| `--smoke-vertical-drag` | [Main.cpp:18431](../juce/src/Main.cpp#L18431) |
| `--smoke-scroll-sync` | [Main.cpp:18524](../juce/src/Main.cpp#L18524) |
| `--smoke-edit-follow` | [Main.cpp:18605](../juce/src/Main.cpp#L18605) |
| `--smoke-note-drag-threshold` | [Main.cpp:18705](../juce/src/Main.cpp#L18705) |
| `--smoke-roll-drag` | [Main.cpp:18780](../juce/src/Main.cpp#L18780) |
| `--smoke-playhead-band` | [Main.cpp:18911](../juce/src/Main.cpp#L18911) |
| `--smoke-roll-cull` | [Main.cpp:18964](../juce/src/Main.cpp#L18964) |
| `--smoke-roll-timing` | [Main.cpp:19037](../juce/src/Main.cpp#L19037) |
| `--smoke-envelope-shapes` | [Main.cpp:19083](../juce/src/Main.cpp#L19083) |
| `--smoke-envelope-presets` | [Main.cpp:19400](../juce/src/Main.cpp#L19400) |
| `--smoke-piano-roll` | [Main.cpp:19416](../juce/src/Main.cpp#L19416) |
| `--render-project` | [Main.cpp:19464](../juce/src/Main.cpp#L19464) |
| `--inspect-project` | [Main.cpp:19541](../juce/src/Main.cpp#L19541) |
| `--inspect-settings` | [Main.cpp:19577](../juce/src/Main.cpp#L19577) |
| `--smoke-oto-spectrum` | [Main.cpp:19604](../juce/src/Main.cpp#L19604) |
| `--smoke-game-defaults` | [Main.cpp:19611](../juce/src/Main.cpp#L19611) |
| `--inspect-analysis` | [Main.cpp:19617](../juce/src/Main.cpp#L19617) |
| `--inspect-fcpe` | [Main.cpp:19639](../juce/src/Main.cpp#L19639) |
| `--smoke-export` | [Main.cpp:19665](../juce/src/Main.cpp#L19665) |
| `--smoke-mld5` | [Main.cpp:19695](../juce/src/Main.cpp#L19695) |
| `--smoke-utau-selection` | [Main.cpp:19788](../juce/src/Main.cpp#L19788) |
| `--smoke-utau-voicebank` | [Main.cpp:19902](../juce/src/Main.cpp#L19902) |
| `--smoke-utau` | [Main.cpp:19985](../juce/src/Main.cpp#L19985) |
| `--inspect-midi` | [Main.cpp:20071](../juce/src/Main.cpp#L20071) |
| `--inspect-audio` | [Main.cpp:20094](../juce/src/Main.cpp#L20094) |
| `--inspect-mpd` | [Main.cpp:20129](../juce/src/Main.cpp#L20129) |

## 范围与静态体量

扫描自有 `juce/src` 非测试 `.h/.cpp/.inc`：151 个文件，95,153 行（包括注释和空行）。不含 juce/third_party、tests 和名称带 Smoke 的包含单元。

| 文件 | 行数 |
| --- | --- |
| [juce/src/Main.cpp](../juce/src/Main.cpp) | 20,297 |
| [juce/src/PianoRollComponent.cpp](../juce/src/PianoRollComponent.cpp) | 10,909 |
| [juce/src/ProjectModel.cpp](../juce/src/ProjectModel.cpp) | 8,790 |
| [juce/src/MainComponent.cpp](../juce/src/MainComponent.cpp) | 6,391 |
| [juce/src/AudioEngine.cpp](../juce/src/AudioEngine.cpp) | 3,586 |
| [juce/src/backend/UtauRenderer.cpp](../juce/src/backend/UtauRenderer.cpp) | 2,747 |
| [juce/src/PinyinTable.inc](../juce/src/PinyinTable.inc) | 2,709 |
| [juce/src/backend/McpServer.cpp](../juce/src/backend/McpServer.cpp) | 2,066 |
| [juce/src/backend/NsfHifiganRenderer.cpp](../juce/src/backend/NsfHifiganRenderer.cpp) | 2,039 |
| [juce/src/SampleSettings.cpp](../juce/src/SampleSettings.cpp) | 1,982 |
| [juce/src/OtoWaveformEditorComponent.cpp](../juce/src/OtoWaveformEditorComponent.cpp) | 1,664 |
| [juce/src/PianoRollComponent.h](../juce/src/PianoRollComponent.h) | 1,484 |
| [juce/src/ProjectModel.h](../juce/src/ProjectModel.h) | 1,324 |
| [juce/src/AssetManagerComponent.cpp](../juce/src/AssetManagerComponent.cpp) | 1,281 |
| [juce/src/backend/RenderService.cpp](../juce/src/backend/RenderService.cpp) | 1,174 |

完整扫描指纹及接口条目：[interface-audit-inventory.json](interface-audit-inventory.json)。行数用于导航和识别职责集中，不是性能或质量评分。
