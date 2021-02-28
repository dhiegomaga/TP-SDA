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

#include <stdio.h>

#include "SimpleOPCClient_v3.h"
#include "SOCAdviseSink.h"
#include "SOCDataCallback.h"
#include "SOCWrapperFunctions.h"

using namespace std;

// ------- THREADS AND SYNCHRONICITY -------
std::mutex socket_mutex; // Protects socket variable
std::mutex opc_mutex; // Protects opc variables
bool executing= false; 
bool connected = false;
HANDLE connectionLostEvent; // Event to handle lost connections


// ------- WINSOCK -------
#define DEFAULT_PORT "3445"
SOCKET ConnectSocket = INVALID_SOCKET;
char recvbuf[DEFAULT_BUFLEN];
int recvbuflen = DEFAULT_BUFLEN;

// ------- OPC GLOBAL VARIABLES -------
IOPCServer* pIOPCServer = NULL;   //pointer to IOPServer interface
IOPCItemMgt* pIOPCItemMgt = NULL; //pointer to IOPCItemMgt interface
OPCHANDLE hServerGroup; // server handle to the group

// Write items (Posicao)
Opc_item vel_trans = {NULL, L"Bucket Brigade.Real4", REAL4,1 };
Opc_item coord_x = { NULL, L"Bucket Brigade.UInt1", UINT1,2 };
Opc_item coord_y = { NULL, L"Bucket Brigade.UInt2", UINT2,3 };
Opc_item coord_z = { NULL, L"Bucket Brigade.UInt4", UINT4,4 };
Opc_item taxa_rec = { NULL, L"Bucket Brigade.Real8", REAL8,5 };

// Read items (Status)
Opc_item taxa_rec_real = { NULL, L"Random.UInt1", UINT1,6 };
Opc_item potencia = { NULL, L"Random.Real4", REAL4,7 };
Opc_item temp_transl = { NULL, L"Saw-Toothed Waves.Real4", REAL4,8 };
Opc_item temp_roda = { NULL, L"Square Waves.Real4", REAL4,9 };

// State variables
unsigned int msg_seq = 1;
Posicao posicao = { 0.0,0,0,0, 0.0 };
Status_rec status = { 0,0,0,0 };

// The OPC DA Spec requires that some constants be registered in order to use
// them. The one below refers to the OPC DA 1.0 IDataObject interface.
UINT OPC_DATA_TIME = RegisterClipboardFormat (_T("OPCSTMFORMATDATATIME"));

