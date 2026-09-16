# Tutorial - extending and reviewing the B-LSC example

[README.md](README.md) says what this example *is*. This document is the
*how*: how to extend it into your own device, who serves which property, how
to review the result for conformance, and what goes wrong when you get it
subtly right.

Read this once before you start changing `main.cpp`. The most expensive
mistake in this example is silent, and the section it lives in is
[Adding an object](#adding-an-object---read-this-first).

- [Extending the example](#extending-the-example)
- [Who serves what: the application or the stack?](#who-serves-what-the-application-or-the-stack)
- [Adding an object - read this first](#adding-an-object---read-this-first)
- [Reviewing your device](#reviewing-your-device)
- [Troubleshooting](#troubleshooting)

## Extending the example

The example is intentionally linear so it's easy to change.

**Change a sensor's value or name** - edit the constants / callbacks in
`main.cpp` (e.g. `g_analogInput1Value`, or the colour-name strings in
`GetPropertyCharString`).

**Change the device identity before you ship** - vendor ID, vendor name,
model name, description, firmware revision, DCC password and device name are
all in the `CHANGE ALL OF THIS BEFORE YOU SHIP` block at the top of
`main.cpp`, with a per-field comment saying what to change it to and why.
That block is the authoritative checklist; it is in the source rather than
here so it cannot be skipped by someone who only reads the code. In
particular, `DEVICE_NAME` carries its own warning: `Object_Name` must be
unique across the BACnet internetwork, it is a compile-time constant in this
example, and a real product must make it per-unit configurable too (serial
number, DIP switches, a config file, or a `--deviceName` argument) - the
device *instance* is already runtime-configurable via `--deviceID`, but
shipping two units with the same compiled-in `Object_Name` is a spec
violation clients will not politely ignore.

## Who serves what: the application or the stack?

The single most common question when reading `main.cpp` is "who answers this
property?" For Life Safety Point 1 **"Amber"** - the alarm-capable object,
and the most interesting one in this example:

| Property | Served by | How |
|---|---|---|
| `Object_Identifier` | **stack** | generated from the object you added |
| `Object_Type` | **stack** | generated |
| `Object_List` | **stack** | generated (Device object) |
| `Property_List` | **stack** | generated |
| `Status_Flags` | **stack** | generated (and reflects the alarm state) |
| `Event_State` | **stack, genuinely computed** | because this example arms an intrinsic ChangeOfLifeSafety algorithm plus a fault algorithm on Amber (`SetIntrinsicChangeOfLifeSafetyAlgorithm`/`SetFaultLifeSafetyAlgorithm` + `SetAlarmsAndEventsForObjectEnabled`), the stack drives `Event_State` to `normal` / `life-safety-alarm` / `fault`. On an object with no alarming configured, nothing serves `Event_State` and it reads its datatype default `normal(0)` by coincidence - the opposite situation, and the one every other input/output object in this file is in. |
| `Present_Value` / `Tracking_Value` | **you** | `GetPropertyEnumerated` / `SetPropertyEnumerated` |
| `Mode` | **you** | `GetPropertyEnumerated` / `SetPropertyEnumerated` |
| `Reliability` | **you** | `GetPropertyEnumerated`, fixed at `no-fault-detected` (the fault *algorithm* is what drives `Event_State` to fault, not this property) |
| `Silenced` | **you** | `GetPropertyEnumerated`, updated by `LifeSafetyOperation()` |
| `Operation_Expected` | **you** | `GetPropertyEnumerated` |
| `Object_Name` | **you** | `GetPropertyCharString` |

That `Event_State` row is the whole point of B-LSC: arming the algorithm is
what turns a plain writable Life Safety Point into an alarm source, and it is
why `Event_State` moves from "defaulted by coincidence" to "genuinely
computed" the moment you copy this pattern to a new alarming object.

**This table describes what `main.cpp` is coded to do - not what a client
sees on the wire today.** Because of the stack defect this README documents
prominently ([chipkin/cas-bacnet-stack#2036](https://github.com/chipkin/cas-bacnet-stack/issues/2036)),
every row above except `Object_Identifier`/`Object_Type` currently answers
`unknown-object` when you actually query Amber or Azure - the stack's
generated-property dispatch for these two object types requires an internal
engine object that `BACnetStack_AddObject` never creates, and it bails before
reaching any of the callbacks in this table. See
[docs/PICS.md](docs/PICS.md) and [TODO.md](TODO.md#0-critical-life-safetypointzone-objects-cannot-serve-almost-any-property-via-the-customer-surface)
for the full trace. The table above is still correct as a map of the *code*,
and is exactly what starts working the day the stack exports the missing
`Add*` functions.

For every other object type in this example (Analog/Binary/Multi-State Input,
the three commandable Outputs, Notification Class, Network Port), the
callbacks above work end to end today - see
[docs/PICS.md](docs/PICS.md#11-objects-and-properties) for the full,
generated per-object breakdown.

## Adding an object - read this first

Adding an object is the easiest place to ship a silent non-conformance. The
callbacks are **not uniformly strict**: `GetPropertyReal` /
`GetPropertyEnumerated` / `GetPropertyUnsignedInteger` match on object type
**and instance** (directly, or through `GetCommandable()`, which looks up the
exact type+instance pair) - so a new instance falls through every one of
them. `GetPropertyBool` is the exception: it serves `Out_Of_Service` on
object **type only**, so a new instance gets it for free.

Here is the part that matters, and it is the opposite of what most people
assume: falling through a callback does **not** reliably produce an error.
The stack errors only for a short list of properties it refuses to invent -
`Present_Value`, `Number_Of_States`, `Relinquish_Default`, `Local_Date`,
`Local_Time`, and a Network Port's `APDU_Length`. For **everything else** it
**silently substitutes a default**, while `Property_List` still advertises
the property as present:

| Property | If you forget to serve it | Loud? |
|---|---|:--:|
| `Present_Value` | Error (`read-access-denied`) | yes |
| `Object_Name` | reads back as the literal string **`"undefined"`** | **no** |
| `Units` | reads back as **`no-units` (95)** | **no** |
| `Out_Of_Service` | served on type alone - works by accident | n/a |

So a half-added object does not look broken; it looks **healthy**. Add two of
them and both report `Object_Name "undefined"` - duplicate object names
inside one device, a spec violation and a hard BTL failure that every scan
tool will render as a perfectly good object. **"It scanned OK" is exactly the
failure mode, not evidence against it.**

When you add an instance:

1. Add its instance constant. Naming: a second object of a type is
   `"<Colour> 2"` - each object *type* owns one colour series-wide.
2. `BACnetStack_AddObject` it in `main()`, checking the return like every
   other stack call in this file.
3. Serve **every** required property in the relevant `Get*`/`Set*` callbacks.
4. If it should alarm, arm it: `SetAlarmsAndEventsForObjectEnabled` +
   `SetIntrinsicChangeOfLifeSafetyAlgorithm`/`SetFaultLifeSafetyAlgorithm`,
   and wire it to a Notification Class (`AddNotificationClassObject` +
   `AddRecipientToNotificationClass`, or reuse an existing one).
5. If it is commandable, enable `Priority_Array` and `Relinquish_Default` and
   make `Present_Value` writable - see the `outputs[]` loop in `main()` and
   its comment on why this is a no-op for the built-in Output types but
   load-bearing for a Value type.
6. Read back **every** required property of the new object with a BACnet
   client and **diff it against an existing object of the same type**. Any
   property that comes back `"undefined"`, `no-units`, or `0` where the
   existing object returns something real is a step you missed. Because the
   failure is silent (see the table above), this diff is the only thing that
   catches it.

## Reviewing your device

After you have changed anything, review it against the conformance statement
rather than against "it looked fine in the explorer":

1. Regenerate [docs/PICS.md](docs/PICS.md) after editing `docs/objects.json`
   (see [Keeping the PICS honest](#keeping-the-pics-honest) below).
2. Read **every** property listed for **every** object with a BACnet client,
   and compare the value against the PICS. `"undefined"`, `no-units` and `0`
   are the three shapes a missed callback takes - except on Life Safety
   Point/Zone, where every property currently answers `unknown-object`
   regardless of what `main.cpp` does (see the callout in
   [docs/PICS.md](docs/PICS.md) - that is the known stack defect, not
   something to "fix" here).
3. Diff a new object of a type against the existing one of that type.
   Anything that differs and shouldn't is a callback that matched on
   instance.
4. Fire a life-safety alarm (once #2036 is resolved, or against a patched
   stack): WriteProperty Amber's or Azure's `Present_Value` to `2` (alarm),
   confirm `Event_State` -> `life-safety-alarm` and a `CHANGE_OF_LIFE_SAFETY`
   `EventNotification` arrives at the recipient; write `3` for fault; write
   `0` to clear. Send `AcknowledgeAlarm` and `GetEventInformation` and confirm
   both respond.
5. WriteProperty a commandable output's `Present_Value` at a priority,
   re-read it (and its `Priority_Array`), then write `NULL` to relinquish and
   confirm it falls back to `Relinquish_Default`. Confirm a write to a
   read-only input is rejected.
6. SubscribeCOV to Bronze's or Amber's `Present_Value`; change it (up/down
   key for Bronze, WriteProperty for Amber) and confirm a COV notification
   arrives.
7. ReinitializeDevice `COLDSTART`; confirm a `SimpleACK`, then that the
   process actually restarts and re-announces with an I-Am.

### Keeping the PICS honest

`docs/PICS.md` is partly generated. `docs/objects.json` describes each object
and who serves which property; the series tool regenerates the object tables
from it plus the stack's own `docs/property-profile-reference.md` at the
pinned commit:

```bash
python tools/gen-objects-properties.py BACnetProfileExample-B-LSC-CPP            # rewrite
python tools/gen-objects-properties.py BACnetProfileExample-B-LSC-CPP --check    # fail if stale
```

(That tool lives in the example-series repository, not in this one. If you
only have this repository, edit the generated block by hand and keep it
matching the callbacks in `main.cpp`.)

When you add an object or a property to `main.cpp`, update `docs/objects.json`
in the same change and regenerate. The `app` list is what the callbacks
serve; `accepted` is for a required property you deliberately leave to the
stack's default, and each one needs a justification. Anything required, not
in `app` and not in `accepted`, comes out as a ⚠ row in the generated table -
that is a defect the generator can catch, and a different thing from the
Life Safety Point/Zone wire-level defect this document keeps calling out
separately (the generator has no way to see that; it only knows a callback
exists in the source).

## Troubleshooting

| Symptom | Cause / fix |
|---------|-------------|
| ReadProperty of Amber's/Azure's `Object_Name`, `Present_Value`, or almost anything but `Object_Identifier`/`Object_Type` returns `Error(...): object: unknown-object` | **Expected, and not your bug.** This is [chipkin/cas-bacnet-stack#2036](https://github.com/chipkin/cas-bacnet-stack/issues/2036) - see the callout in [docs/PICS.md](docs/PICS.md) and [TODO.md #0](TODO.md). |
| WriteProperty of Amber's/Azure's `Present_Value` (the alarm/fault demo trigger) fails the same way | Same root cause as above - the stack's Life Safety engine dispatch, not this example's `SetProperty*` callback. |
| LifeSafetyOperation requests get `unrecognized-service` | Expected - the linked stack build was not compiled with `STACK_OPTION_DM_LSO_LIFE_SAFETY_OPERATION`. See [TODO.md #2](TODO.md). |
| On start-up the app prints a wall of red `Error:` lines but the device works | **Expected - this is not your bug.** Two benign sources, both from the stack's own debug logging: (1) the device receives its **own** broadcast I-Am and logs a decode cascade - any BACnet/IP device that listens for broadcasts hears itself; (2) a one-time *"UUID has not been set. A UUID must be set for the BACnetSC device to start."* - the stack starts a BACnet/SC datalink these IP-only examples never configure. |
| CMake error: *"CAS BACnet Stack adapter not found under: ..."* | Submodules not initialized. Run `git submodule update --init --recursive` (or pass `-D CAS_STACK_DIR=...`). |
| `CASBACnetStackDLL.h: No such file or directory` | Same - submodules not checked out. |
| Windows: *"No CMAKE_CXX_COMPILER could be found"* | Install Visual Studio with the "Desktop development with C++" workload, then re-run from a fresh terminal. |
| First build seems stuck for minutes | Normal - it's compiling ~600 stack files. Only the first build is slow. |
| App prints *"Failed to bind UDP port 47808"* | Another BACnet program is already using 47808. Stop it, or run with `--port <n>`. |
| Client sends Who-Is but sees no I-Am | Firewall is blocking UDP 47808, or the client and device are on different subnets (Who-Is is a broadcast). Allow the port; test on the same subnet first. |
| Replies show an unexpected device instance or vendor | Another BACnet device is already answering on this host/port. Stop the other device, or use `--port`. |
