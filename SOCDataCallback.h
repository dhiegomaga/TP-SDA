// Class for implementing the client-side IOPCDataCallback interface.
// This is based on the KEPWARE sample client code, to which goes all
// credits.
//
// Though the OPC DA 2.0 Spec defines 4 methods for this interface,
// in this example code only the OnDataChange() method is implemented.
//
// Luiz T. S. Mendes - DELT/UFMG - 05/09/2011
// 

#include "opcda.h"

#ifndef _SOCDATACALLBACK_H
#define _SOCDATACALLBACK_H

struct Posicao { 
	float vel_transl; 
	unsigned int coord_x;
	unsigned int coord_y;
	unsigned int coord_z;
	double taxa_rec;
};
typedef struct Posicao Posicao;

struct Status_rec {
	unsigned int taxa_rec_real;
	float potencia;
	float temp_transl;
	float temp_roda;
};
typedef struct Status_rec Status_rec;

struct Opc_item {
	OPCHANDLE item_handle;
	wchar_t *item_id;
	int type;
};

// **************************************************************************
class SOCDataCallback : public IOPCDataCallback
	{
	public:
		SOCDataCallback (
			Opc_item*,
			Status_rec*
		);
		~SOCDataCallback ();

		// IUnknown Methods
		HRESULT STDMETHODCALLTYPE QueryInterface (REFIID iid, LPVOID *ppv);
		ULONG STDMETHODCALLTYPE AddRef ();
		ULONG STDMETHODCALLTYPE Release ();

		// IOPCDataCallback Methods 
		HRESULT STDMETHODCALLTYPE OnDataChange (	// OnDataChange notifications
			DWORD dwTransID,			// 0 for normal OnDataChange events, non-zero for Refreshes
			OPCHANDLE hGroup,			// client group handle
			HRESULT hrMasterQuality,	// S_OK if all qualities are GOOD, otherwise S_FALSE
			HRESULT hrMasterError,		// S_OK if all errors are S_OK, otherwise S_FALSE
			DWORD dwCount,				// number of items in the lists that follow
			OPCHANDLE *phClientItems,	// item client handles
			VARIANT *pvValues,			// item data
			WORD *pwQualities,			// item qualities
			FILETIME *pftTimeStamps,	// item timestamps
			HRESULT *pErrors);			// item errors	

		HRESULT STDMETHODCALLTYPE OnReadComplete (	// OnReadComplete notifications
			DWORD dwTransID,			// Transaction ID returned by the server when the read was initiated
			OPCHANDLE hGroup,			// client group handle
			HRESULT hrMasterQuality,	// S_OK if all qualities are GOOD, otherwise S_FALSE
			HRESULT hrMasterError,		// S_OK if all errors are S_OK, otherwise S_FALSE
			DWORD dwCount,				// number of items in the lists that follow
			OPCHANDLE *phClientItems,	// item client handles
			VARIANT *pvValues,			// item data
			WORD *pwQualities,			// item qualities
			FILETIME *pftTimeStamps,	// item timestamps
			HRESULT *pErrors);			// item errors	

		HRESULT STDMETHODCALLTYPE OnWriteComplete (	// OnWriteComplete notifications
			DWORD dwTransID,			// Transaction ID returned by the server when the write was initiated
			OPCHANDLE hGroup,			// client group handle
			HRESULT hrMasterError,		// S_OK if all errors are S_OK, otherwise S_FALSE
			DWORD dwCount,				// number of items in the lists that follow
			OPCHANDLE *phClientItems,	// item client handles
			HRESULT *pErrors);			// item errors	

		HRESULT STDMETHODCALLTYPE OnCancelComplete (	// OnCancelComplete notifications
			DWORD dwTransID,			// Transaction ID provided by the client when the read/write/refresh was initiated
			OPCHANDLE hGroup);

	private:
		DWORD m_cnRef;
		Opc_item * taxa_rec_real;
		Status_rec * status;
	};


#endif // _SOCDATACALLBACK_H