int main(void)
{
	int i;
	char buf[100];
	unsigned int loop_web_time = 2000;
	unsigned int loop_opc_time = 1000;
	executing = true;

	// ---------- RECONNECT EVENT -----------
	// Source: https://docs.microsoft.com/en-us/windows/win32/sync/using-event-objects
	connectionLostEvent = CreateEvent( 
        NULL,               // default security attributes
        FALSE,               // NOT manual-reset event
        FALSE,              // initial state is nonsignaled
        TEXT("ReconnectEvent")  // object name
        ); 
    if (connectionLostEvent == NULL) 
    { 
        printf("CreateEvent failed (%d)\n", GetLastError());
        return 1;
    }

	// ----------- WINSOCKETS -----------
	WSADATA wsaData;
    struct addrinfo *result = NULL,
                    *ptr = NULL,
                    hints;
    
    int iResult;

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    ZeroMemory( &hints, sizeof(hints) );
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // Resolve the server address and port
    iResult = getaddrinfo("localhost", DEFAULT_PORT, &hints, &result);
    if ( iResult != 0 ) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return 1;
    }

	// Connection to be estabilished
	connected = false;

	// ----------- OPC -----------
	printf("Initializing the COM environment\n");
	CoInitializeEx(NULL,COINIT_MULTITHREADED); // Initialize COM environment
	pIOPCServer = InstantiateServer(OPC_SERVER_NAME); // Take ProgId -> Generate COM (server) instance 
	
	// Add the OPC group the OPC server and get an handle to the IOPCItemMgt
	//interface:
	AddTheGroup(pIOPCServer, pIOPCItemMgt, hServerGroup);

	// Add the OPC item. First we have to convert from wchar_t* to char*
	// in order to print the item name in the console.

	AddTheItem(pIOPCItemMgt, vel_trans.item_handle , vel_trans.item_id , vel_trans.type, vel_trans.id );
	AddTheItem(pIOPCItemMgt, coord_x.item_handle , coord_x.item_id , coord_x.type, coord_x.id );
	AddTheItem(pIOPCItemMgt, coord_y.item_handle , coord_y.item_id , coord_y.type, coord_y.id );
	AddTheItem(pIOPCItemMgt, coord_z.item_handle , coord_z.item_id , coord_z.type, coord_z.id );
	AddTheItem(pIOPCItemMgt, taxa_rec.item_handle , taxa_rec.item_id , taxa_rec.type, taxa_rec.id );

	// Status items
	AddTheItem(pIOPCItemMgt, taxa_rec_real.item_handle, taxa_rec_real.item_id, taxa_rec_real.type, taxa_rec_real.id);
	AddTheItem(pIOPCItemMgt, potencia.item_handle, potencia.item_id, potencia.type, potencia.id);
	AddTheItem(pIOPCItemMgt, temp_transl.item_handle, temp_transl.item_id, temp_transl.type, temp_transl.id);
	AddTheItem(pIOPCItemMgt, temp_roda.item_handle, temp_roda.item_id, temp_roda.type, temp_roda.id);

	VARIANT varValue; //to store the read value
	VariantInit(&varValue);
	
	// Initialize reconnect thread
	std::thread t1(reconnect_server_thread, result);
	SetEvent(connectionLostEvent); // estabilish connection

	// Initialize web clint thread
	std::thread t2(webclient_loop, loop_web_time);

	// Initialize opc client thread
	std::thread t3(opcclient_loop, loop_opc_time);


	//std::thread t4(opcread_loop, loop_opc_read__time);
	
	// Establish a callback asynchronous read by means of the IOPCDataCallback
	// (OPC DA 2.0) method. We first instantiate a new SOCDataCallback object and
	// adjusts its reference count, and then call a wrapper function to
	// setup the callback.
	IConnectionPoint* pIConnectionPoint = NULL; //pointer to IConnectionPoint Interface
	DWORD dwCookie = 0;
	SOCDataCallback* pSOCDataCallback = new SOCDataCallback(
		&taxa_rec_real, 
		&potencia, 
		&temp_transl, 
		&temp_roda, 
		&status,
		&opc_mutex);
	pSOCDataCallback->AddRef();
	SetDataCallback(pIOPCItemMgt, pSOCDataCallback, pIConnectionPoint, &dwCookie);

	// Change the group to the ACTIVE state so that we can receive the
	// server's callback notification
    SetGroupActive(pIOPCItemMgt); 

	printf("Press Q+ENTER to terminate ... \n");
	while(true){
		int c=getchar();

		if((char)c=='p') {
			// Solicit position from web server ---
			// seq.33 ->
			// position X, Y, Z ... <- 
			// ACK ->

			if(!connected){
				printf("Nao e' possivel mandar mensagens enquanto \
				a Conexao Nao for reestabelecida. \n");
			}
			else{
				socket_mutex.lock();
				if(!connected){
					// Test connection again in case it was lost during the mutex wait
					printf("Nao e' possivel mandar mensagens enquanto \
					a Conexao Nao for reestabelecida. \n");
					socket_mutex.unlock();
					continue;
				}
				// Build message
				std::string send_msg = get_msg_seq();
				send_msg+= "$33";

				// Send request
				iResult = send( ConnectSocket, send_msg.c_str(), (int) send_msg.size() , 0 );
				if (iResult == SOCKET_ERROR) {
					printf("Erro em send(): %d\n", WSAGetLastError());

					// Reconnect to server ...
					set_disconnected();
					socket_mutex.unlock();
					continue;
				}

				printf("SENT: %s\n", send_msg.c_str());

				// Receive data
				iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);
				if ( iResult > 0 )
				{
					printf("RECV: %s\n", recvbuf);
					if(++msg_seq >= 1000000) msg_seq = 1;

					// Update posicao
					string data(recvbuf);
					std::vector<string> fields = split(data, '$');

					// Test if message has 7 fields
					if(fields.size() != 7){
						printf("MENSAGEM DO SERVIDOR NAO ESTA NO FORMATO ESPERADO.\n\n");
					}

					posicao.vel_transl = std::stod(fields.at(2));
					posicao.coord_x = std::stoi(fields.at(3));
					posicao.coord_y = std::stoi(fields.at(4));
					posicao.coord_z = std::stoi(fields.at(5));
					posicao.taxa_rec = std::stod(fields.at(6));

				}
				else 
				{
					if ( iResult == 0 ) printf("Conexao perdida. \n");
					else printf("Erro em recv(): %d\n", WSAGetLastError());
					// Reconnect to server ...
					set_disconnected();
					socket_mutex.unlock();
					continue;
				}

				// Send ACK
				std::string send_ack = get_msg_seq();
				send_ack+= "$99";
				iResult = send( ConnectSocket, send_ack.c_str(), (int) send_ack.size() , 0 );
				if (iResult == SOCKET_ERROR) {
					printf("Erro em send(): %d\n", WSAGetLastError());

					// Reconnect to server ...
					set_disconnected();
					socket_mutex.unlock();
					continue;
				}


				socket_mutex.unlock();
			}

		}

		if((char)c=='q') break;
	}
	

	// Cancel the callback and release its reference
	CancelDataCallback(pIConnectionPoint, dwCookie);
	pSOCDataCallback->Release();
	
	// Stop threads
	executing = false;

	// Signal reconnect_server_thread() to stop working
	SetEvent(connectionLostEvent);

	// Wait threads to finish
	t1.join();
	t2.join();
	t3.join();

	//t4.join();

	// Close event
	CloseHandle(connectionLostEvent);
	freeaddrinfo(result);

	// Remove items
	printf("Removing items ...\n");
	RemoveItem(pIOPCItemMgt, taxa_rec_real.item_handle);
	RemoveItem(pIOPCItemMgt, potencia.item_handle);
	RemoveItem(pIOPCItemMgt, temp_transl.item_handle);
	RemoveItem(pIOPCItemMgt, temp_roda.item_handle);

	// Remove the OPC group:
    pIOPCItemMgt->Release();
	RemoveGroup(pIOPCServer, hServerGroup);

	// release the interface references:
	pIOPCServer->Release();

	//close the COM library:
	CoUninitialize();
	
	// CLOSE SOCKET
	// shutdown the connection since no more data will be sent
	if (connected){
		iResult = shutdown(ConnectSocket, SD_SEND);
		if (iResult == SOCKET_ERROR) {
			printf("shutdown failed with error: %d\n", WSAGetLastError());
			closesocket(ConnectSocket);
			WSACleanup();
			return 1;
		}
	}
	else{
		closesocket(ConnectSocket);
	}
    
	// Receive until the peer closes the connection
    do {

        iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);
        if ( iResult > 0 )
            printf("Bytes received: %d\n", iResult);
        else if ( iResult == 0 )
            printf("Connection closed\n");
        else
            printf("recv failed with error: %d\n", WSAGetLastError());

    } while( iResult > 0 );

	return 0;
}

