///////////////////////////////////////////////////////////////////////
// PGSuper - Prestressed Girder SUPERstructure Design and Analysis
// Copyright � 1999-2013  Washington State Department of Transportation
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

// VoidedSlab2DistFactorEngineer.h : Declaration of the CVoidedSlabFactorEngineer

#ifndef __VoidedSlab2DistFactorEngineer_H_
#define __VoidedSlab2DistFactorEngineer_H_

#include "resource.h"       // main symbols
#include "DistFactorEngineerImpl.h"
#include <Plugins\CLSID.h>

struct VOIDEDSLAB_J_SOLID
{
   Float64 A;
   Float64 Ip;
};

struct VOIDEDSLAB_J_VOID
{
   Float64 Ao;
   typedef std::pair<Float64,Float64> Element; // first = s, second = t
   std::vector<Element> Elements;
   Float64 S_over_T; // Sum of s/t for all the elements
};

struct VOIDEDSLAB_LLDFDETAILS : public BASE_LLDFDETAILS
{
   Float64 L;
   Float64 I;
   Float64 b;
   Float64 d;
   Float64 leftDe;
   Float64 rightDe;
   Float64 J;
   Float64 PossionRatio;
   pgsTypes::AdjacentTransverseConnectivity TransverseConnectivity;

   VOIDEDSLAB_J_SOLID Jsolid;
   VOIDEDSLAB_J_VOID  Jvoid;

   IndexType nVoids;
};

/////////////////////////////////////////////////////////////////////////////
// CVoidedSlab2DistFactorEngineer
class ATL_NO_VTABLE CVoidedSlab2DistFactorEngineer : 
   public CComObjectRootEx<CComSingleThreadModel>,
//   public CComRefCountTracer<CVoidedSlab2DistFactorEngineer,CComObjectRootEx<CComSingleThreadModel> >,
   public CComCoClass<CVoidedSlab2DistFactorEngineer, &CLSID_VoidedSlab2DistFactorEngineer>,
   public CDistFactorEngineerImpl<VOIDEDSLAB_LLDFDETAILS>
{
public:
	CVoidedSlab2DistFactorEngineer()
	{
	}

   HRESULT FinalConstruct();

DECLARE_REGISTRY_RESOURCEID(IDR_VOIDEDSLAB2DISTFACTORENGINEER)

BEGIN_COM_MAP(CVoidedSlab2DistFactorEngineer)
   COM_INTERFACE_ENTRY(IDistFactorEngineer)
   COM_INTERFACE_ENTRY(IInitialize)
END_COM_MAP()

// IInitialize
public:
//   virtual void SetBroker(IBroker* pBroker,StatusGroupIDType statusGroupID);

public:
   // IDistFactorEngineer
//   virtual void SetBroker(IBroker* pBroker,StatusGroupIDType statusGroupID);
//   virtual double GetMomentDF(SpanIndexType span,GirderIndexType gdr);
//   virtual double GetNegMomentDF(PierIndexType pier,GirderIndexType gdr);
//   virtual double GetShearDF(SpanIndexType span,GirderIndexType gdr);
//   virtual double GetReactionDF(PierIndexType pier,GirderIndexType gdr);
   virtual void BuildReport(SpanIndexType span,GirderIndexType gdr,rptChapter* pChapter,IEAFDisplayUnits* pDisplayUnits);
   virtual std::_tstring GetComputationDescription(SpanIndexType span,GirderIndexType gdr,const std::_tstring& libraryEntryName,pgsTypes::SupportedDeckType decktype, pgsTypes::AdjacentTransverseConnectivity connect);

private:
   
   lrfdLiveLoadDistributionFactorBase* GetLLDFParameters(SpanIndexType spanOrPier,GirderIndexType gdr,DFParam dfType,Float64 fcgdr,VOIDEDSLAB_LLDFDETAILS* plldf);

   void ReportMoment(rptParagraph* pPara,VOIDEDSLAB_LLDFDETAILS& lldf,lrfdILiveLoadDistributionFactor::DFResult& gM1,lrfdILiveLoadDistributionFactor::DFResult& gM2,double gM,bool bSIUnits,IEAFDisplayUnits* pDisplayUnits);
   void ReportShear(rptParagraph* pPara,VOIDEDSLAB_LLDFDETAILS& lldf,lrfdILiveLoadDistributionFactor::DFResult& gV1,lrfdILiveLoadDistributionFactor::DFResult& gV2,double gV,bool bSIUnits,IEAFDisplayUnits* pDisplayUnits);
};

#endif //__VoidedSlab2DistFactorEngineer_H_
