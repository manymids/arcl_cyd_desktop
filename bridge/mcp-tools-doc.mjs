// Generates docs/mcp-tools_ja.md and docs/mcp-tools_en.md from the tools the
// bridge actually lists, so the reference cannot drift from bridge/index.mjs.
//
//   npm run docs:tools          rewrite both files
//
// English descriptions come from the tools themselves. The Japanese ones are
// kept below; mcp-tools-doc.test.mjs fails when a tool has none.

import { writeFileSync } from 'node:fs';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { StdioClientTransport } from '@modelcontextprotocol/sdk/client/stdio.js';
import { LAYERS, TOOLS } from './arcl-tools.mjs';

const ROOT = fileURLToPath(new URL('..', import.meta.url));
export const OUTPUT = {
  ja: new URL('../docs/mcp-tools_ja.md', import.meta.url),
  en: new URL('../docs/mcp-tools_en.md', import.meta.url),
};

export const JA = {
  arcl_status: '機種 ID（`cyd`）、有効なレイヤ、現在の画面と前面アプリ、MicroPython アプリの実行状態を返す。実機なので `running` は常に true、`frame` は `null`、`time_control` は `"none"`。',
  cyd_runtime_status: '前面の MicroPython アプリの一時停止状態、`cyd.update()` の回数（`frame`）、残りのステップ数を返す。',
  cyd_pause: '前面の MicroPython アプリを、次の `cyd.update()` で止める。ネイティブのシェルやアプリ、動画、時計は止まらない。',
  cyd_resume: '止めていた MicroPython アプリを再開する。',
  cyd_step: 'MicroPython アプリを止めたうえで、`cyd.update()` を 1 回だけ進める。',
  cyd_run: 'MicroPython アプリを止めたうえで、`cyd.update()` を `frames` 回分進めてよいことにする。進み終わるのを待たずに返る。',
  cyd_ui_tree: '今表示中の画面の UI ノード（id、役割、ラベル、位置と大きさ、有効かどうか）をすべて返す。画面の状態を知るいちばん軽い方法。',
  cyd_screen_mirror: 'UI ツリーから 320×240 の PNG を描いて返す。LCD の画素を読んだものではなく、キャンバスの図形、ゲーム、JPEG、ネイティブアプリの画面は写らない。',
  cyd_input_state: 'タッチが今押されているか、最後の座標、キューに残っている合成タップの数を返す。',
  cyd_input_read: '直近の入力イベント（`tap.home`、`tap.<ボタン id>` など）を、消さずに返す。',
  cyd_tap: '画面座標にタップを 1 回キューに入れる。',
  cyd_input_macro: 'タップを最大 64 回、指定した待ち時間（ミリ秒）をはさんで順に送る。',
  cyd_input_clear: '直近の入力イベントを返し、消す。',
  cyd_apps_list: '組み込みの画面とネイティブアプリの id を返す（ファームウェアに固定で書かれた一覧）。SD のアプリは `cyd_installed_apps_list`。',
  cyd_installed_apps_list: 'microSD にインストールされた MicroPython アプリと、Home に固定されているかを返す。',
  cyd_shortcuts_list: 'Home に表示している microSD 上のショートカットを返す。',
  cyd_settings_get: '明るさ、テーマ、アニメーション、Bluetooth キーボードのキー配列の設定を返す。',
  cyd_launch: '組み込みの画面（clock、calendar、scripts、settings、editor）、ネイティブアプリ、インストール済みの MicroPython アプリを id で開く。MicroPython アプリは起動をキューに入れるだけなので、成功は「起動を受け付けた」の意味。',
  cyd_activate: '`cyd_launch` と同じ。古い名前で書かれたクライアント向け。',
  cyd_home: '前面のアプリを終了して Home に戻る。',
  cyd_script_launch: '`/sd/apps/<app_id>/` の MicroPython アプリを前面で起動する。',
  cyd_settings_set: '明るさ、テーマ、アニメーション、Bluetooth キーボードのキー配列を変更して保存する。指定しなかった項目は変わらない。',
  cyd_key_press: 'ハードウェアキーボードの 1 キーとして押す。エディタは編集に使い、実行中のアプリは cyd.key() で受け取る。名前は文字そのもの（a、A）、ENTER、BACKSPACE、DELETE、TAB、ESC、UP、DOWN、LEFT、RIGHT、HOME、END、PAGEUP、PAGEDOWN、INSERT、F1〜F12 で、前に CTRL+・ALT+・SHIFT+ を付けられる（CTRL+S）。CTRL+C は実行中のアプリを中断する。',
  cyd_type_text: '文字列をハードウェアキーボードの打鍵として 1 文字ずつ送る。改行は ENTER。表示可能な ASCII だけ。',
  cyd_bt_status: 'Bluetooth キーボードの状態を返す。接続中と記憶済みのキーボード、ペアリングの進行と入力すべき PIN またはパスキー、直前のスキャン結果。Bluetooth は無線モードが bluetooth のときだけ動く。',
  cyd_bt_scan: 'ペアリングモードの Bluetooth Classic キーボードを指定秒数だけ探す。結果は cyd_bt_status で確認する。',
  cyd_bt_connect: '直前のスキャン結果の index のキーボードとペアリングする。画面（と cyd_bt_status）に出る PIN またはパスキーをキーボードで入力して Enter。新しくペアリングすると前のキーボードは忘れる。',
  cyd_bt_forget: '記憶している Bluetooth キーボードを切断して忘れる。',
  cyd_radio_status: 'この起動の無線モード（off、wifi、bluetooth）と、次回起動用に保存されたモードを返す。Wi-Fi と Bluetooth は排他。',
  cyd_radio_set: '次回起動の無線モード（off、wifi、bluetooth）を保存する。再起動後に有効。restart を true にすると応答の直後に再起動する（前面アプリの実行中は失敗）。',
  cyd_time_set: 'Unix 時刻（秒）で本体の時計を合わせる。',
  cyd_package_upload: 'ファイル 1 つを `/sd/apps/<app_id>/` へ SHA-256 を確かめながら書き込む。バイナリは `encoding: "base64"`。`replace` が false なら既存のファイルは上書きしない。アプリの実行中は失敗する。',
  cyd_app_deploy: 'アプリのディレクトリ（manifest.json、起動ファイル、画像などの全ファイル）を、manifest を検査したうえで転送する。`app_dir` は `CYD_DESKTOP_APPS_ROOT`（既定はリポジトリの `examples`）の中だけ。転送前に Home へ戻る。',
  cyd_app_delete: 'インストール済みの MicroPython アプリと、それを指す Home のショートカットを削除する。元に戻せない。',
  cyd_shortcut_create: 'Home のショートカットを作る。Home の 1 ページ目に 2 つ、以降のページに 8 つずつ、最大 18 個。',
  cyd_shortcut_update: '既存のショートカットを変更する。',
  cyd_shortcut_delete: 'ショートカットを削除する。',
  cyd_diagnostics: '空きヒープ、タスク数、描画回数、SPI 転送量、Watchdog の状態、ネイティブアプリのフレーム時間などを返す。`fingerprint: 1` で LCD へ送った内容の指紋（CRC-32 の和）を 0 から数え始め、`0` で止める。',
  cyd_logs: '直近 16 件の UI・入力・アプリのイベントログを返す。',
  cyd_app_error: '直近に失敗した MicroPython アプリのトレースバック全文を返す（イベントログは 48 文字の要約だけ）。',
  cyd_sd_status: '起動時に microSD を安全にマウントできたかを返す。',
  cyd_touch_calibrate_start: 'タッチ較正を始める。表示される 3 つの十字を順にタップすると保存される。',
};

