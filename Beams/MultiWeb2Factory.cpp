///////////////////////////////////////////////////////////////////////
// PGSuper - Prestressed Girder SUPERstructure Design and Analysis
// Copyright (C) 1999  Washington State Department of Transportation
//                     Bridge and Structures Office
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

// MultiWeb2Factory.cpp : Implementation of CMultiWeb2Factory
#include "stdafx.h"
#include <Plugins\Beams.h>
#include "BeamFamilyCLSID.h"
#include "MultiWeb2Factory.h"
#include "MultiWebDistFactorEngineer.h"
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
// CMultiWeb2Factory
HRESULT CMultiWeb2Factory::FinalConstruct()
{
   // Initialize with default values... This are not necessarily valid dimensions
   m_DimNames.push_back("C1");
   m_DimNames.push_back("C2");
   m_DimNames.push_back("H1");
   m_DimNames.push_back("H2");
   m_DimNames.push_back("H3");
   m_DimNames.push_back("T1");
   m_DimNames.push_back("T2");
   m_DimNames.push_back("T3");
   m_DimNames.push_back("F1");
   m_DimNames.push_back("W2");
   m_DimNames.push_back("Wmin");
   m_DimNames.push_back("Wmax");

   m_DefaultDims.push_back(::ConvertToSysUnits( 0.0,unitMeasure::Inch)); // C1
   m_DefaultDims.push_back(::ConvertToSysUnits( 0.0,unitMeasure::Inch)); // C2
   m_DefaultDims.push_back(::ConvertToSysUnits(27.0,unitMeasure::Inch)); // H1
   m_DefaultDims.push_back(::ConvertToSysUnits( 3.0,unitMeasure::Inch)); // H2
   m_DefaultDims.push_back(::ConvertToSysUnits( 6.0,unitMeasure::Inch)); // H3
   m_DefaultDims.push_back(::ConvertToSysUnits( 0.0,unitMeasure::Inch)); // T1
   m_DefaultDims.push_back(::ConvertToSysUnits( 6.5,unitMeasure::Inch)); // T2
   m_DefaultDims.push_back(::ConvertToSysUnits( 1.5,unitMeasure::Inch)); // T3
   m_DefaultDims.push_back(::ConvertToSysUnits( 3.0,unitMeasure::Inch)); // F1
   m_DefaultDims.push_back(::ConvertToSysUnits(48.0,unitMeasure::Inch)); // W2
   m_DefaultDims.push_back(::ConvertToSysUnits( 2.5,unitMeasure::Feet)); // Wmin
   m_DefaultDims.push_back(::ConvertToSysUnits( 3.0,unitMeasure::Feet)); // Wmax


   // SI Units
   m_DimUnits[0].push_back(&unitMeasure::Millimeter); // C1
   m_DimUnits[0].push_back(&unitMeasure::Millimeter); // C2
   m_DimUnits[0].push_back(&unitMeasure::Millimeter); // H1
   m_DimUnits[0].push_back(&unitMeasure::Millimeter); // H2
   m_DimUnits[0].push_back(&unitMeasure::Millimeter); // H3
   m_DimUnits[0].push_back(&unitMeasure::Millimeter); // T1
   m_DimUnits[0].push_back(&unitMeasure::Millimeter); // T2
   m_DimUnits[0].push_back(&unitMeasure::Millimeter); // T3
   m_DimUnits[0].push_back(&unitMeasure::Millimeter); // F1
   m_DimUnits[0].push_back(&unitMeasure::Millimeter); // W2
   m_DimUnits[0].push_back(&unitMeasure::Meter); // Wmin
   m_DimUnits[0].push_back(&unitMeasure::Meter); // Wmax

   // US Units
   m_DimUnits[1].push_back(&unitMeasure::Inch); // C1
   m_DimUnits[1].push_back(&unitMeasure::Inch); // C2
   m_DimUnits[1].push_back(&unitMeasure::Inch); // H1
   m_DimUnits[1].push_back(&unitMeasure::Inch); // H2
   m_DimUnits[1].push_back(&unitMeasure::Inch); // H3
   m_DimUnits[1].push_back(&unitMeasure::Inch); // T1
   m_DimUnits[1].push_back(&unitMeasure::Inch); // T2
   m_DimUnits[1].push_back(&unitMeasure::Inch); // T3
   m_DimUnits[1].push_back(&unitMeasure::Inch); // F1
   m_DimUnits[1].push_back(&unitMeasure::Inch); // W2
   m_DimUnits[1].push_back(&unitMeasure::Feet); // Wmin
   m_DimUnits[1].push_back(&unitMeasure::Feet); // Wmax
   

   return S_OK;
}

