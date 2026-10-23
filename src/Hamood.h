#pragma once
#include "ProjectModel.h"

namespace hachi::hamood
{
struct ManualSection
{
    int startBar = 1, endBar = 8; // Inclusive, ruler numbering; bar 0 can cover a pickup.
    int tonic = 0;
    bool minor = false;
    bool confirmed = true;
};
// Native implementation inspired by HARMOLOID's workflow, not a port of its code.
struct Chord { double start=0,end=0,score=0; juce::String label; std::vector<int> pitches; };
struct Options
{
    juce::String trackId;
    std::vector<juce::String> noteIds;
    bool wholeTrack = false;
    juce::String keyMode = "auto"; // auto, sections, manual
    int tonic = 0;
    bool minor = false;
    int sectionBars = 8;
    std::vector<ManualSection> manualSections;
    std::vector<int> voices { 2 }; // Signed diatonic steps: 2=third, 5=sixth, 7=octave.
    bool preservePitch = true;
    float gainDb = -6.0f;
    juce::String audioClipId;
    std::vector<Chord> chords;
    double minimumChordScore = .35;
};
struct Passage
{
    double start = 0, end = 0;
    int key = 0;
    double margin = 0;
    int startBar = 1, endBar = 1, evidenceNotes = 0;
    double evidenceBeats = 0;
    std::vector<std::pair<int,double>> candidates;
};
struct Plan
{
    juce::String error;
    std::vector<Passage> passages;
    struct Note { juce::String id, lyric; double start=0, duration=0; float pitch=0; int key=0; std::vector<float> targets; };
    std::vector<Note> notes;
    int chromaticNotes = 0, analysisNotes = 0;
    bool mixedTonality = false;
    juce::String keyMode;
    int audioAdjustedNotes = 0, audioCoveredNotes = 0;
    juce::String audioClipId;
    std::vector<std::pair<int,double>> globalCandidates;
    juce::String description() const;
    juce::var json() const;
};
juce::String keyName(int key);
juce::String voiceName(int steps);
float harmonyPitch(float pitch, int key, int steps);
Plan analyse(const ProjectData& project, const Options& options);
// Returns the new track IDs; all validation happens before changing data.
std::vector<juce::String> generate(ProjectData& data, const Options& options, const Plan& plan);
bool parseOptions(const juce::var& args, const juce::String& focusedTrack,
                  const std::vector<juce::String>& selection, Options& out, juce::String& error);
}
