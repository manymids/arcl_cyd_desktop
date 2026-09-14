// Structural rules for the firmware that a compiler cannot check.
//
// The structure review found "neon3d" and "mjpplayer" written into the shell in
// 12 places, which is what made adding a native app a multi-file edit. Native
// apps are now listed only in native_registry.cpp and described by their own
// NativeApp fields. This test keeps the shell from growing new special cases.

import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readdirSync, readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { join } from 'node:path';

const SHELL = fileURLToPath(new URL('../firmware/cyd_desktop_shell', import.meta.url));

function sources() {
  const files = [];
  for (const name of readdirSync(SHELL)) {
    if (/\.(cpp|h)$/.test(name)) files.push(join(SHELL, name));
  }
  for (const dir of ['include', join('include', 'screen')]) {
    for (const name of readdirSync(join(SHELL, dir))) {
      if (/\.h$/.test(name)) files.push(join(SHELL, dir, name));
    }
  }
  return files.map((path) => ({ path, text: readFileSync(path, 'utf8') }));
}

// Strip comments so prose that mentions an app does not count.
const code = (text) => text.replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/[^\n]*/g, '');

test('native app ids appear only in the file that defines the app', () => {
  const files = sources();
  const definitions = new Map();
  for (const { path, text } of files) {
    for (const match of code(text).matchAll(/const NativeApp app\{\s*"([^"]+)"/g)) {
      definitions.set(match[1], path);
    }
  }
  assert.ok(definitions.size >= 2, 'no NativeApp definitions found');

  const offenders = [];
  for (const [id, home] of definitions) {
    for (const { path, text } of files) {
      if (path !== home && code(text).includes(`"${id}"`)) offenders.push(`${id} in ${path}`);
    }
  }
  assert.deepEqual(offenders, [],
    `native app ids must not be special-cased outside their definition:\n  ${offenders.join('\n  ')}`);
});

test('every NativeApp definition is listed in the registry', () => {
  const registry = readFileSync(join(SHELL, 'native_registry.cpp'), 'utf8');
  const accessors = [];
  for (const { text } of sources()) {
    for (const match of text.matchAll(/^const NativeApp &(\w+)\(\)\s*\{/gm)) accessors.push(match[1]);
  }
  const unlisted = accessors.filter((name) => !registry.includes(`&${name}()`));
  assert.deepEqual(unlisted, [], `defined but not registered: ${unlisted.join(', ')}`);
});

test('every native source is compiled into the firmware', () => {
  // bt_keyboard.cpp needs the bt component, so an IDF component builds it.
  const cmake = readFileSync(join(SHELL, 'micropython.cmake'), 'utf8') +
    readFileSync(join(SHELL, '../cyd_desktop_board/components/cyd_bt_keyboard/CMakeLists.txt'), 'utf8');
  const missing = readdirSync(SHELL)
    .filter((name) => name.endsWith('.cpp'))
    .filter((name) => !cmake.includes(`/${name}`));
  assert.deepEqual(missing, [], `not listed in micropython.cmake: ${missing.join(', ')}`);
});
