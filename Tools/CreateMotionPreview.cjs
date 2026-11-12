// Preserve the rendered 24-bit colour; per-frame GIF palettes cause visible shimmer.
const fs = require('node:fs');
const path = require('node:path');
const [directory, output, rate = '30'] = process.argv.slice(2);
if (!directory || !output) throw new Error('Usage: node CreateMotionPreview.cjs frames output.png [fps]');
const fps = Number(rate);
if (!Number.isInteger(fps) || fps < 1 || fps > 120) throw new Error('Invalid fps');
const names = fs.readdirSync(directory).filter(n => /^\d{3}\.png$/.test(n)).sort();
if (!names.length) throw new Error('No rendered frames');
const table = new Uint32Array(256);
for (let n = 0; n < 256; n++) {
  let c = n;
  for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
  table[n] = c >>> 0;
}
function chunk(type, bytes) {
  const body = Buffer.concat([Buffer.from(type), bytes]);
  let crc = 0xffffffff;
  for (const byte of body) crc = table[(crc ^ byte) & 255] ^ (crc >>> 8);
  const header = Buffer.alloc(4), tail = Buffer.alloc(4);
  header.writeUInt32BE(bytes.length);
  tail.writeUInt32BE((crc ^ 0xffffffff) >>> 0);
  return Buffer.concat([header, body, tail]);
}
function parse(name) {
  const bytes = fs.readFileSync(path.join(directory, name));
  if (bytes.subarray(0, 8).toString('hex') !== '89504e470d0a1a0a') throw new Error('Invalid PNG');
  let header;
  const data = [];
  for (let p = 8; p < bytes.length;) {
    const size = bytes.readUInt32BE(p), type = bytes.toString('ascii', p + 4, p + 8);
    const content = bytes.subarray(p + 8, p + 8 + size);
    if (type === 'IHDR') header = content;
    if (type === 'IDAT') data.push(content);
    p += size + 12;
  }
  if (!header || !data.length || ![2, 6].includes(header[9])) throw new Error('Expected truecolour PNG');
  return {header, data};
}
const frames = names.map(parse), header = frames[0].header;
if (frames.some(f => !f.header.equals(header))) throw new Error('Frame sizes or formats differ');
const animation = Buffer.alloc(8);
animation.writeUInt32BE(frames.length, 0);
const chunks = [Buffer.from('89504e470d0a1a0a', 'hex'), chunk('IHDR', header), chunk('acTL', animation)];
let sequence = 0;
frames.forEach((frame, index) => {
  const control = Buffer.alloc(26);
  control.writeUInt32BE(sequence++, 0);
  header.copy(control, 4, 0, 8);
  control.writeUInt16BE(1, 20);
  control.writeUInt16BE(fps, 22);
  chunks.push(chunk('fcTL', control));
  for (const pixels of frame.data) {
    if (index === 0) chunks.push(chunk('IDAT', pixels));
    else {
      const number = Buffer.alloc(4);
      number.writeUInt32BE(sequence++);
      chunks.push(chunk('fdAT', Buffer.concat([number, pixels])));
    }
  }
});
chunks.push(chunk('IEND', Buffer.alloc(0)));
fs.writeFileSync(output, Buffer.concat(chunks));
console.log(`${frames.length} truecolour frames at ${fps} fps: ${output}`);
