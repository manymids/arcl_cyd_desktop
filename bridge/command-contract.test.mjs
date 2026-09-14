// Static contract between the bridge scripts and the firmware's command table.
//
// bridge/ holds ~57 scripts and npm test ran two of them. Nothing compared what
// they send against what protocol.cpp dispatches, so check-events.mjs asked for
// desktop_event_count and debug-jpeg.mjs asked for desktop_eval - commands the
// firmware has never implemented - and both sat there failing unnoticed
// (issues D6 and D7). This test needs no device.

import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync, readdirSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';

const BRIDGE_DIR = dirname(fileURLToPath(import.meta.url));
const ROOT = dirname(BRIDGE_DIR);
const PROTOCOL = join(ROOT, 'firmware', 'cyd_desktop_shell', 'protocol.cpp');

/** Commands protocol.cpp dispatches, aliases included. */
function firmwareCommands() {
  const source = readFileSync(PROTOCOL, 'utf8');
  const found = source.matchAll(/std::strcmp\(command,\s*"([a-z_0-9]+)"\)/g);
  const commands = new Set([...found].map((match) => match[1]));
  assert.ok(commands.size > 0, 'no command dispatch found in protocol.cpp');
  return commands;
}

/** MCP tool names, which are composed on this side rather than sent as-is. */
function bridgeToolNames() {
  const source = readFileSync(join(BRIDGE_DIR, 'index.mjs'), 'utf8');
  return new Set([...source.matchAll(/server\.tool\('([a-z_0-9]+)'/g)].map((m) => m[1]));
}

/** Wire commands each script sends, keyed by file name. */
function sentCommands() {
  const perFile = new Map();
  for (const name of readdirSync(BRIDGE_DIR)) {
    if (!name.endsWith('.mjs')) continue;
    const source = readFileSync(join(BRIDGE_DIR, name), 'utf8');
    const sent = new Set();
    for (const match of source.matchAll(/\b(?:request|invoke)\(\s*'([a-z_0-9]+)'/g)) {
      sent.add(match[1]);
    }
    if (sent.size > 0) perFile.set(name, sent);
  }
  assert.ok(perFile.size > 0, 'no bridge script sends any command');
  return perFile;
}

test('every command a bridge script sends is implemented by the firmware', () => {
  const implemented = firmwareCommands();
  const tools = bridgeToolNames();
  const offenders = [];

  for (const [file, sent] of sentCommands()) {
    for (const command of sent) {
      // index.mjs also names its own MCP tools, which compose lower-level
      // commands rather than going on the wire under that name.
      if (file === 'index.mjs' && tools.has(command)) continue;
      if (!implemented.has(command)) offenders.push(`${file} -> ${command}`);
    }
  }

  assert.deepEqual(
    offenders,
    [],
    `these scripts send commands the firmware does not implement:\n  ${offenders.join('\n  ')}`,
  );
});
