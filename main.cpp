// SPDX-License-Identifier: CC0-1.0
// Public-domain example code (CC0) - see LICENSE. The CAS BACnet Stack itself is
// a separate, commercially licensed product and is not covered by CC0.
// =============================================================================
// BACnet Profile Example - B-LSC (Life Safety Controller) - C++
//
// THE PROFILE FUNCTION (ANSI/ASHRAE 135-2024, Annex L.5.2): "A B-LSC device
// performs life safety alarm detection and control. It supports life safety
// objects." Its Alarm and Event Management requirements are "Generation of
// alarm / event notifications, including life safety alarms and the ability to
// direct them to recipients", "Execution of all forms of life safety
// operations", "Maintain a list of unacknowledged alarms / events" and
// "Notifying other recipients that the acknowledgment has been received".
//
// WHAT THIS EXAMPLE DOES WITH IT (README.md "The scenario"):
//
//   1. A smoke detector - SIMULATION, section 1b - sees smoke. Life Safety
//      Point 1 "Amber" (the detector) follows it: Tracking_Value goes to
//      alarm(2) at once, and Present_Value LATCHES alarm. Life Safety Zone 1
//      "Azure" (the detector's zone) rolls the point up the same way.
//   2. The stack's CHANGE_OF_LIFE_SAFETY algorithm puts both objects in the
//      LIFE_SAFETY_ALARM event state and sends a ConfirmedEventNotification to
//      the operator workstation, Device 389107, named in Notification Class 1
//      "Crimson"'s Recipient_List (the stack finds its address from its I-Am).
//   3. The operator silences the sounders (LifeSafetyOperation SILENCE),
//      acknowledges the alarm (AcknowledgeAlarm - every recipient then gets an
//      ACK_NOTIFICATION), and once the smoke has cleared resets the latch
//      (LifeSafetyOperation RESET) - the return to normal is notified too.
//
//   Failure Modes, each shown by a Wire Test (tests/):
//   - RESET while the detector still sees smoke is refused, OBJECT /
//     INVALID_OPERATION_IN_THIS_STATE (cl. 13.13.1.3.1 / 13.13.2), and the
//     latch holds. So is a RESET of a point that is not latched.
//   - A detector fault (its head pulled off the base) drives Reliability to
//     MULTI_STATE_FAULT and the object to the FAULT event state (the stack's
//     FAULT_LIFE_SAFETY algorithm, cl. 13.4), notified to-fault.
//   - An alarm nobody acknowledges stays in GetEventInformation's list.
//
// The BACnet Surface is the B-LSC profile's BIBBs (README.md has the table):
//
//     DS-RP-B, DS-WP-B    - ReadProperty, WriteProperty (Mode is writable),
//     DS-COV-B            - SubscribeCOV (Amber's and Bronze's Present_Value),
//     AE-LS-B             - CHANGE_OF_LIFE_SAFETY notifications + executing
//                           LifeSafetyOperation (silence / unsilence / reset),
//     AE-ACK-B            - AcknowledgeAlarm,
//     AE-INFO-B           - GetEventInformation,
//     DM-DDB-A,B, DM-DOB-B - Who-Is/I-Am (answer + initiate on start-up), Who-Has/I-Have,
//     DM-DCC-B            - DeviceCommunicationControl.
//   It also keeps, from the B-AAC base it was seeded from, ReadPropertyMultiple,
//   WritePropertyMultiple, Time/UTCTimeSynchronization and ReinitializeDevice
//   (README.md says which are beyond the profile's table).
//
// LifeSafetyOperation (service 37) is compiled in by this example's own
// CMakeLists.txt: STACK_OPTION_DM_LSO_LIFE_SAFETY_OPERATION is the stack's
// documented public toggle for it (CASBACnetStackOptions.h) and the default
// STACK_OPTION_TARGET_FULL preset leaves it off. A STATIC/DLL build must link a
// stack library built with that option - see README.md "Building".
//
// WHAT IS NOT IMPLEMENTED (README.md + TODO.md):
//   - Life Safety Zone 1 "Azure"'s Zone_Members (cl. 12.16.26) reads an EMPTY
//     list. It should name Amber - main.cpp rolls Amber up into Azure - but
//     the customer interface has no way to fill it (stack #2036, open). [KNOWN GAP]
//   - The stack's OWN Life Safety Point/Zone engine (Mode / Accepted_Modes /
//     latching / zone roll-up) is not exported to the customer interface
//     (stack #2036). Like every other object here, Amber and Azure are
//     BACnetStack_AddObject() plus this file's Get/Set callbacks holding their
//     state; the stack's intrinsic CHANGE_OF_LIFE_SAFETY and FAULT_LIFE_SAFETY
//     algorithms watch the Present_Value those callbacks report. Mode is
//     accepted unconditionally rather than checked against Accepted_Modes
//     (which has no customer setter either).
//
// Each object has a colour name (the convention shared across this example
// series):
//
//     Device 389007            "Chipkin Example B-LSC"     (instance configurable with --deviceID)
//     Analog Input  1          "Bronze"      (REAL, degrees Celsius; read-only)
//     Binary Input  1          "Emerald"     (active / inactive; read-only)
//     Multi-State Input 1      "Hot Pink"    (state 1..3; read-only)
//     Analog Output 1          "Chartreuse"  (REAL setpoint; WRITABLE, commandable)
//     Binary Output 1          "Fuchsia"     (active / inactive; WRITABLE, commandable)
//     Multi-State Output 1     "Indigo"      (state 1..3; WRITABLE, commandable)
//     Life Safety Point 1      "Amber"       (the smoke detector; CHANGE_OF_LIFE_SAFETY + FAULT_LIFE_SAFETY)
//     Life Safety Zone 1       "Azure"       (the detector's zone; the same two algorithms)
//     Notification Class 1     "Crimson"     (routes Amber's/Azure's alarms to Device 389107)
//     Network Port 1           "Vermilion"   (the BACnet/IP port - required)
//
// Output objects are COMMANDABLE: their Present_Value is driven by a 16-slot
// BACnet Priority_Array. A WriteProperty(Present_Value, value, priority) sets a
// slot; writing NULL relinquishes it; the stack reports the highest-priority
// non-null slot (or Relinquish_Default) as the effective Present_Value.
//
// Both life-safety algorithms only look at Present_Value once
// BACnetStack_UpdateValue is called for it (see that function's comment in
// CASBACnetStackDLL.h - "a host that never calls it will never generate an event
// notification"), so every change to Amber's or Azure's state below calls it.
// The same call feeds Amber's COV subscribers (DS-COV-B). Present_Value is
// read-only, as Table 12-18 says: only the detector and LifeSafetyOperation move it.
//
// F-REINIT (DM-RD-B): unchanged from B-AAC/B-ASC - see ReinitializeDevice() and the
// deferred-restart block in the main loop. B-LSC is this series' canonical copy
// source for F-REINIT; the comments below are written for that.
//
// To be a conformant BACnet device (Protocol_Revision 30) each object must
// expose its full set of REQUIRED properties. Most are generated by the stack
// (Object_Identifier, Object_Type, Status_Flags, Object_List, Protocol_*).
// Event_State is subtle here: for the objects with NO alarming it just reads its
// datatype default of normal(0) (correct by coincidence, not computed). For Amber
// and Azure the stack genuinely COMPUTES Event_State from the two algorithms.
// The handful of properties the application must supply are served by the
// Get*Property callbacks below, and a few are turned on with SetPropertyEnabled.
//
// Interactive keys (handled by the shared helper): h = help, q = quit, up/down =
// nudge Analog Input 1 by +/-1.1 (also feeds its COV subscribers). There is no
// detector key (docs/menu-keys.md claims none for life safety): the Wire Tests
// move the simulated detector through the loopback-only test control channel
// (simulation/SimulationControl.h), which is off unless
// BLSC_SIMULATION_CONTROL_PORT is set.
// Command line: --port <n>, --deviceID <n>.
//
// All the UDP/stack plumbing lives in common/CASExampleHelper so this file can
// stay focused on the BACnet logic.
// =============================================================================

#include "CASExampleHelper.h"
#include "CASBACnetStackExampleConstants.h"
#include "CASBACnetStackAdapter.h" // the CAS BACnet Stack C API (BACnetStack_*); call
                                    // LoadBACnetFunctions() before any BACnetStack_* call -
                                    // see the top of main() below.
#include "simulation/SimulationControl.h" // SIMULATION only: the Wire Tests' detector back door

#include <stdio.h>
#include <string.h>
#include <string>
#include <time.h>  // InitSyncedDateTimeFromHost() seeds Local_Date/Local_Time

#if defined(_WIN32)
#include <windows.h> // Sleep()
#else
#include <unistd.h>  // usleep()
#endif

using namespace CASBACnetStackExampleConstants;

// -----------------------------------------------------------------------------
// 1. Example + device configuration
// -----------------------------------------------------------------------------
static const char* APP_NAME = "BACnet B-LSC (Life Safety Controller) Example - C++";
static const char* APP_VERSION = "1.2.0";

// The device instance. BACnet requires this to be configurable, so it defaults
// to 389007 (docs/colour-table.md) and can be overridden on the command line
// with --deviceID.
static uint32_t g_deviceInstance = 389007;

// ---- Device identity: CHANGE ALL OF THIS BEFORE YOU SHIP --------------------
// Everything in this block is read by clients and shown to the operator in every
// discovery tool on the network. Left as-is, your product will appear on a real
// site announcing itself as a Chipkin demo. None of it is cosmetic:
// Object_Name must be unique across the BACnet internetwork, and Model_Name /
// Vendor_Identifier are what a building operator uses to identify your device.
// -----------------------------------------------------------------------------

// Your BACnet Vendor Identifier. 389 = Chipkin Automation Systems; change this
// to YOUR company's vendor ID before shipping a product. Vendor IDs are assigned
// by ASHRAE - request one (free) at https://bacnet.org/assigned-vendor-ids/.
// Update VENDOR_NAME below to match.
static const uint32_t VENDOR_IDENTIFIER = 389;

// Your device's Object_Name. MUST BE UNIQUE ACROSS THE BACNET INTERNETWORK -
// this is the one that will bite you. The device INSTANCE is runtime-
// configurable via --deviceID (see g_deviceInstance above), but DEVICE_NAME is
// a compile-time constant: ship two units and configure their instances
// correctly, and both still announce Object_Name "Chipkin Example B-LSC" - a spec violation
// on the wire, not just a documentation nit. In a real product, Object_Name
// must be made per-unit configurable too - a serial number, DIP switches, a
// config file, or a --deviceName command-line argument, the same way
// --deviceID makes the instance configurable here.
static const char* DEVICE_NAME = "Chipkin Example B-LSC";

// What your device actually is - replace this tutorial description with your
// product's own.
static const char* DEVICE_DESCRIPTION =
    "Chipkin CAS BACnet Stack example - B-LSC (Life Safety Controller) profile. "
    "A SIMULATED smoke detector (Life Safety Point 1) and its zone raise life-safety "
    "alarms to an operator workstation, which silences, acknowledges and resets them "
    "(AE-LS-B / AE-ACK-B / AE-INFO-B).";

// Device identity strings (read by clients, and used to populate I-Am).
// VENDOR_NAME must match VENDOR_IDENTIFIER above - your company name, not
// Chipkin's. MODEL_NAME is your model designation - what a building operator
// reads on a real job site to identify your device, not a tutorial label.
static const char* VENDOR_NAME = "Chipkin Automation Systems";
static const char* MODEL_NAME = "CAS BACnet Stack Example - B-LSC";

// DeviceCommunicationControl password. A management station may include a password
// with a DeviceCommunicationControl (or ReinitializeDevice) request; the device
// accepts the command only if it matches. Set to NULL/empty to accept any request
// (no password required). Change this to your device's secret before shipping.
static const char* DCC_PASSWORD = "";  // "" = no password required

// Application_Software_Version (12) is just APP_VERSION - one source of
// truth, so it can never drift from what --version/the startup banner
// prints (it did drift: this used to be a separate hardcoded "1.0.0"
// constant nobody updated across patch releases).
//
// Firmware_Revision (44) is meant to name the underlying platform/stack,
// not this example's own version - built at runtime from the CAS BACnet
// Stack's own BACnetStack_GetAPIMajorVersion()/etc. (the same 4 calls
// common/CASExampleHelper.cpp's PrintVersion() already uses for the
// startup banner's "CAS BACnet Stack version: X.Y.Z.W" line), so it can
// never go stale either - see g_firmwareRevision below, populated once
// right after LoadBACnetFunctions() succeeds (those functions are what
// the version getters themselves are, so they must be loaded first).
static std::string g_firmwareRevision;

// The sensor objects (all instance 1) and their colour names.
static const uint32_t ANALOG_INPUT_INSTANCE = 1;       // "Bronze"
static const uint32_t BINARY_INPUT_INSTANCE = 1;       // "Emerald"
static const uint32_t MULTI_STATE_INPUT_INSTANCE = 1;  // "Hot Pink"
static const uint32_t MULTI_STATE_INPUT_NUMBER_OF_STATES = 3;

// The Network Port object - every BACnet device must have one. It represents
// the BACnet/IP port this device communicates on.
static const uint32_t NETWORK_PORT_INSTANCE = 1;       // "Vermilion"
static const uint32_t MAX_APDU_LENGTH = 1476;          // BACnet/IP APDU length

// BACnet/IP addressing the Network Port reports. The IP address and subnet mask
// are filled in at start-up from the host's primary interface; the gateway is
// left unset (0.0.0.0) for this example. The stack also uses IP_Address +
// BACnet_IP_UDP_Port to build the port's MAC_Address automatically.
static uint8_t g_ipAddress[4] = { 0, 0, 0, 0 };
static uint8_t g_ipSubnetMask[4] = { 0, 0, 0, 0 };
static uint8_t g_ipDefaultGateway[4] = { 0, 0, 0, 0 };
static uint16_t g_bacnetIpUdpPort = 47808;

// Analog Input 1's live present value (degrees Celsius). Starts at 21.5 and is
// nudged by the up/down arrow keys. A real sensor would update this from
// hardware instead.
static float g_analogInput1Value = 21.5f;

// The commandable OUTPUT objects (all instance 1) and their colour names. These
// are what make this a B-SA actuator: clients drive them with WriteProperty.
static const uint32_t ANALOG_OUTPUT_INSTANCE = 1;        // "Chartreuse"
static const uint32_t BINARY_OUTPUT_INSTANCE = 1;        // "Fuchsia"
static const uint32_t MULTI_STATE_OUTPUT_INSTANCE = 1;   // "Indigo"
static const uint32_t MULTI_STATE_OUTPUT_NUMBER_OF_STATES = 3;
static const uint32_t BACNET_PRIORITY_ARRAY_SIZE = 16;

// A BACnet commandable value: a 16-slot Priority_Array plus a Relinquish_Default.
// Each slot is either null (relinquished) or holds a commanded value. A real
// device would map the resolved Present_Value onto its physical output; here we
// just store the commands. The values are kept as double and cast per object
// type (REAL for AO, 0/1 for BO, state number for MSO).
struct Commandable {
    bool isSet[16];          // is slot i (1..16) commanded?
    double value[16];        // the commanded value at slot i
    double relinquishDefault; // used when every slot is null
};

