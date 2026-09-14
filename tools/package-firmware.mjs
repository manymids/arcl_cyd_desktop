// Packages the built firmware for a binary release (GitHub Releases).
//
//   npm run package:firmware [-- --allow-dirty] [--no-merged]
//
// Input: micropython.bin, bootloader.bin and partition-table.bin in the
// repository root, where the README's copy command puts them after a build.
// Output: dist/cyd-desktop-firmware-<version>.zip with
//
//   cyd-desktop-firmware-<version>/
//     bootloader.bin, partition-table.bin, micropython.bin
//     cyd-desktop-<version>-full.bin   one image for 0x0 (blank boards; erases settings)
//     FLASHING.md                      how to flash, in Japanese and English
//     BUILD-INFO.json                  versions, commits, checksums
//     SHA256SUMS
//     LICENSE, THIRD_PARTY_NOTICES*.md, licenses/
//
// A binary must be traceable to its source, so a repository with uncommitted
// changes is refused unless --allow-dirty is given (the info then says so).

import { execFileSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { existsSync, mkdirSync, readdirSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { crc32, deflateRawSync } from 'node:zlib';

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), '..');

// Flash layout of firmware/cyd_desktop_board/partitions-cyd.csv on a 4 MiB ESP32.
export const FLASH_LAYOUT = [
  { offset: 0x1000, file: 'bootloader.bin' },
  { offset: 0x8000, file: 'partition-table.bin' },
  { offset: 0x10000, file: 'micropython.bin' },
];
const APP_PARTITION_BYTES = 0x300000;

/** ESP-IDF's esp_app_desc_t, which sits right after the first segment header of an app image. */
export function readAppDescription(image) {
  if (image.length < 0x100 || image[0] !== 0xe9) throw new Error('micropython.bin is not an ESP32 app image');
  const base = 24 + 8;
  if (image.readUInt32LE(base) !== 0xabcd5432) throw new Error('micropython.bin has no ESP-IDF app description');
  const text = (offset, length) => image.subarray(base + offset, base + offset + length).toString('latin1').replace(/\0.*$/s, '');
  return {
    idf_version: text(112, 32),
    compile_date: text(96, 16),
    compile_time: text(80, 16),
    elf_sha256: image.subarray(base + 144, base + 176).toString('hex'),
  };
}

export function sha256(buffer) {
  return createHash('sha256').update(buffer).digest('hex');
}

/** A zip archive (deflate, no extras) from [{ name, data }]. */
export function zip(entries) {
  const locals = [];
  const centrals = [];
  let offset = 0;
  // 1980-01-01 00:00: fixed, so the same inputs make the same archive.
  const dosTime = 0;
  const dosDate = (0 << 9) | (1 << 5) | 1;
  for (const { name, data } of entries) {
    const nameBytes = Buffer.from(name, 'utf8');
    const compressed = deflateRawSync(data, { level: 9 });
    const checksum = crc32(data) >>> 0;
    const local = Buffer.alloc(30);
    local.writeUInt32LE(0x04034b50, 0);
    local.writeUInt16LE(20, 4);
    local.writeUInt16LE(0x0800, 6);  // UTF-8 names
    local.writeUInt16LE(8, 8);       // deflate
    local.writeUInt16LE(dosTime, 10);
    local.writeUInt16LE(dosDate, 12);
    local.writeUInt32LE(checksum, 14);
    local.writeUInt32LE(compressed.length, 18);
    local.writeUInt32LE(data.length, 22);
    local.writeUInt16LE(nameBytes.length, 26);
    local.writeUInt16LE(0, 28);
    locals.push(local, nameBytes, compressed);

    const central = Buffer.alloc(46);
    central.writeUInt32LE(0x02014b50, 0);
    central.writeUInt16LE(20, 4);
    central.writeUInt16LE(20, 6);
    central.writeUInt16LE(0x0800, 8);
    central.writeUInt16LE(8, 10);
    central.writeUInt16LE(dosTime, 12);
    central.writeUInt16LE(dosDate, 14);
    central.writeUInt32LE(checksum, 16);
    central.writeUInt32LE(compressed.length, 20);
    central.writeUInt32LE(data.length, 24);
    central.writeUInt16LE(nameBytes.length, 28);
    central.writeUInt32LE(offset, 42);
    centrals.push(central, nameBytes);
    offset += local.length + nameBytes.length + compressed.length;
  }
  const centralSize = centrals.reduce((sum, part) => sum + part.length, 0);
  const end = Buffer.alloc(22);
  end.writeUInt32LE(0x06054b50, 0);
  end.writeUInt16LE(entries.length, 8);
  end.writeUInt16LE(entries.length, 10);
  end.writeUInt32LE(centralSize, 12);
  end.writeUInt32LE(offset, 16);
  return Buffer.concat([...locals, ...centrals, end]);
}

