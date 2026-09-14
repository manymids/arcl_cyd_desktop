// bridge/ once held 30 one-off scripts that nothing ran: stale tap
// coordinates, a hard-coded COM6, and one that printed /sd/wifi.json to the
// console. This keeps new ones from piling up unnoticed. No device needed.

import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readdirSync, readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const BRIDGE_DIR = dirname(fileURLToPath(import.meta.url));
const ROOT = dirname(BRIDGE_DIR);
const scripts = readdirSync(BRIDGE_DIR).filter((name) => name.endsWith('.mjs'));
const source = (name) => readFileSync(join(BRIDGE_DIR, name), 'utf8');

test('every bridge script is run by npm or imported by another module', () => {
  const packageScripts = Object.values(JSON.parse(readFileSync(join(ROOT, 'package.json'), 'utf8')).scripts).join('\n');
  const imports = scripts.map(source).join('\n');
  const orphans = scripts
    .filter((name) => !name.endsWith('.test.mjs'))
    .filter((name) => !packageScripts.includes(`bridge/${name}`))
    .filter((name) => !imports.includes(`'./${name}'`));
  assert.deepEqual(orphans, [], `unused scripts: ${orphans.join(', ')}`);
});

test('no bridge script hard-codes a serial port', () => {
  // DesktopClient reads CYD_DESKTOP_PORT or finds the CH340 itself.
  const offenders = scripts
    .filter((name) => !name.endsWith('.test.mjs'))
    .filter((name) => /path:\s*['"](COM\d+|\/dev\/tty)|\?\?\s*['"]COM\d+/.test(source(name)));
  assert.deepEqual(offenders, []);
});
