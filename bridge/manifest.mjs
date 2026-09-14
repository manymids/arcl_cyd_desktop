// PC-side check of an app's manifest.json, so a bad manifest is refused before
// anything is uploaded rather than making the app vanish from Scripts on the
// device.
//
// The schema lives in firmware/cyd_desktop_board/modules/_manifest.py. This is
// the same rules in JavaScript; both implementations are held to
// firmware/cyd_desktop_board/tests/manifest-cases.json. The API level is read
// out of _manifest.py so there is only one number to bump.

import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';

const MANIFEST_MODULE = fileURLToPath(
  new URL('../firmware/cyd_desktop_board/modules/_manifest.py', import.meta.url));

export const KEYS = ['id', 'title', 'entry', 'api', 'version'];
export const MAX_MANIFEST_BYTES = 2048;

export const API_LEVEL = (() => {
  const match = /^API_LEVEL\s*=\s*(\d+)\s*$/m.exec(readFileSync(MANIFEST_MODULE, 'utf8'));
  if (!match) throw new Error(`API_LEVEL not found in ${MANIFEST_MODULE}`);
  return Number(match[1]);
})();

const COMPONENT = /^[A-Za-z0-9_.-]{1,24}$/;
const TITLE = /^[A-Za-z0-9 _.-]{1,12}$/;

export function isSafeComponent(value) {
  return typeof value === 'string' && COMPONENT.test(value) && !value.includes('..');
}

export function isSafeTitle(value) {
  return typeof value === 'string' && TITLE.test(value);
}

// Errors carry the offending field first, as "<field>: <reason>", matching the
// device so logs from either side read the same.
export class ManifestError extends Error {
  constructor(field, reason) {
    super(`${field}: ${reason}`);
    this.field = field;
  }
}

export function validateManifest(appId, data) {
  if (data === null || data === undefined) return { title: appId.slice(0, 12), entry: 'main.py' };
  if (typeof data !== 'object' || Array.isArray(data)) {
    throw new ManifestError('shape', 'manifest must be a JSON object');
  }
  for (const key of Object.keys(data)) {
    if (!KEYS.includes(key)) throw new ManifestError('key', `unknown key ${key.slice(0, 16)}`);
  }
  if ('id' in data && data.id !== appId) throw new ManifestError('id', 'does not match the directory name');
  if (!isSafeTitle(data.title)) throw new ManifestError('title', 'need 1-12 of A-Z 0-9 space _.-');
  const entry = data.entry ?? 'main.py';
  if (!isSafeComponent(entry) || !entry.endsWith('.py')) throw new ManifestError('entry', 'must be a .py file name');
  const api = data.api ?? 1;
  if (typeof api !== 'number' || !Number.isInteger(api) || api < 1) {
    throw new ManifestError('api', 'must be a positive integer');
  }
  if (api > API_LEVEL) throw new ManifestError('api', `needs ${api}, firmware has ${API_LEVEL}`);
  if (data.version !== undefined && data.version !== null
      && !(typeof data.version === 'string' && data.version.length > 0 && data.version.length <= 16)) {
    throw new ManifestError('version', 'must be 1-16 characters');
  }
  return { title: data.title, entry };
}

export function parseManifest(appId, text) {
  if (Buffer.byteLength(text, 'utf8') > MAX_MANIFEST_BYTES) {
    throw new ManifestError('shape', `larger than ${MAX_MANIFEST_BYTES} bytes`);
  }
  let data;
  try {
    data = JSON.parse(text);
  } catch {
    throw new ManifestError('json', 'manifest.json is not valid JSON');
  }
  return validateManifest(appId, data);
}
