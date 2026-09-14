// Holds bridge/manifest.mjs to the same cases as the device's _manifest.py
// (firmware/cyd_desktop_board/tests/manifest_test.py runs them in Python).

import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { API_LEVEL, ManifestError, parseManifest, validateManifest } from './manifest.mjs';

const spec = JSON.parse(readFileSync(fileURLToPath(
  new URL('../firmware/cyd_desktop_board/tests/manifest-cases.json', import.meta.url)), 'utf8'));

test('the API level comes from the firmware and matches the shared cases', () => {
  assert.equal(API_LEVEL, spec.api_level);
});

for (const item of spec.cases) {
  test(`shared case: ${item.name}`, () => {
    if (item.ok) {
      assert.deepEqual(validateManifest(item.app_id, item.manifest), { title: item.title, entry: item.entry });
    } else {
      assert.throws(() => validateManifest(item.app_id, item.manifest),
        (error) => error instanceof ManifestError && error.field === item.field);
    }
  });
}

test('parse errors name a field too', () => {
  assert.throws(() => parseManifest('x', '{ not json'), (error) => error.field === 'json');
  assert.throws(() => parseManifest('x', JSON.stringify({ title: 'X', version: '1'.repeat(3000) })),
    (error) => error.field === 'shape');
});
