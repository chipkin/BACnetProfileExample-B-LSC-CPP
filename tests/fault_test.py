#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
# Public-domain example code (CC0) - see ../LICENSE.
"""B-LSC Wire Test - Failure Mode: the smoke detector develops a fault.

    python3 tests/fault_test.py [--exe build/BACnetExampleBLSC] [--port 47821]

A detector whose head is pulled off its base can no longer see smoke. The Life Safety Point's
FAULT_LIFE_SAFETY fault algorithm (135-2024 cl. 13.4, applied per cl. 12.15) turns that into a
fault the operator is told about: Reliability MULTI_STATE_FAULT, Status_Flags FAULT, Event_State
FAULT, and a to-fault notification (priority 50) to the workstation. The SIMULATION test control
channel pulls the head.

  1. The workstation announces itself; the detector faults.
  2. The workstation receives the to-fault notification for the point (and its zone).
  3. Life Safety Point 1: Present_Value / Tracking_Value fault, Reliability MULTI_STATE_FAULT,
     Status_Flags {in-alarm, fault}, Event_State FAULT. GetEventInformation lists it.
  4. A RESET_FAULT is refused, OBJECT / INVALID_OPERATION_IN_THIS_STATE: the fault is the
     detector's, not a latch an operator can clear (cl. 13.13.2).
  5. The head goes back: Reliability NO_FAULT_DETECTED, Event_State NORMAL, and the
     return to normal is notified.
"""
import sys

import lsc_setup as setup
from lsc_setup import Workstation, alarm_codec, extras, wiretest
from wiretest import record

ALARM = alarm_codec


def run(workstation, args):
    record("The panel binds Device 389107 (Crimson's recipient) from the workstation's I-Am",
           setup.announce(workstation, args), "Device_Address_Binding")

    # --- 1-2. The fault -----------------------------------------------------------------
    mark = workstation.mark()
    reply = setup.detector(args, "fault")
    record("SIMULATION: the detector's head is pulled off its base (test control channel)", reply == "ok",
           f"reply {reply!r}")
    to_fault = workstation.wait_for_notification(
        setup.notification_from(setup.AMBER, ALARM.NOTIFY_TYPE_ALARM, ALARM.EVENT_STATE_FAULT),
        setup.NOTIFICATION_WAIT, mark)
    record("The workstation receives the point's to-fault notification (normal -> FAULT, priority 50)",
           to_fault is not None and (to_fault.from_state, to_fault.priority) ==
           (ALARM.EVENT_STATE_NORMAL, setup.PRIORITY_TO_FAULT), setup.waited(to_fault))
    zone_fault = workstation.wait_for_notification(
        setup.notification_from(setup.AZURE, ALARM.NOTIFY_TYPE_ALARM, ALARM.EVENT_STATE_FAULT),
        setup.NOTIFICATION_WAIT, mark)
    record("  ... and its zone's", zone_fault is not None, setup.waited(zone_fault))

    # --- 3. What the point reports -------------------------------------------------------
    value = setup.read(workstation, args, setup.AMBER, wiretest.PROPERTY_PRESENT_VALUE)
    tracking = setup.read(workstation, args, setup.AMBER, extras.PROPERTY_TRACKING_VALUE)
    record("Point: Present_Value and Tracking_Value fault",
           (value, tracking) == (extras.LIFE_SAFETY_STATE_FAULT, extras.LIFE_SAFETY_STATE_FAULT),
           f"read {value}, {tracking}")
    reliability = setup.read(workstation, args, setup.AMBER, wiretest.PROPERTY_RELIABILITY)
    record("Point: Reliability MULTI_STATE_FAULT", reliability == extras.RELIABILITY_MULTI_STATE_FAULT,
           f"read {reliability}")
    flags = setup.read(workstation, args, setup.AMBER, wiretest.PROPERTY_STATUS_FLAGS)
    record("Point: Status_Flags in-alarm and fault set",
           flags is not None and flags[wiretest.STATUS_FLAG_IN_ALARM] and flags[wiretest.STATUS_FLAG_FAULT],
           f"read {flags}")
    event_state = setup.read(workstation, args, setup.AMBER, ALARM.PROPERTY_EVENT_STATE)
    record("Point: Event_State FAULT", event_state == ALARM.EVENT_STATE_FAULT, f"read {event_state}")
    summaries, _, answer = setup.get_event_information(workstation, args)
    entry = setup.summary_for(summaries, setup.AMBER)
    record("GetEventInformation lists the point in FAULT", entry is not None and
           entry["event_state"] == ALARM.EVENT_STATE_FAULT,
           setup.describe_summary(entry) if summaries is not None else answer.describe())

    # --- 4. An operator cannot clear a detector fault -------------------------------------
    answer = setup.life_safety_operation(workstation, args, extras.LIFE_SAFETY_OPERATION_RESET_FAULT, setup.AMBER)
    record("Failure Mode: RESET_FAULT while the detector is faulty -> OBJECT / INVALID_OPERATION_IN_THIS_STATE",
           answer.error == (wiretest.ERROR_CLASS_OBJECT, extras.ERROR_CODE_INVALID_OPERATION_IN_THIS_STATE),
           answer.describe())

    # --- 5. Repaired ---------------------------------------------------------------------
    mark = workstation.mark()
    reply = setup.detector(args, "quiet")
    record("SIMULATION: the head goes back on its base", reply == "ok", f"reply {reply!r}")
    to_normal = workstation.wait_for_notification(
        setup.notification_from(setup.AMBER, ALARM.NOTIFY_TYPE_ALARM, ALARM.EVENT_STATE_NORMAL),
        setup.NOTIFICATION_WAIT, mark)
    record("The workstation receives the point's return to normal (FAULT -> normal)",
           to_normal is not None and to_normal.from_state == ALARM.EVENT_STATE_FAULT, setup.waited(to_normal))
    reliability = setup.read(workstation, args, setup.AMBER, wiretest.PROPERTY_RELIABILITY)
    event_state = setup.read(workstation, args, setup.AMBER, ALARM.PROPERTY_EVENT_STATE)
    record("Point: Reliability NO_FAULT_DETECTED, Event_State NORMAL",
           (reliability, event_state) == (wiretest.RELIABILITY_NO_FAULT_DETECTED, ALARM.EVENT_STATE_NORMAL),
           f"read {reliability}, {event_state}")


def main():
    args = setup.parser(__doc__).parse_args()
    controller = setup.start_controller(args)
    workstation = Workstation()
    try:
        run(workstation, args)
    finally:
        workstation.close()
        controller.stop()
        if not all(wiretest.results.RESULTS):
            controller.keep_log()
    return wiretest.summary()


if __name__ == "__main__":
    sys.exit(main())
