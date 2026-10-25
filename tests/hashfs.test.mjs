import test from 'node:test';
import assert from 'node:assert/strict';
import { cityHash } from '../tools/read-hashfs.mjs';

test('SCS CityHash variant covers each supported path-length bucket', () => {
  // Reference vectors: scs_tools/src/cityhash.rs (see third-party notices).
  const cases = [
    ['', 0x9ae16a3b2f90404fn],
    ['a', 0x2420662cd003acfan],
    ['abc', 0x3a912f483a4ece31n],
    ['def/city.sii', 0x9a5cb1ebaac98229n],
    ['version.txt', 0x9df28354d1b80f29n],
    ['x'.repeat(17), 0xccde5b4c17d97655n],
    ['x'.repeat(33), 0x83d2f4e954e9c837n],
    ['x'.repeat(40), 0x3613c0a0c0ee3c93n],
    ['automat/01/0123456789abcdef.dds', 0xf567a715b38ab762n],
  ];
  for (const [name, expected] of cases) assert.equal(cityHash(name), expected, name);
});

test('unsupported long paths fail instead of selecting a wrong archive entry', () => {
  assert.throws(() => cityHash('x'.repeat(65)), /exceeds supported/);
});
