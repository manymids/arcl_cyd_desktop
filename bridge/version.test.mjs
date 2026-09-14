// One release version everywhere: package.json, package-lock.json, the MCP
// server and the firmware (desktop_status). The firmware string once stayed at
// a development-phase tag for many releases. No device needed.

import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';

const read = (path) => readFileSync(new URL(`../${path}`, import.meta.url), 'utf8');

test('package, lock file, MCP server and firmware report the same version', () => {
  const version = JSON.parse(read('package.json')).version;
  assert.match(version, /^\d+\.\d+\.\d+$/);
  const lock = JSON.parse(read('package-lock.json'));
  assert.equal(lock.version, version, 'package-lock.json version');
  assert.equal(lock.packages[''].version, version, 'package-lock.json root package version');
  const firmware = /#define CYD_DESKTOP_VERSION "([^"]+)"/.exec(read('firmware/cyd_desktop_shell/include/desktop_version.h'))?.[1];
  assert.equal(firmware, version, 'firmware CYD_DESKTOP_VERSION');
  assert.match(read('firmware/cyd_desktop_shell/protocol.cpp'), /"version\\":\\"" CYD_DESKTOP_VERSION/,
    'desktop_status must report CYD_DESKTOP_VERSION');
  const server = /new McpServer\(\{ name: 'cyd-desktop', version: '([^']+)' \}/.exec(read('bridge/index.mjs'))?.[1];
  assert.equal(server, version, 'MCP server version');
});