// The { { false }, { 0 }, default } initializer zero-fills all 16 slots of isSet
// and value (C++ aggregate rules: the remaining elements are value-initialized),
// so every priority slot starts null and Present_Value reports relinquishDefault.
static Commandable g_analogOutput = { { false }, { 0 }, 20.0 }; // setpoint, default 20.0 C
static Commandable g_binaryOutput = { { false }, { 0 }, 0.0 };  // default inactive (0)
static Commandable g_multiStateOutput = { { false }, { 0 }, 1.0 }; // default state 1

// --- Life Safety Point/Zone + their Notification Class (the B-LSC additions) ---
// Life Safety Point 1 "Amber" is the smoke detector; Life Safety Zone 1 "Azure"
// is the zone it sits in. Present_Value and Tracking_Value are
// BACnetLifeSafetyState values (BACnetLifeSafetyState.h at the pin): this example
// uses quiet(0), alarm(2) and fault(3).
static const uint32_t LIFE_SAFETY_POINT_INSTANCE = 1;   // "Amber"
static const uint32_t LIFE_SAFETY_ZONE_INSTANCE = 1;    // "Azure"
static const uint32_t LIFE_SAFETY_STATE_QUIET = 0;
static const uint32_t LIFE_SAFETY_STATE_ALARM = 2;
static const uint32_t LIFE_SAFETY_STATE_FAULT = 3;
static const uint32_t LIFE_SAFETY_MODE_ON = 1;           // BACnetLifeSafetyMode default we report

// The state of one life-safety object, held by this example (see the file header
// for why not by the stack). The rules that move it - the "logic internal to the
// device" that cl. 12.15 / 12.16 leave a local matter - are in
// ApplyDetectorReading() and LifeSafetyOperation() below:
//
//   trackingValue  follows the detector continuously (cl. 12.15.5).
//   presentValue   LATCHES alarm: smoke sets it to alarm, and it stays alarm after
//                  the smoke clears until an operator RESETs it (cl. 12.15.4 "may
//                  latch non-NORMAL state values until reset"). A fault is not
//                  latched: it follows the detector, and clears when the detector
//                  is repaired - an alarm already latched wins over it.
//   silenced*      the sounders (audible) and strobes (visual); a new alarm
//                  unsilences both, LifeSafetyOperation SILENCE* sets them.
struct LifeSafetyObjectState {
    uint32_t trackingValue;
    uint32_t presentValue;
    uint32_t mode;
    bool silencedAudible;
    bool silencedVisual;
};
static const LifeSafetyObjectState LIFE_SAFETY_POWER_ON_STATE = {
    LIFE_SAFETY_STATE_QUIET, LIFE_SAFETY_STATE_QUIET, LIFE_SAFETY_MODE_ON, false, false };
static LifeSafetyObjectState g_lifeSafetyPoint = LIFE_SAFETY_POWER_ON_STATE;  // Amber
static LifeSafetyObjectState g_lifeSafetyZone = LIFE_SAFETY_POWER_ON_STATE;   // Azure

// -----------------------------------------------------------------------------
// 1b. SIMULATION: the smoke detector
//
// Everything in this section stands in for hardware. On a real panel the
// detector loop card reports what each detector sees - quiet, smoke, or a fault
// on its wiring - and that reading is what Amber's Tracking_Value follows. Here
// the reading is a variable, and the only thing that moves it is the Wire Tests'
// loopback-only control channel (simulation/SimulationControl.h; off unless
// BLSC_SIMULATION_CONTROL_PORT is set):
//
//     detector quiet     -> ok        the smoke has cleared / the head is back
//     detector smoke     -> ok        the detector sees smoke
//     detector fault     -> ok        the head is pulled off its base
//     detector           -> detector <quiet|smoke|fault>
//
// Nothing on the BACnet network can move it: Present_Value and Tracking_Value
// are read-only (Table 12-18), so a client cannot fake an alarm.
// -----------------------------------------------------------------------------
static uint32_t g_detectorReading = LIFE_SAFETY_STATE_QUIET;   // SIMULATION: what the detector sees

// The values that drive each intrinsic algorithm (passed to
// BACnetStack_SetIntrinsicChangeOfLifeSafetyAlgorithm / SetFaultLifeSafetyAlgorithm
// at start-up).
static const uint32_t LIFE_SAFETY_ALARM_VALUES[] = { LIFE_SAFETY_STATE_ALARM };
static const uint32_t LIFE_SAFETY_FAULT_VALUES[] = { LIFE_SAFETY_STATE_FAULT };
static const uint32_t LIFE_SAFETY_TIME_DELAY = 0;   // seconds the alarm/fault value must hold

static const uint32_t NOTIFICATION_CLASS_INSTANCE = 1;     // "Crimson"
// Notification priorities for the three transitions (lower = more urgent).
static const uint8_t NC_PRIORITY_TO_OFFNORMAL = 100;
static const uint8_t NC_PRIORITY_TO_FAULT = 50;
static const uint8_t NC_PRIORITY_TO_NORMAL = 200;

// Where Amber's/Azure's alarms are sent: the operator workstation (a B-OWS on a
// real site; a Python Peer in the Wire Tests). A recipient can be named two ways:
// by ADDRESS, or by DEVICE instance - and the device form is what a fire panel
// wants, because the workstation's address can change and its instance does not.
// The stack resolves the instance to an address itself, from the workstation's
// I-Am (it sends a Who-Is for it when it has none). The alarms are CONFIRMED, so
// the stack retries a notification the workstation does not answer (see
// NUMBER_OF_APDU_RETRIES) and the workstation knows it has missed nothing.
// 389107 is this example's choice for its Peer (B-LSC's 389007 + 100); on a real
// site it is the head-end's device instance. A B-LSC is "not required to support
// modification of its alarm and event reporting by another device" (L.5.2), so
// this list is configured here, not over the network.
static const uint32_t RECIPIENT_DEVICE_INSTANCE = 389107;
static const uint32_t RECIPIENT_PROCESS_IDENTIFIER = 1;

// COV settings (DS-COV-B). Bronze (Analog Input 1) and Amber (Life Safety Point 1)
// Present_Value are subscribable - see BACnetStack_SetPropertySubscribable in main().
static const uint32_t COV_MAX_ACTIVE_SUBSCRIPTIONS = 32;
static const uint32_t COV_MAX_SUPPORTED_LIFETIME_SECONDS = 3600;

// BACnet object type / property identifier / service numbers not already in
// CASBACnetStackExampleConstants.h (verified against BACnetObjectType.h /
// BACnetPropertyIdentifier.h / BACnetServicesSupported.h / BACnetReliability.h /
// BACnetErrorCode.h at the pin).
static const uint16_t OBJECT_TYPE_LIFE_SAFETY_POINT = 21;  // "Amber"
static const uint16_t OBJECT_TYPE_LIFE_SAFETY_ZONE = 22;   // "Azure"
static const uint32_t PROPERTY_IDENTIFIER_NUMBER_OF_APDU_RETRIES = 73;
static const uint32_t PROPERTY_IDENTIFIER_RELIABILITY = 103;
static const uint32_t PROPERTY_IDENTIFIER_MODE = 160;
static const uint32_t PROPERTY_IDENTIFIER_OPERATION_EXPECTED = 161;
static const uint32_t PROPERTY_IDENTIFIER_SILENCED = 163;
static const uint32_t PROPERTY_IDENTIFIER_TRACKING_VALUE = 164;
static const uint32_t RELIABILITY_NO_FAULT_DETECTED = 0;
static const uint32_t SERVICE_SUBSCRIBE_COV = 5;
static const uint32_t SERVICE_LIFE_SAFETY_OPERATION = 37;
static const uint32_t ERROR_CODE_INVALID_OPERATION_IN_THIS_STATE = 139;
static const uint32_t ERROR_CODE_SERVICE_REQUEST_DENIED = 29;

// Number_Of_APDU_Retries (Device property 73): how many times this device sends a
// confirmed request again when no answer comes back within APDU_Timeout - which is
// what happens to a ConfirmedEventNotification whose recipient missed it. The stack
// reads it through GetPropertyUnsignedInteger below; served by nobody it reads 0,
// which 135-2024 12.11.29 says means "this device does not perform retries". 3 is
// the clause's suggested default. (APDU_Timeout is left to the stack's default.)
static const uint32_t NUMBER_OF_APDU_RETRIES = 3;

// BACnetSilencedState (BACnetSilencedState.h): unsilenced(0), audible-silenced(1),
// visible-silenced(2), all-silenced(3).
static uint32_t SilencedStateOf(const LifeSafetyObjectState& state) {
    return (state.silencedAudible && state.silencedVisual) ? 3 :
           state.silencedAudible ? 1 : state.silencedVisual ? 2 : 0;
}
// BACnetLifeSafetyOperation (BACnetLifeSafetyOperation.h). none(0) never arrives as
// a request; Operation_Expected uses it for "nothing to do".
static const uint8_t LIFE_SAFETY_OP_NONE = 0;
static const uint8_t LIFE_SAFETY_OP_SILENCE = 1;
static const uint8_t LIFE_SAFETY_OP_SILENCE_AUDIBLE = 2;
static const uint8_t LIFE_SAFETY_OP_SILENCE_VISUAL = 3;
static const uint8_t LIFE_SAFETY_OP_RESET = 4;
static const uint8_t LIFE_SAFETY_OP_RESET_ALARM = 5;
static const uint8_t LIFE_SAFETY_OP_RESET_FAULT = 6;
static const uint8_t LIFE_SAFETY_OP_UNSILENCE = 7;
static const uint8_t LIFE_SAFETY_OP_UNSILENCE_AUDIBLE = 8;
static const uint8_t LIFE_SAFETY_OP_UNSILENCE_VISUAL = 9;

// Operation_Expected (cl. 12.15.24, "the next operation expected by this object to
// handle a specific life safety situation"): while the alarm sounds, SILENCE; once
// silenced, RESET - which is refused until the smoke has cleared (see
// LifeSafetyOperation()). Nothing otherwise.
static uint32_t OperationExpectedOf(const LifeSafetyObjectState& state) {
    if (state.presentValue != LIFE_SAFETY_STATE_ALARM) {
        return LIFE_SAFETY_OP_NONE;
    }
    return (state.silencedAudible && state.silencedVisual) ? LIFE_SAFETY_OP_RESET : LIFE_SAFETY_OP_SILENCE;
}

// The life-safety object an (objectType, objectInstance) names, or NULL.
static LifeSafetyObjectState* GetLifeSafetyObject(uint16_t objectType, uint32_t objectInstance) {
    if (objectType == OBJECT_TYPE_LIFE_SAFETY_POINT && objectInstance == LIFE_SAFETY_POINT_INSTANCE) {
        return &g_lifeSafetyPoint;
    }
    if (objectType == OBJECT_TYPE_LIFE_SAFETY_ZONE && objectInstance == LIFE_SAFETY_ZONE_INSTANCE) {
        return &g_lifeSafetyZone;
    }
    return NULL;
}

static const char* LifeSafetyStateName(uint32_t state) {
    return state == LIFE_SAFETY_STATE_ALARM ? "alarm" : state == LIFE_SAFETY_STATE_FAULT ? "fault" : "quiet";
}

// Tell the stack Amber's and Azure's Present_Value may have changed: this is what
// runs the two intrinsic algorithms (and Amber's COV subscriptions).
static void UpdateLifeSafetyObjects() {
    BACnetStack_UpdateValue(g_deviceInstance, OBJECT_TYPE_LIFE_SAFETY_POINT, LIFE_SAFETY_POINT_INSTANCE,
                            PROPERTY_IDENTIFIER_PRESENT_VALUE);
    BACnetStack_UpdateValue(g_deviceInstance, OBJECT_TYPE_LIFE_SAFETY_ZONE, LIFE_SAFETY_ZONE_INSTANCE,
                            PROPERTY_IDENTIFIER_PRESENT_VALUE);
}

// One object follows a new detector reading (the rules in LifeSafetyObjectState's
// comment). Returns true if Present_Value changed.
static bool FollowReading(LifeSafetyObjectState* state, uint32_t reading) {
    const uint32_t before = state->presentValue;
    state->trackingValue = reading;
    if (reading == LIFE_SAFETY_STATE_ALARM) {
        if (state->presentValue != LIFE_SAFETY_STATE_ALARM) {
            state->silencedAudible = false;   // a new alarm sounds again
            state->silencedVisual = false;
        }
        state->presentValue = LIFE_SAFETY_STATE_ALARM;
    } else if (state->presentValue != LIFE_SAFETY_STATE_ALARM) {
        state->presentValue = reading;        // fault and quiet are not latched
    }
    return state->presentValue != before;
}

// SIMULATION -> BACnet: the detector's reading reaches Amber and, rolled up, Azure
// (the zone takes its only member's reading - the roll-up rule is a local matter,
// cl. 12.16).
static void ApplyDetectorReading(uint32_t reading) {
    g_detectorReading = reading;
    FollowReading(&g_lifeSafetyPoint, reading);
    FollowReading(&g_lifeSafetyZone, reading);
    UpdateLifeSafetyObjects();
    printf("SIMULATION: smoke detector reads %s -> Amber Tracking_Value %s, Present_Value %s\n",
           reading == LIFE_SAFETY_STATE_ALARM ? "SMOKE" : reading == LIFE_SAFETY_STATE_FAULT ? "FAULT" : "quiet",
           LifeSafetyStateName(g_lifeSafetyPoint.trackingValue),
           LifeSafetyStateName(g_lifeSafetyPoint.presentValue));
}

// The test control channel's commands (see the table at the top of this section).
static void HandleSimulationCommand(const char* command, char* reply, size_t replyLength) {
    if (strncmp(command, "detector", 8) != 0) {
        snprintf(reply, replyLength, "error unknown command");
        return;
    }
    const char* argument = command + 8;
    while (*argument == ' ') {
        ++argument;
    }
    if (*argument == '\0') {
        snprintf(reply, replyLength, "detector %s",
                 g_detectorReading == LIFE_SAFETY_STATE_ALARM ? "smoke" :
                 g_detectorReading == LIFE_SAFETY_STATE_FAULT ? "fault" : "quiet");
        return;
    }
    if (strcmp(argument, "quiet") == 0) {
        ApplyDetectorReading(LIFE_SAFETY_STATE_QUIET);
    } else if (strcmp(argument, "smoke") == 0) {
        ApplyDetectorReading(LIFE_SAFETY_STATE_ALARM);
    } else if (strcmp(argument, "fault") == 0) {
        ApplyDetectorReading(LIFE_SAFETY_STATE_FAULT);
    } else {
        snprintf(reply, replyLength, "error unknown detector reading");
        return;
    }
    snprintf(reply, replyLength, "ok");
}

// A WriteProperty to a commandable Present_Value carries a priority 1..16. When a
// client omits it, BACnet uses 16 (the lowest priority) - so normalise anything
// out of range to 16, matching the stack's own behaviour.
static uint8_t EffectivePriority(uint8_t priority) {
    return (priority >= 1 && priority <= BACNET_PRIORITY_ARRAY_SIZE) ? priority : 16;
}

// Store a commanded value at a priority slot (a WriteProperty of a value).
static void CommandWrite(Commandable* c, uint8_t priority, double value) {
    const uint8_t p = EffectivePriority(priority);
    c->isSet[p - 1] = true;
    c->value[p - 1] = value;
}

// Relinquish (clear) a priority slot - i.e. a WriteProperty of NULL.
static void CommandRelinquish(Commandable* c, uint8_t priority) {
    const uint8_t p = EffectivePriority(priority);
    c->isSet[p - 1] = false;
}

