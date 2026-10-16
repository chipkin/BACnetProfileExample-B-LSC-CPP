# BACnet B-LSC (Life Safety Controller) - C++ example

A tutorial example of the BACnet **B-LSC (Life Safety Controller)** device
profile in C++, using the
[CAS BACnet Stack](https://store.chipkin.com/services/stacks/bacnet-stack).

**What it does** - the profile's function, 135-2024 Annex L.5.2: "A B-LSC
device performs life safety alarm detection and control. It supports life
safety objects." A smoke detector goes into alarm; the controller latches the
alarm, tells the operator workstation, and lets the operator silence it,
acknowledge it and - once the smoke has gone - reset it:

1. **Smoke.** The detector (Life Safety Point 1 "Amber") sees smoke:
   `Tracking_Value` goes to `alarm`, `Present_Value` **latches** `alarm`, and
   its zone (Life Safety Zone 1 "Azure") follows.
2. **The alarm goes to a recipient.** The stack's `CHANGE_OF_LIFE_SAFETY`
   algorithm puts both objects in the `LIFE_SAFETY_ALARM` event state and
   sends a **ConfirmedEventNotification** to the operator workstation, which
   Notification Class 1 "Crimson" names by **device instance** (389107) - the
   stack finds its address from the workstation's I-Am.
3. **The operator handles it.** `LifeSafetyOperation` **SILENCE** silences the
   sounders (`Silenced`, `Operation_Expected` moves to `RESET`);
   **AcknowledgeAlarm** acknowledges it and every recipient gets an
   `ACK_NOTIFICATION`; when the smoke has cleared `Tracking_Value` returns to
   `quiet` but the alarm stays latched until **RESET**, and then the return to
   normal is notified too.

**Failure Modes** the Wire Tests show:

- **RESET while the detector still sees smoke is refused** - `OBJECT` /
  `INVALID_OPERATION_IN_THIS_STATE` (cl. 13.13.1.3.1, 13.13.2) - and the
  alarm stays latched. So is a RESET of an object that is not latched.
- **A detector fault** (its head pulled off its base) gives `Reliability`
  `MULTI_STATE_FAULT`, the `FAULT` status flag and the `FAULT` event state,
  notified to the workstation (the `FAULT_LIFE_SAFETY` algorithm, cl. 13.4).
  An operator cannot `RESET_FAULT` it away; repairing the detector clears it.
- **An alarm nobody acknowledges stays listed** by GetEventInformation after
  it has returned to normal.

> **The smoke detector is Simulation.** There is no detector loop card here:
> what the detector reads (`quiet`, `smoke`, `fault`) is a variable in
> `main.cpp` (section 1b, "SIMULATION: the smoke detector"). Only the Wire
> Tests move it, through a **loopback-only test control channel**
> ([`simulation/SimulationControl.h`](simulation/SimulationControl.h)) that is
> off unless the environment variable `BLSC_SIMULATION_CONTROL_PORT` names a
> UDP port - it is not a command-line option, and nothing on the BACnet
> network can reach it. A real product deletes `simulation/` and feeds
> `ApplyDetectorReading()` from its detector inputs. Everything a BACnet
> client sees - the objects, the latching, the alarms, the operations - is
> real BACnet behaviour of this example and the CAS BACnet Stack.

**This example is not yet released.** There is no `releases` page with
prebuilt binaries yet; clone this repository and [build it yourself](#build).

- **[TUTORIAL.md](TUTORIAL.md)** - how to extend this example and how to
  review it for conformance. Read it when you start turning this into your
  own device.
- **[docs/PICS.md](docs/PICS.md)** - the Protocol Implementation Conformance
  Statement: every object, every property, and who answers it.
- **[tests/](tests/README.md)** - the Wire Tests that prove the scenario and
  the Failure Modes, from outside the example, over BACnet/IP.

> **Versions:** this document describes **example v1.2.0**, built and
> verified against the **CAS BACnet Stack** (`submodules/cas-bacnet-stack` @
> `64b2cfac5`, reporting **6.0.23**), at **Protocol_Revision 30**, with the
> vendored `common/` helper at **v3.4.0** and `wiretest/` at **v0.7.0**.
> Running the example prints the first three - if what it prints disagrees
> with this line, trust the program and check `CHANGELOG.md`.

> **Known gaps at stack `6.x` @ `64b2cfac5`** (details in [TODO.md](TODO.md)):
>
> - **Zone_Members is always empty.** Azure rolls up Amber, but the customer
>   interface has no way to list Amber in `Zone_Members`
>   ([chipkin/cas-bacnet-stack#2036](https://github.com/chipkin/cas-bacnet-stack/issues/2036),
>   open). The Wire Tests report it as a `[KNOWN GAP]`.
> - **The stack's own life-safety engine is not exported** (#2036 again), so
>   this example holds Amber's and Azure's state itself and `Mode` is accepted
>   without an `Accepted_Modes` check.

This example is seeded from
[B-AAC (Advanced Application Controller)](https://github.com/chipkin/BACnetProfileExample-B-AAC-CPP)
**minus** its Schedule/Calendar objects (not part of the B-LSC profile) and
adds the two life-safety object types and the life-safety operations.

## The device this example creates

```
Device 389007  "Chipkin Example B-LSC"   (Vendor 389 - Chipkin Automation Systems)
    ├── Analog Input  1       "Bronze"      read-only sensor (REAL, deg C); COV-subscribable
    ├── Binary Input  1       "Emerald"     read-only sensor (active/inactive)
    ├── Multi-State Input 1   "Hot Pink"    read-only sensor (state 1..3)
    ├── Analog Output 1       "Chartreuse"  writable, commandable (REAL)
    ├── Binary Output 1       "Fuchsia"     writable, commandable (0/1)
    ├── Multi-State Output 1  "Indigo"      writable, commandable (state 1..3)
    ├── Life Safety Point 1   "Amber"       the smoke detector (SIMULATION input); latches alarm; COV-subscribable
    ├── Life Safety Zone 1    "Azure"       the detector's zone; rolls Amber up (Zone_Members empty - see above)
    ├── Notification Class 1  "Crimson"     sends Amber's/Azure's alarms, confirmed, to Device 389107
    └── Network Port 1        "Vermilion"   the BACnet/IP port (required)
```

## What this example supports

### BIBBs (BACnet Interoperability Building Blocks)

The B-LSC column of the Annex L.5 table is DS-RP-B, DS-WP-B, DS-COV-B, AE-LS-B,
AE-ACK-B, AE-INFO-B, DM-DDB-A,B, DM-DOB-B and DM-DCC-B. The rows marked
"beyond the profile" are kept from the B-AAC base this example was seeded from.

| BIBB | Description | Supported |
|------|-------------|:---------:|
| DS-RP-B | Data Sharing - ReadProperty - B | ✅ |
| DS-RPM-B | Data Sharing - ReadPropertyMultiple - B | ✅ (beyond the profile) |
| DS-WP-B | Data Sharing - WriteProperty - B | ✅ |
| DS-WPM-B | Data Sharing - WritePropertyMultiple - B | ✅ (beyond the profile) |
| DS-COV-B | Data Sharing - COV - B | ✅ |
| AE-LS-B | Alarm and Event - Life Safety - B | ✅ `CHANGE_OF_LIFE_SAFETY` notifications and LifeSafetyOperation (silence / unsilence / reset) - Wire Tested |
| AE-ACK-B | Alarm and Event - ACK - B | ✅ Wire Tested |
| AE-INFO-B | Alarm and Event - Information - B | ✅ Wire Tested |
| DM-DDB-A | Device Management - Dynamic Device Binding - A (initiate) | ✅ |
| DM-DDB-B | Device Management - Dynamic Device Binding - B (answer) | ✅ |
| DM-DOB-B | Device Management - Dynamic Object Binding - B | ✅ |
| DM-DCC-B | Device Management - Device Communication Control - B | ✅ |
| DM-TS-B | Device Management - Time Synchronization - B | ✅ (beyond the profile) |
| DM-UTC-B | Device Management - UTC Time Synchronization - B | ✅ (beyond the profile) |
| DM-RD-B | Device Management - Reinitialize Device - B | ✅ (cold/warm start; beyond the profile) |

### Services (executed / B-side)

| Service | Notes |
|---------|-------|
| ReadProperty / ReadPropertyMultiple | Responds to property reads (DS-RP-B / DS-RPM-B). |
| WriteProperty / WritePropertyMultiple | Accepts writes to the commandable outputs and to Amber's/Azure's `Mode` (DS-WP-B / DS-WPM-B). `Present_Value` of Amber/Azure is read-only (Table 12-18): `write-access-denied`. |
| SubscribeCOV | Bronze's and Amber's `Present_Value` are subscribable (DS-COV-B). |
| ConfirmedEventNotification | Sent to Device 389107 when Amber/Azure change event state (AE-LS-B), retried up to `Number_Of_APDU_Retries` (3) times. |
| LifeSafetyOperation | Silence / unsilence (audible, visual, both) and reset (AE-LS-B). Compiled in by `CMakeLists.txt` - see [Build](#build). |
| AcknowledgeAlarm | Accepts an operator acknowledgement; the stack sends the `ACK_NOTIFICATION` (AE-ACK-B). |
| GetEventInformation | Lists active and unacknowledged events (AE-INFO-B). |
| Who-Is / I-Am | Answers Who-Is with I-Am, and broadcasts an I-Am on start-up (DM-DDB-B); also broadcasts a Who-Is on start-up (DM-DDB-A). |
| Who-Has / I-Have | Answers Who-Has with I-Have (DM-DOB-B). |
| DeviceCommunicationControl | Enable / disable-initiation, password-gated (DM-DCC-B). |
| ReinitializeDevice | Cold/warm start; SimpleACKs then actually restarts (DM-RD-B). |
| TimeSynchronization / UTCTimeSynchronization | Accepted (DM-TS-B / DM-UTC-B). |
### Object types

| Object type | Instance | Name |
|-------------|:--------:|------|
| Device | 389007 | Chipkin Example B-LSC |
| Analog Input | 1 | Bronze |
| Binary Input | 1 | Emerald |
| Multi-State Input | 1 | Hot Pink |
| Analog Output | 1 | Chartreuse |
| Binary Output | 1 | Fuchsia |
| Multi-State Output | 1 | Indigo |
| Life Safety Point | 1 | Amber |
| Life Safety Zone | 1 | Azure |
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

- `main.cpp` - the example device. Section 1b is the smoke detector
  **Simulation**.
- `simulation/` - the Wire Tests' loopback-only test control channel to the
  simulated detector (**Simulation**, off unless `BLSC_SIMULATION_CONTROL_PORT`
  is set; a real product deletes it).
- `common/` - the shared helper (UDP, callbacks, CLI, keyboard) vendored in.
- `CMakeLists.txt` - the build, the same on Windows, Linux, and macOS. It also
  turns on the stack's LifeSafetyOperation service (see [Build](#build)).
- `docs/PICS.md` - the conformance statement.
- `TODO.md` - the known gaps, with their evidence.
- `tests/` - the Wire Tests (see [Wire Tests](#wire-tests)), and `wiretest/`, the
  series' shared Python harness they are written against (vendored, like `common/`).
- `submodules/cas-bacnet-stack/` - the **CAS BACnet Stack as a git submodule**
  (private; requires a license - see above). Its sources are compiled into
  the executable, so there is no library or DLL to build, ship, or install.

## Prerequisites

- A C++17 compiler (MSVC, GCC, or Clang).
- CMake >= 3.15.
- Git (to fetch the stack submodule).
- Python 3 (standard library only) to run the Wire Tests.

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

**LifeSafetyOperation needs a stack build option.** AE-LS-B executes
LifeSafetyOperation, and the stack compiles that service only when
`STACK_OPTION_DM_LSO_LIFE_SAFETY_OPERATION` is defined - its documented public
toggle (`CASBACnetStackOptions.h`); the default `STACK_OPTION_TARGET_FULL`
preset leaves it off. In SOURCE mode `CMakeLists.txt` defines it for you. If
you link a prebuilt stack instead (`-D CAS_BACNET_STACK_LINK=STATIC` or `DLL`),
build that library with the option, e.g. on Windows from the examples-series
folder:

```bash
CL=-DSTACK_OPTION_DM_LSO_LIFE_SAFETY_OPERATION tools/build-stack-static.sh BACnetProfileExample-B-LSC-CPP
```

(`-D`, not `/D`: Git Bash rewrites `/D...` into a path. If the library was
already built, rebuild it from clean - msbuild does not notice a changed `CL`.)
A stack built without it makes the example stop at start-up with
`Error: Failed to enable the LifeSafetyOperation (AE-LS-B) ...` rather than
advertise a service it cannot execute.

## Run

```bash
# Linux / macOS
./build/BACnetExampleBLSC

# Windows
.\build\Release\BACnetExampleBLSC.exe
```

Expected output (captured from a real build, here with `--port 48065`):

```
BACnet B-LSC (Life Safety Controller) Example - C++ v1.2.0
CAS BACnet Stack version: 6.0.23.0
Common helper (common/) version: 3.4.0
FYI: Listening for BACnet/IP on UDP port 48065 (Network Port 1).
TX 21 bytes to 192.168.3.255:48065 (broadcast) (Network Port 1) - Unconfirmed: I-Am
TX 8 bytes to 192.168.3.255:48065 (broadcast) (Network Port 1) - Unconfirmed: Who-Is
FYI: Device 389007 ("Chipkin Example B-LSC") ready. Vendor ID 389. Press 'h' for help.
RX 21 bytes from 192.168.3.64:48065 (Network Port 1) - Unconfirmed: I-Am
TX 20 bytes to 192.168.3.255:48065 (broadcast) (Network Port 1) - Unconfirmed: Who-Is DNET=65535
```

The first two `TX` lines are the start-up I-Am the device broadcasts to
announce itself and the start-up Who-Is (DM-DDB-A). Both go to the **local
subnet broadcast** address (computed from the Network Port's interface), not
the global `255.255.255.255`. The `RX ... I-Am` is the device hearing its own
broadcast. The `Who-Is DNET=65535` is the stack looking for **Device 389107**,
the operator workstation Notification Class 1 sends alarms to: until a device
with that instance answers (or announces itself with an I-Am) the alarms have
nowhere to go. As clients talk to the device you'll see `RX ... bytes from ...`
and `TX ... bytes to ...` lines showing the traffic.

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

There is no detector key. The **Simulation**'s smoke detector is moved through
the loopback test control channel, which is off unless you start the example
with `BLSC_SIMULATION_CONTROL_PORT` set (Linux:
`BLSC_SIMULATION_CONTROL_PORT=47809 ./build/BACnetExampleBLSC`). Then send it
one plain-text UDP datagram per command, from the same host:

| Command | Reply | The detector |
|---------|-------|--------------|
| `detector smoke` | `ok` | sees smoke - Amber and Azure go to alarm and latch |
| `detector quiet` | `ok` | the smoke has cleared / the head is back on its base |
| `detector fault` | `ok` | its head is pulled off its base |
| `detector` | `detector <smoke\|quiet\|fault>` | (just reports) |

For example, on Linux: `echo -n "detector smoke" | nc -u -w1 127.0.0.1 47809`.

## Verify

Use a BACnet client such as the
[**CAS BACnet Explorer**](https://store.chipkin.com/products/tools/cas-bacnet-explorer):

1. **Discover** - send a **Who-Is**. The device replies with **I-Am** from
   instance **389007** (vendor **389**). It also broadcasts an I-Am and a
   Who-Is at start-up.
2. **Browse the object model** - the device shows ten objects, including
   Life Safety Point "Amber" and Life Safety Zone "Azure". Reading the
   Device's `Object_List` returns all ten; `Protocol_Revision` returns `30`.
   Every property each object lists in its `Property_List` reads as a value
   (the Wire Tests check this).
3. **Read the inputs and outputs** - ReadProperty every required property of
   Bronze, Emerald, Hot Pink, Chartreuse, Fuchsia and Indigo and confirm they
   answer. WriteProperty a commandable output's `Present_Value` at a
   priority, re-read it and its `Priority_Array`, then write `NULL` to
   relinquish and confirm it falls back to `Relinquish_Default`.
4. **Life-safety alarming** - give your client the device instance **389107**
   (or change `RECIPIENT_DEVICE_INSTANCE` in `main.cpp` to your client's), start
   the example with the test control channel on, and send `detector smoke`.
   Your client receives a `CHANGE_OF_LIFE_SAFETY` ConfirmedEventNotification
   (to `life-safety-alarm`) for Amber and for Azure. Silence it with
   LifeSafetyOperation `silence`, acknowledge it, send `detector quiet`, and
   reset it with LifeSafetyOperation `reset`; try `reset` before the smoke
   clears and see it refused. [tests/life_safety_test.py](tests/life_safety_test.py)
   does exactly this.
5. **Device management** - DeviceCommunicationControl
   `disable-initiation`/`enable`; ReinitializeDevice `COLDSTART` (confirm a
   SimpleACK, then that the device actually restarts and re-announces with
   an I-Am); TimeSynchronization / UTCTimeSynchronization are accepted.

For a property-by-property review against the conformance statement, see
[TUTORIAL.md](TUTORIAL.md) and [docs/PICS.md](docs/PICS.md).

## Wire Tests

Automated, from outside the example: Python 3 (standard library only) starts
the example with its test control channel on, plays the **operator
workstation** (Device 389107, the Peer) over BACnet/IP, and plays the physical
world through the control channel. Each check fails when the function it
proves is removed - every one was mutation-verified (see `CHANGELOG.md`).

| Script | What it proves |
|--------|----------------|
| `discovery_test.py` | Who-Is, `Object_List`, and a ReadProperty of every property every object lists. |
| `life_safety_test.py` | The scenario: smoke -> latched alarm -> ConfirmedEventNotifications to Device 389107 (and not to a stranger) -> GetEventInformation -> RESET refused while the smoke is there -> SILENCE -> AcknowledgeAlarm + ACK_NOTIFICATION -> smoke clears, the latch holds -> RESET -> return to normal. Failure Modes: reset refused (twice), an unacknowledged alarm stays listed. Reports Zone_Members as a `[KNOWN GAP]`. |
| `fault_test.py` | Failure Mode: a detector fault -> Reliability `MULTI_STATE_FAULT`, `FAULT` flag and event state, to-fault notification; `RESET_FAULT` refused; repaired -> return to normal. |

```bash
python3 tests/run_wire_tests.py --exe build/BACnetExampleBLSC          # Linux/macOS
python tests\run_wire_tests.py --exe build\Release\BACnetExampleBLSC.exe   # Windows
```

It ends with `ALL WIRE TESTS PASSED` (exit 0). The example uses `--port` and
`--port` + 1 (the control channel). [tests/README.md](tests/README.md) says
what each check proves.

## The BACnet profile example series

<!-- PROFILE-TABLE:BEGIN (generated from cas-bacnet-stack-examples/docs/profile-table.md - do not edit here) -->
There is one example repository per standardized device profile in ASHRAE 135-2024 Annex L. An example marked ⛔ is blocked: the stack cannot yet perform that profile's function through its customer interface, and the example's README says what it does today. Pick the profile your device claims, then the language you build in. "Ask" means the example hasn't been built yet for that language - [contact Chipkin](https://store.chipkin.com/contact-us) if you need one.

### Controllers (Annex L.4)

| Profile | C++ | Node.js | C# | Rust | Python | Go |
|---|---|---|---|---|---|---|
| **B-SS** Smart Sensor | [B-SS-CPP](https://github.com/chipkin/BACnetProfileExample-B-SS-CPP) | [B-SS-Node](https://github.com/chipkin/BACnetProfileExample-B-SS-Node) | [B-SS-CS](https://github.com/chipkin/BACnetProfileExample-B-SS-CS) | [B-SS-Rust](https://github.com/chipkin/BACnetProfileExample-B-SS-Rust) | [B-SS-Python](https://github.com/chipkin/BACnetProfileExample-B-SS-Python) | [B-SS-Go](https://github.com/chipkin/BACnetProfileExample-B-SS-Go) |
| **B-SA** Smart Actuator | [B-SA-CPP](https://github.com/chipkin/BACnetProfileExample-B-SA-CPP) | Ask | Ask | Ask | Ask | Ask |
| **B-ASC** Application Specific Controller | [B-ASC-CPP](https://github.com/chipkin/BACnetProfileExample-B-ASC-CPP) | [B-ASC-Node](https://github.com/chipkin/BACnetProfileExample-B-ASC-Node) | Ask | Ask | Ask | Ask |
| **B-AAC** Advanced Application Controller | [B-AAC-CPP](https://github.com/chipkin/BACnetProfileExample-B-AAC-CPP) | Ask | Ask | Ask | Ask | Ask |
| **B-BC** Building Controller | [B-BC-CPP](https://github.com/chipkin/BACnetProfileExample-B-BC-CPP) | Ask | Ask | Ask | Ask | Ask |

### Life safety controllers (Annex L.5)

| Profile | C++ | Node.js | C# | Rust | Python | Go |
|---|---|---|---|---|---|---|
| **B-LSC** Life Safety Controller | [B-LSC-CPP](https://github.com/chipkin/BACnetProfileExample-B-LSC-CPP) 🚧 | Ask | Ask | Ask | Ask | Ask |
| **B-ALSC** Advanced Life Safety Controller | [B-ALSC-CPP](https://github.com/chipkin/BACnetProfileExample-B-ALSC-CPP) | Ask | Ask | Ask | Ask | Ask |

### Access control controllers (Annex L.6)

| Profile | C++ | Node.js | C# | Rust | Python | Go |
|---|---|---|---|---|---|---|
| **B-ACC** Access Control Controller | [B-ACC-CPP](https://github.com/chipkin/BACnetProfileExample-B-ACC-CPP) ⛔ blocked on [stack #2150](https://github.com/chipkin/cas-bacnet-stack/issues/2150), [#2044](https://github.com/chipkin/cas-bacnet-stack/issues/2044) | Ask | Ask | Ask | Ask | Ask |
| **B-AACC** Advanced Access Control Controller | [B-AACC-CPP](https://github.com/chipkin/BACnetProfileExample-B-AACC-CPP) ⛔ blocked on [stack #2150](https://github.com/chipkin/cas-bacnet-stack/issues/2150), [#2044](https://github.com/chipkin/cas-bacnet-stack/issues/2044) | Ask | Ask | Ask | Ask | Ask |

### Lighting controllers (Annex L.11)

| Profile | C++ | Node.js | C# | Rust | Python | Go |
|---|---|---|---|---|---|---|
| **B-LD** Lighting Device | [B-LD-CPP](https://github.com/chipkin/BACnetProfileExample-B-LD-CPP) | Ask | Ask | Ask | Ask | Ask |
| **B-LS** Lighting Supervisor | [B-LS-CPP](https://github.com/chipkin/BACnetProfileExample-B-LS-CPP) | Ask | Ask | Ask | Ask | Ask |

### Elevator controllers (Annex L.13)

| Profile | C++ | Node.js | C# | Rust | Python | Go |
|---|---|---|---|---|---|---|
| **B-EM** Elevator Monitor | [B-EM-CPP](https://github.com/chipkin/BACnetProfileExample-B-EM-CPP) | Ask | Ask | Ask | Ask | Ask |
| **B-EC** Elevator Controller | [B-EC-CPP](https://github.com/chipkin/BACnetProfileExample-B-EC-CPP) | Ask | Ask | Ask | Ask | Ask |
| **B-AEC** Advanced Elevator Controller | [B-AEC-CPP](https://github.com/chipkin/BACnetProfileExample-B-AEC-CPP) | Ask | Ask | Ask | Ask | Ask |

### Authentication and authorization (Annex L.14)

| Profile | C++ | Node.js | C# | Rust | Python | Go |
|---|---|---|---|---|---|---|
| **B-AS** Authorization Server | [B-AS-CPP](https://github.com/chipkin/BACnetProfileExample-B-AS-CPP) ⛔ blocked on [stack #2043](https://github.com/chipkin/cas-bacnet-stack/issues/2043) | Ask | Ask | Ask | Ask | Ask |

### Miscellaneous (Annex L.7, combinable with any one family)

| Profile | C++ | Node.js | C# | Rust | Python | Go |
|---|---|---|---|---|---|---|
| **B-BBMD** Broadcast Management Device | [B-BBMD-CPP](https://github.com/chipkin/BACnetProfileExample-B-BBMD-CPP) | Ask | Ask | Ask | Ask | Ask |
| **B-ACDC** Access Control Door Controller | [B-ACDC-CPP](https://github.com/chipkin/BACnetProfileExample-B-ACDC-CPP) | Ask | Ask | Ask | Ask | Ask |
| **B-ACCR** Access Control Credential Reader | [B-ACCR-CPP](https://github.com/chipkin/BACnetProfileExample-B-ACCR-CPP) ⛔ blocked on [stack #3934](https://github.com/chipkin/cas-bacnet-stack/issues/3934) | Ask | Ask | Ask | Ask | Ask |
| **B-RTR** Router | [B-RTR-CPP](https://github.com/chipkin/BACnetProfileExample-B-RTR-CPP) | Ask | Ask | Ask | Ask | Ask |
| **B-GW** Gateway | [B-GW-CPP](https://github.com/chipkin/BACnetProfileExample-B-GW-CPP) | Ask | Ask | Ask | Ask | Ask |
| **B-DAP** Device Address Proxy | [B-DAP-CPP](https://github.com/chipkin/BACnetProfileExample-B-DAP-CPP) ⛔ blocked on [stack #2032](https://github.com/chipkin/cas-bacnet-stack/issues/2032) | Ask | Ask | Ask | Ask | Ask |
| **B-SCHUB** BACnet/SC Hub | [B-SCHUB-CPP](https://github.com/chipkin/BACnetProfileExample-B-SCHUB-CPP) | Ask | Ask | Ask | Ask | Ask |
| **B-GENERAL** General device (Annex L.8) | *(satisfied by every example above)* | — | — | — | — | — |

### Operator interfaces and workstations (Annex L.1–L.3, L.9–L.10, L.12)

Client-side profiles.

| Profile | C++ | Node.js | C# | Rust | Python | Go |
|---|---|---|---|---|---|---|
| **B-OD** Operator Display | [B-OD-CPP](https://github.com/chipkin/BACnetProfileExample-B-OD-CPP) | Ask | Ask | Ask | Ask | Ask |
| **B-OWS** Operator Workstation | planned | — | — | — | — | — |
| **B-AWS** Advanced Operator Workstation | planned | — | — | — | — | — |
| **B-XAWS** Extended Advanced Operator Workstation | planned | — | — | — | — | — |
| **B-LSAP** Life Safety Annunciator Panel | planned | — | — | — | — | — |
| **B-LSWS** Life Safety Workstation | planned | — | — | — | — | — |
| **B-ALSWS** Advanced Life Safety Workstation | planned | — | — | — | — | — |
| **B-ACSD** Access Control Security Display | planned | — | — | — | — | — |
| **B-ACWS** Access Control Workstation | planned | — | — | — | — | — |
| **B-AACWS** Advanced Access Control Workstation | planned | — | — | — | — | — |
| **B-LOD** Lighting Operator Display | planned | — | — | — | — | — |
| **B-ALWS** Advanced Lighting Workstation | planned | — | — | — | — | — |
| **B-LCS** Lighting Control Station | planned | — | — | — | — | — |
| **B-ALCS** Advanced Lighting Control Station | planned | — | — | — | — | — |
| **B-ED** Elevator Display | planned | — | — | — | — | — |
| **B-EWS** Elevator Workstation | planned | — | — | — | — | — |
| **B-AEWS** Advanced Elevator Workstation | planned | — | — | — | — | — |

🚧 = in progress. ⛔ = Blocked Example (blocked on the named stack gap). "Ask" = not yet built for that language; contact Chipkin if you need it. Profile definitions: ANSI/ASHRAE 135-2024 Annex L. BIBB definitions: Annex K. Get the stack: <https://store.chipkin.com/services/stacks/bacnet-stack>.
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