export function flashingGuide(version, fullImage) {
  const threeFiles = FLASH_LAYOUT.map(({ offset, file }) => `0x${offset.toString(16)} ${file}`).join(' ');
  const full = fullImage
    ? {
      ja: `\nWeb ブラウザの書き込みツールなどで 1 ファイルにまとめたい場合は、\`${fullImage}\` を 0x0 に書きます（新品の基板用）。\nこのイメージは設定領域（NVS）も消去するので、タッチ補正と設定は初期化されます。\n\n\`\`\`\npython -m esptool --chip esp32 --port COM6 --before default_reset write_flash 0x0 ${fullImage}\n\`\`\`\n`,
      en: `\nTo flash a single file (for example from a browser-based flasher), write \`${fullImage}\` at 0x0 (blank boards).\nThis image also erases the settings area (NVS), so touch calibration and settings are reset.\n\n\`\`\`\npython -m esptool --chip esp32 --port COM6 --before default_reset write_flash 0x0 ${fullImage}\n\`\`\`\n`,
    }
    : { ja: '', en: '' };
  return `# ARCL CYD Desktop ${version} firmware

## 日本語

対応するのは **ILI9341 版の ESP32-2432S028（CYD）** だけです。ST7789 版、静電容量タッチ版、画面サイズの違う派生品では動きません。

1. esptool を入れます: \`pip install esptool\`
2. \`SHA256SUMS\` でファイルが壊れていないことを確認します（PowerShell: \`Get-FileHash micropython.bin\`）。
3. USB で接続し、ポート（例 \`COM6\`）を確認して、他のシリアル接続を閉じます。

**新品の基板、またはこれより前のファームウェアが入った基板**（bootloader とパーティション表も書きます。タッチ補正と設定は残ります）

\`\`\`
python -m esptool --chip esp32 --port COM6 --before default_reset write_flash ${threeFiles}
\`\`\`
${full.ja}
**更新**（このファームウェアが入っている基板）

\`\`\`
python -m esptool --chip esp32 --port COM6 --before default_reset write_flash 0x10000 micropython.bin
\`\`\`

タッチ補正が保存されていない基板では、起動時に補正の十字が 3 か所に順に出るので、それぞれの中心をタップします。無線（Wi-Fi／Bluetooth）は Settings > WIRELESS で選びます（初期値は OFF）。
使い方は公開リポジトリの README を参照してください。第三者ソフトウェアのライセンスは \`THIRD_PARTY_NOTICES_ja.md\` と \`licenses/\` にあります。

## English

Only the **ESP32-2432S028 (CYD) with the ILI9341 panel** is supported. Variants with an ST7789 panel, capacitive touch or another screen size do not work.

1. Install esptool: \`pip install esptool\`
2. Check the files against \`SHA256SUMS\` (PowerShell: \`Get-FileHash micropython.bin\`).
3. Connect over USB, find the port (for example \`COM6\`) and close other serial connections.

**Blank boards, or boards with older firmware** (writes the bootloader and partition table too; touch calibration and settings are kept)

\`\`\`
python -m esptool --chip esp32 --port COM6 --before default_reset write_flash ${threeFiles}
\`\`\`
${full.en}
**Updates** (boards already running this firmware)

\`\`\`
python -m esptool --chip esp32 --port COM6 --before default_reset write_flash 0x10000 micropython.bin
\`\`\`

A board without a stored touch calibration shows a crosshair at three places in turn at boot; tap the centre of each. Choose the radio (Wi-Fi or Bluetooth) in Settings > WIRELESS (OFF by default).
See the repository README for usage. Third-party licenses are in \`THIRD_PARTY_NOTICES_en.md\` and \`licenses/\`.
`;
}

function git(args) {
  return execFileSync('git', args, { cwd: ROOT, encoding: 'utf8' }).trim();
}

/** Throws unless each file sits at its offset in the merged image. */
export function checkMergedImage(merged, images) {
  for (const { offset, file } of FLASH_LAYOUT) {
    const expected = images[file];
    if (!merged.subarray(offset, offset + expected.length).equals(expected)) {
      throw new Error(`the merged image does not contain ${file} at 0x${offset.toString(16)}`);
    }
  }
}