// Resolve which Commandable an (objectType, objectInstance) maps to, or NULL.
static Commandable* GetCommandable(uint16_t objectType, uint32_t objectInstance) {
    if (objectType == OBJECT_TYPE_ANALOG_OUTPUT && objectInstance == ANALOG_OUTPUT_INSTANCE) {
        return &g_analogOutput;
    }
    if (objectType == OBJECT_TYPE_BINARY_OUTPUT && objectInstance == BINARY_OUTPUT_INSTANCE) {
        return &g_binaryOutput;
    }
    if (objectType == OBJECT_TYPE_MULTI_STATE_OUTPUT && objectInstance == MULTI_STATE_OUTPUT_INSTANCE) {
        return &g_multiStateOutput;
    }
    return NULL;
}

// Is this read a single Priority_Array element (Priority_Array[1..16])? If so,
// report whether that slot is commanded (*slotIsSet) and its value (*slotValue).
// The typed Get callbacks use this to serve a commandable object's Priority_Array
// and to let the stack compute Present_Value from the highest non-null slot.
static bool ReadPrioritySlot(const Commandable* c, uint32_t propertyIdentifier,
                             bool useArrayIndex, uint32_t propertyArrayIndex,
                             bool* slotIsSet, double* slotValue) {
    if (propertyIdentifier != PROPERTY_IDENTIFIER_PRIORITY_ARRAY || !useArrayIndex ||
        propertyArrayIndex < 1 || propertyArrayIndex > BACNET_PRIORITY_ARRAY_SIZE) {
        return false;
    }
    *slotIsSet = c->isSet[propertyArrayIndex - 1];
    *slotValue = c->value[propertyArrayIndex - 1];
    return true;
}

// -----------------------------------------------------------------------------
// 2. Property "get" callbacks
//
// The stack calls these when a client reads a property. For each data type the
// stack uses a separate callback. We return true (and fill *value) when we
// recognise the (object, property) pair, and false otherwise.
//
// THE errorCode OUT-PARAMETER. Every Get callback ends with uint32_t* errorCode.
// The stack PRESETS it to success (84) before the call, and reads it only if you
// return false. That gives a declining callback two distinct meanings:
//
//   1. return false and LEAVE errorCode ALONE  -> "I have no opinion on this
//      property." The stack falls back to its own handling (see below).
//   2. return false and SET *errorCode         -> "This read fails, with THIS
//      BACnet error." The client gets exactly that Error-PDU.
//
// Option 2 is new (CAS BACnet Stack issue #974); before it, a Get callback had
// no way to name an error at all. Do not reach for it reflexively - option 1 is
// still the right answer most of the time, for the reason in the next paragraph.
//
// WHAT false-WITHOUT-AN-ERROR-CODE ACTUALLY DOES - the most important paragraph
// in this file, and the opposite of what most people assume. It does NOT
// reliably produce a BACnet error. The stack errors only for the handful of
// properties it refuses to invent: Present_Value, Number_Of_States,
// Relinquish_Default, Local_Date, Local_Time, and a Network Port's APDU_Length
// (declining one of those now reads back as Error: read-access-denied, where
// older stack versions said value-not-initialized).
// For EVERYTHING ELSE, a false return means the stack SILENTLY SUBSTITUTES a
// default:
//     Object_Name -> the literal string "undefined"
//     Units       -> no-units (95)
//     otherwise   -> a datatype zero-value
//
// AND THAT FALLBACK IS LOAD-BEARING, WHICH IS WHY IT IS NOT "FIXED" HERE. It is
// tempting to end every callback with *errorCode = unknown-property so nothing is
// ever silently invented. That breaks the device. The stack relies on the
// decline-and-fabricate path to answer required properties the application is
// not expected to serve - the Device's Max_APDU_Length_Accepted, APDU_Timeout
// and Number_Of_APDU_Retries among them. Name an error on the catch-all return
// and those required properties start failing instead of answering.
// So: set *errorCode ONLY where THIS device knows the read is wrong. There is
// exactly one such case below (State_Text with an out-of-range array index); the
// catch-all `return false` at the end of each callback deliberately leaves
// errorCode alone.
//
// ADDING AN OBJECT? READ THIS FIRST.
// The consequence is the opposite of reassuring. These callbacks are not
// uniformly strict:
//   - GetPropertyReal / GetPropertyEnumerated / GetPropertyUnsignedInteger match
//     on object type AND INSTANCE (directly, or via GetCommandable(), which
//     looks up the exact type+instance pair). A new instance falls through every
//     one of those checks.
//   - GetPropertyBool serves Out_Of_Service on object TYPE ONLY, so a new
//     instance of an existing type gets Out_Of_Service for free.
// So a half-added object does NOT fail loudly. Its Present_Value errors (that
// one is in the list above) - but its Object_Name reads back as "undefined" and
// its Units as no-units, with no error at all. Add two objects that way and BOTH
// report Object_Name "undefined": duplicate object names within one device, which
// is a spec violation and a hard BTL failure, and which every scan tool will show
// you as a healthy object. The device looks fine and is non-conformant.
//
// So: when you add an instance, walk EVERY callback below, then read back every
// required property of the new object and DIFF IT against the existing one. Do
// not trust "it scanned OK" - that is exactly the failure mode.
// -----------------------------------------------------------------------------

// REAL (floating point) - the Analog Input's Present_Value.
bool GetPropertyReal(const uint32_t deviceInstance, const uint16_t objectType,
                     const uint32_t objectInstance, const uint32_t propertyIdentifier,
                     float* value, const bool useArrayIndex,
                     const uint32_t propertyArrayIndex, uint32_t* errorCode) {
    (void)errorCode; // see "THE errorCode OUT-PARAMETER" below: every catch-all here declines without naming an error
    if (deviceInstance != g_deviceInstance) {
        return false;
    }
    if (objectType == OBJECT_TYPE_ANALOG_INPUT &&
        objectInstance == ANALOG_INPUT_INSTANCE &&
        propertyIdentifier == PROPERTY_IDENTIFIER_PRESENT_VALUE) {
        // ON REAL HARDWARE: return the live sensor reading here. Read it from a
        // cached variable that your hardware updates (as g_analogInput1Value is),
        // NOT directly from a slow/blocking device (I2C, SPI, ADC conversion):
        // this callback runs on the BACnetStack_Tick() thread, so blocking it
        // delays all BACnet processing. Sample the sensor on a timer/another
        // thread and just hand back the latest value from here.
        *value = g_analogInput1Value;
        return true;
    }
    // Analog Output (commandable): serve its Priority_Array slots and
    // Relinquish_Default. The stack reads each slot to compute Present_Value and
    // to answer a ReadProperty of the whole array. For a null (relinquished) slot
    // we return false - the stack then takes the "slot is null" answer from
    // GetPropertyBool below.
    const Commandable* c = GetCommandable(objectType, objectInstance);
    if (c != NULL && objectType == OBJECT_TYPE_ANALOG_OUTPUT) {
        bool slotIsSet = false;
        double slotValue = 0.0;
        if (ReadPrioritySlot(c, propertyIdentifier, useArrayIndex, propertyArrayIndex,
                             &slotIsSet, &slotValue)) {
            if (!slotIsSet) {
                return false;
            }
            *value = (float)slotValue;
            return true;
        }
        if (propertyIdentifier == PROPERTY_IDENTIFIER_RELINQUISH_DEFAULT) {
            *value = (float)c->relinquishDefault;
            return true;
        }
    }
    return false;
}

// ENUMERATED - the Binary Input's Present_Value (0 = inactive, 1 = active) and
// the Analog Input's Units (degrees Celsius).
bool GetPropertyEnumerated(const uint32_t deviceInstance, const uint16_t objectType,
                           const uint32_t objectInstance, const uint32_t propertyIdentifier,
                           uint32_t* value, const bool useArrayIndex,
                           const uint32_t propertyArrayIndex, uint32_t* errorCode) {
    (void)errorCode;
    (void)useArrayIndex;
    (void)propertyArrayIndex;
    if (deviceInstance != g_deviceInstance) {
        return false;
    }
    // Life Safety Point 1 (Amber) / Life Safety Zone 1 (Azure): Present_Value (the
    // latched state), Tracking_Value (the detector's live reading), Reliability,
    // Mode, Silenced and Operation_Expected - all from this example's own state,
    // moved by the detector (section 1b) and LifeSafetyOperation(). See
    // LifeSafetyObjectState for the rules.
    const LifeSafetyObjectState* lifeSafetyObject = GetLifeSafetyObject(objectType, objectInstance);
    if (lifeSafetyObject != NULL) {
        switch (propertyIdentifier) {
            case PROPERTY_IDENTIFIER_PRESENT_VALUE:   *value = lifeSafetyObject->presentValue; return true;
            case PROPERTY_IDENTIFIER_TRACKING_VALUE:  *value = lifeSafetyObject->trackingValue; return true;
            // Reliability: what this callback serves is cl. 13.2.2.2's FIRST stage - the
            // hardware's own health (a broken loop card, say), which takes precedence.
            // This example's detector hardware never fails that way, so: no fault. The
            // SECOND stage is the stack's: its FAULT_LIFE_SAFETY algorithm (armed in
            // main()) reports MULTI_STATE_FAULT while Present_Value is fault, and the
            // stack serves that on a read (stack #2359). Do not compute it here.
            case PROPERTY_IDENTIFIER_RELIABILITY:     *value = RELIABILITY_NO_FAULT_DETECTED; return true;
            case PROPERTY_IDENTIFIER_MODE:            *value = lifeSafetyObject->mode; return true;
            case PROPERTY_IDENTIFIER_SILENCED:        *value = SilencedStateOf(*lifeSafetyObject); return true;
            case PROPERTY_IDENTIFIER_OPERATION_EXPECTED: *value = OperationExpectedOf(*lifeSafetyObject); return true;
            default: break;
        }
    }
    if (objectType == OBJECT_TYPE_BINARY_INPUT &&
        objectInstance == BINARY_INPUT_INSTANCE) {
        if (propertyIdentifier == PROPERTY_IDENTIFIER_PRESENT_VALUE) {
            *value = 1; // active
            return true;
        }
        if (propertyIdentifier == PROPERTY_IDENTIFIER_POLARITY) {
            *value = POLARITY_NORMAL; // required property of a Binary Input
            return true;
        }
    }
    // Binary Output (commandable): Present_Value is an enumerated active/inactive
    // driven through the Priority_Array. Serve the array slots and Relinquish_Default
    // (plus its required Polarity).
    const Commandable* c = GetCommandable(objectType, objectInstance);
    if (c != NULL && objectType == OBJECT_TYPE_BINARY_OUTPUT) {
        bool slotIsSet = false;
        double slotValue = 0.0;
        if (ReadPrioritySlot(c, propertyIdentifier, useArrayIndex, propertyArrayIndex,
                             &slotIsSet, &slotValue)) {
            if (!slotIsSet) {
                return false;
            }
            *value = (uint32_t)slotValue;
            return true;
        }
        if (propertyIdentifier == PROPERTY_IDENTIFIER_RELINQUISH_DEFAULT) {
            *value = (uint32_t)c->relinquishDefault;
            return true;
        }
        if (propertyIdentifier == PROPERTY_IDENTIFIER_POLARITY) {
            *value = POLARITY_NORMAL; // required property of a Binary Output
            return true;
        }
    }
    // Units is REQUIRED on an Analog Input AND on an Analog Output. Serve BOTH.
    // If you only serve the input's, the output does not error - it silently
    // reports no-units(95), because Units is not in the stack's
    // valueShouldBeInitialized list and so falls through to a substituted default
    // (see the note at the top of this section). A setpoint that reads back "no
    // units" next to a degC sensor is the kind of thing nobody notices until
    // commissioning.
    if (propertyIdentifier == PROPERTY_IDENTIFIER_UNITS &&
        ((objectType == OBJECT_TYPE_ANALOG_INPUT && objectInstance == ANALOG_INPUT_INSTANCE) ||
         (objectType == OBJECT_TYPE_ANALOG_OUTPUT && objectInstance == ANALOG_OUTPUT_INSTANCE))) {
        *value = ENGINEERING_UNITS_DEGREES_CELSIUS;
        return true;
    }
    if (objectType == OBJECT_TYPE_NETWORK_PORT &&
        objectInstance == NETWORK_PORT_INSTANCE &&
        propertyIdentifier == PROPERTY_IDENTIFIER_BACNET_IP_MODE) {
        *value = BACNET_IP_MODE_NORMAL; // not foreign-device, not BBMD
        return true;
    }
    return false;
}

// UNSIGNED INTEGER - the Multi-State Input's Present_Value, and the Device's
// Vendor_Identifier (the stack also uses Vendor_Identifier to build I-Am).
bool GetPropertyUnsignedInteger(const uint32_t deviceInstance, const uint16_t objectType,
                                const uint32_t objectInstance, const uint32_t propertyIdentifier,
                                uint32_t* value, const bool useArrayIndex,
                                const uint32_t propertyArrayIndex, uint32_t* errorCode) {
    (void)errorCode;
    if (deviceInstance != g_deviceInstance) {
        return false;
    }
    if (objectType == OBJECT_TYPE_MULTI_STATE_INPUT &&
        objectInstance == MULTI_STATE_INPUT_INSTANCE) {
        if (propertyIdentifier == PROPERTY_IDENTIFIER_PRESENT_VALUE) {
            *value = 1; // state 1 (valid range is 1..Number_Of_States)
            return true;
        }
        if (propertyIdentifier == PROPERTY_IDENTIFIER_NUMBER_OF_STATES) {
            *value = MULTI_STATE_INPUT_NUMBER_OF_STATES; // required property
            return true;
        }
        // State_Text is an array. The stack asks for its LENGTH here (array
        // index 0) before reading each element via GetPropertyCharString.
        if (propertyIdentifier == PROPERTY_IDENTIFIER_STATE_TEXT &&
            useArrayIndex && propertyArrayIndex == 0) {
            *value = MULTI_STATE_INPUT_NUMBER_OF_STATES;
            return true;
        }
    }
    if (objectType == OBJECT_TYPE_DEVICE && objectInstance == g_deviceInstance &&
        propertyIdentifier == PROPERTY_IDENTIFIER_VENDOR_IDENTIFIER) {
        *value = VENDOR_IDENTIFIER;
        return true;
    }
    if (objectType == OBJECT_TYPE_DEVICE && objectInstance == g_deviceInstance &&
        propertyIdentifier == PROPERTY_IDENTIFIER_NUMBER_OF_APDU_RETRIES) {
        *value = NUMBER_OF_APDU_RETRIES; // the stack retries confirmed requests (alarms) this many times
        return true;
    }
    if (objectType == OBJECT_TYPE_NETWORK_PORT && objectInstance == NETWORK_PORT_INSTANCE) {
        if (propertyIdentifier == PROPERTY_IDENTIFIER_APDU_LENGTH) {
            *value = MAX_APDU_LENGTH;
            return true;
        }
        if (propertyIdentifier == PROPERTY_IDENTIFIER_REFERENCE_PORT) {
            *value = NETWORK_PORT_REFERENCE_PORT_NONE;
            return true;
        }
        if (propertyIdentifier == PROPERTY_IDENTIFIER_BACNET_IP_UDP_PORT) {
            *value = g_bacnetIpUdpPort;
            return true;
        }
    }
    // Multi-State Output (commandable): Present_Value is an unsigned state number
    // driven through the Priority_Array. Serve the array slots, Relinquish_Default,
    // and the required Number_Of_States.
    const Commandable* c = GetCommandable(objectType, objectInstance);
    if (c != NULL && objectType == OBJECT_TYPE_MULTI_STATE_OUTPUT) {
        bool slotIsSet = false;
        double slotValue = 0.0;
        if (ReadPrioritySlot(c, propertyIdentifier, useArrayIndex, propertyArrayIndex,
                             &slotIsSet, &slotValue)) {
            if (!slotIsSet) {
                return false;
            }
            *value = (uint32_t)slotValue;
            return true;
        }
        if (propertyIdentifier == PROPERTY_IDENTIFIER_RELINQUISH_DEFAULT) {
            *value = (uint32_t)c->relinquishDefault;
            return true;
        }
        if (propertyIdentifier == PROPERTY_IDENTIFIER_NUMBER_OF_STATES) {
            *value = MULTI_STATE_OUTPUT_NUMBER_OF_STATES;
            return true;
        }
    }
    return false;
}

