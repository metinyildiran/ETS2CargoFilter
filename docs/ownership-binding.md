# Exact-build ownership binding

Target: ETS2 1.61.1.9 Windows x64, SHA-256
`b70c2357cc56eb5834ee47fea3c60b2f2d4591383ff2ac479cfd3cd254301b8c`.
All addresses below are preferred-image VAs (base `0x140000000`); convert to
RVAs before adding the actual module base. Source evidence was read from the
installed executable using Node's file reads and bounded MSVC disassembly.
This is separate from REA evidence already retained by the main investigation.

## Owned trailer enumeration

The shipped unit/field descriptor tables establish these object-relative fields:

| Object | Field | Offset | Evidence |
|---|---|---:|---|
| economy | player pointer | `0x18` | generator decompilation; root's runtime read |
| player | my_vehicles array | `0x70` | field row `0x142d69b60` |
| player | trailers array | `0x1a8` | field row `0x142d69bd8` |
| player | trailer_defs array | `0x1f8` | field row `0x142d69c28` |
| player_vehicles | current trailer | `0x48` | field row `0x142d69a60`; generator |
| trailer | trailer_definition | `0x1a0` | field row `0x142d84190`; generator |
| trailer | slave_trailer | `0x1b8` | field row `0x142d842f8` |
| trailer | is_private | `0x1c0` | field row `0x142d84320` |
| core_vehicle | accessories | `0x10` | field row `0x142d6af50` |
| core_vehicle | license_plate | `0x70` | field row `0x142d6b158` |

Each field row consists of the declaring type descriptor, object-relative offset,
field kind, optional related descriptor, and name pointer (five qwords). The
trailer type's parent descriptor points to core_vehicle, so the license plate
field applies to trailers as well. Field kind `0x3e` is used for both the
player's my_vehicles and trailers owner-pointer arrays; their observed storage
is vtable/data/count/capacity/allocator, at offsets 0/8/16/24/32 within a
40-byte array object. Thus owned trailer data/count are at player+`0x1b0`/
`0x1b8`, and current player-vehicle data/count at player+`0x78`/`0x80`.

The current economy singleton is addressed at RVA `0x3046780`. The root agent
independently reported successful read-only runtime enumeration of ten owned
trailers, with non-null definitions and one active trailer, in
`.work/runtime-ownership.json`. The ownership agent did not attach to or modify
that running process. This runtime observation corroborates the particular
loaded profile, not every profile/gameplay state.

## Object sizes and inheritance

Unit descriptors contain the parent descriptor at +`0x18`, field table at
+`0x20`, object size at +`0x30`, and alignment at +`0x38`.

| Type | Descriptor VA | Size | Parent |
|---|---:|---:|---|
| economy | `0x141e16610` | `0x1df0` | unit base |
| player | `0x141e17cf0` | `0x440` | unit base |
| player_vehicles | `0x141e17c90` | `0x80` | unit base |
| core_vehicle | `0x141e18280` | `0x1a0` | unit base |
| trailer | `0x141e1caf0` | `0x1d0` | core_vehicle |
| company | `0x141e1abd0` | `0x118` | descriptor `0x141e2e770` |
| job_offer_data | `0x141e26650` | `0xd8` | unit base |
| cargo_offer_data | `0x141e26600` | `0xe0` | job_offer_data |

The raw parsed descriptor rows are retained in `.work/ownership/descriptors.json`.

## Implemented read-only adapter

`src/native_ownership.hpp/.cpp` provides `read_ownership(module_base)` in
`ets2::cargo_filter::native`. The caller must first pass the host's exact-build
gate and call synchronously on the game thread. It uses the shared safe-read
helper, bounds array allocations, checks unit live flags as used by the game's
observed code, and rechecks the economy/profile plus array headers and pointer
lists after reading. Changed or invalid state returns no trailer results.

The result copies display strings and returns borrowed native addresses plus
session IDs of the form `player-pointer:trailer-pointer`. These are **not
persistent save IDs** and provide no identity guarantee after reload or object
address reuse. No reference count is incremented, so pointers must not be
retained for worker threads or future frames without revalidation.

Plate strings use data/length at trailer+`0x78`/`0x80`. Native plate text can
contain PML tags and a country suffix. The selector label strips all `<...>`
tags, stops at `|`, removes control bytes, and prefixes the source ordinal
(`Dorse 1 - ...`). Empty/unreadable plates use `Dorse 1`. Private and publicly
shared trailers both remain owned; a missing definition stays visible with a
zero definition and must not be treated as compatible with any offer.

`tests/native_ownership_test.cpp` uses only synthetic memory, not the installed
game. All 15 checks passed under MSVC `/W4 /WX`: unloaded state, full ownership
versus active trailer, exact definitions, sanitized labels, privacy/identity,
missing definitions, dead units, corrupt count, invalid pointers, empty arrays,
and profile unload.

## Scratch generator lifecycle: observed leads and unresolved conditions

The company descriptor serializes freight `job_offer` at +`0x78` and
`cargo_offer_seeds` at +`0x40`. Its transient cargo-offer array at +`0xa0`
does not appear in that serialization table. The generator and company code
do establish that it owns its offer units:

- `0x1407cc210` (company deleting destructor) calls `0x140115140` with
  company+`0xa0`, then destroys other owned members and eventually optionally
  frees `0x118` bytes.
- `0x1407cc320` (reset-like method) calls `0x140116510` with company+`0xa0`.
  The generator begins with the same cargo-array clear function.
- `0x1407cc1d0` allocates via the company descriptor and calls constructor
  `0x1407d13c0`.
- `0x140abac60` allocates via the cargo_offer_data descriptor.

Disassembly is retained in `.work/ownership/company-destructor.asm.txt`.
These observations do not certify safe shadow-context generation. In particular,
raw-copying a company and then destroying it would release aliases owned by the
real company. Copying an array allocator pointer without taking the proper
allocator reference can also imbalance its lifetime. The generator's helpers
must be checked for global/current-player accesses and any retained pointers
before cloned economy/player/player_vehicles contexts are used. No clone,
native generator call, refcount mutation, or destructor invocation is performed
by the ownership adapter.
