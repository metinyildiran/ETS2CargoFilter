# Native Cargo Market UI evidence

Static reconstruction only; no game launch or runtime ABI validation was
performed. Applies solely to ETS2 1.61.1.9 executable SHA-256
`b70c2357cc56eb5834ee47fea3c60b2f2d4591383ff2ac479cfd3cd254301b8c`.
Addresses below are preferred VAs with image base `0x140000000`; code uses RVAs.

## Handler identity and events

The factory at `0x1411A2880` writes vtable `0x1423A03E8` to the newly
constructed Cargo Market handler. The deleting destructor at `0x1411A28D0`
uses allocation size `0x598`. This evidence distinguishes the vtable from
the static registration records that also reference the class name.

The event method is vtable slot `+0x68`, implementation `0x1411A2FE0`.
Observed x64 arguments are handler in RCX, control in RDX, event code in R8D,
and event payload in R9. The method handles button 390 then delegates to
`0x14106A380`. That common method routes the existing filter component before
dispatching other events. The native selection event is code **21**:
`0x14106A4DC` checks `uint32(control+0x14)==400`, reads
`uint32(payload+0x10)`, changes the sort setting and sorts/redraws.

The payload field could represent the row's ID rather than its ordinal;
vanilla sets both to the same value. Our new control 9470 does the same and
therefore does not depend on that distinction. Its events should be handled
without swallowing unrelated native events.

Initialization slot +0xC8 (`0x1411A2EA0`) has observed contract
`void(handler)`; its return value must not be interpreted as a success flag.
Event slot +0x68 returns a boolean in AL. The deleting destructor slot 0
(`0x1411A28D0`) takes `(handler,uint32 flags)` and returns the original handler
pointer; flags&1 requests allocation release.

## Dropdown population

The reference sequence is the first portion of `0x141069610`:

| Function | Observed call contract |
| --- | --- |
| `0x140385C60` | `Control* find(Window*, uint32 id)`, window at handler+0x10 |
| `0x14039E0F0` | `void clear(List*)`, list at dropdown+0xA0 |
| `0x1403A1F90` | `bool grow(Vector*, uint64 extra)`, vector at list+0x160 |
| `0x1400F2F60` | string assignment `(NativeString*, const char* const*)`; dereferences argument 2 |
| `0x1403A0180` | list geometry update `(List*, bool countChanged)` |
| `0x14039FB00` | list refresh `(List*)` |
| `0x1403C9090` | `bool select(Dropdown*, uint64 index)` |

List storage: data pointer `+0x168`, count `+0x170`, capacity `+0x178`.
Items have stride `0x58`; grow constructs both native strings at offsets 0
and `0x20`. Item+0x40 is its uint64 ID; +0x48 is a reference-counted pointer;
the vanilla enabled item sets the uint32 flags at +0x50 to `0x00150001`.
New entries retain the default null reference at +0x48. The string assignment
copies the label into native storage. Existing native strings remain owned by
the list and must never be released by the plugin's C++ allocator.

After each append vanilla tests byte(list+0x1C8); when false, it calls geometry
update with `uint32(list+0x1BC)!=uint32(list+0x170)`, then refreshes. Selection
is stored at uint32(list+0x1C0). Selection can synchronously call
`0x1403C82A0`; the adapter must suppress its own selection callback during
population to prevent recursive rebuilds.

`src/native_ui.cpp` follows this sequence, assigning the constructed item
string in place instead of constructing a temporary and swapping it. This is
a reasoned use of the observed string routine, not a runtime-tested claim.
It bounds entry counts and checks readable state; SEH containment cannot undo
partial native mutations. A failed population must disable further adapter
work for that handler rather than repeatedly retrying unknown state.

## Rows, sort and redraw

### Dropdown content height

The cloned list now sets `max_list_height: 450`, enabling the native content
layout instead of retaining the vanilla sort list's fixed 450-unit box.
Property registration at `0x142D2FAD8` maps offset +0xB0 to the name pointer
at `0x142D2FAF0` (`max_list_height`, string `0x1422309B8`). The existing
refresh routine `0x14039FB00` returns immediately when that float is <=0.
Otherwise it computes `min(cap, max(0, count*(item_height+gap)-gap))`, using
count +0x170, item height +0xA4 and gap +0xAC, and moves the list bottom while
keeping its top fixed. At `0x14039FCC2..0x14039FDE9` it also finds its text
frame child and applies the same bottom. The type descriptor `0x141E13440`
names `ui::text` through `0x142D2F330`; the cloned `ui::text_common` frame
inherits that native type.

`NativeUi` already invokes this native refresh after each inserted entry, so
no new memory writes or geometry ABI calls are required. Eleven entries at
25 units with zero gap produce height275 and bottom534 under top809. The
450-unit cap keeps longer lists within the previous screen footprint. The
six UI package tests pass, including unchanged original controls and enabled
content sizing only on the cloned list. This sizing change has static/native
evidence and build verification; its final appearance awaits a new game run.

Handler+0x2A0 is the **native row vector**, with data at +0x2A8 and count at
+0x2B0. `0x1410741F0` returns `data+index*0xC8`; these are not raw offer-unit
pointers. Rows own native strings and references and cannot be copied as an
unowned byte array.

`0x14106D2A0(handler)` reads the existing sort setting and selects one of 18
comparators. `0x14106C1D0(handler,true)` resets selection/map overlays and
redraws five job slots using the current page at handler+0x318. It does not
generate offers, rebuild the native vector, or reset pagination. Callers that
replace a list must separately preserve native pagination invariants.

`0x141070F90(handler)` requests an asynchronous provider rebuild: it sets
handler+0x57A and +0x578 to one, shows loading control 383 and disables the
accept button. The update loop discards any completed pending result while
the rebuild flag is set, then creates the next provider. It does not change
the sort setting. This is the routine called by the existing filter paths
at `0x1412BC062`, `0x1412BC264` and `0x1412BC283`.

