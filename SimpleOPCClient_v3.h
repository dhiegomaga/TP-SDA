// Simple OPC Client
//
// This is a modified version of the "Simple OPC Client" originally
// developed by Philippe Gras (CERN) for demonstrating the basic techniques
// involved in the development of an OPC DA client.
//
// The modifications are the introduction of two C++ classes to allow the
// the client to ask for callback notifications from the OPC server, and
// the corresponding introduction of a message comsumption loop in the
// main program to allow the client to process those notifications. The
// C++ classes implement the OPC DA 1.0 IAdviseSink and the OPC DA 2.0
// IOPCDataCallback client interfaces, and in turn were adapted from the
// KEPWARE�s  OPC client sample code. A few wrapper functions to initiate
// and to cancel the notifications were also developed.
//
// The original Simple OPC Client code can still be found (as of this date)
// in
//        http://pgras.home.cern.ch/pgras/OPCClientTutorial/
//
//
// Luiz T. S. Mendes - DELT/UFMG - 15 Sept 2011
// luizt at cpdee.ufmg.br
//

#ifndef SIMPLE_OPC_CLIENT_H
#define SIMPLE_OPC_CLIENT_H

#include <atlbase.h>    // required for using the "_T" macro
#include <iostream>
#include <ObjIdl.h>

// String manipulation
#include <string>
#include <sstream>
#include <iomanip>
#include <vector>

// Threads, etc
#include <chrono>
#include <thread>
#include <functional>
#include <mutex>

// Winsockets
// Source: https://docs.microsoft.com/en-us/windows/win32/winsock/complete-client-code
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")
#define DEFAULT_BUFLEN 512



#include "opcda.h"
#include "opcerror.h"

// Definitions
#define OPC_SERVER_NAME L"Matrikon.OPC.Simulation.1"
#define REAL4 VT_R4
#define UINT1 VT_UI1
#define UINT2 VT_UI2
#define UINT4 VT_UI4
#define REAL8 VT_R8

IOPCServer *InstantiateServer(wchar_t ServerName[]);
void AddTheGroup(IOPCServer* pIOPCServer, IOPCItemMgt* &pIOPCItemMgt, OPCHANDLE& hServerGroup);
void AddTheItem(IOPCItemMgt* pIOPCItemMgt, OPCHANDLE& hServerItem, wchar_t*, int, int);
void WriteItem(IUnknown* pGroupIUnknown, OPCHANDLE hServerItem, VARIANT* varValue);
void ReadItem(IUnknown* pGroupIUnknown, OPCHANDLE hServerItem, VARIANT& varValue);
void RemoveItem(IOPCItemMgt* pIOPCItemMgt, OPCHANDLE hServerItem);
void RemoveGroup(IOPCServer* pIOPCServer, OPCHANDLE hServerGroup);

// Added functions
void webclient_loop(unsigned int loop_delay);
void opcclient_loop(unsigned int loop_delay);
void set_disconnected();
void reconnect_server_thread(struct addrinfo *result);
std::string get_msg_seq();
std::string get_int_str(int val);
std::string get_float_str(float val);
std::vector<std::string> split (const std::string &s, char delim);
#endif // SIMPLE_OPC_CLIENT_H not defined
