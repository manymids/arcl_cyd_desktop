// Keeps docs/cyd-api_ja.md honest.
//
// The structure review found 10 of the 37 public cyd functions documented
// nowhere, and limits such as "4 buttons" written as bare literals in the
// firmware. This fails when a public function or constant is added without
// documentation, or when a documented limit no longer matches the source.

import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';

const read = (path) => readFileSync(fileURLToPath(new URL(`../${path}`, import.meta.url)), 'utf8');

const reference = read('docs/cyd-api_ja.md');
const cyd = read('firmware/cyd_desktop_board/modules/cyd.py');
const appView = read('firmware/cyd_desktop_shell/screen_app.cpp');
const jpeg = read('firmware/cyd_desktop_shell/screen_jpeg.cpp');

test('every public cyd function is documented', () => {
  const functions = [...cyd.matchAll(/^def ([a-z][a-z0-9_]*)\(/gm)].map((match) => match[1]);
  assert.ok(functions.length >= 37, `expected the full API, found ${functions.length}`);
  const missing = functions.filter((name) => !reference.includes(`cyd.${name}(`));
  assert.deepEqual(missing, [], `docs/cyd-api_ja.md does not document: ${missing.join(', ')}`);
});

test('every public cyd constant is documented', () => {
  const constants = [...cyd.matchAll(/^([A-Z][A-Z0-9_]*) = /gm)].map((match) => match[1]);
  if (/^from _manifest import .*\bAPI_LEVEL\b/m.test(cyd)) constants.push('API_LEVEL');
  const missing = constants.filter((name) => !reference.includes(`cyd.${name}`));
  assert.deepEqual(missing, [], `docs/cyd-api_ja.md does not document: ${missing.join(', ')}`);
});

test('documented capacity limits match the firmware constants', () => {
  const constant = (name) => {
    const match = new RegExp(`constexpr\\s+\\w+\\s+${name}\\s*=\\s*(\\d+)`).exec(appView);
    assert.ok(match, `${name} not found in screen_app.cpp`);
    return Number(match[1]);
  };
  for (const name of ['kAppTextCapacity', 'kAppButtonCapacity', 'kAppPrimitiveCapacity',
    'kGameSpriteCapacity', 'kConsoleMaxLines']) {
    const row = reference.split('\n').find((line) => line.includes(`\`${name}\``));
    assert.ok(row, `docs/cyd-api_ja.md has no row for ${name}`);
    const documented = Number(/\|\s*(\d+)\s*[件行]\s*\|/.exec(row)?.[1]);
    assert.equal(documented, constant(name), `${name}: documented ${documented}, firmware ${constant(name)}`);
  }
});

test('the documented HTTP body limit matches cyd.py', () => {
  const value = Number(/^HTTP_MAX_BYTES = (\d+)/m.exec(cyd)[1]);
  const documented = Number(/`cyd\.HTTP_MAX_BYTES`[^\n]*（([\d,]+)）/.exec(reference)[1].replaceAll(',', ''));
  assert.equal(documented, value);
});

test('the JPEG result codes in the reference match the firmware comment', () => {
  const firmwareCodes = [...jpeg.matchAll(/^\s*\/\/\s+(-\d+)\s/gm)].map((match) => Number(match[1]));
  for (const code of [-1, -4, -5, -6]) {
    assert.ok(firmwareCodes.includes(code) || jpeg.includes(`${code} `), `firmware does not list ${code}`);
    assert.match(reference, new RegExp(`\\|\\s*${code}\\s*\\|`), `docs/cyd-api_ja.md does not list ${code}`);
  }
});
