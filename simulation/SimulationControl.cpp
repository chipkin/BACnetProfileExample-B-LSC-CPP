// SPDX-License-Identifier: CC0-1.0
// Public-domain example code (CC0) - see ../LICENSE.
// =============================================================================
// SimulationControl.cpp - SIMULATION ONLY, TEST ONLY. See SimulationControl.h.
// =============================================================================

#if defined(_WIN32) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS // getenv: read once, never kept, so MSVC's C4996 is noise
#endif

#include "SimulationControl.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET ControlSocket;
static const ControlSocket NO_SOCKET = INVALID_SOCKET;
#define CloseControlSocket closesocket
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int ControlSocket;
static const ControlSocket NO_SOCKET = -1;
#define CloseControlSocket close
#endif

namespace {

ControlSocket g_controlSocket = NO_SOCKET;
SimulationControl::CommandHandler g_handler = NULL;

} // namespace

namespace SimulationControl {

bool Start(const char* portVariable, CommandHandler handler) {
    const char* portText = getenv(portVariable);
    if (portText == NULL || portText[0] == '\0') {
        return true; // off - the normal case
    }
    const long port = strtol(portText, NULL, 10);
    if (port <= 0 || port > 65535) {
        printf("Error: %s=%s is not a UDP port.\n", portVariable, portText);
        return false;
    }
    g_controlSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (g_controlSocket == NO_SOCKET) {
        return false;
    }
    sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons((uint16_t)port);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // loopback only, never the network
    if (bind(g_controlSocket, (sockaddr*)&address, sizeof(address)) != 0) {
        printf("Error: could not bind the simulation control socket to 127.0.0.1:%ld.\n", port);
        CloseControlSocket(g_controlSocket);
        g_controlSocket = NO_SOCKET;
        return false;
    }
#if defined(_WIN32)
    u_long nonBlocking = 1;
    ioctlsocket(g_controlSocket, FIONBIO, &nonBlocking);
#else
    fcntl(g_controlSocket, F_SETFL, fcntl(g_controlSocket, F_GETFL, 0) | O_NONBLOCK);
#endif
    g_handler = handler;
    printf("SIMULATION: test control channel listening on 127.0.0.1:%ld (%s).\n", port, portVariable);
    return true;
}

void Service() {
    if (g_controlSocket == NO_SOCKET || g_handler == NULL) {
        return;
    }
    char command[128];
    sockaddr_in sender;
#if defined(_WIN32)
    int senderLength = sizeof(sender);
#else
    socklen_t senderLength = sizeof(sender);
#endif
    const int received = (int)recvfrom(g_controlSocket, command, sizeof(command) - 1, 0,
                                       (sockaddr*)&sender, &senderLength);
    if (received <= 0) {
        return;
    }
    command[received] = '\0';
    char reply[96];
    g_handler(command, reply, sizeof(reply));
    sendto(g_controlSocket, reply, (int)strlen(reply), 0, (sockaddr*)&sender, senderLength);
}

void Stop() {
    if (g_controlSocket != NO_SOCKET) {
        CloseControlSocket(g_controlSocket);
        g_controlSocket = NO_SOCKET;
    }
    g_handler = NULL;
}

} // namespace SimulationControl