// BOOLEAN - Out_Of_Service is a required property of every input object and of
// the Network Port. This is a read-only sensor, so nothing is ever out of
// service: always false.
bool GetPropertyBool(const uint32_t deviceInstance, const uint16_t objectType,
                     const uint32_t objectInstance, const uint32_t propertyIdentifier,
                     bool* value, const bool useArrayIndex,
                     const uint32_t propertyArrayIndex, uint32_t* errorCode) {
    (void)errorCode;
    if (deviceInstance != g_deviceInstance) {
        return false;
    }
    // Commandable outputs: the stack asks "is this Priority_Array slot null?" with
    // the boolean getter. Answer true (1) for a relinquished slot, false (0) for a
    // commanded one. This is how the stack knows which slots to skip when computing
    // Present_Value and how it encodes the NULLs in a ReadProperty of the array.
    const Commandable* c = GetCommandable(objectType, objectInstance);
    if (c != NULL && propertyIdentifier == PROPERTY_IDENTIFIER_PRIORITY_ARRAY &&
        useArrayIndex && propertyArrayIndex >= 1 &&
        propertyArrayIndex <= BACNET_PRIORITY_ARRAY_SIZE) {
        *value = !c->isSet[propertyArrayIndex - 1];
        return true;
    }
    // Out_Of_Service is a required property of every input and output object and of
    // the Network Port. This example never takes anything out of service: false.
    if (propertyIdentifier == PROPERTY_IDENTIFIER_OUT_OF_SERVICE &&
        (objectType == OBJECT_TYPE_ANALOG_INPUT ||
         objectType == OBJECT_TYPE_BINARY_INPUT ||
         objectType == OBJECT_TYPE_MULTI_STATE_INPUT ||
         objectType == OBJECT_TYPE_ANALOG_OUTPUT ||
         objectType == OBJECT_TYPE_BINARY_OUTPUT ||
         objectType == OBJECT_TYPE_MULTI_STATE_OUTPUT ||
         objectType == OBJECT_TYPE_LIFE_SAFETY_POINT ||
         objectType == OBJECT_TYPE_LIFE_SAFETY_ZONE ||
         objectType == OBJECT_TYPE_NETWORK_PORT)) {
        *value = false;
        return true;
    }
    return false;
}

// OCTET STRING - the Network Port's BACnet/IP addressing. The stack cannot know
// the host's IP, so the application must supply IP_Address and IP_Subnet_Mask
// (and IP_Default_Gateway). Each is four octets. The stack also reads IP_Address
// (with BACnet_IP_UDP_Port) to build the port's six-octet MAC_Address.
bool GetPropertyOctetString(const uint32_t deviceInstance, const uint16_t objectType,
                            const uint32_t objectInstance, const uint32_t propertyIdentifier,
                            uint8_t* value, uint32_t* valueElementCount,
                            const uint32_t maxElementCount, const bool useArrayIndex,
                            const uint32_t propertyArrayIndex, uint32_t* errorCode) {
    (void)useArrayIndex;
    (void)errorCode;
    (void)propertyArrayIndex;
    if (deviceInstance != g_deviceInstance ||
        objectType != OBJECT_TYPE_NETWORK_PORT ||
        objectInstance != NETWORK_PORT_INSTANCE ||
        maxElementCount < 4) {
        return false;
    }
    const uint8_t* source = NULL;
    switch (propertyIdentifier) {
        case PROPERTY_IDENTIFIER_IP_ADDRESS:         source = g_ipAddress; break;
        case PROPERTY_IDENTIFIER_IP_SUBNET_MASK:     source = g_ipSubnetMask; break;
        case PROPERTY_IDENTIFIER_IP_DEFAULT_GATEWAY: source = g_ipDefaultGateway; break;
        default: return false;
    }
    memcpy(value, source, 4);
    *valueElementCount = 4;
    return true;
}

// Small helper: copy a C string into the stack's character-string buffer and
// set the element count + encoding. Returns true (so callers can `return`).
static bool ReturnCharacterString(const char* text, char* value,
                                  uint32_t* valueElementCount,
                                  const uint32_t maxElementCount,
                                  uint8_t* encodingType) {
    uint32_t length = (uint32_t)strlen(text);
    if (length > maxElementCount) {
        // Truncate SILENTLY to fit the stack's buffer. maxElementCount is
        // MAX_CHARACTER_STRING_SIZE (256 in this build), and our longest string
        // (DEVICE_DESCRIPTION) fits with room to spare - so this never trips
        // here. But if you build with STACK_OPTION_TARGET_EMBEDDED, that limit drops to
        // 64, and a long Object_Name or Description would be clipped mid-word
        // with nothing on the wire or console to tell you. If you lengthen any
        // served string, check it against MAX_CHARACTER_STRING_SIZE for your
        // target, or make this truncation loud.
        length = maxElementCount;
    }
    memcpy(value, text, length);
    *valueElementCount = length;
    *encodingType = CHARACTER_STRING_ENCODING_UTF8;
    return true;
}

// CHARACTER STRING - Object_Name for each object, and the device Description.
bool GetPropertyCharString(const uint32_t deviceInstance, const uint16_t objectType,
                           const uint32_t objectInstance, const uint32_t propertyIdentifier,
                           char* value, uint32_t* valueElementCount,
                           const uint32_t maxElementCount, uint8_t* encodingType,
                           const bool useArrayIndex, const uint32_t propertyArrayIndex,
                           uint32_t* errorCode) {
    if (deviceInstance != g_deviceInstance) {
        return false;
    }

    // State_Text (optional) - one label per state of the Multi-State Input. It is
    // a BACnet array, so the stack asks for one element at a time by index
    // (1..Number_Of_States). Present_Value 1 -> "On", 2 -> "Off", 3 -> "Auto".
    if (objectType == OBJECT_TYPE_MULTI_STATE_INPUT &&
        objectInstance == MULTI_STATE_INPUT_INSTANCE &&
        propertyIdentifier == PROPERTY_IDENTIFIER_STATE_TEXT && useArrayIndex) {
        static const char* const stateText[] = { "On", "Off", "Auto" };
        if (propertyArrayIndex >= 1 && propertyArrayIndex <= MULTI_STATE_INPUT_NUMBER_OF_STATES) {
            return ReturnCharacterString(stateText[propertyArrayIndex - 1], value,
                                         valueElementCount, maxElementCount, encodingType);
        }
        // The one place in this file where naming an error is clearly right: the
        // client asked for State_Text[n] and this object has no element n. That
        // is not "no opinion" - it is a wrong read, and the spec has a code for
        // it. Without this the client would silently receive an empty string.
        *errorCode = ERROR_CODE_INVALID_ARRAY_INDEX;
        return false;
    }

    // Object_Name - the colour name for each object.
    if (propertyIdentifier == PROPERTY_IDENTIFIER_OBJECT_NAME) {
        if (objectType == OBJECT_TYPE_DEVICE && objectInstance == g_deviceInstance) {
            return ReturnCharacterString(DEVICE_NAME, value, valueElementCount, maxElementCount, encodingType);
        }
        if (objectType == OBJECT_TYPE_ANALOG_INPUT && objectInstance == ANALOG_INPUT_INSTANCE) {
            return ReturnCharacterString("Bronze", value, valueElementCount, maxElementCount, encodingType);
        }
        if (objectType == OBJECT_TYPE_BINARY_INPUT && objectInstance == BINARY_INPUT_INSTANCE) {
            return ReturnCharacterString("Emerald", value, valueElementCount, maxElementCount, encodingType);
        }
        if (objectType == OBJECT_TYPE_MULTI_STATE_INPUT && objectInstance == MULTI_STATE_INPUT_INSTANCE) {
            return ReturnCharacterString("Hot Pink", value, valueElementCount, maxElementCount, encodingType);
        }
        if (objectType == OBJECT_TYPE_ANALOG_OUTPUT && objectInstance == ANALOG_OUTPUT_INSTANCE) {
            return ReturnCharacterString("Chartreuse", value, valueElementCount, maxElementCount, encodingType);
        }
        if (objectType == OBJECT_TYPE_BINARY_OUTPUT && objectInstance == BINARY_OUTPUT_INSTANCE) {
            return ReturnCharacterString("Fuchsia", value, valueElementCount, maxElementCount, encodingType);
        }
        if (objectType == OBJECT_TYPE_LIFE_SAFETY_POINT && objectInstance == LIFE_SAFETY_POINT_INSTANCE) {
            return ReturnCharacterString("Amber", value, valueElementCount, maxElementCount, encodingType);
        }
        if (objectType == OBJECT_TYPE_LIFE_SAFETY_ZONE && objectInstance == LIFE_SAFETY_ZONE_INSTANCE) {
            return ReturnCharacterString("Azure", value, valueElementCount, maxElementCount, encodingType);
        }
        if (objectType == OBJECT_TYPE_NOTIFICATION_CLASS && objectInstance == NOTIFICATION_CLASS_INSTANCE) {
            return ReturnCharacterString("Crimson", value, valueElementCount, maxElementCount, encodingType);
        }
        if (objectType == OBJECT_TYPE_MULTI_STATE_OUTPUT && objectInstance == MULTI_STATE_OUTPUT_INSTANCE) {
            return ReturnCharacterString("Indigo", value, valueElementCount, maxElementCount, encodingType);
        }
        if (objectType == OBJECT_TYPE_NETWORK_PORT && objectInstance == NETWORK_PORT_INSTANCE) {
            return ReturnCharacterString("Vermilion", value, valueElementCount, maxElementCount, encodingType);
        }
    }

    // The remaining strings are all on the Device object - its identity, read
    // by clients and used to populate the device's I-Am / object list.
    if (objectType == OBJECT_TYPE_DEVICE && objectInstance == g_deviceInstance) {
        switch (propertyIdentifier) {
            case PROPERTY_IDENTIFIER_DESCRIPTION:
                return ReturnCharacterString(DEVICE_DESCRIPTION, value, valueElementCount, maxElementCount, encodingType);
            case PROPERTY_IDENTIFIER_VENDOR_NAME:
                return ReturnCharacterString(VENDOR_NAME, value, valueElementCount, maxElementCount, encodingType);
            case PROPERTY_IDENTIFIER_MODEL_NAME:
                return ReturnCharacterString(MODEL_NAME, value, valueElementCount, maxElementCount, encodingType);
            case PROPERTY_IDENTIFIER_FIRMWARE_REVISION:
                return ReturnCharacterString(g_firmwareRevision.c_str(), value, valueElementCount, maxElementCount, encodingType);
            case PROPERTY_IDENTIFIER_APPLICATION_SOFTWARE_VERSION:
                return ReturnCharacterString(APP_VERSION, value, valueElementCount, maxElementCount, encodingType);
            default:
                break;
        }
    }

    return false;
}

// -----------------------------------------------------------------------------
// 2b. Property "set" callbacks - the heart of B-SA (DS-WP-B)
//
// The stack calls these when a client sends WriteProperty to a commandable
// output's Present_Value. The value arrives already decoded into the matching
// data type, together with the priority (1..16) the client wrote at. We store it
// in the object's Priority_Array; the stack recomputes Present_Value from the
// array on the next read. A WriteProperty of NULL relinquishes a slot and arrives
// through SetPropertyNull instead.
//
// Return true when we accept the write; return false (optionally setting
// *errorCode) to reject it, and the stack answers with a BACnet Error-PDU.
// -----------------------------------------------------------------------------

// REAL write - Analog Output 1 (Chartreuse) Present_Value.
bool SetPropertyReal(const uint32_t deviceInstance, const uint16_t objectType,
                     const uint32_t objectInstance, const uint32_t propertyIdentifier,
                     const float value, const bool useArrayIndex,
                     const uint32_t propertyArrayIndex, const uint8_t priority,
                     uint32_t* errorCode) {
    (void)useArrayIndex;
    (void)propertyArrayIndex;
    (void)errorCode;
    if (deviceInstance != g_deviceInstance) {
        return false;
    }
    Commandable* c = GetCommandable(objectType, objectInstance);
    if (c != NULL && objectType == OBJECT_TYPE_ANALOG_OUTPUT &&
        propertyIdentifier == PROPERTY_IDENTIFIER_PRESENT_VALUE) {
        // An Analog Output accepts any REAL here. A real device that models the
        // optional Min_Pres_Value / Max_Pres_Value properties would reject an
        // out-of-band value with value-out-of-range, exactly as the Binary and
        // Multi-State Output setters below do for their fixed ranges:
        //     if (value < g_min || value > g_max) {
        //         *errorCode = ERROR_CODE_VALUE_OUT_OF_RANGE; return false;
        //     }
        CommandWrite(c, priority, (double)value);
        printf("WriteProperty: Analog Output %u (Chartreuse) <- %.2f @ priority %u\n",
               objectInstance, value, EffectivePriority(priority));
        return true;
    }
    return false;
}

// ENUMERATED write - Binary Output 1 (Fuchsia) Present_Value (0/1).
bool SetPropertyEnumerated(const uint32_t deviceInstance, const uint16_t objectType,
                           const uint32_t objectInstance, const uint32_t propertyIdentifier,
                           const uint32_t value, const bool useArrayIndex,
                           const uint32_t propertyArrayIndex, const uint8_t priority,
                           uint32_t* errorCode) {
    (void)useArrayIndex;
    (void)propertyArrayIndex;
    if (deviceInstance != g_deviceInstance) {
        return false;
    }
    // Life Safety Point 1 (Amber) / Life Safety Zone 1 (Azure): Mode is the one
    // writable property (Table 12-18 / 12-19 "W"). It is accepted unconditionally -
    // see the file header's note on Accepted_Modes. Present_Value and
    // Tracking_Value are NOT writable: only the detector (section 1b) and a
    // LifeSafetyOperation move them, so no client can fake or clear an alarm.
    LifeSafetyObjectState* lifeSafetyObject = GetLifeSafetyObject(objectType, objectInstance);
    if (lifeSafetyObject != NULL && propertyIdentifier == PROPERTY_IDENTIFIER_MODE) {
        lifeSafetyObject->mode = value;
        printf("WriteProperty: Life Safety %s %u (%s) Mode <- %u\n",
               lifeSafetyObject == &g_lifeSafetyPoint ? "Point" : "Zone", objectInstance,
               lifeSafetyObject == &g_lifeSafetyPoint ? "Amber" : "Azure", value);
        return true;
    }
    Commandable* c = GetCommandable(objectType, objectInstance);
    if (c != NULL && objectType == OBJECT_TYPE_BINARY_OUTPUT &&
        propertyIdentifier == PROPERTY_IDENTIFIER_PRESENT_VALUE) {
        // A Binary Output's Present_Value is 0 (inactive) or 1 (active). Reject
        // anything else with value-out-of-range - validating the written value is
        // part of being a conformant DS-WP-B device.
        if (value > 1) {
            *errorCode = ERROR_CODE_VALUE_OUT_OF_RANGE;
            return false;
        }
        CommandWrite(c, priority, (double)value);
        printf("WriteProperty: Binary Output %u (Fuchsia) <- %s @ priority %u\n",
               objectInstance, value ? "active" : "inactive", EffectivePriority(priority));
        return true;
    }
    return false;
}

