# BACnet Protocol Implementation Conformance Statement (PICS)

For the **BACnet B-LSC (Life Safety Controller) C++ example** -
see [README.md](../README.md).

> This is the PICS **for the example as shipped**. It describes a tutorial
> device announcing itself as a Chipkin demo, not a product. When you turn this
> example into your own device, this document is one of the things you rewrite:
> the vendor, model and version rows all come from the
> `CHANGE ALL OF THIS BEFORE YOU SHIP` block at the top of `main.cpp`. The
> example has **not** been submitted for BTL certification.

> **Status at stack `6.x` @ `64b2cfac5`.** The example is Profile-functional:
> a simulated smoke detector drives Life Safety Point 1 ("Amber") and Life
> Safety Zone 1 ("Azure") to alarm, the alarms go confirmed to the operator
> workstation (Device 389107), and LifeSafetyOperation silences and resets
> them - each step proved by the Wire Tests. Still missing: Zone 1's
> `Zone_Members` is always empty (no customer setter,
> [chipkin/cas-bacnet-stack#2036](https://github.com/chipkin/cas-bacnet-stack/issues/2036)).
> See [TODO.md](../TODO.md).

## 1. Product description

| | |
|---|---|
| **Vendor Name** | Chipkin Automation Systems |
| **Vendor Identifier** | 389 |
| **Product Name** | CAS BACnet Stack Example - B-LSC |
| **Product Model Number** | CAS BACnet Stack Example - B-LSC |
| **Application Software Version** | 1.2.0 |
| **Firmware Revision** | 6.0.23.0 (the CAS BACnet Stack version) |
| **BACnet Protocol Version** | 1 |
| **BACnet Protocol Revision** | 30 |

**Product Description:** a BACnet/IP life safety controller built on the CAS
BACnet Stack. A smoke detector (simulated - the example's only input from the
physical world) is Life Safety Point 1; its zone is Life Safety Zone 1. The
device latches the detector's alarm, sends `CHANGE_OF_LIFE_SAFETY` event
notifications to an operator workstation, and executes LifeSafetyOperation
(silence / unsilence / reset), AcknowledgeAlarm and GetEventInformation. It
also answers ReadProperty / ReadPropertyMultiple, accepts WriteProperty /
WritePropertyMultiple and SubscribeCOV, synchronises its clock, and handles
DeviceCommunicationControl and ReinitializeDevice. It is a tutorial for
implementers of the B-LSC profile, and it documents - rather than hides - the
one thing the customer interface cannot yet deliver (see the callout above).

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
routing/gateway BIBBs. The Annex L.5 table's B-LSC column is DS-RP-B, DS-WP-B,
DS-COV-B, AE-LS-B, AE-ACK-B, AE-INFO-B, DM-DDB-A,B, DM-DOB-B and DM-DCC-B;
DS-RPM-B, DS-WPM-B, DM-TS-B, DM-UTC-B and DM-RD-B are kept from the B-AAC base
this example was seeded from. AE-LS-B's LifeSafetyOperation (Execute, 135-2024
K.2.13: "able to process silence and reset operations on its life safety
objects") is executed - see section 4.

## 4. Application services supported

| Service | Initiate | Execute |
|---|:---:|:---:|
| ReadProperty | no | **yes** |
| ReadPropertyMultiple | no | **yes** |
| WriteProperty | no | **yes** |
| WritePropertyMultiple | no | **yes** |
| SubscribeCOV | no | **yes** |
| ConfirmedEventNotification | **yes** | - |
| AcknowledgeAlarm | no | **yes** |
| GetEventInformation | no | **yes** |
| Who-Is | no | **yes** |
| I-Am | **yes** | - |
| Who-Has | no | **yes** |
| I-Have | **yes** | - |
| DeviceCommunicationControl | no | **yes** |
| ReinitializeDevice | no | **yes** |
| TimeSynchronization / UTCTimeSynchronization | no | **yes** |
| LifeSafetyOperation | no | **yes** |

An unsolicited I-Am is broadcast to the local subnet at start-up, as is a
Who-Is (DM-DDB-A).

**Event notifications** go to the one recipient in Notification Class 1's
`Recipient_List`: Device 389107 (named by device instance), confirmed, every
transition. `Number_Of_APDU_Retries` is 3, so an unanswered notification is
sent again.

**LifeSafetyOperation** needs `STACK_OPTION_DM_LSO_LIFE_SAFETY_OPERATION`, the
stack's documented public toggle (not part of the `STACK_OPTION_TARGET_FULL`
preset). `CMakeLists.txt` defines it for a SOURCE build; a prebuilt STATIC/DLL
library must be built with it (README "Build"). A RESET of an object that is
not latched in alarm, or whose detector still sees smoke, and every
RESET_FAULT, are refused with `OBJECT` / `INVALID_OPERATION_IN_THIS_STATE`
(cl. 13.13.1.3.1). An operation addressed to Zone 1 also operates its member,
Point 1; one with no object operates both.

Any other confirmed service is rejected.

## 5. Segmentation capability

Segmentation is **supported in both directions**
(`Segmentation_Supported` = `segmented-both`), by the stack: at `6.x` @
`64b2cfac5` the customer build compiles `STACK_OPTION_SEGMENTATION` in (stack
#2992), and this example keeps the stack's defaults for `Max_Segments_Accepted`
and `APDU_Segment_Timeout`. Table 12-13 footnote 1: "These properties are
required if, and shall be present only if, segmentation of any kind is
supported" - they are not rows in section 11 because its generator lists only
unconditionally required properties. `Max_APDU_Length_Accepted` is 1476 octets,
the BACnet/IP maximum. (At the previous pin, `abd4cee1`, this example answered
`no-segmentation`.)

## 6. Standard object types supported

No object is dynamically creatable or deletable.

| Object type | Instance | Object_Name | Writable properties | Optional properties supported |
|---|:---:|---|---|---|
| Device | 389007 | Chipkin Example B-LSC | - | Description |
| Analog Input | 1 | Bronze | - | - |
| Binary Input | 1 | Emerald | - | - |
| Multi-State Input | 1 | Hot Pink | - | State_Text |
| Analog Output | 1 | Chartreuse | Present_Value (commandable) | - |
| Binary Output | 1 | Fuchsia | Present_Value (commandable) | - |
| Multi-State Output | 1 | Indigo | Present_Value (commandable) | - |
| Life Safety Point | 1 | Amber | Mode | - |
| Life Safety Zone | 1 | Azure | Mode | - |
| Notification Class | 1 | Crimson | - | - |
| Network Port | 1 | Vermilion | - | - |

Every object in this table was added with the documented, exported
`BACnetStack_AddObject` pattern and, at `6.x` @ `64b2cfac5`, serves every
property it lists. Life Safety Zone 1's `Zone_Members` reads as an empty list:
the customer interface cannot set it (see the callout at the top).

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

### Device 389007 "Chipkin Example B-LSC" - the device itself; the instance is configurable with --deviceID. The stack rows are device-wide facts only the stack knows - the protocol version and revision it implements, the services and object types it was configured with, the live object list and address-binding table. Number_Of_APDU_Retries is served by the app as 3 (12.11.29 suggested default; the stack default of 0 would never retransmit a ConfirmedEventNotification). The accepted rows are the stack's configured defaults for APDU limits, segmentation, system status and database revision; an application that answered them from its own constants could contradict the stack, so this example does not

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
| Segmentation_Supported | BACnetSegmentation | stack default, accepted (`BACnetSegmentation::segmentedBoth` with `STACK_OPTION_SEGME) | no |
| APDU_Timeout | Unsigned | stack default, accepted (`CAS_BACNET_DEVICE_DEFAULT_APDU_TIMEOUT`) | no |
| Number_Of_APDU_Retries | Unsigned | app | no |
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

### Life Safety Point 1 "Amber" - The smoke detector. F-LIFESAFETY / F-ALARM-LS (canonical: B-LSC). Tracking_Value follows the SIMULATED detector (main.cpp section 1b: quiet(0) / alarm(2) / fault(3)); Present_Value LATCHES alarm until a LifeSafetyOperation RESET, which is refused (OBJECT / INVALID_OPERATION_IN_THIS_STATE) while the detector still sees smoke or when nothing is latched; a fault follows the detector and is not latched. Present_Value and Tracking_Value are read-only (Table 12-18). Silenced and Operation_Expected (SILENCE while sounding, RESET once silenced) follow LifeSafetyOperation. Reliability is served by the stack: this example's callback supplies the first stage (cl. 13.2.2.2, hardware - always no-fault-detected here) and the stack's FAULT_LIFE_SAFETY algorithm the second (MULTI_STATE_FAULT while Present_Value is fault). Mode is required-writable and accepted unconditionally (no customer-facing Accepted_Modes setter exists - see the file header); Accepted_Modes is therefore accepted at the stack's generic default. Event_State is NOT actually a stack default here - it is genuinely computed by the CHANGE_OF_LIFE_SAFETY and FAULT_LIFE_SAFETY algorithms this example arms; it is marked accepted only because property-profile-reference.md's generic table does not know an algorithm was armed (same convention as B-AAC's Diamond). Present_Value is also DS-COV-B subscribable

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Present_Value | BACnetLifeSafetyState | app | no |
| Tracking_Value | BACnetLifeSafetyState | app | no |
| Status_Flags | BACnetStatusFlags | stack | no |
| Event_State | BACnetEventState | stack default, accepted (Generic Enumerated default: `0`) | no |
| Reliability | BACnetReliability | stack | no |
| Out_Of_Service | Boolean | app | no |
| Mode | BACnetLifeSafetyMode | app | yes |
| Accepted_Modes | BACnetLIST of BACnetLifeSafetyMode | stack default, accepted (Generic Enumerated default: `0`) | no |
| Silenced | BACnetSilencedState | app | no |
| Operation_Expected | BACnetLifeSafetyOperation | app | no |

### Life Safety Zone 1 "Azure" - The smoke detector's zone. F-LIFESAFETY / F-ALARM-LS. Rolls up Life Safety Point 1 (Amber): takes its detector reading, latches alarm the same way, and a LifeSafetyOperation addressed to the zone also operates Amber. Event_State is genuinely computed (the same two algorithms are armed on Azure), marked accepted for the same generator-limitation reason. Reliability as Amber's. Zone_Members (cl. 12.16.26, required, BACnetLIST of BACnetDeviceObjectReference) should list Amber but reads as an empty list at 64b2cfac5: the stack serves it, but only the test-tool BACnetStackTestTool_SetLifeSafetyZoneMembers can fill it, and the series does not use the test-tool interface. [KNOWN GAP] in tests/life_safety_test.py; see TODO.md and stack #2036 (open)

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Present_Value | BACnetLifeSafetyState | app | no |
| Tracking_Value | BACnetLifeSafetyState | app | no |
| Status_Flags | BACnetStatusFlags | stack | no |
| Event_State | BACnetEventState | stack default, accepted (Generic Enumerated default: `0`) | no |
| Reliability | BACnetReliability | stack | no |
| Out_Of_Service | Boolean | app | no |
| Mode | BACnetLifeSafetyMode | app | yes |
| Accepted_Modes | BACnetLIST of BACnetLifeSafetyMode | stack default, accepted (Generic Enumerated default: `0`) | no |
| Silenced | BACnetSilencedState | app | no |
| Operation_Expected | BACnetLifeSafetyOperation | app | no |
| Zone_Members | BACnetLIST of BACnetDeviceObjectReference | stack default, accepted (None known - a read fails with `unknown-property` or an empt) | no |
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |

### Notification Class 1 "Crimson" - Priority, Ack_Required and Recipient_List are NOT stack DEFAULTS - they are genuinely populated, by BACnetStack_AddNotificationClassObject (Priority, Ack_Required) and BACnetStack_AddRecipientToNotificationClass (Recipient_List) at start-up. They are marked accepted only because property-profile-reference.md's generic per-type table does not know about this object-specific host-configuration API and so cannot credit them as stack-served. Routes Amber's and Azure's CHANGE_OF_LIFE_SAFETY / fault notifications, CONFIRMED, to one recipient named by DEVICE instance: 389107, the operator workstation (the stack resolves its address from its I-Am). Not writable: a B-LSC is not required to support modification of its event reporting (L.5.2)

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
each property. Up to stack `abd4cee1` the stack answered `unknown-object` before
reaching those callbacks; at `64b2cfac5` they are reached, and the discovery
Wire Test reads every listed property of both objects.

## 12. References

- ANSI/ASHRAE Standard 135-2024, Annex A (PICS template), Annex K (BIBBs),
  Annex L (device profiles), Clause 12 (object types), Clause 13 (alarm and
  event services).
- [README.md](../README.md) - what this example is and how to build it.
- [TUTORIAL.md](../TUTORIAL.md) - how to extend it, and how to keep this
  document honest when you do.
- [TODO.md](../TODO.md) - the engineering detail behind the callout above.
- [chipkin/cas-bacnet-stack#2036](https://github.com/chipkin/cas-bacnet-stack/issues/2036) -
  the open stack issue for customer Life Safety exports (Zone_Members).
