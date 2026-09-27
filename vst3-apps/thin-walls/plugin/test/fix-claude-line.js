// The FLEET-WIDE line in CLAUDE.md was inserted through a shell that executed its
// backticks; rewrite that one line from here, where nothing touches the text.
const fs = require("fs");
const p = "C:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/CLAUDE.md";
let s = fs.readFileSync(p, "utf8");
const NL = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
const lines = s.split(/\r?\n/);
const i = lines.findIndex(l => l.startsWith("- **FLEET-WIDE, found because Peter ran the downloaded exe on his WORK PC"));
if (i < 0) { console.error("line not found"); process.exit(1); }
const BT = String.fromCharCode(96);
const code = t => BT + t + BT;
lines[i] = "- **FLEET-WIDE, found because Peter ran the downloaded exe on his WORK PC and got Internet Explorer's \"navigation cancelled\" page**: every Brokild WebView plugin loads " + code("WebView2Loader.dll") +
  " DYNAMICALLY (the JUCE default), and on the home PC it is found only because the Windows Performance Toolkit is on the PATH (" + code("C:\\Program Files (x86)\\Windows Kits\\10\\Windows Performance Toolkit\\WebView2Loader.dll") +
  "). JUCE's CMake already links " + code("WebView2LoaderStatic.lib") + " under " + code("NEEDS_WEBVIEW2 TRUE") + "; adding " + code("JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1") +
  " to the target's PUBLIC compile definitions makes the plugin independent of it. Thin Walls has it; the rest of the fleet does not yet. Thin Walls also checks " + code("GetAvailableCoreWebView2BrowserVersionString") +
  " at editor start and paints a message with the runtime download link instead of the IE page, and its standalone uses a per-launch WebView2 profile in %TEMP% (a stale msedgewebview2 from a CDP run holding the shared profile is the other way to get that page). Not yet confirmed on the work PC; " + code("HANDOVER.md") + " in the tree lists what is left.";
fs.writeFileSync(p, lines.join(NL));
console.log("line " + (i + 1) + " rewritten, " + lines[i].length + " chars");
