import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { buildUi, generateUi, parseBlocks, readVanillaUi, defaultGameDir,
  rootName, controlName } from '../tools/build-ui.mjs';
import { generateLocalization, supportedLocales } from '../tools/build-localization.mjs';

const project = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const gameDir = process.env.ETS2_GAME_DIR || defaultGameDir;
const available = fs.existsSync(path.join(gameDir, 'base_share.scs'));
const integration = { skip: available ? false : 'Set ETS2_GAME_DIR to the supported game installation' };
const original = available ? readVanillaUi(gameDir) : null;
const generated = available ? generateUi(original) : null;
const originals = available ? parseBlocks(original.toString('utf8')) : [];
const blocks = available ? parseBlocks(generated.toString('utf8')) : [];
const byName = new Map(blocks.map(block => [block.name, block]));
const property = (block, key) => {
  const line = block.text.split(/\r?\n/).find(line => line.startsWith(` ${key}: `));
  assert.ok(line, `${block.name} has ${key}`);
  return line.slice(key.length + 3);
};

test('unrecognized resource is rejected instead of patching another game version', () => {
  assert.throws(() => generateUi(Buffer.from('SiiNunit\n{\n}\n')), /Unsupported vanilla/);
});

test('all original controls and their bytes are preserved', integration, () => {
  assert.equal(blocks.length, originals.length + 3);
  for (const block of originals) {
    if (block.name === rootName) continue;
    assert.equal(byName.get(block.name)?.text, block.text, block.name);
  }
  // Explicitly protect the user's existing sorting, search, results and filter.
  for (const [name, id] of [['_nameless.0081036826', '400'], ['_nameless._.search', '402'],
    ['_nameless.008103685', '200'], ['_nameless._.cargo.top.button', '470'],
    ['_nameless._.cargo.group', '472']]) assert.equal(property(byName.get(name), 'id'), id);
  assert.equal(property(byName.get('_nameless.008103685'), 'child_path'), '"/ui/job_view.sii"');
  const root = byName.get(rootName).text.replace(' my_children: 24', ' my_children: 23')
    .replace(/^ my_children\[23\]: [^\r\n]+\r?\n/m, '');
  assert.equal(root, originals.find(block => block.name === rootName).text);
});

test('unique units have consistent parent/child links and declared counts', integration, () => {
  assert.equal(byName.size, blocks.length, 'unit names are unique');
  const referenced = new Set();
  for (const block of blocks) {
    const links = [...block.text.matchAll(/^ my_children\[(\d+)\]: ([\w.]+)\r?$/gm)];
    const count = block.text.match(/^ my_children: (\d+)\r?$/m);
    if (count) assert.equal(links.length, Number(count[1]), `${block.name} child count`);
    links.forEach((link, index) => {
      assert.equal(Number(link[1]), index, 'contiguous child indices');
      assert.ok(!referenced.has(link[2]), `${link[2]} is linked only once`);
      referenced.add(link[2]);
      assert.ok(byName.has(link[2]), `child exists: ${link[2]}`);
      assert.equal(property(byName.get(link[2]), 'my_parent'), block.name);
    });
  }
  assert.equal(property(byName.get(rootName), 'my_children'), '24');
  assert.equal(property(byName.get(rootName), 'my_children[23]'), controlName);
});

test('new selector uses native styling and intended dropdown/list/frame geometry', integration, () => {
  assert.equal((generated.toString('utf8').match(/^ id: 9470\r?$/gm) || []).length, 1);
  const dropdown = byName.get(controlName);
  const list = byName.get('_nameless.cf.list');
  const frame = byName.get('_nameless.cf.frame');
  assert.equal(dropdown.type, 'ui::droplist_common');
  assert.equal(property(dropdown, 'look_template'), 'btn.listbox.sorting');
  assert.equal(property(list, 'look_template'), 'btn.list.item.sorting');
  assert.equal(property(frame, 'look_template'), 'txt.frame');
  for (const [block, left, right, top, bottom] of [[dropdown,154,474,840,810],
    [list,155,474,809,359], [frame,155,474,809,359]]) {
    for (const [side, value] of [['l',left],['r',right],['t',top],['b',bottom]]) {
      assert.equal(Number(property(block, `coords_${side}`)), value);
      if (block !== frame) assert.equal(Number(property(block, `area_${side}`)), value);
    }
  }
  assert.equal(property(list, 'my_children'), '1');
  assert.equal(property(frame, 'value'), '""');
  // Enable the native count-height layout for only our list. Its frame follows
  // that layout; do not bake the user's current trailer count into the resource.
  assert.equal(property(list, 'max_list_height'), '450');
  assert.equal(property(list, 'item_height'), '25');
  assert.equal(property(list, 'item_gap'), '0');
  assert.equal(property(byName.get('_nameless._.ui.comp.list'), 'max_list_height'), '0');
});

test('development metadata declares optional Convoy UI and exact native requirement', () => {
  const manifest = fs.readFileSync(path.join(project, 'mod', 'manifest.sii'), 'utf8');
  assert.match(manifest, /package_version: "0\.1\.0-dev"/);
  assert.match(manifest, /compatible_versions\[\]: "1\.61\.\*"/);
  assert.match(manifest, /mp_mod_optional: true/);
  assert.match(manifest, /description_file: "mod_description\.txt"/);
  assert.match(generateLocalization().get('mod_description.txt').toString('utf8'), /1\.61\.1\.9/);
});

test('build writes only loose UI and authored metadata and is repeatable', integration, () => {
  const outputDir = fs.mkdtempSync(path.join(os.tmpdir(), 'ets2-ui-test-'));
  try {
    buildUi({ gameDir, outputDir });
    buildUi({ gameDir, outputDir });
    assert.deepEqual(fs.readdirSync(outputDir).sort(), ['locale','manifest.sii','mod_description.txt','ui',
      ...supportedLocales.map(locale => `mod_description.${locale}.txt`)].sort());
    assert.deepEqual(fs.readdirSync(path.join(outputDir, 'ui')), ['cargo_market.sii']);
    assert.deepEqual(fs.readFileSync(path.join(outputDir, 'ui/cargo_market.sii')), generated);
    for (const name of ['manifest.sii'])
      assert.deepEqual(fs.readFileSync(path.join(outputDir, name)), fs.readFileSync(path.join(project, 'mod', name)));
    for (const [name, bytes] of generateLocalization())
      assert.deepEqual(fs.readFileSync(path.join(outputDir, name)), bytes, name);
  } finally {
    // Only remove the exact directory returned by mkdtemp, beneath the temp root.
    assert.equal(path.dirname(path.resolve(outputDir)), path.resolve(os.tmpdir()));
    assert.ok(path.basename(outputDir).startsWith('ets2-ui-test-'));
    fs.rmSync(outputDir, { recursive: true, force: true });
  }
});