// UNSIGNED write - Multi-State Output 1 (Indigo) Present_Value (state 1..3).
bool SetPropertyUnsignedInteger(const uint32_t deviceInstance, const uint16_t objectType,
                                const uint32_t objectInstance, const uint32_t propertyIdentifier,
                                const uint32_t value, const bool useArrayIndex,
                                const uint32_t propertyArrayIndex, const uint8_t priority,
                                uint32_t* errorCode) {
    (void)useArrayIndex;
    (void)propertyArrayIndex;
    if (deviceInstance != g_deviceInstance) {
        return false;
    }
    Commandable* c = GetCommandable(objectType, objectInstance);
    if (c != NULL && objectType == OBJECT_TYPE_MULTI_STATE_OUTPUT &&
        propertyIdentifier == PROPERTY_IDENTIFIER_PRESENT_VALUE) {
        // A Multi-State Output's Present_Value is a state number in 1..Number_Of_States.
        // Reject anything outside that range with value-out-of-range.
        if (value < 1 || value > MULTI_STATE_OUTPUT_NUMBER_OF_STATES) {
            *errorCode = ERROR_CODE_VALUE_OUT_OF_RANGE;
            return false;
        }
        CommandWrite(c, priority, (double)value);
        printf("WriteProperty: Multi-State Output %u (Indigo) <- state %u @ priority %u\n",
               objectInstance, value, EffectivePriority(priority));
        return true;
    }
    return false;
}

// NULL write - relinquish a commandable output's Present_Value at a priority. The
// stack routes a WriteProperty of NULL here (one callback for every data type).
bool SetPropertyNull(const uint32_t deviceInstance, const uint16_t objectType,
                     const uint32_t objectInstance, const uint32_t propertyIdentifier,
                     const bool useArrayIndex, const uint32_t propertyArrayIndex,
                     const uint8_t priority, uint32_t* errorCode) {
    (void)useArrayIndex;
    (void)propertyArrayIndex;
    (void)errorCode;
    if (deviceInstance != g_deviceInstance) {
        return false;
    }
    Commandable* c = GetCommandable(objectType, objectInstance);
    if (c != NULL && propertyIdentifier == PROPERTY_IDENTIFIER_PRESENT_VALUE) {
        CommandRelinquish(c, priority);
        printf("WriteProperty: relinquished %s %u @ priority %u\n",
               objectType == OBJECT_TYPE_ANALOG_OUTPUT ? "Analog Output" :
               objectType == OBJECT_TYPE_BINARY_OUTPUT ? "Binary Output" :
               "Multi-State Output",
               objectInstance, EffectivePriority(priority));
        return true;
    }
    return false;
}

// -----------------------------------------------------------------------------
// Shared password check for DeviceCommunicationControl and ReinitializeDevice.
// A device with no configured password (DCC_PASSWORD == "") accepts any request.
// The length is checked first (so the byte compare never reads past the wire
// buffer, which is NOT null-terminated). The byte loop folds into one accumulator
// rather than short-circuiting on the first wrong byte, so it does not leak WHERE
// the password first differs; the length itself is not treated as secret.
static bool PasswordAccepted(const char* password, uint32_t passwordLength) {
    const uint32_t requiredLength = (uint32_t)strlen(DCC_PASSWORD);
    if (requiredLength == 0) {
        return true; // no password required
    }
    if (password == NULL || passwordLength != requiredLength) {
        return false;
    }
    unsigned diff = 0;
    for (uint32_t i = 0; i < requiredLength; ++i) {
        diff |= (unsigned)((unsigned char)password[i] ^ (unsigned char)DCC_PASSWORD[i]);
    }
    return diff == 0;
}

// -----------------------------------------------------------------------------
// 2c. DeviceCommunicationControl callback - the B-ASC addition (DM-DCC-B)
//
// A management station sends DeviceCommunicationControl to tell a device to stop
// or resume communicating - useful to quiet a noisy device during commissioning.
// The CAS BACnet Stack runs the actual enable/disable state machine (and the
// optional re-enable timer) for us; this callback's job is to (a) validate the
// optional password and (b) let the application know what was asked.
//
//   enableDisable: 0 = enable (resume), 1 = disable (stop initiating AND
//                  responding), 2 = disable-initiation (keep responding).
//   useTimeDuration/timeDuration: if set, the device auto-re-enables after
//                  timeDuration minutes. The stack handles that timer.
//
// Return true to accept (the stack then applies the new communication state), or
// false with *errorCode = password-failure to reject a bad password.
//
// NOTE (Protocol_Revision >= 20): the plain "disable" value (1) is DEPRECATED.
// Even if this callback accepts it, the stack rejects the request with
// service-request-denied - the standard now expects "disable-initiation" (2)
// (the device keeps answering reads but stops initiating). So at rev 24 only
// enable (0) and disable-initiation (2) actually take effect.
// -----------------------------------------------------------------------------
bool DeviceCommunicationControl(const uint32_t deviceInstance, const uint8_t enableDisable,
                                const char* password, const uint8_t passwordLength,
                                const bool useTimeDuration, const uint16_t timeDuration,
                                uint32_t* errorCode) {
    if (deviceInstance != g_deviceInstance) {
        // Not our device. Set *errorCode even here - see the note at the end of
        // this function: a false return with *errorCode unset ships
        // "Error Code = success(84)", which is meaningless on the wire.
        *errorCode = ERROR_CODE_OPTIONAL_FUNCTIONALITY_NOT_SUPPORTED;
        return false;
    }

    // Check the password if this device requires one. A device with no configured
    // password (DCC_PASSWORD == "") accepts any request.
    //
    // Compare by LENGTH FIRST, then bytes. The reason is not buffer safety - the
    // stack hands us a null-terminated string - it is that a BACnet
    // CharacterString may legitimately contain embedded NULs, and strcmp would
    // silently compare only up to the first one. Never strcmp a wire string.
    //
    // On a mismatch we set *errorCode = password-failure, and the stack pairs
    // that specific code with Error Class = SECURITY (clause 16.1.1.3.1).
    //
    // NOTE ON SECURITY, because this is a tutorial and the honest answer matters:
    // a DCC password crosses the wire in PLAINTEXT. This is not a security
    // boundary - it is a guard against accidents. Anyone who can time this
    // compare can simply sniff the password instead. If you need real protection,
    // use BACnet/SC. (Do not read the accumulator loop below as a constant-time
    // compare: the printf on the reject path dwarfs any timing signal it removes.)
    const size_t requiredLength = strlen(DCC_PASSWORD);
    if (requiredLength > 0) {
        bool matches = (password != NULL) && (passwordLength == requiredLength);
        if (matches) {
            for (size_t i = 0; i < requiredLength; ++i) {
                if (password[i] != DCC_PASSWORD[i]) {
                    matches = false;
                    break;
                }
            }
        }
        if (!matches) {
            printf("DeviceCommunicationControl: REJECTED (password failure)\n");
            *errorCode = ERROR_CODE_PASSWORD_FAILURE;
            return false;
        }
    }

    // NOTE: the stack applies the deprecation rule AFTER this callback. For the
    // deprecated plain "disable" (1) at Protocol_Revision >= 20 it overrides our
    // acceptance and answers service-request-denied - so the line we print for
    // that case reflects the request received, not a state the device entered.
    const char* action = (enableDisable == DCC_ENABLE) ? "enable (resume communication)" :
                         (enableDisable == DCC_DISABLE) ? "disable (1) - DEPRECATED, the stack will reject this" :
                         (enableDisable == DCC_DISABLE_INITIATION) ? "disable-initiation (keep responding)" :
                         "unknown";
    if (useTimeDuration) {
        printf("DeviceCommunicationControl: %s for %u minute(s)\n", action, timeDuration);
    } else {
        printf("DeviceCommunicationControl: %s (indefinitely)\n", action);
    }
    // Accept. Nothing to write to *errorCode on the success path.
    //
    // IMPORTANT, AND IT IS NOT WHAT YOU WOULD GUESS: this callback MUST set
    // *errorCode on EVERY `false` return. The DCC path has no default. The stack
    // pre-initialises errorCode to BACnetErrorCode::success (which is 84, NOT 0)
    // and then, on a false return, does:
    //     if (errorCode == passwordFailure) -> Error Class SECURITY
    //     else                              -> Error Class SERVICES, code = errorCode
    // So returning false without setting *errorCode puts the literal nonsense
    // "Error Class = SERVICES, Error Code = success(84)" on the wire.
    //
    // This differs from the SetProperty* callbacks, which DO have a sensible
    // fallback (writeAccessDenied) - so do not carry the habit across.
    return true;
}

// -----------------------------------------------------------------------------
// 2d. The remaining B-AAC service callbacks
// -----------------------------------------------------------------------------

// ReinitializeDevice (DM-RD-B). A management station asks the device to restart.
// reinitializedState: 0 = COLDSTART, 1 = WARMSTART (2..6 are backup/restore states
// this example does not support). The stack handles the BACnet exchange; a real
// device would actually reboot/reset on COLD/WARMSTART. Here we just validate the
// password and acknowledge. Returns true to accept, false (+errorCode) to reject.
bool ReinitializeDevice(const uint32_t deviceInstance, const uint32_t reinitializedState,
                        const char* password, const uint32_t passwordLength,
                        uint32_t* errorCode) {
    if (deviceInstance != g_deviceInstance) {
        // Not our device. Set *errorCode even here (see the DCC note): an unset
        // false return ships the meaningless "Error Code = success(84)".
        *errorCode = ERROR_CODE_OPTIONAL_FUNCTIONALITY_NOT_SUPPORTED;
        return false;
    }
    if (!PasswordAccepted(password, passwordLength)) {
        printf("ReinitializeDevice: REJECTED (password failure)\n");
        *errorCode = ERROR_CODE_PASSWORD_FAILURE;
        return false;
    }
    // NOTE: do NOT restart here. Returning true only tells the stack the request
    // was accepted - it encodes the SimpleACK, which does not go out on the wire
    // until a later BACnetStack_Tick(). Reboot/exit/reset at this point and the
    // ACK is never transmitted: the client times out and reports this device as
    // unresponsive even though it obeyed. So record a deadline, return, let the
    // ACK ship, and do the actual restart from the main loop.
    if (reinitializedState == REINITIALIZE_STATE_COLDSTART) {
        printf("ReinitializeDevice: COLDSTART accepted (restarting in %u ms)\n",
               (unsigned)CASExampleHelper::RESTART_DELAY_MS);
        CASExampleHelper::RequestRestart(CASExampleHelper::RestartKind::Cold,
                                         CASExampleHelper::RESTART_DELAY_MS);
        return true;
    }
    if (reinitializedState == REINITIALIZE_STATE_WARMSTART) {
        printf("ReinitializeDevice: WARMSTART accepted (re-initializing in %u ms)\n",
               (unsigned)CASExampleHelper::RESTART_DELAY_MS);
        CASExampleHelper::RequestRestart(CASExampleHelper::RestartKind::Warm,
                                         CASExampleHelper::RESTART_DELAY_MS);
        return true;
    }
    // Backup/restore states (2..6) are not supported by this example. The
    // conventional rejection is optional-functionality-not-supported (Error Class
    // SERVICES) - "I don't do that" rather than "bad value".
    printf("ReinitializeDevice: state %u not supported\n", reinitializedState);
    *errorCode = ERROR_CODE_OPTIONAL_FUNCTIONALITY_NOT_SUPPORTED;
    return false;
}

// SetSystemTime (DM-TS-B and DM-UTC-B). The stack executes both
// TimeSynchronization and UTCTimeSynchronization and, after converting UTC to
// local time for the UTC variant, calls this one callback so the application can
// set its clock. A real device would set its RTC; here we remember the value so
// Local_Date / Local_Time read it back (below). The matching GetSystemTime
// callback (used for notification time stamps and event time delays) is
// registered by the shared helper.
//
// Local_Date / Local_Time: enabling either TimeSynchronization service makes the
// stack list both in the Device's Property_List (135-2024 Table 12-13 footnotes 3
// and 4), but the stack does not serve them - the GetPropertyDate /
// GetPropertyTime callbacks below do, from g_syncedDateTime. It starts seeded from
// the host clock (InitSyncedDateTimeFromHost, called once at start-up), so a read
// before any TimeSynchronization still answers. Without these callbacks both reads
// failed with value-not-initialized (found by the discovery Wire Test).
struct SyncedDateTime {
    bool valid = false;
    uint8_t yearMinus1900 = 0, month = 1, day = 1, weekday = 1;
    uint8_t hour = 0, minute = 0, second = 0, hundredthSecond = 0;
};
static SyncedDateTime g_syncedDateTime;

// Seed g_syncedDateTime from the host OS clock. ON REAL HARDWARE read the
// device's own RTC instead.
static void InitSyncedDateTimeFromHost() {
    const time_t now = time(NULL);
    struct tm parts;
#if defined(_WIN32)
    localtime_s(&parts, &now);
#else
    localtime_r(&now, &parts);
#endif
    g_syncedDateTime.yearMinus1900 = (uint8_t)parts.tm_year;
    g_syncedDateTime.month = (uint8_t)(parts.tm_mon + 1);
    g_syncedDateTime.day = (uint8_t)parts.tm_mday;
    // BACnet weekday is 1=Monday..7=Sunday; struct tm's tm_wday is 0=Sunday..6=Saturday.
    g_syncedDateTime.weekday = (uint8_t)(parts.tm_wday == 0 ? 7 : parts.tm_wday);
    g_syncedDateTime.hour = (uint8_t)parts.tm_hour;
    g_syncedDateTime.minute = (uint8_t)parts.tm_min;
    g_syncedDateTime.second = (uint8_t)parts.tm_sec;
    g_syncedDateTime.hundredthSecond = 0;
    g_syncedDateTime.valid = true;
}

bool SetSystemTime(const uint32_t deviceInstance, const uint8_t year, const uint8_t month,
                   const uint8_t day, const uint8_t weekday, const uint8_t hour,
                   const uint8_t minute, const uint8_t second, const uint8_t hundrethSeconds) {
    if (deviceInstance != g_deviceInstance) {
        return false;
    }
    g_syncedDateTime.yearMinus1900 = year;
    g_syncedDateTime.month = month;
    g_syncedDateTime.day = day;
    g_syncedDateTime.weekday = weekday;
    g_syncedDateTime.hour = hour;
    g_syncedDateTime.minute = minute;
    g_syncedDateTime.second = second;
    g_syncedDateTime.hundredthSecond = hundrethSeconds;
    g_syncedDateTime.valid = true;
    // BACnet dates count years from 1900 (year value 0 == 1900).
    printf("SetSystemTime: %04u-%02u-%02u %02u:%02u:%02u (Local_Date/Local_Time now read this back)\n",
           1900u + year, month, day, hour, minute, second);
    return true;
}

