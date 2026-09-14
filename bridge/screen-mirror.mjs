import { readFileSync } from 'node:fs';
import { deflateSync } from 'node:zlib';

const WIDTH = 320;
const HEIGHT = 240;

const fontSource = readFileSync(new URL('../firmware/cyd_desktop_shell/include/ui_font.h', import.meta.url), 'utf8');
const fontBody = fontSource.slice(fontSource.indexOf('kFont5x7'));
const glyphs = [...fontBody.slice(0, fontBody.indexOf('};')).matchAll(/\{([^{}]+)\}/g)]
  .map((match) => match[1].split(',').map((value) => Number(value.trim())));

// Exposed so a test can catch the glyph table silently failing to parse.
export const glyphCount = () => glyphs.length;

function crc32(buffer) {
  let crc = 0xffffffff;
  for (const byte of buffer) {
    crc ^= byte;
    for (let bit = 0; bit < 8; bit += 1) crc = (crc >>> 1) ^ (0xedb88320 & -(crc & 1));
  }
  return (crc ^ 0xffffffff) >>> 0;
}

function chunk(type, data = Buffer.alloc(0)) {
  const name = Buffer.from(type, 'ascii');
  const output = Buffer.alloc(12 + data.length);
  output.writeUInt32BE(data.length, 0);
  name.copy(output, 4);
  data.copy(output, 8);
  output.writeUInt32BE(crc32(Buffer.concat([name, data])), 8 + data.length);
  return output;
}

function png(rgb) {
  const raw = Buffer.alloc((WIDTH * 3 + 1) * HEIGHT);
  for (let y = 0; y < HEIGHT; y += 1) {
    const row = y * (WIDTH * 3 + 1);
    raw[row] = 0;
    rgb.copy(raw, row + 1, y * WIDTH * 3, (y + 1) * WIDTH * 3);
  }
  const header = Buffer.alloc(13);
  header.writeUInt32BE(WIDTH, 0);
  header.writeUInt32BE(HEIGHT, 4);
  header[8] = 8;
  header[9] = 2;
  return Buffer.concat([
    Buffer.from('89504e470d0a1a0a', 'hex'),
    chunk('IHDR', header),
    chunk('IDAT', deflateSync(raw, { level: 6 })),
    chunk('IEND'),
  ]);
}

function canvas() {
  const pixels = Buffer.alloc(WIDTH * HEIGHT * 3);
  const set = (x, y, color) => {
    if (x < 0 || y < 0 || x >= WIDTH || y >= HEIGHT) return;
    const offset = (y * WIDTH + x) * 3;
    pixels[offset] = color[0];
    pixels[offset + 1] = color[1];
    pixels[offset + 2] = color[2];
  };
  const fill = (x, y, width, height, color) => {
    for (let py = Math.max(0, y); py < Math.min(HEIGHT, y + height); py += 1)
      for (let px = Math.max(0, x); px < Math.min(WIDTH, x + width); px += 1) set(px, py, color);
  };
  const rounded = (x, y, width, height, radius, color) => {
    const r = Math.max(0, Math.min(radius, Math.floor(Math.min(width, height) / 2)));
    for (let py = 0; py < height; py += 1) {
      for (let px = 0; px < width; px += 1) {
        const dx = px < r ? r - px : px >= width - r ? px - (width - r - 1) : 0;
        const dy = py < r ? r - py : py >= height - r ? py - (height - r - 1) : 0;
        if (!dx || !dy || dx * dx + dy * dy <= r * r) set(x + px, y + py, color);
      }
    }
  };
  const text = (x, y, value, color, maxWidth = WIDTH - x) => {
    let cursor = x;
    for (const character of String(value ?? '')) {
      if (cursor + 5 > x + maxWidth) break;
      const code = character.charCodeAt(0);
      const glyph = code >= 32 && code <= 126 ? glyphs[code - 32] : glyphs['?'.charCodeAt(0) - 32];
      for (let column = 0; column < 5; column += 1)
        for (let row = 0; row < 7; row += 1)
          if (glyph?.[column] & (1 << row)) set(cursor + column, y + row, color);
      cursor += 6;
    }
  };
  return { pixels, set, fill, rounded, text };
}