void CMultiWeb2Factory::CreateGirderSection(IBroker* pBroker,long agentID,SpanIndexType spanIdx,GirderIndexType gdrIdx,const IBeamFactory::Dimensions& dimensions,IGirderSection** ppSection)
{
   CComPtr<IMultiWebSection2> gdrsection;
   gdrsection.CoCreateInstance(CLSID_MultiWebSection2);
   CComPtr<IMultiWeb2> beam;
   gdrsection->get_Beam(&beam);

   double c1,c2;
   double h1,h2,h3;
   double w2,wmin,wmax;
   double t1,t2,t3;
   double f1;
   GetDimensions(dimensions,h1,h2,h3,t1,t2,t3,f1,c1,c2,w2,wmin,wmax);

   beam->put_C1(c1);
   beam->put_C2(c2);
   beam->put_H1(h1);
   beam->put_H2(h2);
   beam->put_H3(h3);
   beam->put_T1(t1);
   beam->put_T2(t2);
   beam->put_T3(t3);
   beam->put_T4(0);
   beam->put_T5(0);
   beam->put_F1(f1);
   beam->put_F2(0);
   beam->put_W2(w2);
   beam->put_WebCount(2);

   // figure out the web spacing, w2, based on the girder spacing
   double w1;
   if ( pBroker == NULL || spanIdx == INVALID_INDEX || gdrIdx == INVALID_INDEX )
   {
      // just use the max
      w1 = wmax;
   }
   else
   {
#pragma Reminder("UPDATE: Assuming uniform spacing")
      // uniform spacing is required for this type of girder so maybe this is ok

      // use raw input here because requesting it from the bridge will cause an infite loop.
      // bridge agent calls this during validation
      GET_IFACE2(pBroker,IBridgeDescription,pIBridgeDesc);
      const CBridgeDescription* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
      ATLASSERT(pBridgeDesc->GetGirderSpacingType() == pgsTypes::sbsConstantAdjacent);
      double spacing = pBridgeDesc->GetGirderSpacing();;

      double top_flange_max = 2*(wmax + t1+t2+t3) + w2;
      double top_flange_min = 2*(wmin + t1+t2+t3) + w2;

      // if this is a fixed width section, then set the spacing equal to the width
      if ( IsEqual(top_flange_max,top_flange_min) )
         spacing = top_flange_max;

      w1 = (spacing - 2*(t1+t2+t3) - w2)/2;
   }
   beam->put_W1(w1);

   gdrsection.QueryInterface(ppSection);
}

void CMultiWeb2Factory::CreateGirderProfile(IBroker* pBroker,long agentID,SpanIndexType spanIdx,GirderIndexType gdrIdx,const IBeamFactory::Dimensions& dimensions,IShape** ppShape)
{
   GET_IFACE2(pBroker,IBridge,pBridge);
   Float64 length = pBridge->GetGirderLength(spanIdx,gdrIdx);

   double c1,c2;
   double h1,h2,h3;
   double w2,wmin,wmax;
   double t1,t2,t3;
   double f1;
   GetDimensions(dimensions,h1,h2,h3,t1,t2,t3,f1,c1,c2,w2,wmin,wmax);

   Float64 height = h1 + h2 + h3;

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

void CMultiWeb2Factory::LayoutGirderLine(IBroker* pBroker,long agentID,SpanIndexType spanIdx,GirderIndexType gdrIdx,ISuperstructureMember* ssmbr)
{
   CComPtr<IPrismaticSegment> segment;
   segment.CoCreateInstance(CLSID_PrismaticSegment);

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

   CComPtr<IGirderSection> gdrsection;
   CreateGirderSection(pBroker,agentID,spanIdx,gdrIdx,dimensions,&gdrsection);
   CComQIPtr<IShape> shape(gdrsection);
   segment->putref_Shape(shape);

   // Beam materials
   GET_IFACE2(pBroker,IBridgeMaterial,pMaterial);
   CComPtr<IMaterial> material;
   material.CoCreateInstance(CLSID_Material);
   material->put_E(pMaterial->GetEcGdr(spanIdx,gdrIdx));
   material->put_Density(pMaterial->GetStrDensityGdr(spanIdx,gdrIdx));
   segment->putref_Material(material);

   ssmbr->AddSegment(segment);
}

void CMultiWeb2Factory::LayoutSectionChangePointsOfInterest(IBroker* pBroker,SpanIndexType span,GirderIndexType gdr,pgsPoiMgr* pPoiMgr)
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
}