const TEXT = {
  ja: {
    title: '# MCP ツール一覧',
    switcher: '[English](mcp-tools_en.md) | 日本語',
    intro: [
      'MCP bridge（`node bridge/index.mjs`）が公開するツールの一覧です。',
      '**この文書は `npm run docs:tools` で bridge の定義から生成しています。直接編集しないでください。**',
      '定義と食い違うと `bridge/mcp-tools-doc.test.mjs` が失敗します。',
      '',
      '- **名前**: [ARCL 共通仕様](external/arcl-common-spec_ja.md)（外部仕様）の命名規約に従い、共通 API は `arcl_*`、CYD 固有のものは `cyd_*` です。',
      '- **種別**: Observation は状態を読むだけ、Action は状態を変える、Control は実行を制御します。',
      '- **レイヤ**: `node bridge/index.mjs --mcp-layers=L0,L1`（または環境変数 `CYD_MCP_LAYERS`）で、公開するレイヤを選べます。指定しなければ全レイヤです。Control 系はレイヤに関係なく常に公開されます。',
      '- **時間**: CYD はエミュレータではなく実機です。シェル、ネイティブアプリ、動画、時計は実時間で動き続けます。`cyd_pause` などで止められるのは、前面の MicroPython アプリの `cyd.update()` だけです。',
      '- **プロトコルとの関係**: ファームウェアのシリアルプロトコルのコマンドは `desktop_*` という名前です。MCP のツール名とは別物です。',
    ],
    total: (count) => `全 ${count} ツール。`,
    groups: { Control: 'Control（常に公開）', L0: 'L0 画面と入力', L1: 'L1 アプリ・設定・ファイル', L2: 'L2 ログと診断', L3: 'L3 ハードウェア', L4: 'L4' },
    header: '| ツール | 種別 | 引数 | 説明 |',
    separator: '、',
    none: 'なし',
    types: { integer: '整数', string: '文字列', boolean: '真偽値', array: '配列', number: '数値', object: 'オブジェクト' },
    optional: '省略可',
    default: (value) => `既定 ${value}`,
    range: (min, max) => (max === undefined ? `${min} 以上` : `${min}〜${max}`),
    length: (min, max) => (min === undefined ? `最大 ${max} 文字` : `${min}〜${max} 文字`),
    items: (min, max, fields) => `${min}〜${max} 件、各要素は ${fields}`,
  },
  en: {
    title: '# MCP tools',
    switcher: 'English | [日本語](mcp-tools_ja.md)',
    intro: [
      'The tools published by the MCP bridge (`node bridge/index.mjs`).',
      '**This document is generated from the bridge by `npm run docs:tools`. Do not edit it by hand.**',
      '`bridge/mcp-tools-doc.test.mjs` fails when it no longer matches the bridge.',
      '',
      '- **Names** follow the [ARCL common specification](external/arcl-common-spec_en.md) (an external specification): `arcl_*` for the common API, `cyd_*` for CYD-specific tools.',
      '- **Category**: Observation only reads, Action changes state, Control controls execution.',
      '- **Layers**: choose the published layers with `node bridge/index.mjs --mcp-layers=L0,L1` (or `CYD_MCP_LAYERS`). Without a selection every layer is published. Control tools are always published.',
      '- **Time**: CYD is a physical board, not an emulator. The shell, native apps, video and the clock run in real time. `cyd_pause` and its relatives only hold the foreground MicroPython app at `cyd.update()`.',
      '- **Protocol**: the firmware\'s serial protocol commands are named `desktop_*`. They are separate from the MCP tool names.',
    ],
    total: (count) => `${count} tools in all.`,
    groups: { Control: 'Control (always published)', L0: 'L0 Screen and input', L1: 'L1 Apps, settings and files', L2: 'L2 Logs and diagnostics', L3: 'L3 Hardware', L4: 'L4' },
    header: '| Tool | Category | Arguments | Description |',
    separator: ', ',
    none: 'none',
    types: { integer: 'integer', string: 'string', boolean: 'boolean', array: 'array', number: 'number', object: 'object' },
    optional: 'optional',
    default: (value) => `default ${value}`,
    range: (min, max) => (max === undefined ? `≥ ${min}` : `${min}–${max}`),
    length: (min, max) => (min === undefined ? `up to ${max} chars` : `${min}–${max} chars`),
    items: (min, max, fields) => `${min}–${max} items of ${fields}`,
  },
};

