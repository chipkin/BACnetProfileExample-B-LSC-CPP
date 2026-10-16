# SPDX-License-Identifier: CC0-1.0
# Public-domain example code (CC0) - see ../LICENSE.
"""Generic Wire Test helpers that wiretest/ 0.6.0 does not have yet (proposed for 0.7.0).

Nothing here names B-LSC: each piece is a BACnet encoding or a harness tool any example
could use. Kept apart from the profile's tests so it can be folded into wiretest/ as is.

    life_safety_operation_request   LifeSafetyOperation-Request (cl. 21, cl. 13.13)
    announce                        send an I-Am for a Workstation, unicast, so a device that
                                    names it by DEVICE instance (a Recipient_List entry) can
                                    bind its address without hearing a broadcast
    SimulationControl               the client side of an example's loopback-only test control
                                    channel (one plain-text command per datagram, one reply)
    request_with_retry              Workstation.request() with wiretest.Client's opt-in, visible
                                    [RETRY] when nothing at all comes back
"""
import socket

from wiretest import codec

# BACnetConfirmedServiceChoice life-safety-operation (cl. 21).
SERVICE_LIFE_SAFETY_OPERATION = 27

# BACnetLifeSafetyOperation (cl. 21).
LIFE_SAFETY_OPERATION_NONE = 0
LIFE_SAFETY_OPERATION_SILENCE = 1
LIFE_SAFETY_OPERATION_SILENCE_AUDIBLE = 2
LIFE_SAFETY_OPERATION_SILENCE_VISUAL = 3
LIFE_SAFETY_OPERATION_RESET = 4
LIFE_SAFETY_OPERATION_RESET_ALARM = 5
LIFE_SAFETY_OPERATION_RESET_FAULT = 6
LIFE_SAFETY_OPERATION_UNSILENCE = 7

# BACnetLifeSafetyState (cl. 21), the values the examples use.
LIFE_SAFETY_STATE_QUIET = 0
LIFE_SAFETY_STATE_ALARM = 2
LIFE_SAFETY_STATE_FAULT = 3

# BACnetSilencedState (cl. 21).
SILENCED_UNSILENCED = 0
SILENCED_AUDIBLE = 1
SILENCED_VISIBLE = 2
SILENCED_ALL = 3

# BACnetEventState / BACnetEventType values wiretest.alarm does not name yet.
EVENT_STATE_LIFE_SAFETY_ALARM = 5
EVENT_TYPE_CHANGE_OF_LIFE_SAFETY = 8

# BACnetReliability multi-state-fault (cl. 21) - the FAULT_LIFE_SAFETY algorithm's fault.
RELIABILITY_MULTI_STATE_FAULT = 9

# Error code invalid-operation-in-this-state (cl. 21 BACnetErrorCode).
ERROR_CODE_INVALID_OPERATION_IN_THIS_STATE = 139

# Life-safety property identifiers (cl. 21 BACnetPropertyIdentifier).
PROPERTY_MODE = 160
PROPERTY_OPERATION_EXPECTED = 161
PROPERTY_SILENCED = 163
PROPERTY_TRACKING_VALUE = 164
PROPERTY_ZONE_MEMBERS = 165


def life_safety_operation_request(invoke_id, process_identifier, source, operation, object_reference=None):
    """LifeSafetyOperation-Request (cl. 21): requestingProcessIdentifier [0], requestingSource [1],
    request [2], objectIdentifier [3] OPTIONAL. `object_reference` is (type, instance) or None
    ("all applicable objects", cl. 13.13.1.1.4)."""
    parameters = (codec.context_unsigned(0, process_identifier) + codec.context_character_string(1, source) +
                  codec.context_enumerated(2, operation))
    if object_reference is not None:
        parameters += codec.context_object_id(3, *object_reference)
    return codec.confirmed_request(invoke_id, SERVICE_LIFE_SAFETY_OPERATION, parameters)


def request_with_retry(workstation, build_apdu, port, retries=1, timeout=3.0):
    """workstation.request(), sent again (with a visible [RETRY] line) when NO answer comes back at
    all - a lost UDP datagram, seen about 3 in 1000 on a busy Windows host. An Error or Reject is an
    answer and is never retried. A request whose answer (not the request) was lost is executed
    twice; the second answer then shows that, so a test does not hide it."""
    answer = workstation.request(build_apdu, port, timeout=timeout)
    for attempt in range(retries):
        if answer.answered:
            break
        print(f"      [RETRY] no answer in {timeout} s - sending the request again (attempt {attempt + 2})", flush=True)
        answer = workstation.request(build_apdu, port, timeout=timeout)
    return answer


def announce(workstation, device_instance, port, ip="127.0.0.1", vendor_identifier=389):
    """Send an I-Am for `device_instance` from `workstation`'s socket to one device. A device
    whose Recipient_List names `device_instance` learns the workstation's address from it - the
    way a workstation's start-up I-Am reaches a panel, without relying on broadcasts (which do
    not reach a 127.0.0.1 socket)."""
    workstation.send_unconfirmed(codec.i_am(device_instance, vendor_identifier), port, ip)


class SimulationControl:
    """Client of an example's SIMULATION test control channel (loopback UDP, one plain-text
    command per datagram, one reply), e.g. B-GW-CPP's and B-LSC-CPP's simulation/SimulationControl."""

    def __init__(self, port, ip="127.0.0.1", timeout=2.0, retries=2):
        self.address = (ip, port)
        self.timeout = timeout
        self.retries = retries

    def command(self, text):
        """The reply text, or None if the example never answered (each lost datagram retried once
        more, visibly, up to `retries` times)."""
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.settimeout(self.timeout)
        try:
            for attempt in range(self.retries + 1):
                if attempt:
                    print(f"      [RETRY] simulation control '{text}' (attempt {attempt + 1})", flush=True)
                sock.sendto(text.encode("ascii"), self.address)
                try:
                    reply, _ = sock.recvfrom(256)
                    return reply.decode("ascii", "replace")
                except (socket.timeout, ConnectionResetError):
                    continue
            return None
        finally:
            sock.close()
