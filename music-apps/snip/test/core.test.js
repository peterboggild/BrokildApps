// node test/core.test.js — checks the pure audio core of index.html and the MP3 encoder.
const fs=require('fs'), path=require('path'), vm=require('vm');
const root=path.join(__dirname,'..');
const html=fs.readFileSync(path.join(root,'index.html'),'utf8');
const script=html.match(/<script>([\s\S]*?)<\/script>/)[1];
new vm.Script(script); // whole app parses
const core=script.split('// ===== CORE-BEGIN =====')[1].split('// ===== CORE-END =====')[0];
const ctx={}; vm.createContext(ctx);
vm.runInContext(core+';this.parseWav=parseWav;this.writeWav=writeWav;this.resample=resample;this.sniffRate=sniffRate;',ctx);
const {parseWav,writeWav,resample}=ctx;
let fails=0; const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails++; };

// round-trips
const n=4801, L=new Float32Array(n), R=new Float32Array(n);
for(let i=0;i<n;i++){ L[i]=0.8*Math.sin(i*0.01); R[i]=-0.5*Math.cos(i*0.037); }
const tol={'16':1/32768,'24':1/8388608,'32':1e-7,'32f':0};
for(const k of ['16','24','32','32f']){
  for(const ch of [[L,R],[L]]){
    const buf=writeWav(ch,44100,k,false), r=parseWav(buf);
    let e=0; ch.forEach((x,c)=>{ for(let i=0;i<n;i++) e=Math.max(e,Math.abs(x[i]-r.ch[c][i])); });
    ok(r.sr===44100&&r.ch.length===ch.length&&r.ch[0].length===n&&r.bits===k&&e<=tol[k]*0.51+1e-9, `WAV ${k} ${ch.length}ch round-trip (err ${e.toExponential(2)}, bytes ${buf.byteLength})`);
  }
}
// 16-bit lossless re-save
{ const a=writeWav([L],48000,'16',false), b=writeWav(parseWav(a).ch,48000,'16',false);
  ok(Buffer.compare(Buffer.from(a),Buffer.from(b))===0,'16-bit open→save is bit-identical'); }

// resampler: 1 kHz tone 48k→44.1k amplitude & alias test
function tone(f,sr,n,a=0.5){ const x=new Float32Array(n); for(let i=0;i<n;i++) x[i]=a*Math.sin(2*Math.PI*f*i/sr); return x; }
function amp(x,f,sr){ let s=0,c=0; const a=2000,b=x.length-2000; for(let i=a;i<b;i++){ s+=x[i]*Math.sin(2*Math.PI*f*i/sr); c+=x[i]*Math.cos(2*Math.PI*f*i/sr);} return 2*Math.hypot(s,c)/(b-a); }
const t0=Date.now();
const y=resample(tone(1000,48000,48000),48000,44100);
ok(Math.abs(y.length-44100)<=1,'48k→44.1k length');
ok(Math.abs(amp(y,1000,44100)-0.5)<0.001,`1 kHz amplitude preserved (${amp(y,1000,44100).toFixed(5)})`);
const z=resample(tone(23000,48000,48000),48000,44100); // above new Nyquist → must vanish
const alias=Math.max(...[21100].map(f=>amp(z,f,44100)));
ok(alias<0.5e-4,`23 kHz rejected: alias at 21.1 kHz = ${(20*Math.log10(alias/0.5)).toFixed(1)} dB`);
const u=resample(tone(15000,44100,44100),44100,96000);
ok(Math.abs(amp(u,15000,96000)-0.5)<0.002,`upsample 15 kHz amplitude ${amp(u,15000,96000).toFixed(4)}`);
console.log(`     resample speed: ${(3*45000/((Date.now()-t0)/1000)/1e6).toFixed(2)} M samples/s`);

// MP3 encoder (third-party code: only with --mp3)
if(process.argv.includes('--mp3')){
const lctx={}; vm.createContext(lctx);
vm.runInContext(fs.readFileSync(path.join(root,'vendor','lame.min.js'),'utf8')+';this.lamejs=lamejs;',lctx);
const enc=new lctx.lamejs.Mp3Encoder(2,44100,192), l16=new Int16Array(44100), r16=new Int16Array(44100);
for(let i=0;i<44100;i++){ l16[i]=10000*Math.sin(i*0.06); r16[i]=l16[i]; }
let bytes=0; for(let a=0;a<44100;a+=1152*8){ bytes+=enc.encodeBuffer(l16.subarray(a,a+1152*8),r16.subarray(a,a+1152*8)).length; } bytes+=enc.flush().length;
ok(bytes>20000&&bytes<30000,`MP3 1 s @192 kbps = ${bytes} bytes`);
}
const mp3=new Uint8Array(1000); mp3.set([0xFF,0xFB,0x90,0x00]); ok(ctx.sniffRate(mp3).sr===44100,'MP3 header sniff 44.1k');

console.log(fails?`\n${fails} FAILED`:'\nall passed'); process.exit(fails?1:0);