////////////////////////////////////////////////////////////////////
// Instantiate the IOPCServer interface of the OPCServer
// having the name ServerName. Return a pointer to this interface
//
IOPCServer* InstantiateServer(wchar_t ServerName[])
{
	CLSID CLSID_OPCServer;
	HRESULT hr;

	// get the CLSID from the OPC Server Name:
	hr = CLSIDFromString(ServerName, &CLSID_OPCServer); // CLSIDfromProgId()
	_ASSERT(!FAILED(hr));


	//queue of the class instances to create
	LONG cmq = 1; // nbr of class instance to create.
	MULTI_QI queue[1] =
		{{&IID_IOPCServer,
		NULL,
		0}};

	//Server info:
	//COSERVERINFO CoServerInfo =
    //{
	//	/*dwReserved1*/ 0,
	//	/*pwszName*/ REMOTE_SERVER_NAME,
	//	/*COAUTHINFO*/  NULL,
	//	/*dwReserved2*/ 0
    //}; 

	// create an instance of the IOPCServer (COM intance)
	hr = CoCreateInstanceEx(CLSID_OPCServer, NULL, CLSCTX_SERVER, /*&CoServerInfo*/ NULL, cmq, queue);
	_ASSERT(!hr);

	// return a pointer to the IOPCServer interface:
	return(IOPCServer*) queue[0].pItf;
}


