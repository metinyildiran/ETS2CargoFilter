import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { generateLocalization, readTranslations, validateTranslations,
  supportedLocales, messageKeys, moduleName } from '../tools/build-localization.mjs';
import { openArchive } from '../tools/read-hashfs.mjs';
import { defaultGameDir } from '../tools/build-ui.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const catalog = readTranslations();
const gameDir = process.env.ETS2_GAME_DIR || defaultGameDir;
const archivePath = path.join(gameDir, 'locale.scs');
const integration = { skip: fs.existsSync(archivePath) ? false : 'Set ETS2_GAME_DIR to the supported game installation' };

test('every supported locale contains every message and a localized description', () => {
  assert.equal(supportedLocales.length, 43);
  validateTranslations(catalog);
  const files = generateLocalization(catalog);
  assert.equal(files.size, 87);
  assert.deepEqual(files.get('mod_description.txt'), files.get('mod_description.en_gb.txt'));
  for (const locale of supportedLocales) {
    const module = files.get(`locale/${locale}/${moduleName}`).toString('utf8');
    const entries = [...module.matchAll(/ key\[\]: "ets2cf_(\w+)"\n val\[\]: ("(?:[^"\\]|\\.)*")/gu)];
    assert.equal(entries.length, messageKeys.length, locale);
    for (const [i, [, key, quoted]] of entries.entries()) {
      assert.equal(key, messageKeys[i]);
      assert.equal(JSON.parse(quoted), catalog[locale][key], `${locale}/${key}`);
    }
    assert.match(files.get(`mod_description.${locale}.txt`).toString('utf8'), /ETS2CargoFilter\.dll/);
    if (!locale.startsWith('en_')) {
      for (const key of messageKeys) assert.notEqual(catalog[locale][key], catalog.en_gb[key], `${locale}/${key} must be translated`);
    }
  }
});

test('incomplete translations and malformed text fail instead of publishing raw keys', () => {
  for (const mutate of [c => delete c.fr_ca, c => delete c.ja_jp.busy,
    c => { c.tr_tr.all_trailers = ''; }, c => { c.zh_cn.attached = '\ufffd'; },
    c => { c.de_de.all_trailers = '@@unresolved@@'; },
    c => { c.en_gb.requirements = 'Wrong version'; }]) {
    const copy = structuredClone(catalog); mutate(copy);
    assert.throws(() => generateLocalization(copy));
  }
});

test('all native UI message tokens have a catalog entry', () => {
  const header = fs.readFileSync(path.join(root, 'src/ui_text.hpp'), 'utf8');
  const tokens = [...header.matchAll(/"@@ets2cf_(\w+)@@"/g)].map(m => m[1]);
  assert.deepEqual(tokens.sort(), [...messageKeys].sort());
  const adapter = fs.readFileSync(path.join(root, 'src/game_adapter.cpp'), 'utf8');
  assert.doesNotMatch(adapter, /screen\.status\s*=\s*"/);
  assert.doesNotMatch(adapter, /"[^"\n]*[çğıöşüÇĞİÖŞÜ][^"\n]*"/u);
});

test('locale coverage and built-in body names match the installed game', integration, () => {
  const archive = openArchive(archivePath);
  try {
    const bytes = archive.get('locale').data, count = bytes.readUInt32LE(0);
    let p = 4 + count;
    const locales = [];
    for (let i = 0; i < count; i++) {
      const name = bytes.toString('utf8', p, p + bytes[4 + i]); p += bytes[4 + i];
      if (/^\/[a-z]{2}_[a-z]{2}$/.test(name)) locales.push(name.slice(1));
    }
    assert.deepEqual(locales.sort(), [...supportedLocales].sort());
    const source = fs.readFileSync(path.join(root, 'src/native_ownership.cpp'), 'utf8');
    const types = [...source.match(/types\[\] = \{([\s\S]+?)\};/)[1].matchAll(/"(\w+)"/g)].map(m => m[1]);
    assert.equal(types.length, 19);
    for (const locale of locales) {
      const original = archive.get(`locale/${locale}/localization.sui`).data.toString('utf8');
      for (const type of types) assert.ok(original.includes(`key[]: "${type}"`), `${locale}/${type}`);
      for (const key of messageKeys) assert.ok(!original.includes(`key[]: "ets2cf_${key}"`), 'custom key must not shadow native key');
    }
  } finally { archive.close(); }
});
