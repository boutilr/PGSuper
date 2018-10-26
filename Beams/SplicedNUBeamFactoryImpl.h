///////////////////////////////////////////////////////////////////////
// PGSuper - Prestressed Girder SUPERstructure Design and Analysis
// Copyright � 1999-2017  Washington State Department of Transportation
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

// SplicedNUBeamFactoryImpl.h : Declaration of the CSplicedNUBeamFactory
#pragma once

#include "resource.h"       // main symbols
#include "IFace\BeamFactory.h"
#include "IBeamFactory.h" // CLSID

#include <vector>

/////////////////////////////////////////////////////////////////////////////
// CSplicedNUBeamFactory
class ATL_NO_VTABLE CSplicedNUBeamFactory : 
   public CComObjectRootEx<CComSingleThreadModel>,
   public CComCoClass<CSplicedNUBeamFactory, &CLSID_SplicedNUBeamFactory>,
   public ISplicedBeamFactory
{
public:
	CSplicedNUBeamFactory()
	{
	}

   HRESULT FinalConstruct();

DECLARE_REGISTRY_RESOURCEID(IDR_SPLICEDNUBEAMFACTORY)
DECLARE_CLASSFACTORY_SINGLETON(CSplicedNUBeamFactory)

BEGIN_COM_MAP(CSplicedNUBeamFactory)
   COM_INTERFACE_ENTRY(ISplicedBeamFactory)
   COM_INTERFACE_ENTRY(IBeamFactory)
END_COM_MAP()

public:
   // IBeamFactory
   virtual void CreateGirderSection(IBroker* pBroker,StatusGroupIDType statusGroupID,const IBeamFactory::Dimensions& dimensions,Float64 overallHeight,Float64 bottomFlangeHeight,IGirderSection** ppSection) override;
   virtual void CreateGirderProfile(IBroker* pBroker,StatusGroupIDType statusGroupID,const CSegmentKey& segmentKey,const IBeamFactory::Dimensions& dimensions,IShape** ppShape) override;
   virtual void CreateSegment(IBroker* pBroker,StatusGroupIDType statusGroupID,const CSegmentKey& segmentKey,ISuperstructureMember* ssmbr) override;
   virtual void LayoutSectionChangePointsOfInterest(IBroker* pBroker,const CSegmentKey& segmentKey,pgsPoiMgr* pPoiMgr) override;
   virtual void CreateDistFactorEngineer(IBroker* pBroker,StatusGroupIDType statusGroupID,const pgsTypes::SupportedBeamSpacing* pSpacingType,const pgsTypes::SupportedDeckType* pDeckType, const pgsTypes::AdjacentTransverseConnectivity* pConnect,IDistFactorEngineer** ppEng) override;
   virtual void CreatePsLossEngineer(IBroker* pBroker,StatusGroupIDType statusGroupID,const CGirderKey& girderKey,IPsLossEngineer** ppEng) override;
   virtual void CreateStrandMover(const IBeamFactory::Dimensions& dimensions,  Float64 Hg,
                                  IBeamFactory::BeamFace endTopFace, Float64 endTopLimit, IBeamFactory::BeamFace endBottomFace, Float64 endBottomLimit, 
                                  IBeamFactory::BeamFace hpTopFace, Float64 hpTopLimit, IBeamFactory::BeamFace hpBottomFace, Float64 hpBottomLimit, 
                                  Float64 endIncrement, Float64 hpIncrement, IStrandMover** strandMover) override;
   virtual std::vector<std::_tstring> GetDimensionNames() override;
   virtual std::vector<const unitLength*> GetDimensionUnits(bool bSIUnits) override;
   virtual std::vector<Float64> GetDefaultDimensions() override;
   virtual bool ValidateDimensions(const IBeamFactory::Dimensions& dimensions,bool bSIUnits,std::_tstring* strErrMsg) override;
   virtual void SaveSectionDimensions(sysIStructuredSave* pSave,const IBeamFactory::Dimensions& dimensions) override;
   virtual IBeamFactory::Dimensions LoadSectionDimensions(sysIStructuredLoad* pLoad) override;
   virtual bool IsPrismatic(const IBeamFactory::Dimensions& dimensions) override;
   virtual bool IsSymmetric(const IBeamFactory::Dimensions& dimensions) override;
   virtual Float64 GetInternalSurfaceAreaOfVoids(IBroker* pBroker,const CSegmentKey& segmentKey) override;
   virtual std::_tstring GetImage() override;
   virtual std::_tstring GetSlabDimensionsImage(pgsTypes::SupportedDeckType deckType) override;
   virtual std::_tstring GetPositiveMomentCapacitySchematicImage(pgsTypes::SupportedDeckType deckType) override;
   virtual std::_tstring GetNegativeMomentCapacitySchematicImage(pgsTypes::SupportedDeckType deckType) override;
   virtual std::_tstring GetShearDimensionsSchematicImage(pgsTypes::SupportedDeckType deckType) override;
   virtual std::_tstring GetInteriorGirderEffectiveFlangeWidthImage(IBroker* pBroker,pgsTypes::SupportedDeckType deckType) override;
   virtual std::_tstring GetExteriorGirderEffectiveFlangeWidthImage(IBroker* pBroker,pgsTypes::SupportedDeckType deckType) override;
   virtual CLSID GetCLSID() override;
   virtual std::_tstring GetName() override;
   virtual CLSID GetFamilyCLSID() override;
   virtual std::_tstring GetGirderFamilyName() override;
   virtual std::_tstring GetPublisher() override;
   virtual std::_tstring GetPublisherContactInformation() override;
   virtual HINSTANCE GetResourceInstance() override;
   virtual LPCTSTR GetImageResourceName() override;
   virtual HICON GetIcon() override;
   virtual pgsTypes::SupportedDeckTypes GetSupportedDeckTypes(pgsTypes::SupportedBeamSpacing sbs) override;
   virtual pgsTypes::SupportedBeamSpacings GetSupportedBeamSpacings() override;
   virtual pgsTypes::SupportedDiaphragmTypes GetSupportedDiaphragms() override;
   virtual pgsTypes::SupportedDiaphragmLocationTypes GetSupportedDiaphragmLocations(pgsTypes::DiaphragmType type) override;
   virtual void GetAllowableSpacingRange(const IBeamFactory::Dimensions& dimensions,pgsTypes::SupportedDeckType sdt, pgsTypes::SupportedBeamSpacing sbs, Float64* minSpacing, Float64* maxSpacing) override;
   virtual WebIndexType GetWebCount(const IBeamFactory::Dimensions& dimensions) override;
   virtual Float64 GetBeamHeight(const IBeamFactory::Dimensions& dimensions,pgsTypes::MemberEndType endType) override;
   virtual Float64 GetBeamWidth(const IBeamFactory::Dimensions& dimensions,pgsTypes::MemberEndType endType) override;
   virtual bool IsShearKey(const IBeamFactory::Dimensions& dimensions, pgsTypes::SupportedBeamSpacing spacingType) override;
   virtual void GetShearKeyAreas(const IBeamFactory::Dimensions& dimensions, pgsTypes::SupportedBeamSpacing spacingType,Float64* uniformArea, Float64* areaPerJoint) override;
   virtual GirderIndexType GetMinimumBeamCount() override;

// ISplicedBeamFactory
   virtual bool SupportsVariableDepthSection() override;
   virtual LPCTSTR GetVariableDepthDimension() override;
   virtual std::vector<pgsTypes::SegmentVariationType> GetSupportedSegmentVariations(bool bIsVariableDepthSection) override;
   virtual bool CanBottomFlangeDepthVary() override;
   virtual LPCTSTR GetBottomFlangeDepthDimension() override;
   virtual bool SupportsEndBlocks() override;

private:
   std::vector<std::_tstring> m_DimNames;
   std::vector<Float64> m_DefaultDims;
   std::vector<const unitLength*> m_DimUnits[2];

   void GetDimensions(const IBeamFactory::Dimensions& dimensions,
                      Float64& d1,Float64& d2,Float64& d3,Float64& d4,Float64& d5,
                      Float64& r1,Float64& r2,Float64& r3,Float64& r4,
                      Float64& t,
                      Float64& w1,Float64& w2,
                      Float64& c1);

   Float64 GetDimension(const IBeamFactory::Dimensions& dimensions,const std::_tstring& name);
};