`0x1410702A0(handler,bool enabled,const char* const* caption,bool visible)`
configures accept button 300, not the status area. The actual empty-results
message method is slot +0x298, `0x141070DF0(handler,const char* const*)`:
it hides job portal 200 and pagination controls, shows message label 381,
and copies text into its native string. `NativeUi::set_status` exposes this
behavior. A completed native rebuild restores normal result visibility.

## Acceptance and teardown

All observed button and double-click acceptance routes converge on handler
vtable +0x220, `0x1411A2C80(handler)`, which returns boolean AL. Event 5 on
button 300 and event 16 double-click on row controls 201000..201004 use this
slot. The method first calls `0x14106EB60`: that routine checks the selected
row and player/job state, but does not establish that the row's required
trailer is attached. The adapter must check a generated offer's trailer before
delegating to acceptance. The selected row index is handler+0x308 and its
offer reference is at row+8. In global mode acceptance stages
`0x140600530(player,row.company,row.offer)`; at a company it calls
`0x14079E1E0(economy,row.company,row.offer)`.

Handler vtable +0x08 is teardown `0x1411A2910(handler)`, which calls
`0x141068FD0`. It passes any pending task to scheduler+0x28 at
`0x141068FF5`; the default `0x140167E40` sets cancellation and joins through
`0x140167E70`. Only after joining does teardown delete the pending task,
null handler+0x328 and clear native row vector +0x2A0 via `0x141074160`.
Thus releasing handler-owned generated-offer backing storage **after** this
teardown returns is consistent with observed task and row lifetimes. A
deleting-destructor fallback can also release remaining adapter state after
calling the original destructor; do not release backing offers before native
rows/task references are disposed.

Provider destructor slot 0 is `0x1411A3270(task,uint32 flags)` returning the
original task pointer. It calls `0x141072E10` to destroy row/city vectors and
filter/calculator state, then frees 0x360 bytes if flags&1. It contains no
join: the scheduler/handler must already have synchronized execution.

## Native provider route

Handler vtable slot +0x1E8, `0x1411A2B30`, allocates the provider/task (size
0x360), runs base constructor `0x141072DA0`, and sets vtable `0x1423A03B8`.
Its slot +0x08 is `0x1411A32B0`; slot +0x20 is the existing search/filter
predicate `0x1411A27A0`. Task+0x350 is the optional source-company restriction;
byte +0x358 gates the attached-trailer requirement; byte +0x24 cancels work.

`0x1411A32B0` walks global economy+0x28/count+0x30 company pointers and each
company's +0xA8/count+0xB0 offer pointers. It preserves existing source-city,
company visibility, expiry, cargo-reference and search/filter checks. For
each matching offer it constructs a temporary 0xC8 row whose first two
fields are retained source-company and offer pointers. It calls
`0x141072620(task,row,sourceCity,destinationCity)` for derived row values,
appends via `0x1410745C0(task+0x168,row)`, destroys the temporary via
`0x141073E50(row)`, and finally calls `0x141073990(task)`. The last routine is
a pay calculation pass, not task completion: it calls `0x14079BF30` and
stores total/base pay at row+0x88/+0x8C.

Task vtable slot +0x18 is `0x141072D40(task,destinationVector)`: it swaps
vector data/count/capacity and the extra +0x20 ownership field with task+0x168.
Slot +0x10 is `0x141072E90(task,handler)`, which copies the existing filter
component into task+0x1C8 and initializes pay-calculator settings. This
supports preserving the normal provider task and its transfer path.

The default scheduler is initialized at `0x140167CD7..0x140167D38`, stored at
`0x1436AD978`, and its +0x08 method queues through `0x140167E00` /
`0x140167F30`. Its worker helper `0x140167FD0` calls task vtable+0x08 at
`0x140168078`; only after return does it set task+0x20 to 3 at
`0x1401680D3..0x1401680E6`, with barriers. Handler tick `0x14106AB80` submits
the task at `0x14106AC09`, waits for state>=3 at `0x14106AC25..0x14106AC3B`,
swaps rows at `0x14106ACE8`, and deletes the task at `0x14106AD15`.
Consequently a replacement execute callback can append before returning,
under this default scheduler and the original one-execution task setup,
without racing that normal row-publication path. Task cancellation and handler
teardown must still be honored.

`src/native_rows.cpp` reconstructs only the native stack-row sequence observed
at `0x1411A348B..0x1411A3677`. It retains original company and offer units,
initializes three inline strings with vtable `0x1421F3A10`, applies source,
expiry and existing predicate checks, and uses native append/destruction. It
stores distance at row+0x90 from offer ushort+0x50 and mass at row+0x94 from
offer uint32+0xCC multiplied by cargo float+0x9C, with the observed accessory
fallback. The destination descriptor sort key is limited to seven bytes:
the string reserve method `0x1401E4410` always returns capacity 8.
Both wrapper files compile under `/W4 /WX`; none have been runtime-tested.

These findings identify an integration route; they do not by themselves
validate generated offer lifetime, cloned economy contexts, or job acceptance.
Those must be resolved before feeding additional rows.

## Reproducible evidence

Focused dumpbin output is under `.work/ui-binding/`; selected Ghidra
reconstructions are under `.work/native-batch/`. `tools/ui-pe-probe.mjs`
parses PE sections and exception records, including chained unwind function
ranges; its byte-wise `rip` hits are candidates and must be disassembled before
being treated as references. No analysis output was used to claim runtime
success. The wrapper compiled with MSVC 19.44, `/std:c++20 /W4 /WX /EHsc`.
