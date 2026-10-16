# TODO - B-LSC known gaps

This example is **Profile-functional** (series bar, `docs/GLOSSARY.md` in the
examples-series folder): it performs the B-LSC Profile Function against a Peer, shows
its Failure Modes, and proves both with Wire Tests (`tests/`). The items below are what
it still does not do, why, and what it would take. Re-check every entry whenever the
Stack Pin moves. Checked at stack `6.x` @ `64b2cfac5` (6.0.23), 2026-10-16.

## 1. Life Safety Zone 1 (Azure) Zone_Members cannot be set [KNOWN GAP]

`Zone_Members` (cl. 12.16.26, required, a `BACnetLIST of BACnetDeviceObjectReference`)
"shall indicate which Life Safety Point and Life Safety Zone objects are members of the
zone". `main.cpp` rolls Life Safety Point 1 (Amber) up into Azure, so Zone_Members should
name Amber - but it reads as an **empty list**: the stack serves it, and the only way to
fill it is `BACnetStackTestTool_SetLifeSafetyZoneMembers` in `CASBACnetStackTestToolDLL.h`,
which this series may not use. The generic constructed-property callback
(`BACnetStack_RegisterCallbackGetPropertyConstructed`) is test-tool only too.

**Evidence:** `tests/life_safety_test.py` reads it and reports
`[KNOWN GAP] Life Safety Zone 1's Zone_Members lists Life Safety Point 1, the member it
rolls up - stack #2036 ...; read value=None` on every run (the read succeeds with an
empty list). When a customer setter exists the line prints `[GAP CLOSED]`: set the
members in `main()` and turn it into a `record()` check.

**Blocked on** [chipkin/cas-bacnet-stack#2036](https://github.com/chipkin/cas-bacnet-stack/issues/2036)
(open; owner-approved as a customer-interface addition under tracking issue #2159).

## 2. The stack's own life-safety engine is not used

The stack has a Life Safety Point/Zone engine (`BACnetStackLifeSafetyObject` /
`BACnetStackLifeSafetyPoint` / `BACnetStackLifeSafetyZone`: Mode / Accepted_Modes,
latching, zone roll-up), but its configuration surface (`AddLifeSafetyPointObject`,
`AddLifeSafetyZoneObject`, `SetLifeSafetyAcceptedModes`, ...) is not exported through
`CASBACnetStackDLL.h` (#2036 again). So this example holds Amber's and Azure's state
itself (`LifeSafetyObjectState` in `main.cpp`) and applies the latching, silencing and
reset rules in its own callbacks; the stack's intrinsic `CHANGE_OF_LIFE_SAFETY` and
`FAULT_LIFE_SAFETY` algorithms watch the `Present_Value` those callbacks report. This
works - the Wire Tests prove it - but:

- `Mode` is accepted unconditionally: `Accepted_Modes` has no customer setter, and the
  stack's reading of cl. 12.15.13 with an empty `Accepted_Modes` would refuse every Mode
  write.
- A mode change does not run the algorithm's "transition on a pMode change" path
  (cl. 13.3.8), because nothing calls `UpdateValue` for `Mode`. Not tested.

## 3. Not Wire-Tested

- **COV** (DS-COV-B): Amber's and Bronze's `Present_Value` are subscribable and every
  detector change calls `UpdateValue`, but no Wire Test subscribes.
- **A silent recipient**: `Number_Of_APDU_Retries` is served as 3 and the Wire Test
  checks the value, but no test lets a notification go unanswered and counts the
  retransmissions (B-AAC-CPP's `silent_recipient_test.py` does that).
- **UNSILENCE** and the audible/visual-only variants: implemented, not exercised.

## Done in 1.2.0 (was open in 1.1.0)

- LifeSafetyOperation is compiled in (`STACK_OPTION_DM_LSO_LIFE_SAFETY_OPERATION`, the
  stack's documented public toggle, defined in `CMakeLists.txt`) and enabled; AE-LS-B is
  met in full.
- A simulated smoke detector raises the alarm; a client write cannot (`Present_Value`
  is read-only again, as Table 12-18 says).
- The alarms go, confirmed, to a particular recipient (Device 389107, the operator
  workstation), not the subnet broadcast.
- AcknowledgeAlarm and GetEventInformation are exercised end to end.
