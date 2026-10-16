# SPDX-License-Identifier: CC0-1.0
# Public-domain example code (CC0) - see ../LICENSE.
"""What B-LSC's Profile Function Wire Tests share: this controller's numbers, and starting it.

The setup, all on one host:

    Workstation (127.0.0.1, ephemeral UDP port) - the Peer: a Python operator workstation,
        |  BACnet/IP, unicast both ways       Device 389107 (it announces itself with an I-Am)
    BACnetExampleBLSC   UDP --port   Device 389007
        |  loopback test control channel, UDP --port + 1 (BLSC_SIMULATION_CONTROL_PORT)
    the SIMULATED smoke detector inside the example (main.cpp section 1b)

The Peer plays the head-end on a real fire alarm system (a B-OWS): the panel's Notification
Class 1 names it by device instance, it receives the life-safety alarms, silences, acknowledges
and resets them. The test control channel plays the physical world - smoke in front of the
detector, its head pulled off the base - and nothing on the BACnet side can do that.
"""
import argparse
import sys
import time
from pathlib import Path

TESTS = Path(__file__).resolve().parent
REPOSITORY = TESTS.parent
sys.path.insert(0, str(REPOSITORY))
sys.path.insert(0, str(TESTS))

import wiretest  # noqa: E402  (the vendored harness at the repository root)
from wiretest import Workstation  # noqa: E402  (wiretest 0.6.0)
from wiretest import alarm as alarm_codec  # noqa: E402
import wiretest_extras as extras  # noqa: E402  (generic helpers proposed for wiretest 0.7.0)

# Match main.cpp.
DEVICE_INSTANCE = 389007
DEVICE = (wiretest.OBJECT_TYPE_DEVICE, DEVICE_INSTANCE)
OBJECT_TYPE_LIFE_SAFETY_POINT = 21
OBJECT_TYPE_LIFE_SAFETY_ZONE = 22
OBJECT_TYPE_NOTIFICATION_CLASS = 15
AMBER = (OBJECT_TYPE_LIFE_SAFETY_POINT, 1)            # Life Safety Point 1, the smoke detector
AZURE = (OBJECT_TYPE_LIFE_SAFETY_ZONE, 1)             # Life Safety Zone 1, the detector's zone
CRIMSON = (OBJECT_TYPE_NOTIFICATION_CLASS, 1)         # Notification Class 1
WORKSTATION_DEVICE_INSTANCE = 389107                  # Crimson's recipient (device form)
RECIPIENT_PROCESS_IDENTIFIER = 1                      # ... and its process id
NOTIFICATION_CLASS = 1
PRIORITY_TO_OFFNORMAL = 100
PRIORITY_TO_FAULT = 50
PRIORITY_TO_NORMAL = 200
PROPERTY_NUMBER_OF_APDU_RETRIES = 73
OPERATOR_PROCESS_IDENTIFIER = 7                      # the workstation's own process (LSO / ack)
NOTIFICATION_WAIT = 5.0                               # seconds allowed for a notification to arrive
QUIET_WAIT = 2.0                                      # seconds watched to show that nothing arrives
SIMULATION_CONTROL_VARIABLE = "BLSC_SIMULATION_CONTROL_PORT"


def parser(description):
    arguments = argparse.ArgumentParser(description=description, formatter_class=argparse.RawDescriptionHelpFormatter)
    arguments.add_argument("--exe", default=str(wiretest.default_exe(REPOSITORY, "BACnetExampleBLSC")),
                           help="path to BACnetExampleBLSC (default: build/BACnetExampleBLSC)")
    arguments.add_argument("--port", type=int, default=47821,
                           help="the controller's UDP port (default 47821); the test control channel uses port + 1")
    return arguments


def start_controller(args):
    """The example, with its SIMULATION test control channel on --port + 1."""
    return wiretest.start(args.exe, ["--port", args.port], "controller",
                          environment={SIMULATION_CONTROL_VARIABLE: args.port + 1})