// GET - Local_Date / Local_Time, from g_syncedDateTime (see above).
bool GetPropertyDate(const uint32_t deviceInstance, const uint16_t objectType,
                     const uint32_t objectInstance, const uint32_t propertyIdentifier,
                     uint8_t* yearMinus1900, uint8_t* month, uint8_t* day, uint8_t* weekday,
                     const bool useArrayIndex, const uint32_t propertyArrayIndex,
                     uint32_t* errorCode) {
    (void)useArrayIndex;
    (void)propertyArrayIndex;
    (void)errorCode;
    if (deviceInstance != g_deviceInstance || objectType != OBJECT_TYPE_DEVICE ||
        objectInstance != g_deviceInstance ||
        propertyIdentifier != PROPERTY_IDENTIFIER_LOCAL_DATE || !g_syncedDateTime.valid) {
        return false;
    }
    *yearMinus1900 = g_syncedDateTime.yearMinus1900;
    *month = g_syncedDateTime.month;
    *day = g_syncedDateTime.day;
    *weekday = g_syncedDateTime.weekday;
    return true;
}

bool GetPropertyTime(const uint32_t deviceInstance, const uint16_t objectType,
                     const uint32_t objectInstance, const uint32_t propertyIdentifier,
                     uint8_t* hour, uint8_t* minute, uint8_t* second, uint8_t* hundredthSecond,
                     const bool useArrayIndex, const uint32_t propertyArrayIndex,
                     uint32_t* errorCode) {
    (void)useArrayIndex;
    (void)propertyArrayIndex;
    (void)errorCode;
    if (deviceInstance != g_deviceInstance || objectType != OBJECT_TYPE_DEVICE ||
        objectInstance != g_deviceInstance ||
        propertyIdentifier != PROPERTY_IDENTIFIER_LOCAL_TIME || !g_syncedDateTime.valid) {
        return false;
    }
    *hour = g_syncedDateTime.hour;
    *minute = g_syncedDateTime.minute;
    *second = g_syncedDateTime.second;
    *hundredthSecond = g_syncedDateTime.hundredthSecond;
    return true;
}

/// AcknowledgeAlarm (AE-ACK-B) - "Maintain a list of unacknowledged alarms / events"
// and "Notifying other recipients that the acknowledgment has been received"
// (135-2024 L.5.2). The STACK runs the 13.5.2 service procedure: it finds the object
// (UNKNOWN_OBJECT if there is none, NO_ALARM_CONFIGURED if it has no event
// reporting), then calls this callback, then checks that the acknowledged 'Time
// Stamp' is the latest one for that transition (else SERVICES / INVALID_TIMESTAMP)
// and that 'Event State Acknowledged' matches it. Only then does it set the
// Acked_Transitions bit, answer SimpleACK and send the ACK_NOTIFICATION to the same
// recipients the alarm went to. So this callback runs BEFORE that check - it is told
// about an attempt, not a success. (Same finding as B-AAC-CPP 1.4.0.)
//
// What the callback is for: the application's own say. Return false with *errorCode
// set (the stack pairs it with Error Class SERVICES) to refuse - SERVICE_REQUEST_DENIED
// for an operator who may not acknowledge this alarm, say. This example accepts every
// operator and logs who it was. Parameters it does not use are left UNNAMED (C++ lets
// you omit the name of a parameter you do not use, which is cleaner than a wall of
// (void) casts).
bool AcknowledgeAlarm(const uint32_t deviceInstance, const uint32_t acknowledgingProcessIdentifier,
                      const uint16_t eventObjectType, const uint32_t eventObjectInstance,
                      const uint16_t eventStateAcknowledged, const uint8_t /*eventTimeStampYear*/,
                      const uint8_t /*eventTimeStampMonth*/, const uint8_t /*eventTimeStampDay*/,
                      const uint8_t /*eventTimeStampWeekday*/, const uint8_t /*eventTimeStampHour*/,
                      const uint8_t /*eventTimeStampMinute*/, const uint8_t /*eventTimeStampSecond*/,
                      const uint8_t /*eventTimeStampHundrethSecond*/, const char* acknowledgementSource,
                      const uint32_t acknowledgementSourceLength, const uint8_t /*acknowledgementSourceEncoding*/,
                      const bool /*timeOfAcknowledgementIsTime*/, const bool /*timeOfAcknowledgementIsSequenceNumber*/,
                      const bool /*timeOfAcknowledgementIsDateTime*/, const uint8_t /*timeOfAcknowledgementYear*/,
                      const uint8_t /*timeOfAcknowledgementMonth*/, const uint8_t /*timeOfAcknowledgementDay*/,
                      const uint8_t /*timeOfAcknowledgementWeekday*/, const uint8_t /*timeOfAcknowledgementHour*/,
                      const uint8_t /*timeOfAcknowledgementMinute*/, const uint8_t /*timeOfAcknowledgementSecond*/,
                      const uint8_t /*timeOfAcknowledgementHundrethSecond*/,
                      const uint16_t /*timeOfAcknowledgementSequenceNumber*/, uint32_t* errorCode) {
    if (deviceInstance != g_deviceInstance) {
        *errorCode = ERROR_CODE_SERVICE_REQUEST_DENIED;
        return false;
    }
    // The source is a wire string: not null-terminated, and possibly longer than we
    // want to print. Print at most 64 characters of it, by length.
    const int sourceLength = (int)(acknowledgementSourceLength < 64 ? acknowledgementSourceLength : 64);
    printf("AcknowledgeAlarm: \"%.*s\" (process %u) acknowledges object (type %u, instance %u) "
           "event state %u - the stack now checks its time stamp\n",
           sourceLength, acknowledgementSource != NULL ? acknowledgementSource : "",
           acknowledgingProcessIdentifier, eventObjectType, eventObjectInstance, eventStateAcknowledged);
    return true; // accept; the stack decides whether the acknowledgement matches the alarm
}

// LifeSafetyOperation (AE-LS-B, cl. 13.13) - "Execution of all forms of life safety
// operations" (L.5.2). An operator silences the sounders and strobes, unsilences them,
// or resets a latched alarm. The STACK checks that a named object exists
// (UNKNOWN_OBJECT) and that the 'Request' is a standard operation; whether the
// operation fits the object's CURRENT STATE is this callback's decision (the stack's
// own comment on the processor: "that check is ENTIRELY delegated to
// CallbackLifeSafetyOperation"). A false return with *errorCode set is sent as Error
// Class OBJECT with that code (SERVICES for SERVICE_REQUEST_DENIED).
//
// What each request does here:
//   SILENCE / _AUDIBLE / _VISUAL, UNSILENCE / _AUDIBLE / _VISUAL - set or clear the
//      object's audible and/or visual silenced flags (Silenced, cl. 12.15.23).
//   RESET, RESET_ALARM - unlatch an alarm: Present_Value returns to the detector's
//      reading (quiet). Refused with INVALID_OPERATION_IN_THIS_STATE (cl. 13.13.1.3.1,
//      "The operation specified in the 'Request' parameter is invalid in the current
//      state of the selected object") when the object is not latched in alarm - "a
//      RESET operation is requested but the object is not ready to be reset or is
//      already reset" (cl. 13.13.2) - and when the detector STILL sees smoke: a panel
//      does not let an operator clear an alarm whose cause is still there.
//   RESET_FAULT - a fault is not latched here (it clears when the detector is
//      repaired), so there is never a fault latch to reset: refused the same way.
//
// A zone is reset or silenced together with its member: an operation addressed to
// Azure also applies to Amber. With no 'Object Identifier' the operation goes to
// "all applicable objects" (cl. 13.13.1.1.4): each object it fits is operated, the
// rest are left alone, and the answer is Result(+) (cl. 13.13.2).
static bool IsResetOperation(uint8_t operation) {
    return operation == LIFE_SAFETY_OP_RESET || operation == LIFE_SAFETY_OP_RESET_ALARM ||
           operation == LIFE_SAFETY_OP_RESET_FAULT;
}

static bool IsSilenceOperation(uint8_t operation) {
    return operation >= LIFE_SAFETY_OP_SILENCE && operation <= LIFE_SAFETY_OP_SILENCE_VISUAL;
}

static bool IsUnsilenceOperation(uint8_t operation) {
    return operation >= LIFE_SAFETY_OP_UNSILENCE && operation <= LIFE_SAFETY_OP_UNSILENCE_VISUAL;
}

// Can `operation` be carried out on this object in its current state?
static bool OperationFits(const LifeSafetyObjectState& state, uint8_t operation) {
    if (IsSilenceOperation(operation) || IsUnsilenceOperation(operation)) {
        return true;
    }
    if (operation == LIFE_SAFETY_OP_RESET || operation == LIFE_SAFETY_OP_RESET_ALARM) {
        return state.presentValue == LIFE_SAFETY_STATE_ALARM &&
               state.trackingValue != LIFE_SAFETY_STATE_ALARM;
    }
    return false; // RESET_FAULT (no fault latch) and anything non-standard
}

static void ApplyOperation(LifeSafetyObjectState* state, uint8_t operation) {
    const bool audible = operation == LIFE_SAFETY_OP_SILENCE || operation == LIFE_SAFETY_OP_SILENCE_AUDIBLE ||
                         operation == LIFE_SAFETY_OP_UNSILENCE || operation == LIFE_SAFETY_OP_UNSILENCE_AUDIBLE;
    const bool visual = operation == LIFE_SAFETY_OP_SILENCE || operation == LIFE_SAFETY_OP_SILENCE_VISUAL ||
                        operation == LIFE_SAFETY_OP_UNSILENCE || operation == LIFE_SAFETY_OP_UNSILENCE_VISUAL;
    if (IsSilenceOperation(operation) || IsUnsilenceOperation(operation)) {
        if (audible) {
            state->silencedAudible = IsSilenceOperation(operation);
        }
        if (visual) {
            state->silencedVisual = IsSilenceOperation(operation);
        }
        return;
    }
    if (IsResetOperation(operation)) {
        state->presentValue = state->trackingValue;   // unlatch: back to what the detector reads
        state->silencedAudible = false;
        state->silencedVisual = false;
    }
}

bool LifeSafetyOperation(const uint32_t deviceInstance, const uint32_t requestingProcessIdentifier,
                         const char* requestingSource, const uint32_t requestingSourceLength,
                         const uint8_t requestOperation, const bool useObjectIdentifier,
                         const uint16_t objectType, const uint32_t objectInstance,
                         uint32_t* errorCode) {
    if (deviceInstance != g_deviceInstance) {
        *errorCode = ERROR_CODE_OPTIONAL_FUNCTIONALITY_NOT_SUPPORTED;
        return false;
    }
    if (requestOperation < LIFE_SAFETY_OP_SILENCE || requestOperation > LIFE_SAFETY_OP_UNSILENCE_VISUAL) {
        *errorCode = ERROR_CODE_VALUE_OUT_OF_RANGE; // "does not support the operation" (cl. 13.13.1.3.1)
        return false;
    }
    const int sourceLength = (int)(requestingSourceLength < 64 ? requestingSourceLength : 64);
    const char* sourceText = requestingSource != NULL ? requestingSource : "";

    // The objects this request operates: the named one (a zone with its member), or
    // every life-safety object when none is named.
    LifeSafetyObjectState* targets[2] = { NULL, NULL };
    if (!useObjectIdentifier) {
        targets[0] = &g_lifeSafetyPoint;
        targets[1] = &g_lifeSafetyZone;
    } else {
        LifeSafetyObjectState* named = GetLifeSafetyObject(objectType, objectInstance);
        if (named == NULL) {
            // An object that exists but is not a life-safety object (the stack has
            // already answered UNKNOWN_OBJECT for one that does not exist).
            *errorCode = ERROR_CODE_OPTIONAL_FUNCTIONALITY_NOT_SUPPORTED;
            return false;
        }
        targets[0] = named;
        if (named == &g_lifeSafetyZone) {
            targets[1] = &g_lifeSafetyPoint;   // Azure's member
        }
        if (!OperationFits(*named, requestOperation)) {
            printf("LifeSafetyOperation: \"%.*s\" (process %u) operation %u on %s REFUSED - "
                   "invalid in its current state (Present_Value %s, detector %s)\n",
                   sourceLength, sourceText, requestingProcessIdentifier, requestOperation,
                   named == &g_lifeSafetyPoint ? "Amber" : "Azure",
                   LifeSafetyStateName(named->presentValue), LifeSafetyStateName(named->trackingValue));
            *errorCode = ERROR_CODE_INVALID_OPERATION_IN_THIS_STATE;
            return false;
        }
    }
    for (size_t targetIndex = 0; targetIndex < 2; ++targetIndex) {
        LifeSafetyObjectState* target = targets[targetIndex];
        if (target != NULL && OperationFits(*target, requestOperation)) {
            ApplyOperation(target, requestOperation);
            printf("LifeSafetyOperation: \"%.*s\" (process %u) operation %u on %s -> Present_Value %s, "
                   "Silenced %u\n", sourceLength, sourceText, requestingProcessIdentifier, requestOperation,
                   target == &g_lifeSafetyPoint ? "Amber" : "Azure", LifeSafetyStateName(target->presentValue),
                   SilencedStateOf(*target));
        }
    }
    // A reset may have changed Present_Value: run the algorithms (the return to
    // normal is notified from here) and Amber's COV subscriptions.
    UpdateLifeSafetyObjects();
    return true;
}

// Build the 6-octet BACnet/IP connection string for the LOCAL SUBNET BROADCAST:
// the directed broadcast address (IP | ~mask) followed by the UDP port in network
// byte order. (When the mask is 0.0.0.0 - the helper's fallback - this collapses to
// the limited broadcast 255.255.255.255.)
static void LocalBroadcastConnString(uint8_t out[6]) {
    out[0] = (uint8_t)(g_ipAddress[0] | ~g_ipSubnetMask[0]);
    out[1] = (uint8_t)(g_ipAddress[1] | ~g_ipSubnetMask[1]);
    out[2] = (uint8_t)(g_ipAddress[2] | ~g_ipSubnetMask[2]);
    out[3] = (uint8_t)(g_ipAddress[3] | ~g_ipSubnetMask[3]);
    out[4] = (uint8_t)(g_bacnetIpUdpPort >> 8);
    out[5] = (uint8_t)(g_bacnetIpUdpPort & 0xFF);
}

