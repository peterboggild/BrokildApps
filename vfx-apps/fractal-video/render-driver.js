#!/usr/bin/env node
/* Render driver (stage-3 scaffold).
   Today: loads an exported project, normalises it with the same schema as the app, builds the
   frame plan and runs the shared evaluator over every frame of the range as a sanity check
   (finite values, scene/section coverage). Frame capture + ffmpeg mux are the next step (PLAN.md §4, §8). */
const fs = require('fs');
const path = require('path');
const Schema = require('./js/schema.js');
const Ev = require('./js/evaluator.js');

function help() {
  console.log(`
Fractal Video render driver

Usage:
  node render-driver.js --project <project.json> [options]

Options:
  --fps <n>        override frames per second
  --width <px>     override output width
  --height <px>    override output height
  --samples <n>    override samples per frame
  --start <frame>  first frame (default 0)
  --end <frame>    end frame, exclusive (default: whole song)
  --out <dir>      output folder (default: project render.outDir)
  --dry-run        validate and evaluate every frame, write nothing
  --help           this message

Without audio analysis the song length comes from the sections; audio-driven
routes evaluate to zero here (grid routes are exact).
`);
}

function parseArgs(argv) {
  const o = { dryRun: false };
  for (let i = 0; i < argv.length; i += 1) {
    const a = argv[i];
    const next = () => { i += 1; return argv[i]; };
    if (a === '--project') o.project = next();
    else if (a === '--fps') o.fps = Number(next());
    else if (a === '--width') o.width = Number(next());
    else if (a === '--height') o.height = Number(next());
    else if (a === '--samples') o.samples = Number(next());
    else if (a === '--start') o.startFrame = Number(next());
    else if (a === '--end') o.endFrame = Number(next());
    else if (a === '--out') o.outDir = next();
    else if (a === '--dry-run') o.dryRun = true;
    else if (a === '--help' || a === '-h') { help(); process.exit(0); }
    else { console.error(`Unknown option ${a}`); help(); process.exit(1); }
  }
  return o;
}

function finiteDeep(v) {
  if (typeof v === 'number') return Number.isFinite(v);
  if (Array.isArray(v) || ArrayBuffer.isView(v)) return Array.from(v).every(finiteDeep);
  if (v && typeof v === 'object') return Object.values(v).every(finiteDeep);
  return true;
}

function main() {
  const args = parseArgs(process.argv.slice(2));
  if (!args.project) { help(); process.exit(1); }
  const raw = JSON.parse(fs.readFileSync(path.resolve(args.project), 'utf8'));
  const project = Schema.normalizeProject(raw);
  const overrides = {};
  ['fps', 'width', 'height', 'samples', 'startFrame', 'endFrame', 'outDir'].forEach((k) => { if (args[k] !== undefined && args[k] === args[k]) overrides[k] = args[k]; });
  const plan = Ev.renderPlan(project, null, overrides);

  console.log(`Project   ${project.name}`);
  console.log(`Scenes    ${project.scenes.length}, sections ${project.sections.length}, routes ${project.mod.length}`);
  console.log(`Output    ${plan.width}x${plan.height} @ ${plan.fps} fps, ${plan.samples} samples/frame -> ${plan.outDir}`);
  console.log(`Song      ${plan.duration.toFixed(2)} s = ${plan.frames} frames; range ${plan.start}..${plan.end}`);

  const t0 = Date.now();
  const seen = new Set();
  let bad = 0;
  for (let n = plan.start; n < plan.end; n += 1) {
    const f = Ev.evaluate(project, null, plan.timeOf(n));
    seen.add(f.sceneId);
    if (!finiteDeep(f.gpu) || !finiteDeep(f.post)) {
      bad += 1;
      if (bad <= 5) console.error(`  frame ${n}: non-finite value`);
    }
  }
  console.log(`Evaluated ${plan.end - plan.start} frames in ${Date.now() - t0} ms; scenes used: ${seen.size}; bad frames: ${bad}`);
  if (bad) process.exit(2);
  if (args.dryRun) { console.log('Dry run OK.'); return; }
  console.log('Frame capture and ffmpeg muxing are not implemented yet (next stage). Use --dry-run, or Render still in the app.');
}

main();
