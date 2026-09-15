# TODO - B-LSC features not yet implemented

This example implements **as much of the B-LSC (Life Safety Controller) profile
as the standard CAS BACnet Stack's customer-facing API supports today**. The
items below are the parts of B-LSC that are **not** implemented here, why, and
what it would take. They are revisited as the stack gains the capability.

See the README's "What this example does NOT do yet" section for the
user-facing summary; this file is the engineering detail.

## 0. CRITICAL: Life Safety Point/Zone objects cannot serve almost any property via the customer surface

**Status: verified by running the built binary against a live BACnet client
(`bacpypes3`); this is the dominant finding of this task and the reason
`BACnetProfileExample-B-LSC-CPP` cannot yet claim full B-LSC conformance. Not
wave-stopping per the runbook's roadblock protocol, but it materially affects
every later repo seeded from this one (B-ALSC, B-ACC per the runbook's wave
plan) and should be read before building on this example.**

### Reproduction

With the example running and a `bacpypes3` client on the same subnet:

```
ReadProperty life-safety-point,1 . objectIdentifier  -> OK (life-safety-point,1)
ReadProperty life-safety-point,1 . objectType        -> OK (life-safety-point)
ReadProperty life-safety-point,1 . objectName        -> Error(read-property): object: unknown-object
ReadProperty life-safety-point,1 . presentValue      -> Error(read-property): object: unknown-object
ReadProperty life-safety-point,1 . outOfService      -> Error(read-property): object: unknown-object
WriteProperty life-safety-point,1 . presentValue = 2 -> Error(write-property): object: unknown-object
```

`Object_List` correctly lists both `(life-safety-point, 1)` and
`(life-safety-zone, 1)` (so the objects "exist" in the generic sense
`BACnetStack_AddObject` created), and `Object_Identifier`/`Object_Type` read
back correctly - but essentially every other property, on both Amber and
Azure, answers `unknown-object` rather than a value or a property-specific
error. The same is true for a WriteProperty of `Present_Value` (the demo
alarm/fault trigger this README documents) and, by the same code path, `Mode`.

### Root cause (traced in the stack source at the pin)

`BACnetDBDevice::GetGeneratedPropertyValue` and
`BACnetDBDevice::SetGeneratedPropertyValue` (`BACnetDBDevice.cpp`, ~line 10521
and ~line 12786) both special-case `lifeSafetyPoint`/`lifeSafetyZone` **before**
falling through to the generic property-profile/host-callback path that every
other object type in this series uses:

```cpp
if (objectType == BACnetObjectType::lifeSafetyPoint) {
    BACnetStackLifeSafetyPoint* lifeSafetyPoint = this->GetLifeSafetyPoint(objectInstance);
    if (lifeSafetyPoint == NULL) {
        APIDebugLogError("lifeSafetyPoint %u does not exist", objectInstance);
        errorContainer->SetError(BACnetErrorClass::object, BACnetErrorCode::unknownObject);
        return false;
    }
    ...
}
```

`GetLifeSafetyPoint(objectInstance)` looks up the object in
`BACnetDBDevice::m_lifeSafetyPoints`, a map that is populated **only** by
`BACnetDBDevice::AddLifeSafetyPointObject` (and the equivalent
`m_lifeSafetyZones` / `AddLifeSafetyZoneObject` for a Zone) - **not** by the
generic `BACnetStack_AddObject` this example (and every other object type in
this series) uses. Confirmed by reading `BACnetDBDevice::AddObject` and
`BACnetInterface::AddObject` end to end: neither calls
`AddLifeSafetyPointObject`/`AddLifeSafetyZoneObject`, and neither is
`DllExport`ed in `CASBACnetStackDLL.h` (re-confirmed:
`grep -n "DllExport.*LifeSafety" source/CASBACnetStackDLL.h` at the pin lists
only `SetIntrinsicChangeOfLifeSafetyAlgorithm`, `SetFaultLifeSafetyAlgorithm`,
`RegisterCallbackLifeSafetyOperation`, `SendLifeSafetyOperation` - see item 1
below for the related `Zone_Members` finding, which shares this exact root
cause).

