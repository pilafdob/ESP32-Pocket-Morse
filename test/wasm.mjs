import createMorse from '../web/generated/morse.mjs';
import { runScenarios } from '../web/scenarios.js';
const wasm = await createMorse();
const results = runScenarios(wasm);
for (const r of results) console.log(`${r.passed ? 'PASS' : 'FAIL'} ${r.name}${r.error ? ': ' + r.error : ''}`);
if (results.some(r => !r.passed)) process.exit(1);
console.log(`${results.length} WebAssembly scenarios passed.`);