/////////////////////////////////////////////////////////////////////
// Add group "Group1" to the Server whose IOPCServer interface
// is pointed by pIOPCServer. 
// Returns a pointer to the IOPCItemMgt interface of the added group
// and a server opc handle to the added group.
//
void AddTheGroup(IOPCServer* pIOPCServer, IOPCItemMgt* &pIOPCItemMgt, 
				 OPCHANDLE& hServerGroup)
{
	DWORD dwUpdateRate = 0;
	OPCHANDLE hClientGroup = 0;

	// pIOPCServer: instance of the OPCServer COM Interface 

    HRESULT hr = pIOPCServer->AddGroup(/*szName*/ L"Group1",
		/*bActive*/ FALSE,
		/*dwRequestedUpdateRate*/ 1000,
		/*hClientGroup*/ hClientGroup,
		/*pTimeBias*/ 0,
		/*pPercentDeadband*/ 0,
		/*dwLCID*/0,
		/*phServerGroup*/&hServerGroup,
		&dwUpdateRate,
		/*riid*/ IID_IOPCItemMgt,
		/*ppUnk*/ (IUnknown**) &pIOPCItemMgt);
	_ASSERT(!FAILED(hr));
}



//////////////////////////////////////////////////////////////////
// Add the Item ITEM_ID to the group whose IOPCItemMgt interface
// is pointed by pIOPCItemMgt pointer. Return a server opc handle
// to the item.
 
void AddTheItem(IOPCItemMgt* pIOPCItemMgt, OPCHANDLE& hServerItem, wchar_t* ITEM_ID, int VT, int identifier)
{
	HRESULT hr;

	// Array of items to add:
	OPCITEMDEF ItemArray[1] =
	{{
	/*szAccessPath*/ L"",
	/*szItemID*/ ITEM_ID,
	/*bActive*/ TRUE,
	/*hClient*/ identifier,
	/*dwBlobSize*/ 0,
	/*pBlob*/ NULL,
	/*vtRequestedDataType*/ VT,
	/*wReserved*/0
	}};

	//Add Result:
	OPCITEMRESULT* pAddResult=NULL;
	HRESULT* pErrors = NULL;

	// Add an Item to the previous Group:
	hr = pIOPCItemMgt->AddItems(1, ItemArray, &pAddResult, &pErrors);
	if (hr != S_OK){
		printf("Failed call to AddItems function. Error code = %x\n", hr);
		exit(0);
	}

	// Server handle for the added item:
	hServerItem = pAddResult[0].hServer;

	// release memory allocated by the server:
	CoTaskMemFree(pAddResult->pBlob);

	CoTaskMemFree(pAddResult);
	pAddResult = NULL;

	CoTaskMemFree(pErrors);
	pErrors = NULL;
}

