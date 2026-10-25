// Build the UI overlay from the user's installed, exact vanilla resource.
// No game files or profiles are modified; output remains a loose development mod.
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import { fileURLToPath } from 'node:url';
import { openArchive } from './read-hashfs.mjs';

const projectRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
export const defaultGameDir = 'D:\\SteamLibrary\\steamapps\\common\\Euro Truck Simulator 2';
export const expectedUiSha256 = '4120fa091af99c7fb047f91758aa9bfd9e334b0ab1a8177bbcb8f6e6eb63eebd';
export const resourcePath = 'ui/cargo_market.sii';
export const rootName = '_nameless.008103681';
export const controlName = '_nameless.cf.trailer';

// This is deliberately a bounded parser for the hash-pinned, flat vanilla file.
// Keep every unmodified byte rather than reserializing all SII fields.
export function parseBlocks(text) {
  return [...text.matchAll(/^([\w:]+) : ([\w.]+) \{\r?\n[\s\S]*?^\}/gm)]
    .map(match => ({ type: match[1], name: match[2], text: match[0], index: match.index }));
}

export function generateUi(source) {
  const bytes = Buffer.isBuffer(source) ? source : Buffer.from(source);
  const hash = crypto.createHash('sha256').update(bytes).digest('hex');
  if (hash !== expectedUiSha256)
    throw new Error(`Unsupported vanilla Cargo Market UI: SHA-256 ${hash}; expected ${expectedUiSha256}`);
  const text = bytes.toString('utf8');
  const newline = text.includes('\r\n') ? '\r\n' : '\n';
  const blocks = parseBlocks(text);
  const byName = new Map(blocks.map(block => [block.name, block]));
  const names = new Map([
    ['_nameless.0081036826', controlName],
    ['_nameless._.ui.comp.list', '_nameless.cf.list'],
    ['_nameless.0081036828', '_nameless.cf.frame'],
  ]);
  const clones = [...names.keys()].map(name => {
    const original = byName.get(name);
    if (!original) throw new Error(`Missing vanilla sort component: ${name}`);
    let clone = original.text.replace(/_nameless\.[\w.]+/g, token => names.get(token) ?? token);
    // Widen the cloned dropdown to 320 units; keep the list's one-unit inset.
    clone = clone.replace(/^( (?:coords|area)_[lr]: )(943|944|1177)(?=\r?$)/gm,
      (_, field, value) => field + ({ 943: 154, 944: 155, 1177: 474 })[value]);
    if (name === '_nameless.0081036826') clone = clone.replace(/^ id: 400(?=\r?$)/m, ' id: 9470');
    // A positive cap enables the engine's content-height layout, including the
    // frame child. Zero leaves the vanilla sorting list's fixed 450-unit box.
    // Native refresh already runs whenever the plugin adds an owned trailer.
    if (name === '_nameless._.ui.comp.list') {
      if (!/^ max_list_height: 0(?=\r?$)/m.test(clone))
        throw new Error('Unexpected vanilla dropdown height policy');
      clone = clone.replace(/^ max_list_height: 0(?=\r?$)/m, ' max_list_height: 450');
    }
    return clone;
  });
  const root = byName.get(rootName);
  if (!root || !root.text.includes(' my_children: 23')) throw new Error('Unexpected vanilla root');
  const patchedRoot = root.text.replace(' my_children: 23', ' my_children: 24')
    .replace(/( my_children\[22\]: [^\r\n]+)(\r?\n)/,
      `$1$2 my_children[23]: ${controlName}${newline}`);
  const patched = text.slice(0, root.index) + patchedRoot + text.slice(root.index + root.text.length);
  const closing = patched.lastIndexOf('}');
  return Buffer.from(patched.slice(0, closing) + clones.join(newline + newline)
    + newline + newline + patched.slice(closing));
}

export function readVanillaUi(gameDir = defaultGameDir) {
  const archive = openArchive(path.join(gameDir, 'base_share.scs'));
  try {
    const entry = archive.get(resourcePath);
    if (!entry || entry.directory) throw new Error(`Missing file ${resourcePath} in base_share.scs`);
    return entry.data;
  } finally { archive.close(); }
}

export function buildUi({ gameDir = defaultGameDir, outputDir = path.join(projectRoot, 'build', 'mod') } = {}) {
  const ui = generateUi(readVanillaUi(gameDir));
  // Read authored metadata before creating output, so missing inputs fail cleanly.
  const metadata = ['manifest.sii', 'mod_description.txt'].map(name =>
    [name, fs.readFileSync(path.join(projectRoot, 'mod', name))]);
  fs.mkdirSync(path.join(outputDir, 'ui'), { recursive: true });
  fs.writeFileSync(path.join(outputDir, resourcePath), ui);
  for (const [name, bytes] of metadata) fs.writeFileSync(path.join(outputDir, name), bytes);
  return { outputDir: path.resolve(outputDir), uiBytes: ui.length };
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  const options = {};
  const args = process.argv.slice(2);
  for (let i = 0; i < args.length; ++i) {
    if (args[i] === '--help') {
      console.log('Usage: node tools/build-ui.mjs [--game-dir PATH] [--output-dir PATH]');
      process.exit(0);
    }
    const key = { '--game-dir': 'gameDir', '--output-dir': 'outputDir' }[args[i]];
    if (!key || !args[i + 1] || args[i + 1].startsWith('--'))
      throw new Error(`Unknown or incomplete argument: ${args[i]}`);
    options[key] = path.resolve(args[++i]);
  }
  const result = buildUi(options);
  console.log(`Development UI written to ${result.outputDir} (${result.uiBytes} bytes); not installed or packaged.`);
}
