///////////////////////////////////////////////////////////////////////
// PGSuper - Prestressed Girder SUPERstructure Design and Analysis
// Copyright � 1999-2010  Washington State Department of Transportation
//                        Bridge and Structures Office
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the Alternate Route Open Source License as 
// published by the Washington State Department of Transportation, 
// Bridge and Structures Office.
//
// This program is distributed in the hope that it will be useful, but 
// distribution is AS IS, WITHOUT ANY WARRANTY; without even the implied 
// warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See 
// the Alternate Route Open Source License for more details.
//
// You should have received a copy of the Alternate Route Open Source 
// License along with this program; if not, write to the Washington 
// State Department of Transportation, Bridge and Structures Office, 
// P.O. Box  47340, Olympia, WA 98503, USA or e-mail 
// Bridge_Support@wsdot.wa.gov
///////////////////////////////////////////////////////////////////////

// EffectiveFlangeWidthTool.h : Declaration of the CEffectiveFlangeWidthTool

#ifndef __EFFECTIVEFLANGEWIDTHTOOL_H_
#define __EFFECTIVEFLANGEWIDTHTOOL_H_

#include "resource.h"       // main symbols
#include "BridgeAgent_clsid.c"
#include <PgsExt\PoiKey.h>
#include <Reporter\Reporter.h>
#include <EAF\EAFDisplayUnits.h>
#include <map>

interface IBeamFactory;
class CBridgeDescription;

// {5D8E135F-F568-4287-87E4-E92538AC2C7F}
DEFINE_GUID(IID_IReportEffectiveFlangeWidth, 
0x5d8e135f, 0xf568, 0x4287, 0x87, 0xe4, 0xe9, 0x25, 0x38, 0xac, 0x2c, 0x7f);
interface IReportEffectiveFlangeWidth : IUnknown
{
   virtual void ReportEffectiveFlangeWidth(IBroker* pBroker,IGenericBridge* bridge,SpanIndexType span,GirderIndexType girder,rptChapter* pChapter,IEAFDisplayUnits* pDisplayUnits) = 0;
};
struct __declspec(uuid("{5D8E135F-F568-4287-87E4-E92538AC2C7F}")) IReportEffectiveFlangeWidth;


// {A14DCE16-2C8E-4609-8608-1A704E311431}
DEFINE_GUID(IID_IPGSuperEffectiveFlangeWidthTool, 
0xa14dce16, 0x2c8e, 0x4609, 0x86, 0x8, 0x1a, 0x70, 0x4e, 0x31, 0x14, 0x31);
interface IPGSuperEffectiveFlangeWidthTool : IUnknown
{
   STDMETHOD(put_UseTributaryWidth)(VARIANT_BOOL bUseTribWidth) = 0;
   STDMETHOD(get_UseTributaryWidth)(VARIANT_BOOL* bUseTribWidth) = 0;
};
struct __declspec(uuid("{A14DCE16-2C8E-4609-8608-1A704E311431}")) IPGSuperEffectiveFlangeWidthTool;

