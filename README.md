# BACnet B-LSC (Life Safety Controller) - C++ example

A tutorial example showing how to implement as much of the BACnet **B-LSC
(Life Safety Controller)** device profile as the
[CAS BACnet Stack](https://store.chipkin.com/services/stacks/bacnet-stack)
supports today, in C++. It answers **ReadProperty / ReadPropertyMultiple**,
accepts **WriteProperty / WritePropertyMultiple**, accepts **SubscribeCOV**,
generates **intrinsic life-safety alarms** (`CHANGE_OF_LIFE_SAFETY` event
notifications), accepts **AcknowledgeAlarm** and **GetEventInformation**,
synchronises its clock, and handles **DeviceCommunicationControl** and
**ReinitializeDevice**.

**This example is not yet released** - it lives on the `implement-lsc`
branch, blocked on a stack defect (see the callout below). There is no
`releases` page with prebuilt binaries yet; clone this repository and
[build it yourself](#build).

- **[TUTORIAL.md](TUTORIAL.md)** - how to extend this example and how to
  review it for conformance. Read it when you start turning this into your
  own device.
- **[docs/PICS.md](docs/PICS.md)** - the Protocol Implementation Conformance
  Statement: every object, every property, and who answers it.

> **Versions:** this document describes **example v1.0.0**, built and
> verified against the **CAS BACnet Stack** compiled from source
> (`submodules/cas-bacnet-stack` @ `abd4cee1`, reporting **6.0.21**), at
> **Protocol_Revision 24**, with the vendored `common/` helper at **v2.5.0**.
> Running the example prints all three - if what it prints disagrees with
> this line, trust the program and check `CHANGELOG.md`.

> **⚠ CRITICAL, VERIFIED LIMITATION: Life Safety Point/Zone objects (Amber,
> Azure) currently cannot serve almost any property over the wire.** Confirmed
> by running the built binary against a live BACnet client: `Object_List`
> correctly lists both objects and `Object_Identifier`/`Object_Type` read
> back, but `Object_Name`, `Present_Value`, `Out_Of_Service`, and every other
> property answer `Error(...): object: unknown-object` - and the same
> WriteProperty of `Present_Value` this README uses as the alarm/fault demo
> trigger fails the same way. **Root cause is in the stack, not this
> example's code:** `BACnetStack_AddObject` (the only customer-facing way to
> create one of these two object types) never populates the stack's internal
> life-safety engine object, so the stack's own generated-property dispatch
> answers `unknown-object` before this example's callbacks are ever reached.
> See **[TODO.md #0](TODO.md)** for the full trace (it names the exact source
> lines) and the filed stack issue
> ([chipkin/cas-bacnet-stack#2036](https://github.com/chipkin/cas-bacnet-stack/issues/2036)).
> This example implements every B-LSC capability the standard CAS BACnet
> Stack's customer-facing API exposes, and this is the dominant thing it
> cannot do today - full detail in **[TODO.md](TODO.md)**,
> **[TUTORIAL.md](TUTORIAL.md)**, and **[docs/PICS.md](docs/PICS.md)**.

This example is seeded from
[B-AAC (Advanced Application Controller)](https://github.com/chipkin/BACnetProfileExample-B-AAC-CPP)
**minus** its Schedule/Calendar objects (not part of the B-LSC profile) and
adds the two life-safety object types plus a LifeSafetyOperation responder.

## The device this example creates

```
Device 389007  "Rainbow"   (Vendor 389 - Chipkin Automation Systems)
    ├── Analog Input  1       "Bronze"      read-only sensor (REAL, deg C); COV-subscribable
    ├── Binary Input  1       "Emerald"     read-only sensor (active/inactive)
    ├── Multi-State Input 1   "Hot Pink"    read-only sensor (state 1..3)
    ├── Analog Output 1       "Chartreuse"  writable, commandable (REAL)
    ├── Binary Output 1       "Fuchsia"     writable, commandable (0/1)
    ├── Multi-State Output 1  "Indigo"      writable, commandable (state 1..3)
    ├── Life Safety Point 1   "Amber"       ⚠ configured but not yet functional (see above)
    ├── Life Safety Zone 1    "Azure"       ⚠ configured but not yet functional (see above)
    ├── Notification Class 1  "Crimson"     routes Amber's/Azure's alarms to recipients
    └── Network Port 1        "Vermilion"   the BACnet/IP port (required)
```

## What this example supports

### BIBBs (BACnet Interoperability Building Blocks)

These are what the B-LSC profile requires. All are implemented; the ⚠ column
flags the one whose objects are configured but not currently functional over
the wire because of the stack defect above.

| BIBB | Description | Supported |
|------|-------------|:---------:|
| DS-RP-B | Data Sharing - ReadProperty - B | ✅ |
| DS-RPM-B | Data Sharing - ReadPropertyMultiple - B | ✅ |
| DS-WP-B | Data Sharing - WriteProperty - B | ✅ |
| DS-WPM-B | Data Sharing - WritePropertyMultiple - B | ✅ |
| DS-COV-B | Data Sharing - COV - B | ✅ |
| AE-LS-B | Alarm and Event - Life Safety - B | ✅ implemented; ⚠ Amber/Azure not functional over the wire yet - see above |
| AE-ACK-B | Alarm and Event - ACK - B | ✅ |
| AE-INFO-B | Alarm and Event - Information - B | ✅ |
| DM-DDB-A | Device Management - Dynamic Device Binding - A (initiate) | ✅ |
| DM-DDB-B | Device Management - Dynamic Device Binding - B (answer) | ✅ |
| DM-DOB-B | Device Management - Dynamic Object Binding - B | ✅ |
| DM-DCC-B | Device Management - Device Communication Control - B | ✅ |
| DM-TS-B | Device Management - Time Synchronization - B | ✅ |
| DM-UTC-B | Device Management - UTC Time Synchronization - B | ✅ |
| DM-RD-B | Device Management - Reinitialize Device - B | ✅ (cold/warm start) |

### Services (executed / B-side)

| Service | Notes |
|---------|-------|
| ReadProperty / ReadPropertyMultiple | Responds to property reads (DS-RP-B / DS-RPM-B). |
| WriteProperty / WritePropertyMultiple | Accepts writes to the commandable outputs and (in code; see the ⚠ above) Amber/Azure (DS-WP-B / DS-WPM-B). |
| SubscribeCOV | Bronze's and Amber's `Present_Value` are subscribable (DS-COV-B). |
| ConfirmedEventNotification / UnconfirmedEventNotification | Sent when Amber/Azure transition, once the stack defect above is resolved (AE-LS-B). |
| AcknowledgeAlarm | Accepts an operator acknowledgement (AE-ACK-B). |
| GetEventInformation | Reports active events (AE-INFO-B). |
| Who-Is / I-Am | Answers Who-Is with I-Am, and broadcasts an I-Am on start-up (DM-DDB-B); also broadcasts a Who-Is on start-up (DM-DDB-A). |
| Who-Has / I-Have | Answers Who-Has with I-Have (DM-DOB-B). |
| DeviceCommunicationControl | Enable / disable-initiation, password-gated (DM-DCC-B). |
| ReinitializeDevice | Cold/warm start; SimpleACKs then actually restarts (DM-RD-B). |
| TimeSynchronization / UTCTimeSynchronization | Accepted (DM-TS-B / DM-UTC-B). |
| LifeSafetyOperation | Implemented in `main.cpp` and registered, but **not enabled** on the linked stack build - see [TODO.md #2](TODO.md). Not itself a BIBB this profile requires. |

### Object types

| Object type | Instance | Name |
|-------------|:--------:|------|
| Device | 389007 | Rainbow |
| Analog Input | 1 | Bronze |
| Binary Input | 1 | Emerald |
| Multi-State Input | 1 | Hot Pink |
| Analog Output | 1 | Chartreuse |
| Binary Output | 1 | Fuchsia |
| Multi-State Output | 1 | Indigo |
| Life Safety Point ⚠ | 1 | Amber |
| Life Safety Zone ⚠ | 1 | Azure |
| Notification Class | 1 | Crimson |
| Network Port | 1 | Vermilion |

Every required property of every object, and who answers it, is in
[docs/PICS.md](docs/PICS.md).

## Requires the CAS BACnet Stack (licensed product)

This example **builds against the CAS BACnet Stack, which is a commercial
Chipkin product** - it is not free or open source, and there is no
public/trial build. The stack is referenced here as the **private** git
submodule `submodules/cas-bacnet-stack`; you can only fetch and build it once
you have a CAS BACnet Stack license and access to that repository.

**To get the CAS BACnet Stack (and access to build this example), contact
Chipkin:** <https://store.chipkin.com/services/stacks/bacnet-stack> or
sales@chipkin.com.

You do not need a stack licence to *read* this example's own source: every
file outside `submodules/` is CC0 public domain (see [LICENSE](LICENSE)).
The licence is what lets you *build* it.

## What's in this repository

This is a **self-contained** project. It ships:

- `main.cpp` - the example device.
- `common/` - the shared helper (UDP, callbacks, CLI, keyboard) vendored in.
- `CMakeLists.txt` - the build, the same on Windows, Linux, and macOS.
- `docs/PICS.md` - the conformance statement.
- `TODO.md` - the engineering detail behind the stack-defect callouts above.
- `submodules/cas-bacnet-stack/` - the **CAS BACnet Stack as a git submodule**
  (private; requires a license - see above). Its sources are compiled into
  the executable, so there is no library or DLL to build, ship, or install.

## Prerequisites

- A C++17 compiler (MSVC, GCC, or Clang).
- CMake >= 3.15.
- Git (to fetch the stack submodule).

### Windows

- **C++ compiler** - install
  [Visual Studio Community](https://visualstudio.microsoft.com/downloads/)
  (free) and select the **"Desktop development with C++"** workload.
- **CMake** - from <https://cmake.org/download/>, or `winget install Kitware.CMake`.

### Linux / macOS

- Debian/Ubuntu: `sudo apt install build-essential cmake git`
- macOS: `xcode-select --install` and `brew install cmake`

## Build

CMake only, and the same two commands on every platform:

```bash
git clone --recursive https://github.com/chipkin/BACnetProfileExample-B-LSC-CPP.git
cd BACnetProfileExample-B-LSC-CPP

cmake -B build -S .
cmake --build build --config Release
```

Already cloned without `--recursive`? Run `git submodule update --init --recursive`
first - the build needs the stack submodule.

> **The first build takes a few minutes** - it compiles the entire CAS
> BACnet Stack (~600 source files) into the executable. Rebuilds after that
> are incremental and take seconds.

If your CAS BACnet Stack lives somewhere other than the bundled submodule,
point CMake at it: `cmake -B build -S . -D CAS_STACK_DIR=/path/to/cas-bacnet-stack`.

This uses the adapter's default **SOURCE** mode: the stack's `source/*.cpp`
is compiled straight into the executable, so there is no library or DLL to
build, ship, or install first, and the build is identical on every platform.

## Run

```bash
# Linux / macOS
./build/BACnetExampleBLSC

# Windows
.\build\Release\BACnetExampleBLSC.exe
```

Expected output (captured from a real build of this documented command):

```
BACnet B-LSC (Life Safety Controller) Example - C++ v1.0.0
CAS BACnet Stack version: 6.0.21.0
Common helper (common/) version: 2.5.0
FYI: Listening for BACnet/IP on UDP port 47808 (Network Port 1).
::CASBACnetStack::BACnetInterface::RegisterCallbackLifeSafetyOperation() ... - FYI: This feature was not compiled. To enable, re-compile the CAS BACnet Stack with this defined: STACK_OPTION_DM_LSO_LIFE_SAFETY_OPERATION
TX 21 bytes to 192.168.3.255:47808 (broadcast) (Network Port 1)
TX 8 bytes to 192.168.3.255:47808 (broadcast) (Network Port 1)
FYI: Device 389007 ("Rainbow") ready. Vendor ID 389. Press 'h' for help.
```

The first `TX` line is the start-up I-Am the device broadcasts to announce
itself; the second is the start-up Who-Is (DM-DDB-A). Both go to the **local
subnet broadcast** address (computed from the Network Port's interface), not
the global `255.255.255.255`. As clients talk to the device you'll see
`RX ... bytes from ...` and `TX ... bytes to ...` lines showing the traffic.
The `RegisterCallbackLifeSafetyOperation` line is expected - see the ⚠
callout above and [TODO.md #2](TODO.md).

The device listens on UDP **47808** (BACnet/IP). Allow that port through your
firewall. To use a different port, pass `--port` (see below).

> **A wall of red `Error:` lines at start-up is expected and is not your
> bug** - it is the stack's own debug logging (the device hearing its own
> broadcast I-Am, and a one-time BACnet/SC UUID notice).
> [TUTORIAL.md](TUTORIAL.md#troubleshooting) explains both.

### Command-line options

| Option | Default | Meaning |
|--------|---------|---------|
| `--port <n>` | `47808` | UDP port to listen on (BACnet/IP). |
| `--deviceID <n>` | `389007` | The device's BACnet instance number (BACnet requires this to be configurable). |
| `--help`, `-h` | - | Show usage and exit. |
| `--version` | - | Print the example, stack, and `common/` helper versions, then exit. |

### Interactive commands

While the example runs, these keys are available:

| Key | Action |
|-----|--------|
| `h` | Show the version information and this command list. |
| `q` | Quit. |
| up arrow | Increase Analog Input 1 (`Bronze`) by 1.1 (also feeds its COV subscribers). |
| down arrow | Decrease Analog Input 1 (`Bronze`) by 1.1 (also feeds its COV subscribers). |

To fire a life-safety alarm (once the stack defect above is resolved),
WriteProperty Amber's or Azure's `Present_Value` to `2` (alarm) or `3`
(fault); write `0` to clear either.

## Verify

Use a BACnet client such as the
[**CAS BACnet Explorer**](https://store.chipkin.com/products/tools/cas-bacnet-explorer)
(or `bacpypes3`/`BAC0`):

1. **Discover** - send a **Who-Is**. The device replies with **I-Am** from
   instance **389007** (vendor **389**). It also broadcasts an I-Am and a
   Who-Is at start-up.
2. **Browse the object model** - the device shows ten objects, including
   Life Safety Point "Amber" and Life Safety Zone "Azure". Reading the
   Device's `Object_List` returns all ten; `Protocol_Revision` returns `24`.
   `Object_Identifier`/`Object_Type` read back correctly on every object
   including Amber/Azure - everything else on those two currently does not,
   see the callout above.
3. **Read the inputs and outputs** - ReadProperty every required property of
   Bronze, Emerald, Hot Pink, Chartreuse, Fuchsia and Indigo and confirm they
   answer. WriteProperty a commandable output's `Present_Value` at a
   priority, re-read it and its `Priority_Array`, then write `NULL` to
   relinquish and confirm it falls back to `Relinquish_Default`.
4. **Life-safety alarming** - **currently blocked**, see the ⚠ callout at the
   top of this document. Once resolved: WriteProperty Amber's `Present_Value`
   to `2` (alarm), confirm `Event_State` becomes `life-safety-alarm` and a
   `CHANGE_OF_LIFE_SAFETY` `EventNotification` arrives; write `0` to return
   to normal.
5. **Device management** - DeviceCommunicationControl
   `disable-initiation`/`enable`; ReinitializeDevice `COLDSTART` (confirm a
   SimpleACK, then that the device actually restarts and re-announces with
   an I-Am); TimeSynchronization / UTCTimeSynchronization are accepted.

For a property-by-property review against the conformance statement, and for
what is and is not exercised end-to-end, see [TUTORIAL.md](TUTORIAL.md) and
[docs/PICS.md](docs/PICS.md).

## Footprint

This example has not been released yet, so there are no published binary
size / start-up timing numbers. Footprint numbers will be filled in at the
first tagged release, built with the SOURCE-mode build documented above.

## The BACnet profile example series

<!-- PROFILE-TABLE:BEGIN (generated from cas-bacnet-stack-examples/docs/profile-table.md - do not edit here) -->
The CAS BACnet Stack supports every standardized device profile in ASHRAE 135-2024 Annex L, and there is one example repository per profile. Pick the profile your device claims, then the language you build in.

### Controllers (Annex L.4)

| Profile | C++ | Node.js |
|---|---|---|
| **B-SS** Smart Sensor | [B-SS-CPP](https://github.com/chipkin/BACnetProfileExample-B-SS-CPP) | — |
| **B-SA** Smart Actuator | [B-SA-CPP](https://github.com/chipkin/BACnetProfileExample-B-SA-CPP) | — |
| **B-ASC** Application Specific Controller | [B-ASC-CPP](https://github.com/chipkin/BACnetProfileExample-B-ASC-CPP) | [B-ASC-Node](https://github.com/chipkin/BACnetProfileExample-B-ASC-Node) |
| **B-AAC** Advanced Application Controller | [B-AAC-CPP](https://github.com/chipkin/BACnetProfileExample-B-AAC-CPP) | — |
| **B-BC** Building Controller | [B-BC-CPP](https://github.com/chipkin/BACnetProfileExample-B-BC-CPP) | — |

### Life safety controllers (Annex L.5)

| Profile | C++ | Node.js |
|---|---|---|
| **B-LSC** Life Safety Controller | [B-LSC-CPP](https://github.com/chipkin/BACnetProfileExample-B-LSC-CPP) 🚧 | — |
| **B-ALSC** Advanced Life Safety Controller | [B-ALSC-CPP](https://github.com/chipkin/BACnetProfileExample-B-ALSC-CPP) | — |

### Access control controllers (Annex L.6)

| Profile | C++ | Node.js |
|---|---|---|
| **B-ACC** Access Control Controller | [B-ACC-CPP](https://github.com/chipkin/BACnetProfileExample-B-ACC-CPP) | — |
| **B-AACC** Advanced Access Control Controller | [B-AACC-CPP](https://github.com/chipkin/BACnetProfileExample-B-AACC-CPP) | — |

### Lighting controllers (Annex L.11)

| Profile | C++ | Node.js |
|---|---|---|
| **B-LD** Lighting Device | [B-LD-CPP](https://github.com/chipkin/BACnetProfileExample-B-LD-CPP) | — |
| **B-LS** Lighting Supervisor | [B-LS-CPP](https://github.com/chipkin/BACnetProfileExample-B-LS-CPP) | — |

### Elevator controllers (Annex L.13)

| Profile | C++ | Node.js |
|---|---|---|
| **B-EM** Elevator Monitor | [B-EM-CPP](https://github.com/chipkin/BACnetProfileExample-B-EM-CPP) | — |
| **B-EC** Elevator Controller | [B-EC-CPP](https://github.com/chipkin/BACnetProfileExample-B-EC-CPP) | — |
| **B-AEC** Advanced Elevator Controller | [B-AEC-CPP](https://github.com/chipkin/BACnetProfileExample-B-AEC-CPP) | — |

### Authentication and authorization (Annex L.14)

| Profile | C++ | Node.js |
|---|---|---|
| **B-AS** Authorization Server | [B-AS-CPP](https://github.com/chipkin/BACnetProfileExample-B-AS-CPP) | — |

### Miscellaneous (Annex L.7, combinable with any one family)

| Profile | C++ | Node.js |
|---|---|---|
| **B-BBMD** Broadcast Management Device | [B-BBMD-CPP](https://github.com/chipkin/BACnetProfileExample-B-BBMD-CPP) | — |
| **B-ACDC** Access Control Door Controller | [B-ACDC-CPP](https://github.com/chipkin/BACnetProfileExample-B-ACDC-CPP) | — |
| **B-ACCR** Access Control Credential Reader | [B-ACCR-CPP](https://github.com/chipkin/BACnetProfileExample-B-ACCR-CPP) | — |
| **B-RTR** Router | [B-RTR-CPP](https://github.com/chipkin/BACnetProfileExample-B-RTR-CPP) | — |
| **B-GW** Gateway | [B-GW-CPP](https://github.com/chipkin/BACnetProfileExample-B-GW-CPP) | — |
| **B-DAP** Device Address Proxy | [B-DAP-CPP](https://github.com/chipkin/BACnetProfileExample-B-DAP-CPP) | — |
| **B-SCHUB** BACnet/SC Hub | [B-SCHUB-CPP](https://github.com/chipkin/BACnetProfileExample-B-SCHUB-CPP) | — |
| **B-GENERAL** General device (Annex L.8) | *(satisfied by every example above)* | — |

### Operator interfaces and workstations (Annex L.1–L.3, L.9–L.10, L.12)

Client-side profiles.

| Profile | C++ | Node.js |
|---|---|---|
| **B-OD** Operator Display | [B-OD-CPP](https://github.com/chipkin/BACnetProfileExample-B-OD-CPP) | — |
| **B-OWS** Operator Workstation | planned | — |
| **B-AWS** Advanced Operator Workstation | planned | — |
| **B-XAWS** Extended Advanced Operator Workstation | planned | — |
| **B-LSAP** Life Safety Annunciator Panel | planned | — |
| **B-LSWS** Life Safety Workstation | planned | — |
| **B-ALSWS** Advanced Life Safety Workstation | planned | — |
| **B-ACSD** Access Control Security Display | planned | — |
| **B-ACWS** Access Control Workstation | planned | — |
| **B-AACWS** Advanced Access Control Workstation | planned | — |
| **B-LOD** Lighting Operator Display | planned | — |
| **B-ALWS** Advanced Lighting Workstation | planned | — |
| **B-LCS** Lighting Control Station | planned | — |
| **B-ALCS** Advanced Lighting Control Station | planned | — |
| **B-ED** Elevator Display | planned | — |
| **B-EWS** Elevator Workstation | planned | — |
| **B-AEWS** Advanced Elevator Workstation | planned | — |

🚧 = in progress. Profile definitions: ANSI/ASHRAE 135-2024 Annex L. BIBB definitions: Annex K. Get the stack: <https://store.chipkin.com/services/stacks/bacnet-stack>.
<!-- PROFILE-TABLE:END -->

## References

- **ANSI/ASHRAE Standard 135** (BACnet) - the protocol standard. Object
  model (Clause 12), alarm and event services (Clause 13), services (Clause
  16), BACnet/IP (Annex J), device profiles (Annex L). Purchase / preview via
  the [ASHRAE store](https://www.ashrae.org/technical-resources/standards-and-guidelines).
- **What is BACnet?** - Chipkin's introduction:
  <https://docs.chipkin.com/protocols/bacnet/>.
- **CAS BACnet Stack** - product page and documentation:
  <https://store.chipkin.com/services/stacks/bacnet-stack>.
- **CAS BACnet Explorer** - client for testing this device:
  <https://store.chipkin.com/products/tools/cas-bacnet-explorer>.
- **B-AAC example** (the sibling this builds on) -
  <https://github.com/chipkin/BACnetProfileExample-B-AAC-CPP>.
- **Shared helper used by this example** - [`common/README.md`](common/README.md).

See also [TUTORIAL.md](TUTORIAL.md), [docs/PICS.md](docs/PICS.md),
[TODO.md](TODO.md), [CHANGELOG.md](CHANGELOG.md), and [AGENTS.md](AGENTS.md).