So `GetLifeSafetyPoint`/`GetLifeSafetyZone` return `NULL` unconditionally for
any object added the only way the customer API allows, and
`GetGeneratedPropertyValue`/`SetGeneratedPropertyValue` set `unknownObject`
and bail **before ever reaching this example's own `GetProperty*`/
`SetProperty*` callbacks** - so no amount of correct application code in
`main.cpp` can work around it. This is a stack-source-level gap, not a gap in
this example's implementation of the documented customer pattern (which is
otherwise identical, and correct, for every other object in this file).

### What still works

- The objects are created and appear in `Object_List` (so DM-DOB-B's object
  discovery is intact) and answer `Object_Identifier`/`Object_Type` (both are
  resolved by a path that doesn't go through `GetGeneratedPropertyValue`'s
  type dispatch for these two properties).
- The intrinsic `ChangeOfLifeSafety`/fault algorithms, `Notification Class 1`
  "Crimson", `AcknowledgeAlarm`, and `GetEventInformation` are armed and wired
  correctly per the documented, exported API (`SetIntrinsicChangeOfLifeSafetyAlgorithm`,
  `SetFaultLifeSafetyAlgorithm`, `SetAlarmsAndEventsForObjectEnabled`) - but
  **not verified end-to-end** in this task, since driving `Present_Value` via
  WriteProperty (the only trigger this example can offer) is itself blocked by
  this same gap. Whether the algorithm's own internal read of `Present_Value`
  goes through the same blocked path is not yet determined; flagged rather
  than assumed either way.
- Every other object in this repository (Bronze/Emerald/Hot Pink/Chartreuse/
  Fuchsia/Indigo/Crimson/Vermilion) is unaffected - this is specific to the
  `lifeSafetyPoint`/`lifeSafetyZone` object types.

### To do when available

Export `BACnetStack_AddLifeSafetyPointObject(deviceInstance, objectInstance)`
and `BACnetStack_AddLifeSafetyZoneObject(deviceInstance, objectInstance)` (thin
wrappers around the existing, correct internal
`BACnetDBDevice::AddLifeSafetyPointObject`/`AddLifeSafetyZoneObject`) to
`CASBACnetStackDLL.h`. Once available, this example needs a **small, mechanical
change**: call the new export right after `BACnetStack_AddObject` for Amber and
Azure. Everything else in `main.cpp` - the Get/Set callbacks, the intrinsic
algorithm setup, the writable `Present_Value`/`Mode` - can very likely stay,
though re-verify against the internal engine's own Mode/Accepted_Modes/
Silenced semantics once it is actually reachable (see the file header's note
on the simplifications this example currently makes in its place).

**Stack issue:** file with `gh issue create -R chipkin/cas-bacnet-stack`, titled
along the lines of "customer-facing AddLifeSafetyPointObject/
AddLifeSafetyZoneObject exports needed - generic AddObject leaves
lifeSafetyPoint/lifeSafetyZone objects unable to serve ReadProperty/
WriteProperty for any property but Object_Identifier/Object_Type", with this
section's reproduction and source citations. Not wave-stopping, but the
runbook's B-ALSC/B-ACC cards (Wave 3) seed from this repo and should read this
entry first.

## 1. Life Safety Zone 1 (Azure) Zone_Members is not servable

**Status: verified customer-surface gap; shares its root cause with item 0
above (the same `AddLifeSafetyZoneObject`/`SetLifeSafetyZoneMembers` exports
are missing) but is independently true even if item 0 is fixed, unless
`SetLifeSafetyZoneMembers` (or an equivalent) ships too.**

Cl. 12.16.4 requires `Zone_Members`, a `BACnetLIST of BACnetDeviceObjectReference`
(a constructed, variable-length SEQUENCE - device identifier optional + object
identifier). Serving an arbitrary constructed property requires
`BACnetStack_RegisterCallbackGetPropertyConstructed`, and that export was moved to
the test-tool surface (`CASBACnetStackTestToolDLL.h`, behind
`BACNET_STACK_TESTTOOL`) - see the comment next to its old declaration in
`CASBACnetStackDLL.h`: `// PR #193 review: BACnetStack_RegisterCallbackGetPropertyConstructed
moved to CASBACnetStackTestToolDLL.h.`. This series forbids the test-tool surface
(Global constraints, `runbook-update-examples.md`), so there is no customer-facing
way to serve `Zone_Members` at all - not even a partial one, unlike Calendar's
`Date_List` gap in B-AAC (which at least has a *write* path, just no *evaluation*
path).