/////////////////////////////////////////////////////////////////////////////
// CEffectiveFlangeWidthTool
class ATL_NO_VTABLE CEffectiveFlangeWidthTool : 
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CEffectiveFlangeWidthTool, &CLSID_CustomEffectiveFlangeWidthTool>,
	public ISupportErrorInfo,
	public IPGSuperEffectiveFlangeWidthTool,
	public IEffectiveFlangeWidthTool,
   public IReportEffectiveFlangeWidth
{
public:
	CEffectiveFlangeWidthTool()
	{
	}

   void Init(IBroker* pBroker,StatusGroupIDType statusGroupID);

   HRESULT FinalConstruct();
   void FinalRelease();

//DECLARE_REGISTRY_RESOURCEID(IDR_EFFECTIVEFLANGEWIDTHTOOL)

DECLARE_PROTECT_FINAL_CONSTRUCT()

BEGIN_COM_MAP(CEffectiveFlangeWidthTool)
	COM_INTERFACE_ENTRY(IPGSuperEffectiveFlangeWidthTool)
	COM_INTERFACE_ENTRY(IEffectiveFlangeWidthTool)
   COM_INTERFACE_ENTRY(IReportEffectiveFlangeWidth)
	COM_INTERFACE_ENTRY(ISupportErrorInfo)
END_COM_MAP()

private:
   CComPtr<IEffectiveFlangeWidthTool> m_Tool;
   IBroker* m_pBroker;
   StatusGroupIDType m_StatusGroupID;

   // callback IDs for the status callbacks we register
   StatusCallbackIDType m_scidInformationalWarning;
   StatusCallbackIDType m_scidBridgeDescriptionError;

   // container for tributary width
   struct TribWidth
   {
      double twLeft,twRight,tribWidth;
   };

   typedef std::map<PoiStageKey,TribWidth> TWContainer;
   TWContainer m_TFW;

   struct EffFlangeWidth
   {
      CComPtr<IEffectiveFlangeWidthDetails> m_Details;
      double twLeft, twRight, tribWidth;
      bool bContinuousBarrier;
      double Ab,ts;
      double effFlangeWidth;
   };
   typedef std::map<PoiStageKey,EffFlangeWidth> FWContainer;
   FWContainer m_EFW;

   VARIANT_BOOL m_bUseTribWidth;

   HRESULT EffectiveFlangeWidthDetails(IGenericBridge* bridge,SpanIndexType spanIdx,GirderIndexType gdrIdx,double location, EffFlangeWidth* effFlangeWidth);

   void ReportEffectiveFlangeWidth_InteriorGirder(IBroker* pBroker,IGenericBridge* bridge,SpanIndexType span,GirderIndexType gdr,rptChapter* pChapter,IEAFDisplayUnits* pDisplayUnits);
   void ReportEffectiveFlangeWidth_InteriorGirder_Prismatic(IBroker* pBroker,IGenericBridge* bridge,SpanIndexType span,GirderIndexType gdr,rptChapter* pChapter,IEAFDisplayUnits* pDisplayUnits);
   void ReportEffectiveFlangeWidth_InteriorGirder_Nonprismatic(IBroker* pBroker,IGenericBridge* bridge,SpanIndexType span,GirderIndexType gdr,rptChapter* pChapter,IEAFDisplayUnits* pDisplayUnits);
   void ReportEffectiveFlangeWidth_InteriorGirderRow(IEffectiveFlangeWidthDetails* details,RowIndexType row,rptRcTable* table,IEAFDisplayUnits* pDisplayUnits);
   void ReportEffectiveFlangeWidth_ExteriorGirder(IBroker* pBroker,IGenericBridge* bridge,SpanIndexType span,GirderIndexType gdr,rptChapter* pChapter,IEAFDisplayUnits* pDisplayUnits);
   void ReportEffectiveFlangeWidth_ExteriorGirder_SingleTopFlange(IBroker* pBroker,IGenericBridge* bridge,SpanIndexType span,GirderIndexType gdr,rptChapter* pChapter,IEAFDisplayUnits* pDisplayUnits);
   void ReportEffectiveFlangeWidth_ExteriorGirder_SingleTopFlange_Prismatic(IBroker* pBroker,IGenericBridge* bridge,SpanIndexType span,GirderIndexType gdr,rptChapter* pChapter,IEAFDisplayUnits* pDisplayUnits);
   void ReportEffectiveFlangeWidth_ExteriorGirder_SingleTopFlange_Nonprismatic(IBroker* pBroker,IGenericBridge* bridge,SpanIndexType span,GirderIndexType gdr,rptChapter* pChapter,IEAFDisplayUnits* pDisplayUnits);
   void ReportEffectiveFlangeWidth_ExteriorGirder_MultiTopFlange(IBroker* pBroker,IGenericBridge* bridge,SpanIndexType span,GirderIndexType gdr,rptChapter* pChapter,IEAFDisplayUnits* pDisplayUnits);
   void ReportEffectiveFlangeWidth_ExteriorGirder_MultiTopFlange_Prismatic(IBroker* pBroker,IGenericBridge* bridge,SpanIndexType span,GirderIndexType gdr,rptChapter* pChapter,IEAFDisplayUnits* pDisplayUnits);
   void ReportEffectiveFlangeWidth_ExteriorGirder_MultiTopFlange_Nonprismatic(IBroker* pBroker,IGenericBridge* bridge,SpanIndexType span,GirderIndexType gdr,rptChapter* pChapter,IEAFDisplayUnits* pDisplayUnits);

   void GetBeamFactory(IBroker* pBroker,SpanIndexType spanIdx,GirderIndexType gdrIdx,IBeamFactory** factory);
   bool DoUseTributaryWidth(const CBridgeDescription* pBridgeDesc);

// ISupportsErrorInfo
public:
	STDMETHOD(InterfaceSupportsErrorInfo)(REFIID riid);

// IPGSuperEffectiveFlangeWidthTool
public:
   STDMETHOD(put_UseTributaryWidth)(VARIANT_BOOL bUseTribWidth);
   STDMETHOD(get_UseTributaryWidth)(VARIANT_BOOL* bUseTribWidth);

// IEffectiveFlangeWidthTool
public:
	STDMETHOD(TributaryFlangeWidth)(/*[in]*/IGenericBridge* bridge,/*[in]*/SpanIndexType span,/*[in]*/GirderIndexType gdr,/*[in]*/double location, /*[out,retval]*/double *tribFlangeWidth);
	STDMETHOD(TributaryFlangeWidthEx)(/*[in]*/IGenericBridge* bridge,/*[in]*/SpanIndexType span,/*[in]*/GirderIndexType gdr,/*[in]*/double location, /*[out]*/double *twLeft, /*[out]*/double *twRight, /*[out]*/double *tribFlangeWidth);
	STDMETHOD(EffectiveFlangeWidth)(/*[in]*/IGenericBridge* bridge,/*[in]*/SpanIndexType span,/*[in]*/GirderIndexType gdr,/*[in]*/double location, /*[out,retval]*/double *effFlangeWidth);
   STDMETHOD(EffectiveFlangeWidthEx)(/*[in]*/IGenericBridge* bridge,/*[in]*/SpanIndexType span,/*[in]*/GirderIndexType gdr,/*[in]*/double location, /*[out,retval]*/IEffectiveFlangeWidthDetails** details);

// IReportEffectiveFlangeWidth
public:
   virtual void ReportEffectiveFlangeWidth(IBroker* pBroker,IGenericBridge* bridge,SpanIndexType span,GirderIndexType gdr,rptChapter* pChapter,IEAFDisplayUnits* pDisplayUnits);
};

#endif //__EFFECTIVEFLANGEWIDTHTOOL_H_
