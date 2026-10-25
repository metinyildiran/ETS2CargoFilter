# Cargo Market access without an assigned trailer

Static evidence for ETS2 1.61.1.9 x64, executable SHA-256
`b70c2357cc56eb5834ee47fea3c60b2f2d4591383ff2ac479cfd3cd254301b8c`.
Preferred image base is `0x140000000`; addresses below are preferred VAs.

`0x1405f9e60(player,economy)` enables several cargo actions only when the first
player vehicle has a live trailer at +0x48. Otherwise it disables those actions.
The global Cargo Market action string is `jobs.cargo.market` (`0x14224c730`).
The mod relaxes only this action, leaving company and external cargo actions alone.

The action manager singleton is at `0x1436af838`. Its methods take
`(manager, const char* const*)`: `0x14050ab00` queries enabled state,
`0x14050a940` enables, and `0x14050aa20` disables. They synchronize their native
disabled-action collection at manager+0x260 and refresh the open menu controls.
Native strings/collections are never edited directly by the mod.

Desktop factory `0x140fe4c10` installs vtable `0x14236f220`. Slot +0xc8 is
`void init(handler)` at `0x140fe4e80`; slot +0x60 is `void update(handler)` at
`0x140fe58b0`. Both callbacks retain their originals. The mod refreshes access
after initialization, then at most every 250 ms in the desktop's own update
callback, including when simulation telemetry is paused. All 11 hook slots are
validated before installing any hook; the existing exact executable gate applies.

Eligibility requires a stable loaded ownership snapshot, at least one owned
trailer, a player vehicle and a live own truck. The last conditions are necessary:
Cargo provider factory `0x1411a2b30` asserts on an empty vehicle list. A missing
trailer merely sets task+0x358 to zero; the existing adapter supports that case
and appends all owned-trailer batches. The preview generator uses a shadow
vehicle with the selected trailer, and the native acceptance path supports a
null previous trailer.

The mod remembers only permissions it added, scoped to the world/player. Selling
the final trailer revokes that override if no trailer is assigned. Switching
profiles drops the old ownership of the override. Menu state changes are applied
on the main thread using the native action methods.

Local tests cover unloaded/changing profiles, zero owned trailers, unassigned
owned trailers, live/expired/missing trucks, and removal of the final trailer.
Runtime validation must separately establish that the button opens the market
and accepting a preview job attaches the selected trailer from an unassigned state.
