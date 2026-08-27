/*  brokild_paths.h — where a Brokild plugin keeps the user's patches.
    ==========================================================================

    ONE PLACE, ONE FOLDER PER PLUGIN:

        Documents/Brokild patches/<Plugin Name>/

    This replaces three different schemes that had drifted apart, and one of
    them destroyed real work:

      * six synths shared a SINGLE "User presets" folder beside the installed
        bundles, so Escape Room, Black Rider and Mars Wars patches sat mixed
        together in one list;
      * Clone Wars had a folder of its own next to it, under another name;
      * Full Metal Racket kept its patches INSIDE the .vst3 bundle — which an
        installer deletes and replaces. Two of Peter's kits went that way.

    Documents is the right home for all of it: it survives every install, it
    needs no administrator, it is backed up by whatever backs up Documents,
    and a user can find it without being told where to look.

    MIGRATION IS BY COPY, NEVER BY MOVE. The old folders are left exactly as
    they are. If something is mis-sorted, nothing has been lost — and every
    Brokild preset carries an "app" tag in its own JSON, so sorting the shared
    folder out is a content test rather than a guess about filenames.

    It runs once, marked by a stamp file. Without the stamp, deleting a
    migrated patch would bring it back from the dead on the next launch.
*/
#pragma once

#include <juce_core/juce_core.h>

namespace brokild
{

/*  Windows lies about this: File::hasWriteAccess answers from the read-only
    attribute, so Program Files claims to be writable and then refuses. The
    only honest test is to write something. */
inline bool canWriteInto (const juce::File& dir)
{
    if (! dir.isDirectory()) return false;
    auto probe = dir.getChildFile (".brokild-write-probe");
    if (! probe.replaceWithText ("x")) return false;
    probe.deleteFile();
    return true;
}

/*  A remembered folder is normally the user's own choice and must be obeyed.
    These two are not choices, they are the old defaults leaking forward: a
    folder INSIDE an installed .vst3 bundle, or anywhere under Program Files.
    An installer replaces both, so work saved there does not survive — which
    is precisely how two of Peter's kits were lost. Refuse them and fall back
    to the house folder. */
inline bool isUnsafePatchFolder (const juce::File& f)
{
    const auto path = f.getFullPathName();
    if (path.containsIgnoreCase (".vst3" + juce::String (juce::File::getSeparatorString()))) return true;
    if (path.containsIgnoreCase ("Program Files")) return true;
    return false;
}

/*  The folder the plugin's own bundle sits in — or, for a standalone, the
    folder the executable sits in. Only used to FIND the old patches. */
inline juce::File installedNextTo()
{
    const auto self = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
    for (auto d = self.getParentDirectory();
         d.exists() && d.getParentDirectory() != d;
         d = d.getParentDirectory())
        if (d.getFileName().endsWithIgnoreCase (".vst3"))
            return d.getParentDirectory();
    return self.getParentDirectory();
}

/*  Everywhere a Brokild plugin has ever kept a user patch.

    installedNextTo() alone is not enough, and the first version of this file
    got that wrong: run from the STANDALONE there is no .vst3 in the path, so
    it answered with the build folder, found nothing, and stamped the
    migration done — which would then have skipped it for the plugin too. The
    old patches live beside the installed BUNDLES, so look where bundles are
    installed, not only where this executable happens to sit. */
inline juce::Array<juce::File> legacyPatchFolders (const juce::String& pluginName)
{
    const auto self = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
    const auto docs = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);

    juce::Array<juce::File> roots;
    roots.add (installedNextTo());
    for (const char* var : { "CommonProgramFiles", "CommonProgramFiles(x86)" })
    {
        const auto v = juce::SystemStats::getEnvironmentVariable (var, {});
        if (v.isNotEmpty()) roots.add (juce::File (v).getChildFile ("VST3"));
    }
    roots.add (juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getSiblingFile ("Local").getChildFile ("Programs")
                   .getChildFile ("Common").getChildFile ("VST3"));

    juce::Array<juce::File> out;
    for (const auto& r : roots)
        for (const auto& base : { r, r.getChildFile ("Brokild") })
        {
            out.addIfNotAlreadyThere (base.getChildFile ("User presets"));   // the shared one
            out.addIfNotAlreadyThere (base.getChildFile ("Clone Wars User Patches"));
        }
    //  Full Metal Racket kept them INSIDE its own bundle
    out.addIfNotAlreadyThere (self.getParentDirectory().getChildFile ("User patches"));
    for (const auto& r : roots)
        for (const auto& b : r.findChildFiles (juce::File::findDirectories, false, "*.vst3"))
            out.addIfNotAlreadyThere (b.getChildFile ("Contents").getChildFile ("x86_64-win")
                                       .getChildFile ("User patches"));
    out.addIfNotAlreadyThere (docs.getChildFile (pluginName + " Presets"));
    out.addIfNotAlreadyThere (docs.getChildFile (pluginName));
    return out;
}

/*  THE FOLDER. Creates it, migrates once, and hands it back.

    appTag is the fragment that identifies this plugin's own files inside the
    shared folder — the "app" value every Brokild preset writes, or the format
    marker for the ones that predate it. Pass an empty string to take every
    file matching the wildcard, which is right for a folder that was never
    shared (Clone Wars' slots).
*/
inline juce::File patchFolder (const juce::String& pluginName,
                               const juce::String& appTag,
                               const juce::String& wildcard = "*.json")
{
    const auto root = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                          .getChildFile ("Brokild patches");
    const auto dir = root.getChildFile (pluginName);
    if (! dir.isDirectory() && ! dir.createDirectory().wasOk()) return {};

    const auto stamp = dir.getChildFile (".migrated");
    if (stamp.existsAsFile()) return dir;

    int moved = 0;
    juce::StringArray looked;
    for (const auto& old : legacyPatchFolders (pluginName))
    {
        if (! old.isDirectory() || old == dir) continue;
        looked.add (old.getFullPathName());
        for (const auto& f : old.findChildFiles (juce::File::findFiles, true, wildcard))
        {
            if (f.getFileName().startsWithChar ('.')) continue;
            //  a shared folder holds other plugins' patches; the file says
            //  whose it is, which is far safer than reading its name
            if (appTag.isNotEmpty() && ! f.loadFileAsString().contains (appTag)) continue;
            //  keep any subfolder structure — those are the browser's groups,
            //  and flattening them would quietly collide two same-named files
            const auto rel = f.getRelativePathFrom (old);
            const auto dst = dir.getChildFile (rel);
            if (dst.existsAsFile()) continue;
            dst.getParentDirectory().createDirectory();
            if (f.copyFileTo (dst)) ++moved;
        }
    }
    /*  Name the folders that were actually searched. A migration that copies
        nothing is either "there was nothing to copy" or "it looked in the
        wrong place", and those two look identical from outside — the first
        version of this file was the second one. */
    stamp.replaceWithText ("Brokild patches: migrated " + juce::String (moved)
                           + " file(s). The originals were left where they were;"
                             " delete this file to migrate again.\n\nSearched:\n"
                           + (looked.isEmpty() ? juce::String ("  (no old folder existed)\n")
                                               : "  " + looked.joinIntoString ("\n  ") + "\n"));
    return dir;
}

} // namespace brokild
