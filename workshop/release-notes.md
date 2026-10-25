Owned-trailer Cargo Market filter for **ETS2 1.61.1.9, Windows x64**.

This experimental r4 release includes:
- Cargo Market access with an owned truck and at least one owned trailer, even when none is assigned.
- Interface and Mod Manager descriptions for all 43 supported game locales.
- Trailer type/registration labels, fitted dropdown height and automatic trailer assignment when confirming an offer.
- Native job sorting, search and pagination.
- Workshop-ready UI packaging and mod artwork.

**The DLL is required. Subscribing to the Workshop UI alone does not install it.**

Download the ZIP, close the game, back up your saves and copy `ETS2CargoFilter.dll` to the game's `bin/win_x64/plugins` directory. Use either the included `.scs` in your user mod directory or the Workshop subscription, and enable that UI in Mod Manager. Full instructions are in `INSTALL.txt`.

The plugin verifies the exact supported executable and will not activate on another game build. **Convoy session testing is pending.** Single-player functionality and several UI languages have been checked in game; all 43 translations have automated catalog validation.

Validation: 7 native CTest programs and 12 Node tests passed before release packaging. Release ZIP and individual binary SHA-256 hashes are included.
