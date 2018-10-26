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

// BoxBeamFactory.cpp : Implementation of CBoxBeamFactoryImpl
#include "stdafx.h"
#include <Plugins\Beams.h>

#include "BeamFamilyCLSID.h"

#include "BoxBeamFactoryImpl.h"
#include "BoxBeamDistFactorEngineer.h"
#include "UBeamDistFactorEngineer.h"
#include "PsBeamLossEngineer.h"
#include "StrandMoverImpl.h"
#include <BridgeModeling\PrismaticGirderProfile.h>
#include <GeomModel\PrecastBeam.h>
#include <MathEx.h>
#include <sstream>
#include <algorithm>

#include <IFace\Project.h>
#include <IFace\Bridge.h>
#include <PgsExt\BridgeDescription.h>

#include <IFace\StatusCenter.h>
#include <PgsExt\StatusItem.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


/////////////////////////////////////////////////////////////////////////////
// CBoxBeamFactoryImpl
void CBoxBeamFactoryImpl::CreateGirderProfile(IBroker* pBroker,long agentID,SpanIndexType spanIdx,GirderIndexType gdrIdx,const IBeamFactory::Dimensions& dimensions,IShape** ppShape)
{
   GET_IFACE2(pBroker,IBridge,pBridge);
   Float64 length = pBridge->GetGirderLength(spanIdx,gdrIdx);

   double H1 = GetDimension(dimensions,"H1");
   double H2 = GetDimension(dimensions,"H2");
   double H3 = GetDimension(dimensions,"H3");

   Float64 height = H1 + H2 + H3;

   CComPtr<IRectangle> rect;
   rect.CoCreateInstance(CLSID_Rect);
   rect->put_Height(height);
   rect->put_Width(length);

   CComQIPtr<IXYPosition> position(rect);
   CComPtr<IPoint2d> topLeft;
   position->get_LocatorPoint(lpTopLeft,&topLeft);
   topLeft->Move(0,0);
   position->put_LocatorPoint(lpTopLeft,topLeft);

   rect->QueryInterface(ppShape);
}

void CBoxBeamFactoryImpl::LayoutGirderLine(IBroker* pBroker,long agentID,SpanIndexType spanIdx,GirderIndexType gdrIdx,ISuperstructureMember* ssmbr)
{
   CComPtr<IBoxBeamEndBlockSegment> segment;
   HRESULT hr = segment.CoCreateInstance(CLSID_BoxBeamEndBlockSegment);

   // Length of the segments will be measured fractionally
   ssmbr->put_AreSegmentLengthsFractional(VARIANT_TRUE);
   segment->put_Length(-1.0);

   // Build up the beam shape
   GET_IFACE2(pBroker,ILibrary,pLib);
   GET_IFACE2(pBroker,IGirderData, pGirderData);
   GET_IFACE2(pBroker,IBridgeDescription,pIBridgeDesc);

   const CBridgeDescription* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();

   const GirderLibraryEntry* pGdrEntry = pBridgeDesc->GetSpan(spanIdx)->GetGirderTypes()->GetGirderLibraryEntry(gdrIdx);
   const GirderLibraryEntry::Dimensions& dimensions = pGdrEntry->GetDimensions();

   Float64 endBlockLength = GetDimension(dimensions,"EndBlockLength");
   segment->put_EndBlockLength(etStart,endBlockLength);
   segment->put_EndBlockLength(etEnd,endBlockLength);

   CComPtr<IGirderSection> gdrsection;
   CreateGirderSection(pBroker,agentID,spanIdx,gdrIdx,dimensions,&gdrsection);

   CComQIPtr<IBoxBeamSection> section(gdrsection);
   segment->putref_BeamSection(section);

   // Beam materials
   GET_IFACE2(pBroker,IBridgeMaterial,pMaterial);
   CComPtr<IMaterial> material;
   material.CoCreateInstance(CLSID_Material);
   material->put_E(pMaterial->GetEcGdr(spanIdx,gdrIdx));
   material->put_Density(pMaterial->GetStrDensityGdr(spanIdx,gdrIdx));
   segment->putref_Material(material);

   ssmbr->AddSegment(segment);
}

