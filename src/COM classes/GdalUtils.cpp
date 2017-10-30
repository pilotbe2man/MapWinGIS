/**************************************************************************************
* File name: GdalUtils.cpp
*
* Project: MapWindow Open Source (MapWinGis ActiveX control)
* Description: Implementation of CGdalUtils
*
**************************************************************************************
* The contents of this file are subject to the Mozilla Public License Version 1.1
* (the "License"); you may not use this file except in compliance with
* the License. You may obtain a copy of the License at http://www.mozilla.org/mpl/
* See the License for the specific language governing rights and limitations
* under the License.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
* DEALINGS IN THE SOFTWARE.
**************************************************************************************
* Contributor(s):
* (Open source contributors should list themselves and their modifications here). */
// june 2017 PaulM - Initial creation of this file

#include "stdafx.h"
#include "GdalUtils.h"
// #include "GdalDataset.h"

// *********************************************************************
//		~CGdalUtils
// *********************************************************************
CGdalUtils::~CGdalUtils()
{
	gReferenceCounter.Release(tkInterface::idGdalUtils);
}

// *********************************************************************
//		get_LastErrorCode
// *********************************************************************
STDMETHODIMP CGdalUtils::get_LastErrorCode(long *pVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState())
		*pVal = _lastErrorCode;
	_lastErrorCode = tkNO_ERROR;
	return S_OK;
}

// *********************************************************************
//		get_ErrorMsg
// *********************************************************************
STDMETHODIMP CGdalUtils::get_ErrorMsg(long ErrorCode, BSTR *pVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState())
		USES_CONVERSION;
	*pVal = A2BSTR(ErrorMsg(ErrorCode));
	return S_OK;
}

// *********************************************************************
//		GlobalCallback
// *********************************************************************
STDMETHODIMP CGdalUtils::get_GlobalCallback(ICallback **pVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState())

		*pVal = _globalCallback;
	if (_globalCallback != NULL)
	{
		_globalCallback->AddRef();
	}
	return S_OK;
}

STDMETHODIMP CGdalUtils::put_GlobalCallback(ICallback *newVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState())
		ComHelper::SetRef(newVal, (IDispatch**)&_globalCallback);
	return S_OK;
}

STDMETHODIMP CGdalUtils::get_Key(BSTR *pVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState())
		USES_CONVERSION;

	*pVal = OLE2BSTR(_key);

	return S_OK;
}

STDMETHODIMP CGdalUtils::put_Key(BSTR newVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState())
		USES_CONVERSION;

	::SysFreeString(_key);
	_key = OLE2BSTR(newVal);

	return S_OK;
}
// *********************************************************
//	     GdalWarp()
// *********************************************************
STDMETHODIMP CGdalUtils::GdalWarp(BSTR bstrSrcFilename, BSTR bstrDstFilename, SAFEARRAY* options, VARIANT_BOOL* retVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	*retVal = VARIANT_FALSE;

	USES_CONVERSION;
	CStringW srcFilename = OLE2W(bstrSrcFilename);
	if (!Utility::FileExistsW(srcFilename))
	{
		CallbackHelper::ErrorMsg(Debug::Format("Source file %s does not exists.", srcFilename));
		ErrorMessage(tkINVALID_FILENAME);		
		return S_OK;
	}

	// Open file as GdalDataset:
	CallbackHelper::Progress(_globalCallback, 0, "Open source file as raster", _key);
	GDALDatasetH dt = GdalHelper::OpenRasterDatasetW(srcFilename, GA_ReadOnly);
	if (!dt)
	{
		CallbackHelper::ErrorMsg(Debug::Format("Can't open %s as a raster file.", srcFilename));
		ErrorMsg(tkINVALID_FILENAME);
		goto cleaning;
	}

	// Make options:		
	if (SafeArrayGetDim(options) != 1)
	{
		CallbackHelper::ErrorMsg(Debug::Format("The warp option are invalid."));
		ErrorMessage(tkINVALID_PARAMETERS_ARRAY);
		goto cleaning;
	}

	char** warpOptions = ConvertSafeArray(options);
	GDALWarpAppOptions* gdalWarpOptions = GDALWarpAppOptionsNew(warpOptions, NULL);

	// Call the gdalWarp function:
	CallbackHelper::Progress(_globalCallback, 50, "Start warping", _key);
	GDALWarpAppOptionsSetProgress(gdalWarpOptions, GDALProgressCallback, NULL); // TODO: Is this the correct implementation?
	auto dtNew = GDALWarp(OLE2A(bstrDstFilename), NULL, 1, &dt, gdalWarpOptions, NULL);
	CallbackHelper::Progress(_globalCallback, 75, "Finished warping", _key);
	if (dtNew)
	{
		*retVal = VARIANT_TRUE;
		GDALClose(dtNew);
	}
	else
	{
		CallbackHelper::ErrorMsg("GdalUtils", _globalCallback, _key, "Warping failed");
	}

cleaning:
	// ------------------------------------------------------
	//	Cleaning
	// ------------------------------------------------------
	if (gdalWarpOptions)
		GDALWarpAppOptionsFree(gdalWarpOptions);

	if (warpOptions)
		CSLDestroy(warpOptions);

	if (dt)
		GDALClose(dt);

	CallbackHelper::ProgressCompleted(_globalCallback);

	return S_OK;
}

// **********************************************************
//		ErrorMessage()
// **********************************************************
inline void CGdalUtils::ErrorMessage(long ErrorCode)
{
	_lastErrorCode = ErrorCode;
	CallbackHelper::ErrorMsg("GdalUtils", _globalCallback, _key, ErrorMsg(_lastErrorCode));
}

// ***************************************************************************************
//		ConvertSafeArray()
//      Convert a safearray (coming outside the ocx) to char** (used internal) 
// ***************************************************************************************
char** CGdalUtils::ConvertSafeArray(SAFEARRAY* safeArray)
{
	USES_CONVERSION;
	char** papsz_str_list = NULL;
	LONG lLBound, lUBound;
	BSTR HUGEP *pbstr;
	HRESULT hr1 = SafeArrayGetLBound(safeArray, 1, &lLBound);
	HRESULT hr2 = SafeArrayGetUBound(safeArray, 1, &lUBound);
	HRESULT hr3 = SafeArrayAccessData(safeArray, (void HUGEP* FAR*)&pbstr);
	if (!FAILED(hr1) && !FAILED(hr2) && !FAILED(hr3))
	{
		LONG count = lUBound - lLBound + 1;
		for (int i = 0; i < count; i++){
			// Create array:
			papsz_str_list = CSLAddString(papsz_str_list, OLE2A(pbstr[i]));
		}
	}

	return papsz_str_list;
}