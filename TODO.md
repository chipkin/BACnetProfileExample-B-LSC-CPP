# TODO - B-LSC features not yet implemented

This example implements **as much of the B-LSC (Life Safety Controller) profile
as the standard CAS BACnet Stack's customer-facing API supports today**. The item
below is the part of B-LSC that is **not** implemented here, why, and what it
would take. It is revisited as the stack gains the capability.

See the README's "What this example does NOT do yet" section for the user-facing
summary; this file is the engineering detail.

## 1. Life Safety Zone 1 (Azure) Zone_Members is not servable

**Status: verified customer-surface gap; a stack issue is filed (see the link
below when the issue is created).**

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
  not exported through `CASBACnetStackDLL.h` (confirmed:
  `grep -n "DllExport.*LifeSafety" source/CASBACnetStackDLL.h` at the pin lists
  only `SetIntrinsicChangeOfLifeSafetyAlgorithm`, `SetFaultLifeSafetyAlgorithm`,
  `RegisterCallbackLifeSafetyOperation`, and `SendLifeSafetyOperation` - none of
  the object-configuration API).

Azure is still added (`BACnetStack_AddObject`) and correctly serves every other
required property, with its own independent `Present_Value`/`Tracking_Value`
(not rolled up from members, since there is no way to read the members at all).
`Zone_Members` is accepted with this note rather than faked.

**To do when available:** either export
`BACnetStack_RegisterCallbackGetPropertyConstructed` (or an equivalent) to the
customer surface, or add a dedicated `BACnetStack_SetLifeSafetyZoneMembers`-style
host-configuration export mirroring the internal `SetLifeSafetyZoneMembers`, then
serve `Zone_Members` for real and (optionally) switch Azure's `Present_Value` to a
genuine roll-up of Amber (and any other members) via the internal engine's own
2-tier deterministic convention (`BACnetStackLifeSafetyZone.h`).

**Stack issue:** file with `gh issue create -R chipkin/cas-bacnet-stack` per the
runbook's roadblock protocol; not wave-stopping.
