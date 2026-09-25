/*  Writes test/live-v5-jobs.json: the live check of light sync and wall pictures
    against the real standalone. The picture is a real JPEG, embedded as base64
    and handed to the page's own file input, so it goes through the page's path.
      node test/make-live-v5-jobs.js <image.jpg> <shot folder> */
"use strict";
const fs = require("fs"), path = require("path");
const img = fs.readFileSync(process.argv[2]).toString("base64");
const SH = (process.argv[3] || ".").split(path.sep).join("/");
const sleep = "var sleep=function(ms){return new Promise(function(r){setTimeout(r,ms);});};";
const load = "(async function(){ " + sleep + " var T=window.__TW; var b64='" + img + "'; var bin=atob(b64); var u=new Uint8Array(bin.length); for(var i=0;i<bin.length;i++) u[i]=bin.charCodeAt(i);" +
  " var f=new File([u],'cover.jpg',{type:'image/jpeg'}); var dt=new DataTransfer(); dt.items.add(f); var inp=document.getElementById('picFile'); inp.files=dt.files; inp.dispatchEvent(new Event('change',{bubbles:true}));" +
  " var t0=performance.now(); while(!T.picArmed && performance.now()-t0<5000) await sleep(50); if(!T.picArmed) return 'not armed';" +
  " var plan=document.getElementById('plan'), PF=T.planFit(), r=plan.getBoundingClientRect(); var AL=ALONG;" +
  " var cx=r.left+PF.ox+0.12*PF.s, cy=r.top+PF.oy+(9-AL)*PF.s;" +
  " plan.dispatchEvent(new PointerEvent('pointerdown',{bubbles:true,clientX:cx,clientY:cy,button:0,pointerId:3})); window.dispatchEvent(new PointerEvent('pointerup',{bubbles:true,clientX:cx,clientY:cy,pointerId:3}));" +
  " var k=document.getElementById('picKind'); k.value='1'; k.dispatchEvent(new Event('change',{bubbles:true}));" +
  " document.querySelector('[data-picsize=\"1.5\"]').click();" +
  " var fr=document.getElementById('picFrame'); fr.value='FRAME'; fr.dispatchEvent(new Event('change',{bubbles:true}));" +
  " return JSON.stringify(T.pics()); })()";
const rt = "(async function(){ " + sleep + " await sleep(1500); var s=window.__TW.scene(); return JSON.stringify(s && s.rt ? s.rt[0] : null); })()";
const J = [
  { name:"settle", wait:3500 },
  { name:"test signal, tiled living room, listener facing the west wall", eval:"(async function(){ " + sleep + " var b=window.__JUCE__.backend, T=window.__TW; b.emitEvent('tw',{k:'wavPath',path:'C:/Users/peter/b/ThinWalls/test/testsignal.wav'}); b.emitEvent('tw',{k:'wavPlay',v:1}); var P={lisx:4.2/18, lisy:2.5/9, lisyaw:0.5, mat1:3/4}; for (var k in P){ b.emitEvent('tw',{k:'p',id:k,v:P[k]}); T.setParam(k,P[k]); } T.setSync({s:0}); T.setCine(false); await sleep(1500); var s=T.scene(); return 'RT60 LARGE before ' + JSON.stringify(s && s.rt ? s.rt[0] : null); })()" },
  { name:"load the cover, hang it as an XL ACOUSTIC PANEL on the LARGE west wall (oak)", eval:load.replace("ALONG", "1.4").replace("FRAME", "1") },
  { name:"and a second one (gilt)", eval:load.replace("ALONG", "3.7").replace("FRAME", "2") },
  { name:"RT60 LARGE after two panels", eval:rt },
  { name:"wait", wait:600 },
  { name:"OFF view with the panels", shoot:SH + "/V-pics-off.png" },
  { name:"cinematic on, held realtime", eval:"(async function(){ " + sleep + " var T=window.__TW; T.cineHold(true); T.setCine(true,'high'); var t0=performance.now(); while(T.cine().ok!==true && performance.now()-t0<60000) await sleep(100); await sleep(3500); return JSON.stringify(T.cine()); })()" },
  { name:"cinematic realtime with the panels", shoot:SH + "/V-pics-cine.png" },
  { name:"photo", eval:"(async function(){ " + sleep + " var T=window.__TW; T.cineHold(false); var t0=performance.now(); while(!T.cine().converged && performance.now()-t0<30000) await sleep(200); return JSON.stringify(T.cine()); })()" },
  { name:"photo with the panels", shoot:SH + "/V-pics-photo.png" },
  { name:"light sync 0.6 FOLLOW, cinematic off; frame rate with sync 0 vs 0.6, sound playing", eval:"(async function(){ " + sleep + " var T=window.__TW; T.setCine(false); T.setSync({s:0}); await sleep(500); var d0=T.draws; await sleep(2000); var r0=(T.draws-d0)/2; T.setSync({s:0.6, mode:'follow'}); await sleep(500); var d1=T.draws; await sleep(2000); var r1=(T.draws-d1)/2; var b0=T.cineBench('off',30); return JSON.stringify({drawsPerS_sync0:r0, drawsPerS_sync06:r1, msPerFrame:b0.ms, sync:T.sync()}); })()" },
  { name:"wait for a loud moment in the LARGE room", eval:"(async function(){ " + sleep + " var T=window.__TW; var lo=9, hi=-9, t0=performance.now(); while(performance.now()-t0<3000){ var v=T.sync().k[0]; lo=Math.min(lo,v); hi=Math.max(hi,v); await sleep(5); } window.__kr=[lo,hi]; t0=performance.now(); while(T.sync().k[0] < hi-0.03*(hi-lo) && performance.now()-t0<10000) await sleep(2); return 'range ' + lo.toFixed(2) + '..' + hi.toFixed(2) + ', shot at ' + T.sync().k[0].toFixed(2); })()" },
  { name:"lamps up", shoot:SH + "/V-sync-loud.png" },
  { name:"wait for a quiet moment", eval:"(async function(){ " + sleep + " var T=window.__TW; var lo=window.__kr[0], hi=window.__kr[1], t0=performance.now(); while(T.sync().k[0] > lo+0.05*(hi-lo) && performance.now()-t0<10000) await sleep(2); return 'shot at ' + T.sync().k[0].toFixed(2); })()" },
  { name:"lamps down", shoot:SH + "/V-sync-quiet.png" },
  { name:"record a short take with sync on", eval:"(async function(){ " + sleep + " var T=window.__TW; document.getElementById('bRec').click(); var t0=performance.now(); while(T.rec().state!=='recording' && performance.now()-t0<3000) await sleep(50); await sleep(4000); document.getElementById('bRec').click(); await sleep(700); return JSON.stringify(T.rec()); })()" },
  { name:"export it (AS SHOWN, 1280x720, 30 fps)", eval:"(async function(){ " + sleep + " var T=window.__TW; T.expOpt({res:0, fps:0, pic:'shown', inset:false}); document.getElementById('bExpOpen').click(); document.getElementById('bExport').click(); var t0=performance.now(); while(performance.now()-t0<300000){ var s=T.rec().state; if((s==='done'||s==='error') && !T.exporting) break; await sleep(250); } return JSON.stringify({rec:T.rec(), errs:T.errors.slice(0,4)}); })()" }
];
fs.writeFileSync(path.join(__dirname, "live-v5-jobs.json"), JSON.stringify(J, null, 1));
console.log("wrote test/live-v5-jobs.json (" + Math.round(img.length / 1024) + " KB of image)");
