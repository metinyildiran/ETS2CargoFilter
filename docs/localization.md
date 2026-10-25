# Localization

The mod uses the game's selected language. There is no separate language
setting, profile parsing, Windows-language inference or extra runtime hook.

`localization/locales.json` records the 43 locale directories in ETS2 1.61.1.9's
`locale.scs`. `localization/translations.json` is the authored source for 14 UI
messages and two description paragraphs per locale. British/American English,
European/Canadian French and European/Latin American Spanish have separate
complete entries. Serbian Latin/Cyrillic and Simplified/Traditional Chinese
are also separate. `pl_si` is Silesian.

`tools/build-localization.mjs` validates the catalog and emits:

- `locale/<locale>/local_module.ets2cargo_filter.sii` (43 files).
- `mod_description.<locale>.txt` (43 files).
- `mod_description.txt` (English description fallback).

The `.scs` package includes all of these files. The DLL supplies
`@@ets2cf_*@@` tokens to the native UI. Nineteen built-in trailer body names
use the game's own `@@body_type@@` translations, verified in every installed
locale. Plate identifiers are preserved. Third-party body types without a
known game translation retain their readable custom identifier.

Every locale must contain every key: SCS does not automatically fall back to
English for missing mod localization tokens. Keys use the `ets2cf_` prefix,
and no base-game localization file or key is overridden. The product name
`ETS2 Cargo Filter`, file names and diagnostic logs remain stable across locales.
Logs are diagnostic output, not translated player-facing UI.

References:
- [SCS localization modules](https://modding.scssoft.com/wiki/Games/ETS2/Modding_guides/1.33#Mods_localization_support)
- [SCS localized mod descriptions](https://modding.scssoft.com/wiki/Documentation/Engine/Mod_manager#Localized_description)

## Validation and editing

Edit the locale's named keys in `localization/translations.json`, then run:

```powershell
node --test tests/hashfs.test.mjs tests/ui-package.test.mjs tests/localization.test.mjs
./tools/build-native.ps1
./tools/package-dev.ps1 -SkipBuild
```

Validation rejects missing/extra keys or locales, empty text, broken Unicode,
embedded UI markup, missing version/plugin requirements and untranslated English
UI messages outside English locales. Integration tests compare coverage with the
installed game and verify all built-in trailer-name keys. Generated SII values
round-trip to the authored UTF-8 strings, including Cyrillic, Georgian and CJK.

These are authored translations, not certified native-speaker reviews. Automated
coverage checks do not establish linguistic perfection or in-game rendering for
every language. Runtime checks should cover the selector, attached marker, a
status message and Mod Manager description in several scripts and verify language
switching after reopening the market. Check `game.log.txt` for module load errors.

On 25 October 2026, the user confirmed translated UI in several game languages.
The languages were not specified. This confirms the localization path works in
those observed cases; it does not establish visual coverage of all 43 locales,
status messages or Mod Manager descriptions.
