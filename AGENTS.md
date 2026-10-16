# AGENTS.md

Guidance for AI coding agents working in this repository. See
<https://agents.md/> for the format. Human contributors should read
[README.md](README.md) first.

## What this project is

A **tutorial** C++ example of the BACnet **B-LSC (Life Safety Controller)**
profile, and it is **Profile-functional** (the series bar): a SIMULATED smoke
detector drives Life Safety Point 1 (Amber) and its zone (Azure) to a latched
life-safety alarm, the alarms go confirmed to an operator workstation named by
device instance (389107), and the workstation silences, acknowledges and resets
them with LifeSafetyOperation / AcknowledgeAlarm - proved by Wire Tests against a
Python Peer, with the Failure Modes (reset refused while smoke is present, a
detector fault, an unacknowledged alarm). It is one of a series - one git repo per
BACnet profile - seeded from B-AAC minus its Schedule/Calendar objects. What B-LSC
requires but the customer interface still cannot do (Zone's `Zone_Members`,
[chipkin/cas-bacnet-stack#2036](https://github.com/chipkin/cas-bacnet-stack/issues/2036))
is a `[KNOWN GAP]` in [TODO.md](TODO.md) and the Wire Tests - keep both honest and
current. The top priority is that the code reads like a tutorial a customer can
learn from and copy-paste. Favour clarity over cleverness.

## Layout

This repository is self-contained:

- `main.cpp` - the example device. Section 1b is the smoke-detector SIMULATION.
- `simulation/` - the Wire Tests' loopback-only test control channel (SIMULATION,
  off unless `BLSC_SIMULATION_CONTROL_PORT` is set). Never a CLI option.
- `common/` - the shared helper (vendored).
- `README.md` - what this example is. Keep it short and about THIS example
  only.
- `TUTORIAL.md` - how to extend and review the example. Long-form material
  that would bloat the README belongs here.
- `docs/PICS.md` - the Protocol Implementation Conformance Statement. Its
  objects-and-properties section is GENERATED from `docs/objects.json`; do
  not hand-edit between the `OBJECTS-PROPERTIES` markers.
- `docs/objects.json` - the input to that generator. Update it in the same
  change as any `main.cpp` change that adds an object or a `GetProperty*`
  branch.
- `TODO.md` - what B-LSC requires that this example cannot yet do, and why.
  Keep it honest and current.
- `tests/` + `wiretest/` - the Wire Tests (discovery, `life_safety_test.py`,
  `fault_test.py`) and the series' shared harness (vendored at 0.7.0; change it
  only as the series does, in every example). Every check must fail when
  its feature is removed: mutation-verify any change (tests/README.md).
- `submodules/cas-bacnet-stack/` - the **CAS BACnet Stack** as a git submodule
  (private; compiled from source). After cloning, run
  `git submodule update --init --recursive`.

The `PROFILE-TABLE` block in README.md is also generated, from the
example-series repository's `docs/profile-table.md`. Edit it there, not here.

## Build

Plain CMake, identical on every platform, in the adapter's default SOURCE
mode (the stack's sources are compiled into the executable - no prebuilt
library, no DLL, no per-platform pre-step):

```bash
git submodule update --init --recursive   # once, if not cloned with --recursive
cmake -B build -S .
cmake --build build --config Release
```

The first build compiles the whole stack (~600 files) and takes a few
minutes; rebuilds after that are incremental and fast. Use
`-D CAS_STACK_DIR=...` only if your stack lives outside the bundled
submodule. Do not reintroduce a link-mode flag or a series-root build script
into the documented build: a customer downloads this repository on its own
and must be able to build it with the two commands above.

## Run

```bash
./build/BACnetExampleBLSC [--port 47808] [--deviceID 389007]   # Linux/macOS
.\build\Release\BACnetExampleBLSC.exe [--port 47808] [--deviceID 389007]   # Windows
```

Interactive keys while running: `h` help, `q` quit, up/down nudge Analog Input 1
(also feeds its COV subscribers).

## Conventions

- Device is named "Chipkin Example B-LSC"; objects use the series' colour names; vendor id 389.
- Implement the B-LSC services the stack supports; expose **every required
  property** of each object for Protocol_Revision 30. Anything B-LSC requires that
  is NOT implemented must be listed in [TODO.md](TODO.md) and the README.
- **Life Safety Point/Zone objects use the SAME generic pattern as every other
  object in this series** - `BACnetStack_AddObject` + this file's own Get/Set
  callbacks holding the state - NOT the stack's internal
  `BACnetStackLifeSafetyPoint`/`Zone` engine, whose configuration surface
  (`AddLifeSafetyPointObject`, `SetLifeSafetyZoneMembers`,
  `SetLifeSafetyAcceptedModes`/`AlarmValues`/`FaultValues`, and
  `SetLifeSafetyTrackingValue`) is internal to `BACnetDBDevice` and is not
  exported through `CASBACnetStackDLL.h`. Re-verify this with a fresh
  `grep -n "DllExport.*LifeSafety" source/CASBACnetStackDLL.h` before assuming it
  has changed - it is the load-bearing fact behind this file's whole life-safety
  design (see the file header comment in `main.cpp`).
- Intrinsic life-safety alarming: arm an object with
  `SetIntrinsicChangeOfLifeSafetyAlgorithm` / `SetFaultLifeSafetyAlgorithm` + a
  Notification Class (`AddNotificationClassObject` +
  `AddRecipientToNotificationClass`) + `SetAlarmsAndEventsForObjectEnabled(...,
  true)`. Drive the monitored `Present_Value` and call `BACnetStack_UpdateValue`
  so the stack re-evaluates and fires the notification - the doc comment on that
  function is not decorative: "a host that never calls it will never generate an
  event notification."
