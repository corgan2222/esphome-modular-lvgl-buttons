// tests/test_convert_counts.js
// Asserts the vendored splash_animations.h matches the claudepix index.
const fs = require('fs');

const HEADER = process.argv[2] ||
  'esphome-modular-lvgl-buttons/ui/clawdmeter/splash_animations.h';
const INDEX = process.argv[3] ||
  'D:/Coding/git-corgan/Clawdmeter/tools/claudepix_data/_index.json';

const idx = JSON.parse(fs.readFileSync(INDEX, 'utf8'));
const src = fs.readFileSync(HEADER, 'utf8');

let fail = 0;
function check(cond, msg) { if (!cond) { console.error('FAIL: ' + msg); fail++; } }

const m = src.match(/#define\s+SPLASH_ANIM_COUNT\s+(\d+)/);
check(m, 'SPLASH_ANIM_COUNT define present');
if (m) check(Number(m[1]) === idx.length,
  `SPLASH_ANIM_COUNT ${m && m[1]} === index length ${idx.length}`);

check(/#define\s+SPLASH_PALETTE_SIZE\s+10/.test(src), 'SPLASH_PALETTE_SIZE == 10');

for (const a of idx) {
  const ident = a.name.replace(/[^a-z0-9]+/gi, '_').toLowerCase();
  const re = new RegExp(`splash_${ident}_frames\\[${a.frame_count}\\]\\[400\\]`);
  check(re.test(src), `frames array for "${a.name}" has ${a.frame_count}x400`);
  const rh = new RegExp(`splash_${ident}_holds\\[${a.frame_count}\\]`);
  check(rh.test(src), `holds array for "${a.name}" has ${a.frame_count}`);
}

if (fail) { console.error(`${fail} check(s) failed`); process.exit(1); }
console.log('OK: header matches index (' + idx.length + ' animations)');
