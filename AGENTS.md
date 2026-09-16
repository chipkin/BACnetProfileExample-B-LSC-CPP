# AGENTS.md

Guidance for AI coding agents working in this repository. See
<https://agents.md/> for the format. Human contributors should read
[README.md](README.md) first.

## What this project is

A **tutorial** C++ example that implements **as much of** the BACnet **B-LSC
(Life Safety Controller)** profile as the standard CAS BACnet Stack's
customer-facing API supports. It is one of a series - one git repo per BACnet
profile - seeded from B-AAC (Advanced Application Controller) minus its
Schedule/Calendar objects, and adds **intrinsic life-safety alarming** (AE-LS-B /
AE-ACK-B / AE-INFO-B), **LifeSafetyOperation** (silence/reset), and **DS-COV-B**.
What B-LSC requires but the stack's customer surface cannot yet do (Life Safety
Zone's `Zone_Members`) is documented in [TODO.md](TODO.md) - keep that file
honest and current. The top priority is that the code reads like a tutorial a
customer can learn from and copy-paste. Favour clarity over cleverness.

## Layout

This repository is self-contained:

- `main.cpp` - the example device.
- `common/` - the shared helper (vendored).
- `submodules/cas-bacnet-stack/` - the **CAS BACnet Stack** as a git submodule
  (private; compiled from source). After cloning, run
  `git submodule update --init --recursive`.

## Build

This example links the CAS BACnet Stack as a prebuilt **STATIC** library (the
only mode it ships in - see the README's "Link mode" section):

```bash
git submodule update --init --recursive   # once, if not cloned with --recursive
tools/build-stack-static.sh BACnetProfileExample-B-LSC-CPP   # from the series root
cmake -B build -S . -DCAS_BACNET_STACK_LINK=STATIC
cmake --build build --config Release
```

The stack library build takes a few minutes the first time - it compiles the
whole stack (~600 files) once, via the stack's own project files; the example
itself then builds in seconds against that library. Use `-D CAS_STACK_DIR=...`
only if your stack lives outside the bundled submodule.

## Run

```bash
./build/BACnetExampleBLSC [--port 47808] [--deviceID 389007]   # Linux/macOS
.\build\Release\BACnetExampleBLSC.exe [--port 47808] [--deviceID 389007]   # Windows
```

Interactive keys while running: `h` help, `q` quit, up/down nudge Analog Input 1
(also feeds its COV subscribers).

## Conventions

- Device is named "Rainbow"; objects use the series' colour names; vendor id 389.
- Implement the B-LSC services the stack supports; expose **every required
  property** of each object for Protocol_Revision 24. Anything B-LSC requires that
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
- LifeSafetyOperation: register `BACnetStack_RegisterCallbackLifeSafetyOperation`;
  `useObjectIdentifier == false` means "apply to every life-safety object in this
  device" (cl. 13.8.1.4).
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
   values; confirm `Protocol_Revision` is 24 and `Object_List` lists all objects.
4. **WriteProperty** a commandable output's `Present_Value` at a priority, re-read
   it (and its `Priority_Array`), then write NULL to relinquish and confirm it
   falls back to `Relinquish_Default`. Confirm a write to a read-only input is
   rejected.
5. **Life-safety alarming**: WriteProperty Life Safety Point 1 "Amber"
   `Present_Value` to `2` (alarm); confirm `Event_State` goes to
   `life-safety-alarm` and a `CHANGE_OF_LIFE_SAFETY` EventNotification is sent;
   write `0` and confirm it returns to normal. Repeat with `3` (fault) and the
   fault algorithm. AcknowledgeAlarm and GetEventInformation both respond.
6. **LifeSafetyOperation**: send `silence`; confirm `Silenced` reflects it. Latch
   an alarm, send `reset-alarm`, confirm `Present_Value` returns to quiet.
7. **DS-COV-B**: SubscribeCOV to Bronze's or Amber's `Present_Value`; change it
   (up/down key, or a WriteProperty) and confirm a COV notification arrives.
8. **Device management**: ReinitializeDevice COLDSTART SimpleACKs, then the
   process actually restarts and re-announces with an I-Am; DCC
   `disable-initiation`/`enable` SimpleACK; TimeSynchronization accepted.

Verification is manual (no in-repo test suite ships).

## Releasing

Bump `APP_VERSION` in `main.cpp` and add an entry to [CHANGELOG.md](CHANGELOG.md),
then tag `vX.Y.Z`. The GitHub Actions workflow builds and publishes the release.

## License

The example source code is dedicated to the public domain under
[CC0-1.0](LICENSE). The CAS BACnet Stack is a separate, commercially licensed
product and is not covered by that dedication.
