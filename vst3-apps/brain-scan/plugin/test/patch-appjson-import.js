const fs = require("fs");
const p = "c:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/BrokildApps/vst3-apps/brain-scan/app.json";
const a = JSON.parse(fs.readFileSync(p, "utf8"));
const add = "IMPORT reads a real volume off disk and uses it instead of the nine - a NIfTI file, a folder of DICOM slices or a stack of images - resampled in millimetres so an anisotropic scan is not squashed, area-averaged so decimation does not alias, and windowed by percentile so one surgical clip cannot crush the tissue; with a CT loaded, WINDOW and LEVEL are literally a radiographer's window width and level in Hounsfield units. ";
const anchor = "The Brokild World FX rack and five macros.";
if (a.description.indexOf(add) < 0){
  if (a.description.split(anchor).length - 1 !== 1){ console.error("anchor"); process.exit(1); }
  a.description = a.description.split(anchor).join(add + anchor);
}
a.note = "Windows VST3 - build 260904.2 - free download - 9 specimens plus your own volume (NIfTI, DICOM or an image stack) - hints on every control - 20-page manual.";
fs.writeFileSync(p, JSON.stringify(a, null, 2) + "\n");
console.log(a.note);
