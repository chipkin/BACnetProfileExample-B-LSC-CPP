# BACnet Protocol Implementation Conformance Statement (PICS)

For the **BACnet B-LSC (Life Safety Controller) C++ example** -
see [README.md](../README.md).

> This is the PICS **for the example as shipped**. It describes a tutorial
> device announcing itself as a Chipkin demo, not a product. When you turn this
> example into your own device, this document is one of the things you rewrite:
> the vendor, model and version rows all come from the
> `CHANGE ALL OF THIS BEFORE YOU SHIP` block at the top of `main.cpp`. The
> example has **not** been submitted for BTL certification.

> **⚠ Known, filed limitation - read before relying on this document.** Life
> Safety Point 1 ("Amber") and Life Safety Zone 1 ("Azure") are configured and
> present in `Object_List`, but the CAS BACnet Stack's `GetGeneratedPropertyValue`/
> `SetGeneratedPropertyValue` require an internal life-safety engine object that
> `BACnetStack_AddObject` (the only customer-facing way to create one) never
> populates, so **almost every property on these two objects currently answers
> `unknown-object` over the wire**, including the `Present_Value` WriteProperty
> this document's alarm/fault procedures describe. `Object_Identifier` and
> `Object_Type` are the only exceptions. This is a stack-source defect, not an
> error in this example's code - see
> [chipkin/cas-bacnet-stack#2036](https://github.com/chipkin/cas-bacnet-stack/issues/2036)
> and [TODO.md #0](../TODO.md). Section 6 and section 11 mark both objects
> accordingly rather than claiming they work.

## 1. Product description

| | |
|---|---|
| **Vendor Name** | Chipkin Automation Systems |
| **Vendor Identifier** | 389 |
| **Product Name** | CAS BACnet Stack Example - B-LSC |
| **Product Model Number** | CAS BACnet Stack Example - B-LSC |
| **Application Software Version** | 1.0.0 |
| **Firmware Revision** | 1.0.0 |
| **BACnet Protocol Version** | 1 |
| **BACnet Protocol Revision** | 24 |

**Product Description:** a BACnet/IP device built on the CAS BACnet Stack that
implements as much of the B-LSC (Life Safety Controller) profile as the
stack's customer-facing API supports today. It answers ReadProperty /
ReadPropertyMultiple, accepts WriteProperty / WritePropertyMultiple, accepts
SubscribeCOV, generates intrinsic life-safety event notifications, accepts
AcknowledgeAlarm and GetEventInformation, synchronises its clock, and handles
DeviceCommunicationControl and ReinitializeDevice. It is a tutorial for
implementers of the B-LSC profile, and it documents - rather than hides - the
one capability the linked stack cannot yet deliver (see the callout above).

## 2. BACnet standardized device profile (Annex L)

**B-LSC - BACnet Life Safety Controller.**

This device claims exactly one profile. Because the B-LSC requirements are a
superset of B-GENERAL's, a conformant B-LSC device also satisfies
**B-GENERAL** (Annex L.8); that is subsumption, not a second claim. The
example does **not** claim BTL certification - see the callout at the top of
this document.

## 3. BIBBs supported (Annex K)

| BIBB | Description |
|---|---|
| DS-RP-B | Data Sharing - ReadProperty - B |
| DS-RPM-B | Data Sharing - ReadPropertyMultiple - B |
| DS-WP-B | Data Sharing - WriteProperty - B |
| DS-WPM-B | Data Sharing - WritePropertyMultiple - B |
| DS-COV-B | Data Sharing - COV - B |
| AE-LS-B | Alarm and Event - Life Safety - B |
| AE-ACK-B | Alarm and Event - ACK - B |
| AE-INFO-B | Alarm and Event - Information - B |
| DM-DDB-A | Device Management - Dynamic Device Binding - A |
| DM-DDB-B | Device Management - Dynamic Device Binding - B |
| DM-DOB-B | Device Management - Dynamic Object Binding - B |
| DM-DCC-B | Device Management - Device Communication Control - B |
| DM-TS-B | Device Management - Time Synchronization - B |
| DM-UTC-B | Device Management - UTC Time Synchronization - B |
| DM-RD-B | Device Management - Reinitialize Device - B |

No other BIBBs are supported. In particular this device does **not** support
scheduling (SCHED-*), trending (T-*), access control (any AC-*/ACC-*), or
routing/gateway BIBBs. **DM-LSO-B** (LifeSafetyOperation) is implemented in
`main.cpp` and its callback is registered, but the service is **not enabled**
on the linked stack build (see section 4) - DM-LSO-B is not itself a BIBB
B-LSC requires, so this does not affect the table above.

## 4. Application services supported

| Service | Initiate | Execute |
|---|:---:|:---:|
| ReadProperty | no | **yes** |
| ReadPropertyMultiple | no | **yes** |
| WriteProperty | no | **yes** |
| WritePropertyMultiple | no | **yes** |
| SubscribeCOV | no | **yes** |
| ConfirmedEventNotification / UnconfirmedEventNotification | **yes** | - |
| AcknowledgeAlarm | no | **yes** |
| GetEventInformation | no | **yes** |
| Who-Is | no | **yes** |
| I-Am | **yes** | - |
| Who-Has | no | **yes** |
| I-Have | **yes** | - |
| DeviceCommunicationControl | no | **yes** |
| ReinitializeDevice | no | **yes** |
| TimeSynchronization / UTCTimeSynchronization | no | **yes** |
| LifeSafetyOperation | no | no *(registered in code, not enabled on the linked build - see below)* |

An unsolicited I-Am is broadcast to the local subnet at start-up, as is a
Who-Is (DM-DDB-A).

**LifeSafetyOperation is implemented but not enabled.** `main.cpp` registers
`BACnetStack_RegisterCallbackLifeSafetyOperation` and implements the
silence/unsilence/reset handling, but the linked CAS BACnet Stack build was
compiled without `STACK_OPTION_DM_LSO_LIFE_SAFETY_OPERATION` (not part of the
`STACK_OPTION_TARGET_FULL` preset this series builds with), so the stack logs
*"This feature was not compiled"* at start-up and the example deliberately
leaves `Protocol_Services_Supported` bit 37 off rather than advertise a
service the linked build cannot execute. See [TODO.md #2](../TODO.md).

Any other confirmed service is rejected.

## 5. Segmentation capability

Segmentation is **not supported** in either direction
(`Segmentation_Supported` = `no-segmentation`). `Max_APDU_Length_Accepted` is
1476 octets, the BACnet/IP maximum.

## 6. Standard object types supported

No object is dynamically creatable or deletable.

| Object type | Instance | Object_Name | Writable properties | Optional properties supported |
|---|:---:|---|---|---|
| Device | 389007 | Rainbow | - | Description |
| Analog Input | 1 | Bronze | - | - |
| Binary Input | 1 | Emerald | - | - |
| Multi-State Input | 1 | Hot Pink | - | State_Text |
| Analog Output | 1 | Chartreuse | Present_Value (commandable) | - |
| Binary Output | 1 | Fuchsia | Present_Value (commandable) | - |
| Multi-State Output | 1 | Indigo | Present_Value (commandable) | - |
| Life Safety Point ⚠ | 1 | Amber | Present_Value, Mode *(configured in code; not currently readable/writable over the wire - see the callout above)* | - |
| Life Safety Zone ⚠ | 1 | Azure | Present_Value, Mode *(configured in code; not currently readable/writable over the wire - see the callout above)* | - |
| Notification Class | 1 | Crimson | - | - |
| Network Port | 1 | Vermilion | - | - |

**Life Safety Point and Life Safety Zone are configured but not currently
functional over the wire** - see the callout at the top of this document and
[chipkin/cas-bacnet-stack#2036](https://github.com/chipkin/cas-bacnet-stack/issues/2036).
Every other object type in this table is fully functional and was implemented
using the documented, exported `BACnetStack_AddObject` pattern.

The device instance is configurable at run time with `--deviceID` (BACnet
requires the device instance to be configurable).

## 7. Data link layer options

**BACnet/IP (Annex J)**, UDP port 47808 (0xBAC0) by default, configurable at
run time with `--port`.

BBMD is not supported, Foreign Device registration is not supported, and
BACnet/SC, MS/TP, Ethernet (Annex H) and PTP are not supported.

## 8. Device address binding

Static device binding is **not supported**. The device resolves alarm
recipients named by address directly, and by device instance via its own
Device-Address-Binding cache populated from Who-Is/I-Am traffic; it does not
require a pre-configured binding table.

## 9. Networking options

None. The device is not a router, not a BBMD, and does not register as a
foreign device.

## 10. Character sets supported

UTF-8 (ANSI X3.4). Supporting a character set does not imply the device can
handle data in all character sets.

## 11. Objects and properties

<!-- OBJECTS-PROPERTIES:BEGIN (generated by tools/gen-objects-properties.py from docs/objects.json - do not edit here) -->
Every object this example creates, and every REQUIRED property of each (per ANSI/ASHRAE 135-2024 clause 12 and the stack's `docs/property-profile-reference.md`), plus the optional properties the example turns on. **Served by** says who answers a ReadProperty: the **stack** generates it, or the **app** serves it from a `GetProperty*` callback in `main.cpp`. A ⚠ row is a required property the app does not serve and the stack would fill with a default - that is a defect, not a feature.

### Device 389007 "Rainbow" - the device itself; the instance is configurable with --deviceID. The stack rows are device-wide facts only the stack knows - the protocol version and revision it implements, the services and object types it was configured with, the live object list and address-binding table. The accepted rows are the stack's configured defaults for APDU limits, segmentation, system status and database revision; an application that answered them from its own constants could contradict the stack, so this example does not

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| System_Status | BACnetDeviceStatus | stack default, accepted (Generic Enumerated default: `0`) | no |
| Vendor_Name | CharacterString | app | no |
| Vendor_Identifier | Unsigned16 | app | no |
| Model_Name | CharacterString | app | no |
| Firmware_Revision | CharacterString | app | no |
| Application_Software_Version | CharacterString | app | no |
| Description *(optional, enabled)* | CharacterString | app | no |
| Protocol_Version | Unsigned | stack | no |
| Protocol_Revision | Unsigned | stack | no |
| Protocol_Services_Supported | BACnetServicesSupported | stack | no |
| Protocol_Object_Types_Supported | BACnetObjectTypesSupported | stack | no |
| Object_List | BACnetARRAY[N] of BACnetObjectIdentifier | stack | no |
| Max_APDU_Length_Accepted | Unsigned | stack default, accepted (`CAS_BACNET_DEVICE_DEFAULT_MAX_APDU_LENGTH_ACCEPTED`) | no |
| Segmentation_Supported | BACnetSegmentation | stack default, accepted (`BACnetSegmentation::noSegmentation`) | no |
| APDU_Timeout | Unsigned | stack default, accepted (`CAS_BACNET_DEVICE_DEFAULT_APDU_TIMEOUT`) | no |
| Number_Of_APDU_Retries | Unsigned | stack default, accepted (`CAS_BACNET_DEVICE_DEFAULT_NUMBER_OF_APDU_RETRIES`) | no |
| Device_Address_Binding | BACnetLIST of BACnetAddressBinding | stack | no |
| Database_Revision | Unsigned | stack default, accepted (Generic UnsignedInteger default: `0`) | no |
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |

### Analog Input 1 "Bronze" - REAL, degrees Celsius; starts at 21.5. Present_Value is DS-COV-B subscribable (BACnetStack_SetPropertySubscribable); the up/down key feeds subscribers via BACnetStack_UpdateValue

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Present_Value | Real | app | no |
| Status_Flags | BACnetStatusFlags | stack | no |
| Event_State | BACnetEventState | stack default, accepted (Generic Enumerated default: `0`) | no |
| Out_Of_Service | Boolean | app | no |
| Units | BACnetEngineeringUnits | app | no |
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |

### Binary Input 1 "Emerald" - starts inactive

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Present_Value | BACnetBinaryPV | app | no |
| Status_Flags | BACnetStatusFlags | stack | no |
| Event_State | BACnetEventState | stack default, accepted (Generic Enumerated default: `0`) | no |
| Out_Of_Service | Boolean | app | no |
| Polarity | BACnetPolarity | app | no |
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |

### Multi-state Input 1 "Hot Pink" - state 1 of 3: On, Off, Auto

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Present_Value | Unsigned | app | no |
| Status_Flags | BACnetStatusFlags | stack | no |
| Event_State | BACnetEventState | stack default, accepted (Generic Enumerated default: `0`) | no |
| Out_Of_Service | Boolean | app | no |
| Number_Of_States | Unsigned | app | no |
| State_Text *(optional, enabled)* | BACnetARRAY[N] of CharacterString | app | no |
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |

### Analog Output 1 "Chartreuse" - F-OUTPUTS (canonical: B-SA). commandable; 16-slot Priority_Array, Relinquish_Default 20.0 C, served by GetPropertyReal. Present_Value, Priority_Array and Current_Command_Priority are resolved by the stack from the priority array

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Present_Value | Real | stack | yes |
| Status_Flags | BACnetStatusFlags | stack | no |
| Event_State | BACnetEventState | stack default, accepted (Generic Enumerated default: `0`) | no |
| Out_Of_Service | Boolean | app | no |
| Units | BACnetEngineeringUnits | app | no |
| Priority_Array | BACnetARRAY[16] of BACnetOptionalReal | stack | no |
| Relinquish_Default | Real | app | no |
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |
| Current_Command_Priority | BACnetOptionalUnsigned | stack | no |

### Binary Output 1 "Fuchsia" - F-OUTPUTS. commandable; 16-slot Priority_Array, Relinquish_Default inactive, served by GetPropertyEnumerated

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Present_Value | BACnetBinaryPV | stack | yes |
| Status_Flags | BACnetStatusFlags | stack | no |
| Event_State | BACnetEventState | stack default, accepted (Generic Enumerated default: `0`) | no |
| Out_Of_Service | Boolean | app | no |
| Polarity | BACnetPolarity | app | no |
| Priority_Array | BACnetARRAY[16] of BACnetOptionalBinaryPV | stack | no |
| Relinquish_Default | BACnetBinaryPV | app | no |
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |
| Current_Command_Priority | BACnetOptionalUnsigned | stack | no |

### Multi-state Output 1 "Indigo" - F-OUTPUTS. commandable; 16-slot Priority_Array, Relinquish_Default state 1 of 3, served by GetPropertyUnsignedInteger

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Present_Value | Unsigned | stack | yes |
| Status_Flags | BACnetStatusFlags | stack | no |
| Event_State | BACnetEventState | stack default, accepted (Generic Enumerated default: `0`) | no |
| Out_Of_Service | Boolean | app | no |
| Number_Of_States | Unsigned | app | no |
| Priority_Array | BACnetARRAY[16] of BACnetOptionalUnsigned | stack | no |
| Relinquish_Default | Unsigned | app | no |
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |
| Current_Command_Priority | BACnetOptionalUnsigned | stack | no |

### Life Safety Point 1 "Amber" - VERIFIED CRITICAL GAP (TODO.md #0): almost every property on this object currently answers unknown-object over the wire, because BACnetStack_AddObject never populates the stack internal life-safety engine object (AddLifeSafetyPointObject/AddLifeSafetyZoneObject are not customer-exported) - see chipkin/cas-bacnet-stack#2036. F-LIFESAFETY / F-ALARM-LS (canonical: B-LSC). BACnetLifeSafetyState Present_Value: quiet(0)/alarm(2)/fault(3); Tracking_Value mirrors it (this example's simplification - see main.cpp file header for why it does not use the stack's internal, non-customer-exported life-safety engine). Present_Value is made WRITABLE beyond the profile's read-only column purely as this tutorial's alarm/fault trigger (mirrors B-AAC's Diamond). Mode is required-writable by the profile and accepted unconditionally (no customer-facing Accepted_Modes setter exists - see the file header); Accepted_Modes is therefore accepted at the stack's generic default. Event_State is NOT actually a stack default here - it is genuinely computed, because this example arms ChangeOfLifeSafety + fault intrinsic algorithms on Amber (SetIntrinsicChangeOfLifeSafetyAlgorithm/SetFaultLifeSafetyAlgorithm); it is marked accepted only because property-profile-reference.md's generic table does not know an algorithm was armed (same convention as B-AAC's Diamond). Present_Value is also DS-COV-B subscribable

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Present_Value | BACnetLifeSafetyState | app | yes |
| Tracking_Value | BACnetLifeSafetyState | app | no |
| Status_Flags | BACnetStatusFlags | stack | no |
| Event_State | BACnetEventState | stack default, accepted (Generic Enumerated default: `0`) | no |
| Reliability | BACnetReliability | app | no |
| Out_Of_Service | Boolean | app | no |
| Mode | BACnetLifeSafetyMode | app | yes |
| Accepted_Modes | BACnetLIST of BACnetLifeSafetyMode | stack default, accepted (Generic Enumerated default: `0`) | no |
| Silenced | BACnetSilencedState | app | no |
| Operation_Expected | BACnetLifeSafetyOperation | app | no |

### Life Safety Zone 1 "Azure" - VERIFIED CRITICAL GAP (TODO.md #0): almost every property on this object currently answers unknown-object over the wire, because BACnetStack_AddObject never populates the stack internal life-safety engine object (AddLifeSafetyPointObject/AddLifeSafetyZoneObject are not customer-exported) - see chipkin/cas-bacnet-stack#2036. F-LIFESAFETY / F-ALARM-LS. Event_State is genuinely computed (same as Amber's - ChangeOfLifeSafety + fault algorithms armed on Azure too), marked accepted for the same generator-limitation reason. Same shape as Amber (this example holds Azure's own independent Present_Value rather than rolling it up from Zone_Members - the stack's real zone roll-up engine is not customer-exported, see the file header). Zone_Members (cl. 12.16, required, BACnetLIST of BACnetDeviceObjectReference) has NO servable path on the customer surface: the only generic constructed-property callback, BACnetStack_RegisterCallbackGetPropertyConstructed, was moved to the test-tool DLL (CASBACnetStackDLL.h's own PR #193 review comment) and is forbidden by this series. See TODO.md and the filed stack issue - this is a real, verified gap, not a convenience shortcut

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Present_Value | BACnetLifeSafetyState | app | yes |
| Tracking_Value | BACnetLifeSafetyState | app | no |
| Status_Flags | BACnetStatusFlags | stack | no |
| Event_State | BACnetEventState | stack default, accepted (Generic Enumerated default: `0`) | no |
| Reliability | BACnetReliability | app | no |
| Out_Of_Service | Boolean | app | no |
| Mode | BACnetLifeSafetyMode | app | yes |
| Accepted_Modes | BACnetLIST of BACnetLifeSafetyMode | stack default, accepted (Generic Enumerated default: `0`) | no |
| Silenced | BACnetSilencedState | app | no |
| Operation_Expected | BACnetLifeSafetyOperation | app | no |
| Zone_Members | BACnetLIST of BACnetDeviceObjectReference | stack default, accepted (None known - a read fails with `unknown-property` or an empt) | no |
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |

### Notification Class 1 "Crimson" - Priority, Ack_Required and Recipient_List are NOT stack DEFAULTS - they are genuinely populated, by BACnetStack_AddNotificationClassObject (Priority, Ack_Required) and BACnetStack_AddRecipientToNotificationClass (Recipient_List) at start-up. They are marked accepted only because property-profile-reference.md's generic per-type table does not know about this object-specific host-configuration API and so cannot credit them as stack-served. Routes Amber's and Azure's CHANGE_OF_LIFE_SAFETY / fault notifications

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Priority | BACnetARRAY[3] of Unsigned | stack default, accepted (Generic UnsignedInteger default: `0`) | no |
| Ack_Required | BACnetEventTransitionBits | stack default, accepted (Generic BitString default: empty bitstring (zero bits - NOT ) | no |
| Recipient_List | BACnetLIST of BACnetDestination | stack default, accepted (None known - a read fails with `unknown-property` or an empt) | no |
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |

### Network Port 1 "Vermilion" - BACnet/IP; Network_Type and Protocol_Level are set from BACnetStack_AddNetworkPortObject()'s arguments (IPv4, BACnet Application) at start-up, not a GetProperty callback like the object's other app-served rows; Changes_Pending is likewise computed and answered natively by the stack's Network Port object. Reliability has no fault condition this example detects, so it is accepted at the generic default (normal)

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Status_Flags | BACnetStatusFlags | stack | no |
| Reliability | BACnetReliability | stack default, accepted (Generic Enumerated default: `0`) | no |
| Out_Of_Service | Boolean | app | no |
| Network_Type | BACnetNetworkType | app | no |
| Protocol_Level | BACnetProtocolLevel | app | no |
| Changes_Pending | Boolean | app | no |
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |

<!-- OBJECTS-PROPERTIES:END -->

**Reading the Life Safety Point/Zone rows above:** `app` in `docs/objects.json`
lists the `GetProperty*`/`SetProperty*` callback that exists in `main.cpp` for
each property - the generator can only see that the code *would* serve the
property, not that the stack's `unknown-object` short-circuit currently
prevents it from ever being reached on the wire for Life Safety Point/Zone.
That is why no row below is flagged with the generator's own `⚠` (a `⚠` means
"no callback exists at all", which is not this defect) - the gap is
runtime-only and is documented here, in the callout above, in the README, and
in [TODO.md #0](../TODO.md) instead. A `⚠` row on any *other* object below
would indicate a real, different defect.

## 12. References

- ANSI/ASHRAE Standard 135-2024, Annex A (PICS template), Annex K (BIBBs),
  Annex L (device profiles), Clause 12 (object types), Clause 13 (alarm and
  event services).
- [README.md](../README.md) - what this example is and how to build it.
- [TUTORIAL.md](../TUTORIAL.md) - how to extend it, and how to keep this
  document honest when you do.
- [TODO.md](../TODO.md) - the engineering detail behind the ⚠ callouts above.
- [chipkin/cas-bacnet-stack#2036](https://github.com/chipkin/cas-bacnet-stack/issues/2036) -
  the filed stack defect.
