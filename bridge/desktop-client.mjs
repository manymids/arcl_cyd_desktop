import { SerialPort } from 'serialport';

export class DesktopClient {
  #port;
  #pending = null;
  #buffer = '';
  #sequence = 0;
  #tail = Promise.resolve();

  constructor({ path = process.env.CYD_DESKTOP_PORT ?? null, baudRate = 115200, timeoutMs = 12000 } = {}) {
    this.path = path;
    this.baudRate = baudRate;
    this.timeoutMs = timeoutMs;
  }

  async open() {
    if (this.#port?.isOpen) return;
    if (!this.path) {
      const ports = await SerialPort.list();
      const cyd = ports.find((port) =>
        port.vendorId?.toLowerCase() === '1a86' && port.productId?.toLowerCase() === '7523');
      if (!cyd) {
        // Falling back to a hard-coded port turned "no board attached" into a
        // confusing failure about whichever port that happened to be.
        throw new Error(
          'No CH340 CYD found. Attach the board or set CYD_DESKTOP_PORT (for example COM6). '
          + `Ports seen: ${ports.map((port) => port.path).join(', ') || 'none'}`);
      }
      this.path = cyd.path;
    }
    this.#buffer = '';
    this.#port = new SerialPort({ path: this.path, baudRate: this.baudRate, autoOpen: false });
    this.#port.on('data', (chunk) => this.#onData(chunk.toString('utf8')));
    await new Promise((resolve, reject) => this.#port.open((error) => error ? reject(error) : resolve()));
  }

  async close() {
    const port = this.#port;
    this.#port = null;
    this.#buffer = '';
    if (!port?.isOpen) return;
    await new Promise((resolve, reject) => port.close((error) => error ? reject(error) : resolve()));
  }

  async setBaudRate(baudRate) {
    if (![115200, 460800, 921600].includes(baudRate))
      throw new RangeError('Unsupported CYD transport baud rate');
    await this.request('desktop_transport_baud', { baud: baudRate });
    const port = this.#port;
    if (!port?.isOpen) throw new Error('Serial port closed while changing baud rate');
    await new Promise((resolve, reject) =>
      port.update({ baudRate }, (error) => error ? reject(error) : resolve()));
    this.baudRate = baudRate;
    // Firmware switches 50 ms after draining its acknowledgement at the old
    // rate. Do not let the caller transmit the next JSONL request too early.
    await new Promise((resolve) => setTimeout(resolve, 120));
  }

  #invalidate(port = this.#port) {
    if (this.#port !== port) return;
    this.#port = null;
    // Drop any partial line: it belongs to a connection that is going away and
    // would otherwise be glued onto the first chunk of the next one.
    this.#buffer = '';
    if (port?.isOpen) port.close(() => {});
  }

  request(command, parameters = {}) {
    const run = this.#tail.then(() => this.#request(command, parameters));
    this.#tail = run.catch(() => undefined);
    return run;
  }

  async #request(command, parameters) {
    await this.open();
    const id = `mcp-${++this.#sequence}`;
    const request = `${JSON.stringify({ id, command, ...parameters })}\n`;
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        this.#pending = null;
        this.#invalidate();
        reject(new Error(`Timed out waiting for ${command} on ${this.path}`));
      }, this.timeoutMs);
      this.#pending = { id, resolve, reject, timer };
      this.#port.write(request, (error) => {
        if (!error) return;
        clearTimeout(timer);
        this.#pending = null;
        this.#invalidate();
        reject(error);
      });
    });
  }

  #onData(chunk) {
    this.#buffer += chunk;
    // A line this long is not a response; keep only the tail so a device
    // babbling without newlines cannot grow the buffer without bound.
    if (this.#buffer.length > 16384) this.#buffer = this.#buffer.slice(-4096);
    for (;;) {
      const newline = this.#buffer.indexOf('\n');
      if (newline < 0) return;
      const line = this.#buffer.slice(0, newline).trim();
      this.#buffer = this.#buffer.slice(newline + 1);
      if (!line.startsWith('{')) continue;
      let response;
      try { response = JSON.parse(line); } catch { continue; }
      if (!this.#pending || response.id !== this.#pending.id) continue;
      const pending = this.#pending;
      this.#pending = null;
      clearTimeout(pending.timer);
      if (response.ok) pending.resolve(response.result);
      else pending.reject(new Error(`${response.error?.code ?? 'DEVICE_ERROR'}: ${response.error?.message ?? 'unknown error'}`));
    }
  }
}
