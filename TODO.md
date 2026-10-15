# TODO - B-LSC features not yet implemented

This example implements as much of the B-LSC (Life Safety Controller) profile as the
CAS BACnet Stack's customer-facing API supports. The items below are what it does not do,
why, and what it would take. Re-check every entry whenever the Stack Pin moves.
Checked at stack `6.x` @ `64b2cfac5` (6.0.23), 2026-10-15.

The old item 0 ("Life Safety Point/Zone objects cannot serve almost any property") is
gone: at `64b2cfac5` the stack falls through to this example's callbacks (stack #2187), the
discovery Wire Test reads every property both objects list, and a WriteProperty of
`Present_Value` = alarm / fault / quiet moves `Event_State` to life-safety-alarm / fault /
normal with an event notification each time. Stack issue
[#2036](https://github.com/chipkin/cas-bacnet-stack/issues/2036) (customer
`AddLifeSafetyPointObject` / `AddLifeSafetyZoneObject` exports) is still open, for item 1
and for the stack's own life-safety engine (Mode / Accepted_Modes / Silenced / zone
roll-up), which this example still stands in for with its own globals.

## 1. Life Safety Zone 1 (Azure) Zone_Members cannot be set

`Zone_Members` (cl. 12.16, required, a `BACnetLIST of BACnetDeviceObjectReference`) is listed
and reads as an **empty list** at `64b2cfac5`: the stack serves it, but the only way to fill
it is `BACnetStackTestTool_SetLifeSafetyZoneMembers` in `CASBACnetStackTestToolDLL.h`, and
this series uses the customer interface only. The generic constructed-property callback
(`BACnetStack_RegisterCallbackGetPropertyConstructed`) is test-tool only too.

**Blocked on** [chipkin/cas-bacnet-stack#2036](https://github.com/chipkin/cas-bacnet-stack/issues/2036)
(open; owner-approved as a customer-interface addition under tracking issue #2159). When a
customer setter exists, set Zone 1's members to Life Safety Point 1.

## 2. LifeSafetyOperation (service 37) is registered but not enabled

`main.cpp` registers `BACnetStack_RegisterCallbackLifeSafetyOperation` and implements
`LifeSafetyOperation()` (silence / unsilence / reset, applied to Amber, Azure or both). The
stack's default preset (`STACK_OPTION_TARGET_FULL`) does not compile the service:
`STACK_OPTION_DM_LSO_LIFE_SAFETY_OPERATION` is set only by `BACNET_STACK_TESTTOOL`. At start-up
the example prints

```
::CASBACnetStack::BACnetInterface::RegisterCallbackLifeSafetyOperation() ... - FYI: This
feature was not compiled. To enable, re-compile the CAS BACnet Stack with this defined:
STACK_OPTION_DM_LSO_LIFE_SAFETY_OPERATION
```

and leaves the service bit off rather than advertise a service it cannot execute.

**This is a real gap in the profile, not an optional extra.** An earlier version of this file
said DM-LSO-B is not a B-LSC BIBB, "only AE-LS-B is". That is wrong: AE-LS-B itself lists
LifeSafetyOperation under Execute (135-2024 K.2.13: "Life safety device B is able to generate
life safety notifications and is able to process silence and reset operations on its life
safety objects"). So AE-LS-B is met only in part.

**To do (phase 2):** the earlier claim that turning the option on needs an edit inside the
stack submodule is also wrong. This example's own `CMakeLists.txt` can add it to the adapter
target, the way `BACnetProfileExample-B-LS-CPP` adds `STACK_OPTION_WRITE_GROUP`:

```cmake
target_compile_definitions(cas_bacnet_stack_adapter PRIVATE STACK_OPTION_DM_LSO_LIFE_SAFETY_OPERATION)
```

(SOURCE build; a prebuilt STATIC/DLL library must be built with it.) Then add
`SERVICE_LIFE_SAFETY_OPERATION` to the enabled services and prove silence/reset with a Wire
Test. Not done in the phase-1 update because it changes behaviour that has never been tested.

## 3. Profile-functional gaps (phase 2)

- **Only a client write raises an alarm.** There is no simulated detector; the alarm demo is a
  WriteProperty of `Present_Value`.
- **No particular recipient.** Notification Class 1 "Crimson" sends to the local subnet
  broadcast, unconfirmed, so no Peer is addressed and nothing waits for an acknowledgement.
- **AcknowledgeAlarm and GetEventInformation** are enabled but have not been exercised end to
  end at this pin.
- **No Wire Test of the Profile Function yet.** `tests/` has the discovery test only.
