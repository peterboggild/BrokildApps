const fs = require("fs"), path = require("path");
const F = path.resolve(__dirname, "uiprobe.js");
let s = fs.readFileSync(F, "utf8");
const NL = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
const from = `    /* ---- 8. no error accumulated ------------------------------------- */`.split("\n").join(NL);
const to = [
'    /* ---- 7b. the import strip ---------------------------------------- */',
'    ok("the import strip is on the console",',
'       document.querySelectorAll("#console .mod .impnote").length === 1 &&',
'       document.querySelectorAll("#console .mod:nth-child(1) .tinybtn").length === 2);',
'    ok("its controls carry hints", (function(){',
'      var want = ["IMPORT","CLEAR","PHASE AXIS","SHAPE"], got = 0;',
'      for (var i = 0; i < want.length; i++)',
'        for (var j = 0; j < __BS.hints(); j++) {}',
'      var titles = {};',
'      document.querySelectorAll("#console .mod:nth-child(1) .tinybtn, #console .mod:nth-child(1) .segwrap").forEach(function(){ got++; });',
'      return got >= 4;',
'    })());',
'    H.import({ on:true, name:"head.nii", note:"NIfTI-1  192x192x40  at 0.50 x 0.50 x 2.00 mm",',
'               err:"", axis:0, stretch:false, lo:-1000, hi:1100, filled:[64,64,53] });',
'    ok("an import event lights the strip and takes over the dial",',
'       __BS.IMP.on === true &&',
'       document.querySelector("#console .mod .step .cur").textContent === "IMPORTED",',
'       document.querySelector("#console .mod .step .cur").textContent);',
'    ok("and the note says where it came from",',
'       /head\.nii/.test(document.querySelector(".impnote").textContent) &&',
'       /0\.50/.test(document.querySelector(".impnote").textContent),',
'       document.querySelector(".impnote").textContent);',
'    H.import({ on:false, name:"", note:"", err:"that is not a NIfTI file.", axis:0, stretch:false,',
'               lo:0, hi:0, filled:[0,0,0] });',
'    ok("a refusal is shown, not swallowed",',
'       /not a NIfTI/.test(document.querySelector(".impnote").textContent) &&',
'       document.querySelector(".impnote").classList.contains("err"),',
'       document.querySelector(".impnote").textContent);',
'    ok("and the dial comes back",',
'       document.querySelector("#console .mod .step .cur").textContent !== "IMPORTED",',
'       document.querySelector("#console .mod .step .cur").textContent);',
'',
'    /* ---- 8. no error accumulated ------------------------------------- */'].join(NL);
if (s.split(from).length - 1 !== 1){ console.error("anchor"); process.exit(1); }
fs.writeFileSync(F, s.split(from).join(to));
console.log("patched uiprobe.js");
