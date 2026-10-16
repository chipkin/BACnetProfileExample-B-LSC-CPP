#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
# Public-domain example code (CC0) - see ../LICENSE.
"""B-LSC Wire Test - a smoke detector goes to alarm; the operator silences, acknowledges and resets it.

    python3 tests/life_safety_test.py [--exe build/BACnetExampleBLSC] [--port 47821]

The Profile Function (135-2024 L.5.2): "A B-LSC device performs life safety alarm detection
and control. It supports life safety objects." - "Generation of alarm / event notifications,
including life safety alarms and the ability to direct them to recipients", "Execution of all
forms of life safety operations", "Maintain a list of unacknowledged alarms / events",
"Notifying other recipients that the acknowledgment has been received". The Peer is a Python
operator workstation (wiretest.Workstation) that is Device 389107.

  0. Controls: nothing is in alarm, no client can write the detector's Present_Value, and
     nothing reaches the workstation before smoke.
  1. The workstation announces itself (I-Am). A second workstation that is not Device 389107
     listens too.
  2. Smoke (SIMULATION, the test control channel). Life Safety Point 1 (the detector) and Life
     Safety Zone 1 go to alarm; each sends the workstation a ConfirmedEventNotification:
     CHANGE_OF_LIFE_SAFETY, normal -> LIFE_SAFETY_ALARM, ack required, priority 100. The second
     workstation hears nothing.
  3. GetEventInformation lists both, to-offnormal unacknowledged.
  4. Failure Mode (cl. 13.13.2): RESET while the detector still sees smoke is refused with
     OBJECT / INVALID_OPERATION_IN_THIS_STATE, and the alarm stays latched.
  5. SILENCE: Silenced reads all-silenced; Operation_Expected moves from SILENCE to RESET.
  6. AcknowledgeAlarm for the point: SimpleACK, Acked_Transitions set, ACK_NOTIFICATION arrives.
  7. The smoke clears: Tracking_Value follows (quiet) but Present_Value stays latched in alarm,
     and no return to normal is notified.
  8. RESET the point: Present_Value quiet, the to-normal notification arrives (ack required,
     priority 200). RESET with no object (all applicable objects) resets the zone the same way.
  9. Failure Modes: a RESET of a point that is no longer latched is refused; the zone's alarm,
     never acknowledged, stays in GetEventInformation's list.
"""
import struct
import sys

import lsc_setup as setup
from lsc_setup import Workstation, alarm_codec, extras, wiretest
from wiretest import record

ALARM = alarm_codec
LIFE_SAFETY_ALARM = extras.EVENT_STATE_LIFE_SAFETY_ALARM


def check_alarm(notification, object_reference, name):
    record(f"The workstation receives {name}'s life-safety alarm (confirmed)",
           notification is not None and notification.confirmed, setup.waited(notification))
    if notification is None:
        return False
    record(f"  ... from Device 389007, process id 1, Notification Class 1",
           (notification.initiating_device, notification.process_identifier, notification.notification_class) ==
           (setup.DEVICE, setup.RECIPIENT_PROCESS_IDENTIFIER, setup.NOTIFICATION_CLASS),
           f"{notification.initiating_device}, {notification.process_identifier}, {notification.notification_class}")
    record(f"  ... CHANGE_OF_LIFE_SAFETY, normal -> LIFE_SAFETY_ALARM, ack required, priority 100",
           (notification.event_type, notification.from_state, notification.to_state, notification.ack_required,
            notification.priority) ==
           (extras.EVENT_TYPE_CHANGE_OF_LIFE_SAFETY, ALARM.EVENT_STATE_NORMAL, LIFE_SAFETY_ALARM, True,
            setup.PRIORITY_TO_OFFNORMAL), notification.describe())
    return True


