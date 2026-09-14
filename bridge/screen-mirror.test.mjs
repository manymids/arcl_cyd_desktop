import assert from 'node:assert/strict';
import { glyphCount } from './screen-mirror.mjs';
import test from 'node:test';
import { inflateSync } from 'node:zlib';
import { renderScreenMirror } from './screen-mirror.mjs';

test('renders a valid 320x240 RGB PNG from a retained UI tree', () => {
  const image = renderScreenMirror({
    view: 'home',
    nodes: [
      { id: 'home.clock', role: 'button', x: 10, y: 58, width: 68, height: 64, label: 'Clock', enabled: true },
      { id: 'taskbar.start', role: 'button', x: 134, y: 214, width: 30, height: 21, label: 'Start', enabled: true },
    ],
  });
  assert.equal(image.subarray(0, 8).toString('hex'), '89504e470d0a1a0a');
  assert.equal(image.readUInt32BE(16), 320);
  assert.equal(image.readUInt32BE(20), 240);
  const idat = [];
  for (let offset = 8; offset < image.length;) {
    const length = image.readUInt32BE(offset);
    const type = image.subarray(offset + 4, offset + 8).toString('ascii');
    if (type === 'IDAT') idat.push(image.subarray(offset + 8, offset + 8 + length));
    offset += 12 + length;
  }
  assert.equal(inflateSync(Buffer.concat(idat)).length, (320 * 3 + 1) * 240);

  // The glyph table is scraped out of firmware/.../ui_font.h by regex. Without
  // this the test passes on a blank image, so a reformat of that header would
  // silently produce mirrors with no text at all.
  assert.equal(glyphCount(), 95, 'expected one glyph per printable ASCII code');
});