///////////////////////////////////////////////////////////////////////////////
// Read from device the value of the item having the "hServerItem" server 
// handle and belonging to the group whose one interface is pointed by
// pGroupIUnknown. The value is put in varValue. 
//
void ReadItem(IUnknown* pGroupIUnknown, OPCHANDLE hServerItem, VARIANT& varValue)
{
	// value of the item:
	OPCITEMSTATE* pValue = NULL;

	//get a pointer to the IOPCSyncIOInterface:
	IOPCSyncIO* pIOPCSyncIO;
	pGroupIUnknown->QueryInterface(__uuidof(pIOPCSyncIO), (void**) &pIOPCSyncIO);

	// read the item value from the device:
	HRESULT* pErrors = NULL; //to store error code(s)
	HRESULT hr = pIOPCSyncIO->Read(OPC_DS_DEVICE, 1, &hServerItem, &pValue, &pErrors);
	_ASSERT(!hr);
	_ASSERT(pValue!=NULL);

	varValue = pValue[0].vDataValue;

	//Release memeory allocated by the OPC server:
	CoTaskMemFree(pErrors);
	pErrors = NULL;

	CoTaskMemFree(pValue);
	pValue = NULL;

	// release the reference to the IOPCSyncIO interface:
	pIOPCSyncIO->Release();
}

void WriteItem(IUnknown* pGroupIUnknown, OPCHANDLE hServerItem, VARIANT * varValue)
{
	//get a pointer to the IOPCSyncIOInterface:
	IOPCSyncIO* pIOPCSyncIO;
	pGroupIUnknown->QueryInterface(__uuidof(pIOPCSyncIO), (void**)&pIOPCSyncIO);

	// read the item value from the device:
	HRESULT* pErrors = NULL; //to store error code(s)
	HRESULT hr = pIOPCSyncIO->Write(1, &hServerItem, varValue, &pErrors);
	_ASSERT(!hr);

	//Release memeory allocated by the OPC server:
	CoTaskMemFree(pErrors);
	pErrors = NULL;

	// release the reference to the IOPCSyncIO interface:
	pIOPCSyncIO->Release();
}

///////////////////////////////////////////////////////////////////////////
// Remove the item whose server handle is hServerItem from the group
// whose IOPCItemMgt interface is pointed by pIOPCItemMgt
//
void RemoveItem(IOPCItemMgt* pIOPCItemMgt, OPCHANDLE hServerItem)
{
	// server handle of items to remove:
	OPCHANDLE hServerArray[1];
	hServerArray[0] = hServerItem;
	
	//Remove the item:
	HRESULT* pErrors; // to store error code(s)
	HRESULT hr = pIOPCItemMgt->RemoveItems(1, hServerArray, &pErrors);
	_ASSERT(!hr);

	//release memory allocated by the server:
	CoTaskMemFree(pErrors);
	pErrors = NULL;
}

////////////////////////////////////////////////////////////////////////
// Remove the Group whose server handle is hServerGroup from the server
// whose IOPCServer interface is pointed by pIOPCServer
//
void RemoveGroup (IOPCServer* pIOPCServer, OPCHANDLE hServerGroup)
{
	// Remove the group:
	HRESULT hr = pIOPCServer->RemoveGroup(hServerGroup, FALSE);
	if (hr != S_OK){
		if (hr == OPC_S_INUSE)
			printf ("Failed to remove OPC group: object still has references to it.\n");
		else printf ("Failed to remove OPC group. Error code = %x\n", hr);
		exit(0);
	}
}