void CMultiWeb2Factory::CreateDistFactorEngineer(IBroker* pBroker,long agentID,const pgsTypes::SupportedDeckType* pDeckType, const pgsTypes::AdjacentTransverseConnectivity* pConnect,IDistFactorEngineer** ppEng)
{
   GET_IFACE2(pBroker,IBridge,pBridge);
   
   CComObject<CMultiWebDistFactorEngineer>* pEngineer;
   CComObject<CMultiWebDistFactorEngineer>::CreateInstance(&pEngineer);
   pEngineer->SetBroker(pBroker,agentID);
   (*ppEng) = pEngineer;
   (*ppEng)->AddRef();
}

void CMultiWeb2Factory::CreatePsLossEngineer(IBroker* pBroker,long agentID,SpanIndexType spanIdx,GirderIndexType gdrIdx,IPsLossEngineer** ppEng)
{
    CComObject<CPsBeamLossEngineer>* pEngineer;
    CComObject<CPsBeamLossEngineer>::CreateInstance(&pEngineer);
    pEngineer->Init(CPsLossEngineer::IBeam);
    pEngineer->SetBroker(pBroker,agentID);
    (*ppEng) = pEngineer;
    (*ppEng)->AddRef();
}

void CMultiWeb2Factory::CreateStrandMover(const IBeamFactory::Dimensions& dimensions, 
                                  IBeamFactory::BeamFace endTopFace, double endTopLimit, IBeamFactory::BeamFace endBottomFace, double endBottomLimit, 
                                  IBeamFactory::BeamFace hpTopFace, double hpTopLimit, IBeamFactory::BeamFace hpBottomFace, double hpBottomLimit, 
                                  double endIncrement, double hpIncrement, IStrandMover** strandMover)
{
   double h1,h2,h3;
   double c1,c2;
   double w2,wmin,wmax;
   double t1,t2,t3;
   double f1;
   GetDimensions(dimensions,h1,h2,h3,t1,t2,t3,f1,c1,c2,w2,wmin,wmax);

   HRESULT hr = S_OK;

   CComObject<CStrandMoverImpl>* pStrandMover;
   CComObject<CStrandMoverImpl>::CreateInstance(&pStrandMover);

   CComPtr<IStrandMover> sm = pStrandMover;

   double width = t2;
   double depth = h1 + h2 + h3;

   CComPtr<IRectangle> lft_harp_rect, rgt_harp_rect;
   hr = lft_harp_rect.CoCreateInstance(CLSID_Rect);
   ATLASSERT (SUCCEEDED(hr));
   hr = rgt_harp_rect.CoCreateInstance(CLSID_Rect);
   ATLASSERT (SUCCEEDED(hr));

   lft_harp_rect->put_Width(width);
   lft_harp_rect->put_Height(depth);
   rgt_harp_rect->put_Width(width);
   rgt_harp_rect->put_Height(depth);

   double hook_offset = w2/2.0 + t2/2.0 + t3;

   CComPtr<IPoint2d> lft_hook, rgt_hook;
   lft_hook.CoCreateInstance(CLSID_Point2d);
   rgt_hook.CoCreateInstance(CLSID_Point2d);

   lft_hook->Move(-hook_offset, depth/2.0);
   rgt_hook->Move( hook_offset, depth/2.0);

   lft_harp_rect->putref_HookPoint(lft_hook);
   rgt_harp_rect->putref_HookPoint(rgt_hook);

   CComPtr<IShape> lft_shape, rgt_shape;
   lft_harp_rect->get_Shape(&lft_shape);
   rgt_harp_rect->get_Shape(&rgt_shape);

   CComQIPtr<IConfigureStrandMover> configurer(sm);
   hr = configurer->AddRegion(lft_shape, 0.0);
   ATLASSERT (SUCCEEDED(hr));
   hr = configurer->AddRegion(rgt_shape, 0.0);
   ATLASSERT (SUCCEEDED(hr));

   // set vertical offset bounds and increments
   double hptb = hpTopFace==IBeamFactory::BeamBottom ? hpTopLimit : depth-hpTopLimit;
   double hpbb = hpBottomFace==IBeamFactory::BeamBottom ? hpBottomLimit : depth-hpBottomLimit;
   double endtb = endTopFace==IBeamFactory::BeamBottom ? endTopLimit : depth-endTopLimit;
   double endbb = endBottomFace==IBeamFactory::BeamBottom ? endBottomLimit : depth-endBottomLimit;

   hr = configurer->SetHarpedStrandOffsetBounds(depth, hptb, hpbb, endtb, endbb, endIncrement, hpIncrement);
   ATLASSERT (SUCCEEDED(hr));

   hr = sm.CopyTo(strandMover);
   ATLASSERT (SUCCEEDED(hr));
}

