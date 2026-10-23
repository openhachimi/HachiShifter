#pragma once
#include <juce_data_structures/juce_data_structures.h>
#include <juce_cryptography/juce_cryptography.h>

namespace hachi::projectio
{
// JUCE can return a partially parsed root on truncated input. Re-encoding must
// exactly match before a file is accepted or rotated over the previous backup.
inline juce::ValueTree validatedTree(const juce::MemoryBlock& bytes)
{
    const auto tree=juce::ValueTree::readFromData(bytes.getData(),bytes.getSize());
    if (!tree.hasType("HachiShifterProject")) return {};
    juce::MemoryOutputStream encoded;
    tree.writeToStream(encoded);
    return encoded.getMemoryBlock()==bytes ? tree : juce::ValueTree{};
}
inline juce::File backupFile(const juce::File& file)
{
    return file.getSiblingFile(file.getFileNameWithoutExtension() + ".backup" + file.getFileExtension());
}
// Write/flush/read back before any replacement. All temporary files are siblings,
// so JUCE's platform replacement never crosses volumes.
inline bool stage(const juce::File& file, const juce::MemoryBlock& bytes, juce::String& error)
{
    auto out = file.createOutputStream();
    if (!out) { error = "Could not create temporary project: " + file.getFullPathName(); return false; }
    const auto seek = out->setPosition(0);
    const auto truncate = out->truncate();
    const auto written = seek && truncate.wasOk() && out->write(bytes.getData(), bytes.getSize());
    out->flush();
    const auto status = out->getStatus();
    out.reset();
    if (!written || status.failed()) { error = "Project write failed: " + status.getErrorMessage(); return false; }
    juce::MemoryBlock actual;
    if (!file.loadFileAsData(actual) || actual != bytes)
    { error = "Project verification failed: " + file.getFullPathName(); return false; }
    return true;
}
inline bool save(const juce::File& file, const juce::MemoryBlock& bytes,
                 juce::String& error, bool keepBackup)
{
    error.clear();
    if (!validatedTree(bytes).isValid())
    { error = "Invalid project data; original file was not changed."; return false; }
    juce::InterProcessLock lock("HachiShifter-project-save-" + juce::SHA256(
        file.getFullPathName().toLowerCase().toUTF8()).toHexString());
    if (!lock.enter(0)) { error = "This project is being saved by another process."; return false; }
    struct Unlock { juce::InterProcessLock& lock; ~Unlock(){lock.exit();} } unlock{lock};
    juce::TemporaryFile temporary(file);
    if (!stage(temporary.getFile(), bytes, error)) return false;
    if (keepBackup && file.existsAsFile())
    {
        juce::MemoryBlock previous;
        if (!file.loadFileAsData(previous))
        { error = "Could not read previous project for backup."; return false; }
        // Never rotate a corrupt current file over the last known backup.
        if (!validatedTree(previous).isValid())
        { error = "Existing project is invalid; use Save As to preserve it and its backup."; return false; }
        juce::TemporaryFile backup(backupFile(file));
        if (!stage(backup.getFile(), previous, error)) return false;
        if (!backup.overwriteTargetFileWithTemporary())
        { error = "Could not replace project backup; original project was not changed."; return false; }
    }
    if (!temporary.overwriteTargetFileWithTemporary())
    { error = "Could not replace project: " + file.getFullPathName() + ". Original file and backup were retained."; return false; }
    return true;
}
}