export async function listBridgeTools() {
  const transport = new StdioClientTransport({
    command: process.execPath, args: ['bridge/index.mjs'], cwd: ROOT, stderr: 'pipe',
  });
  const client = new Client({ name: 'mcp-tools-doc', version: '0.0.0' }, { capabilities: {} });
  await client.connect(transport);
  try {
    return (await client.listTools()).tools;
  } finally {
    await client.close();
  }
}

const cell = (text) => String(text).replaceAll('|', '\\|').replaceAll('\n', ' ');

function describeArgument(name, schema, required, t) {
  const parts = [t.types[schema.type] ?? schema.type];
  if (schema.enum) parts.push(schema.enum.map((value) => `\`"${value}"\``).join(' / '));
  if (schema.minimum !== undefined || schema.maximum !== undefined) parts.push(t.range(schema.minimum, schema.maximum));
  if (schema.maxLength !== undefined) parts.push(t.length(schema.minLength, schema.maxLength));
  if (schema.pattern) parts.push(`\`${schema.pattern}\``);
  if (schema.type === 'array' && schema.items?.properties) {
    const fields = Object.keys(schema.items.properties)
      .map((field) => (schema.items.required?.includes(field) ? field : `${field}?`)).join(', ');
    parts.push(t.items(schema.minItems, schema.maxItems, `{${fields}}`));
  }
  if (schema.default !== undefined) parts.push(t.default(`\`${JSON.stringify(schema.default)}\``));
  else if (!required) parts.push(t.optional);
  return `\`${name}\`: ${parts.join(t.separator)}`;
}

