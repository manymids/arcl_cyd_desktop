// The firmware release package: the zip writer produces archives other tools
// can read, the app description is parsed from real image bytes, and the
// flashing guide uses the partition layout. No device or build needed.

import { test } from 'node:test';
import assert from 'node:assert/strict';
import { inflateRawSync, crc32 } from 'node:zlib';
import { checkMergedImage, flashingGuide, FLASH_LAYOUT, readAppDescription, sha256, zip } from './package-firmware.mjs';

function unzip(archive) {
  const end = archive.lastIndexOf(Buffer.from([0x50, 0x4b, 0x05, 0x06]));
  assert.ok(end >= 0, 'no end of central directory');
  const count = archive.readUInt16LE(end + 10);
  let pointer = archive.readUInt32LE(end + 16);
  const files = new Map();
  for (let index = 0; index < count; index += 1) {
    assert.equal(archive.readUInt32LE(pointer), 0x02014b50);
    const method = archive.readUInt16LE(pointer + 10);
    const checksum = archive.readUInt32LE(pointer + 16);
    const compressedSize = archive.readUInt32LE(pointer + 20);
    const nameLength = archive.readUInt16LE(pointer + 28);
    const localOffset = archive.readUInt32LE(pointer + 42);
    const name = archive.subarray(pointer + 46, pointer + 46 + nameLength).toString('utf8');
    assert.equal(archive.readUInt32LE(localOffset), 0x04034b50);
    const dataStart = localOffset + 30 + archive.readUInt16LE(localOffset + 26) + archive.readUInt16LE(localOffset + 28);
    assert.equal(method, 8);
    const data = inflateRawSync(archive.subarray(dataStart, dataStart + compressedSize));
    assert.equal(crc32(data) >>> 0, checksum, `${name} checksum`);
    files.set(name, data);
    pointer += 46 + nameLength;
  }
  return files;
}

test('zip archives round-trip, including binary and empty files', () => {
  const binary = Buffer.from(Array.from({ length: 5000 }, (_, index) => (index * 37) & 0xff));
  const archive = zip([
    { name: 'pkg/a.bin', data: binary },
    { name: 'pkg/empty.txt', data: Buffer.alloc(0) },
    { name: 'pkg/licenses/日本語.md', data: Buffer.from('テキスト\n') },
  ]);
  const files = unzip(archive);
  assert.deepEqual([...files.keys()], ['pkg/a.bin', 'pkg/empty.txt', 'pkg/licenses/日本語.md']);
  assert.ok(files.get('pkg/a.bin').equals(binary));
  assert.equal(files.get('pkg/empty.txt').length, 0);
  assert.equal(files.get('pkg/licenses/日本語.md').toString('utf8'), 'テキスト\n');
  // Fixed timestamps: the same inputs give the same bytes.
  assert.equal(sha256(archive), sha256(zip([
    { name: 'pkg/a.bin', data: binary },
    { name: 'pkg/empty.txt', data: Buffer.alloc(0) },
    { name: 'pkg/licenses/日本語.md', data: Buffer.from('テキスト\n') },
  ])));
});

test('the ESP-IDF app description is read from an app image', () => {
  const image = Buffer.alloc(0x200);
  image[0] = 0xe9;
  const base = 32;
  image.writeUInt32LE(0xabcd5432, base);
  image.write('23:11:01', base + 80, 'latin1');
  image.write('Sep 13 2026', base + 96, 'latin1');
  image.write('v5.5.2', base + 112, 'latin1');
  image.fill(0xab, base + 144, base + 176);
  assert.deepEqual(readAppDescription(image), {
    idf_version: 'v5.5.2', compile_date: 'Sep 13 2026', compile_time: '23:11:01', elf_sha256: 'ab'.repeat(32),
  });
  assert.throws(() => readAppDescription(Buffer.alloc(0x200)), /not an ESP32 app image/);
  image.writeUInt32LE(0, base);
  assert.throws(() => readAppDescription(image), /no ESP-IDF app description/);
});

test('a merged image must hold each file at its offset', () => {
  const images = { 'bootloader.bin': Buffer.from([1, 2, 3]), 'partition-table.bin': Buffer.from([4, 5]), 'micropython.bin': Buffer.from([6]) };
  const merged = Buffer.alloc(0x10001, 0xff);
  for (const { offset, file } of FLASH_LAYOUT) images[file].copy(merged, offset);
  checkMergedImage(merged, images);
  merged[0x8001] = 0;
  assert.throws(() => checkMergedImage(merged, images), /partition-table\.bin at 0x8000/);
});

test('the flashing guide writes the partition layout and warns about the full image', () => {
  const guide = flashingGuide('0.0.1', 'cyd-desktop-0.0.1-full.bin');
  assert.match(guide, /write_flash 0x1000 bootloader\.bin 0x8000 partition-table\.bin 0x10000 micropython\.bin/);
  assert.match(guide, /write_flash 0x10000 micropython\.bin/);
  assert.match(guide, /write_flash 0x0 cyd-desktop-0\.0\.1-full\.bin/);
  assert.match(guide, /erases the settings area/);
  assert.match(guide, /ILI9341/);
  assert.doesNotMatch(flashingGuide('0.0.1', null), /full\.bin/);
});
