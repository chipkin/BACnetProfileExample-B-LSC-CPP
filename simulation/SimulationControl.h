// SPDX-License-Identifier: CC0-1.0
// Public-domain example code (CC0) - see ../LICENSE.
#ifndef SIMULATION_CONTROL_H
#define SIMULATION_CONTROL_H

// =============================================================================
// SIMULATION ONLY, TEST ONLY - the back door the Wire Tests (tests/) use to
// move the SIMULATED smoke detector from outside the process: put smoke in
// front of it, clear it, pull its head off its base (a wiring fault).
//
// It is OFF unless the environment variable named by Start() (this example:
// BLSC_SIMULATION_CONTROL_PORT) names a UDP port. Then it listens on 127.0.0.1
// (loopback only) on that port for one plain-text command per datagram, hands
// each one to the example's handler, and sends the handler's reply back as one
// datagram. The commands themselves are the example's (see main.cpp, section
// 1b); this file only carries them.
//
// It is deliberately NOT a command-line option: nothing here changes what the
// controller does on the BACnet network, and a customer reading --help should
// never meet it. A real product has no such thing - its detector is wired to
// a real input. Delete this file and the handler in main.cpp together.
// (Same shape as B-GW-CPP's simulation/SimulationControl, with the command
// parsing moved out to a handler so the channel itself is generic.)
// =============================================================================

#include <stddef.h>

namespace SimulationControl {

// Runs one command (a NUL-terminated line) and writes the reply into reply.
typedef void (*CommandHandler)(const char* command, char* reply, size_t replyLength);

// Opens the control socket if the environment variable portVariable is set.
// Call after common/'s SetupUDP (which has already initialised sockets on
// Windows). Returns false only if the variable is set and the socket could
// not be opened.
bool Start(const char* portVariable, CommandHandler handler);

// Answer any commands waiting on the socket. Non-blocking; call every loop.
void Service();

void Stop();

} // namespace SimulationControl

#endif // SIMULATION_CONTROL_H