def run(workstation, bystander, args):
    # --- 0. Controls ---------------------------------------------------------------
    summaries, _, answer = setup.get_event_information(workstation, args)
    record("Control: GetEventInformation lists no event before any smoke", summaries == [],
           f"read {summaries}" if summaries is not None else answer.describe())
    value = setup.read(workstation, args, setup.AMBER, wiretest.PROPERTY_PRESENT_VALUE)
    record("Control: the detector (Life Safety Point 1) reads quiet", value == extras.LIFE_SAFETY_STATE_QUIET,
           f"read {value}")
    answer = extras.request_with_retry(workstation, lambda invoke_id: wiretest.write_property_request(
        invoke_id, *setup.AMBER, wiretest.PROPERTY_PRESENT_VALUE,
        wiretest.application_enumerated(extras.LIFE_SAFETY_STATE_ALARM)), args.port)
    record("Control: a client cannot write the detector's Present_Value (write-access-denied)",
           answer.error == (wiretest.ERROR_CLASS_PROPERTY, wiretest.ERROR_CODE_WRITE_ACCESS_DENIED),
           answer.describe())
    retries = setup.read(workstation, args, setup.DEVICE, setup.PROPERTY_NUMBER_OF_APDU_RETRIES)
    record("Device Number_Of_APDU_Retries is 3, so an alarm the workstation misses is sent again",
           retries == 3, f"read {retries}")

    zone_members = workstation.read_property(args.port, *setup.AZURE, extras.PROPERTY_ZONE_MEMBERS)
    wiretest.known_gap("Life Safety Zone 1's Zone_Members lists Life Safety Point 1, the member it rolls up",
                       zone_members.value_octets is not None and
                       struct.pack(">I", (setup.AMBER[0] << 22) | setup.AMBER[1]) in zone_members.value_octets,
                       "stack #2036: no customer-interface setter for Zone_Members (TODO.md 1); "
                       f"read {zone_members.describe()}")

    # --- 1. The workstation announces itself ------------------------------------------
    record("The panel binds Device 389107 (Crimson's recipient) from the workstation's I-Am",
           setup.announce(workstation, args), "Device_Address_Binding")
    quiet = workstation.wait_for_notification(lambda notification: True, setup.QUIET_WAIT)
    record("Control: no notification reaches the workstation before smoke", quiet is None, setup.waited(quiet, setup.QUIET_WAIT))

    # --- 2. Smoke ------------------------------------------------------------------------
    mark = workstation.mark()
    reply = setup.detector(args, "smoke")
    record("SIMULATION: the detector sees smoke (test control channel)", reply == "ok", f"reply {reply!r}")
    point_alarm = workstation.wait_for_notification(
        setup.notification_from(setup.AMBER, ALARM.NOTIFY_TYPE_ALARM, LIFE_SAFETY_ALARM), setup.NOTIFICATION_WAIT, mark)
    if not check_alarm(point_alarm, setup.AMBER, "Life Safety Point 1"):
        return
    zone_alarm = workstation.wait_for_notification(
        setup.notification_from(setup.AZURE, ALARM.NOTIFY_TYPE_ALARM, LIFE_SAFETY_ALARM), setup.NOTIFICATION_WAIT, mark)
    check_alarm(zone_alarm, setup.AZURE, "Life Safety Zone 1")
    stray = bystander.wait_for_notification(lambda notification: True, setup.QUIET_WAIT)
    record("Control: a workstation that is not Device 389107 receives nothing", stray is None,
           setup.waited(stray, setup.QUIET_WAIT))
    readings = {name: setup.read(workstation, args, setup.AMBER, identifier) for name, identifier in (
        ("Tracking_Value", extras.PROPERTY_TRACKING_VALUE), ("Present_Value", wiretest.PROPERTY_PRESENT_VALUE),
        ("Event_State", ALARM.PROPERTY_EVENT_STATE), ("Silenced", extras.PROPERTY_SILENCED),
        ("Operation_Expected", extras.PROPERTY_OPERATION_EXPECTED))}
    record("Point: Tracking_Value and Present_Value alarm, Event_State LIFE_SAFETY_ALARM",
           (readings["Tracking_Value"], readings["Present_Value"], readings["Event_State"]) ==
           (extras.LIFE_SAFETY_STATE_ALARM, extras.LIFE_SAFETY_STATE_ALARM, LIFE_SAFETY_ALARM), f"read {readings}")
    record("Point: unsilenced, Operation_Expected SILENCE",
           (readings["Silenced"], readings["Operation_Expected"]) ==
           (extras.SILENCED_UNSILENCED, extras.LIFE_SAFETY_OPERATION_SILENCE), f"read {readings}")

    # --- 3. GetEventInformation --------------------------------------------------------
    summaries, _, answer = setup.get_event_information(workstation, args)
    for name, reference in (("Life Safety Point 1", setup.AMBER), ("Life Safety Zone 1", setup.AZURE)):
        entry = setup.summary_for(summaries, reference)
        record(f"GetEventInformation lists {name}: LIFE_SAFETY_ALARM, to-offnormal unacknowledged",
               entry is not None and entry["event_state"] == LIFE_SAFETY_ALARM and
               entry["acked_transitions"] == [False, True, True],
               setup.describe_summary(entry) if summaries is not None else answer.describe())

    # --- 4. Failure Mode: reset while the smoke is still there ---------------------------
    answer = setup.life_safety_operation(workstation, args, extras.LIFE_SAFETY_OPERATION_RESET, setup.AMBER)
    record("Failure Mode: RESET while the detector still sees smoke -> OBJECT / INVALID_OPERATION_IN_THIS_STATE",
           answer.error == (wiretest.ERROR_CLASS_OBJECT, extras.ERROR_CODE_INVALID_OPERATION_IN_THIS_STATE),
           answer.describe())
    value = setup.read(workstation, args, setup.AMBER, wiretest.PROPERTY_PRESENT_VALUE)
    record("  ... and the alarm stays latched", value == extras.LIFE_SAFETY_STATE_ALARM, f"read {value}")

    # --- 5. Silence ----------------------------------------------------------------------
    answer = setup.life_safety_operation(workstation, args, extras.LIFE_SAFETY_OPERATION_SILENCE, setup.AMBER)
    record("LifeSafetyOperation SILENCE on the point gets a SimpleACK", answer.ok, answer.describe())
    silenced = setup.read(workstation, args, setup.AMBER, extras.PROPERTY_SILENCED)
    expected = setup.read(workstation, args, setup.AMBER, extras.PROPERTY_OPERATION_EXPECTED)
    record("Point: Silenced all-silenced, Operation_Expected RESET",
           (silenced, expected) == (extras.SILENCED_ALL, extras.LIFE_SAFETY_OPERATION_RESET),
           f"read {silenced}, {expected}")

    # --- 6. Acknowledge -----------------------------------------------------------------
    mark = workstation.mark()
    answer = setup.acknowledge(workstation, args, setup.AMBER, LIFE_SAFETY_ALARM, point_alarm.time_stamp)
    record("AcknowledgeAlarm for the point, with the alarm's time stamp, gets a SimpleACK", answer.ok,
           answer.describe())
    acked = setup.read(workstation, args, setup.AMBER, ALARM.PROPERTY_ACKED_TRANSITIONS)
    record("Point: Acked_Transitions to-offnormal acknowledged", acked == [True, True, True], f"read {acked}")
    ack_notification = workstation.wait_for_notification(
        setup.notification_from(setup.AMBER, ALARM.NOTIFY_TYPE_ACK_NOTIFICATION), setup.NOTIFICATION_WAIT, mark)
    record("The workstation receives the ACK_NOTIFICATION for LIFE_SAFETY_ALARM",
           ack_notification is not None and ack_notification.to_state == LIFE_SAFETY_ALARM,
           setup.waited(ack_notification))

    # --- 7. The smoke clears; the latch holds --------------------------------------------
    mark = workstation.mark()
    reply = setup.detector(args, "quiet")
    record("SIMULATION: the smoke clears", reply == "ok", f"reply {reply!r}")
    tracking = setup.read(workstation, args, setup.AMBER, extras.PROPERTY_TRACKING_VALUE)
    value = setup.read(workstation, args, setup.AMBER, wiretest.PROPERTY_PRESENT_VALUE)
    event_state = setup.read(workstation, args, setup.AMBER, ALARM.PROPERTY_EVENT_STATE)
    record("Point: Tracking_Value quiet, Present_Value still latched in alarm, Event_State LIFE_SAFETY_ALARM",
           (tracking, value, event_state) ==
           (extras.LIFE_SAFETY_STATE_QUIET, extras.LIFE_SAFETY_STATE_ALARM, LIFE_SAFETY_ALARM),
           f"read {tracking}, {value}, {event_state}")
    early = workstation.wait_for_notification(
        setup.notification_from(setup.AMBER, ALARM.NOTIFY_TYPE_ALARM, ALARM.EVENT_STATE_NORMAL), setup.QUIET_WAIT, mark)
    record("  ... and no return to normal is notified before the reset", early is None,
           setup.waited(early, setup.QUIET_WAIT))

    # --- 8. Reset --------------------------------------------------------------------------
    mark = workstation.mark()
    answer = setup.life_safety_operation(workstation, args, extras.LIFE_SAFETY_OPERATION_RESET, setup.AMBER)
    record("LifeSafetyOperation RESET on the point, smoke gone, gets a SimpleACK", answer.ok, answer.describe())
    value = setup.read(workstation, args, setup.AMBER, wiretest.PROPERTY_PRESENT_VALUE)
    silenced = setup.read(workstation, args, setup.AMBER, extras.PROPERTY_SILENCED)
    record("Point: Present_Value quiet, unsilenced", (value, silenced) ==
           (extras.LIFE_SAFETY_STATE_QUIET, extras.SILENCED_UNSILENCED), f"read {value}, {silenced}")
    to_normal = workstation.wait_for_notification(
        setup.notification_from(setup.AMBER, ALARM.NOTIFY_TYPE_ALARM, ALARM.EVENT_STATE_NORMAL),
        setup.NOTIFICATION_WAIT, mark)
    record("The workstation receives the point's return to normal (LIFE_SAFETY_ALARM -> normal, ack required, "
           "priority 200)", to_normal is not None and (to_normal.from_state, to_normal.ack_required,
                                                      to_normal.priority) ==
           (LIFE_SAFETY_ALARM, True, setup.PRIORITY_TO_NORMAL), setup.waited(to_normal))
    zone_value = setup.read(workstation, args, setup.AZURE, wiretest.PROPERTY_PRESENT_VALUE)
    record("Control: the zone was not named, so it is still latched in alarm",
           zone_value == extras.LIFE_SAFETY_STATE_ALARM, f"read {zone_value}")
    mark = workstation.mark()
    answer = setup.life_safety_operation(workstation, args, extras.LIFE_SAFETY_OPERATION_RESET)
    record("LifeSafetyOperation RESET with no object (all applicable objects) gets a SimpleACK", answer.ok,
           answer.describe())
    zone_normal = workstation.wait_for_notification(
        setup.notification_from(setup.AZURE, ALARM.NOTIFY_TYPE_ALARM, ALARM.EVENT_STATE_NORMAL),
        setup.NOTIFICATION_WAIT, mark)
    record("The workstation receives the zone's return to normal", zone_normal is not None,
           setup.waited(zone_normal))

    # --- 9. Failure Modes: nothing to reset; an alarm nobody acknowledged ----------------
    answer = setup.life_safety_operation(workstation, args, extras.LIFE_SAFETY_OPERATION_RESET, setup.AMBER)
    record("Failure Mode: RESET of a point that is not latched -> OBJECT / INVALID_OPERATION_IN_THIS_STATE",
           answer.error == (wiretest.ERROR_CLASS_OBJECT, extras.ERROR_CODE_INVALID_OPERATION_IN_THIS_STATE),
           answer.describe())
    summaries, _, answer = setup.get_event_information(workstation, args)
    zone = setup.summary_for(summaries, setup.AZURE)
    record("Failure Mode: the zone's alarm was never acknowledged, so GetEventInformation still lists it "
           "(normal, to-offnormal and to-normal unacknowledged)",
           zone is not None and zone["event_state"] == ALARM.EVENT_STATE_NORMAL and
           zone["acked_transitions"] == [False, True, False],
           setup.describe_summary(zone) if summaries is not None else answer.describe())
    point = setup.summary_for(summaries, setup.AMBER)
    record("  ... and the point, whose to-normal is unacknowledged too, is listed with to-offnormal acknowledged",
           point is not None and point["acked_transitions"] == [True, True, False], setup.describe_summary(point))


def main():
    args = setup.parser(__doc__).parse_args()
    controller = setup.start_controller(args)
    workstation = Workstation()
    bystander = Workstation()
    try:
        run(workstation, bystander, args)
    finally:
        bystander.close()
        workstation.close()
        controller.stop()
        if not all(wiretest.results.RESULTS):
            controller.keep_log()
    return wiretest.summary()


if __name__ == "__main__":
    sys.exit(main())