void webclient_loop(unsigned int loop_delay) {
	// Send STATUS to webserver

	int iResult;
	std::chrono::milliseconds interval(loop_delay);

	while(executing)
	{	
		if(socket_mutex.try_lock())
		{
			if(!connected){
				socket_mutex.unlock();
				continue;
			}
			std::string send_msg = get_msg_seq();
			send_msg+= "$";
			send_msg+= "11";
			send_msg+= "$";
			send_msg+= get_int_str(status.taxa_rec_real);
			send_msg+= "$";
			send_msg+= get_float_str(status.potencia);
			send_msg+= "$";
			send_msg+= get_float_str(status.temp_transl);
			send_msg+= "$";
			send_msg+= get_float_str(status.temp_roda);


			iResult = send( ConnectSocket, send_msg.c_str(), (int) send_msg.size() , 0 );
			printf("SENT: %s\n", send_msg.c_str());
			if (iResult == SOCKET_ERROR) {
				printf("Erro em send(): %d\n", WSAGetLastError());

				// Reconnect to server ...
				set_disconnected();
				socket_mutex.unlock();
				continue;
			}

			iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);
			if ( iResult > 0 )
			{
				if(iResult>= recvbuflen)iResult-=1; // Prevent possible invalid memory access
				recvbuf[iResult]=0;
				printf("RECV: %s\n", recvbuf);
				if(++msg_seq >= 1000000) msg_seq = 1;
			}
			else 
			{
				if ( iResult == 0 ) printf("Conexao perdida. \n");
				else printf("Erro em recv(): %d\n", WSAGetLastError());

				// Reconnect to server ...
				set_disconnected();
				socket_mutex.unlock();
				continue;
			}

			socket_mutex.unlock();
		}
		else{
			continue;
		}
		
		std::this_thread::sleep_for(interval);
	}

}

void opcclient_loop(unsigned int loop_delay) {
	// WRITE variables (Posicao) to OPC server
	VARIANT varValue; //to store the read value
	VariantInit(&varValue);
	std::chrono::milliseconds interval(loop_delay);

	while(executing)
	{
		if(opc_mutex.try_lock())
		{
			varValue.vt = vel_trans.type;
			varValue.fltVal = posicao.vel_transl;
			WriteItem(pIOPCItemMgt, vel_trans.item_handle, &varValue);

			varValue.vt = coord_x.type;
			
			// Cap posicao.coord_x to 255, because it is only 1 byte
			unsigned int cx = posicao.coord_x;
			if (cx > 255) cx = 255;
			varValue.uintVal = cx;
			varValue.uiVal = (unsigned short) posicao.coord_x;
			WriteItem(pIOPCItemMgt, coord_x.item_handle, &varValue);

			varValue.vt = coord_y.type;
			varValue.uintVal = posicao.coord_y;
			WriteItem(pIOPCItemMgt, coord_y.item_handle, &varValue);

			varValue.vt = coord_z.type;
			varValue.uintVal = posicao.coord_z;
			WriteItem(pIOPCItemMgt, coord_z.item_handle, &varValue);

			varValue.vt = taxa_rec.type;
			varValue.dblVal = posicao.taxa_rec;
			WriteItem(pIOPCItemMgt, taxa_rec.item_handle, &varValue);
			opc_mutex.unlock();
		}
		else{
			continue;
		}
		std::this_thread::sleep_for(interval);
	}
}

void opcread_loop(unsigned int loop_delay) {
	// READ variables (status) from OPC Server

	VARIANT varValue; 
	VariantInit(&varValue);
	std::chrono::milliseconds interval(loop_delay);
	bool verbose = false;
	while(executing)
	{
		if(opc_mutex.try_lock())
		{
			ReadItem(pIOPCItemMgt, taxa_rec_real.item_handle, varValue);
			if(status.taxa_rec_real != varValue.uintVal)
			{
				status.taxa_rec_real = varValue.uintVal;
				if(verbose) printf("%S: %d\n", taxa_rec_real.item_id , status.taxa_rec_real );
			}
			
			ReadItem(pIOPCItemMgt, potencia.item_handle, varValue);
			if(status.potencia != varValue.fltVal)
			{
				status.potencia = varValue.fltVal;
				if(verbose) printf("%S: %f\n", potencia.item_id , status.potencia );
			}

			ReadItem(pIOPCItemMgt, temp_transl.item_handle, varValue);
			if(status.temp_transl != varValue.fltVal)
			{
				status.temp_transl = varValue.fltVal;
				if(verbose) printf("%S: %f\n", temp_transl.item_id , status.temp_transl );
			}
			
			ReadItem(pIOPCItemMgt, temp_roda.item_handle, varValue);
			if(status.temp_roda != varValue.fltVal)
			{
				status.temp_roda = varValue.fltVal;
				if(verbose) printf("%S: %f\n", temp_roda.item_id , status.temp_roda );
			}
				opc_mutex.unlock();
		}
		else{
			continue;
		}
	
		std::this_thread::sleep_for(interval);
	}
}