// -----------------------------------------------------------------------------
// 3. main()
// -----------------------------------------------------------------------------
int main(int argc, char** argv) {
    // Show printf output immediately, even when stdout is piped to a file.
    setvbuf(stdout, NULL, _IONBF, 0);

    // --- Load the CAS BACnet Stack -------------------------------------------
    // Required in every link mode (source/static/DLL) before any other
    // BACnetStack_* call - see CASBACnetStackAdapter.h. In DLL mode this is the
    // step that actually resolves the symbols; skipping it there is a null-pointer
    // call, not a silent no-op, so it comes before even --version (which calls
    // BACnetStack_GetAPIMajorVersion() to print the linked stack's version).
    if (!LoadBACnetFunctions()) {
        fprintf(stderr, "Error: failed to load the CAS BACnet Stack: %s\n",
                CASBACnetStackAdapter_LastError());
        return 1;
    }

    // g_firmwareRevision (Device object property 44) - see its own doc
    // comment above for why this is the STACK's version, not this example's
    // own (that's Application_Software_Version/APP_VERSION instead). Must
    // happen after LoadBACnetFunctions() (these getters ARE some of the
    // functions it loads) and before the Device object is ever readable.
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "%u.%u.%u.%u",
                 BACnetStack_GetAPIMajorVersion(), BACnetStack_GetAPIMinorVersion(),
                 BACnetStack_GetAPIPatchVersion(), BACnetStack_GetAPIBuildVersion());
        g_firmwareRevision = buf;
    }

    // --- Command line + version --------------------------------------------
    // --help / --version print and exit, so handle them before we bind a socket
    // or touch the stack.
    if (CASExampleHelper::HandleHelpAndVersionArgs(argc, argv, APP_NAME, APP_VERSION)) {
        return 0;
    }
    const uint16_t port = CASExampleHelper::ParsePortArg(argc, argv, 47808);
    g_deviceInstance = CASExampleHelper::ParseDeviceIdArg(argc, argv, g_deviceInstance);
    CASExampleHelper::PrintVersion(APP_NAME, APP_VERSION);

    // --- Bind the BACnet/IP socket -----------------------------------------
    if (!CASExampleHelper::SetupUDP(port)) {
        return 1;
    }

    // Capture the BACnet/IP addressing the Network Port object will report.
    g_bacnetIpUdpPort = port;
    if (!CASExampleHelper::GetLocalIPv4(g_ipAddress, g_ipSubnetMask)) {
        printf("FYI: could not read a local IPv4 address; Network Port IP_Address "
               "will report 0.0.0.0.\n");
    }

    // --- Register callbacks -------------------------------------------------
    // Tell the helper which Network Port object owns the socket it just bound.
    // The stack identifies a link by its Network Port INSTANCE, so the transport
    // callbacks (and the start-up I-Am) have to name the one added below.
    CASExampleHelper::SetNetworkPortInstance(NETWORK_PORT_INSTANCE);
    // The transport + time callbacks are shared boilerplate.
    CASExampleHelper::RegisterCommonCallbacks();
    // The property callbacks are specific to this example.
    BACnetStack_RegisterCallbackGetPropertyReal(GetPropertyReal);
    BACnetStack_RegisterCallbackGetPropertyEnumerated(GetPropertyEnumerated);
    BACnetStack_RegisterCallbackGetPropertyUnsignedInteger(GetPropertyUnsignedInteger);
    BACnetStack_RegisterCallbackGetPropertyCharacterString(GetPropertyCharString);
    BACnetStack_RegisterCallbackGetPropertyBool(GetPropertyBool);
    BACnetStack_RegisterCallbackGetPropertyOctetString(GetPropertyOctetString);
    // Local_Date / Local_Time (listed because TimeSynchronization is executed),
    // seeded from the host clock until a TimeSynchronization arrives.
    BACnetStack_RegisterCallbackGetPropertyDate(GetPropertyDate);
    BACnetStack_RegisterCallbackGetPropertyTime(GetPropertyTime);
    InitSyncedDateTimeFromHost();
    // The "set" callbacks accept WriteProperty (DS-WP-B) to the commandable
    // outputs. One callback per written data type, plus the NULL callback that
    // relinquishes a priority slot.
    BACnetStack_RegisterCallbackSetPropertyReal(SetPropertyReal);
    BACnetStack_RegisterCallbackSetPropertyEnumerated(SetPropertyEnumerated);
    BACnetStack_RegisterCallbackSetPropertyUnsignedInteger(SetPropertyUnsignedInteger);
    BACnetStack_RegisterCallbackSetPropertyNull(SetPropertyNull);
    // Device-management callbacks (B-ASC + the B-AAC additions).
    BACnetStack_RegisterCallbackDeviceCommunicationControl(DeviceCommunicationControl); // DM-DCC-B
    BACnetStack_RegisterCallbackReinitializeDevice(ReinitializeDevice);                 // DM-RD-B
    BACnetStack_RegisterCallbackSetSystemTime(SetSystemTime);              // DM-TS-B / DM-UTC-B
    BACnetStack_RegisterCallbackAcknowledgeAlarm(AcknowledgeAlarm);                     // AE-ACK-B
    // AE-LS-B: executing LifeSafetyOperation (silence / unsilence / reset). The
    // service is compiled in by CMakeLists.txt (STACK_OPTION_DM_LSO_LIFE_SAFETY_OPERATION);
    // a stack built without it logs "This feature was not compiled" here, and
    // enabling the service below then fails, so the example stops at start-up
    // rather than advertise a service it cannot execute.
    BACnetStack_RegisterCallbackLifeSafetyOperation(LifeSafetyOperation);

    // --- Create the device --------------------------------------------------
    if (!BACnetStack_AddDevice(g_deviceInstance)) {
        printf("Error: Failed to add the Device %u.\n", g_deviceInstance);
        return 1;
    }

    // Enable the services a B-LSC must execute. We set each one explicitly so the
    // profile requirements are obvious. Numbers verified against
    // BACnetServicesSupported.h at the pin (see the runbook's §10 table).
    const struct { uint32_t service; const char* name; } services[] = {
        { SERVICE_READ_PROPERTY,                 "ReadProperty (DS-RP-B)" },
        { SERVICE_READ_PROPERTY_MULTIPLE,        "ReadPropertyMultiple (DS-RPM-B)" },
        { SERVICE_WRITE_PROPERTY,                "WriteProperty (DS-WP-B)" },
        { SERVICE_WRITE_PROPERTY_MULTIPLE,       "WritePropertyMultiple (DS-WPM-B)" },
        { SERVICE_SUBSCRIBE_COV,                 "SubscribeCOV (DS-COV-B)" },
        { SERVICE_DEVICE_COMMUNICATION_CONTROL,  "DeviceCommunicationControl (DM-DCC-B)" },
        { SERVICE_REINITIALIZE_DEVICE,           "ReinitializeDevice (DM-RD-B)" },
        { SERVICE_TIME_SYNCHRONIZATION,          "TimeSynchronization (DM-TS-B)" },
        { SERVICE_UTC_TIME_SYNCHRONIZATION,      "UTCTimeSynchronization (DM-UTC-B)" },
        { SERVICE_ACKNOWLEDGE_ALARM,             "AcknowledgeAlarm (AE-ACK-B)" },
        { SERVICE_GET_EVENT_INFORMATION,         "GetEventInformation (AE-INFO-B)" },
        { SERVICE_CONFIRMED_EVENT_NOTIFICATION,  "ConfirmedEventNotification (AE-LS-B)" },
        { SERVICE_UNCONFIRMED_EVENT_NOTIFICATION,"UnconfirmedEventNotification (AE-LS-B)" },
        // LifeSafetyOperation: AE-LS-B lists it under Execute (135-2024 K.2.13,
        // "able to process silence and reset operations on its life safety
        // objects"). Needs STACK_OPTION_DM_LSO_LIFE_SAFETY_OPERATION - see
        // CMakeLists.txt; since stack #3031 enabling a compiled-out service fails.
        { SERVICE_LIFE_SAFETY_OPERATION,         "LifeSafetyOperation (AE-LS-B) - build the stack with STACK_OPTION_DM_LSO_LIFE_SAFETY_OPERATION" },
    };
    for (size_t i = 0; i < sizeof(services) / sizeof(services[0]); ++i) {
        if (!BACnetStack_SetServiceEnabled(g_deviceInstance, services[i].service, true)) {
            printf("Error: Failed to enable the %s service.\n", services[i].name);
            return 1;
        }
    }

    // Discovery: Who-Is/I-Am (DM-DDB-B) and Who-Has/I-Have (DM-DOB-B).
    //
    // These need enabling even though the device already ANSWERS them. The
    // stack's service defaults are whoIs + whoHas + readProperty only
    // (BACnetDBDevice.cpp) - iAm and iHave are left FALSE. Who-Is is answered and
    // the start-up I-Am is sent regardless, because neither is gated on the bit;
    // but Protocol_Services_Supported is emitted verbatim from that bitstring, so
    // without these calls the device DOES I-Am and I-Have while telling every
    // client it supports neither. The README claims DM-DDB-B and DM-DOB-B; this
    // is what makes the claim true on the wire.
    if (!BACnetStack_SetServiceEnabled(g_deviceInstance, SERVICE_WHO_IS, true) ||
        !BACnetStack_SetServiceEnabled(g_deviceInstance, SERVICE_I_AM, true) ||
        !BACnetStack_SetServiceEnabled(g_deviceInstance, SERVICE_WHO_HAS, true) ||
        !BACnetStack_SetServiceEnabled(g_deviceInstance, SERVICE_I_HAVE, true)) {
        printf("Error: Failed to enable the discovery services (Who-Is/I-Am, Who-Has/I-Have).\n");
        return 1;
    }
    // --- Add the read-only sensor objects -----------------------------------
    // Every stack setup call returns a bool; a real device should always check
    // it, so this example does too.
    if (!BACnetStack_AddObject(g_deviceInstance, OBJECT_TYPE_ANALOG_INPUT, ANALOG_INPUT_INSTANCE)) {
        printf("Error: Failed to add Analog Input %u (Bronze).\n", ANALOG_INPUT_INSTANCE);
        return 1;
    }
    if (!BACnetStack_AddObject(g_deviceInstance, OBJECT_TYPE_BINARY_INPUT, BINARY_INPUT_INSTANCE)) {
        printf("Error: Failed to add Binary Input %u (Emerald).\n", BINARY_INPUT_INSTANCE);
        return 1;
    }
    if (!BACnetStack_AddObject(g_deviceInstance, OBJECT_TYPE_MULTI_STATE_INPUT, MULTI_STATE_INPUT_INSTANCE)) {
        printf("Error: Failed to add Multi-State Input %u (Hot Pink).\n", MULTI_STATE_INPUT_INSTANCE);
        return 1;
    }

    // --- Add the commandable OUTPUT objects (the B-SA additions) -------------
    // These accept WriteProperty. We make each one commandable below.
    if (!BACnetStack_AddObject(g_deviceInstance, OBJECT_TYPE_ANALOG_OUTPUT, ANALOG_OUTPUT_INSTANCE)) {
        printf("Error: Failed to add Analog Output %u (Chartreuse).\n", ANALOG_OUTPUT_INSTANCE);
        return 1;
    }
    if (!BACnetStack_AddObject(g_deviceInstance, OBJECT_TYPE_BINARY_OUTPUT, BINARY_OUTPUT_INSTANCE)) {
        printf("Error: Failed to add Binary Output %u (Fuchsia).\n", BINARY_OUTPUT_INSTANCE);
        return 1;
    }
    if (!BACnetStack_AddObject(g_deviceInstance, OBJECT_TYPE_MULTI_STATE_OUTPUT, MULTI_STATE_OUTPUT_INSTANCE)) {
        printf("Error: Failed to add Multi-State Output %u (Indigo).\n", MULTI_STATE_OUTPUT_INSTANCE);
        return 1;
    }

    // --- Add the life-safety objects (the B-LSC additions) -------------------
    if (!BACnetStack_AddObject(g_deviceInstance, OBJECT_TYPE_LIFE_SAFETY_POINT, LIFE_SAFETY_POINT_INSTANCE)) {
        printf("Error: Failed to add Life Safety Point %u (Amber).\n", LIFE_SAFETY_POINT_INSTANCE);
        return 1;
    }
    if (!BACnetStack_AddObject(g_deviceInstance, OBJECT_TYPE_LIFE_SAFETY_ZONE, LIFE_SAFETY_ZONE_INSTANCE)) {
        printf("Error: Failed to add Life Safety Zone %u (Azure).\n", LIFE_SAFETY_ZONE_INSTANCE);
        return 1;
    }

    // --- Add the Network Port object ----------------------------------------
    // Every BACnet device (Protocol_Revision 17+) must have at least one Network
    // Port object describing the port it talks on. This one is the BACnet/IP
    // application port; it is the lowest layer, so its reference port is "none".
    // networkNumber 0 with quality "unknown" describes a local port that has not
    // learned its network number - the right answer for a device that is not a
    // router and has not been told one.
    if (!BACnetStack_AddNetworkPortObject(
            g_deviceInstance, NETWORK_PORT_INSTANCE,
            NETWORK_PORT_NETWORK_TYPE_IPV4,
            NETWORK_PORT_PROTOCOL_LEVEL_BACNET_APPLICATION,
            0,  // networkNumber: not configured
            NETWORK_NUMBER_QUALITY_UNKNOWN,
            NETWORK_PORT_REFERENCE_PORT_NONE)) {
        printf("Error: Failed to add Network Port 1 (Vermilion).\n");
        return 1;
    }

    // --- Enable the OPTIONAL properties we choose to expose ------------------
    // The stack automatically enables an object's REQUIRED properties when the
    // object is added (AddObject / AddNetworkPortObject) - so Units, Polarity,
    // Number_Of_States, Out_Of_Service, and the Network Port's BACnet/IP
    // addressing (IP_Address, IP_Subnet_Mask, BACnet_IP_UDP_Port, ...) are
    // already enabled; our Get* callbacks just supply their values. Only
    // OPTIONAL properties need SetPropertyEnabled. State_Text is optional on a
    // Multi-State Input, so we enable it here (and serve it in GetPropertyCharString).
    //
    // The Device's Description is optional too, and it is an easy one to get
    // wrong: serving it from a Get callback is NOT enough. The stack checks
    // IsPropertyEnabled BEFORE it ever reaches the callbacks, and for an optional
    // property that check falls back to "is it required?" - which is false. So a
    // Description branch in the callback without this enable is DEAD CODE, and
    // the client reads back Error: unknown-property. (This example shipped
    // exactly that bug; it was caught by a reviewer tracing the stack source, not
    // by running it - a plausible-looking callback branch that never executes.)
    if (!BACnetStack_SetPropertyEnabled(g_deviceInstance, OBJECT_TYPE_DEVICE,
                                        g_deviceInstance, PROPERTY_IDENTIFIER_DESCRIPTION, true)) {
        printf("Error: Failed to enable Description on the Device object.\n");
        return 1;
    }

    if (!BACnetStack_SetPropertyEnabled(g_deviceInstance, OBJECT_TYPE_MULTI_STATE_INPUT,
                                        MULTI_STATE_INPUT_INSTANCE, PROPERTY_IDENTIFIER_STATE_TEXT, true)) {
        printf("Error: Failed to enable State_Text on Multi-State Input 1 (Hot Pink).\n");
        return 1;
    }

    // --- Make the output objects commandable --------------------------------
    // A commandable object's Present_Value is resolved from a 16-slot
    // Priority_Array plus a Relinquish_Default: a WriteProperty sets a slot,
    // writing NULL relinquishes it, and the highest-priority non-null slot (or
    // Relinquish_Default) wins.
    //
    // WORTH KNOWING BEFORE YOU COPY THIS: for ANALOG/BINARY/MULTI-STATE OUTPUT
    // the three calls below are effectively NO-OPS. They reproduce the
    // stack's own defaults. Verified in the stack source:
    //   - Present_Value on an Analog Output already defaults to required AND
    //     writable (BACnetDBPropertyProfile.cpp: presentValue -> SetProperty(
    //     true, true, Real)), and Priority_Array / Relinquish_Default default to
    //     required - so AddObject already enabled all three; and
    //   - IsPropertyCommandable() (BACnetBusinessLogic.cpp) returns true for
    //     analogOutput / binaryOutput / multiStateOutput Present_Value
    //     UNCONDITIONALLY - it consults no enable at all.
    // Delete this loop and these objects still accept WriteProperty. Nothing
    // here "flips the object into commandable mode"; the stack already did.
    //
    // So why keep it? Because it states the commandable contract in one visible
    // place, and because it becomes LOAD-BEARING the moment you copy this pattern
    // to an optionally-commandable type - Analog Value, Binary Value, Multi-State
    // Value. There Priority_Array / Relinquish_Default default to OPTIONAL (not
    // enabled), and IsPropertyCommandable() explicitly requires BOTH to be
    // enabled before it will treat the object as commandable. Omit these calls on
    // an Analog Value and it silently is not commandable.
    //
    // Carry the INSTANCE alongside the type rather than assuming instance 1. On
    // these output types the distinction is benign (see above) - but it is fatal
    // on a Value type, where the enable must land on the exact object you mean.
    // Say what you mean, so the pattern stays correct when it is copied.
    struct CommandableObject { uint16_t type; uint32_t instance; };
    const CommandableObject outputs[] = {
        { OBJECT_TYPE_ANALOG_OUTPUT,      ANALOG_OUTPUT_INSTANCE },
        { OBJECT_TYPE_BINARY_OUTPUT,      BINARY_OUTPUT_INSTANCE },
        { OBJECT_TYPE_MULTI_STATE_OUTPUT, MULTI_STATE_OUTPUT_INSTANCE },
    };
    for (size_t i = 0; i < sizeof(outputs) / sizeof(outputs[0]); ++i) {
        if (!BACnetStack_SetPropertyEnabled(g_deviceInstance, outputs[i].type, outputs[i].instance,
                                            PROPERTY_IDENTIFIER_PRIORITY_ARRAY, true) ||
            !BACnetStack_SetPropertyEnabled(g_deviceInstance, outputs[i].type, outputs[i].instance,
                                            PROPERTY_IDENTIFIER_RELINQUISH_DEFAULT, true) ||
            !BACnetStack_SetPropertyWritable(g_deviceInstance, outputs[i].type, outputs[i].instance,
                                             PROPERTY_IDENTIFIER_PRESENT_VALUE, true)) {
            printf("Error: Failed to make object type %u instance %u commandable.\n",
                   outputs[i].type, outputs[i].instance);
            return 1;
        }
    }

    // --- Configure intrinsic life-safety ALARMING (AE-LS-B / AE-ACK-B / AE-INFO-B)
    // 1) Mode is required-writable (Table 12-18 / 12-19 "W"). Present_Value stays
    //    read-only: the detector moves it, not a client.
    if (!BACnetStack_SetPropertyWritable(g_deviceInstance, OBJECT_TYPE_LIFE_SAFETY_POINT,
                                         LIFE_SAFETY_POINT_INSTANCE, PROPERTY_IDENTIFIER_MODE, true) ||
        !BACnetStack_SetPropertyWritable(g_deviceInstance, OBJECT_TYPE_LIFE_SAFETY_ZONE,
                                         LIFE_SAFETY_ZONE_INSTANCE, PROPERTY_IDENTIFIER_MODE, true)) {
        printf("Error: Failed to make Amber/Azure Mode writable.\n");
        return 1;
    }

    // 2) Create Notification Class 1 "Crimson" - it holds the recipient list and
    //    the notification priority for each transition (to-offnormal / to-fault /
    //    to-normal). Amber and Azure both route through it.
    if (!BACnetStack_AddNotificationClassObject(
            g_deviceInstance, NOTIFICATION_CLASS_INSTANCE,
            NC_PRIORITY_TO_OFFNORMAL, NC_PRIORITY_TO_FAULT, NC_PRIORITY_TO_NORMAL,
            true /*toOffNormalAckRequired*/, false /*toFaultAck*/, true /*toNormalAck*/)) {
        printf("Error: Failed to add Notification Class 1 (Crimson).\n");
        return 1;
    }

    // 3) Add a recipient to Crimson - WHERE the alarm notifications go: the operator
    //    workstation, by DEVICE instance (see RECIPIENT_DEVICE_INSTANCE), CONFIRMED.
    //    validDays 0x7F = every day; the time window 00:00:00 - 23:59:59 = always.
    const uint8_t validDaysAll = 0x7F;
    if (!BACnetStack_AddRecipientToNotificationClass(
            g_deviceInstance, NOTIFICATION_CLASS_INSTANCE,
            validDaysAll,
            0, 0, 0, 0,         // from 00:00:00.00
            23, 59, 59, 99,     // to   23:59:59.99
            RECIPIENT_PROCESS_IDENTIFIER,
            true,               // issue CONFIRMED notifications (retried if unanswered)
            true, true, true,   // notify on to-offnormal, to-fault, to-normal
            true, RECIPIENT_DEVICE_INSTANCE, // the DEVICE choice: the stack finds the address
            false,              // NOT the address choice
            0, NULL, 0)) {
        printf("Error: could not seed the Notification Class recipient (Crimson).\n");
        return 1;
    }

    // 4) Turn on intrinsic event reporting for Amber and Azure, routed through Crimson.
    if (!BACnetStack_SetAlarmsAndEventsForObjectEnabled(
            g_deviceInstance, OBJECT_TYPE_LIFE_SAFETY_POINT, LIFE_SAFETY_POINT_INSTANCE,
            NOTIFICATION_CLASS_INSTANCE, NOTIFY_TYPE_ALARM,
            true /*enableToOffNormal*/, true /*enableToFault*/, true /*enableToNormal*/,
            true /*enableEventDetection*/, true /*enabled*/) ||
        !BACnetStack_SetAlarmsAndEventsForObjectEnabled(
            g_deviceInstance, OBJECT_TYPE_LIFE_SAFETY_ZONE, LIFE_SAFETY_ZONE_INSTANCE,
            NOTIFICATION_CLASS_INSTANCE, NOTIFY_TYPE_ALARM,
            true, true, true, true, true)) {
        printf("Error: could not enable alarms on Amber/Azure.\n");
        return 1;
    }

    // 5) Arm the ChangeOfLifeSafety algorithm (F-ALARM-LS): alarm(2) drives the
    //    object to LIFE_SAFETY_ALARM event state, so it goes in
    //    lifeSafetyAlarmValues; alarmValues (the OFFNORMAL list) is empty. See
    //    the doc comment on BACnetStack_SetIntrinsicChangeOfLifeSafetyAlgorithm:
    //    "Values in alarmValues drive the object to OFFNORMAL and values in
    //    lifeSafetyAlarmValues drive it to LIFE_SAFETY_ALARM". (Up to example
    //    1.0.2 alarm(2) was passed as an alarmValue, so the object went to
    //    OFFNORMAL, not LIFE_SAFETY_ALARM as this comment said.)
    if (!BACnetStack_SetIntrinsicChangeOfLifeSafetyAlgorithm(
            g_deviceInstance, OBJECT_TYPE_LIFE_SAFETY_POINT, LIFE_SAFETY_POINT_INSTANCE,
            NULL, 0, LIFE_SAFETY_ALARM_VALUES, 1,
            LIFE_SAFETY_MODE_ON, 0 /*operationExpected - served via Get callback instead*/,
            LIFE_SAFETY_TIME_DELAY, false, 0, true) ||
        !BACnetStack_SetIntrinsicChangeOfLifeSafetyAlgorithm(
            g_deviceInstance, OBJECT_TYPE_LIFE_SAFETY_ZONE, LIFE_SAFETY_ZONE_INSTANCE,
            NULL, 0, LIFE_SAFETY_ALARM_VALUES, 1,
            LIFE_SAFETY_MODE_ON, 0,
            LIFE_SAFETY_TIME_DELAY, false, 0, true)) {
        printf("Error: could not arm the ChangeOfLifeSafety algorithm on Amber/Azure.\n");
        return 1;
    }

    // 6) Arm the fault algorithm: fault(3) drives the object to FAULT event state.
    if (!BACnetStack_SetFaultLifeSafetyAlgorithm(
            g_deviceInstance, OBJECT_TYPE_LIFE_SAFETY_POINT, LIFE_SAFETY_POINT_INSTANCE,
            LIFE_SAFETY_FAULT_VALUES, 1, true) ||
        !BACnetStack_SetFaultLifeSafetyAlgorithm(
            g_deviceInstance, OBJECT_TYPE_LIFE_SAFETY_ZONE, LIFE_SAFETY_ZONE_INSTANCE,
            LIFE_SAFETY_FAULT_VALUES, 1, true)) {
        printf("Error: could not arm the fault algorithm on Amber/Azure.\n");
        return 1;
    }

    // --- DS-COV-B: Bronze (Analog Input 1) and Amber (Life Safety Point 1) -----
    // Present_Value are COV-subscribable. SetCOVSettings bounds the subscription
    // table; SetMaxActiveCOVSubscriptions is the same bound expressed the older
    // way - both are set for clarity, they agree.
    if (!BACnetStack_SetCOVSettings(g_deviceInstance, COV_MAX_ACTIVE_SUBSCRIPTIONS,
                                    COV_MAX_SUPPORTED_LIFETIME_SECONDS) ||
        !BACnetStack_SetMaxActiveCOVSubscriptions(g_deviceInstance, COV_MAX_ACTIVE_SUBSCRIPTIONS)) {
        printf("Error: could not configure COV settings.\n");
        return 1;
    }
    if (!BACnetStack_SetPropertySubscribable(g_deviceInstance, OBJECT_TYPE_ANALOG_INPUT,
                                             ANALOG_INPUT_INSTANCE, PROPERTY_IDENTIFIER_PRESENT_VALUE, true) ||
        !BACnetStack_SetPropertySubscribable(g_deviceInstance, OBJECT_TYPE_LIFE_SAFETY_POINT,
                                             LIFE_SAFETY_POINT_INSTANCE, PROPERTY_IDENTIFIER_PRESENT_VALUE, true)) {
        printf("Error: could not make Bronze/Amber Present_Value COV-subscribable.\n");
        return 1;
    }

    // Who-Is is answered automatically. The spec also requires a device to
    // announce itself on start-up, so broadcast an unsolicited I-Am now (to the
    // local subnet broadcast - the Network Port's own network).
    CASExampleHelper::SendIAm(g_deviceInstance);

    // A B-AAC must also be able to DISCOVER other devices (DM-DDB-A), so broadcast
    // a Who-Is on start-up - every device on the subnet answers with its I-Am.
    uint8_t broadcastConn[6];
    LocalBroadcastConnString(broadcastConn);
    BACnetStack_SendWhoIs(broadcastConn, sizeof(broadcastConn), NETWORK_PORT_INSTANCE,
                          true /*broadcast*/, 0, NULL, 0);

    // SIMULATION only: the Wire Tests' detector back door (off unless
    // BLSC_SIMULATION_CONTROL_PORT is set - see simulation/SimulationControl.h).
    if (!SimulationControl::Start("BLSC_SIMULATION_CONTROL_PORT", HandleSimulationCommand)) {
        return 1;
    }

    printf("FYI: Device %u (\"%s\") ready. Vendor ID %u. Press 'h' for help.\n",
           g_deviceInstance, DEVICE_NAME, VENDOR_IDENTIFIER);

    // --- Run the stack ------------------------------------------------------
    // BACnetStack_Tick() processes incoming messages and timers. Call it
    // continuously, and poll the keyboard for interactive commands.
    bool running = true;
    while (running) {
        BACnetStack_Tick();
        SimulationControl::Service();   // SIMULATION: the detector's test control channel

        // --- Deferred restart (DM-RD-B) -------------------------------------
        // ReinitializeDevice only ARMED the restart; the SimpleACK has now had a
        // full second of ticks to reach the wire, so it is safe to act.
        //
        // A real device calls its platform reset here (reboot / watchdog / a
        // longjmp back to power-on init) and never returns from this block. This
        // example has no hardware to reset, so it demonstrates the equivalent
        // in-process work honestly rather than pretending:
        //
        //   COLDSTART - the full power-on path: every object returns to its
        //               start-up value, all commanded priorities are relinquished,
        //               and the device re-announces itself with an I-Am (which is
        //               what a client watches for to know the restart finished).
        //   WARMSTART - re-initialize communications but keep the process state a
        //               reboot would have preserved; the outputs a controls
        //               engineer commanded stay commanded. Still re-announces.
        //
        // A real device would also record Last_Restart_Reason and
        // Time_Of_Device_Restart at this point - see docs/deferred-restart-adoption.md.
        CASExampleHelper::RestartKind restartKind;
        if (CASExampleHelper::RestartDue(&restartKind)) {
            if (restartKind == CASExampleHelper::RestartKind::Cold) {
                printf("Restart: COLDSTART - restoring power-on state.\n");
                g_analogInput1Value = 21.5f;
                const Commandable analogOutputAtPowerOn = { { false }, { 0 }, 20.0 };
                const Commandable binaryOutputAtPowerOn = { { false }, { 0 }, 0.0 };
                const Commandable multiStateOutputAtPowerOn = { { false }, { 0 }, 1.0 };
                g_analogOutput = analogOutputAtPowerOn;
                g_binaryOutput = binaryOutputAtPowerOn;
                g_multiStateOutput = multiStateOutputAtPowerOn;
                // Amber and Azure return to quiet, unsilenced, and Mode "on" -
                // the same power-on-state convention as the outputs above.
                // A cold start clears every latch; the detector itself still
                // reads whatever it reads, so the objects take that reading again.
                g_lifeSafetyPoint = LIFE_SAFETY_POWER_ON_STATE;
                g_lifeSafetyZone = LIFE_SAFETY_POWER_ON_STATE;
                ApplyDetectorReading(g_detectorReading);
            } else {
                printf("Restart: WARMSTART - re-initializing, keeping commanded values.\n");
            }
            // Both kinds re-announce: a restarted device must issue an I-Am so
            // clients that had it bound learn it is back (and re-bind if its
            // address changed).
            CASExampleHelper::SendIAm(g_deviceInstance);
            printf("Restart: complete. Device %u is back.\n", g_deviceInstance);
        }

        switch (CASExampleHelper::PollKey()) {
            case CASExampleHelper::KeyCommand::Help:
                CASExampleHelper::PrintHelp(APP_NAME, APP_VERSION);
                break;
            case CASExampleHelper::KeyCommand::Quit:
                running = false;
                break;
            case CASExampleHelper::KeyCommand::ArrowUp:
                g_analogInput1Value += 1.1f;
                // Feed Bronze's COV subscribers (DS-COV-B) - see the file header.
                BACnetStack_UpdateValue(g_deviceInstance, OBJECT_TYPE_ANALOG_INPUT,
                                        ANALOG_INPUT_INSTANCE, PROPERTY_IDENTIFIER_PRESENT_VALUE);
                printf("Analog Input 1 (Bronze) = %.1f C\n", g_analogInput1Value);
                break;
            case CASExampleHelper::KeyCommand::ArrowDown:
                g_analogInput1Value -= 1.1f;
                BACnetStack_UpdateValue(g_deviceInstance, OBJECT_TYPE_ANALOG_INPUT,
                                        ANALOG_INPUT_INSTANCE, PROPERTY_IDENTIFIER_PRESENT_VALUE);
                printf("Analog Input 1 (Bronze) = %.1f C\n", g_analogInput1Value);
                break;
            case CASExampleHelper::KeyCommand::DemoAdvance:
                // Claimed by B-AAC's Schedule demo (docs/menu-keys.md) - B-LSC has
                // no Schedule object, so this key does nothing here.
                break;
            case CASExampleHelper::KeyCommand::None:
            default:
                break;
        }

#if defined(_WIN32)
        Sleep(1); // 1 ms - be a good citizen, don't spin the CPU
#else
        usleep(1000);
#endif
    }

    SimulationControl::Stop();
    CASExampleHelper::RestoreInput();
    CASExampleHelper::ShutdownUDP();
    return 0;
}
