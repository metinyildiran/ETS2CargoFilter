# Workshop publishing

Workshop distributes the UI and translations only. The native Windows DLL must
be downloaded from GitHub Releases and installed in the game's
`bin/win_x64/plugins` folder. Keep this requirement visible in the title, cover
and description. Do not include the DLL inside the Workshop content package.

Current release: **v0.1.0-preview.4 / r4**, exact ETS2 **1.61.1.9**.
Convoy session testing remains pending.

## Build

```powershell
./tools/build-workshop-art.ps1
./tools/package-workshop.ps1
node --test tests/hashfs.test.mjs tests/ui-package.test.mjs tests/localization.test.mjs
```

Use `-SkipBuild` only when the native release binary is already current and
tested. Packaging refuses an existing Workshop content/release staging folder
to prevent stale files from leaking into a later release; archive the old output
before rebuilding.

Outputs:

- `dist/workshop/content/versions.sii`: exact game-version mapping.
- `dist/workshop/content/ets2_16119/`: UI, 43 locales, localized descriptions,
  icon and Workshop manifest (without display_name/compatible_versions).
- `dist/workshop/preview.jpg`: 640 x 360 JPG, below 1 MB.
- `dist/ETS2CargoFilter-1.61.1.9-r4.zip`: manual UI + plugin + instructions/licenses.
- `dist/SHA256SUMS-r4.txt`: ZIP digest; individual digests are inside the ZIP.

The authored mod icon is 276 x 162 JPG. Artwork is generated from simple original
geometry by `tools/build-workshop-art.ps1` and uses no SCS logo or external image.

## Upload

Use the installed SCS Workshop Uploader with Euro Truck Simulator 2 selected.
Point Mod data at `dist/workshop/content`, Preview image at the generated JPG,
title at `ETS2 Cargo Filter - Owned Trailers (Plugin Required)`, type at UI,
and visibility at Public. Copy `workshop/description.txt` as the description.
The GitHub release referenced there must exist before publishing.

Let the official uploader validate the package. Fix errors before publishing;
review warnings instead of suppressing validation. Preserve the returned
Workshop item ID for subsequent updates; select the existing item rather than
creating duplicates. Steam may require the account owner to accept Workshop
terms before the item becomes publicly visible.

Official reference:
https://modding.scssoft.com/wiki/Tutorials/SCS_Workshop_Uploader/How_to_upload_new_mod%3F