void CBoxBeamFactoryImpl::LayoutSectionChangePointsOfInterest(IBroker* pBroker,SpanIndexType span,GirderIndexType gdr,pgsPoiMgr* pPoiMgr)
{
   // This is a prismatic beam so only add section change POI at the start and end of the beam
   GET_IFACE2(pBroker,IBridge,pBridge);
   Float64 gdrLength = pBridge->GetGirderLength(span,gdr);

   pgsPointOfInterest poiStart(pgsTypes::CastingYard,span,gdr,0.00,POI_SECTCHANGE | POI_TABULAR | POI_GRAPHICAL);
   pgsPointOfInterest poiEnd(pgsTypes::CastingYard,span,gdr,gdrLength,POI_SECTCHANGE | POI_TABULAR | POI_GRAPHICAL);
   pPoiMgr->AddPointOfInterest(poiStart);
   pPoiMgr->AddPointOfInterest(poiEnd);

   // move bridge site poi to the start/end bearing
   std::set<pgsTypes::Stage> stages;
   stages.insert(pgsTypes::GirderPlacement);
   stages.insert(pgsTypes::TemporaryStrandRemoval);
   stages.insert(pgsTypes::BridgeSite1);
   stages.insert(pgsTypes::BridgeSite2);
   stages.insert(pgsTypes::BridgeSite3);
   
   Float64 start_length = pBridge->GetGirderStartConnectionLength(span,gdr);
   Float64 end_length   = pBridge->GetGirderEndConnectionLength(span,gdr);
   poiStart.SetDistFromStart(start_length);
   poiEnd.SetDistFromStart(gdrLength-end_length);

poiStart.RemoveStage(pgsTypes::CastingYard);
   poiStart.AddStages(stages);

   poiEnd.RemoveStage(pgsTypes::CastingYard);
   poiEnd.AddStages(stages);

   pPoiMgr->AddPointOfInterest(poiStart);
   pPoiMgr->AddPointOfInterest(poiEnd);

   // put section breaks just on either side of the end blocks/void interface
   GET_IFACE2(pBroker,IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CSpanData* pSpan = pBridgeDesc->GetSpan(span);
   const GirderLibraryEntry* pGirderLib = pSpan->GetGirderTypes()->GetGirderLibraryEntry(gdr);
   Float64 endBlockLength = pGirderLib->GetDimension("EndBlockLength");

   if ( !IsZero(endBlockLength) )
   {
      Float64 delta = 1.5*pPoiMgr->GetTolerance();

      stages.insert(pgsTypes::CastingYard);

      std::set<pgsTypes::Stage> ebStages;
      if ( start_length < endBlockLength )
         ebStages.insert(pgsTypes::CastingYard); // end block is after brg... only add to cy stage
      else
         ebStages = stages; // all stages

      pPoiMgr->AddPointOfInterest( pgsPointOfInterest(stages,span,gdr,endBlockLength,       POI_SECTCHANGE | POI_TABULAR | POI_GRAPHICAL) );
      pPoiMgr->AddPointOfInterest( pgsPointOfInterest(stages,span,gdr,endBlockLength+delta,                  POI_TABULAR | POI_GRAPHICAL) );

      if ( end_length < endBlockLength )
         ebStages.insert(pgsTypes::CastingYard);
      else
         ebStages = stages;

      pPoiMgr->AddPointOfInterest( pgsPointOfInterest(stages,span,gdr,gdrLength - (endBlockLength+delta),                  POI_TABULAR | POI_GRAPHICAL) );
      pPoiMgr->AddPointOfInterest( pgsPointOfInterest(stages,span,gdr,gdrLength - endBlockLength,         POI_SECTCHANGE | POI_TABULAR | POI_GRAPHICAL) );
   }
}

void CBoxBeamFactoryImpl::CreateDistFactorEngineer(IBroker* pBroker,long agentID,
                                               const pgsTypes::SupportedDeckType* pDeckType, const pgsTypes::AdjacentTransverseConnectivity* pConnect,
                                               IDistFactorEngineer** ppEng)
{
   GET_IFACE2(pBroker,IBridge,pBridge);

   // use passed value if not null
   pgsTypes::SupportedDeckType deckType = (pDeckType!=NULL) ? *pDeckType : pBridge->GetDeckType();
   
   if ( deckType == pgsTypes::sdtCompositeOverlay || deckType == pgsTypes::sdtNone )
   {
      CComObject<CBoxBeamDistFactorEngineer>* pEngineer;
      CComObject<CBoxBeamDistFactorEngineer>::CreateInstance(&pEngineer);
      pEngineer->SetBroker(pBroker,agentID);
      (*ppEng) = pEngineer;
      (*ppEng)->AddRef();
   }
   else
   {
      // this is a type b section... type b's are the same as type c's which are U-beams
      ATLASSERT( deckType == pgsTypes::sdtCompositeCIP || deckType == pgsTypes::sdtCompositeSIP );

      CComObject<CUBeamDistFactorEngineer>* pEngineer;
      CComObject<CUBeamDistFactorEngineer>::CreateInstance(&pEngineer);
      pEngineer->Init(true); // this is a type b cross section
      pEngineer->SetBroker(pBroker,agentID);
      (*ppEng) = pEngineer;
      (*ppEng)->AddRef();
   }
}

void CBoxBeamFactoryImpl::CreatePsLossEngineer(IBroker* pBroker,long agentID,SpanIndexType spanIdx,GirderIndexType gdrIdx,IPsLossEngineer** ppEng)
{
    CComObject<CPsBeamLossEngineer>* pEngineer;
    CComObject<CPsBeamLossEngineer>::CreateInstance(&pEngineer);
    pEngineer->Init(CPsLossEngineer::BoxBeam);
    pEngineer->SetBroker(pBroker,agentID);
    (*ppEng) = pEngineer;
    (*ppEng)->AddRef();
}


std::vector<std::string> CBoxBeamFactoryImpl::GetDimensionNames()
{
   return m_DimNames;
}

std::vector<double> CBoxBeamFactoryImpl::GetDefaultDimensions()
{
   return m_DefaultDims;
}

std::vector<const unitLength*> CBoxBeamFactoryImpl::GetDimensionUnits(bool bSIUnits)
{
   return m_DimUnits[ bSIUnits ? 0 : 1 ];
}

bool CBoxBeamFactoryImpl::IsPrismatic(IBroker* pBroker,SpanIndexType spanIdx,GirderIndexType gdrIdx)
{
   GET_IFACE2(pBroker,IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const GirderLibraryEntry* pGdrEntry = pBridgeDesc->GetSpan(spanIdx)->GetGirderTypes()->GetGirderLibraryEntry(gdrIdx);
   const GirderLibraryEntry::Dimensions& dimensions = pGdrEntry->GetDimensions();
   Float64 endBlockLength = GetDimension(dimensions,"EndBlockLength");

   return IsZero(endBlockLength) ? true : false;
}

Float64 CBoxBeamFactoryImpl::GetVolume(IBroker* pBroker,SpanIndexType spanIdx,GirderIndexType gdrIdx)
{
   GET_IFACE2(pBroker,ISectProp2,pSectProp2);
   GET_IFACE2(pBroker,IPointOfInterest,pPOI);

   std::vector<pgsPointOfInterest> vPOI = pPOI->GetPointsOfInterest(pgsTypes::CastingYard,spanIdx,gdrIdx,POI_SECTCHANGE);
   ATLASSERT( 2 <= vPOI.size() );
   Float64 V = 0;
   std::vector<pgsPointOfInterest>::iterator iter = vPOI.begin();
   pgsPointOfInterest prev_poi = *iter;
   Float64 prev_area = pSectProp2->GetAg(pgsTypes::CastingYard,prev_poi);
   iter++;

   for ( ; iter != vPOI.end(); iter++ )
   {
      pgsPointOfInterest poi = *iter;
      Float64 area = pSectProp2->GetAg(pgsTypes::CastingYard,poi);

      Float64 avg_area = (prev_area + area)/2;
      V += avg_area*(poi.GetDistFromStart() - prev_poi.GetDistFromStart());

      prev_poi = poi;
      prev_area = area;
   }

   return V;
}


CLSID CBoxBeamFactoryImpl::GetFamilyCLSID()
{
   return CLSID_BoxBeamFamily;
}

std::string CBoxBeamFactoryImpl::GetGirderFamilyName()
{
   USES_CONVERSION;
   LPOLESTR pszUserType;
   OleRegGetUserType(GetFamilyCLSID(),USERCLASSTYPE_SHORT,&pszUserType);
   return std::string( OLE2A(pszUserType) );
}

std::string CBoxBeamFactoryImpl::GetPublisher()
{
   return std::string("WSDOT");
}

HINSTANCE CBoxBeamFactoryImpl::GetResourceInstance()
{
   return _Module.GetResourceInstance();
}

double CBoxBeamFactoryImpl::GetDimension(const IBeamFactory::Dimensions& dimensions,
                                        const std::string& name)
{
   IBeamFactory::Dimensions::const_iterator iter;
   for ( iter = dimensions.begin(); iter != dimensions.end(); iter++ )
   {
      const IBeamFactory::Dimension& dim = *iter;
      if ( name == dim.first )
         return dim.second;
   }

   ATLASSERT(false); // should never get here
   return -99999;
}

pgsTypes::SupportedDeckTypes CBoxBeamFactoryImpl::GetSupportedDeckTypes(pgsTypes::SupportedBeamSpacing sbs)
{
   pgsTypes::SupportedDeckTypes sdt;
   switch( sbs )
   {
   case pgsTypes::sbsUniform:
   case pgsTypes::sbsGeneral:
      sdt.push_back(pgsTypes::sdtCompositeCIP);
      sdt.push_back(pgsTypes::sdtCompositeSIP);
      break;

   case pgsTypes::sbsUniformAdjacent:
   case pgsTypes::sbsGeneralAdjacent:
      sdt.push_back(pgsTypes::sdtCompositeOverlay);
      sdt.push_back(pgsTypes::sdtNone);
      break;

   default:
      ATLASSERT(false);
   }

   return sdt;
}

pgsTypes::SupportedBeamSpacings CBoxBeamFactoryImpl::GetSupportedBeamSpacings()
{
   pgsTypes::SupportedBeamSpacings sbs;
   sbs.push_back(pgsTypes::sbsUniform);
   sbs.push_back(pgsTypes::sbsGeneral);
   sbs.push_back(pgsTypes::sbsUniformAdjacent);
   sbs.push_back(pgsTypes::sbsGeneralAdjacent);

   return sbs;
}


long CBoxBeamFactoryImpl::GetNumberOfWebs(const IBeamFactory::Dimensions& dimensions)
{
   return 2;
}


Float64 CBoxBeamFactoryImpl::GetBeamHeight(const IBeamFactory::Dimensions& dimensions,pgsTypes::MemberEndType endType)
{
   double H1 = GetDimension(dimensions,"H1");
   double H2 = GetDimension(dimensions,"H2");
   double H3 = GetDimension(dimensions,"H3");

   return H1 + H2 + H3;
}



std::string CBoxBeamFactoryImpl::GetSlabDimensionsImage(pgsTypes::SupportedDeckType deckType)
{
   std::string strImage;
   switch(deckType)
   {
   case pgsTypes::sdtCompositeCIP:
      strImage = "BoxBeam_Composite_CIP.gif";
      break;

   case pgsTypes::sdtCompositeSIP:
      strImage = "BoxBeam_Composite_SIP.gif";
      break;

   case pgsTypes::sdtCompositeOverlay:
      strImage = "BoxBeam_Composite.gif";
      break;

   case pgsTypes::sdtNone:
      strImage = "BoxBeam_Noncomposite.gif";
      break;

   default:
      ATLASSERT(false); // shouldn't get here
      break;
   };

   return strImage;
}

std::string CBoxBeamFactoryImpl::GetPositiveMomentCapacitySchematicImage(pgsTypes::SupportedDeckType deckType)
{
   std::string strImage;
   switch(deckType)
   {
   case pgsTypes::sdtCompositeCIP:
   case pgsTypes::sdtCompositeSIP:
      strImage =  "+Mn_SpreadBoxBeam_Composite.gif";
      break;

   case pgsTypes::sdtCompositeOverlay:
      strImage =  "+Mn_BoxBeam_Composite.gif";
      break;

   case pgsTypes::sdtNone:
      strImage =  "+Mn_BoxBeam_Noncomposite.gif";
      break;

   default:
      ATLASSERT(false); // shouldn't get here
      break;
   };

   return strImage;
}

std::string CBoxBeamFactoryImpl::GetNegativeMomentCapacitySchematicImage(pgsTypes::SupportedDeckType deckType)
{
   std::string strImage;
   switch(deckType)
   {
   case pgsTypes::sdtCompositeCIP:
   case pgsTypes::sdtCompositeSIP:
      strImage =  "-Mn_SpreadBoxBeam_Composite.gif";
      break;

   case pgsTypes::sdtCompositeOverlay:
      strImage =  "-Mn_BoxBeam_Composite.gif";
      break;

   case pgsTypes::sdtNone:
      strImage =  "-Mn_BoxBeam_Noncomposite.gif";
      break;

   default:
      ATLASSERT(false); // shouldn't get here
      break;
   };

   return strImage;
}

std::string CBoxBeamFactoryImpl::GetShearDimensionsSchematicImage(pgsTypes::SupportedDeckType deckType)
{
   std::string strImage;
   switch(deckType)
   {
   case pgsTypes::sdtCompositeCIP:
   case pgsTypes::sdtCompositeSIP:
      strImage =  "Vn_SpreadBoxBeam_Composite.gif";
      break;

   case pgsTypes::sdtCompositeOverlay:
      strImage =  "Vn_BoxBeam_Composite.gif";
      break;

   case pgsTypes::sdtNone:
      strImage =  "Vn_BoxBeam_Noncomposite.gif";
      break;

   default:
      ATLASSERT(false); // shouldn't get here
      break;
   };

   return strImage;
}

std::string CBoxBeamFactoryImpl::GetInteriorGirderEffectiveFlangeWidthImage(IBroker* pBroker,pgsTypes::SupportedDeckType deckType)
{
   GET_IFACE2(pBroker, ILibrary,       pLib);
   GET_IFACE2(pBroker, ISpecification, pSpec);
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( pSpec->GetSpecification().c_str() );

   std::string strImage;
   switch(deckType)
   {
   case pgsTypes::sdtCompositeCIP:
   case pgsTypes::sdtCompositeSIP:
      if ( pSpecEntry->GetEffectiveFlangeWidthMethod() == pgsTypes::efwmTribWidth || lrfdVersionMgr::FourthEditionWith2008Interims <= pSpecEntry->GetSpecificationType() )
      {
         strImage =  "SpreadBoxBeam_Effective_Flange_Width_Interior_Girder_2008.gif";
      }
      else
      {
         strImage =  "SpreadBoxBeam_Effective_Flange_Width_Interior_Girder.gif";
      }
      break;

   case pgsTypes::sdtCompositeOverlay:
      if ( pSpecEntry->GetEffectiveFlangeWidthMethod() == pgsTypes::efwmTribWidth || lrfdVersionMgr::FourthEditionWith2008Interims <= pSpecEntry->GetSpecificationType() )
      {
         strImage =  "BoxBeam_Effective_Flange_Width_Interior_Girder_2008.gif";
      }
      else
      {
         strImage =  "BoxBeam_Effective_Flange_Width_Interior_Girder.gif";
      }
      break;

   case pgsTypes::sdtNone:
   default:
      ATLASSERT(false); // shouldn't get here
      break;
   };

   return strImage;
}

std::string CBoxBeamFactoryImpl::GetExteriorGirderEffectiveFlangeWidthImage(IBroker* pBroker,pgsTypes::SupportedDeckType deckType)
{
   GET_IFACE2(pBroker, ILibrary,       pLib);
   GET_IFACE2(pBroker, ISpecification, pSpec);
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( pSpec->GetSpecification().c_str() );

   std::string strImage;
   switch(deckType)
   {
   case pgsTypes::sdtCompositeCIP:
   case pgsTypes::sdtCompositeSIP:
      if ( pSpecEntry->GetEffectiveFlangeWidthMethod() == pgsTypes::efwmTribWidth || lrfdVersionMgr::FourthEditionWith2008Interims <= pSpecEntry->GetSpecificationType() )
      {
         strImage =  "SpreadBoxBeam_Effective_Flange_Width_Exterior_Girder_2008.gif";
      }
      else
      {
         strImage =  "SpreadBoxBeam_Effective_Flange_Width_Exterior_Girder.gif";
      }
      break;

   case pgsTypes::sdtCompositeOverlay:
      if ( pSpecEntry->GetEffectiveFlangeWidthMethod() == pgsTypes::efwmTribWidth || lrfdVersionMgr::FourthEditionWith2008Interims <= pSpecEntry->GetSpecificationType() )
      {
         strImage =  "BoxBeam_Effective_Flange_Width_Exterior_Girder_2008.gif";
      }
      else
      {
         strImage =  "BoxBeam_Effective_Flange_Width_Exterior_Girder.gif";
      }
      break;

   case pgsTypes::sdtNone:
   default:
      ATLASSERT(false); // shouldn't get here
      break;
   };

   return strImage;
}
