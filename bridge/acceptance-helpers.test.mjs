import assert from 'node:assert/strict';
import test from 'node:test';
import { acceptanceIds, acceptanceSource } from './acceptance-helpers.mjs';

test('acceptance ids are safe, bounded, and separate', () => {
  const { appId, shortcutId } = acceptanceIds(1_789_000_000_000);
  assert.match(appId, /^[A-Za-z0-9_.-]{1,24}$/);
  assert.match(shortcutId, /^[A-Za-z0-9_.-]{1,24}$/);
  assert.notEqual(appId, shortcutId);
});

test('acceptance app is a bounded cooperative MicroPython program', () => {
  const source = acceptanceSource();
  assert.ok(Buffer.byteLength(source, 'utf8') <= 32768);
  assert.match(source, /^import cyd/m);
  assert.match(source, /cyd\.run\(\)/);
});