void reconnect_server_thread(struct addrinfo *result){
	struct addrinfo *ptr = NULL;
	std::chrono::milliseconds interval(2000);
	int iResult;
	while(true){
		// wait for event...
		DWORD dwWaitResult = WaitForSingleObject( 
			connectionLostEvent, // event handle
			INFINITE);    // indefinite wait

		if (dwWaitResult!=WAIT_OBJECT_0) {
			printf("Wait error (%d)\n", GetLastError()); 
		}

		// check if execution terminated
		if(!executing) break;

		// lock connection mutex
		socket_mutex.lock();
		connected = false;

		while(executing){ // Connection loop

			if (ConnectSocket != INVALID_SOCKET) {
				// Test if there is connection
				printf("Testando Conexao... \n");

				// Build message
				std::string send_msg = get_msg_seq();
				send_msg+= "$33";

				// Send request
				iResult = send( ConnectSocket, send_msg.c_str(), (int) send_msg.size() , 0 );
				printf("SENT: %s\n", send_msg.c_str());

				// Receive data
				iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);
				if ( iResult > 0 )
				{
					printf("RECV: %s\n\n", recvbuf);
					msg_seq += 1;

					// Send ACK if data was received
					std::string send_ack = get_msg_seq();
					send_ack+= "$99";
					iResult = send( ConnectSocket, send_ack.c_str(), (int) send_ack.size() , 0 );

					// Connection restored
					printf("Conexao estabelecida. \n\n");
					connected = true;

					break; // Break connection loop
				}

				else{
					// No connection 
					closesocket(ConnectSocket);
					ConnectSocket = INVALID_SOCKET;
				}
			}

			// No connection, attempt to connect
			printf("Reconectando... \n");
			msg_seq = 1;

			// Wait to try again
			std::this_thread::sleep_for(interval);
			
			for(ptr=result; ptr != NULL ;ptr=ptr->ai_next) {

				// Create a SOCKET for connecting to server
				ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, 
					ptr->ai_protocol);
				if (ConnectSocket == INVALID_SOCKET) {
					printf("socket failed with error: %ld\n", WSAGetLastError());
				}

				// Connect to server.
				iResult = connect( ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
				if (iResult == SOCKET_ERROR) {
					closesocket(ConnectSocket);
					ConnectSocket = INVALID_SOCKET;
					continue;
				}
				break; // Finished connecting
			}

		}

		socket_mutex.unlock();
	}
}

void set_disconnected(){
	SetEvent(connectionLostEvent);
	connected = false;
}

std::string get_msg_seq() {
	// Fills string with leading zeros
	// Source: https://stackoverflow.com/questions/225362/convert-a-number-to-a-string-with-specified-length-in-c

	std::stringstream ss;
	ss << std::setw(6) << std::setfill('0') << msg_seq;
	std::string s = ss.str();

	if(++msg_seq >= 1000000) msg_seq = 1;

	return s; 
}

std::string get_int_str(int val){
	std::stringstream ss;
	if(val > 999999) val = 999999;
	if(val <0) val = 0;
	ss << std::setw(6) << std::setfill('0') << val;
	std::string s = ss.str();
	return s;
}

std::string get_float_str(float val){
	std::stringstream ss;
	if(val>9999.0) val = 9999.0;
	if(val<0.0) val = 0.0;
	ss << std::setw(6) << std::setfill('0') <<  std::fixed << std::setprecision(1) << val;
	std::string s = ss.str();
	return s;
}

std::vector<string> split (const string &s, char delim) {
	// Splits string given character 
	// Source: https://stackoverflow.com/a/46931770/2076973
    std::vector<string> result;
    stringstream ss (s);
    string item;

    while (getline (ss, item, delim)) {
        result.push_back (item);
    }

    return result;
}