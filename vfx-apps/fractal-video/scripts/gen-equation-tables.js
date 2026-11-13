const F = require('../js/formulas.js');
const fs = require('fs');
const esc = (s) => String(s).replace(/&/g,'\\&').replace(/%/g,'\\%').replace(/_/g,'\\_').replace(/#/g,'\\#')
  .replace(/°/g,'$^\\circ$').replace(/²/g,'$^2$').replace(/'/g,"'");
const num = (x) => { const r = Math.round(x*1e4)/1e4; return String(r).replace('-', '$-$'); };
for (const f of F.FORMULAS) {
  let t = '';
  t += `\\begin{figure}[H]\\centering\\includegraphics[width=0.88\\textwidth]{img/${f.id}.jpg}\n`;
  t += `\\caption{${esc(f.name)}, default parameters (${f.iter.def} iterations).}\\end{figure}\n`;
  t += `\\paragraph{Parameters (app defaults).} `;
  t += `Iterations ${f.iter.def} (range ${f.iter.min}--${f.iter.max}); step factor ${num(f.step)}; bounding size ${num(f.size)}${f.unbounded ? '; \\emph{unbounded} (no bounding sphere assumed)' : ''}.\n\n`;
  t += `{\\footnotesize\\begin{tabular}{@{}llrr@{}}\\toprule Parameter & Key & Default & Range\\\\\\midrule\n`;
  f.params.forEach((p, i) => {
    t += `${esc(p.label)} & \\texttt{${esc(p.key)}} & ${num(p.def)}${p.unit ? esc(p.unit) : ''} & ${num(p.min)}\\,\\dots\\,${num(p.max)}\\\\\n`;
  });
  t += `\\bottomrule\\end{tabular}}\n`;
  fs.writeFileSync(`docs/equations/gen/${f.id}.tex`, t);
}
console.log(F.FORMULAS.length);
