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

// MultiWeb2Factory.h : Declaration of the CMultiWeb2Factory

#ifndef __MULTIWEB2FACTORY_H_
#define __MULTIWEB2FACTORY_H_

#include "resource.h"       // main symbols
#include "IFace\BeamFactory.h"
#include "IBeamFactory.h" // CLSID

#include <vector>

/////////////////////////////////////////////////////////////////////////////
// CMultiWeb2Factory
class ATL_NO_VTABLE CMultiWeb2Factory : 
   public CComObjectRootEx<CComSingleThreadModel>,
   public CComCoClass<CMultiWeb2Factory, &CLSID_MultiWeb2Factory>,
   public IBeamFactory
{
public:
	CMultiWeb2Factory()
	{
	}

   HRESULT FinalConstruct();

DECLARE_REGISTRY_RESOURCEID(IDR_MULTIWEB2FACTORY)
DECLARE_CLASSFACTORY_SINGLETON(CMultiWeb2Factory)

BEGIN_COM_MAP(CMultiWeb2Factory)
   COM_INTERFACE_ENTRY(IBeamFactory)
END_COM_MAP()

public:
   // IBeamFactory
   virtual void CreateGirderSection(IBroker* pBroker,StatusGroupIDType statusGroupID,SpanIndexType spanIdx,GirderIndexType gdrIdx,const IBeamFactory::Dimensions& dimensions,IGirderSection** ppSection);
   virtual void CreateGirderProfile(IBroker* pBroker,StatusGroupIDType statusGroupID,SpanIndexType spanIdx,GirderIndexType gdrIdx,const IBeamFactory::Dimensions& dimensions,IShape** ppShape);
   virtual void LayoutGirderLine(IBroker* pBroker,StatusGroupIDType statusGroupID,SpanIndexType spanIdx,GirderIndexType gdrIdx,ISuperstructureMember* ssmbr);
   virtual void LayoutSectionChangePointsOfInterest(IBroker* pBroker,SpanIndexType span,GirderIndexType gdr,pgsPoiMgr* pPoiMgr);
   virtual void CreateDistFactorEngineer(IBroker* pBroker,StatusGroupIDType statusGroupID,const pgsTypes::SupportedDeckType* pDeckType, const pgsTypes::AdjacentTransverseConnectivity* pConnect,IDistFactorEngineer** ppEng);
   virtual void CreatePsLossEngineer(IBroker* pBroker,StatusGroupIDType statusGroupID,SpanIndexType spanIdx,GirderIndexType gdrIdx,IPsLossEngineer** ppEng);
   virtual void CreateStrandMover(const IBeamFactory::Dimensions& dimensions, 
                                  IBeamFactory::BeamFace endTopFace, Float64 endTopLimit, IBeamFactory::BeamFace endBottomFace, Float64 endBottomLimit, 
                                  IBeamFactory::BeamFace hpTopFace, Float64 hpTopLimit, IBeamFactory::BeamFace hpBottomFace, Float64 hpBottomLimit, 
                                  Float64 endIncrement, Float64 hpIncrement, IStrandMover** strandMover);
   virtual std::vector<std::_tstring> GetDimensionNames();
   virtual std::vector<const unitLength*> GetDimensionUnits(bool bSIUnits);
   virtual std::vector<Float64> GetDefaultDimensions();
   virtual bool ValidateDimensions(const IBeamFactory::Dimensions& dimensions,bool bSIUnits,std::_tstring* strErrMsg);
   virtual void SaveSectionDimensions(sysIStructuredSave* pSave,const IBeamFactory::Dimensions& dimensions);
   virtual IBeamFactory::Dimensions LoadSectionDimensions(sysIStructuredLoad* pLoad);
   virtual bool IsPrismatic(IBroker* pBroker,SpanIndexType spanIdx,GirderIndexType gdrIdx);
   virtual Float64 GetVolume(IBroker* pBroker,SpanIndexType spanIdx,GirderIndexType gdrIdx);
   virtual Float64 GetSurfaceArea(IBroker* pBroker,SpanIndexType spanIdx,GirderIndexType gdrIdx,bool bReduceForPoorlyVentilatedVoids);
   virtual std::_tstring GetImage();
   virtual std::_tstring GetSlabDimensionsImage(pgsTypes::SupportedDeckType deckType);
   virtual std::_tstring GetPositiveMomentCapacitySchematicImage(pgsTypes::SupportedDeckType deckType);
   virtual std::_tstring GetNegativeMomentCapacitySchematicImage(pgsTypes::SupportedDeckType deckType);
   virtual std::_tstring GetShearDimensionsSchematicImage(pgsTypes::SupportedDeckType deckType);
   virtual std::_tstring GetInteriorGirderEffectiveFlangeWidthImage(IBroker* pBroker,pgsTypes::SupportedDeckType deckType);
   virtual std::_tstring GetExteriorGirderEffectiveFlangeWidthImage(IBroker* pBroker,pgsTypes::SupportedDeckType deckType);
   virtual CLSID GetCLSID();
   virtual CLSID GetFamilyCLSID();
   virtual std::_tstring GetGirderFamilyName();
   virtual std::_tstring GetPublisher();
   virtual HINSTANCE GetResourceInstance();
   virtual LPCTSTR GetImageResourceName();
   virtual HICON GetIcon();
   virtual pgsTypes::SupportedDeckTypes GetSupportedDeckTypes(pgsTypes::SupportedBeamSpacing sbs);
   virtual pgsTypes::SupportedBeamSpacings GetSupportedBeamSpacings();
   virtual void GetAllowableSpacingRange(const IBeamFactory::Dimensions& dimensions,pgsTypes::SupportedDeckType sdt, pgsTypes::SupportedBeamSpacing sbs, Float64* minSpacing, Float64* maxSpacing);
   virtual WebIndexType GetNumberOfWebs(const IBeamFactory::Dimensions& dimensions);
   virtual Float64 GetBeamHeight(const IBeamFactory::Dimensions& dimensions,pgsTypes::MemberEndType endType);
   virtual Float64 GetBeamWidth(const IBeamFactory::Dimensions& dimensions,pgsTypes::MemberEndType endType);
   virtual bool IsShearKey(const IBeamFactory::Dimensions& dimensions, pgsTypes::SupportedBeamSpacing spacingType);
   virtual void GetShearKeyAreas(const IBeamFactory::Dimensions& dimensions, pgsTypes::SupportedBeamSpacing spacingType,Float64* uniformArea, Float64* areaPerJoint);

private:
   std::vector<std::_tstring> m_DimNames;
   std::vector<Float64> m_DefaultDims;
   std::vector<const unitLength*> m_DimUnits[2];

   void GetDimensions(const IBeamFactory::Dimensions& dimensions,
                      Float64& h1,Float64& h2,Float64& h3,
                      Float64& t1,Float64& t2,Float64& t3,
                      Float64& f1,
                      Float64& c1,Float64& c2,
                      Float64& w2,Float64& wmin,Float64& wmax);

   Float64 GetDimension(const IBeamFactory::Dimensions& dimensions,const std::_tstring& name);
};

#endif //__MULTIWEB2FACTORY_H_