function describeArguments(inputSchema, t) {
  const properties = inputSchema?.properties ?? {};
  const names = Object.keys(properties);
  if (!names.length) return t.none;
  const required = inputSchema.required ?? [];
  return names.map((name) => describeArgument(name, properties[name], required.includes(name), t)).join('<br>');
}

// Drops the "Machine-specific (cyd). Action, L0. " prefix; the table has columns for it.
const plainDescription = (description) => description.replace(/^[^.]+\. [A-Za-z]+, (?:L\d|any layer)\. /, '');

export function renderToolsDoc(tools, language) {
  const t = TEXT[language];
  const lines = [t.title, '', t.switcher, '', ...t.intro, '', t.total(tools.length), ''];
  const groups = ['Control', ...LAYERS];
  for (const group of groups) {
    const members = tools.filter((tool) => {
      const entry = TOOLS[tool.name];
      return group === 'Control' ? entry.layer === null : entry.layer === group;
    });
    if (!members.length) continue;
    lines.push(`## ${t.groups[group]}`, '', t.header, '| --- | --- | --- | --- |');
    for (const tool of members) {
      // The English text is plain prose, where <app_id> would be read as an HTML tag.
      const description = language === 'ja'
        ? JA[tool.name]
        : plainDescription(tool.description).replaceAll('<', '&lt;').replaceAll('>', '&gt;');
      lines.push(`| \`${tool.name}\` | ${TOOLS[tool.name].category} | ${describeArguments(tool.inputSchema, t)} | ${cell(description)} |`);
    }
    lines.push('');
  }
  return lines.join('\n');
}

if (import.meta.url === pathToFileURL(process.argv[1]).href) {
  const tools = await listBridgeTools();
  for (const [language, url] of Object.entries(OUTPUT)) {
    writeFileSync(url, renderToolsDoc(tools, language));
    console.log(`wrote ${fileURLToPath(url)} (${tools.length} tools)`);
  }
}
