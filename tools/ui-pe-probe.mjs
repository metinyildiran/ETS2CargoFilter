// Read-only research helper. Addresses are preferred-image VAs, not runtime VAs.
// RIP candidates are byte-pattern leads, not validated instruction references.
import fs from 'node:fs';
const [exe, operation, arg] = process.argv.slice(2);
const b = fs.readFileSync(exe);
const pe = b.readUInt32LE(0x3c), opt = pe + 24;
const base = Number(b.readBigUInt64LE(opt + 24));
const sections = [];
for (let i = 0, p = opt + b.readUInt16LE(pe + 20); i < b.readUInt16LE(pe + 6); i++, p += 40) {
  sections.push({ name: b.toString('ascii', p, p + 8).replace(/\0/g, ''), rva: b.readUInt32LE(p + 12), raw: b.readUInt32LE(p + 20), size: b.readUInt32LE(p + 16) });
}
const raw = rva => { const s = sections.find(s => rva >= s.rva && rva < s.rva + s.size); if (!s) throw Error('Unmapped RVA ' + rva.toString(16)); return s.raw + rva - s.rva; };
const va = n => '0x' + (base + n).toString(16);
const exceptionRva = b.readUInt32LE(opt + 112 + 3 * 8), exceptionSize = b.readUInt32LE(opt + 116 + 3 * 8);
const records = [];
for (let p = raw(exceptionRva), end = p + exceptionSize; p < end; p += 12) records.push({ begin: b.readUInt32LE(p), end: b.readUInt32LE(p + 4), unwind: b.readUInt32LE(p + 8) });
function parent(record, seen = new Set()) {
  if (seen.has(record.unwind)) throw Error('Unwind cycle');
  seen.add(record.unwind);
  const p = raw(record.unwind);
  if (!((b[p] >> 3) & 4)) return record.begin;
  const chained = p + 4 + ((b[p + 2] + 1) & ~1) * 2;
  return parent({ begin: b.readUInt32LE(chained), end: b.readUInt32LE(chained + 4), unwind: b.readUInt32LE(chained + 8) }, seen);
}
const target = operation === 'bytes' ? 0 : Number(BigInt(arg));
if (operation === 'bytes') {
  if (!/^(?:[0-9a-f]{2})+$/i.test(arg)) throw Error('Expected even hex-byte pattern');
  const needle = Buffer.from(arg, 'hex');
  const code = sections.find(s => s.name === '.text');
  for (let p = code.raw; p <= code.raw + code.size - needle.length; p++) {
    if (!b.subarray(p, p + needle.length).equals(needle)) continue;
    const rva = code.rva + p - code.raw;
    const record = records.find(r => r.begin <= rva && rva < r.end);
    console.log(va(rva), record ? va(parent(record)) : 'no-unwind');
  }
} else if (operation === 'function') {
  const rva = target - base, item = records.find(r => r.begin <= rva && rva < r.end);
  if (!item) throw Error('No unwind record');
  const root = parent(item);
  console.log(JSON.stringify({ root: va(root), ranges: records.filter(r => parent(r) === root).map(r => ({ begin: va(r.begin), end: va(r.end) })) }, null, 2));
} else if (operation === 'rip') {
  const code = sections.find(s => s.name === '.text');
  for (let p = code.raw; p < code.raw + code.size - 4; p++) {
    if (base + code.rva + p - code.raw + 4 + b.readInt32LE(p) === target) console.log(va(code.rva + p - code.raw));
  }
} else if (operation === 'pointers') {
  const needle = Buffer.alloc(8); needle.writeBigUInt64LE(BigInt(target));
  for (const s of sections) for (let p = s.raw; p < s.raw + s.size - 8; p += 8) if (b.subarray(p, p + 8).equals(needle)) console.log(va(s.rva + p - s.raw));
} else if (operation === 'table') {
  const p = raw(target - base);
  for (let i = 0; i < 0x2d0; i += 8) console.log('+' + i.toString(16), '0x' + b.readBigUInt64LE(p + i).toString(16));
} else throw Error('Expected function, rip, pointers, table or bytes');