std::vector<std::string> CMultiWeb2Factory::GetDimensionNames()
{
   return m_DimNames;
}

std::vector<double> CMultiWeb2Factory::GetDefaultDimensions()
{
   return m_DefaultDims;
}

std::vector<const unitLength*> CMultiWeb2Factory::GetDimensionUnits(bool bSIUnits)
{
   return m_DimUnits[ bSIUnits ? 0 : 1 ];
}

bool CMultiWeb2Factory::ValidateDimensions(const IBeamFactory::Dimensions& dimensions,bool bSIUnits,std::string* strErrMsg)
{
   double h1,h2,h3;
   double c1,c2;
   double w2,wmin,wmax;
   double t1,t2,t3;
   double f1;
   GetDimensions(dimensions,h1,h2,h3,t1,t2,t3,f1,c1,c2,w2,wmin,wmax);

 // C1
 // C2
 // H1
 // H2
 // H3
 // T1
 // T2
 // T3
 // F1
 // W2
 // Wmin
 // Wmax

   // values that must be postive
   if ( h1 <= 0.0 )
   {
      std::ostringstream os;
      os << "H1 must be greater than 0.0"<< std::ends;
      *strErrMsg = os.str();
      return false;
   }

   if ( h3 <= 0.0 )
   {
      std::ostringstream os;
      os << "H3 must be greater than 0.0"<< std::ends;
      *strErrMsg = os.str();
      return false;
   }

   if ( t2 <= 0.0 )
   {
      std::ostringstream os;
      os << "T2 must be greater than 0.0"<< std::ends;
      *strErrMsg = os.str();
      return false;
   }

   if ( wmin <= 0.0 )
   {
      std::ostringstream os;
      os << "Wmin must be greater than 0.0"<< std::ends;
      *strErrMsg = os.str();
      return false;
   }

   if ( wmax <= 0.0 )
   {
      std::ostringstream os;
      os << "Wmax must be greater than 0.0"<< std::ends;
      *strErrMsg = os.str();
      return false;
   }

   // values that cant be negative
   if ( f1 < 0.0 )
   {
      std::ostringstream os;
      os << "F1 must be zero or greater"<< std::ends;
      *strErrMsg = os.str();
      return false;
   }

   if ( h2 < 0.0 )
   {
      std::ostringstream os;
      os << "H2 must be zero or greater"<< std::ends;
      *strErrMsg = os.str();
      return false;
   }

   if ( c1 < 0.0 )
   {
      std::ostringstream os;
      os << "C1 must be zero or greater"<< std::ends;
      *strErrMsg = os.str();
      return false;
   }

   if ( c2 < 0.0 )
   {
      std::ostringstream os;
      os << "C2 must be zero or greater"<< std::ends;
      *strErrMsg = os.str();
      return false;
   }

   if ( t1 < 0.0 )
   {
      std::ostringstream os;
      os << "T1 must be zero or greater"<< std::ends;
      *strErrMsg = os.str();
      return false;
   }

   if ( t3 < 0.0 )
   {
      std::ostringstream os;
      os << "T3 must be zero or greater"<< std::ends;
      *strErrMsg = os.str();
      return false;
   }

   // relations
   if ( wmin < f1 )
   {
      std::ostringstream os;
      os << "Wmin must be greater than F1"<< std::ends;
      *strErrMsg = os.str();
      return false;
   }

   if ( w2 < 2*f1 )
   {
      std::ostringstream os;
      os << "W2 must be greater than 2*F1"<< std::ends;
      *strErrMsg = os.str();
      return false;
   }

   if ( wmax < wmin )
   {
      std::ostringstream os;
      os << "Wmax must be greater than Wmin"<< std::ends;
      *strErrMsg = os.str();
      return false;
   }

   if ( c1 > t2/2.0 )
   {
      std::ostringstream os;
      os << "T2 must be greater than 2 * C1"<< std::ends;
      *strErrMsg = os.str();
      return false;
   }

   if ( c2 > h3 )
   {
      std::ostringstream os;
      os << "C2 must be less than H3"<< std::ends;
      *strErrMsg = os.str();
      return false;
   }

   return true;
}