def detector(args, reading):
    """Move the simulated detector: 'quiet', 'smoke' or 'fault'. Returns the channel's reply."""
    return extras.SimulationControl(args.port + 1).command(f"detector {reading}")


def is_bound(workstation, args):
    """Does the panel's Device_Address_Binding list Device 389107 (the workstation)?"""
    answer = extras.request_with_retry(workstation, lambda invoke_id: wiretest.read_property_request(
        invoke_id, *DEVICE, wiretest.PROPERTY_DEVICE_ADDRESS_BINDING), args.port)
    return answer.ok and (wiretest.OBJECT_TYPE_DEVICE, WORKSTATION_DEVICE_INSTANCE) in         wiretest.decoded_values(answer.value_octets)


def announce(workstation, args, seconds=5.0):
    """The workstation's I-Am as Device 389107 - what lets the panel address its alarms - sent
    (again each second) until the panel's Device_Address_Binding shows it. Returns whether it does."""
    def announced_and_bound():
        extras.announce(workstation, WORKSTATION_DEVICE_INSTANCE, args.port)
        return wiretest.wait_until(lambda: is_bound(workstation, args), 1.0)
    return bool(wiretest.wait_until(announced_and_bound, seconds, interval=0))


def read(workstation, args, object_reference, property_identifier, array_index=None):
    """The decoded value of one property, or None (the Answer is printed on failure)."""
    answer = extras.request_with_retry(workstation, lambda invoke_id: wiretest.read_property_request(
        invoke_id, *object_reference, property_identifier, array_index), args.port)
    if not answer.ok:
        print(f"      read {object_reference} property {property_identifier}: {answer.describe()}")
    return answer.value


def life_safety_operation(workstation, args, operation, object_reference=None, source="Operator Olive"):
    return extras.request_with_retry(workstation, lambda invoke_id: extras.life_safety_operation_request(
        invoke_id, OPERATOR_PROCESS_IDENTIFIER, source, operation, object_reference), args.port)


def get_event_information(workstation, args):
    """(summaries, more_events, Answer) from one GetEventInformation."""
    answer = extras.request_with_retry(
        workstation, lambda invoke_id: alarm_codec.get_event_information_request(invoke_id), args.port)
    if answer.frame is None or answer.frame.pdu_type != wiretest.PDU_COMPLEX_ACK:
        return None, None, answer
    summaries, more_events = alarm_codec.parse_event_summaries(answer.frame.payload)
    return summaries, more_events, answer


def summary_for(summaries, object_reference):
    for entry in summaries or []:
        if entry["object"] == object_reference:
            return entry
    return None


def describe_summary(entry):
    if entry is None:
        return "not listed"
    return f"{entry['object']} event-state={entry.get('event_state')} acked={entry.get('acked_transitions')}"


def acknowledge(workstation, args, object_reference, event_state, time_stamp, source="Operator Olive"):
    """AcknowledgeAlarm, the time of acknowledgment being now (host clock)."""
    now = time.localtime()
    time_of_acknowledgment = (bytes([now.tm_year - 1900, now.tm_mon, now.tm_mday, now.tm_wday + 1]),
                              bytes([now.tm_hour, now.tm_min, now.tm_sec, 0]))
    return extras.request_with_retry(workstation, lambda invoke_id: alarm_codec.acknowledge_alarm_request(
        invoke_id, OPERATOR_PROCESS_IDENTIFIER, *object_reference, event_state, time_stamp, source,
        time_of_acknowledgment), args.port)


def notification_from(object_reference, notify_type, to_state=None):
    def matches(notification):
        return (notification.event_object == object_reference and notification.notify_type == notify_type and
                (to_state is None or notification.to_state == to_state))
    return matches


def waited(notification, seconds=None):
    return notification.describe() if notification else f"nothing in {seconds or NOTIFICATION_WAIT} s"


__all__ = ["Workstation", "alarm_codec", "extras", "wiretest"]
