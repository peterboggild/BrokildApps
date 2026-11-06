/* Inlines src/*.js into the single published page between markers, so
 * the source can live in files while the artifact stays one file that
 * the browser never has to fetch a second time. */
const fs=require('fs');
const page=process.argv[2];
if(!page){console.error('usage: node inline.js <page.html>');process.exit(1);}
let html=fs.readFileSync(page,'utf8');
for(const mod of ['planetgen']){
  const src=fs.readFileSync(__dirname+'/'+mod+'.js','utf8');
  const a='/* ===== '+mod.toUpperCase()+' (inlined from src/'+mod+'.js) ===== */';
  const b='/* ===== END '+mod.toUpperCase()+' ===== */';
  const i=html.indexOf(a), j=html.indexOf(b);
  if(i<0||j<0){console.error('markers for '+mod+' not found in '+page);process.exit(1);}
  html=html.slice(0,i+a.length)+'\n'+src+'\n'+html.slice(j);
  console.log(mod+': '+src.split('\n').length+' lines inlined');
}
fs.writeFileSync(page,html);
