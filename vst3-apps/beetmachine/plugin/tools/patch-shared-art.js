// Beetmachine onto the drum family's shared art (vst3-apps/machine-art).
// The drums' panels are compiled into Beetmachine and draw with the shared
// parts, so Beetmachine uses the same set instead of a private copy.
// Every anchor is checked before anything is written.
const fs = require("fs");
const path = require("path");
const root = path.join(__dirname, "..");
const miss = [], writers = [];
function patch(rel, pairs) {
  const f = path.join(root, rel);
  let s = fs.readFileSync(f, "utf8");
  const crlf = (s.match(/\r\n/g) || []).length > (s.match(/\n/g) || []).length / 2;
  s = s.replace(/\r\n/g, "\n");
  for (const [a, b] of pairs) {
    const n = s.split(a).length - 1;
    if (n !== 1) { miss.push(rel + ": " + n + "x " + a.slice(0, 60).replace(/\n/g, "\\n")); continue; }
    s = s.replace(a, () => b);
  }
  writers.push(() => fs.writeFileSync(f, crlf ? s.replace(/\n/g, "\r\n") : s, "utf8"));
}

patch("CMakeLists.txt", [
  ['set(BEET_BUILD_ID "260926.2")', 'set(BEET_BUILD_ID "260926.3")'],
  ['file(GLOB BEET_DECALS "${CMAKE_CURRENT_SOURCE_DIR}/art/*.png" "${CMAKE_CURRENT_SOURCE_DIR}/art/*.jpg")\n' +
   'list(LENGTH BEET_DECALS BEET_DECAL_COUNT)\n' +
   'message(STATUS "Beetmachine: ${BEET_DECAL_COUNT} decal file(s) embedded (re-run configure after adding art)")\n' +
   'if(BEET_DECAL_COUNT GREATER 0)\n' +
   '    juce_add_binary_data(BeetmachineArt HEADER_NAME BeetArt.h NAMESPACE BeetArt SOURCES ${BEET_DECALS})\n' +
   '    target_link_libraries(Beetmachine PRIVATE BeetmachineArt)\n' +
   '    target_compile_definitions(Beetmachine PUBLIC BEET_HAS_ART=1)\n' +
   'endif()\n',
   '# The drum family\'s shared machine-style parts (vst3-apps/machine-art). The\n' +
   '# drums\' panels are compiled in here and draw with them too: ONE set.\n' +
   'include("${CMAKE_CURRENT_SOURCE_DIR}/../../machine-art/machine-art.cmake")\n' +
   'brokild_add_machine_art(Beetmachine)\n'],
]);

patch("src/BeetLook.cpp", [
  ['#include "BeetLook.h"\n#if BEET_HAS_ART\n #include "BeetArt.h"\n#endif\n',
   '#include "BeetLook.h"\n#include "MachineArt.h"\n'],
  ['juce::Image BeetLook::art (const char* file)\n{\n   #if BEET_HAS_ART\n' +
   '    for (int i = 0; i < BeetArt::namedResourceListSize; ++i)\n' +
   '        if (juce::String (BeetArt::originalFilenames[i]) == file)\n' +
   '        {\n' +
   '            int size = 0;\n' +
   '            if (const char* data = BeetArt::getNamedResource (BeetArt::namedResourceList[i], size))\n' +
   '                return juce::ImageCache::getFromMemory (data, size);\n' +
   '        }\n' +
   '   #endif\n' +
   '    juce::ignoreUnused (file);\n' +
   '    return {};\n' +
   '}\n',
   '//  one parts set for the whole drum family (vst3-apps/machine-art); two of\n' +
   '//  Beetmachine\'s own names map onto the shared files\n' +
   'juce::Image BeetLook::art (const char* file)\n{\n' +
   '    const juce::String f (file);\n' +
   '    if (f == "ground.jpg") return machineart::image ("ground-beet.jpg");\n' +
   '    if (f == "plate.png")  return machineart::image ("plate-blank.png");\n' +
   '    return machineart::image (file);\n' +
   '}\n'],
]);

if (miss.length) { console.log("ABORTED, nothing written:\n  " + miss.join("\n  ")); process.exit(1); }
writers.forEach(w => w());
console.log("Beetmachine now uses the shared machine art");