void CMultiWeb2Factory::SaveSectionDimensions(sysIStructuredSave* pSave,const IBeamFactory::Dimensions& dimensions)
{
   std::vector<std::string>::iterator iter;
   pSave->BeginUnit("MultiWeb2Dimensions",1.0);
   for ( iter = m_DimNames.begin(); iter != m_DimNames.end(); iter++ )
   {
      std::string name = *iter;
      Float64 value = GetDimension(dimensions,name);
      pSave->Property(name.c_str(),value);
   }
   pSave->EndUnit();
}

IBeamFactory::Dimensions CMultiWeb2Factory::LoadSectionDimensions(sysIStructuredLoad* pLoad)
{
   Float64 parent_version = pLoad->GetVersion();

   IBeamFactory::Dimensions dimensions;
   std::vector<std::string>::iterator iter;

   Float64 dimVersion = 1.0;
   if ( 14 <= parent_version )
   {
      if ( pLoad->BeginUnit("MultiWeb2Dimensions") )
         dimVersion = pLoad->GetVersion();
      else
         THROW_LOAD(InvalidFileFormat,pLoad);
   }

   for ( iter = m_DimNames.begin(); iter != m_DimNames.end(); iter++ )
   {
      std::string name = *iter;
      Float64 value;
      if ( !pLoad->Property(name.c_str(),&value) )
      {
         // failed to read dimension value...
         
         if ( dimVersion < 2 && parent_version < 3.0 && name == "C1" )
         {
            value = 0.0; // set the default value
         }
         else
         {
            THROW_LOAD(InvalidFileFormat,pLoad);
         }
      }
      dimensions.push_back( std::make_pair(name,value) );
   }

   if ( 14 <= parent_version && !pLoad->EndUnit() )
      THROW_LOAD(InvalidFileFormat,pLoad);

   return dimensions;
}

bool CMultiWeb2Factory::IsPrismatic(IBroker* pBroker,SpanIndexType spanIdx,GirderIndexType gdrIdx)
{
   return true;
}

Float64 CMultiWeb2Factory::GetVolume(IBroker* pBroker,SpanIndexType spanIdx,GirderIndexType gdrIdx)
{
   GET_IFACE2(pBroker,ISectProp2,pSectProp2);
   Float64 area = pSectProp2->GetAg(pgsTypes::CastingYard,pgsPointOfInterest(spanIdx,gdrIdx,0.00));
   
   GET_IFACE2(pBroker,IBridge,pBridge);
   Float64 Lg = pBridge->GetGirderLength(spanIdx,gdrIdx);

   Float64 volume = area*Lg;

   return volume;
}

Float64 CMultiWeb2Factory::GetSurfaceArea(IBroker* pBroker,SpanIndexType spanIdx,GirderIndexType gdrIdx,bool bReduceForPoorlyVentilatedVoids)
{
   GET_IFACE2(pBroker,ISectProp2,pSectProp2);
   Float64 perimeter = pSectProp2->GetPerimeter(pgsPointOfInterest(spanIdx,gdrIdx,0.00));
   
   GET_IFACE2(pBroker,IBridge,pBridge);
   Float64 Lg = pBridge->GetGirderLength(spanIdx,gdrIdx);

   Float64 surface_area = perimeter*Lg;

   return surface_area;
}

std::string CMultiWeb2Factory::GetImage()
{
   return std::string("MultiWeb2.gif");
}

std::string CMultiWeb2Factory::GetSlabDimensionsImage(pgsTypes::SupportedDeckType deckType)
{
   std::string strImage;

   switch(deckType)
   {
   case pgsTypes::sdtCompositeOverlay:
      strImage =  "DoubleTee_Composite.gif";
      break;

   case pgsTypes::sdtNone:
      strImage =  "DoubleTee_Noncomposite.gif";
      break;

   default:
      ATLASSERT(false); // shouldn't get here
      break;
   };

   return strImage;
}

