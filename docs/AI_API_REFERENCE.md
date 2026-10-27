# 实际接口声明清单（自动生成）

核对日期：2026-10-27。配套解读：[功能主入口与架构审计](AI_INTERFACE_ARCHITECTURE.md)。

这是源文件导航，不是承诺稳定的公共 SDK。仅枚举下列 11 个边界类的公开方法，不把 private 函数、UI 回调、纯计算辅助函数或每个第三方函数算作产品接口。重载各列一行；诊断接口单独标记。源码行号为生成时位置。

生成命令：`python tools/generate-interface-reference.py`。词法提取不是 C++ 编译器，不能用条目数证明所有业务路径正确；新增边界类时需更新脚本 HEADERS。

## ProjectModel（204 个声明）

| 实际签名 | 类型 | 定义入口 |
| --- | --- | --- |
| `ProjectModel()` | 生命周期 | [ProjectModel.h:1034](../src/ProjectModel.h#L1034) |
| `ProjectData snapshot() const` | 业务／适配 | [ProjectModel.h:1036](../src/ProjectModel.h#L1036) |
| `std::uint64_t revisionNumber() const` | 业务／适配 | [ProjectModel.h:1037](../src/ProjectModel.h#L1037) |
| `void replace(ProjectData replacement)` | 业务／适配 | [ProjectModel.h:1038](../src/ProjectModel.h#L1038) |
| `void resetDocument(ProjectData replacement = {})` | 业务／适配 | [ProjectModel.h:1040](../src/ProjectModel.h#L1040) |
| `void clear()` | 业务／适配 | [ProjectModel.h:1041](../src/ProjectModel.h#L1041) |
| `void setAssetManagerVisible(bool visible)` | 业务／适配 | [ProjectModel.h:1043](../src/ProjectModel.h#L1043) |
| `bool setHamoodState(const juce::String& state, juce::String& error)` | 业务／适配 | [ProjectModel.h:1044](../src/ProjectModel.h#L1044) |
| `bool undo()` | 业务／适配 | [ProjectModel.h:1045](../src/ProjectModel.h#L1045) |
| `bool redo()` | 业务／适配 | [ProjectModel.h:1046](../src/ProjectModel.h#L1046) |
| `bool canUndo() const` | 业务／适配 | [ProjectModel.h:1047](../src/ProjectModel.h#L1047) |
| `bool canRedo() const` | 业务／适配 | [ProjectModel.h:1048](../src/ProjectModel.h#L1048) |
| `juce::String addAudioFile(const juce::File& file, double durationSeconds, double startSeconds = 0.0, const juce::String& targetTrackId = {}, const VoicebankOtoEntry* oto = nullptr, UtauMode otoMode = UtauMode::classic)` | 业务／适配 | [ProjectModel.h:1050](../src/ProjectModel.h#L1050) |
| `juce::String addMaterialPronunciations( const juce::File& audio, const std::vector<SampleRegionSetting>& completeRows, const std::vector<juce::String>& selectedRegionIds, double absoluteStart, const juce::String& targetTrackId, juce::String& error, const juce::String& chosenAlias = {})` | 业务／适配 | [ProjectModel.h:1062](../src/ProjectModel.h#L1062) |
| `juce::String addTypedTrack(TrackType type = TrackType::hachi, const juce::String& name = {})` | 业务／适配 | [ProjectModel.h:1067](../src/ProjectModel.h#L1067) |
| `juce::String addTrack(const juce::String& name, bool compose = true, bool referenceOnly = false, bool accompaniment = false, TrackType type = TrackType::hachi)` | 业务／适配 | [ProjectModel.h:1069](../src/ProjectModel.h#L1069) |
| `void setTrackReferenceOnly(const juce::String& trackId, bool referenceOnly)` | 业务／适配 | [ProjectModel.h:1072](../src/ProjectModel.h#L1072) |
| `void setTrackName(const juce::String& trackId, const juce::String& name)` | 业务／适配 | [ProjectModel.h:1073](../src/ProjectModel.h#L1073) |
| `bool setClipNotesIfEmpty(const juce::String& clipId, std::vector<NoteData> notes)` | 业务／适配 | [ProjectModel.h:1074](../src/ProjectModel.h#L1074) |
| `std::size_t markNativeUnpitchedRegions(const juce::String& clipId)` | 业务／适配 | [ProjectModel.h:1075](../src/ProjectModel.h#L1075) |
| `bool setNativeTrimSourceAnalysis(const juce::File& file, const std::vector<NoteData>& sourceNotes, const juce::String& expectedSourceFingerprint = {})` | 业务／适配 | [ProjectModel.h:1076](../src/ProjectModel.h#L1076) |
| `bool setClipAudioAnalysis(const juce::String& clipId, std::vector<NoteData> notes, const ClipData& importedClip = ClipData{})` | 业务／适配 | [ProjectModel.h:1078](../src/ProjectModel.h#L1078) |
| `bool addMidiFile(const juce::File& file, juce::String& error)` | 业务／适配 | [ProjectModel.h:1080](../src/ProjectModel.h#L1080) |
| `static std::vector<MidiTrackChoice> midiTrackChoices(const juce::File& file, juce::String& error)` | 业务／适配 | [ProjectModel.h:1090](../src/ProjectModel.h#L1090) |
| `juce::String addMidiTrack(const juce::File& file, int trackIndex, juce::String& error)` | 业务／适配 | [ProjectModel.h:1097](../src/ProjectModel.h#L1097) |
| `static bool writeMidiFile(const ProjectData& data, const juce::File& file, juce::String& error, const juce::String& trackId = {}, bool includeMuted = false)` | 业务／适配 | [ProjectModel.h:1105](../src/ProjectModel.h#L1105) |
| `bool addUstFile(const juce::File& file, juce::String& error, juce::StringArray& warnings, UstImportMode mode = UstImportMode::addTrack, juce::String* importedTrackId = nullptr, int encoding = 0, TrackType destinationType = TrackType::hachi)` | 业务／适配 | [ProjectModel.h:1123](../src/ProjectModel.h#L1123) |
| `bool exportUst(const juce::File& file, const juce::String& trackId, juce::String& error, juce::StringArray& warnings, int encoding = 0) const` | 业务／适配 | [ProjectModel.h:1128](../src/ProjectModel.h#L1128) |
| `void setTempo(double bpm, int numerator, int denominator = 4)` | 业务／适配 | [ProjectModel.h:1130](../src/ProjectModel.h#L1130) |
| `void setTempoChange(double quarterPosition, double bpm, bool synchronizeNotes = true)` | 业务／适配 | [ProjectModel.h:1133](../src/ProjectModel.h#L1133) |
| `juce::int64 contentFingerprint() const` | 业务／适配 | [ProjectModel.h:1137](../src/ProjectModel.h#L1137) |
| `static juce::String registrationMusicSourceFingerprint(const ProjectData&)` | 业务／适配 | [ProjectModel.h:1139](../src/ProjectModel.h#L1139) |
| `bool adoptRegisteredMaterialBindings(std::uint64_t expectedRevision, const juce::String& expectedFingerprint, const std::vector<ProjectMaterialRegistrationOwnerBinding>& owners, const std::vector<ProjectMaterialRegistrationNoteBinding>& notes, juce::String& error)` | 业务／适配 | [ProjectModel.h:1140](../src/ProjectModel.h#L1140) |
| `void setGridDivision(const juce::String& division)` | 业务／适配 | [ProjectModel.h:1145](../src/ProjectModel.h#L1145) |
| `void setNoteEditDivision(int division)` | 业务／适配 | [ProjectModel.h:1146](../src/ProjectModel.h#L1146) |
| `void setBaseScale(const juce::String& scale)` | 业务／适配 | [ProjectModel.h:1147](../src/ProjectModel.h#L1147) |
| `void setTrackCompose(const juce::String& trackId, bool enabled)` | 业务／适配 | [ProjectModel.h:1148](../src/ProjectModel.h#L1148) |
| `void setTrackMuted(const juce::String& trackId, bool muted)` | 业务／适配 | [ProjectModel.h:1149](../src/ProjectModel.h#L1149) |
| `void setTrackSolo(const juce::String& trackId, bool solo)` | 业务／适配 | [ProjectModel.h:1150](../src/ProjectModel.h#L1150) |
| `void setTrackVolume(const juce::String& trackId, float volume)` | 业务／适配 | [ProjectModel.h:1151](../src/ProjectModel.h#L1151) |
| `void setClipGainEnvelope(const juce::String& clipId, std::vector<GainEnvelopePoint> points)` | 业务／适配 | [ProjectModel.h:1152](../src/ProjectModel.h#L1152) |
| `void setTrackPan(const juce::String& trackId, float pan)` | 业务／适配 | [ProjectModel.h:1153](../src/ProjectModel.h#L1153) |
| `void setTrackSmoothOverlaps(const juce::String& trackId, bool enabled)` | 业务／适配 | [ProjectModel.h:1154](../src/ProjectModel.h#L1154) |
| `void setTrackNormalizeVolume(const juce::String& trackId, bool enabled)` | 业务／适配 | [ProjectModel.h:1155](../src/ProjectModel.h#L1155) |
| `void setTrackNsfSmoothPitchTransitions(const juce::String& trackId, bool enabled)` | 业务／适配 | [ProjectModel.h:1156](../src/ProjectModel.h#L1156) |
| `void setTrackNsfNoiseProtection(const juce::String& trackId, bool enabled)` | 业务／适配 | [ProjectModel.h:1157](../src/ProjectModel.h#L1157) |
| `void setTrackVoicebankDirectory(const juce::String& trackId, const juce::File& directory)` | 业务／适配 | [ProjectModel.h:1158](../src/ProjectModel.h#L1158) |
| `void setTrackUtauConsonantVelocity(const juce::String& trackId, int velocity)` | 业务／适配 | [ProjectModel.h:1160](../src/ProjectModel.h#L1160) |
| `void setTrackUtauGlobalFlags(const juce::String& trackId, const juce::String& flags)` | 业务／适配 | [ProjectModel.h:1161](../src/ProjectModel.h#L1161) |
| `void setUtauMode(UtauMode mode)` | 业务／适配 | [ProjectModel.h:1162](../src/ProjectModel.h#L1162) |
| `void setTrackUtauMode(const juce::String& trackId, UtauMode mode)` | 业务／适配 | [ProjectModel.h:1163](../src/ProjectModel.h#L1163) |
| `void setTrackChineseCvvc(const juce::String& trackId, bool enabled)` | 业务／适配 | [ProjectModel.h:1164](../src/ProjectModel.h#L1164) |
| `void setTrackCvvcCurveCrossfade(const juce::String& trackId, bool curved)` | 业务／适配 | [ProjectModel.h:1165](../src/ProjectModel.h#L1165) |
| `void setTrackOutputEngine(const juce::String& trackId, UtauOutputEngine engine, const juce::File& resampler = {}, const juce::File& wavtool = {})` | 业务／适配 | [ProjectModel.h:1166](../src/ProjectModel.h#L1166) |
| `void setNotesVibrato(const std::vector<juce::String>& noteIds, const NoteData& parameters, bool enabled, std::optional<bool> realLine = std::nullopt)` | 业务／适配 | [ProjectModel.h:1168](../src/ProjectModel.h#L1168) |
| `void setNotesVibratoRealLine(const std::vector<juce::String>& noteIds, bool enabled)` | 业务／适配 | [ProjectModel.h:1171](../src/ProjectModel.h#L1171) |
| `bool bakeNoteVibratoIntoPitch(const juce::String& noteId)` | 业务／适配 | [ProjectModel.h:1175](../src/ProjectModel.h#L1175) |
| `void setNotesUtauSplice(const std::vector<juce::String>& noteIds, bool enabled)` | 业务／适配 | [ProjectModel.h:1176](../src/ProjectModel.h#L1176) |
| `bool setPointCurves(std::vector<PointCurveEdit> edits, std::uint64_t expectedRevision, juce::String* error = nullptr)` | 业务／适配 | [ProjectModel.h:1204](../src/ProjectModel.h#L1204) |
| `bool previewPointCurves(const std::vector<PointCurveEdit>& edits, std::uint64_t expectedRevision, PointCurvePlanPreview&, juce::String& error) const` | 业务／适配 | [ProjectModel.h:1206](../src/ProjectModel.h#L1206) |
| `bool resetCvvcBoundaries(const std::vector<juce::String>& noteIds, std::uint64_t expectedRevision)` | 业务／适配 | [ProjectModel.h:1209](../src/ProjectModel.h#L1209) |
| `bool setCvvcBoundary(const juce::String& noteId, int rightPart, double position, std::uint64_t expectedRevision)` | 业务／适配 | [ProjectModel.h:1210](../src/ProjectModel.h#L1210) |
| `bool setCvvcEnvelopes(std::vector<CvvcEnvelopeEdit>, std::uint64_t expectedRevision)` | 业务／适配 | [ProjectModel.h:1211](../src/ProjectModel.h#L1211) |
| `bool setCvvcEnvelope(const juce::String& noteId, int part, const juce::String& flag, backend::CvvcEnvelopeCurve points, std::uint64_t expectedRevision)` | 业务／适配 | [ProjectModel.h:1212](../src/ProjectModel.h#L1212) |
| `void setNotesUtauFlagCurveEnabled(const std::vector<juce::String>& noteIds, bool enabled)` | 业务／适配 | [ProjectModel.h:1214](../src/ProjectModel.h#L1214) |
| `bool resetNotesUtauFlagCurves(const std::vector<juce::String>& noteIds)` | 业务／适配 | [ProjectModel.h:1221](../src/ProjectModel.h#L1221) |
| `bool resetNotesUtauFlagCurve(const std::vector<juce::String>& noteIds, const juce::String& flag)` | 业务／适配 | [ProjectModel.h:1223](../src/ProjectModel.h#L1223) |
| `bool setNoteUtauFlagCurves(const juce::String& flag, const std::map<juce::String, std::vector<FlagCurvePoint>>& edits, std::optional<std::uint64_t> expectedRevision = std::nullopt, bool enabledOnly = true)` | 业务／适配 | [ProjectModel.h:1227](../src/ProjectModel.h#L1227) |
| `bool setNoteUtauFlagCurve(const juce::String& noteId, const juce::String& flag, std::vector<FlagCurvePoint> points)` | 业务／适配 | [ProjectModel.h:1230](../src/ProjectModel.h#L1230) |
| `void setNotesRegionFlags(const std::vector<juce::String>& noteIds, bool split, const juce::String& first, const juce::String& second, const juce::String& third, const juce::String& fourth)` | 业务／适配 | [ProjectModel.h:1232](../src/ProjectModel.h#L1232) |
| `void setNotesUtauJieSplitCleared(const std::vector<juce::String>& noteIds)` | 业务／适配 | [ProjectModel.h:1236](../src/ProjectModel.h#L1236) |
| `void setNotesUtauJieSplit(const std::vector<juce::String>& noteIds, double first, double second, double third)` | 业务／适配 | [ProjectModel.h:1237](../src/ProjectModel.h#L1237) |
| `void clearNotesUtauJieSplit(const std::vector<juce::String>& noteIds)` | 业务／适配 | [ProjectModel.h:1239](../src/ProjectModel.h#L1239) |
| `void setTrackPitchAlgorithm(const juce::String& trackId, PitchAlgorithm algorithm)` | 业务／适配 | [ProjectModel.h:1240](../src/ProjectModel.h#L1240) |
| `void setTrackStretchAlgorithm(const juce::String& trackId, StretchAlgorithm algorithm)` | 业务／适配 | [ProjectModel.h:1241](../src/ProjectModel.h#L1241) |
| `void setPitchAlgorithm(PitchAlgorithm algorithm)` | 业务／适配 | [ProjectModel.h:1242](../src/ProjectModel.h#L1242) |
| `void setStretchAlgorithm(StretchAlgorithm algorithm)` | 业务／适配 | [ProjectModel.h:1243](../src/ProjectModel.h#L1243) |
| `void setTrackRenderOrder(const juce::String& trackId, RenderOrder order, std::optional<bool> mergeContiguous = {})` | 业务／适配 | [ProjectModel.h:1244](../src/ProjectModel.h#L1244) |
| `void setRenderOrder(RenderOrder order)` | 业务／适配 | [ProjectModel.h:1246](../src/ProjectModel.h#L1246) |
| `void moveClip(const juce::String& clipId, double startSeconds)` | 业务／适配 | [ProjectModel.h:1247](../src/ProjectModel.h#L1247) |
| `bool canSplitClip(const juce::String& clipId, double absoluteSeconds) const` | 业务／适配 | [ProjectModel.h:1248](../src/ProjectModel.h#L1248) |
| `juce::String splitClip(const juce::String& clipId, double absoluteSeconds)` | 业务／适配 | [ProjectModel.h:1251](../src/ProjectModel.h#L1251) |
| `juce::String duplicateClip(const juce::String& clipId, double startSeconds = -1.0, const juce::String& targetTrackId = {})` | 业务／适配 | [ProjectModel.h:1252](../src/ProjectModel.h#L1252) |
| `void resizeClip(const juce::String& clipId, double startSeconds, double durationSeconds)` | 业务／适配 | [ProjectModel.h:1255](../src/ProjectModel.h#L1255) |
| `bool trimClip(const juce::String& clipId, double startSeconds, double durationSeconds)` | 业务／适配 | [ProjectModel.h:1257](../src/ProjectModel.h#L1257) |
| `void setClipGain(const juce::String& clipId, float gain)` | 业务／适配 | [ProjectModel.h:1258](../src/ProjectModel.h#L1258) |
| `void setClipFades(const juce::String& clipId, double fadeInSeconds, double fadeOutSeconds)` | 业务／适配 | [ProjectModel.h:1259](../src/ProjectModel.h#L1259) |
| `void setClipMuted(const juce::String& clipId, bool muted)` | 业务／适配 | [ProjectModel.h:1261](../src/ProjectModel.h#L1261) |
| `void setClipNoteHints(const juce::String& clipId, bool enabled)` | 业务／适配 | [ProjectModel.h:1262](../src/ProjectModel.h#L1262) |
| `void setClipNormalDisplay(const juce::String& clipId, bool enabled)` | 业务／适配 | [ProjectModel.h:1263](../src/ProjectModel.h#L1263) |
| `void removeClip(const juce::String& clipId)` | 业务／适配 | [ProjectModel.h:1264](../src/ProjectModel.h#L1264) |
| `void removeClips(const std::vector<juce::String>& clipIds)` | 业务／适配 | [ProjectModel.h:1265](../src/ProjectModel.h#L1265) |
| `bool canMergeClips(const std::vector<juce::String>& clipIds) const` | 业务／适配 | [ProjectModel.h:1266](../src/ProjectModel.h#L1266) |
| `juce::String mergeClips(const std::vector<juce::String>& clipIds)` | 业务／适配 | [ProjectModel.h:1267](../src/ProjectModel.h#L1267) |
| `void moveClips(const std::vector<juce::String>& clipIds, double deltaSeconds, int trackDelta = 0)` | 业务／适配 | [ProjectModel.h:1268](../src/ProjectModel.h#L1268) |
| `void removeTrack(const juce::String& trackId)` | 业务／适配 | [ProjectModel.h:1269](../src/ProjectModel.h#L1269) |
| `void transposeNote(const juce::String& noteId, float semitones)` | 业务／适配 | [ProjectModel.h:1270](../src/ProjectModel.h#L1270) |
| `void transposeNotes(const std::vector<juce::String>& noteIds, float semitones)` | 业务／适配 | [ProjectModel.h:1271](../src/ProjectModel.h#L1271) |
| `bool moveUtauNotes(const std::vector<juce::String>& noteIds, double deltaSeconds, float semitones, juce::String* error = nullptr)` | 业务／适配 | [ProjectModel.h:1275](../src/ProjectModel.h#L1275) |
| `bool moveNativeNotes(const std::vector<juce::String>& noteIds, double deltaSeconds, float semitones, juce::String* error = nullptr)` | 业务／适配 | [ProjectModel.h:1279](../src/ProjectModel.h#L1279) |
| `bool setNativeNoteTransition(const juce::String& noteId,std::optional<double> middleSourceSeconds,juce::String& error)` | 业务／适配 | [ProjectModel.h:1283](../src/ProjectModel.h#L1283) |
| `bool trimNativeNoteEdge(const juce::String& noteId, double deltaSeconds, bool leftEdge)` | 业务／适配 | [ProjectModel.h:1284](../src/ProjectModel.h#L1284) |
| `bool resizeNativeNoteEdge(const juce::String& noteId, double deltaSeconds, bool leftEdge)` | 业务／适配 | [ProjectModel.h:1285](../src/ProjectModel.h#L1285) |
| `bool alignNotePreutteranceToGrid(const juce::String& noteId, double quarterStep, juce::String& error)` | 业务／适配 | [ProjectModel.h:1288](../src/ProjectModel.h#L1288) |
| `void setNotesMidi(const std::vector<juce::String>& noteIds, float midiNote)` | 业务／适配 | [ProjectModel.h:1289](../src/ProjectModel.h#L1289) |
| `void averageNotesMidi(const std::vector<juce::String>& noteIds)` | 业务／适配 | [ProjectModel.h:1290](../src/ProjectModel.h#L1290) |
| `void quantizeNotesMidi(const std::vector<juce::String>& noteIds, float stepSemitones = 1.0f)` | 业务／适配 | [ProjectModel.h:1291](../src/ProjectModel.h#L1291) |
| `std::vector<juce::String> notesOverlapping( const juce::String& clipId, double fromSeconds, double toSeconds) const` | 业务／适配 | [ProjectModel.h:1295](../src/ProjectModel.h#L1295) |
| `std::vector<juce::String> insertNotes( const juce::String& clipId, const std::vector<NoteData>& noteTemplates, double absoluteStartSeconds)` | 业务／适配 | [ProjectModel.h:1297](../src/ProjectModel.h#L1297) |
| `std::vector<juce::String> insertNativeAudioClips( const juce::String& trackId, const std::vector<ClipData>& templates, const std::vector<NativeConnection>& connections, double atSeconds)` | 业务／适配 | [ProjectModel.h:1300](../src/ProjectModel.h#L1300) |
| `std::vector<juce::String> duplicateNotes( const std::vector<juce::String>& noteIds, const juce::String& targetClipId, double absoluteStartSeconds)` | 业务／适配 | [ProjectModel.h:1303](../src/ProjectModel.h#L1303) |
| `void resizeNote(const juce::String& noteId, double newStart, double newDuration)` | 业务／适配 | [ProjectModel.h:1306](../src/ProjectModel.h#L1306) |
| `juce::String splitNote(const juce::String& noteId, double localSeconds)` | 业务／适配 | [ProjectModel.h:1307](../src/ProjectModel.h#L1307) |
| `juce::String mergeNotes(const std::vector<juce::String>& noteIds)` | 业务／适配 | [ProjectModel.h:1309](../src/ProjectModel.h#L1309) |
| `bool canJoinNativeNotes(const juce::String&,const juce::String&) const` | 业务／适配 | [ProjectModel.h:1310](../src/ProjectModel.h#L1310) |
| `juce::String joinNativeNotes(const juce::String&,const juce::String&)` | 业务／适配 | [ProjectModel.h:1311](../src/ProjectModel.h#L1311) |
| `void setNotesConnection(const std::vector<juce::String>& noteIds, bool enabled)` | 业务／适配 | [ProjectModel.h:1314](../src/ProjectModel.h#L1314) |
| `bool setNotesPortamento(const std::vector<juce::String>& noteIds, bool enabled, juce::String& error, double durationSeconds = 0.1)` | 业务／适配 | [ProjectModel.h:1317](../src/ProjectModel.h#L1317) |
| `bool forceNativeConnection(const std::vector<juce::String>& noteIds, bool enabled, juce::String& error, double durationSeconds = 0.1)` | 业务／适配 | [ProjectModel.h:1322](../src/ProjectModel.h#L1322) |
| `bool setConnectionPitchCurve(const juce::String& connectionId, std::vector<PitchCurveEditPoint> absolutePoints, juce::String& error)` | 业务／适配 | [ProjectModel.h:1328](../src/ProjectModel.h#L1328) |
| `bool setConnectionPitchPoint(const juce::String& connectionId, int pointIndex, PitchCurveEditPoint absolutePoint, juce::String& error)` | 业务／适配 | [ProjectModel.h:1330](../src/ProjectModel.h#L1330) |
| `juce::String createNativeVirtualConnection(const std::vector<juce::String>& orderedNoteIds, bool automaticPitch,juce::String& error,double durationSeconds=.1,bool automaticallyCreated=false, const NativeSplitTransitionEvidence* splitTransition=nullptr,bool sharedPitch=false)` | 业务／适配 | [ProjectModel.h:1334](../src/ProjectModel.h#L1334) |
| `bool setNativeVirtualConnectionForNotes(const std::vector<juce::String>& noteIds,bool enabled, bool automaticPitch,juce::String& error,double durationSeconds=.1,bool automaticallyCreated=false, const NativeSplitTransitionEvidence* splitTransition=nullptr,bool sharedPitch=false)` | 业务／适配 | [ProjectModel.h:1339](../src/ProjectModel.h#L1339) |
| `bool setNativeVirtualConnectionEnabled(const juce::String& groupId,bool enabled,juce::String& error)` | 业务／适配 | [ProjectModel.h:1342](../src/ProjectModel.h#L1342) |
| `bool setNativeVirtualMovementCoupled(const juce::String& groupId,bool coupled,juce::String& error)` | 业务／适配 | [ProjectModel.h:1343](../src/ProjectModel.h#L1343) |
| `bool removeNativeVirtualConnection(const juce::String& groupId,juce::String& error)` | 业务／适配 | [ProjectModel.h:1344](../src/ProjectModel.h#L1344) |
| `bool setNativeVirtualPitchCurve(const juce::String& groupId, const std::vector<PitchCurveEditPoint>& absolutePoints,juce::String& error)` | 业务／适配 | [ProjectModel.h:1345](../src/ProjectModel.h#L1345) |
| `bool resetNativeVirtualPitchCurve(const juce::String& groupId,juce::String& error)` | 业务／适配 | [ProjectModel.h:1347](../src/ProjectModel.h#L1347) |
| `bool setNativeVirtualHandoverWindow(const juce::String& groupId,const juce::String& leftNoteId, const juce::String& rightNoteId,double absoluteFrom,double absoluteTo,juce::String& error)` | 业务／适配 | [ProjectModel.h:1348](../src/ProjectModel.h#L1348) |
| `bool resetNativeVirtualHandoverWindow(const juce::String& groupId,const juce::String& leftNoteId, const juce::String& rightNoteId,juce::String& error)` | 业务／适配 | [ProjectModel.h:1350](../src/ProjectModel.h#L1350) |
| `juce::String mergeMelodyneConsonant(const NativeConsonantMergeEvidence&,juce::String& error)` | 业务／适配 | [ProjectModel.h:1352](../src/ProjectModel.h#L1352) |
| `bool restoreMelodyneConsonant(const juce::String& mergedNoteId,bool discardMergedEdits,juce::String& error)` | 业务／适配 | [ProjectModel.h:1353](../src/ProjectModel.h#L1353) |
| `bool disconnectNativeAudio(const std::vector<juce::String>& noteIds)` | 业务／适配 | [ProjectModel.h:1354](../src/ProjectModel.h#L1354) |
| `bool linkNativeAudio(const std::vector<juce::String>& noteIds)` | 业务／适配 | [ProjectModel.h:1355](../src/ProjectModel.h#L1355) |
| `bool setNativeAudioOverlap(const juce::String& trackId, bool enabled)` | 业务／适配 | [ProjectModel.h:1356](../src/ProjectModel.h#L1356) |
| `juce::String insertPrefixNote(const juce::String& noteId, double targetOverlapSeconds)` | 业务／适配 | [ProjectModel.h:1361](../src/ProjectModel.h#L1361) |
| `void setNoteModulation(const juce::String& noteId, float modulation)` | 业务／适配 | [ProjectModel.h:1363](../src/ProjectModel.h#L1363) |
| `void setNoteDrift(const juce::String& noteId, float drift)` | 业务／适配 | [ProjectModel.h:1364](../src/ProjectModel.h#L1364) |
| `void setNoteTension(const juce::String& noteId, float tension)` | 业务／适配 | [ProjectModel.h:1365](../src/ProjectModel.h#L1365) |
| `void setNoteBreath(const juce::String& noteId, float breath)` | 业务／适配 | [ProjectModel.h:1366](../src/ProjectModel.h#L1366) |
| `void setNoteFormant(const juce::String& noteId, float semitones)` | 业务／适配 | [ProjectModel.h:1367](../src/ProjectModel.h#L1367) |
| `void setNoteGain(const juce::String& noteId, float gain)` | 业务／适配 | [ProjectModel.h:1368](../src/ProjectModel.h#L1368) |
| `bool setNotesTailFade(const std::vector<juce::String>& noteIds, int mode)` | 业务／适配 | [ProjectModel.h:1369](../src/ProjectModel.h#L1369) |
| `bool setNotesNativeEnvelope(const std::vector<juce::String>& noteIds, const backend::NativeEnvelopeSettings& settings)` | 业务／适配 | [ProjectModel.h:1370](../src/ProjectModel.h#L1370) |
| `bool setNotesTailFade(const std::vector<juce::String>& noteIds, int mode, const backend::TailFadeSettings& settings)` | 业务／适配 | [ProjectModel.h:1371](../src/ProjectModel.h#L1371) |
| `void setNotesAmplitudeEnvelopeBase(const std::vector<juce::String>& noteIds, float basePercent)` | 业务／适配 | [ProjectModel.h:1373](../src/ProjectModel.h#L1373) |
| `void setNoteAttack(const juce::String& noteId, double consonantSeconds, float attackSpeed)` | 业务／适配 | [ProjectModel.h:1375](../src/ProjectModel.h#L1375) |
| `void setNoteAttackSpeed(const juce::String& noteId, float attackSpeed)` | 业务／适配 | [ProjectModel.h:1376](../src/ProjectModel.h#L1376) |
| `void setNoteLabel(const juce::String& noteId, const juce::String& label)` | 业务／适配 | [ProjectModel.h:1377](../src/ProjectModel.h#L1377) |
| `int convertTrackLyricsToPinyin(const juce::String& trackId)` | 业务／适配 | [ProjectModel.h:1381](../src/ProjectModel.h#L1381) |
| `void setNoteLabels(const std::vector<std::pair<juce::String, juce::String>>& labels)` | 业务／适配 | [ProjectModel.h:1385](../src/ProjectModel.h#L1385) |
| `void setNoteUtauFlags(const juce::String& noteId, const juce::String& flags)` | 业务／适配 | [ProjectModel.h:1389](../src/ProjectModel.h#L1389) |
| `void setNotesUtauFlags(const std::vector<juce::String>& noteIds, const juce::String& flags)` | 业务／适配 | [ProjectModel.h:1390](../src/ProjectModel.h#L1390) |
| `void setNotesUtauConsonantVelocity(const std::vector<juce::String>& noteIds, int velocity)` | 业务／适配 | [ProjectModel.h:1392](../src/ProjectModel.h#L1392) |
| `void setNoteUtauOto(const juce::String& noteId, const backend::UtauOtoOverride& oto)` | 业务／适配 | [ProjectModel.h:1419](../src/ProjectModel.h#L1419) |
| `bool setNoteUtauOtoChecked(const juce::String& noteId, std::uint64_t expectedRevision, const backend::UtauOtoOverride& value, std::uint64_t& resultingRevision, juce::String& error)` | 业务／适配 | [ProjectModel.h:1422](../src/ProjectModel.h#L1422) |
| `void clearNotesUtauOto(const std::vector<juce::String>& noteIds)` | 业务／适配 | [ProjectModel.h:1427](../src/ProjectModel.h#L1427) |
| `void setNotesUtauStp(const std::vector<juce::String>& noteIds, double seconds)` | 业务／适配 | [ProjectModel.h:1429](../src/ProjectModel.h#L1429) |
| `void setNoteUtauTimingOverrides(const juce::String& noteId, bool enabled, double preutteranceSeconds, double overlapSeconds)` | 业务／适配 | [ProjectModel.h:1430](../src/ProjectModel.h#L1430) |
| `void setNoteRobustPitchCurve(const juce::String& noteId, bool enabled)` | 业务／适配 | [ProjectModel.h:1433](../src/ProjectModel.h#L1433) |
| `bool rememberDiffSingerParameters(const std::map<juce::String, std::vector<FlagCurve>>& curves)` | 业务／适配 | [ProjectModel.h:1434](../src/ProjectModel.h#L1434) |
| `bool applyDiffSingerParameters(std::uint64_t expectedRevision, const std::map<juce::String, std::vector<FlagCurve>>& curves)` | 业务／适配 | [ProjectModel.h:1435](../src/ProjectModel.h#L1435) |
| `bool setDiffSingerParameterCurve(const juce::String& id, const juce::String& key, std::vector<FlagCurvePoint> points)` | 业务／适配 | [ProjectModel.h:1437](../src/ProjectModel.h#L1437) |
| `bool setDiffSingerPitchOffset(const juce::String& noteId, std::vector<PitchCurveEditPoint> points)` | 业务／适配 | [ProjectModel.h:1439](../src/ProjectModel.h#L1439) |
| `bool setNotePitchCurve(const juce::String& noteId, std::vector<PitchCurveEditPoint> points, bool storeControlPoints = false, bool authorPitchSupport = false)` | 业务／适配 | [ProjectModel.h:1441](../src/ProjectModel.h#L1441) |
| `bool applyDiffSingerTiming(std::uint64_t expectedRevision, const std::vector<std::pair<juce::String, juce::String>>& timings)` | 业务／适配 | [ProjectModel.h:1444](../src/ProjectModel.h#L1444) |
| `bool applyDiffSingerPronunciation(std::uint64_t expectedRevision, const juce::String& trackId, const std::vector<std::pair<juce::String, juce::String>>& readings, const juce::String& dictionary)` | 业务／适配 | [ProjectModel.h:1446](../src/ProjectModel.h#L1446) |
| `bool restoreDiffSingerPitchPoint(std::uint64_t expectedRevision, const juce::String& noteId, const std::vector<PitchCurveEditPoint>& displayedPoints, int index)` | 业务／适配 | [ProjectModel.h:1448](../src/ProjectModel.h#L1448) |
| `bool applyDiffSingerPitch(std::uint64_t expectedRevision, const std::vector<std::pair<juce::String, std::vector<PitchCurveEditPoint>>>& curves, bool preserveOutsideNoteHandles = false)` | 业务／适配 | [ProjectModel.h:1450](../src/ProjectModel.h#L1450) |
| `void setDiffSingerOptions(const juce::String& trackId, const juce::String& language, const juce::String& speaker)` | 业务／适配 | [ProjectModel.h:1453](../src/ProjectModel.h#L1453) |
| `bool setNoteAmplitudeEnvelope(const juce::String& noteId, std::vector<AmplitudeEnvelopePoint> points)` | 业务／适配 | [ProjectModel.h:1455](../src/ProjectModel.h#L1455) |
| `int crossfadeNotes(std::vector<CrossfadeSpan> spans, std::uint64_t expectedRevision, bool curved = false)` | 业务／适配 | [ProjectModel.h:1467](../src/ProjectModel.h#L1467) |
| `bool rememberCvvcPhonemes(const std::vector<std::pair<juce::String,juce::String>>& caches, std::uint64_t expectedRevision)` | 业务／适配 | [ProjectModel.h:1468](../src/ProjectModel.h#L1468) |
| `bool setNotesAmplitudeEnvelopes( std::vector<std::pair<juce::String, std::vector<AmplitudeEnvelopePoint>>> envelopes)` | 业务／适配 | [ProjectModel.h:1470](../src/ProjectModel.h#L1470) |
| `static PlannedNote plannedNoteFor(double startSeconds, double requestedSeconds, double clipSeconds, const std::vector<NoteSpan>& occupied)` | 业务／适配 | [ProjectModel.h:1496](../src/ProjectModel.h#L1496) |
| `juce::String addClip(const juce::String& trackId, double startSeconds, double durationSeconds)` | 业务／适配 | [ProjectModel.h:1504](../src/ProjectModel.h#L1504) |
| `bool canAddEmptyTuningClip(const juce::String& trackId, double startSeconds) const` | 业务／适配 | [ProjectModel.h:1506](../src/ProjectModel.h#L1506) |
| `juce::String addEmptyTuningClip(const juce::String& trackId, double startSeconds, double durationSeconds = 10.0)` | 业务／适配 | [ProjectModel.h:1508](../src/ProjectModel.h#L1508) |
| `static std::vector<std::size_t> regionsToImport( const std::vector<RegionSpan>& regions)` | 业务／适配 | [ProjectModel.h:1525](../src/ProjectModel.h#L1525) |
| `void flattenNotePitch(const std::vector<juce::String>& noteIds, bool snapNativeToSemitone = false)` | 业务／适配 | [ProjectModel.h:1541](../src/ProjectModel.h#L1541) |
| `bool restoreNativeSourcePitch(const std::vector<juce::String>& noteIds)` | 业务／适配 | [ProjectModel.h:1542](../src/ProjectModel.h#L1542) |
| `static double firstFreeStartFrom(double startSeconds, const std::vector<NoteSpan>& occupied)` | 业务／适配 | [ProjectModel.h:1543](../src/ProjectModel.h#L1543) |
| `juce::String addNote(const juce::String& preferredClipId, double absoluteStart, double duration, float midiNote)` | 业务／适配 | [ProjectModel.h:1546](../src/ProjectModel.h#L1546) |
| `juce::String addNoteFrom(const juce::String& preferredClipId, double absoluteFromSeconds, double maximumDuration, float midiNote)` | 业务／适配 | [ProjectModel.h:1554](../src/ProjectModel.h#L1554) |
| `void removeNote(const juce::String& noteId)` | 业务／适配 | [ProjectModel.h:1557](../src/ProjectModel.h#L1557) |
| `void removeNotes(const std::vector<juce::String>& noteIds)` | 业务／适配 | [ProjectModel.h:1558](../src/ProjectModel.h#L1558) |
| `void removeNotesRippling(const std::vector<juce::String>& noteIds)` | 业务／适配 | [ProjectModel.h:1562](../src/ProjectModel.h#L1562) |
| `void insertGapBeforeNote(const juce::String& noteId, double seconds)` | 业务／适配 | [ProjectModel.h:1566](../src/ProjectModel.h#L1566) |
| `void closeGapBeforeNote(const juce::String& noteId, double seconds)` | 业务／适配 | [ProjectModel.h:1569](../src/ProjectModel.h#L1569) |
| `void toggleNoteConnection(const juce::String& noteId)` | 业务／适配 | [ProjectModel.h:1570](../src/ProjectModel.h#L1570) |
| `MaterialUpdateReport applySourceSettingsWithReport( const juce::File& source, const std::vector<SampleRegionSetting>& rows, bool apply = true, const juce::String& audioVariantScope = {})` | 业务／适配 | [ProjectModel.h:1591](../src/ProjectModel.h#L1591) |
| `void applySourceSettings(const juce::File& source, const std::vector<SampleRegionSetting>& rows)` | 业务／适配 | [ProjectModel.h:1594](../src/ProjectModel.h#L1594) |
| `void setNoteMaterialLocalOverride(const juce::String& noteId, bool enabled)` | 业务／适配 | [ProjectModel.h:1596](../src/ProjectModel.h#L1596) |
| `bool getNoteMaterialLocalBinding(const juce::String& noteId, NoteMaterialLocalBinding&, SampleRegionSetting& row, juce::String& error) const` | 业务／适配 | [ProjectModel.h:1608](../src/ProjectModel.h#L1608) |
| `bool applyNoteMaterialLocalRow(const NoteMaterialLocalBinding&, const SampleRegionSetting&, std::uint64_t& resultingRevision, juce::String& error)` | 业务／适配 | [ProjectModel.h:1610](../src/ProjectModel.h#L1610) |
| `bool restoreNoteMaterialLocalRow(const NoteMaterialLocalBinding&, SampleRegionSetting& restored, std::uint64_t& resultingRevision, juce::String& error)` | 业务／适配 | [ProjectModel.h:1612](../src/ProjectModel.h#L1612) |
| `static bool makeClassicNoteMaterialOto(const SampleRegionSetting&, backend::UtauOtoOverride&, juce::String& error)` | 业务／适配 | [ProjectModel.h:1615](../src/ProjectModel.h#L1615) |
| `MaterialSourceReplacementReport replaceNotesMaterialSources( const std::vector<MaterialSourceChoice>& choices, std::uint64_t expectedRevision, juce::String& error, bool apply = true)` | 业务／适配 | [ProjectModel.h:1638](../src/ProjectModel.h#L1638) |
| `MaterialSourceReplacementReport replaceNoteMaterialSource( const juce::String& noteId, const juce::File& audio, const std::vector<SampleRegionSetting>& completeRows, const juce::String& audioId, const juce::String& regionId, const juce::String& chosenAlias, juce::String& error, bool apply = true)` | 业务／适配 | [ProjectModel.h:1646](../src/ProjectModel.h#L1646) |
| `std::vector<MaterialSourceBindingTicket> pendingMaterialSourceAnalyses() const` | 业务／适配 | [ProjectModel.h:1651](../src/ProjectModel.h#L1651) |
| `bool completeMaterialSourceAnalysis(const MaterialSourceBindingTicket& ticket, const std::vector<NoteData>& fullFileSourceNotes, juce::String& error)` | 业务／适配 | [ProjectModel.h:1656](../src/ProjectModel.h#L1656) |
| `bool save(const juce::File& file, juce::String& error) const` | 业务／适配 | [ProjectModel.h:1659](../src/ProjectModel.h#L1659) |
| `bool load(const juce::File& file, juce::String& error)` | 业务／适配 | [ProjectModel.h:1660](../src/ProjectModel.h#L1660) |
| `static bool saveRecoverySnapshot(ProjectData data, const juce::File& recoveryFile, const juce::File& originalFile, juce::String& error)` | 业务／适配 | [ProjectModel.h:1661](../src/ProjectModel.h#L1661) |

## AudioEngine（75 个声明）

| 实际签名 | 类型 | 定义入口 |
| --- | --- | --- |
| `explicit AudioEngine(bool initialiseOutputDevice = true)` | 生命周期 | [AudioEngine.h:60](../src/AudioEngine.h#L60) |
| `~AudioEngine() override` | 生命周期 | [AudioEngine.h:61](../src/AudioEngine.h#L61) |
| `void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override` | 业务／适配 | [AudioEngine.h:63](../src/AudioEngine.h#L63) |
| `void releaseResources() override` | 业务／适配 | [AudioEngine.h:64](../src/AudioEngine.h#L64) |
| `void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override` | 业务／适配 | [AudioEngine.h:65](../src/AudioEngine.h#L65) |
| `void syncProject(const ProjectData& project, bool diffSingerExport = false, WavExportComponent component = WavExportComponent::full)` | 业务／适配 | [AudioEngine.h:67](../src/AudioEngine.h#L67) |
| `static juce::String componentExportIssue(const ProjectData& project, const juce::File& engine, const juce::String& trackId = {}, const std::vector<juce::String>& noteIds = {}, juce::Range<double> range = {}, const juce::String& auditionTrack = {}, UtauOutputEngine defaultEngine = UtauOutputEngine::resampler)` | 业务／适配 | [AudioEngine.h:69](../src/AudioEngine.h#L69) |
| `void setPlayUntil(double seconds)` | 业务／适配 | [AudioEngine.h:76](../src/AudioEngine.h#L76) |
| `void setUtauRenderNoteSelection(const std::vector<juce::String>& noteIds)` | 业务／适配 | [AudioEngine.h:77](../src/AudioEngine.h#L77) |
| `void setAuditionTrack(const juce::String& trackId)` | 业务／适配 | [AudioEngine.h:81](../src/AudioEngine.h#L81) |
| `static bool trackIsAudible(bool muted, bool solo, bool anySolo, bool referenceOnly, bool beingWorkedOn)` | 业务／适配 | [AudioEngine.h:84](../src/AudioEngine.h#L84) |
| `int selectEveryUtauNote(const ProjectData& project)` | 业务／适配 | [AudioEngine.h:90](../src/AudioEngine.h#L90) |
| `bool selectAllRenderedUtauNotes()` | 业务／适配 | [AudioEngine.h:91](../src/AudioEngine.h#L91) |
| `void setHifiganModelDirectory(const juce::File& directory)` | 业务／适配 | [AudioEngine.h:92](../src/AudioEngine.h#L92) |
| `void setUtauResamplerFile(const juce::File& executable)` | 业务／适配 | [AudioEngine.h:97](../src/AudioEngine.h#L97) |
| `void setUtauOutputDefaults(UtauOutputEngine engine, const juce::File& wavtool)` | 业务／适配 | [AudioEngine.h:98](../src/AudioEngine.h#L98) |
| `UtauOutputEngine currentUtauOutputEngine() const` | 业务／适配 | [AudioEngine.h:99](../src/AudioEngine.h#L99) |
| `juce::File currentUtauWavtoolFile() const` | 业务／适配 | [AudioEngine.h:100](../src/AudioEngine.h#L100) |
| `juce::String outputEngineDisplayName(const TrackData& track) const` | 业务／适配 | [AudioEngine.h:101](../src/AudioEngine.h#L101) |
| `static juce::File bundledUtauResampler( const juce::File& executableDirectory)` | 业务／适配 | [AudioEngine.h:106](../src/AudioEngine.h#L106) |
| `static juce::File resolveUtauResampler( const juce::String& configured, const juce::File& executableDirectory)` | 业务／适配 | [AudioEngine.h:109](../src/AudioEngine.h#L109) |
| `juce::File diagnosticUtauResamplerFile() const` | 诊断 | [AudioEngine.h:116](../src/AudioEngine.h#L116) |
| `juce::File currentUtauResamplerFile() const` | 业务／适配 | [AudioEngine.h:120](../src/AudioEngine.h#L120) |
| `void setInferenceConfiguration(backend::InferenceBackend inference, int deviceIndex)` | 业务／适配 | [AudioEngine.h:124](../src/AudioEngine.h#L124) |
| `std::optional<double> probeDuration(const juce::File& file)` | 业务／适配 | [AudioEngine.h:125](../src/AudioEngine.h#L125) |
| `bool setAuditionFile(const juce::File& file)` | 业务／适配 | [AudioEngine.h:126](../src/AudioEngine.h#L126) |
| `void clearAuditionFile()` | 业务／适配 | [AudioEngine.h:127](../src/AudioEngine.h#L127) |
| `void play()` | 业务／适配 | [AudioEngine.h:129](../src/AudioEngine.h#L129) |
| `void stop()` | 业务／适配 | [AudioEngine.h:130](../src/AudioEngine.h#L130) |
| `void setPosition(double seconds)` | 业务／适配 | [AudioEngine.h:131](../src/AudioEngine.h#L131) |
| `double position() const` | 业务／适配 | [AudioEngine.h:132](../src/AudioEngine.h#L132) |
| `double nativeScorePrerollStart() const` | 业务／适配 | [AudioEngine.h:135](../src/AudioEngine.h#L135) |
| `float trackPeak(const juce::String& trackId) const` | 业务／适配 | [AudioEngine.h:136](../src/AudioEngine.h#L136) |
| `std::optional<double> renderProgress() const` | 业务／适配 | [AudioEngine.h:137](../src/AudioEngine.h#L137) |
| `bool playbackNeedsRender(double lookAheadSeconds = 0.75) const` | 业务／适配 | [AudioEngine.h:140](../src/AudioEngine.h#L140) |
| `bool hasPlayableRenderedAudio() const` | 业务／适配 | [AudioEngine.h:141](../src/AudioEngine.h#L141) |
| `bool hasCurrentRenderedAudio() const` | 业务／适配 | [AudioEngine.h:142](../src/AudioEngine.h#L142) |
| `bool rewindToFirstPlayableRenderedAudio(double leadInSeconds = 0.03)` | 业务／适配 | [AudioEngine.h:143](../src/AudioEngine.h#L143) |
| `juce::String activeRenderBackends() const` | 业务／适配 | [AudioEngine.h:144](../src/AudioEngine.h#L144) |
| `juce::String activeRenderWarnings() const` | 业务／适配 | [AudioEngine.h:145](../src/AudioEngine.h#L145) |
| `static juce::StringArray renderCapabilityWarnings( const ProjectData& project, UtauOutputEngine defaultEngine = UtauOutputEngine::resampler)` | 业务／适配 | [AudioEngine.h:151](../src/AudioEngine.h#L151) |
| `static bool clipsJoinAt(const ClipData& clip, const ClipData& neighbour, bool asNext)` | 业务／适配 | [AudioEngine.h:163](../src/AudioEngine.h#L163) |
| `static std::vector<std::vector<const ClipData*>> glideChains( const std::vector<const ClipData*>& orderedClips)` | 业务／适配 | [AudioEngine.h:171](../src/AudioEngine.h#L171) |
| `static backend::Mld5FileRenderRequest mergedRequestFor( const std::vector<const ClipData*>& group, const TrackData& track, const juce::File& hifiganModelDirectory, const backend::OrtExecutionConfig& inference, std::optional<bool> legacyUvCarrier = {}, bool skipSharedPitch = false, const SharedPitchLines* sharedLines = nullptr)` | 业务／适配 | [AudioEngine.h:177](../src/AudioEngine.h#L177) |
| `void setDiagnosticNsfRendering(bool incrementalRender, bool legacyUvCarrier = false, bool skipSharedPitch = false)` | 业务／适配 | [AudioEngine.h:187](../src/AudioEngine.h#L187) |
| `void setDiagnosticUtauTimelinePitch(bool enabled)` | 业务／适配 | [AudioEngine.h:197](../src/AudioEngine.h#L197) |
| `void setDiagnosticUtauOwnPitch(bool enabled)` | 业务／适配 | [AudioEngine.h:198](../src/AudioEngine.h#L198) |
| `void setDiagnosticUtauNominalBridge(bool enabled)` | 业务／适配 | [AudioEngine.h:199](../src/AudioEngine.h#L199) |
| `int diagnosticMergedPhraseCount() const` | 诊断 | [AudioEngine.h:202](../src/AudioEngine.h#L202) |
| `std::uint64_t diagnosticScheduledNsfUtauRenders() const` | 诊断 | [AudioEngine.h:203](../src/AudioEngine.h#L203) |
| `double diagnosticRenderPriorityPosition() const` | 诊断 | [AudioEngine.h:204](../src/AudioEngine.h#L204) |
| `static std::vector<ProjectedNotePitch> projectTrackNotePitch( const TrackData&, const std::vector<NativeConnection>&)` | 业务／适配 | [AudioEngine.h:220](../src/AudioEngine.h#L220) |
| `static std::vector<float> diagnosticNativeTargetMidi( const ProjectData& project, int trackIndex, int clipIndex)` | 诊断 | [AudioEngine.h:226](../src/AudioEngine.h#L226) |
| `static backend::Mld5FileRenderRequest diagnosticNativeRequest(const ClipData& clip,const TrackData& track)` | 诊断 | [AudioEngine.h:228](../src/AudioEngine.h#L228) |
| `std::shared_ptr<const std::vector<UtauNoteWaveform>> utauNoteWaveforms() const` | 业务／适配 | [AudioEngine.h:234](../src/AudioEngine.h#L234) |
| `void refreshUtauWaveforms()` | 业务／适配 | [AudioEngine.h:240](../src/AudioEngine.h#L240) |
| `void refreshNativeWaveforms()` | 业务／适配 | [AudioEngine.h:241](../src/AudioEngine.h#L241) |
| `std::shared_ptr<const std::vector<NativeRenderedWaveform>> nativeClipWaveforms() const` | 业务／适配 | [AudioEngine.h:242](../src/AudioEngine.h#L242) |
| `static std::uint64_t nativeClipWaveformHash(const ClipData&, const TrackData&, const std::vector<NativeVirtualGroup>* virtualGroups = nullptr)` | 业务／适配 | [AudioEngine.h:246](../src/AudioEngine.h#L246) |
| `static std::uint64_t utauNoteRenderHash(const NoteData& note)` | 业务／适配 | [AudioEngine.h:253](../src/AudioEngine.h#L253) |
| `static std::uint64_t utauNoteAudioHash(const NoteData& note)` | 业务／适配 | [AudioEngine.h:256](../src/AudioEngine.h#L256) |
| `static std::uint64_t utauNotePhonemeHash(const NoteData& note)` | 业务／适配 | [AudioEngine.h:257](../src/AudioEngine.h#L257) |
| `static std::uint64_t utauPhonemeMixContextHash(const TrackData& track)` | 业务／适配 | [AudioEngine.h:258](../src/AudioEngine.h#L258) |
| `static std::vector<backend::UtauNoteRenderSpec> diagnosticUtauRequestNotes( const ProjectData& project, const juce::String& clipId, const std::vector<juce::String>& selection = {})` | 诊断 | [AudioEngine.h:262](../src/AudioEngine.h#L262) |
| `static std::vector<backend::UtauNoteRenderSpec> utauRequestNotesForClip( const ProjectData& project, const juce::String& clipId)` | 业务／适配 | [AudioEngine.h:266](../src/AudioEngine.h#L266) |
| `static backend::RequestedVoicebankFamily diagnosticUtauRequestedFamily( const ProjectData& project, const juce::String& clipId)` | 诊断 | [AudioEngine.h:268](../src/AudioEngine.h#L268) |
| `static std::string diagnosticUtauRenderKey( const ProjectData& project, const juce::String& clipId, UtauOutputEngine defaultEngine = UtauOutputEngine::resampler, const juce::File& resampler = {}, const juce::File& wavtool = {})` | 诊断 | [AudioEngine.h:270](../src/AudioEngine.h#L270) |
| `bool exportWav(const juce::File& file, juce::String& error, const juce::String& trackId = {}, double fromSeconds = 0.0, double toSeconds = 0.0, WavExportOptions options = {})` | 业务／适配 | [AudioEngine.h:275](../src/AudioEngine.h#L275) |
| `std::optional<float> diagnosticNativeSourceEdgeGain(const juce::String& clipId,double localSeconds) const` | 诊断 | [AudioEngine.h:279](../src/AudioEngine.h#L279) |
| `double playbackSampleRate() const` | 业务／适配 | [AudioEngine.h:280](../src/AudioEngine.h#L280) |
| `bool isPlaying() const` | 业务／适配 | [AudioEngine.h:281](../src/AudioEngine.h#L281) |
| `juce::AudioDeviceManager& devices()` | 业务／适配 | [AudioEngine.h:282](../src/AudioEngine.h#L282) |
| `bool ensureOutputDevice(juce::String& error)` | 业务／适配 | [AudioEngine.h:283](../src/AudioEngine.h#L283) |
| `void restoreDeviceState(juce::PropertiesFile& properties)` | 业务／适配 | [AudioEngine.h:284](../src/AudioEngine.h#L284) |
| `void saveDeviceState(juce::PropertiesFile& properties) const` | 业务／适配 | [AudioEngine.h:285](../src/AudioEngine.h#L285) |

## SampleSettings（36 个声明）

| 实际签名 | 类型 | 定义入口 |
| --- | --- | --- |
| `static juce::File sidecarFor(const juce::File& audio)` | 业务／适配 | [SampleSettings.h:95](../src/SampleSettings.h#L95) |
| `static juce::File annotationFor(const juce::File& audio)` | 业务／适配 | [SampleSettings.h:97](../src/SampleSettings.h#L97) |
| `static void setMaterialRoots(const juce::StringArray& roots)` | 业务／适配 | [SampleSettings.h:98](../src/SampleSettings.h#L98) |
| `static juce::StringArray materialRoots()` | 业务／适配 | [SampleSettings.h:99](../src/SampleSettings.h#L99) |
| `static bool hasNativeAnnotation(const juce::File& audio, juce::String* error = nullptr)` | 业务／适配 | [SampleSettings.h:100](../src/SampleSettings.h#L100) |
| `static bool copyAnnotations(const juce::File& source, const juce::File& destination, juce::String& error)` | 业务／适配 | [SampleSettings.h:102](../src/SampleSettings.h#L102) |
| `static std::vector<NativeSegment> nativeSegmentsFor( const SampleRegionSetting& row)` | 业务／适配 | [SampleSettings.h:107](../src/SampleSettings.h#L107) |
| `static std::vector<SampleRegionSetting> loadOrDerive(const juce::File& audio, const ProjectData& project, juce::String* error = nullptr, bool explicitLegacyMigration = false, const juce::String& audioId = {})` | 业务／适配 | [SampleSettings.h:111](../src/SampleSettings.h#L111) |
| `static bool save(const juce::File& audio, const std::vector<SampleRegionSetting>& rows, juce::String& error, const juce::String& audioId = {})` | 业务／适配 | [SampleSettings.h:116](../src/SampleSettings.h#L116) |
| `static bool convertMelodyneProject(ProjectData& project, juce::StringArray& warnings, bool autoRegisterTransitions = true,bool publishAnnotations = true)` | 业务／适配 | [SampleSettings.h:121](../src/SampleSettings.h#L121) |
| `static bool importOto(const juce::File& oto, const juce::File& audio, double audioDuration, std::vector<SampleRegionSetting>& rows, juce::String& error)` | 业务／适配 | [SampleSettings.h:123](../src/SampleSettings.h#L123) |
| `static bool exportOto(const juce::File& oto, const juce::File& audio, const std::vector<SampleRegionSetting>& rows, double audioDuration, juce::String& error)` | 业务／适配 | [SampleSettings.h:126](../src/SampleSettings.h#L126) |
| `static bool importVoicebank(const juce::File& root, juce::StringArray& audioFiles, int& sidecarsWritten, int& regionsWritten, juce::StringArray& warnings)` | 业务／适配 | [SampleSettings.h:129](../src/SampleSettings.h#L129) |
| `static backend::UtauOtoOverride noteOtoFromEntry(const VoicebankOtoEntry& entry, bool withRegions)` | 业务／适配 | [SampleSettings.h:137](../src/SampleSettings.h#L137) |
| `static VoicebankOtoEntry entryWithNoteOto(VoicebankOtoEntry entry, const backend::UtauOtoOverride& oto)` | 业务／适配 | [SampleSettings.h:154](../src/SampleSettings.h#L154) |
| `static std::vector<VoicebankOtoEntry> loadVoicebankOto( const juce::File& root, juce::StringArray& warnings, bool jieMode = false, bool mouMode = false, const juce::File& onlyOtoFile = {})` | 业务／适配 | [SampleSettings.h:173](../src/SampleSettings.h#L173) |
| `static std::vector<VoicebankOtoEntry> loadAudioOto(const juce::File& audio, const juce::File& root, UtauMode mode, juce::StringArray& warnings)` | 业务／适配 | [SampleSettings.h:178](../src/SampleSettings.h#L178) |
| `static std::optional<ClipData> nativeAudioFromOto(const VoicebankOtoEntry& entry, UtauMode mode, double sourceDurationSeconds)` | 业务／适配 | [SampleSettings.h:181](../src/SampleSettings.h#L181) |
| `static std::uint64_t voicebankFilesRevision()` | 业务／适配 | [SampleSettings.h:187](../src/SampleSettings.h#L187) |
| `static bool updateVoicebankOtoEntry(const VoicebankOtoEntry& original, const VoicebankOtoEntry& updated, juce::String& error)` | 业务／适配 | [SampleSettings.h:188](../src/SampleSettings.h#L188) |
| `static juce::File jieClassicOtoFileFor(const juce::File& otoFile)` | 业务／适配 | [SampleSettings.h:194](../src/SampleSettings.h#L194) |
| `static bool updateJieVoicebankOtoEntry(const VoicebankOtoEntry& original, const VoicebankOtoEntry& updated, juce::String& error)` | 业务／适配 | [SampleSettings.h:195](../src/SampleSettings.h#L195) |
| `static bool duplicateVoicebankOtoEntry(const VoicebankOtoEntry& source, const juce::String& newAlias, bool jieMode, juce::String& error)` | 业务／适配 | [SampleSettings.h:200](../src/SampleSettings.h#L200) |
| `static juce::String entryLookupName(const VoicebankOtoEntry& entry)` | 业务／适配 | [SampleSettings.h:208](../src/SampleSettings.h#L208) |
| `static int findEntryForAlias(const std::vector<VoicebankOtoEntry>& entries, const juce::String& alias)` | 业务／适配 | [SampleSettings.h:211](../src/SampleSettings.h#L211) |
| `static juce::File jieOtoFileFor(const juce::File& otoFile)` | 业务／适配 | [SampleSettings.h:213](../src/SampleSettings.h#L213) |
| `static void mergeJieOto(std::vector<VoicebankOtoEntry>& entries)` | 业务／适配 | [SampleSettings.h:215](../src/SampleSettings.h#L215) |
| `static void mergeMouOto(std::vector<VoicebankOtoEntry>& entries)` | 业务／适配 | [SampleSettings.h:218](../src/SampleSettings.h#L218) |
| `static int mouRegionCount(const juce::String& classes)` | 业务／适配 | [SampleSettings.h:221](../src/SampleSettings.h#L221) |
| `static juce::String mouClassesForCount(const juce::String& classes, int count)` | 业务／适配 | [SampleSettings.h:225](../src/SampleSettings.h#L225) |
| `static bool updateMouOtoEntry(const VoicebankOtoEntry& original, const VoicebankOtoEntry& updated, juce::String& error)` | 业务／适配 | [SampleSettings.h:229](../src/SampleSettings.h#L229) |
| `static bool createMouOto(const juce::File& root, int& written, int& kept, int& merged, juce::String& error)` | 业务／适配 | [SampleSettings.h:241](../src/SampleSettings.h#L241) |
| `static juce::File mouOtoFileFor(const juce::File& otoFile)` | 业务／适配 | [SampleSettings.h:243](../src/SampleSettings.h#L243) |
| `static bool createJieOto(const juce::File& root, int& written, int& kept, juce::String& error)` | 业务／适配 | [SampleSettings.h:246](../src/SampleSettings.h#L246) |
| `static bool createJieOtoFromMou(const juce::File& root, JieFromMouResult& result, juce::String& error)` | 业务／适配 | [SampleSettings.h:251](../src/SampleSettings.h#L251) |
| `static bool updateJieOtoEntry(const VoicebankOtoEntry& original, const VoicebankOtoEntry& updated, juce::String& error)` | 业务／适配 | [SampleSettings.h:253](../src/SampleSettings.h#L253) |

## AccompanimentTempoTask（8 个声明）

| 实际签名 | 类型 | 定义入口 |
| --- | --- | --- |
| `explicit AccompanimentTempoTask(ClipData region, juce::File runtime = {})` | 生命周期 | [AccompanimentTempo.h:12](../src/AccompanimentTempo.h#L12) |
| `~AccompanimentTempoTask() override` | 生命周期 | [AccompanimentTempo.h:13](../src/AccompanimentTempo.h#L13) |
| `void cancel()` | 业务／适配 | [AccompanimentTempo.h:14](../src/AccompanimentTempo.h#L14) |
| `bool finished() const` | 业务／适配 | [AccompanimentTempo.h:15](../src/AccompanimentTempo.h#L15) |
| `int stage() const` | 业务／适配 | [AccompanimentTempo.h:16](../src/AccompanimentTempo.h#L16) |
| `juce::var result() const` | 业务／适配 | [AccompanimentTempo.h:17](../src/AccompanimentTempo.h#L17) |
| `static bool canAnalyse(const ClipData&)` | 业务／适配 | [AccompanimentTempo.h:18](../src/AccompanimentTempo.h#L18) |
| `static juce::File runtimeConfig()` | 业务／适配 | [AccompanimentTempo.h:19](../src/AccompanimentTempo.h#L19) |

## AnalysisService（7 个声明）

| 实际签名 | 类型 | 定义入口 |
| --- | --- | --- |
| `static AnalysisConfig configFromProperties(const juce::PropertiesFile* properties)` | 业务／适配 | [backend/AnalysisService.h:52](../src/backend/AnalysisService.h#L52) |
| `static AnalysisConfig configFromEnvironment()` | 业务／适配 | [backend/AnalysisService.h:53](../src/backend/AnalysisService.h#L53) |
| `static AnalysisStatus status(const AnalysisConfig& config)` | 业务／适配 | [backend/AnalysisService.h:54](../src/backend/AnalysisService.h#L54) |
| `static AnalysisResult analyse(const juce::File& file, const AnalysisConfig& config, juce::String& error, Progress progress = {})` | 业务／适配 | [backend/AnalysisService.h:55](../src/backend/AnalysisService.h#L55) |
| `static std::size_t applySourcePitch(ClipData& clip, const std::vector<NoteData>& sourceNotes)` | 业务／适配 | [backend/AnalysisService.h:59](../src/backend/AnalysisService.h#L59) |
| `static bool reanalyseProjectSourcePitch(ProjectData& project, const AnalysisConfig& config, juce::String& error, Progress progress = {}, AnalysisStatus* usedStatus = nullptr)` | 业务／适配 | [backend/AnalysisService.h:61](../src/backend/AnalysisService.h#L61) |
| `static juce::String backendText(const AnalysisStatus& status)` | 业务／适配 | [backend/AnalysisService.h:66](../src/backend/AnalysisService.h#L66) |

## RenderService（12 个声明）

| 实际签名 | 类型 | 定义入口 |
| --- | --- | --- |
| `RenderService()` | 生命周期 | [backend/RenderService.h:97](../src/backend/RenderService.h#L97) |
| `~RenderService()` | 生命周期 | [backend/RenderService.h:98](../src/backend/RenderService.h#L98) |
| `void renderMld5(Mld5RenderRequest request, Completion completion)` | 业务／适配 | [backend/RenderService.h:99](../src/backend/RenderService.h#L99) |
| `void renderMld5File(Mld5FileRenderRequest request, FileCompletion completion, RenderSchedule schedule = {})` | 业务／适配 | [backend/RenderService.h:100](../src/backend/RenderService.h#L100) |
| `void renderUtau(UtauRenderRequest request, FileCompletion completion, RenderSchedule schedule = {})` | 业务／适配 | [backend/RenderService.h:102](../src/backend/RenderService.h#L102) |
| `void renderNsfUtau(UtauRenderRequest request, juce::File modelDirectory, OrtExecutionConfig execution, FileCompletion completion, RenderSchedule schedule = {})` | 业务／适配 | [backend/RenderService.h:108](../src/backend/RenderService.h#L108) |
| `void cancelAll()` | 业务／适配 | [backend/RenderService.h:111](../src/backend/RenderService.h#L111) |
| `bool hasActiveJobs()` | 业务／适配 | [backend/RenderService.h:112](../src/backend/RenderService.h#L112) |
| `void setPlaybackPosition(double seconds) noexcept` | 业务／适配 | [backend/RenderService.h:113](../src/backend/RenderService.h#L113) |
| `void beginUpdate()` | 业务／适配 | [backend/RenderService.h:114](../src/backend/RenderService.h#L114) |
| `void endUpdate(const std::unordered_set<std::string>& keys)` | 业务／适配 | [backend/RenderService.h:115](../src/backend/RenderService.h#L115) |
| `double playbackPriorityPosition() const` | 业务／适配 | [backend/RenderService.h:116](../src/backend/RenderService.h#L116) |

## UtauRenderer（27 个声明）

| 实际签名 | 类型 | 定义入口 |
| --- | --- | --- |
| `static std::optional<UtauSampleTiming> mappedAliasTiming( const juce::File& directory, const juce::String& alias, float midiNote)` | 业务／适配 | [backend/UtauRenderer.h:250](../src/backend/UtauRenderer.h#L250) |
| `static bool supportsComponentExport(const juce::File& engine, const juce::String& effectiveFlags)` | 业务／适配 | [backend/UtauRenderer.h:254](../src/backend/UtauRenderer.h#L254) |
| `static juce::String diagnosticPitchbend( const UtauNoteRenderSpec& note, double bpm, double preutterance, double outputSeconds)` | 诊断 | [backend/UtauRenderer.h:256](../src/backend/UtauRenderer.h#L256) |
| `static std::vector<UtauAmplitudePoint> fitAmplitudeEnvelope( const std::vector<UtauAmplitudePoint>& drawn, double preutteranceSeconds, double soundingEndSeconds)` | 业务／适配 | [backend/UtauRenderer.h:260](../src/backend/UtauRenderer.h#L260) |
| `static void invalidateVoicebankCache()` | 业务／适配 | [backend/UtauRenderer.h:263](../src/backend/UtauRenderer.h#L263) |
| `static void prewarmHfDaemon(const juce::File& resamplerExecutable)` | 业务／适配 | [backend/UtauRenderer.h:269](../src/backend/UtauRenderer.h#L269) |
| `static bool startHfDaemonIfNeeded(const juce::File& resamplerExecutable, int port = 51765)` | 业务／适配 | [backend/UtauRenderer.h:272](../src/backend/UtauRenderer.h#L272) |
| `static void shutdownHfDaemon()` | 业务／适配 | [backend/UtauRenderer.h:277](../src/backend/UtauRenderer.h#L277) |
| `static juce::String hfDaemonInterpreter(const juce::File& engineDirectory)` | 业务／适配 | [backend/UtauRenderer.h:282](../src/backend/UtauRenderer.h#L282) |
| `static bool voicebankIndexReady(const juce::File& voicebankDirectory, bool fourRegion, bool consonantClasses, std::function<void()> whenReady)` | 业务／适配 | [backend/UtauRenderer.h:288](../src/backend/UtauRenderer.h#L288) |
| `static int diagnosticIndexBuilds()` | 诊断 | [backend/UtauRenderer.h:296](../src/backend/UtauRenderer.h#L296) |
| `static int diagnosticMessageThreadWaits()` | 诊断 | [backend/UtauRenderer.h:299](../src/backend/UtauRenderer.h#L299) |
| `static void diagnosticRecheckVoicebankFiles()` | 诊断 | [backend/UtauRenderer.h:300](../src/backend/UtauRenderer.h#L300) |
| `static void recheckVoicebankFiles(const juce::File& root)` | 业务／适配 | [backend/UtauRenderer.h:302](../src/backend/UtauRenderer.h#L302) |
| `static juce::String diagnosticResolve(const juce::File& voicebankDirectory, const juce::String& alias, float midiNote, bool fourRegion, bool consonantClasses, bool byScan)` | 诊断 | [backend/UtauRenderer.h:303](../src/backend/UtauRenderer.h#L303) |
| `static double crossfadeEnd(double soundStart, double overlap, double nextNoteEnd)` | 业务／适配 | [backend/UtauRenderer.h:315](../src/backend/UtauRenderer.h#L315) |
| `static bool crossfadesInto(double soundStart, double noteEnd)` | 业务／适配 | [backend/UtauRenderer.h:324](../src/backend/UtauRenderer.h#L324) |
| `static bool sendsFlagCurves(bool noteFlagCurve)` | 业务／适配 | [backend/UtauRenderer.h:335](../src/backend/UtauRenderer.h#L335) |
| `static bool readsOnlyFirstTwoRegions(bool fourRegion, bool consonantClasses, double durationSeconds)` | 业务／适配 | [backend/UtauRenderer.h:336](../src/backend/UtauRenderer.h#L336) |
| `static std::array<double, 4> firstTwoRegions( const std::array<double, 4>& regionSeconds)` | 业务／适配 | [backend/UtauRenderer.h:339](../src/backend/UtauRenderer.h#L339) |
| `static std::optional<UtauSampleTiming> sampleTiming( const juce::File& voicebankDirectory, const juce::String& alias, float midiNote, int consonantVelocity = 100, bool fourRegion = false, bool consonantClasses = false)` | 业务／适配 | [backend/UtauRenderer.h:341](../src/backend/UtauRenderer.h#L341) |
| `static std::optional<UtauSampleTiming> sampleTiming( const juce::File& voicebankDirectory, const juce::String& alias, float midiNote, int consonantVelocity, bool fourRegion, bool consonantClasses, const UtauOtoOverride* noteOto)` | 业务／适配 | [backend/UtauRenderer.h:348](../src/backend/UtauRenderer.h#L348) |
| `static ResolvedSample resolveVoiceSample( const juce::File& voicebankDirectory, const juce::String& alias, float midiNote, int consonantVelocity = 100, bool fourRegion = false, bool consonantClasses = false, double stpSeconds = 0.0, bool preutteranceOverrideEnabled = false, double preutteranceSeconds = 0.0, bool overlapOverrideEnabled = false, double overlapSeconds = 0.0, const UtauOtoOverride* noteOto = nullptr, bool aliasIsResolved = false)` | 业务／适配 | [backend/UtauRenderer.h:380](../src/backend/UtauRenderer.h#L380) |
| `static UtauRenderResult render(const UtauRenderRequest& request)` | 业务／适配 | [backend/UtauRenderer.h:387](../src/backend/UtauRenderer.h#L387) |
| `static std::optional<UtauTailFadeSpan> tailFadeSpan( const UtauSampleTiming& timing,double soundStart,double soundEnd, double nominalDuration,int velocity,bool fourRegion,bool mou, const std::array<double,3>* manual=nullptr)` | 业务／适配 | [backend/UtauRenderer.h:389](../src/backend/UtauRenderer.h#L389) |
| `static std::optional<UtauTailFadeSpan> headEnvelopeSpan( const UtauSampleTiming& timing,double soundStart,double soundEnd, double nominalDuration,int velocity,bool fourRegion,bool mou, const std::array<double,3>* manual=nullptr)` | 业务／适配 | [backend/UtauRenderer.h:393](../src/backend/UtauRenderer.h#L393) |
| `static UtauRegionSplit regionSplit( const std::array<double, 4>& sourceSeconds, double outputSeconds, int consonantVelocity, double leadInSeconds, const std::array<double, 3>* manualFractions, const juce::String* classes = nullptr)` | 业务／适配 | [backend/UtauRenderer.h:404](../src/backend/UtauRenderer.h#L404) |

## NsfHifiganRenderer（3 个声明）

| 实际签名 | 类型 | 定义入口 |
| --- | --- | --- |
| `static void shutdown()` | 业务／适配 | [backend/NsfHifiganRenderer.h:84](../src/backend/NsfHifiganRenderer.h#L84) |
| `static bool modelAvailable(const juce::File& configuredModelDirectory)` | 业务／适配 | [backend/NsfHifiganRenderer.h:85](../src/backend/NsfHifiganRenderer.h#L85) |
| `static NsfHifiganRenderResult render( const juce::AudioBuffer<float>& source, double sampleRate, int targetSamples, double framePeriodMs, const std::vector<float>& targetMidi, const std::vector<float>& formantSemitones, const std::vector<NsfHifiganTimeMapPoint>& timeMap, const juce::File& configuredModelDirectory, const OrtExecutionConfig& execution, NsfHifiganStretchOrder stretchOrder, bool normalizeVolume = false, const NsfHifiganEdgeGuard& edgeGuard = NsfHifiganEdgeGuard{}, const std::function<bool()>& cancelled = {}, bool protectNativeNoise = false, bool smoothPitchTransitions = true, bool incrementalNativeRender = false, const std::vector<float>& sourceMidi = {}, const NsfHifiganSourceAssembly* sourceAssembly = nullptr)` | 业务／适配 | [backend/NsfHifiganRenderer.h:86](../src/backend/NsfHifiganRenderer.h#L86) |

## DiffSingerRenderer（12 个声明）

| 实际签名 | 类型 | 定义入口 |
| --- | --- | --- |
| `static void openProjectCache(const juce::File& project = {})` | 业务／适配 | [backend/DiffSingerRenderer.h:10](../src/backend/DiffSingerRenderer.h#L10) |
| `static bool saveProjectCache(const juce::File& project)` | 业务／适配 | [backend/DiffSingerRenderer.h:11](../src/backend/DiffSingerRenderer.h#L11) |
| `static juce::String cacheSession()` | 业务／适配 | [backend/DiffSingerRenderer.h:12](../src/backend/DiffSingerRenderer.h#L12) |
| `static void configure(const DiffSingerOptions&)` | 业务／适配 | [backend/DiffSingerRenderer.h:13](../src/backend/DiffSingerRenderer.h#L13) |
| `static juce::var inferenceOptions(bool exporting = false)` | 业务／适配 | [backend/DiffSingerRenderer.h:14](../src/backend/DiffSingerRenderer.h#L14) |
| `static juce::String inferenceStatus()` | 业务／适配 | [backend/DiffSingerRenderer.h:15](../src/backend/DiffSingerRenderer.h#L15) |
| `static juce::var inferenceReport()` | 业务／适配 | [backend/DiffSingerRenderer.h:16](../src/backend/DiffSingerRenderer.h#L16) |
| `static void shutdown()` | 业务／适配 | [backend/DiffSingerRenderer.h:17](../src/backend/DiffSingerRenderer.h#L17) |
| `static bool isVoicebank(const juce::File& directory)` | 业务／适配 | [backend/DiffSingerRenderer.h:18](../src/backend/DiffSingerRenderer.h#L18) |
| `static juce::var invoke(juce::var request, const std::function<bool()>& cancelled = {}, juce::AudioBuffer<float>* audio = nullptr, double* rate = nullptr)` | 业务／适配 | [backend/DiffSingerRenderer.h:19](../src/backend/DiffSingerRenderer.h#L19) |
| `static juce::var requestJson(const UtauRenderRequest&, const juce::String& operation)` | 业务／适配 | [backend/DiffSingerRenderer.h:21](../src/backend/DiffSingerRenderer.h#L21) |
| `static UtauRenderResult render(const UtauRenderRequest&)` | 业务／适配 | [backend/DiffSingerRenderer.h:22](../src/backend/DiffSingerRenderer.h#L22) |

## McpServer（12 个声明）

| 实际签名 | 类型 | 定义入口 |
| --- | --- | --- |
| `explicit McpServer(const juce::StringArray& extraRoots = {})` | 生命周期 | [backend/McpServer.h:18](../src/backend/McpServer.h#L18) |
| `McpServer(ProjectModel& model, AudioEngine* engine, const juce::StringArray& extraRoots = {})` | 生命周期 | [backend/McpServer.h:20](../src/backend/McpServer.h#L20) |
| `juce::var processRequest(const juce::var& request, bool& shouldRespond)` | 业务／适配 | [backend/McpServer.h:22](../src/backend/McpServer.h#L22) |
| `juce::var executeTool(const juce::String& name, const juce::var& args)` | 业务／适配 | [backend/McpServer.h:23](../src/backend/McpServer.h#L23) |
| `juce::var snapshotJson() const` | 业务／适配 | [backend/McpServer.h:24](../src/backend/McpServer.h#L24) |
| `static juce::var noteJson(const NoteData& note, bool includeCurves = true)` | 业务／适配 | [backend/McpServer.h:25](../src/backend/McpServer.h#L25) |
| `void allowFile(const juce::File& file)` | 业务／适配 | [backend/McpServer.h:26](../src/backend/McpServer.h#L26) |
| `static juce::var result(const juce::String& text, bool failed = false)` | 业务／适配 | [backend/McpServer.h:27](../src/backend/McpServer.h#L27) |
| `int run()` | 业务／适配 | [backend/McpServer.h:28](../src/backend/McpServer.h#L28) |
| `static bool pathWithinRoots(const std::vector<juce::File>& roots, const juce::File& target)` | 业务／适配 | [backend/McpServer.h:32](../src/backend/McpServer.h#L32) |
| `static std::vector<juce::File> projectRoots(const ProjectData& data)` | 业务／适配 | [backend/McpServer.h:36](../src/backend/McpServer.h#L36) |
| `static juce::var diagnosticTools()` | 诊断 | [backend/McpServer.h:38](../src/backend/McpServer.h#L38) |

## LiveMcpBridge（5 个声明）

| 实际签名 | 类型 | 定义入口 |
| --- | --- | --- |
| `LiveMcpBridge(Handler handler, std::function<juce::var()> describe)` | 生命周期 | [backend/LiveMcpBridge.h:13](../src/backend/LiveMcpBridge.h#L13) |
| `~LiveMcpBridge() override` | 生命周期 | [backend/LiveMcpBridge.h:14](../src/backend/LiveMcpBridge.h#L14) |
| `const juce::String& sessionId() const` | 业务／适配 | [backend/LiveMcpBridge.h:15](../src/backend/LiveMcpBridge.h#L15) |
| `static int runProxy(const juce::String& session = {}, bool listOnly = false)` | 业务／适配 | [backend/LiveMcpBridge.h:16](../src/backend/LiveMcpBridge.h#L16) |
| `static juce::File sessionsDirectory()` | 业务／适配 | [backend/LiveMcpBridge.h:17](../src/backend/LiveMcpBridge.h#L17) |

## MCP 命令目录

Headless 命令来自 `McpServer::diagnosticTools`；Live 复用它并叠加窗口命令，并非两套完整独立业务实现。Live 写操作另有 expected_revision、会话和草稿提交约束。字段类型／必填项以链接处 schema 为准；不是所有 GUI 功能都有 MCP 命令。

| 命令 | 注册位置 | 源码描述 |
| --- | --- | --- |
| `project_new` | [backend/McpServer.cpp:462](../src/backend/McpServer.cpp#L462) | Create an empty HachiShifter project / 新建工程 |
| `project_open` | [backend/McpServer.cpp:463](../src/backend/McpServer.cpp#L463) | Open HJPX, legacy HSPX, MPD, MIDI, or audio from path / 打开工程或素材 |
| `project_save` | [backend/McpServer.cpp:474](../src/backend/McpServer.cpp#L474) | Save current project as HJPX / 保存工程 |
| `project_snapshot` | [backend/McpServer.cpp:478](../src/backend/McpServer.cpp#L478) | Read every current track, clip, note, pitch and marker / 读取全部工程内容 |
| `import_audio` | [backend/McpServer.cpp:479](../src/backend/McpServer.cpp#L479) | Import an audio file at start_seconds / 导入音频 |
| `analyse_audio` | [backend/McpServer.cpp:488](../src/backend/McpServer.cpp#L488) | Run configured GAME+FCPE analysis with native fallback and return actual backend / 执行 GAME+FCPE 分析并报告实际后端 |
| `analysis_status` | [backend/McpServer.cpp:494](../src/backend/McpServer.cpp#L494) | Inspect GAME large/small, FCPE and inference availability / 查看 GAME、FCPE 与推理状态 |
| `import_midi` | [backend/McpServer.cpp:496](../src/backend/McpServer.cpp#L496) | Import MIDI notes and tempo / 导入 MIDI |
| `export_midi` | [backend/McpServer.cpp:500](../src/backend/McpServer.cpp#L500) | Write all tracks or one track as MIDI, retaining conductor tempo/meter and lyrics / 导出 MIDI |
| `export_ust` | [backend/McpServer.cpp:508](../src/backend/McpServer.cpp#L508) | Export one monophonic track as UST, retaining imported opaque data / 导出单轨 UST |
| `import_ust` | [backend/McpServer.cpp:513](../src/backend/McpServer.cpp#L513) | Import a UTAU project as a plain UTAU track / 导入 UST |
| `import_melodyne` | [backend/McpServer.cpp:518](../src/backend/McpServer.cpp#L518) | Import Melodyne MPD edits; recursive_media, preserve_edits and source_pitch control import / 导入 Melodyne 工程并控制素材搜索、工程编辑与原始 F0 |
| `restore_merged_consonant` | [backend/McpServer.cpp:536](../src/backend/McpServer.cpp#L536) | Restore archived original consonant/vowel fragments through the common Model authority |
| `set_tempo` | [backend/McpServer.cpp:540](../src/backend/McpServer.cpp#L540) | Set BPM and time signature / 设置速度与拍号 |
| `add_track` | [backend/McpServer.cpp:548](../src/backend/McpServer.cpp#L548) | Create a melodic, audio or accompaniment track / 新建旋律、普通或伴奏轨道 |
| `set_track` | [backend/McpServer.cpp:556](../src/backend/McpServer.cpp#L556) | Set compose, mute, solo, gain, pan and render algorithms / 设置轨道及算法 |
| `set_clip` | [backend/McpServer.cpp:600](../src/backend/McpServer.cpp#L600) | Set clip gain, fades and mute state / 设置采样增益、淡入淡出和静音 |
| `move_clip` | [backend/McpServer.cpp:611](../src/backend/McpServer.cpp#L611) | Move a clip on the timeline / 移动采样 |
| `resize_clip` | [backend/McpServer.cpp:617](../src/backend/McpServer.cpp#L617) | Stretch a whole clip while preserving its source media / 整体拉伸采样并保留原始素材 |
| `duplicate_clip` | [backend/McpServer.cpp:625](../src/backend/McpServer.cpp#L625) | Deep-copy a clip and its notes to a timeline position / 深度复制采样及其音符到指定位置 |
| `transpose_note` | [backend/McpServer.cpp:633](../src/backend/McpServer.cpp#L633) | Move a note and its whole contour / 整体移动音高线 |
| `edit_notes_pitch` | [backend/McpServer.cpp:639](../src/backend/McpServer.cpp#L639) | Batch transpose, set, average or quantize note pitches / 批量移调、设置、平均或量化音符 |
| `duplicate_notes` | [backend/McpServer.cpp:655](../src/backend/McpServer.cpp#L655) | Deep-copy selected notes into a target clip / 深度复制所选音符到目标采样 |
| `resize_note` | [backend/McpServer.cpp:665](../src/backend/McpServer.cpp#L665) | Change note time bounds / 修改音符时间 |
| `set_note` | [backend/McpServer.cpp:673](../src/backend/McpServer.cpp#L673) | Set pitch, Robust Pitch Curve, tension, breath, formant, gain, Attack and consonant parameters / 设置稳健音高线及全部音符参数 |
| `set_pitch_curve` | [backend/McpServer.cpp:748](../src/backend/McpServer.cpp#L748) | Draw an absolute target-pitch curve without replacing source F0 / 绘制目标音高线并保留原始 F0 |
| `add_note` | [backend/McpServer.cpp:755](../src/backend/McpServer.cpp#L755) | Create a note in a clip / 在采样中创建音符 |
| `remove_note` | [backend/McpServer.cpp:765](../src/backend/McpServer.cpp#L765) | Delete a note / 删除音符 |
| `toggle_note_connection` | [backend/McpServer.cpp:769](../src/backend/McpServer.cpp#L769) | Connect or separate adjacent notes / 连接或分离相邻音符 |
| `set_notes_portamento` | [backend/McpServer.cpp:774](../src/backend/McpServer.cpp#L774) | Set real pitch-only glide bridges; never join source audio or change envelopes / 设置真实滑音连接，不拼接素材 |
| `remove_clip` | [backend/McpServer.cpp:780](../src/backend/McpServer.cpp#L780) | Delete a clip / 删除采样 |
| `remove_track` | [backend/McpServer.cpp:784](../src/backend/McpServer.cpp#L784) | Delete a track / 删除轨道 |
| `undo` | [backend/McpServer.cpp:788](../src/backend/McpServer.cpp#L788) | Undo the last project edit / 撤销工程编辑 |
| `redo` | [backend/McpServer.cpp:789](../src/backend/McpServer.cpp#L789) | Redo the last project edit / 重做工程编辑 |
| `utau_render_selection` | [backend/McpServer.cpp:790](../src/backend/McpServer.cpp#L790) | Choose which UTAU notes render and play; empty selects every note / 选择参与 UTAU 渲染与试听的音符 |
| `set_utau_resampler` | [backend/McpServer.cpp:797](../src/backend/McpServer.cpp#L797) | Point UTAU rendering at a resampler executable / 指定 UTAU 重采样器 |
| `render_prepare` | [backend/McpServer.cpp:802](../src/backend/McpServer.cpp#L802) | Pre-render the current project with its selected algorithms / 按当前所选算法预渲染工程 |
| `render_status` | [backend/McpServer.cpp:808](../src/backend/McpServer.cpp#L808) | Read pre-render progress and active backends / 读取预渲染进度与实际后端 |
| `export_wav` | [backend/McpServer.cpp:809](../src/backend/McpServer.cpp#L809) | Render every note and export to WAV; track_id exports one track alone, from_seconds/to_seconds one stretch / 渲染全曲并导出 WAV，track_id 可单独导出一个轨道 |
| `transport_play` | [backend/McpServer.cpp:824](../src/backend/McpServer.cpp#L824) | Render if needed and start playback; play_until_seconds stops where a selection ends / 必要时预渲染并开始播放 |
| `transport_stop` | [backend/McpServer.cpp:832](../src/backend/McpServer.cpp#L832) | Stop transport playback / 停止播放 |
| `transport_seek` | [backend/McpServer.cpp:833](../src/backend/McpServer.cpp#L833) | Seek transport to position_seconds / 跳转播放位置 |
| `transport_status` | [backend/McpServer.cpp:837](../src/backend/McpServer.cpp#L837) | Read playback position and render state / 读取播放位置与渲染状态 |
| `sample_settings_read` | [backend/McpServer.cpp:838](../src/backend/McpServer.cpp#L838) | Read or derive audio otto.ini regions / 读取或生成音频 otto.ini 分段 |
| `sample_settings_save` | [backend/McpServer.cpp:843](../src/backend/McpServer.cpp#L843) | Save audio regions to otto.ini / 保存音频分段到 otto.ini |
| `oto_import` | [backend/McpServer.cpp:850](../src/backend/McpServer.cpp#L850) | Import one audio file's regions from UTAU oto.ini / 从 UTAU oto.ini 导入单个音频分段 |
| `oto_export` | [backend/McpServer.cpp:859](../src/backend/McpServer.cpp#L859) | Export one audio file's regions to UTAU oto.ini / 将单个音频分段导出为 UTAU oto.ini |
| `jie_oto_create` | [backend/McpServer.cpp:865](../src/backend/McpServer.cpp#L865) | Seed a voicebank's four-region oto4.ini from its oto.ini / 按 oto.ini 生成界•OTO |
| `voicebank_import` | [backend/McpServer.cpp:869](../src/backend/McpServer.cpp#L869) | Inspect a UTAU or native material folder without writing annotations / 只读检查 UTAU 或原生素材目录，不写入标注 |
| `read_file` | [backend/McpServer.cpp:873](../src/backend/McpServer.cpp#L873) | Read a byte range as base64 / 读取任意文件内容 |
| `list_directory` | [backend/McpServer.cpp:881](../src/backend/McpServer.cpp#L881) | List a directory with type and size / 列出目录内容 |
| `editor_selection` | [MainComponentMcp.cpp:57](../src/MainComponentMcp.cpp#L57) | Read ONLY notes currently selected in the editor / 读取当前选区。Use FIRST for 'these notes', 'this phrase', or 'what did I select'. Returns lyrics, note names, timing and tracks; empty selection returns no notes, NEVER the whole project. |
| `editor_status` | [MainComponentMcp.cpp:60](../src/MainComponentMcp.cpp#L60) | Read the connected window, current file, revision, selection, playback and DS state |
| `editor_query` | [MainComponentMcp.cpp:61](../src/MainComponentMcp.cpp#L61) | Read selected notes by default; range uses absolute project seconds; curves are opt-in |
| `editor_select` | [MainComponentMcp.cpp:66](../src/MainComponentMcp.cpp#L66) | Change real GUI selection and focus; [] clears it |
| `editor_batch` | [MainComponentMcp.cpp:67](../src/MainComponentMcp.cpp#L67) | Apply up to 128 project edit commands atomically with one Undo; imports, file and transport commands are excluded |
| `editor_job_status` | [MainComponentMcp.cpp:69](../src/MainComponentMcp.cpp#L69) | Read a background job's state/result; completed does not imply success, check result.isError |
| `editor_cancel_job` | [MainComponentMcp.cpp:70](../src/MainComponentMcp.cpp#L70) | Cancel a pending job; staged edits are discarded, external file writes may already have completed |
| `ds_generate_pitch` | [MainComponentMcp.cpp:71](../src/MainComponentMcp.cpp#L71) | Regenerate DS pitch using GUI logic; selection required unless whole_track=true |
| `ds_generate_parameters` | [MainComponentMcp.cpp:72](../src/MainComponentMcp.cpp#L72) | Regenerate DS prediction parameters using GUI logic; preserve offsets and unselected notes |
| `ds_query_phonemes` | [MainComponentMcp.cpp:73](../src/MainComponentMcp.cpp#L73) | Predict phoneme timing in full phrase context, returning only selected notes by default; includes stable note IDs and timing context |
| `ds_capabilities` | [MainComponentMcp.cpp:74](../src/MainComponentMcp.cpp#L74) | Inspect the focused DS voicebank and its parameter support asynchronously |
| `ds_set_pitch_offset` | [MainComponentMcp.cpp:75](../src/MainComponentMcp.cpp#L75) | Set an independent additive semitone curve; preserves the original prediction reference |
| `ds_set_parameter` | [MainComponentMcp.cpp:77](../src/MainComponentMcp.cpp#L77) | Set a DS actual parameter or offset curve; supports only parameters exposed by this voicebank |
| `ds_set_pronunciation` | [MainComponentMcp.cpp:80](../src/MainComponentMcp.cpp#L80) | Set per-note pronunciation override; empty text restores automatic conversion |
| `ds_set_timing` | [MainComponentMcp.cpp:82](../src/MainComponentMcp.cpp#L82) | Set phoneme onset overrides from an inspected timing context; null restores prediction |
| `hamood_analyse_audio` | [MainComponentMcp.cpp:84](../src/MainComponentMcp.cpp#L84) | Analyse one accompaniment clip with Beat This! small0 + BTC; background job, cached by source audio/model hash. Read-only project operation. No pitch or tempo edits. |
| `hamood_alignment_preview` | [MainComponentMcp.cpp:86](../src/MainComponentMcp.cpp#L86) | Preview a small beat-phase offset against the existing project tempo grid. Does NOT resolve whole-bar ambiguity or change audio/tempo. Inspect start/middle/end windows before applying move_clip. |
| `hamood_audio_context` | [MainComponentMcp.cpp:90](../src/MainComponentMcp.cpp#L90) | Read cached timestamped chords, low-register chroma candidates, beats and downbeats on the CURRENT project timeline. Scores are not calibrated probabilities. Defaults to selected-note time range; explicit range or whole_clip required otherwise. |
| `hamood_get_context` | [MainComponentMcp.cpp:95](../src/MainComponentMcp.cpp#L95) | Read project-persisted HAMOOD settings, manual key ranges, chord timeline, section labels and confirmation states. Timeline positions use quarter notes. |
| `hamood_set_context` | [MainComponentMcp.cpp:96](../src/MainComponentMcp.cpp#L96) | Replace HAMOOD project metadata with one undo step; read hamood_get_context first and preserve unrelated entries. schema=1; settings object; chords, sections, key_predictions arrays. Chords require start_quarter/end_quarter, label, pitch_classes 0..11, score 0..1 and confirmed. Confirmed chords survive audio reanalysis. |
| `hamood_preview` | [MainComponentMcp.cpp:98](../src/MainComponentMcp.cpp#L98) | 只读预览和声方案 |
| `hamood_generate` | [MainComponentMcp.cpp:98](../src/MainComponentMcp.cpp#L98) | 生成独立和声轨道，一次撤销 |

## 启动参数字面量

仅来自 Main.cpp 的 `--…` 字符串去重；包括诊断、启动选项和帮助文字，不代表全是用户功能，也不等同于测试已执行。参数说明见原有 [命令索引](AI_SOURCE_INDEX.md)。

| 参数 | 首次出现位置 |
| --- | --- |
| `--migrate-otto-folder` | [Main.cpp:258](../src/Main.cpp#L258) |
| `--export-otto-folder` | [Main.cpp:259](../src/Main.cpp#L259) |
| `--include-bare` | [Main.cpp:269](../src/Main.cpp#L269) |
| `--replace` | [Main.cpp:273](../src/Main.cpp#L273) |
| `--smoke-main-project-registration-ui` | [Main.cpp:283](../src/Main.cpp#L283) |
| `--smoke-classic-note-scope` | [Main.cpp:289](../src/Main.cpp#L289) |
| `--smoke-oto-provenance` | [Main.cpp:294](../src/Main.cpp#L294) |
| `--smoke-community-dataset-menu` | [Main.cpp:299](../src/Main.cpp#L299) |
| `--smoke-material-root-registration` | [Main.cpp:304](../src/Main.cpp#L304) |
| `--smoke-variant-asset-ui` | [Main.cpp:309](../src/Main.cpp#L309) |
| `--smoke-voicebank-family` | [Main.cpp:314](../src/Main.cpp#L314) |
| `--smoke-track-types` | [Main.cpp:319](../src/Main.cpp#L319) |
| `--smoke-engine-view-capabilities` | [Main.cpp:324](../src/Main.cpp#L324) |
| `--smoke-track-editing-interfaces-ui` | [Main.cpp:331](../src/Main.cpp#L331) |
| `--smoke-material-variants` | [Main.cpp:338](../src/Main.cpp#L338) |
| `--smoke-community-dataset` | [Main.cpp:343](../src/Main.cpp#L343) |
| `--smoke-ustx-importer` | [Main.cpp:349](../src/Main.cpp#L349) |
| `--smoke-japanese-inference-dialog` | [Main.cpp:354](../src/Main.cpp#L354) |
| `--smoke-japanese-inference-modes` | [Main.cpp:359](../src/Main.cpp#L359) |
| `--smoke-native-source-reopen-scope` | [Main.cpp:364](../src/Main.cpp#L364) |
| `--smoke-native-material-overlap` | [Main.cpp:370](../src/Main.cpp#L370) |
| `--smoke-native-edge-profile-gui` | [Main.cpp:376](../src/Main.cpp#L376) |
| `--smoke-native-pronunciation-edge-edit` | [Main.cpp:381](../src/Main.cpp#L381) |
| `--smoke-native-source-slice-edges` | [Main.cpp:386](../src/Main.cpp#L386) |
| `--smoke-native-source-assembly` | [Main.cpp:391](../src/Main.cpp#L391) |
| `--smoke-world-source-assembly` | [Main.cpp:395](../src/Main.cpp#L395) |
| `--smoke-focused-defaults` | [Main.cpp:399](../src/Main.cpp#L399) |
| `--smoke-transition-pair-import-options` | [Main.cpp:401](../src/Main.cpp#L401) |
| `--smoke-legacy-native-usage` | [Main.cpp:403](../src/Main.cpp#L403) |
| `--smoke-native-melodyne-yellow` | [Main.cpp:405](../src/Main.cpp#L405) |
| `--smoke-native-permanent-seam` | [Main.cpp:407](../src/Main.cpp#L407) |
| `--smoke-native-virtual-single-line` | [Main.cpp:412](../src/Main.cpp#L412) |
| `--smoke-native-virtual-handover-window` | [Main.cpp:417](../src/Main.cpp#L417) |
| `--smoke-native-virtual-handover-gesture` | [Main.cpp:422](../src/Main.cpp#L422) |
| `--smoke-native-virtual-flat-bezier` | [Main.cpp:427](../src/Main.cpp#L427) |
| `--smoke-native-virtual-coupled-move` | [Main.cpp:432](../src/Main.cpp#L432) |
| `--smoke-native-note-transition-menu` | [Main.cpp:437](../src/Main.cpp#L437) |
| `--render-saved-nsf-three` | [Main.cpp:442](../src/Main.cpp#L442) |
| `--profile-teio-request-fingerprint` | [Main.cpp:447](../src/Main.cpp#L447) |
| `--render-teio-full-virtual` | [Main.cpp:459](../src/Main.cpp#L459) |
| `--smoke-main-real-multi-bridge-window` | [Main.cpp:465](../src/Main.cpp#L465) |
| `--smoke-native-real-multi-split-audio` | [Main.cpp:471](../src/Main.cpp#L471) |
| `--smoke-native-virtual-hop-curve` | [Main.cpp:476](../src/Main.cpp#L476) |
| `--smoke-native-virtual-peer-waveform` | [Main.cpp:481](../src/Main.cpp#L481) |
| `--smoke-native-virtual-curve-tool` | [Main.cpp:486](../src/Main.cpp#L486) |
| `--smoke-native-virtual-nonlinear-curve` | [Main.cpp:491](../src/Main.cpp#L491) |
| `--smoke-native-virtual-same-file-skip` | [Main.cpp:496](../src/Main.cpp#L496) |
| `--smoke-melodyne-optimization-entry` | [Main.cpp:501](../src/Main.cpp#L501) |
| `--smoke-native-virtual-multi-split` | [Main.cpp:506](../src/Main.cpp#L506) |
| `--smoke-native-virtual-chain-extension` | [Main.cpp:511](../src/Main.cpp#L511) |
| `--smoke-native-virtual-unavailable-group` | [Main.cpp:516](../src/Main.cpp#L516) |
| `--smoke-native-virtual-shared-pitch-gesture` | [Main.cpp:521](../src/Main.cpp#L521) |
| `--smoke-native-virtual-chain-shared` | [Main.cpp:526](../src/Main.cpp#L526) |
| `--smoke-native-virtual-waveform-hash` | [Main.cpp:531](../src/Main.cpp#L531) |
| `--smoke-native-virtual-export-parity` | [Main.cpp:536](../src/Main.cpp#L536) |
| `--smoke-native-virtual-glide-audio` | [Main.cpp:541](../src/Main.cpp#L541) |
| `--smoke-native-partial-cut-drag-gesture` | [Main.cpp:546](../src/Main.cpp#L546) |
| `--real-partial-cut-drag` | [Main.cpp:551](../src/Main.cpp#L551) |
| `--smoke-native-partial-cut-move` | [Main.cpp:559](../src/Main.cpp#L559) |
| `--smoke-native-consonant-merge-detection` | [Main.cpp:564](../src/Main.cpp#L564) |
| `--smoke-native-consonant-merge-model` | [Main.cpp:569](../src/Main.cpp#L569) |
| `--smoke-native-consonant-merge-plan` | [Main.cpp:574](../src/Main.cpp#L574) |
| `--smoke-native-virtual-connection-ui` | [Main.cpp:579](../src/Main.cpp#L579) |
| `--smoke-native-logical-transition-registration` | [Main.cpp:584](../src/Main.cpp#L584) |
| `--smoke-native-split-transition-detection` | [Main.cpp:589](../src/Main.cpp#L589) |
| `--smoke-native-split-transition-mixer` | [Main.cpp:594](../src/Main.cpp#L594) |
| `--smoke-native-split-transition-plan` | [Main.cpp:599](../src/Main.cpp#L599) |
| `--smoke-native-virtual-auto` | [Main.cpp:604](../src/Main.cpp#L604) |
| `--smoke-native-virtual-pitch-edit` | [Main.cpp:609](../src/Main.cpp#L609) |
| `--smoke-native-virtual-move` | [Main.cpp:614](../src/Main.cpp#L614) |
| `--smoke-native-virtual-pitch-request` | [Main.cpp:619](../src/Main.cpp#L619) |
| `--smoke-native-virtual-pitch-plan` | [Main.cpp:624](../src/Main.cpp#L624) |
| `--smoke-native-virtual-mixer` | [Main.cpp:629](../src/Main.cpp#L629) |
| `--smoke-native-virtual-entrypoints` | [Main.cpp:634](../src/Main.cpp#L634) |
| `--smoke-native-virtual-model` | [Main.cpp:639](../src/Main.cpp#L639) |
| `--smoke-native-virtual-plan` | [Main.cpp:644](../src/Main.cpp#L644) |
| `--smoke-native-source-edge-mixer` | [Main.cpp:649](../src/Main.cpp#L649) |
| `--smoke-native-score-replacement-ui` | [Main.cpp:654](../src/Main.cpp#L654) |
| `--smoke-native-score-material-replacement` | [Main.cpp:660](../src/Main.cpp#L660) |
| `--smoke-translation-no-file` | [Main.cpp:665](../src/Main.cpp#L665) |
| `--smoke-translation-file` | [Main.cpp:670](../src/Main.cpp#L670) |
| `--export-translations` | [Main.cpp:675](../src/Main.cpp#L675) |
| `--smoke-teio-transition-registration` | [Main.cpp:684](../src/Main.cpp#L684) |
| `--smoke-native-transition-detection` | [Main.cpp:689](../src/Main.cpp#L689) |
| `--smoke-native-source-trend` | [Main.cpp:694](../src/Main.cpp#L694) |
| `--smoke-native-transition-shape` | [Main.cpp:699](../src/Main.cpp#L699) |
| `--smoke-native-midi-material` | [Main.cpp:704](../src/Main.cpp#L704) |
| `--smoke-native-material-appearance` | [Main.cpp:709](../src/Main.cpp#L709) |
| `--smoke-native-note-clicks` | [Main.cpp:714](../src/Main.cpp#L714) |
| `--smoke-native-force-connection-ui` | [Main.cpp:719](../src/Main.cpp#L719) |
| `--smoke-native-force-connection` | [Main.cpp:725](../src/Main.cpp#L725) |
| `--smoke-melodyne-timing` | [Main.cpp:730](../src/Main.cpp#L730) |
| `--smoke-native-source-edge-plan` | [Main.cpp:736](../src/Main.cpp#L736) |
| `--smoke-source-preview-ui` | [Main.cpp:741](../src/Main.cpp#L741) |
| `--smoke-main-japanese-annotation` | [Main.cpp:747](../src/Main.cpp#L747) |
| `--smoke-japanese-annotation-safety` | [Main.cpp:754](../src/Main.cpp#L754) |
| `--smoke-japanese-auto-annotation` | [Main.cpp:759](../src/Main.cpp#L759) |
| `--smoke-main-community-import` | [Main.cpp:766](../src/Main.cpp#L766) |
| `--smoke-community-import-gui` | [Main.cpp:773](../src/Main.cpp#L773) |
| `--smoke-registered-project-audio` | [Main.cpp:779](../src/Main.cpp#L779) |
| `--smoke-registration-facts` | [Main.cpp:785](../src/Main.cpp#L785) |
| `--smoke-main-material-variants` | [Main.cpp:790](../src/Main.cpp#L790) |
| `--audit-project-material-registration` | [Main.cpp:796](../src/Main.cpp#L796) |
| `--smoke-project-material-registration-model` | [Main.cpp:805](../src/Main.cpp#L805) |
| `--smoke-project-material-registration` | [Main.cpp:810](../src/Main.cpp#L810) |
| `--smoke-note-material-local-override` | [Main.cpp:815](../src/Main.cpp#L815) |
| `--smoke-local-material-scope` | [Main.cpp:820](../src/Main.cpp#L820) |
| `--smoke-connection-pitch-ui` | [Main.cpp:825](../src/Main.cpp#L825) |
| `--smoke-host-pitch-projection` | [Main.cpp:830](../src/Main.cpp#L830) |
| `--smoke-material-pronunciation-catalog` | [Main.cpp:835](../src/Main.cpp#L835) |
| `--smoke-graph-pitch-edit-batch` | [Main.cpp:840](../src/Main.cpp#L840) |
| `--smoke-overlap-selector` | [Main.cpp:845](../src/Main.cpp#L845) |
| `--smoke-forced-portamento-audio` | [Main.cpp:850](../src/Main.cpp#L850) |
| `--smoke-track-pitch-scope` | [Main.cpp:856](../src/Main.cpp#L856) |
| `--smoke-forced-portamento` | [Main.cpp:861](../src/Main.cpp#L861) |
| `--smoke-named-vowel` | [Main.cpp:866](../src/Main.cpp#L866) |
| `--smoke-vowel-timing-audio` | [Main.cpp:871](../src/Main.cpp#L871) |
| `--smoke-pronunciation-timing` | [Main.cpp:877](../src/Main.cpp#L877) |
| `--smoke-adjacent-pronunciation` | [Main.cpp:882](../src/Main.cpp#L882) |
| `--smoke-vowel-stretch-alignment` | [Main.cpp:887](../src/Main.cpp#L887) |
| `--smoke-material-source-replacement` | [Main.cpp:892](../src/Main.cpp#L892) |
| `--smoke-native-nsf-compatibility` | [Main.cpp:897](../src/Main.cpp#L897) |
| `--smoke-native-workflow-e2e` | [Main.cpp:903](../src/Main.cpp#L903) |
| `--smoke-material-topology-audio` | [Main.cpp:912](../src/Main.cpp#L912) |
| `--smoke-midi-export-scope` | [Main.cpp:918](../src/Main.cpp#L918) |
| `--smoke-unified-draw-action` | [Main.cpp:924](../src/Main.cpp#L924) |
| `--smoke-main-source-replacement-ui` | [Main.cpp:930](../src/Main.cpp#L930) |
| `--smoke-single-entry-ui` | [Main.cpp:938](../src/Main.cpp#L938) |
| `--smoke-nsf-native-expression` | [Main.cpp:946](../src/Main.cpp#L946) |
| `--smoke-material-topology` | [Main.cpp:954](../src/Main.cpp#L954) |
| `--smoke-native-connection-graph` | [Main.cpp:960](../src/Main.cpp#L960) |
| `--smoke-material-format-auto` | [Main.cpp:966](../src/Main.cpp#L966) |
| `--smoke-unified-material-editor` | [Main.cpp:972](../src/Main.cpp#L972) |
| `--smoke-material-insertion` | [Main.cpp:978](../src/Main.cpp#L978) |
| `--smoke-material-asset-tree` | [Main.cpp:984](../src/Main.cpp#L984) |
| `--smoke-material-annotation` | [Main.cpp:990](../src/Main.cpp#L990) |
| `--smoke-material-binding` | [Main.cpp:996](../src/Main.cpp#L996) |
| `--smoke-otto-material` | [Main.cpp:1002](../src/Main.cpp#L1002) |
| `--audit-hamood-keys` | [Main.cpp:1011](../src/Main.cpp#L1011) |
| `--smoke-hamood-utau` | [Main.cpp:1030](../src/Main.cpp#L1030) |
| `--smoke-hamood` | [Main.cpp:1036](../src/Main.cpp#L1036) |
| `--smoke-diffsinger-pitch-offset` | [Main.cpp:1043](../src/Main.cpp#L1043) |
| `--smoke-accompaniment-tempo` | [Main.cpp:1050](../src/Main.cpp#L1050) |
| `--smoke-accompaniment` | [Main.cpp:1066](../src/Main.cpp#L1066) |
| `--smoke-wav-export-options` | [Main.cpp:1073](../src/Main.cpp#L1073) |
| `--smoke-diffsinger-pitch-persistence` | [Main.cpp:1083](../src/Main.cpp#L1083) |
| `--reopen` | [Main.cpp:1087](../src/Main.cpp#L1087) |
| `--smoke-diffsinger-pitch-handles` | [Main.cpp:1091](../src/Main.cpp#L1091) |
| `--smoke-diffsinger-pitch-restore` | [Main.cpp:1097](../src/Main.cpp#L1097) |
| `--smoke-diffsinger-pitch-reference` | [Main.cpp:1104](../src/Main.cpp#L1104) |
| `--smoke-continuous-flag` | [Main.cpp:1110](../src/Main.cpp#L1110) |
| `--smoke-classic-flag-ui` | [Main.cpp:1117](../src/Main.cpp#L1117) |
| `--smoke-diffsinger-inference` | [Main.cpp:1126](../src/Main.cpp#L1126) |
| `--smoke-diffsinger-parameter-retake` | [Main.cpp:1132](../src/Main.cpp#L1132) |
| `--smoke-diffsinger-cache` | [Main.cpp:1139](../src/Main.cpp#L1139) |
| `--smoke-diffsinger-parameters` | [Main.cpp:1144](../src/Main.cpp#L1144) |
| `--smoke-diffsinger-expressions` | [Main.cpp:1149](../src/Main.cpp#L1149) |
| `--smoke-diffsinger-consonants` | [Main.cpp:1156](../src/Main.cpp#L1156) |
| `--smoke-diffsinger-timing` | [Main.cpp:1162](../src/Main.cpp#L1162) |
| `--smoke-diffsinger-ui` | [Main.cpp:1169](../src/Main.cpp#L1169) |
| `--smoke-diffsinger` | [Main.cpp:1178](../src/Main.cpp#L1178) |
| `--smoke-chinese-cvvc` | [Main.cpp:1185](../src/Main.cpp#L1185) |
| `--smoke-output-engine` | [Main.cpp:1193](../src/Main.cpp#L1193) |
| `--smoke-native-vibrato` | [Main.cpp:1202](../src/Main.cpp#L1202) |
| `--smoke-native-vibrato-render` | [Main.cpp:1213](../src/Main.cpp#L1213) |
| `--smoke-native-noise-option` | [Main.cpp:1219](../src/Main.cpp#L1219) |
| `--smoke-native-incremental` | [Main.cpp:1227](../src/Main.cpp#L1227) |
| `--smoke-native-shutdown` | [Main.cpp:1235](../src/Main.cpp#L1235) |
| `--smoke-native-noise` | [Main.cpp:1243](../src/Main.cpp#L1243) |
| `--smoke-wcs-nonloop` | [Main.cpp:1250](../src/Main.cpp#L1250) |
| `--smoke-hn-cache` | [Main.cpp:1257](../src/Main.cpp#L1257) |
| `--force` | [Main.cpp:1260](../src/Main.cpp#L1260) |
| `--smoke-hifisampler` | [Main.cpp:1264](../src/Main.cpp#L1264) |
| `--smoke-hifishifter-mel` | [Main.cpp:1264](../src/Main.cpp#L1264) |
| `--smoke-nsf-regions` | [Main.cpp:1273](../src/Main.cpp#L1273) |
| `--smoke-nsf-pitch-transitions` | [Main.cpp:1281](../src/Main.cpp#L1281) |
| `--smoke-voicebank-import` | [Main.cpp:1287](../src/Main.cpp#L1287) |
| `--smoke-voicebank-selection` | [Main.cpp:1293](../src/Main.cpp#L1293) |
| `--smoke-nsf-picker` | [Main.cpp:1302](../src/Main.cpp#L1302) |
| `--smoke-cross-region-editing` | [Main.cpp:1311](../src/Main.cpp#L1311) |
| `--smoke-hamood-persistence` | [Main.cpp:1311](../src/Main.cpp#L1311) |
| `--smoke-mou-to-jie` | [Main.cpp:1318](../src/Main.cpp#L1318) |
| `--smoke-oto-overlap` | [Main.cpp:1324](../src/Main.cpp#L1324) |
| `--smoke-modeless-oto` | [Main.cpp:1330](../src/Main.cpp#L1330) |
| `--smoke-oto-continuity` | [Main.cpp:1335](../src/Main.cpp#L1335) |
| `--smoke-native-envelope` | [Main.cpp:1340](../src/Main.cpp#L1340) |
| `--smoke-cvvc-layout-cache` | [Main.cpp:1345](../src/Main.cpp#L1345) |
| `--smoke-auto-crossfade` | [Main.cpp:1350](../src/Main.cpp#L1350) |
| `--smoke-pitch-point-selection` | [Main.cpp:1355](../src/Main.cpp#L1355) |
| `--smoke-point-group` | [Main.cpp:1360](../src/Main.cpp#L1360) |
| `--smoke-envelope-point-selection` | [Main.cpp:1365](../src/Main.cpp#L1365) |
| `--smoke-native-linked-envelope` | [Main.cpp:1370](../src/Main.cpp#L1370) |
| `--smoke-advanced-envelope` | [Main.cpp:1375](../src/Main.cpp#L1375) |
| `--smoke-advanced-envelope-panel` | [Main.cpp:1375](../src/Main.cpp#L1375) |
| `--smoke-playback-priority` | [Main.cpp:1380](../src/Main.cpp#L1380) |
| `--smoke-ust-fidelity` | [Main.cpp:1381](../src/Main.cpp#L1381) |
| `--smoke-project-safety` | [Main.cpp:1382](../src/Main.cpp#L1382) |
| `--smoke-normal-display` | [Main.cpp:1390](../src/Main.cpp#L1390) |
| `--smoke-note-hints` | [Main.cpp:1398](../src/Main.cpp#L1398) |
| `--smoke-clip-editor-scope` | [Main.cpp:1406](../src/Main.cpp#L1406) |
| `--smoke-empty-tuning-clip` | [Main.cpp:1414](../src/Main.cpp#L1414) |
| `--smoke-track-gain-envelope` | [Main.cpp:1422](../src/Main.cpp#L1422) |
| `--smoke-native-rendered-waveform` | [Main.cpp:1430](../src/Main.cpp#L1430) |
| `--smoke-native-note-move` | [Main.cpp:1437](../src/Main.cpp#L1437) |
| `--smoke-native-source-pitch-restore` | [Main.cpp:1443](../src/Main.cpp#L1443) |
| `--smoke-native-audio-overlap-focus` | [Main.cpp:1449](../src/Main.cpp#L1449) |
| `--smoke-native-audio-overlap` | [Main.cpp:1455](../src/Main.cpp#L1455) |
| `--smoke-native-linked-pitch` | [Main.cpp:1461](../src/Main.cpp#L1461) |
| `--smoke-source-breathiness` | [Main.cpp:1468](../src/Main.cpp#L1468) |
| `--smoke-native-unpitched-regions` | [Main.cpp:1475](../src/Main.cpp#L1475) |
| `--smoke-native-consonant-display` | [Main.cpp:1483](../src/Main.cpp#L1483) |
| `--smoke-native-pitch-visibility` | [Main.cpp:1490](../src/Main.cpp#L1490) |
| `--smoke-native-audio-link` | [Main.cpp:1496](../src/Main.cpp#L1496) |
| `--smoke-native-note-join` | [Main.cpp:1502](../src/Main.cpp#L1502) |
| `--smoke-native-trim-expand` | [Main.cpp:1507](../src/Main.cpp#L1507) |
| `--smoke-native-audio-trim` | [Main.cpp:1512](../src/Main.cpp#L1512) |
| `--smoke-native-audio-disconnect` | [Main.cpp:1518](../src/Main.cpp#L1518) |
| `--smoke-native-note-copy-paste` | [Main.cpp:1524](../src/Main.cpp#L1524) |
| `--smoke-native-import-performance` | [Main.cpp:1531](../src/Main.cpp#L1531) |
| `--smoke-native-source-pitch` | [Main.cpp:1538](../src/Main.cpp#L1538) |
| `--smoke-native-waveform-preview` | [Main.cpp:1546](../src/Main.cpp#L1546) |
| `--smoke-timeline-notes` | [Main.cpp:1554](../src/Main.cpp#L1554) |
| `--smoke-clip-gain-knob` | [Main.cpp:1562](../src/Main.cpp#L1562) |
| `--smoke-clip-merge` | [Main.cpp:1570](../src/Main.cpp#L1570) |
| `--smoke-timeline-selection` | [Main.cpp:1578](../src/Main.cpp#L1578) |
| `--smoke-clip-trim` | [Main.cpp:1586](../src/Main.cpp#L1586) |
| `--smoke-clip-split` | [Main.cpp:1595](../src/Main.cpp#L1595) |
| `--smoke-independent-zoom` | [Main.cpp:1604](../src/Main.cpp#L1604) |
| `--smoke-scroll-zoom` | [Main.cpp:1613](../src/Main.cpp#L1613) |
| `--preview-note-dance` | [Main.cpp:1622](../src/Main.cpp#L1622) |
| `--smoke-tempo-change` | [Main.cpp:1630](../src/Main.cpp#L1630) |
| `--smoke-timeline-pit` | [Main.cpp:1639](../src/Main.cpp#L1639) |
| `--mcp-live` | [Main.cpp:1645](../src/Main.cpp#L1645) |
| `--mcp-list-sessions` | [Main.cpp:1645](../src/Main.cpp#L1645) |
| `--session` | [Main.cpp:1648](../src/Main.cpp#L1648) |
| `--smoke-hifisampler-envelope` | [Main.cpp:1653](../src/Main.cpp#L1653) |
| `--smoke-nsf-project-note` | [Main.cpp:1659](../src/Main.cpp#L1659) |
| `--mcp` | [Main.cpp:1665](../src/Main.cpp#L1665) |
| `--roots` | [Main.cpp:1667](../src/Main.cpp#L1667) |
| `--smoke-oto-playback` | [Main.cpp:1679](../src/Main.cpp#L1679) |
| `--smoke-oto-editor` | [Main.cpp:1879](../src/Main.cpp#L1879) |
| `--click` | [Main.cpp:1990](../src/Main.cpp#L1990) |
| `--smoke-splice-envelope` | [Main.cpp:2002](../src/Main.cpp#L2002) |
| `--smoke-dialog-enter` | [Main.cpp:2112](../src/Main.cpp#L2112) |
| `--smoke-source-edit-view` | [Main.cpp:2170](../src/Main.cpp#L2170) |
| `--smoke-consonant-hold` | [Main.cpp:2232](../src/Main.cpp#L2232) |
| `--smoke-mou-panel-counts` | [Main.cpp:2452](../src/Main.cpp#L2452) |
| `--smoke-mou-consonant-ticks` | [Main.cpp:2543](../src/Main.cpp#L2543) |
| `--smoke-mou-boundary-drag` | [Main.cpp:2696](../src/Main.cpp#L2696) |
| `--smoke-prefix-map` | [Main.cpp:2785](../src/Main.cpp#L2785) |
| `--repair-mou-oto` | [Main.cpp:2826](../src/Main.cpp#L2826) |
| `--smoke-mou-oto-file` | [Main.cpp:2847](../src/Main.cpp#L2847) |
| `--smoke-utau-mode` | [Main.cpp:3046](../src/Main.cpp#L3046) |
| `--smoke-lane-marquee` | [Main.cpp:3186](../src/Main.cpp#L3186) |
| `--smoke-flag-lane` | [Main.cpp:3287](../src/Main.cpp#L3287) |
| `--smoke-keyboard-labels` | [Main.cpp:4148](../src/Main.cpp#L4148) |
| `--smoke-lane-labels` | [Main.cpp:4202](../src/Main.cpp#L4202) |
| `--smoke-export-targets` | [Main.cpp:4230](../src/Main.cpp#L4230) |
| `--smoke-audition-position` | [Main.cpp:4336](../src/Main.cpp#L4336) |
| `--smoke-lyric-envelope` | [Main.cpp:4378](../src/Main.cpp#L4378) |
| `--smoke-lyric-timing` | [Main.cpp:4450](../src/Main.cpp#L4450) |
| `--smoke-vibrato-bake` | [Main.cpp:4549](../src/Main.cpp#L4549) |
| `--smoke-mou-note-regions` | [Main.cpp:4662](../src/Main.cpp#L4662) |
| `--smoke-region-guides` | [Main.cpp:4948](../src/Main.cpp#L4948) |
| `--smoke-rest-playback` | [Main.cpp:5022](../src/Main.cpp#L5022) |
| `--smoke-rest-lyric` | [Main.cpp:5118](../src/Main.cpp#L5118) |
| `--smoke-missing-alias-piano` | [Main.cpp:5281](../src/Main.cpp#L5281) |
| `--smoke-flag-curve-mode` | [Main.cpp:5408](../src/Main.cpp#L5408) |
| `--smoke-flag-point-value` | [Main.cpp:5593](../src/Main.cpp#L5593) |
| `--smoke-flag-overlap` | [Main.cpp:5683](../src/Main.cpp#L5683) |
| `--smoke-load-timing` | [Main.cpp:5798](../src/Main.cpp#L5798) |
| `--smoke-flatten-pitch-line` | [Main.cpp:5911](../src/Main.cpp#L5911) |
| `--smoke-note-across-tracks` | [Main.cpp:6071](../src/Main.cpp#L6071) |
| `--smoke-clip-across-tracks` | [Main.cpp:6227](../src/Main.cpp#L6227) |
| `--smoke-reference-track` | [Main.cpp:6376](../src/Main.cpp#L6376) |
| `--smoke-native-pitch-seam` | [Main.cpp:6524](../src/Main.cpp#L6524) |
| `--smoke-native-pitch-points` | [Main.cpp:6596](../src/Main.cpp#L6596) |
| `--smoke-paste-at-pointer` | [Main.cpp:6820](../src/Main.cpp#L6820) |
| `--smoke-note-menu-split` | [Main.cpp:6963](../src/Main.cpp#L6963) |
| `--smoke-lyric-gate` | [Main.cpp:7145](../src/Main.cpp#L7145) |
| `--smoke-lyric-tab` | [Main.cpp:7242](../src/Main.cpp#L7242) |
| `--smoke-syllable-cuts` | [Main.cpp:7442](../src/Main.cpp#L7442) |
| `--smoke-single-syllable` | [Main.cpp:7624](../src/Main.cpp#L7624) |
| `--smoke-deleted-notes-silent` | [Main.cpp:7731](../src/Main.cpp#L7731) |
| `--smoke-delete-clip` | [Main.cpp:7878](../src/Main.cpp#L7878) |
| `--smoke-draw-unit` | [Main.cpp:7975](../src/Main.cpp#L7975) |
| `--smoke-new-track-draw` | [Main.cpp:8069](../src/Main.cpp#L8069) |
| `--smoke-draw-drag` | [Main.cpp:8178](../src/Main.cpp#L8178) |
| `--smoke-draw-overlap` | [Main.cpp:8350](../src/Main.cpp#L8350) |
| `--smoke-dropdown-arrow` | [Main.cpp:8536](../src/Main.cpp#L8536) |
| `--smoke-pitch-line` | [Main.cpp:8627](../src/Main.cpp#L8627) |
| `--smoke-view-menu` | [Main.cpp:8918](../src/Main.cpp#L8918) |
| `--inspect-melodyne-tracks` | [Main.cpp:9031](../src/Main.cpp#L9031) |
| `--smoke-melodyne-provider` | [Main.cpp:9042](../src/Main.cpp#L9042) |
| `--probe-melodyne-vst3-instance` | [Main.cpp:9067](../src/Main.cpp#L9067) |
| `--smoke-native-timing` | [Main.cpp:9079](../src/Main.cpp#L9079) |
| `--smoke-file-only-translation-ui` | [Main.cpp:9136](../src/Main.cpp#L9136) |
| `--smoke-gui198-layout` | [Main.cpp:9141](../src/Main.cpp#L9141) |
| `--smoke-integrated-ui` | [Main.cpp:9156](../src/Main.cpp#L9156) |
| `--smoke-integrated` | [Main.cpp:9165](../src/Main.cpp#L9165) |
| `--smoke-native-hjm` | [Main.cpp:9171](../src/Main.cpp#L9171) |
| `--smoke-batch-lyrics` | [Main.cpp:9238](../src/Main.cpp#L9238) |
| `--smoke-envelope-lanes` | [Main.cpp:9330](../src/Main.cpp#L9330) |
| `--smoke-wheel` | [Main.cpp:9389](../src/Main.cpp#L9389) |
| `--smoke-waveform-toggle` | [Main.cpp:9430](../src/Main.cpp#L9430) |
| `--smoke-envelope-base` | [Main.cpp:9540](../src/Main.cpp#L9540) |
| `--smoke-amplitude-waveform` | [Main.cpp:9861](../src/Main.cpp#L9861) |
| `--smoke-vibrato-end` | [Main.cpp:10319](../src/Main.cpp#L10319) |
| `--smoke-hanzi-pinyin` | [Main.cpp:10579](../src/Main.cpp#L10579) |
| `--smoke-shared-pitch-line` | [Main.cpp:10718](../src/Main.cpp#L10718) |
| `--smoke-waveform-alignment` | [Main.cpp:11376](../src/Main.cpp#L11376) |
| `--smoke-note-waveform` | [Main.cpp:11575](../src/Main.cpp#L11575) |
| `--smoke-utau-waveform` | [Main.cpp:11881](../src/Main.cpp#L11881) |
| `--smoke-voicebank-index` | [Main.cpp:12055](../src/Main.cpp#L12055) |
| `--smoke-ust-pitch` | [Main.cpp:12521](../src/Main.cpp#L12521) |
| `--low` | [Main.cpp:12826](../src/Main.cpp#L12826) |
| `--smoke-hf-daemon-owner` | [Main.cpp:12945](../src/Main.cpp#L12945) |
| `--quit` | [Main.cpp:12953](../src/Main.cpp#L12953) |
| `--smoke-hf-prewarm` | [Main.cpp:12958](../src/Main.cpp#L12958) |
| `--smoke-ust-vibrato` | [Main.cpp:13091](../src/Main.cpp#L13091) |
| `--smoke-mcp-roots` | [Main.cpp:13336](../src/Main.cpp#L13336) |
| `--smoke-mcp-schema` | [Main.cpp:13443](../src/Main.cpp#L13443) |
| `--smoke-dragged-transition` | [Main.cpp:13608](../src/Main.cpp#L13608) |
| `--smoke-forward-bend` | [Main.cpp:13831](../src/Main.cpp#L13831) |
| `--smoke-midi-track-import` | [Main.cpp:14003](../src/Main.cpp#L14003) |
| `--smoke-midi-export` | [Main.cpp:14332](../src/Main.cpp#L14332) |
| `--sounding` | [Main.cpp:14457](../src/Main.cpp#L14457) |
| `--still` | [Main.cpp:14589](../src/Main.cpp#L14589) |
| `--smoke-ust-import` | [Main.cpp:14625](../src/Main.cpp#L14625) |
| `--smoke-ust-encoding` | [Main.cpp:14766](../src/Main.cpp#L14766) |
| `--smoke-ust` | [Main.cpp:14918](../src/Main.cpp#L14918) |
| `--smoke-active-resampler` | [Main.cpp:15123](../src/Main.cpp#L15123) |
| `--smoke-bundled-resampler` | [Main.cpp:15168](../src/Main.cpp#L15168) |
| `--smoke-anchor-frequency` | [Main.cpp:15226](../src/Main.cpp#L15226) |
| `--smoke-note-oto` | [Main.cpp:15412](../src/Main.cpp#L15412) |
| `--smoke-prefix-jie` | [Main.cpp:15842](../src/Main.cpp#L15842) |
| `--smoke-prefix-note` | [Main.cpp:15996](../src/Main.cpp#L15996) |
| `--smoke-utau-overlap` | [Main.cpp:16353](../src/Main.cpp#L16353) |
| `--smoke-note-stp` | [Main.cpp:16668](../src/Main.cpp#L16668) |
| `--smoke-track-toggle-tips` | [Main.cpp:16907](../src/Main.cpp#L16907) |
| `--smoke-track-area-menu` | [Main.cpp:17154](../src/Main.cpp#L17154) |
| `--smoke-stretch-items` | [Main.cpp:17255](../src/Main.cpp#L17255) |
| `--smoke-render-order` | [Main.cpp:17307](../src/Main.cpp#L17307) |
| `--smoke-llsm2-length` | [Main.cpp:17439](../src/Main.cpp#L17439) |
| `--smoke-glide-seam` | [Main.cpp:17491](../src/Main.cpp#L17491) |
| `--smoke-native-envelope-base` | [Main.cpp:17555](../src/Main.cpp#L17555) |
| `--smoke-nsf-utau-phrase` | [Main.cpp:17639](../src/Main.cpp#L17639) |
| `--smoke-nsf-utau-synth` | [Main.cpp:17671](../src/Main.cpp#L17671) |
| `--smoke-nsf-utau-f0` | [Main.cpp:17719](../src/Main.cpp#L17719) |
| `--smoke-nsf-utau-mix` | [Main.cpp:17764](../src/Main.cpp#L17764) |
| `--smoke-nsf-utau-plan` | [Main.cpp:17829](../src/Main.cpp#L17829) |
| `--smoke-render-capability` | [Main.cpp:17886](../src/Main.cpp#L17886) |
| `--smoke-oto-hjm-discipline` | [Main.cpp:17937](../src/Main.cpp#L17937) |
| `--smoke-asset-register-dir` | [Main.cpp:18007](../src/Main.cpp#L18007) |
| `--smoke-oto-audio` | [Main.cpp:18051](../src/Main.cpp#L18051) |
| `--smoke-asset-audio` | [Main.cpp:18064](../src/Main.cpp#L18064) |
| `--smoke-project-voicebank-assets` | [Main.cpp:18078](../src/Main.cpp#L18078) |
| `--smoke-asset-search` | [Main.cpp:18084](../src/Main.cpp#L18084) |
| `--smoke-asset-manager` | [Main.cpp:18093](../src/Main.cpp#L18093) |
| `--smoke-gap-menu` | [Main.cpp:18218](../src/Main.cpp#L18218) |
| `--smoke-insert-gap` | [Main.cpp:18337](../src/Main.cpp#L18337) |
| `--smoke-ripple-delete` | [Main.cpp:18453](../src/Main.cpp#L18453) |
| `--smoke-paste-notes` | [Main.cpp:18542](../src/Main.cpp#L18542) |
| `--smoke-consonant-edge` | [Main.cpp:18718](../src/Main.cpp#L18718) |
| `--smoke-consonant-reset` | [Main.cpp:18874](../src/Main.cpp#L18874) |
| `--smoke-region-split` | [Main.cpp:18981](../src/Main.cpp#L18981) |
| `--smoke-vibrato-presets` | [Main.cpp:19073](../src/Main.cpp#L19073) |
| `--smoke-follow-rule` | [Main.cpp:19143](../src/Main.cpp#L19143) |
| `--smoke-play-until` | [Main.cpp:19215](../src/Main.cpp#L19215) |
| `--smoke-flags-field` | [Main.cpp:19274](../src/Main.cpp#L19274) |
| `--smoke-vertical-drag` | [Main.cpp:19339](../src/Main.cpp#L19339) |
| `--smoke-scroll-sync` | [Main.cpp:19432](../src/Main.cpp#L19432) |
| `--smoke-edit-follow` | [Main.cpp:19513](../src/Main.cpp#L19513) |
| `--smoke-note-drag-threshold` | [Main.cpp:19613](../src/Main.cpp#L19613) |
| `--smoke-roll-drag` | [Main.cpp:19688](../src/Main.cpp#L19688) |
| `--smoke-playhead-band` | [Main.cpp:19819](../src/Main.cpp#L19819) |
| `--smoke-roll-cull` | [Main.cpp:19872](../src/Main.cpp#L19872) |
| `--smoke-roll-timing` | [Main.cpp:19945](../src/Main.cpp#L19945) |
| `--smoke-envelope-shapes` | [Main.cpp:19991](../src/Main.cpp#L19991) |
| `--smoke-envelope-presets` | [Main.cpp:20308](../src/Main.cpp#L20308) |
| `--smoke-piano-roll` | [Main.cpp:20324](../src/Main.cpp#L20324) |
| `--render-project` | [Main.cpp:20372](../src/Main.cpp#L20372) |
| `--inspect-project` | [Main.cpp:20501](../src/Main.cpp#L20501) |
| `--inspect-settings` | [Main.cpp:20537](../src/Main.cpp#L20537) |
| `--smoke-oto-spectrum` | [Main.cpp:20564](../src/Main.cpp#L20564) |
| `--smoke-game-defaults` | [Main.cpp:20571](../src/Main.cpp#L20571) |
| `--inspect-analysis` | [Main.cpp:20577](../src/Main.cpp#L20577) |
| `--inspect-fcpe` | [Main.cpp:20599](../src/Main.cpp#L20599) |
| `--smoke-export` | [Main.cpp:20625](../src/Main.cpp#L20625) |
| `--smoke-mld5` | [Main.cpp:20655](../src/Main.cpp#L20655) |
| `--smoke-utau-selection` | [Main.cpp:20748](../src/Main.cpp#L20748) |
| `--smoke-utau-voicebank` | [Main.cpp:20862](../src/Main.cpp#L20862) |
| `--smoke-utau` | [Main.cpp:20945](../src/Main.cpp#L20945) |
| `--inspect-midi` | [Main.cpp:21031](../src/Main.cpp#L21031) |
| `--inspect-audio` | [Main.cpp:21054](../src/Main.cpp#L21054) |
| `--import-mpd` | [Main.cpp:21089](../src/Main.cpp#L21089) |
| `--auto-merge-consonants` | [Main.cpp:21096](../src/Main.cpp#L21096) |
| `--auto-virtual-connections` | [Main.cpp:21097](../src/Main.cpp#L21097) |
| `--auto-virtual-pitch` | [Main.cpp:21098](../src/Main.cpp#L21098) |
| `--no-register-transitions` | [Main.cpp:21099](../src/Main.cpp#L21099) |
| `--recursive-media` | [Main.cpp:21100](../src/Main.cpp#L21100) |
| `--inspect-mpd-split-transitions` | [Main.cpp:21124](../src/Main.cpp#L21124) |
| `--inspect-mpd-joins` | [Main.cpp:21178](../src/Main.cpp#L21178) |
| `--inspect-mpd` | [Main.cpp:21185](../src/Main.cpp#L21185) |

## 范围与静态体量

扫描自有 `juce/src` 非测试 `.h/.cpp/.inc`：227 个文件，120,517 行（包括注释和空行）。不含 juce/third_party、tests 和名称带 Smoke 的包含单元。

| 文件 | 行数 |
| --- | --- |
| [src/Main.cpp](../src/Main.cpp) | 21,375 |
| [src/PianoRollComponent.cpp](../src/PianoRollComponent.cpp) | 12,265 |
| [src/ProjectModel.cpp](../src/ProjectModel.cpp) | 11,875 |
| [src/MainComponent.cpp](../src/MainComponent.cpp) | 7,215 |
| [src/AudioEngine.cpp](../src/AudioEngine.cpp) | 4,395 |
| [src/OtoWaveformEditorComponent.cpp](../src/OtoWaveformEditorComponent.cpp) | 3,081 |
| [src/backend/UtauRenderer.cpp](../src/backend/UtauRenderer.cpp) | 2,835 |
| [src/PinyinTable.inc](../src/PinyinTable.inc) | 2,709 |
| [src/backend/NsfHifiganRenderer.cpp](../src/backend/NsfHifiganRenderer.cpp) | 2,195 |
| [src/backend/McpServer.cpp](../src/backend/McpServer.cpp) | 2,164 |
| [src/SampleSettings.cpp](../src/SampleSettings.cpp) | 2,076 |
| [src/AssetManagerComponent.cpp](../src/AssetManagerComponent.cpp) | 1,865 |
| [src/ProjectModel.h](../src/ProjectModel.h) | 1,722 |
| [src/PianoRollComponent.h](../src/PianoRollComponent.h) | 1,683 |
| [src/TimelineComponent.cpp](../src/TimelineComponent.cpp) | 1,187 |

完整扫描指纹及接口条目：[interface-audit-inventory.json](interface-audit-inventory.json)。行数用于导航和识别职责集中，不是性能或质量评分。