const palette = {
  wallpaper: [10, 28, 57],
  glow: [18, 78, 136],
  panel: [30, 43, 63],
  card: [42, 59, 82],
  button: [49, 84, 130],
  accent: [73, 151, 226],
  text: [239, 246, 255],
  muted: [168, 188, 211],
  danger: [139, 51, 67],
};

export function renderScreenMirror(tree) {
  const draw = canvas();
  draw.fill(0, 0, WIDTH, HEIGHT, palette.wallpaper);
  for (let band = 0; band < 7; band += 1) {
    const color = [10 + band * 2, 30 + band * 6, 60 + band * 10];
    draw.fill(0, band * 31, WIDTH, 31, color);
  }
  draw.rounded(188, -48, 190, 180, 64, palette.glow);
  draw.rounded(-50, 126, 230, 160, 70, [13, 51, 91]);

  const nodes = tree?.nodes ?? [];
  for (const node of nodes) {
    const { x = 0, y = 0, width = 0, height = 0, role = '', label = '', id = '' } = node;
    if (role === 'window' || role === 'dialog') {
      draw.rounded(x + 2, y + 3, width, height, 8, [5, 13, 25]);
      draw.rounded(x, y, width, height, 8, palette.panel);
      draw.fill(x, y + 25, width, 1, palette.accent);
      draw.text(x + 10, y + 9, label, palette.text, width - 20);
    } else if (role === 'button') {
      const color = id.includes('delete') ? palette.danger : id.startsWith('taskbar.') ? [24, 37, 55] : palette.button;
      draw.rounded(x + 1, y + 2, width, height, Math.min(7, Math.floor(height / 4)), [8, 18, 31]);
      draw.rounded(x, y, width, height, Math.min(7, Math.floor(height / 4)), color);
      draw.fill(x + 3, y + 2, Math.max(0, width - 6), 1, id.includes('delete') ? [221, 99, 112] : palette.accent);
      draw.text(x + Math.max(3, Math.floor((width - label.length * 6) / 2)), y + Math.max(3, Math.floor((height - 7) / 2)), label, palette.text, width - 6);
    } else if (role === 'clock') {
      draw.rounded(x, y, width, height, 12, palette.card);
      const cx = x + Math.floor(width / 2), cy = y + Math.floor(height / 2);
      for (let angle = 0; angle < 360; angle += 4) {
        const radians = angle * Math.PI / 180;
        draw.set(Math.round(cx + Math.cos(radians) * 54), Math.round(cy + Math.sin(radians) * 54), palette.muted);
      }
      draw.fill(cx - 1, cy - 33, 3, 35, palette.text);
      draw.fill(cx, cy, 31, 2, palette.accent);
    } else if (role === 'calendar') {
      draw.rounded(x, y, width, height, 10, palette.card);
      for (let row = 1; row < 6; row += 1) draw.fill(x + 7, y + 18 + row * 20, width - 14, 1, [61, 80, 104]);
      for (let column = 1; column < 7; column += 1) draw.fill(x + column * Math.floor(width / 7), y + 18, 1, height - 25, [61, 80, 104]);
    } else if (role === 'status') {
      draw.rounded(x - 3, y - 3, Math.min(width + 6, WIDTH - x + 3), height + 6, 3, [23, 53, 47]);
      draw.text(x, y, label, [129, 230, 190], width);
    } else if (role === 'text') {
      draw.text(x, y, label, palette.text, Math.max(width, 6));
    }
  }
  if (!nodes.some((node) => node.id?.startsWith('taskbar.'))) {
    draw.rounded(126, 211, 128, 27, 8, [24, 37, 55]);
  }
  draw.text(8, 8, `CYD / ${tree?.view ?? 'unknown'}`, palette.muted, 150);
  return png(draw.pixels);
}