std::string CMultiWeb2Factory::GetPositiveMomentCapacitySchematicImage(pgsTypes::SupportedDeckType deckType)
{
   std::string strImage;

   switch(deckType)
   {
   case pgsTypes::sdtCompositeOverlay:
      strImage =  "+Mn_DoubleTee_Composite.gif";
      break;

   case pgsTypes::sdtNone:
      strImage =  "+Mn_DoubleTee_Noncomposite.gif";
      break;

   default:
      ATLASSERT(false); // shouldn't get here
      break;
   };

   return strImage;
}

std::string CMultiWeb2Factory::GetNegativeMomentCapacitySchematicImage(pgsTypes::SupportedDeckType deckType)
{
   std::string strImage;

   switch(deckType)
   {
   case pgsTypes::sdtCompositeOverlay:
      strImage =  "-Mn_DoubleTee_Composite.gif";
      break;

   case pgsTypes::sdtNone:
      strImage =  "-Mn_DoubleTee_Noncomposite.gif";
      break;

   default:
      ATLASSERT(false); // shouldn't get here
      break;
   };

   return strImage;
}

std::string CMultiWeb2Factory::GetShearDimensionsSchematicImage(pgsTypes::SupportedDeckType deckType)
{
   std::string strImage;

   switch(deckType)
   {
   case pgsTypes::sdtCompositeOverlay:
      strImage =  "Vn_DoubleTee_Composite.gif";
      break;

   case pgsTypes::sdtNone:
      strImage =  "Vn_DoubleTee_Noncomposite.gif";
      break;

   default:
      ATLASSERT(false); // shouldn't get here
      break;
   };

   return strImage;
}

std::string CMultiWeb2Factory::GetInteriorGirderEffectiveFlangeWidthImage(IBroker* pBroker,pgsTypes::SupportedDeckType deckType)
{
   GET_IFACE2(pBroker, ILibrary,       pLib);
   GET_IFACE2(pBroker, ISpecification, pSpec);
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( pSpec->GetSpecification().c_str() );
   if ( pSpecEntry->GetEffectiveFlangeWidthMethod() == pgsTypes::efwmTribWidth || lrfdVersionMgr::FourthEditionWith2008Interims <= pSpecEntry->GetSpecificationType() )
   {
      return "DoubleTee_Effective_Flange_Width_Interior_Girder_2008.gif";
   }
   else
   {
      return "DoubleTee_Effective_Flange_Width_Interior_Girder.gif";
   }
}

std::string CMultiWeb2Factory::GetExteriorGirderEffectiveFlangeWidthImage(IBroker* pBroker,pgsTypes::SupportedDeckType deckType)
{
   GET_IFACE2(pBroker, ILibrary,       pLib);
   GET_IFACE2(pBroker, ISpecification, pSpec);
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( pSpec->GetSpecification().c_str() );
   if ( pSpecEntry->GetEffectiveFlangeWidthMethod() == pgsTypes::efwmTribWidth || lrfdVersionMgr::FourthEditionWith2008Interims <= pSpecEntry->GetSpecificationType() )
   {
      return "DoubleTee_Effective_Flange_Width_Exterior_Girder_2008.gif";
   }
   else
   {
      return "DoubleTee_Effective_Flange_Width_Exterior_Girder.gif";
   }
}

CLSID CMultiWeb2Factory::GetCLSID()
{
   return CLSID_MultiWeb2Factory;
}

CLSID CMultiWeb2Factory::GetFamilyCLSID()
{
   return CLSID_RibbedBeamFamily;
}

std::string CMultiWeb2Factory::GetGirderFamilyName()
{
   USES_CONVERSION;
   LPOLESTR pszUserType;
   OleRegGetUserType(GetFamilyCLSID(),USERCLASSTYPE_SHORT,&pszUserType);
   return std::string( OLE2A(pszUserType) );
}

std::string CMultiWeb2Factory::GetPublisher()
{
   return std::string("WSDOT");
}

HINSTANCE CMultiWeb2Factory::GetResourceInstance()
{
   return _Module.GetResourceInstance();
}

LPCTSTR CMultiWeb2Factory::GetImageResourceName()
{
   return _T("MultiWeb2");
}

HICON  CMultiWeb2Factory::GetIcon() 
{
   AFX_MANAGE_STATE(AfxGetStaticModuleState());

   return ::LoadIcon(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_MULTIWEB2) );
}

