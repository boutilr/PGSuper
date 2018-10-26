// PGSuperExporter.h : Declaration of the CPGSuperExporter

#ifndef __KDOTEXPORTER_H_
#define __KDOTEXPORTER_H_

#include "resource.h"       // main symbols

#include <PgsExt\BridgeDescription.h>
#include <IFace\AnalysisResults.h>
#include "KDOTExporter.hxx"


interface IShapes;
class LiveLoadLibraryEntry;

/////////////////////////////////////////////////////////////////////////////
// CPGSuperDataExporter
class ATL_NO_VTABLE CPGSuperDataExporter : 
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CPGSuperDataExporter, &CLSID_PGSuperDataExporter>,
   public IPGSuperDataExporter
{
public:
	CPGSuperDataExporter()
	{
	}

   HRESULT FinalConstruct();

   CBitmap m_bmpLogo;

DECLARE_REGISTRY_RESOURCEID(IDR_PGSUPERDATAEXPORTER)

DECLARE_PROTECT_FINAL_CONSTRUCT()

BEGIN_COM_MAP(CPGSuperDataExporter)
   COM_INTERFACE_ENTRY(IPGSuperDataExporter)
END_COM_MAP()

// IPGSuperDataExporter
public:
   STDMETHOD(Init)(UINT nCmdID);
   STDMETHOD(GetMenuText)(/*[out,retval]*/BSTR*  bstrText);
   STDMETHOD(GetBitmapHandle)(/*[out]*/HBITMAP* phBmp);
   STDMETHOD(GetCommandHintText)(BSTR*  bstrText);
   STDMETHOD(Export)(/*[in]*/IBroker* pBroker);

private:
   Float64 m_BearingHeight;
   HRESULT Export(IBroker* pBroker,CString& strFileName,const std::vector<SpanGirderHashType>& gdrList);

};

#endif //__PGSUPEREXPORTER_H_
