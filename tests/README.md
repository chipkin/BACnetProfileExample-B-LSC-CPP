# B-LSC Wire Tests

Tests that drive the running life safety controller over BACnet/IP from outside it,
and fail when its Profile Function is removed (135-2024 L.5.2: "A B-LSC device performs
life safety alarm detection and control. It supports life safety objects.").

The tests are written against the series' shared harness in
[`../wiretest/`](../wiretest/README.md) (vendored, like `common/`); everything
specific to this example is here. Python 3 standard library only - nothing to
`pip install`.

## Running

Build the example (README "Build"), then from the repository root:

```bash
python3 tests/run_wire_tests.py --exe build/BACnetExampleBLSC
```

On Windows, `--exe build\Release\BACnetExampleBLSC.exe`. It ends with
`ALL WIRE TESTS PASSED` (exit 0) or the scripts that failed. `--port` (default
47821) is the UDP port the test starts the example on; the test control channel
uses `--port` + 1. It takes about 45 seconds. CI (`.github/workflows/release.yml`)
runs it on Linux after the smoke test.

A request that gets no answer at all is sent once more with a visible `[RETRY]`
line (`wiretest_extras.request_with_retry`): a busy Windows host drops about 3 UDP
datagrams in 1000. An Error or Reject is an answer and is never retried.

## The setup

```
Workstation (127.0.0.1, ephemeral UDP port)  - the Peer: a Python operator workstation,
    |  BACnet/IP, unicast both ways             Device 389107 (announced with an I-Am)
BACnetExampleBLSC  UDP --port                - Device 389007
    |  loopback test control channel, UDP --port + 1 (BLSC_SIMULATION_CONTROL_PORT)
the SIMULATED smoke detector (main.cpp section 1b)
```

The Peer is `wiretest.Workstation`: a client that is also an event-notification
recipient, answering each ConfirmedEventNotification with a SimpleACK. It plays the
head-end of a fire alarm system. Notification Class 1 names it by **device
instance**, so before each test it sends the panel an I-Am and waits until the
panel's `Device_Address_Binding` lists Device 389107 (a check of its own).

The **test control channel** is the example's Simulation back door
(`simulation/SimulationControl.h`): `detector smoke`, `detector quiet`,
`detector fault`. It stands in for the physical world - nothing on the BACnet side
can make the detector see smoke, and `life_safety_test.py` checks that a client
WriteProperty of `Present_Value` is refused.

| File | What it is |
|---|---|
| `lsc_setup.py` | This controller's numbers, starting it with the control channel, the requests the tests send. |
| `wiretest_extras.py` | **Generic** helpers wiretest 0.6.0 lacks, proposed for 0.7.0: `life_safety_operation_request`, `announce` (a Workstation's I-Am), `SimulationControl` (the control-channel client), `request_with_retry`, and the life-safety constants. Nothing in it names B-LSC. |
| `discovery_test.py` | Every listed property reads (unchanged from 1.1.0). |
| `life_safety_test.py` | The scenario and two Failure Modes. |
| `fault_test.py` | The detector-fault Failure Mode. |

## `life_safety_test.py` - smoke, silence, acknowledge, reset

0. **Controls.** GetEventInformation lists nothing; the detector reads `quiet`; a
   client WriteProperty of its `Present_Value` gets `write-access-denied`; the
   Device's `Number_Of_APDU_Retries` is 3 (a missed alarm is sent again); no
   notification arrives before smoke. `Zone_Members` should list the detector:
   `[KNOWN GAP]`, stack #2036.
1. **The workstation is bound**: `Device_Address_Binding` lists Device 389107.
2. **Smoke** -> a ConfirmedEventNotification for Life Safety Point 1 and one for
   Life Safety Zone 1: from Device 389007, process id 1, Notification Class 1,
   `CHANGE_OF_LIFE_SAFETY`, normal -> `LIFE_SAFETY_ALARM`, ack required, priority
   100. A second workstation that is not Device 389107 receives nothing.
   `Tracking_Value` / `Present_Value` alarm, `Event_State` `LIFE_SAFETY_ALARM`,
   unsilenced, `Operation_Expected` `SILENCE`.
3. **GetEventInformation** lists both, to-offnormal unacknowledged.
4. **Failure Mode**: RESET while the smoke is still there -> `OBJECT` /
   `INVALID_OPERATION_IN_THIS_STATE` (cl. 13.13.1.3.1, 13.13.2); still latched.
5. **SILENCE** -> SimpleACK, `Silenced` all-silenced, `Operation_Expected` `RESET`.
6. **AcknowledgeAlarm** with the notification's time stamp -> SimpleACK,
   `Acked_Transitions` set, the `ACK_NOTIFICATION` arrives.
7. **The smoke clears** -> `Tracking_Value` quiet, `Present_Value` still alarm,
   `Event_State` still `LIFE_SAFETY_ALARM`, no return to normal notified.
8. **RESET** the point -> SimpleACK, `Present_Value` quiet, unsilenced, the return
   to normal arrives (ack required, priority 200); the zone, not named, is still
   latched; **RESET with no object** ("all applicable objects") resets it too.
9. **Failure Modes**: RESET of a point that is not latched is refused the same
   way; the zone's alarm, never acknowledged, is still listed by
   GetEventInformation (normal, to-offnormal and to-normal unacknowledged).

## `fault_test.py` - the detector's head comes off

1. The workstation is bound. `detector fault` -> to-fault notifications (normal ->
   `FAULT`, priority 50) for the point and its zone.
2. The point: `Present_Value` / `Tracking_Value` fault, `Reliability`
   `MULTI_STATE_FAULT` (the stack's `FAULT_LIFE_SAFETY` algorithm), `Status_Flags`
   in-alarm and fault, `Event_State` `FAULT`; GetEventInformation lists it.
3. **Failure Mode**: `RESET_FAULT` -> `OBJECT` / `INVALID_OPERATION_IN_THIS_STATE`
   (the fault is the detector's, not a latch).
4. `detector quiet` -> the return to normal (`FAULT` -> normal) arrives;
   `Reliability` `NO_FAULT_DETECTED`, `Event_State` `NORMAL`.

## `discovery_test.py` - every listed property reads as a value

1. **Who-Is** (for 389007, unicast) -> the device's I-Am.
2. **Device `Object_List`** names exactly the Device, Analog Input 1, Binary Input 1,
   Multi-state Input 1, Analog Output 1, Binary Output 1, Multi-state Output 1,
   Life Safety Point 1, Life Safety Zone 1, Notification Class 1 and Network Port 1. This is the control: a walk
   over nothing would pass.
3. For each object, **`Property_List`** reads, and a ReadProperty of every
   property it lists - plus `Object_Identifier`, `Object_Name`, `Object_Type`
   and `Property_List`, which cl. 12.1.1.4.1 leaves out of the list - returns a
   ComplexACK. An Error, Reject, Abort or no answer prints an `[UNREADABLE]` line
   and fails the check. 135-2024 cl. 12.1.1.4.1: Property_List holds "one
   property identifier for each property that exists within the object".
4. Network Port 1's **`IP_DNS_Server` (406)**, **`FD_BBMD_Address` (418)** and
   **`FD_Subscription_Lifetime` (419)** are each listed and read as a value: the
   three reads Explorer 4.23.2.0 found failing at stack 6.0.21 (reported against
   B-SS-CPP as #20, #21, #22; every example in the series serves the same
   Network Port).

## Results

Run 2026-10-16, stack `6.x` @ `64b2cfac5` (6.0.23), example v1.2.0:

| Platform | Build | Result |
|---|---|---|
| Windows 11 | STATIC (stack `ReleaseLib` `x64` built with `STACK_OPTION_DM_LSO_LIFE_SAFETY_OPERATION`, VS 18, NMake) | 17 + 37 + 13 = 67/67, 1 known gap - PASS x3, no `[RETRY]` |
| Linux (WSL Ubuntu 22.04) | SOURCE (g++) | 17 + 37 + 13 = 67/67, 1 known gap - PASS x3, no `[RETRY]` |

Every check was mutation-verified: each of 13 mutations of `main.cpp` (no
`UpdateValue`, another recipient instance, LifeSafetyOperation not enabled, reset
ignoring the smoke, no latch, no fault algorithm, silence doing nothing, no zone
roll-up, no retries, RESET_FAULT accepted, reset of an unlatched point accepted,
acknowledgments refused, unconfirmed notifications) makes at least one check fail;
the failure lines are in `CHANGELOG.md` 1.2.0 and the commit message.

Before the example served Local_Date / Local_Time (1.1.0), the discovery run failed
16/17 on exactly two reads - the stack lists both on the Device because the example
executes TimeSynchronization (135-2024 Table 12-13 footnotes 3 and 4):

```
[UNREADABLE] Device 389007 property 56: Error class=2 code=72
[UNREADABLE] Device 389007 property 57: Error class=2 code=72
```
