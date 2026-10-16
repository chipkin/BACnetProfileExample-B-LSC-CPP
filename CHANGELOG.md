# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.2.0] - unreleased

Profile-functional rework (series bar, `docs/GLOSSARY.md`; brief
`docs/phase2-rework-brief.md` in the example-series folder). Stack Pin unchanged
(`6.x` @ `64b2cfac5`), `common/` 3.4.0 and `wiretest/` 0.6.0 unchanged.

Profile Function, 135-2024 L.5.2 (verbatim): "A B-LSC device performs life safety
alarm detection and control. It supports life safety objects."

### Added

- **A simulated smoke detector** (`main.cpp` section 1b, labelled Simulation) drives
  Life Safety Point 1 "Amber". `Tracking_Value` follows it; `Present_Value` latches
  `alarm` until a LifeSafetyOperation RESET; a fault follows the detector. Life Safety
  Zone 1 "Azure" rolls Amber up. The Wire Tests move the detector through a
  loopback-only test control channel (`simulation/SimulationControl.*`, off unless
  `BLSC_SIMULATION_CONTROL_PORT` is set; B-GW-CPP's pattern with a generic handler).
- **LifeSafetyOperation is executed** (AE-LS-B, K.2.13). `CMakeLists.txt` defines
  `STACK_OPTION_DM_LSO_LIFE_SAFETY_OPERATION` (the stack's documented public toggle)
  for SOURCE builds; a STATIC/DLL library must be built with it (README "Build").
  Silence / unsilence set `Silenced`; RESET / RESET_ALARM unlatch; a reset while the
  detector still sees smoke, of an object that is not latched, and every RESET_FAULT
  are refused with `OBJECT` / `INVALID_OPERATION_IN_THIS_STATE` (cl. 13.13.1.3.1,
  13.13.2). An operation on the zone also operates its member; one with no object
  operates both. `Operation_Expected` is SILENCE while sounding, RESET once silenced.
- **Wire Tests** `tests/life_safety_test.py` (37 checks + 1 known gap) and
  `tests/fault_test.py` (13), with `tests/lsc_setup.py` and the generic
  `tests/wiretest_extras.py` (proposed for wiretest 0.7.0:
  `life_safety_operation_request`, `announce`, `SimulationControl`,
  `request_with_retry`, life-safety constants). The Peer is `wiretest.Workstation`
  as Device 389107.
- Device `Number_Of_APDU_Retries` served as 3 (12.11.29's suggested default; the
  stack default 0 never retransmits a confirmed notification).

### Changed

- **Alarms go to a recipient**: Notification Class 1 names the operator workstation
  by device instance (389107), confirmed, instead of the local subnet broadcast,
  unconfirmed. The stack binds the address from the workstation's I-Am.
- **`Present_Value` of Amber and Azure is read-only again** (Table 12-18): a client
  can no longer fake or clear an alarm; only the detector and LifeSafetyOperation
  move it. `Mode` stays writable.
- **Reliability**: the callback serves only cl. 13.2.2.2's first stage
  (`no-fault-detected`); the stack's FAULT_LIFE_SAFETY algorithm supplies
  `multi-state-fault` and serves the combination (stack #2359). A draft of this
  release computed the algorithm's answer in the callback too; mutation testing
  showed the stack never used it, so it was removed.
- AcknowledgeAlarm callback: the stack runs the 13.5.2 checks itself and calls the
  callback before the time-stamp check, so the callback now logs who acknowledges
  and leaves the decision to the stack (B-AAC-CPP 1.4.0's finding). Its object check
  was unreachable.
- README (scenario, Failure Modes, what is Simulation, Wire Tests), TUTORIAL, PICS
  (regenerated; Number_Of_APDU_Retries and Reliability rows), AGENTS, TODO (rewritten:
  Zone_Members `[KNOWN GAP]`, untested COV / silent recipient / unsilence),
  release.yml step name. APP_VERSION 1.1.0 -> 1.2.0.

### Verified (2026-10-16)

- Windows 11 STATIC (NMake, VS 18; stack `ReleaseLib|x64` rebuilt with
  `CL=-DSTACK_OPTION_DM_LSO_LIFE_SAFETY_OPERATION`): 0 warnings; Wire Tests 67/67 x3
  (17 + 37 + 13), 1 known gap, no `[RETRY]`.
- Linux WSL Ubuntu 22.04 SOURCE: Wire Tests 67/67 x3, 1 known gap, no `[RETRY]`.
- 13 mutations of `main.cpp` (Linux), each rebuilt (relink checked) and run; every one
  fails at least one check - see the commit message for the failure lines.

## [1.1.0] - unreleased

### Changed

- **`common/` 3.3.0 -> 3.4.0**, copied verbatim from B-SS-CPP `5d78299`: `HelperSendMessage`
  sends a broadcast addressed to this socket's own address to the subnet broadcast instead of
  unicast to itself (stack #3933). README version lines updated.
- **`wiretest/` 0.5.0 -> 0.6.0**, copied verbatim from B-LS-CPP. Backward compatible: it adds
  codec helpers, `wiretest.SimulatedServer`, `wiretest.alarm`, `wiretest.Workstation` and
  `wiretest.wait_until`; the existing tests are unchanged.
- `tests/discovery_test.py` sends its Who-Is once more, with a visible `[RETRY]` line, when no
  I-Am comes back in 3 s (`tools/patch-discovery-whois-retry.py` from the example series).
  Who-Is is unconfirmed, so nothing else retries it, and the example's UDP receive can drop a
  datagram on Windows (`common/` SimpleUDP).
- **Stack Pin moves to `6.x` @ `64b2cfac5` (6.0.23)** from `6.x` @ `abd4cee1`
  (6.0.21), per the series rule that a pin moves to the latest `6.x` whenever an
  example is updated. Based on the local branch `fix-device-version-properties`
  (`0088ccd`, never pushed), which holds every change in `origin/main` plus 12
  newer commits.
- **Life Safety Point 1 "Amber" and Life Safety Zone 1 "Azure" work over the
  wire at this pin.** Up to `abd4cee1` both answered `unknown-object` for almost
  every property (the old TODO.md item 0). Stack #2187 makes the stack fall
  through to this example's callbacks: every listed property reads, and a
  WriteProperty of `Present_Value` drives the intrinsic algorithms. The README,
  TUTORIAL, AGENTS.md, PICS and `main.cpp` header no longer call this example
  blocked; stack #2036 stays open for `Zone_Members` (TODO.md item 1).
- **Protocol_Revision 24 -> 30** (the stack's default at the new pin).
- **Segmentation is now supported** (`segmented-both`), by the stack (stack
  #2992). `docs/PICS.md` section 5 updated and the objects block regenerated.
- **`common/` 3.0.0 -> 3.3.0**, copied verbatim from B-SS-CPP `b640a81`.
- README, TUTORIAL, AGENTS.md and PICS version lines: stack 6.0.23, `common/`
  3.3.0, Protocol_Revision 30. README profile table re-synced. APP_VERSION
  1.0.2 -> 1.1.0.

### Fixed

- **An alarm(2) now raises LIFE_SAFETY_ALARM, as the docs always said.**
  `BACnetStack_SetIntrinsicChangeOfLifeSafetyAlgorithm` was given alarm(2) as an
  `alarmValues` entry, which the stack documents as driving the object to
  OFFNORMAL; it is now a `lifeSafetyAlarmValues` entry. Checked over the wire:
  Amber `Present_Value` 2 -> `Event_State` life-safety-alarm (it read offnormal
  before), 3 -> fault, 0 -> normal, one event notification each.
- **Local_Date and Local_Time read as values.** Executing TimeSynchronization
  makes the stack list both on the Device (135-2024 Table 12-13 footnotes 3 and
  4), but nothing served them, so both reads failed `value-not-initialized`. The
  discovery Wire Test found it. The example now serves them from the host clock,
  updated by TimeSynchronization, the way B-LD-CPP does.
- **AE-LS-B is marked as met in part, not ✅.** AE-LS-B includes executing
  LifeSafetyOperation (135-2024 K.2.13), which the default stack build does not
  compile, so this example cannot silence or reset. TODO.md item 2 no longer
  says DM-LSO-B is "not a BIBB this profile requires" or that enabling it needs
  a stack edit (the example's own CMake can define the option; phase-2 work).
- AE-ACK-B and AE-INFO-B are marked "configured; not verified" until a test
  exercises them.

### Added

- **Wire Tests** (`tests/`, README "Wire Tests"): `tests/discovery_test.py`, the
  series' discovery test from B-SS-CPP `da3e96f` with this example's
  eleven objects as its control. CI runs it on Linux after the smoke
  test. Result: 17/17, 179 reads, all values on Windows (STATIC) and Linux (SOURCE).
- `wiretest/` 0.5.0, the series' shared Wiretest Harness, vendored verbatim from
  B-BBMD-CPP `75aedd8`.

## [1.0.2] - unreleased

### Changed

- **Device renamed from the series' colour placeholder "Rainbow" to "Chipkin
  Example B-LSC"** so devices from different examples in the series are
  distinguishable from each other on the same BACnet network - every example
  previously announced the identical Object_Name "Rainbow", which made two
  examples on one subnet indistinguishable by name. Sub-object names (Analog
  Input 1 "Bronze", etc.) are unchanged - only the Device object's name
  changed. `docs/colour-table.md` (series root) updated to match. APP_VERSION
  bumped 1.0.1 -> 1.0.2.

## [1.0.1] - unreleased

### Fixed

- **`Application_Software_Version` (12) and `Firmware_Revision` (44) were
  hardcoded and stale** - both were separate `"1.0.0"` literals
  (`APPLICATION_SOFTWARE_VERSION` / `FIRMWARE_REVISION`) that never tracked
  this example's actual build, and `Firmware_Revision` was never meant to be
  this example's own version at all - it names the underlying platform.
  Fixed: `Application_Software_Version` now reads `APP_VERSION` directly (one
  source of truth, can't drift from `--version`'s own banner again).
  `Firmware_Revision` is now built at runtime from the CAS BACnet Stack's own
  `BACnetStack_GetAPIMajorVersion()`/`GetAPIMinorVersion()`/
  `GetAPIPatchVersion()`/`GetAPIBuildVersion()` (the same 4 calls
  `common/CASExampleHelper.cpp`'s `PrintVersion()` already uses for the
  startup banner), populated once right after `LoadBACnetFunctions()`
  succeeds into a new `g_firmwareRevision` string. Verified with a real
  ReadProperty against the running device (via `bacpypes3`):
  `Application_Software_Version = "1.0.1"`, `Firmware_Revision = "6.0.21.0"`
  - both now match the actual running build instead of a stale hardcoded
  string.

## [Unreleased]

### Changed

- **Documentation restructured** to match the rest of the example series:
  `README.md` is cut down to what this example is and how to build/run it;
  long-form extension/review material moved to a new `TUTORIAL.md`; a new
  `docs/PICS.md` holds the Protocol Implementation Conformance Statement
  (ANSI/ASHRAE 135 Annex A shape), with its objects-and-properties section
  generated from `docs/objects.json`, which now includes the Device object
  (previously omitted). `AGENTS.md` updated to match the new file layout and
  build section.
- **Build switched from a prebuilt STATIC library to the adapter's default
  SOURCE mode**: `cmake -B build -S .` / `cmake --build build --config
  Release` now compiles the stack's `source/*.cpp` straight into the
  executable - no `tools/build-stack-static.sh` pre-step, no
  `-DCAS_BACNET_STACK_LINK=STATIC` flag. `.github/workflows/release.yml`
  drops the static-library cache/build steps and matrix `lib:` entries,
  asserts `CAS_BACNET_STACK_LINK` is `SOURCE`, records `"link_mode":
  "SOURCE"` in the published metrics JSON, and packages `TUTORIAL.md` and
  `docs/PICS.md` alongside the binary. This is a build-tooling change only;
  `main.cpp` and `common/` are identical regardless of link mode.
- The **⚠ CRITICAL Life Safety Point/Zone limitation**
  ([chipkin/cas-bacnet-stack#2036](https://github.com/chipkin/cas-bacnet-stack/issues/2036))
  is now documented prominently in `README.md`, `TUTORIAL.md`, and
  `docs/PICS.md` (previously only in `README.md` and `TODO.md`), including in
  `docs/PICS.md`'s "standard object types supported" table, which now marks
  Life Safety Point/Zone as configured-but-non-functional with a reference to
  the issue, rather than listing them as working.

## [1.0.0] - unreleased

### Added

- Initial **B-LSC (Life Safety Controller)** profile example for the CAS BACnet
  Stack in C++. Seeded from B-AAC (Advanced Application Controller) minus its
  Schedule/Calendar objects; implements every B-LSC capability the standard
  stack's customer-facing API exposes; documents the one gap in
  [TODO.md](TODO.md).
- Carries the B-SA/B-ASC object model: Device "Rainbow" (389007), read-only
  inputs (Analog "Bronze" / Binary "Emerald" / Multi-State "Hot Pink"),
  commandable outputs (Analog "Chartreuse" / Binary "Fuchsia" / Multi-State
  "Indigo"), and Network Port "Vermilion".
- **F-LIFESAFETY / F-ALARM-LS (canonical for this series):** Life Safety Point 1
  "Amber" (type 21) and Life Safety Zone 1 "Azure" (type 22), each with an
  intrinsic **ChangeOfLifeSafety** algorithm (`SetIntrinsicChangeOfLifeSafetyAlgorithm`)
  and a **fault** algorithm (`SetFaultLifeSafetyAlgorithm`), routed through
  **Notification Class 1 "Crimson"**. `Present_Value` is writable (beyond the
  profile's read-only column) as this tutorial's alarm/fault trigger, mirroring
  B-AAC's Diamond OutOfRange demo - see the file header in `main.cpp` for why
  this example does not use the stack's internal (not customer-exported)
  life-safety engine.
- **LifeSafetyOperation responder implemented**
  (`BACnetStack_RegisterCallbackLifeSafetyOperation`): silence/unsilence (whole,
  audible-only, visual-only) and reset/reset-alarm/reset-fault, applied to Amber,
  Azure, or both. Registered but **not enabled** on the linked static library -
  see "Not yet implemented" below.
- **AE-ACK-B** (AcknowledgeAlarm) and **AE-INFO-B** (GetEventInformation).
- **DS-COV-B:** Analog Input 1 "Bronze" and Life Safety Point 1 "Amber"
  `Present_Value` are COV-subscribable (`SetPropertySubscribable` +
  `SetCOVSettings`/`SetMaxActiveCOVSubscriptions`); this repo implements the
  pattern independently (B-ACCR, the series' intended F-COV canonical source,
  had not landed yet when this repo was built).
- **F-REINIT (canonical for this series):** DM-RD-B - ReinitializeDevice accepts
  COLDSTART/WARMSTART, returns a SimpleACK, then performs the actual restart from
  the main loop after the ACK has had time to reach the wire.
- **DS-RPM-B / DS-WPM-B** (ReadPropertyMultiple / WritePropertyMultiple),
  **DM-DCC-B** (DeviceCommunicationControl), **DM-TS-B / DM-UTC-B**
  (SetSystemTime callback), DM-DDB-A/B and DM-DOB-B discovery, unsolicited
  start-up I-Am and Who-Is to the local subnet.
- Linked against the CAS BACnet Stack `6.x` @ `abd4cee1` (reports 6.0.21) as a
  prebuilt **STATIC** library (`CAS_BACNET_STACK_LINK=STATIC`), built by
  `tools/build-stack-static.sh` from the stack's own project files.
- `common/` vendored at **v2.5.0**, byte-identical to the rest of the series
  (re-synced mid-task, twice: after B-LS's `WriteGroupDemo`/`DiscoverRemote`
  key bump, then again after B-RTR's multi-port `SetupUDP` support landed -
  both purely additive, no `main.cpp` change needed; this example still calls
  the single-argument `SetupUDP(port)` overload, whose behaviour is
  unchanged).
- All required Protocol_Revision 24 properties across every object; strict build
  warnings on the example's own sources.

### Not yet implemented (see [TODO.md](TODO.md))

- **⚠ CRITICAL: Life Safety Point 1 (Amber) and Life Safety Zone 1 (Azure)
  cannot serve almost any property.** Verified by running the built binary
  against a live BACnet client: `Object_List`/`Object_Identifier`/
  `Object_Type` are correct, but `Object_Name`, `Present_Value`,
  `Out_Of_Service`, and the WriteProperty this repo's own alarm/fault demo
  relies on all answer `unknown-object`. Root cause is in the stack
  (`BACnetDBDevice::GetGeneratedPropertyValue`/`SetGeneratedPropertyValue`
  require the internal `BACnetStackLifeSafetyPoint`/`Zone` engine object,
  populated only by the non-customer-exported `AddLifeSafetyPointObject`/
  `AddLifeSafetyZoneObject`), not this example's code. See TODO.md #0 and
  [chipkin/cas-bacnet-stack#2036](https://github.com/chipkin/cas-bacnet-stack/issues/2036).
- **Life Safety Zone 1 (Azure)'s `Zone_Members`** - no customer-facing callback
  can serve this required, constructed `BACnetLIST of BACnetDeviceObjectReference`
  property (`BACnetStack_RegisterCallbackGetPropertyConstructed` is test-tool
  only).
- **LifeSafetyOperation (service 37) not enabled** - confirmed by running the
  built binary: the linked static library was compiled without
  `STACK_OPTION_DM_LSO_LIFE_SAFETY_OPERATION` (not part of this series'
  `STACK_OPTION_TARGET_FULL` build preset). The callback is registered and
  implemented; the service bit is left off rather than advertised-and-broken.

[1.0.0]: https://github.com/chipkin/BACnetProfileExample-B-LSC-CPP/releases/tag/v1.0.0
