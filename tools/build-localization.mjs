import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
export const supportedLocales = JSON.parse(fs.readFileSync(path.join(root, 'localization/locales.json'), 'utf8'));
export const messageKeys = [
  'all_trailers', 'attached', 'unknown_type', 'selector_failed', 'ownership_failed',
  'refresh_failed', 'ownership_changed', 'reopen_market', 'offers_failed',
  'prepare_failed', 'attach_failed', 'busy', 'offer_changed', 'unavailable',
];
export const moduleName = 'local_module.ets2cargo_filter.sii';
export const readTranslations = () => JSON.parse(fs.readFileSync(path.join(root, 'localization/translations.json'), 'utf8'));

export function validateTranslations(catalog) {
  const keys = [...messageKeys, 'description', 'requirements'].sort();
  if (JSON.stringify(Object.keys(catalog).sort()) !== JSON.stringify([...supportedLocales].sort()))
    throw new Error('Translations must cover exactly all supported game locales');
  for (const locale of supportedLocales) {
    const messages = catalog[locale];
    if (!messages || JSON.stringify(Object.keys(messages).sort()) !== JSON.stringify(keys))
      throw new Error(`Missing or unexpected translation key in ${locale}`);
    for (const [key, value] of Object.entries(messages)) {
      if (typeof value !== 'string' || !value.trim() || value !== value.trim()
          || /[\u0000-\u001f\u007f\ufffd]/u.test(value) || /@@|[<>]/u.test(value))
        throw new Error(`Invalid translation: ${locale}/${key}`);
      if (Buffer.byteLength(value, 'utf8') > 4096)
        throw new Error(`Translation too large: ${locale}/${key}`);
    }
    // The engine has no automatic English fallback for mod localization keys.
    // Locale coverage is therefore mandatory, not silently substituted at build.
    for (const literal of ['1.61.1.9', 'Windows', 'ETS2CargoFilter.dll', '.scs', 'Convoy'])
      if (!messages.requirements.includes(literal)) throw new Error(`Missing requirement ${literal}: ${locale}`);
  }
}

export function generateLocalization(catalog = readTranslations()) {
  validateTranslations(catalog);
  const files = new Map();
  for (const locale of supportedLocales) {
    const messages = catalog[locale];
    const entries = messageKeys.map(key => ` key[]: "ets2cf_${key}"\n val[]: ${JSON.stringify(messages[key])}`);
    files.set(`locale/${locale}/${moduleName}`, Buffer.from(
      `SiiNunit\n{\nlocalization_db : .localization\n{\n${entries.join('\n\n')}\n}\n}\n`, 'utf8'));
    const description = Buffer.from(`ETS2 Cargo Filter\n\n${messages.description}\n\n${messages.requirements}\n`, 'utf8');
    files.set(`mod_description.${locale}.txt`, description);
    if (locale === 'en_gb') files.set('mod_description.txt', description);
  }
  return files;
}
