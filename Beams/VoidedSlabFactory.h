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

// VoidedSlabFactory.h : Declaration of the CVoidedSlabFactory

#ifndef __VOIDEDSLABFACTORY_H_
#define __VOIDEDSLABFACTORY_H_

#include "resource.h"       // main symbols
#include "IFace\BeamFactory.h"
#include "IBeamFactory.h" // CLSID

#include <vector>

/////////////////////////////////////////////////////////////////////////////
// CVoidedSlabFactory
class ATL_NO_VTABLE CVoidedSlabFactory : 
   public CComObjectRootEx<CComSingleThreadModel>,
   public CComCoClass<CVoidedSlabFactory, &CLSID_VoidedSlabFactory>,
   public IBeamFactory
{
public:
	CVoidedSlabFactory()
	{
	}

   HRESULT FinalConstruct();

DECLARE_REGISTRY_RESOURCEID(IDR_VOIDEDSLABFACTORY)
DECLARE_CLASSFACTORY_SINGLETON(CVoidedSlabFactory)

BEGIN_COM_MAP(CVoidedSlabFactory)
   COM_INTERFACE_ENTRY(IBeamFactory)
END_COM_MAP()

public:
   // IBeamFactory
   virtual void CreateGirderSection(IBroker* pBroker,long agentID,SpanIndexType spanIdx,GirderIndexType gdrIdx,const IBeamFactory::Dimensions& dimensions,IGirderSection** ppSection);
   virtual void CreateGirderProfile(IBroker* pBroker,long agentID,SpanIndexType spanIdx,GirderIndexType gdrIdx,const IBeamFactory::Dimensions& dimensions,IShape** ppShape);
   virtual void LayoutGirderLine(IBroker* pBroker,long agentID,SpanIndexType spanIdx,GirderIndexType gdrIdx,ISuperstructureMember* ssmbr);
   virtual void LayoutSectionChangePointsOfInterest(IBroker* pBroker,SpanIndexType span,GirderIndexType gdr,pgsPoiMgr* pPoiMgr);
   virtual void CreateDistFactorEngineer(IBroker* pBroker,long agentID,const pgsTypes::SupportedDeckType* pDeckType, const pgsTypes::AdjacentTransverseConnectivity* pConnect,IDistFactorEngineer** ppEng);
   virtual void CreatePsLossEngineer(IBroker* pBroker,long agentID,SpanIndexType spanIdx,GirderIndexType gdrIdx,IPsLossEngineer** ppEng);
   virtual void CreateStrandMover(const IBeamFactory::Dimensions& dimensions, 
                                  IBeamFactory::BeamFace endTopFace, double endTopLimit, IBeamFactory::BeamFace endBottomFace, double endBottomLimit, 
                                  IBeamFactory::BeamFace hpTopFace, double hpTopLimit, IBeamFactory::BeamFace hpBottomFace, double hpBottomLimit, 
                                  double endIncrement, double hpIncrement, IStrandMover** strandMover);
   virtual std::vector<std::string> GetDimensionNames();
   virtual std::vector<const unitLength*> GetDimensionUnits(bool bSIUnits);
   virtual std::vector<double> GetDefaultDimensions();
   virtual bool ValidateDimensions(const IBeamFactory::Dimensions& dimensions,bool bSIUnits,std::string* strErrMsg);
   virtual void SaveSectionDimensions(sysIStructuredSave* pSave,const IBeamFactory::Dimensions& dimensions);
   virtual IBeamFactory::Dimensions LoadSectionDimensions(sysIStructuredLoad* pLoad);
   virtual bool IsPrismatic(IBroker* pBroker,SpanIndexType spanIdx,GirderIndexType gdrIdx);
   virtual Float64 GetVolume(IBroker* pBroker,SpanIndexType spanIdx,GirderIndexType gdrIdx);
   virtual Float64 GetSurfaceArea(IBroker* pBroker,SpanIndexType spanIdx,GirderIndexType gdrIdx,bool bReduceForPoorlyVentilatedVoids);
   virtual std::string GetImage();
   virtual std::string GetSlabDimensionsImage(pgsTypes::SupportedDeckType deckType);
   virtual std::string GetPositiveMomentCapacitySchematicImage(pgsTypes::SupportedDeckType deckType);
   virtual std::string GetNegativeMomentCapacitySchematicImage(pgsTypes::SupportedDeckType deckType);
   virtual std::string GetShearDimensionsSchematicImage(pgsTypes::SupportedDeckType deckType);
   virtual std::string GetInteriorGirderEffectiveFlangeWidthImage(IBroker* pBroker,pgsTypes::SupportedDeckType deckType);
   virtual std::string GetExteriorGirderEffectiveFlangeWidthImage(IBroker* pBroker,pgsTypes::SupportedDeckType deckType);
   virtual CLSID GetCLSID();
   virtual CLSID GetFamilyCLSID();
   virtual std::string GetGirderFamilyName();
   virtual std::string GetPublisher();
   virtual HINSTANCE GetResourceInstance();
   virtual LPCTSTR GetImageResourceName();
   virtual HICON GetIcon();
   virtual pgsTypes::SupportedDeckTypes GetSupportedDeckTypes(pgsTypes::SupportedBeamSpacing sbs);
   virtual pgsTypes::SupportedBeamSpacings GetSupportedBeamSpacings();
   virtual void GetAllowableSpacingRange(const IBeamFactory::Dimensions& dimensions,pgsTypes::SupportedDeckType sdt, pgsTypes::SupportedBeamSpacing sbs, double* minSpacing, double* maxSpacing);
   virtual long GetNumberOfWebs(const IBeamFactory::Dimensions& dimensions);
   virtual Float64 GetBeamHeight(const IBeamFactory::Dimensions& dimensions,pgsTypes::MemberEndType endType);
   virtual Float64 GetBeamWidth(const IBeamFactory::Dimensions& dimensions,pgsTypes::MemberEndType endType);
   virtual bool IsShearKey(const IBeamFactory::Dimensions& dimensions, pgsTypes::SupportedBeamSpacing spacingType);
   virtual void GetShearKeyAreas(const IBeamFactory::Dimensions& dimensions, pgsTypes::SupportedBeamSpacing spacingType,Float64* uniformArea, Float64* areaPerJoint);

private:
   std::vector<std::string> m_DimNames;
   std::vector<double> m_DefaultDims;
   std::vector<const unitLength*> m_DimUnits[2];

   void GetDimensions(const IBeamFactory::Dimensions& dimensions,
                      double& H,double& W,double& D,double& S,long& N,double& J);

   double GetDimension(const IBeamFactory::Dimensions& dimensions,const std::string& name);
};

#endif //__VOIDEDSLABFACTORY_H_
