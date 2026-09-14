// docs/mcp-tools_*.md must be exactly what the running bridge generates.

import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { JA, listBridgeTools, OUTPUT, renderToolsDoc } from './mcp-tools-doc.mjs';

const tools = await listBridgeTools();

test('every tool has a Japanese description, and none is left over', () => {
  const names = tools.map((tool) => tool.name);
  assert.deepEqual(names.filter((name) => !JA[name]), [], 'add these to JA in bridge/mcp-tools-doc.mjs');
  assert.deepEqual(Object.keys(JA).filter((name) => !names.includes(name)), [], 'remove these from JA');
});

for (const [language, url] of Object.entries(OUTPUT)) {
  test(`docs/mcp-tools_${language}.md is up to date (npm run docs:tools)`, () => {
    const committed = readFileSync(url, 'utf8').replaceAll('\r\n', '\n');
    assert.equal(committed, renderToolsDoc(tools, language));
  });
}

test('the reference does not carry a machine-specific path', () => {
  for (const url of Object.values(OUTPUT)) {
    assert.doesNotMatch(readFileSync(url, 'utf8'), /[A-Z]:\\|\/mnt\/|\/home\//);
  }
});