function mergeImage(output, images) {
  const args = ['-m', 'esptool', '--chip', 'esp32', 'merge_bin', '-o', output,
    '--flash_mode', 'dio', '--flash_freq', '40m', '--flash_size', '4MB',
    ...FLASH_LAYOUT.flatMap(({ offset, file }) => [`0x${offset.toString(16)}`, join(ROOT, file)])];
  execFileSync(process.env.PYTHON ?? 'python', args, { stdio: ['ignore', 'ignore', 'inherit'] });
  const merged = readFileSync(output);
  // esptool rewrites the flash size and mode fields in the bootloader header
  // only when told to; with the values above the bytes must match exactly.
  checkMergedImage(merged, images);
  return merged;
}

function listLicenses() {
  return readdirSync(join(ROOT, 'licenses')).sort().map((name) => `licenses/${name}`);
}

function main(argv) {
  const allowDirty = argv.includes('--allow-dirty');
  const merged = !argv.includes('--no-merged');
  const version = JSON.parse(readFileSync(join(ROOT, 'package.json'), 'utf8')).version;

  const images = Object.fromEntries(FLASH_LAYOUT.map(({ file }) => {
    const path = join(ROOT, file);
    if (!existsSync(path)) throw new Error(`${file} is missing: build the firmware and copy it to the repository root (README)`);
    return [file, readFileSync(path)];
  }));
  if (images['micropython.bin'].length > APP_PARTITION_BYTES) throw new Error('micropython.bin does not fit the 3 MB app partition');
  const app = readAppDescription(images['micropython.bin']);

  // Untracked files count too: a build compiles whatever is on disk.
  const dirty = git(['status', '--porcelain']).split('\n').filter(Boolean)
    .filter((line) => !/ (micropython|bootloader|partition-table)\.bin$/.test(line));
  if (dirty.length && !allowDirty) {
    throw new Error(`the repository has uncommitted changes, so the binary could not be traced to a commit:\n${dirty.join('\n')}\n(use --allow-dirty for a test package)`);
  }
  const submodule = git(['ls-files', '-s', 'firmware/micropython']).split(/\s+/)[1] ?? null;

  const folder = `cyd-desktop-firmware-${version}`;
  const entries = [];
  const add = (name, data) => entries.push({ name: `${folder}/${name}`, data: Buffer.isBuffer(data) ? data : Buffer.from(data) });
  for (const { file } of FLASH_LAYOUT) add(file, images[file]);

  let fullName = null;
  if (merged) {
    fullName = `cyd-desktop-${version}-full.bin`;
    const scratch = join(tmpdir(), `cyd-merge-${process.pid}.bin`);
    try {
      add(fullName, mergeImage(scratch, images));
    } finally {
      rmSync(scratch, { force: true });
    }
  }

  const binaries = entries.map(({ name, data }) => ({ file: name.slice(folder.length + 1), bytes: data.length, sha256: sha256(data) }));
  const info = {
    name: 'ARCL CYD Desktop firmware',
    version,
    board: 'ESP32-2432S028 (ILI9341 panel, XPT2046 touch), ESP32-D0WD-V3, 4 MiB flash',
    source_commit: git(['rev-parse', 'HEAD']),
    source_dirty: dirty.length > 0,
    micropython: { version: 'v1.29.0', commit: submodule },
    esp_idf: app.idf_version,
    compiled: `${app.compile_date} ${app.compile_time}`,
    app_elf_sha256: app.elf_sha256,
    flash: FLASH_LAYOUT.map(({ offset, file }) => ({ offset: `0x${offset.toString(16)}`, file })),
    full_image: fullName ? { offset: '0x0', file: fullName, erases_settings: true } : null,
    files: binaries,
  };
  add('FLASHING.md', flashingGuide(version, fullName));
  add('BUILD-INFO.json', `${JSON.stringify(info, null, 2)}\n`);
  add('SHA256SUMS', binaries.map(({ file, sha256: hash }) => `${hash}  ${file}`).join('\n') + '\n');
  for (const name of ['LICENSE', 'THIRD_PARTY_NOTICES.md', 'THIRD_PARTY_NOTICES_ja.md', 'THIRD_PARTY_NOTICES_en.md', ...listLicenses()]) {
    add(name, readFileSync(join(ROOT, name)));
  }

  mkdirSync(join(ROOT, 'dist'), { recursive: true });
  const archive = join(ROOT, 'dist', `${folder}.zip`);
  writeFileSync(archive, zip(entries));
  console.log(`wrote ${archive} (${entries.length} files)`);
  console.log(`source ${info.source_commit}${info.source_dirty ? ' (uncommitted changes)' : ''}, ESP-IDF ${info.esp_idf}, MicroPython ${submodule}`);
  for (const { file, sha256: hash } of binaries) console.log(`${hash}  ${file}`);
  return 0;
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  try {
    process.exitCode = main(process.argv.slice(2));
  } catch (error) {
    console.error(error.message);
    process.exitCode = 1;
  }
}
