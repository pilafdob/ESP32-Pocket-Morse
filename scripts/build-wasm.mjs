import { mkdirSync, existsSync } from 'node:fs';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { resolve } from 'node:path';
process.chdir(fileURLToPath(new URL('..', import.meta.url)));
const local = resolve('.tools/emsdk/upstream/emscripten/em++');
const compiler = process.env.EMXX || (existsSync(local) ? local : 'em++');
mkdirSync('web/generated', { recursive: true });
mkdirSync('build', { recursive: true });
const cc = compiler.replace(/em\+\+$/, 'emcc');
const cryptoBuild = spawnSync(cc, ['-O2', '-c', 'lib/Monocypher/src/monocypher.c', '-o', 'build/monocypher-wasm.o'], { stdio: 'inherit' });
if (cryptoBuild.status !== 0) process.exit(cryptoBuild.status ?? 1);
const args = ['-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror', '-Ilib/MorseCore/src',
  '-Ilib/Monocypher/src', 'build/monocypher-wasm.o', 'lib/MorseCore/src/SecureChannel.cpp',
  'lib/MorseCore/src/MorseCore.cpp', 'lib/MorseCore/src/InboxJournal.cpp', 'lib/MorseCore/src/SimPair.cpp', 'simulator/bridge.cpp',
  '-sMODULARIZE=1', '-sEXPORT_ES6=1', '-sENVIRONMENT=web,node', '-sALLOW_MEMORY_GROWTH=1',
  '-sEXPORTED_RUNTIME_METHODS=["UTF8ToString"]', '-o', 'web/generated/morse.mjs'];
const result = spawnSync(compiler, args, { stdio: 'inherit' });
if (result.error) console.error('Install Emscripten 4.0.23 or set EMXX to em++:', result.error.message);
process.exit(result.status ?? 1);