Checked and ruled out as alternatives:
- `BACnetStack_RegisterCallbackGetPropertyObjectIdentifier` - serves a single
  `BACnetObjectIdentifier`-typed property (or an array of them via
  `useArrayIndex`/`propertyArrayIndex`), not a `BACnetDeviceObjectReference`
  (which additionally carries an optional device identifier). Does not fit the
  type.
- The stack's own internal Life Safety Zone engine
  (`BACnetDBDevice::SetLifeSafetyZoneMembers`) exists and IS correct - it is just
  not exported through `CASBACnetStackDLL.h`.

**To do when available:** either export
`BACnetStack_RegisterCallbackGetPropertyConstructed` (or an equivalent) to the
customer surface, or add a dedicated `BACnetStack_SetLifeSafetyZoneMembers`-style
host-configuration export mirroring the internal `SetLifeSafetyZoneMembers`.

**Stack issue:** fold into the item 0 issue above (same filing;
`gh issue create -R chipkin/cas-bacnet-stack`); not wave-stopping.

## 2. LifeSafetyOperation (service 37) is registered but not enabled

**Status: verified by running the built binary; a build-tooling gap, not an
application-code gap.**

`main.cpp` registers `BACnetStack_RegisterCallbackLifeSafetyOperation` and
implements `LifeSafetyOperation()` (silence/unsilence/reset family, applied to
Amber, Azure, or both). Running the compiled example prints, at start-up:

```
::CASBACnetStack::BACnetInterface::RegisterCallbackLifeSafetyOperation() ...
FYI: This feature was not compiled. To enable, re-compile the CAS BACnet Stack
with this defined: STACK_OPTION_DM_LSO_LIFE_SAFETY_OPERATION
```

Traced in `CASBACnetStackOptions.h` at the pin: `STACK_OPTION_DM_LSO_LIFE_SAFETY_OPERATION`
is auto-defined only when `BACNET_STACK_TESTTOOL` is set (forbidden by this
series) - "customer firmware opts in by defining
`STACK_OPTION_DM_LSO_LIFE_SAFETY_OPERATION` explicitly" (the header's own
comment). Confirmed this is NOT part of the `STACK_OPTION_TARGET_FULL` preset
every example in this series builds with: the `#ifdef STACK_OPTION_ENABLE_ALL_BIBBS`
block that `STACK_OPTION_TARGET_FULL` expands into (`CASBACnetStackOptions.h`
~line 1168) closes at line 1210, well before the LifeSafetyOperation section
(~line 1349) - so the object-TYPE availability gotcha (series runbook gotcha
21: "every object type is compile-gated, and `TARGET_FULL` un-gates them all")
does NOT extend to this particular SERVICE. (This is also moot in practice
until item 0 above is fixed - a LifeSafetyOperation request names an object
that, today, cannot serve `Present_Value`/`Silenced` either way.)

Setting that define requires editing `CASBACnetStackOptions.h` (or the
`CASBACnetStack.vcxproj` preprocessor definitions) inside the `cas-bacnet-stack`
submodule itself - out of scope for this repository (the vendored stack is not
this example's to edit) and out of scope for `tools/build-stack-static.sh`
(series-wide tooling shared by every example, not owned by this task per the
runbook's "touch ONLY BACnetProfileExample-B-LSC-CPP" instruction).

Rather than advertise `Protocol_Services_Supported` bit 37 while the request
processor cannot execute it (a conformance defect worse than not claiming the
bit at all), this example leaves the service **disabled**. DM-LSO-B is not
itself a BIBB the B-LSC profile requires (see the BIBB table in `README.md`) -
only AE-LS-B is.

**To do when available:** if a future series-wide static-library build defines
`STACK_OPTION_DM_LSO_LIFE_SAFETY_OPERATION` (a `tools/build-stack-static.sh`
change, decided series-wide), this example's `main.cpp` needs **zero changes**
beyond adding `SERVICE_LIFE_SAFETY_OPERATION` back to the enabled-services
list, because the callback is already registered and implemented.

**Stack/tooling issue:** fold into the item 0 issue above; not wave-stopping.
