// Read-only HashFS v2 text extraction for short SCS resource paths.
// Format: https://github.com/Wilps93/scs_tools/blob/main/FORMAT.md
// Hash: CityHash 2011 (SCS variant). Long paths fail explicitly.
import fs from 'node:fs';
import path from 'node:path';
import zlib from 'node:zlib';
import { fileURLToPath } from 'node:url';

const mask = (1n << 64n) - 1n;
const u = x => x & mask;
const k0 = 0xc3a5c85c97cb3127n, k1 = 0xb492b66fbe98f273n;
const k2 = 0x9ae16a3b2f90404fn, k3 = 0xc949d7c7509e6557n;
const rotate = (x, n) => { x = u(x); n = BigInt(n); return n ? u((x >> n) | (x << (64n - n))) : x; };
const mix = x => { x = u(x); return x ^ (x >> 47n); };
const h16 = (a, b) => { const m = 0x9ddfea08eb382d69n; a = mix(u((u(a) ^ u(b)) * m)); return u(mix(u((u(b) ^ a) * m)) * m); };
export function cityHash(text) {
  const s = Buffer.from(text), n = s.length, len = BigInt(n);
  const f = p => s.readBigUInt64LE(p);
  if (n <= 16) {
    if (n > 8) return u(h16(f(0), rotate(f(n - 8) + len, n)) ^ f(n - 8));
    if (n >= 4) return h16(len + (BigInt(s.readUInt32LE(0)) << 3n), BigInt(s.readUInt32LE(n - 4)));
    if (n) return u(mix(u(BigInt(s[0] + (s[n >> 1] << 8)) * k2) ^ u(BigInt(n + (s[n - 1] << 2)) * k3)) * k2);
    return k2;
  }
  if (n <= 32) {
    const a = u(f(0) * k1), b = f(8), c = u(f(n - 8) * k2), d = u(f(n - 16) * k0);
    return h16(rotate(a - b, 43) + rotate(c, 30) + d, a + rotate(b ^ k3, 20) - c + len);
  }
  if (n <= 64) {
    let z = f(24), a = u(f(0) + u((len + f(n - 16)) * k0));
    let b = rotate(a + z, 52), c = rotate(a, 37);
    a = u(a + f(8)); c = u(c + rotate(a, 7)); a = u(a + f(16));
    const vf = u(a + z), vs = u(b + rotate(a, 31) + c);
    a = u(f(16) + f(n - 32)); z = f(n - 8); b = rotate(a + z, 52); c = rotate(a, 37);
    a = u(a + f(n - 24)); c = u(c + rotate(a, 7)); a = u(a + f(n - 16));
    const wf = u(a + z), ws = u(b + rotate(a, 31) + c);
    return u(mix(u(mix(u((vf + ws) * k2 + (wf + vs) * k0)) * k0 + vs)) * k2);
  }
  throw new Error(`Resource path exceeds supported 64 bytes: ${text}`);
}

export function openArchive(archive) {
  const fd = fs.openSync(archive, 'r');
  const read = (offset, size) => {
    const b = Buffer.alloc(size);
    if (fs.readSync(fd, b, 0, size, offset) !== size) throw new Error('Truncated archive');
    return b;
  };
  const header = read(0, 52);
  if (header.toString('ascii', 0, 4) !== 'SCS#' || header.readUInt16LE(4) !== 2 || header.toString('ascii', 8, 12) !== 'CITY') throw new Error('Requires HashFS v2/CITY');
  if (header.readUInt16LE(6)) throw new Error('Salted archives are unsupported');
  const entries = zlib.inflateSync(read(Number(header.readBigUInt64LE(28)), header.readUInt32LE(16)));
  const metadata = zlib.inflateSync(read(Number(header.readBigUInt64LE(36)), header.readUInt32LE(24)));
  if (entries.length !== header.readUInt32LE(12) * 16 || metadata.length !== header.readUInt32LE(20) * 4) throw new Error('Invalid table sizes');
  const index = new Map();
  for (let p = 0; p < entries.length; p += 16) index.set(entries.readBigUInt64LE(p), p);
  function get(name) {
    const p = index.get(cityHash(name));
    if (p === undefined) return null;
    const start = entries.readUInt32LE(p + 8), count = entries.readUInt16LE(p + 12);
    for (let i = 0; i < count; i++) {
      const desc = metadata.readUInt32LE((start + i) * 4);
      if (!((desc >>> 24) & 0x80)) continue;
      const off = (desc & 0xffffff) * 4;
      const packed = metadata.readUInt32LE(off), size = metadata.readUInt32LE(off + 4) & 0xffffff;
      const codec = (packed >>> 24) & 0xf0;
      const bytes = read(metadata.readUInt32LE(off + 12) * 16, packed & 0xffffff);
      const data = codec === 0 ? bytes : codec === 0x10 ? zlib.inflateSync(bytes) : null;
      if (!data) throw new Error(`Unsupported codec ${codec} for ${name}`);
      if (data.length !== size) throw new Error(`Length mismatch for ${name}`);
      return { data, directory: !!(entries.readUInt16LE(p + 14) & 1) };
    }
    throw new Error(`No data part for ${name}`);
  }
  return { get, close: () => fs.closeSync(fd) };
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  const [archive, resource, destination] = process.argv.slice(2);
  if (!archive || resource === undefined) throw new Error('Usage: node tools/read-hashfs.mjs archive.scs resource [new-output-file]');
  const reader = openArchive(archive);
  try {
    const entry = reader.get(resource);
    if (!entry) throw new Error(`Resource not found: ${resource}`);
    if (entry.directory) {
      const n = entry.data.readUInt32LE(0); let p = 4 + n;
      for (let i = 0; i < n; i++) { const length = entry.data[4 + i]; console.log(entry.data.toString('utf8', p, p + length)); p += length; }
    } else if (destination) {
      fs.mkdirSync(path.dirname(destination), { recursive: true });
      fs.writeFileSync(destination, entry.data, { flag: 'wx' });
      console.log(`${resource} -> ${destination} (${entry.data.length} bytes)`);
    } else process.stdout.write(entry.data);
  } finally { reader.close(); }
}