void CMultiWeb2Factory::GetDimensions(const IBeamFactory::Dimensions& dimensions,
                                      double& h1,double& h2,double& h3,
                                      double& t1,double& t2,double& t3,
                                      double& f1,
                                      double& c1,double& c2,
                                      double& w2,double& wmin,double& wmax)
{
   c1 = GetDimension(dimensions,"C1");
   c2 = GetDimension(dimensions,"C2");
   h1 = GetDimension(dimensions,"H1");
   h2 = GetDimension(dimensions,"H2");
   h3 = GetDimension(dimensions,"H3");
   t1 = GetDimension(dimensions,"T1");
   t2 = GetDimension(dimensions,"T2");
   t3 = GetDimension(dimensions,"T3");
   f1 = GetDimension(dimensions,"F1");
   w2 = GetDimension(dimensions,"W2");
   wmin = GetDimension(dimensions,"Wmin");
   wmax = GetDimension(dimensions,"Wmax");
}

double CMultiWeb2Factory::GetDimension(const IBeamFactory::Dimensions& dimensions,const std::string& name)
{
   Dimensions::const_iterator iter;
   for ( iter = dimensions.begin(); iter != dimensions.end(); iter++ )
   {
      const Dimension& dim = *iter;
      if ( name == dim.first )
         return dim.second;
   }

   ATLASSERT(false); // should never get here
   return -99999;
}

pgsTypes::SupportedDeckTypes CMultiWeb2Factory::GetSupportedDeckTypes(pgsTypes::SupportedBeamSpacing sbs)
{
   pgsTypes::SupportedDeckTypes sdt;
   switch(sbs)
   {
   case pgsTypes::sbsConstantAdjacent:
      sdt.push_back(pgsTypes::sdtCompositeOverlay);
      sdt.push_back(pgsTypes::sdtNone);
      break;

   default:
      ATLASSERT(false);
   }
   return sdt;
}

pgsTypes::SupportedBeamSpacings CMultiWeb2Factory::GetSupportedBeamSpacings()
{
   pgsTypes::SupportedBeamSpacings sbs;
   sbs.push_back(pgsTypes::sbsConstantAdjacent);
   return sbs;
}

void CMultiWeb2Factory::GetAllowableSpacingRange(const IBeamFactory::Dimensions& dimensions,pgsTypes::SupportedDeckType sdt, 
                                               pgsTypes::SupportedBeamSpacing sbs, double* minSpacing, double* maxSpacing)
{
   *minSpacing = 0.0;
   *maxSpacing = 0.0;

   double T1 = GetDimension(dimensions,"T1");
   double T2 = GetDimension(dimensions,"T2");
   double T3 = GetDimension(dimensions,"T3");
   double W2 = GetDimension(dimensions,"W2");
   double Wmin = GetDimension(dimensions,"Wmin");
   double Wmax = GetDimension(dimensions,"Wmax");

   double mid_width = W2 + 2.0*(T1 + T2 + T3);

   double gw_min =  mid_width + 2.0 * Wmin;
   double gw_max =  mid_width + 2.0 * Wmax;

   if ( sdt == pgsTypes::sdtNone || sdt == pgsTypes::sdtCompositeOverlay )
   {
      if ( sbs == pgsTypes::sbsConstantAdjacent )
      {
         *minSpacing = gw_min;
         *maxSpacing = gw_max;
      }
      else
      {
         ATLASSERT(false); // shouldn't get here
      }
   }
   else
   {
      ATLASSERT(false); // shouldn't get here
   }
}

long CMultiWeb2Factory::GetNumberOfWebs(const IBeamFactory::Dimensions& dimensions)
{
   return 2;
}

Float64 CMultiWeb2Factory::GetBeamHeight(const IBeamFactory::Dimensions& dimensions,pgsTypes::MemberEndType endType)
{
   double H1 = GetDimension(dimensions,"H1");
   double H2 = GetDimension(dimensions,"H2");
   double H3 = GetDimension(dimensions,"H3");

   return H1 + H2 + H3;
}

Float64 CMultiWeb2Factory::GetBeamWidth(const IBeamFactory::Dimensions& dimensions,pgsTypes::MemberEndType endType)
{
   double T1 = GetDimension(dimensions,"T1");
   double T2 = GetDimension(dimensions,"T2");
   double T3 = GetDimension(dimensions,"T3");

   double W2 = GetDimension(dimensions,"W2");
   double Wmax = GetDimension(dimensions,"Wmax");

   return 2*(T1+T2+T3+Wmax) + W2;
}
