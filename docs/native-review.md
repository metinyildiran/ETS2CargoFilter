# Cargo Market native dispatch: bounded static review

Target: `D:\SteamLibrary\steamapps\common\Euro Truck Simulator 2\bin\win_x64\eurotrucks2.exe`, version 1.61.1.9. The parent investigation established SHA-256 `B70C2357CC56EB5834EE47FEA3C60B2F2D4591383FF2AC479CFD3CD254301B8C`. This review read that target using MSVC 14.44.35207 `dumpbin /disasm:bytes /range:...`; it did not execute or modify the game. All addresses below are preferred-image virtual addresses, with image base `0x140000000`; they are not runtime addresses.

## Observed dispatch and resource linkage

- `0x1411A3220` initializes a caller-provided string-like result in RDX, copies from global `0x142DE4980`, and returns the result pointer. The parent's earlier investigation connects that global to `/def/cargo_market_filter_data.sii`.
- `0x1411A2EA0` calls virtual slot `+0x288`, then calls `0x1412BA560` with RCX=`this+0x3C8`, RDX=`this`, R8=the previous result. This establishes a filter-resource-to-embedded-component initialization path.
- `0x1411A2FE0` checks event code R8D=`5` and control ID `[RDX+0x14]`=`390`. Its special branch refers to literal `jobs.cargo.market` and returns AL=`1`; other cases tailcall `0x14106A380`.
- The extracted `cargo_market.sii` explicitly names ID 390 `_nameless._.view.global.cargo.market.button`. This is the existing global-market action, not an owned-trailer selector.
- `0x14106A380` first invokes the embedded component's virtual slot `+0x10` using RCX=`this+0x3C8`, preserving the event arguments. A true result consumes the event. Otherwise the function dispatches hardcoded event codes and control IDs, and eventually calls `0x1410A8B50` for unhandled cases.

## Existing filter component

`0x1412BA560` retains the owner and string-like argument, calls `0x1412BB730`, and binds UI controls with IDs 470, 471 and 472. It constructs 16-byte entries from eight global token-sized values at `0x142DEA500`, then appends values from the object at component offset `+0x68`. Its continuation creates buttons whose IDs begin at 481.

The resource-loading and filter-component interpretation follows the observed call chain and extracted definition path. The precise class layout and semantics of the appended tokens have not been recovered. In particular, these observations do not establish that the definition can carry arbitrary callbacks or refer to profile-owned trailers.

The on-disk `cargo_market_filter_data.sii` inspected here contains `ui_cargo_filter_data` with only `button_all_img_name: "all_cargo_planner"`. The existence of inherited/default fields is not ruled out by that small definition.

## Sorting path to preserve

The extracted screen names control 400 as the sorting listbox. The shared event handler's branch at `0x14106A4DC` tests ID 400, reads the new value from event payload `+0x10`, compares against virtual getter `+0x200`, calls setter `+0x208`, invokes virtual `+0x1F8`, and calls `0x14106C1D0` with DL=`1`.

A candidate vtable base is `0x1423A03E8`: its `+0x288` entry points to `0x1411A3220`, consistent with the initialization call. Under that interpretation the sort getter, setter and refresh entries point to `0x14106D4A0`, `0x14106D4B0` and `0x14106D2A0`. The vtable-base inference has not been confirmed from a constructor store.

## Other inspected methods

- `0x1411A2C80` accesses the selected row via state `+0x308` and helper `0x1410741F0`, then calls gameplay-related methods and updates selection/UI state. This is not a demonstrated read-only filtering entry point.
- `0x1411A30C0` compares a nested string to `fallback`, then accesses element zero of an array. Its assertion string names `array_t<owner_ptr_t<player_vehicles_u>>`. It obtains a reference-counted object and calls `0x1409CA3A0` with fields from the candidate offer at offsets `+0x58`, `+0xCC` and `+0x60`. A preview-building role is plausible, but not proven; compatibility and field meanings remain unknown.
- `0x1411A31D0` chains calls to `0x14106CF80`, `0x141071300` and `0x141071B40`.
- `0x14119C0C0` updates existing selection-dependent UI; `0x14119C150` forwards layout-like scalar arguments through two other methods. Neither bounded body exposed a public callback-registration facility.

## Missing integration contract

There is a concrete compiled event/refresh path. An added SII control alone is not shown to implement new behavior. Before a functional native adapter can be implemented, recover and validate:

1. Complete active-profile owned-trailer enumeration and stable IDs.
2. Immutable offer snapshot layout, ownership/lifetime, and exact trailer compatibility, including differing quantities and payment.
3. How offers for non-active trailers can be obtained without changing active gameplay state or destroying another trailer's offers.
4. Native list construction/refresh and selected-row mapping after filtering, while retaining sorting.
5. The filter component's full event ABI, lifetime and UI creation/removal behavior.
6. Convoy-specific offer synchronization and job-acceptance behavior.

No native adapter, event interception or executable patch was implemented by this review. Static evidence does not certify runtime behavior or Convoy compatibility.

## Retained local evidence

Bounded disassemblies in `.work/native-review/`:

- `filter-handler.asm.txt`: `0x1411A2C80–0x1411A3264`.
- `shared-handler.asm.txt`: `0x14119C0C0–0x14119C1AF`.
- `shared-event-dispatch.asm.txt`: `0x14106A380–0x14106AB78` (trailing bytes from `0x14106AB2C` are a jump table, not instructions).
- `filter-component-init.asm.txt`: `0x1412BA560–0x1412BA813`.
- `filter-component-init-continuation.asm.txt`: `0x1412BA813–0x1412BAC8F`.

PE `.pdata` entries can split a single function into chained fragments. The entry beginning at `0x1411A30C0` continues beyond its first fragment ending at `0x1411A310E`; the filter initializer similarly spans several adjacent records. This review follows the bounded continuation disassemblies instead of treating each record as a complete function.
