// Keeps the MCP tool list in line with the ARCL common specification:
// arcl_* only for the common API, cyd_* for everything else, every tool
// classified by layer, and the layer selection actually filtering tools/list.

import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { StdioClientTransport } from '@modelcontextprotocol/sdk/client/stdio.js';
import { describe as describeTool, layeredRegistrar, LAYERS, parseLayers, TOOLS } from './arcl-tools.mjs';

const ROOT = fileURLToPath(new URL('..', import.meta.url));
const index = readFileSync(new URL('./index.mjs', import.meta.url), 'utf8');
const registeredInIndex = [...index.matchAll(/^tool\('([a-z0-9_]+)'/gm)].map((match) => match[1]);

// The common API of the ARCL common specification v0.5 (docs/external/), section 7.3. A cyd tool may take
// one of these names only when it honours that contract.
const COMMON_API = [
  'arcl_run', 'arcl_pause', 'arcl_resume', 'arcl_status', 'arcl_reset', 'arcl_save_state', 'arcl_load_state',
  'arcl_screenshot', 'arcl_input_state', 'arcl_audio_record', 'arcl_key', 'arcl_type', 'arcl_mouse',
  'arcl_joypad', 'arcl_clear_input', 'arcl_input_macro',
  'arcl_console_read', 'arcl_host_dir', 'arcl_mount', 'arcl_command',
  'arcl_registers', 'arcl_read_mem', 'arcl_write_mem', 'arcl_write_registers', 'arcl_disasm', 'arcl_stack',
  'arcl_breakpoint', 'arcl_step',
  'arcl_video', 'arcl_palette', 'arcl_sprites', 'arcl_dma', 'arcl_irq', 'arcl_vram',
  'arcl_snapshot', 'arcl_rewind', 'arcl_speed',
];

test('every registered tool is classified, and every classification is registered', () => {
  assert.ok(registeredInIndex.length >= 30, `found only ${registeredInIndex.length} tool() calls in index.mjs`);
  assert.deepEqual(registeredInIndex.filter((name) => !TOOLS[name]), []);
  assert.deepEqual(Object.keys(TOOLS).filter((name) => !registeredInIndex.includes(name)), []);
  assert.doesNotMatch(index, /server\.tool\(/, 'register tools through the layered registrar, not server.tool');
});

test('tool names follow the arcl_* / cyd_* convention', () => {
  for (const name of Object.keys(TOOLS)) {
    assert.match(name, /^(arcl|cyd)_[a-z0-9_]+$/, name);
    if (name.startsWith('arcl_')) assert.ok(COMMON_API.includes(name), `${name} is not in the ARCL common API`);
  }
  assert.ok(TOOLS.arcl_status, 'arcl_status is required by the specification');
});

test('only Control tools are layer independent', () => {
  for (const [name, entry] of Object.entries(TOOLS)) {
    assert.ok(['Observation', 'Action', 'Control'].includes(entry.category), name);
    if (entry.category === 'Control') assert.equal(entry.layer, null, name);
    else assert.ok(LAYERS.includes(entry.layer), `${name} needs a layer`);
  }
});

test('descriptions start with the family, category and layer', () => {
  assert.equal(describeTool('arcl_status', 'X.'), 'Common (arcl). Control, any layer. X.');
  assert.equal(describeTool('cyd_tap', 'X.'), 'Machine-specific (cyd). Action, L0. X.');
});

test('layer selection', () => {
  assert.deepEqual(parseLayers([], {}), LAYERS);
  assert.deepEqual(parseLayers(['--mcp-layers=l1,L0'], {}), ['L0', 'L1']);
  assert.deepEqual(parseLayers([], { CYD_MCP_LAYERS: 'L2' }), ['L2']);
  assert.deepEqual(parseLayers(['--mcp-layers=L0'], { CYD_MCP_LAYERS: 'L2' }), ['L0'], 'the flag wins');
  assert.throws(() => parseLayers(['--mcp-layers=L0,L9'], {}), /L9/);
});

test('the registrar skips tools outside the enabled layers', () => {
  const names = [];
  const register = layeredRegistrar({ tool: (name) => names.push(name) }, ['L0']);
  register('arcl_status', 'd', {}, () => {});
  register('cyd_pause', 'd', {}, () => {});
  register('cyd_tap', 'd', {}, () => {});
  register('cyd_launch', 'd', {}, () => {});
  register('cyd_logs', 'd', {}, () => {});
  assert.deepEqual(names, ['arcl_status', 'cyd_pause', 'cyd_tap']);
  assert.throws(() => register('desktop_status', 'd', {}, () => {}), /no ARCL classification/);
});

test('the running bridge lists only the selected layers', async () => {
  const listFor = async (args) => {
    const transport = new StdioClientTransport({
      command: process.execPath, args: ['bridge/index.mjs', ...args], cwd: ROOT, stderr: 'pipe',
    });
    const client = new Client({ name: 'arcl-tools-test', version: '0.0.0' }, { capabilities: {} });
    await client.connect(transport);
    try {
      return {
        tools: (await client.listTools()).tools,
        instructions: client.getInstructions(),
      };
    } finally {
      await client.close();
    }
  };

  const all = await listFor([]);
  assert.equal(all.tools.length, Object.keys(TOOLS).length);
  assert.match(all.instructions, /machine "cyd"/);

  const l0 = (await listFor(['--mcp-layers=L0'])).tools.map((tool) => tool.name);
  const expected = Object.entries(TOOLS)
    .filter(([, entry]) => entry.layer === null || entry.layer === 'L0')
    .map(([name]) => name);
  assert.deepEqual([...l0].sort(), [...expected].sort());
  assert.ok(!l0.includes('cyd_launch'));
});
