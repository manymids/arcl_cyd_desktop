export function acceptanceIds(timestamp = Date.now()) {
  const suffix = timestamp.toString(36);
  return { appId: `p10a${suffix}`, shortcutId: `p10s${suffix}` };
}

export function acceptanceSource() {
  return [
    'import cyd',
    "cyd.title('P10 ACCEPT')",
    "cyd.text(24, 82, 'MCP E2E RUNNING')",
    "cyd.text(24, 104, 'Home ends this app')",
    "cyd.log('Ready')",
    'cyd.run()',
    '',
  ].join('\n');
}
