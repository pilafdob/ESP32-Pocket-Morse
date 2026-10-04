import { mkdirSync } from 'node:fs';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
process.chdir(fileURLToPath(new URL('..', import.meta.url)));
mkdirSync('build', { recursive: true });
const cryptoBuild = spawnSync(process.env.CC || 'cc', ['-std=c99', '-g', '-fsanitize=address,undefined',
  '-c', 'lib/Monocypher/src/monocypher.c', '-o', 'build/monocypher-native.o'], { stdio: 'inherit' });
if (cryptoBuild.status !== 0) process.exit(cryptoBuild.status ?? 1);
const result = spawnSync(process.env.CXX || 'c++', ['-std=c++17', '-Wall', '-Wextra', '-Werror',
  '-g', '-fsanitize=address,undefined', '-Ilib/MorseCore/src', '-Itest/stubs',
  '-Ilib/Monocypher/src', 'build/monocypher-native.o', 'lib/MorseCore/src/SecureChannel.cpp',
  'lib/MorseCore/src/MorseCore.cpp', 'lib/MorseCore/src/InboxJournal.cpp', 'lib/MorseCore/src/SimPair.cpp', 'test/core.cpp', 'test/storage.cpp', 'src/PairStore.cpp',
  '-o', 'build/core-tests'], { stdio: 'inherit' });
if (result.status !== 0) process.exit(result.status ?? 1);
process.exit(spawnSync('./build/core-tests', [], { stdio: 'inherit' }).status ?? 1);