- LifeSafetyOperation: needs `STACK_OPTION_DM_LSO_LIFE_SAFETY_OPERATION` (the
  stack's documented public toggle, defined in `CMakeLists.txt` for SOURCE builds;
  a STATIC/DLL library must be built with it), plus
  `BACnetStack_RegisterCallbackLifeSafetyOperation` and the service enabled. The
  stack checks the named object exists; the callback decides whether the operation
  fits the object's current state (`OperationFits()`), refusing with
  `INVALID_OPERATION_IN_THIS_STATE`. `useObjectIdentifier == false` means "all
  applicable objects" (cl. 13.13.1.1.4).
- Reliability on Amber/Azure: the callback serves only cl. 13.2.2.2's first stage
  (no-fault-detected); the stack's FAULT_LIFE_SAFETY algorithm supplies
  `multi-state-fault` and serves the combination. Do not compute it in the callback.
- DS-COV-B: `BACnetStack_SetPropertySubscribable` on the property, plus
  `SetCOVSettings`/`SetMaxActiveCOVSubscriptions`; `BACnetStack_UpdateValue` also
  feeds COV subscribers - one call does both jobs, there is no separate "notify
  COV" export.
- Outputs are **commandable**: store the 16-slot `Priority_Array` +
  `Relinquish_Default` in the app (the `Commandable` struct); let the stack
  resolve `Present_Value`. Writes land via the `SetProperty*` callbacks (value)
  and `SetPropertyNull` (relinquish).
- DeviceCommunicationControl (DM-DCC-B): the stack runs the enable/disable state
  machine; the `DeviceCommunicationControl` callback just validates `DCC_PASSWORD`
  and logs. The deprecated plain `disable` (1) is rejected by the stack at
  Protocol_Revision >= 20 - only `enable` (0) and `disable-initiation` (2) apply.
- ReinitializeDevice (F-REINIT, canonical here): never restart inside the
  callback - record a deadline (`CASExampleHelper::RequestRestart`), return true
  so the `SimpleACK` ships on the next `BACnetStack_Tick()`, and perform the
  actual restart from the main loop once `RestartDue()` says the deadline passed.
- Match the surrounding code style: `const`-correct parameters, check every stack
  return value, keep `main.cpp` linear and well-commented.
- **Never edit `common/` in this repo alone** - it is a vendored copy shared by
  every example in the series, with its own version (`COMMON_VERSION`) and
  changelog (`common/CHANGELOG.md`). To change it: edit, bump the version, add
  a changelog entry, then re-copy `common/` into every example repository.

## How to verify a change

There are no unit tests; verification is behavioural:

1. Build, then run one instance on a clear UDP port.
2. With a BACnet client (e.g. the CAS BACnet Explorer, or `bacpypes3`/`BAC0`),
   send **Who-Is** and confirm **I-Am** from the device instance.
3. **ReadProperty** every required property of every object and confirm the
   values; confirm `Protocol_Revision` is 30 and `Object_List` lists all objects.
   `python3 tests/run_wire_tests.py --exe build/BACnetExampleBLSC` automates this.
4. **WriteProperty** a commandable output's `Present_Value` at a priority, re-read
   it (and its `Priority_Array`), then write NULL to relinquish and confirm it
   falls back to `Relinquish_Default`. Confirm a write to a read-only input is
   rejected.
5. **Life-safety alarming** - WriteProperty Life Safety Point 1 "Amber"
   `Present_Value` to `2` (alarm); confirm `Event_State` goes to
   `life-safety-alarm` and a `CHANGE_OF_LIFE_SAFETY` EventNotification is sent;
   write `0` and confirm it returns to normal. Repeat with `3` (fault) and the
   fault algorithm. (All four checked at `64b2cfac5`.) AcknowledgeAlarm and
   GetEventInformation should both respond; not yet checked at this pin.
6. **LifeSafetyOperation**: not enabled on the linked stack build (see
   [TODO.md #2](TODO.md)); a request is expected to answer
   `unrecognized-service` rather than being executed.
7. **DS-COV-B**: SubscribeCOV to Bronze's or Amber's `Present_Value`; change it
   (up/down key, or a WriteProperty) and confirm a COV notification arrives.
8. **Device management**: ReinitializeDevice COLDSTART SimpleACKs, then the
   process actually restarts and re-announces with an I-Am; DCC
   `disable-initiation`/`enable` SimpleACK; TimeSynchronization accepted.
9. If you changed the objects or their properties, regenerate
   `docs/PICS.md` (`python tools/gen-objects-properties.py
   BACnetProfileExample-B-LSC-CPP` from the series root) and confirm no row
   comes out flagged with ⚠ (a ⚠ means no callback exists in `main.cpp` for a
   required property - a different, generator-detectable defect from the
   Life Safety Point/Zone wire-level gap above, which the generator cannot
   see because the callbacks do exist in source).

Verification is manual (no in-repo test suite ships).

## Releasing

Bump `APP_VERSION` in `main.cpp` and add an entry to [CHANGELOG.md](CHANGELOG.md),
then tag `vX.Y.Z`. The GitHub Actions workflow builds and publishes the release.

## License

See [LICENSE](LICENSE). The CAS BACnet Stack is a separate, commercially
licensed product and is not covered by it.
