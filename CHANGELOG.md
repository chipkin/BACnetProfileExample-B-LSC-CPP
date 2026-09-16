# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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
  against a live `bacpypes3` client: `Object_List`/`Object_Identifier`/
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
