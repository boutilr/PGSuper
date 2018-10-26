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

#include "StdAfx.h"
#include <Reporting\ADimChapterBuilder.h>
#include <Reporting\ReportNotes.h>
#include <IFace\DisplayUnits.h>
#include <IFace\Constructability.h>
#include <IFace\Bridge.h>
#include <IFace\Project.h>

#include <PgsExt\BridgeDescription.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/****************************************************************************
CLASS
   CADimChapterBuilder
****************************************************************************/


////////////////////////// PUBLIC     ///////////////////////////////////////

//======================== LIFECYCLE  =======================================
CADimChapterBuilder::CADimChapterBuilder()
{
}

//======================== OPERATORS  =======================================
//======================== OPERATIONS =======================================
LPCTSTR CADimChapterBuilder::GetName() const
{
   return TEXT("Slab Haunch Details");
}

rptChapter* CADimChapterBuilder::Build(CReportSpecification* pRptSpec,Uint16 level) const
{
   CSpanGirderReportSpecification* pSGRptSpec = dynamic_cast<CSpanGirderReportSpecification*>(pRptSpec);
   CComPtr<IBroker> pBroker;
   pSGRptSpec->GetBroker(&pBroker);
   SpanIndexType span = pSGRptSpec->GetSpan();
   GirderIndexType gdr = pSGRptSpec->GetGirder();

   rptChapter* pChapter = CPGSuperChapterBuilder::Build(pRptSpec,level);

   GET_IFACE2(pBroker,IDisplayUnits,pDisplayUnits);
   GET_IFACE2(pBroker,ILibrary, pLib );
   GET_IFACE2(pBroker,ISpecification, pSpec );
   std::string spec_name = pSpec->GetSpecification();
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( spec_name.c_str() );

   GET_IFACE2(pBroker,IBridge,pBridge);
   if ( pBridge->GetDeckType() == pgsTypes::sdtNone || !pSpecEntry->IsSlabOffsetCheckEnabled() )
   {
      rptParagraph* pPara = new rptParagraph;
      *pChapter << pPara;

      *pPara << "Slab Offset check disabled in Project Criteria library entry. No analysis performed." << rptNewLine;
      return pChapter;
   }

   INIT_UV_PROTOTYPE( rptPointOfInterest, location, pDisplayUnits->GetSpanLengthUnit(),   false );
   INIT_UV_PROTOTYPE( rptLengthUnitValue, dim, pDisplayUnits->GetSpanLengthUnit(), false );
   INIT_UV_PROTOTYPE( rptLengthUnitValue, comp, pDisplayUnits->GetComponentDimUnit(), false );

   GET_IFACE2(pBroker,IGirderHaunch,pGdrHaunch);
   GET_IFACE2(pBroker,IGirder,pGdr);

  
   HAUNCHDETAILS haunch_details;
   pGdrHaunch->GetHaunchDetails(span,gdr,&haunch_details);

   rptParagraph* pPara = new rptParagraph;
   *pChapter << pPara;
   
   rptRcTable* pTable1 = pgsReportStyleHolder::CreateDefaultTable(8,"Haunch Details - Part 1");
   *pPara << pTable1;

   rptRcTable* pTable2 = pgsReportStyleHolder::CreateDefaultTable(9,"Haunch Details - Part 2");
   *pPara << pTable2;

   std::string strSlopeTag = pDisplayUnits->GetAlignmentLengthUnit().UnitOfMeasure.UnitTag();

   (*pTable1)(0,0) << COLHDR(RPT_LFT_SUPPORT_LOCATION, rptLengthUnitTag, pDisplayUnits->GetSpanLengthUnit());
   (*pTable1)(0,1) << "Station";
   (*pTable1)(0,2) << COLHDR("Offset", rptLengthUnitTag, pDisplayUnits->GetSpanLengthUnit());
   (*pTable1)(0,3) << COLHDR("Top" << rptNewLine << "Slab" << rptNewLine << "Elevation", rptLengthUnitTag, pDisplayUnits->GetSpanLengthUnit());
   (*pTable1)(0,4) << COLHDR("Girder" << rptNewLine << "Chord" << rptNewLine << "Elevation",rptLengthUnitTag, pDisplayUnits->GetSpanLengthUnit());
   (*pTable1)(0,5) << COLHDR("Slab" << rptNewLine << "Thickness", rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit());
   (*pTable1)(0,6) << COLHDR("Fillet",rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit());
   (*pTable1)(0,7) << COLHDR("Excess" << rptNewLine << "Camber",rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit());

   (*pTable2)(0,0) << COLHDR(RPT_LFT_SUPPORT_LOCATION, rptLengthUnitTag, pDisplayUnits->GetSpanLengthUnit());
   (*pTable2)(0,1) << "Crown" << rptNewLine << "Slope" << rptNewLine << "(" << Sub2("m","d") << ")" << rptNewLine << "(" << strSlopeTag << "/" << strSlopeTag << ")";
   (*pTable2)(0,2) << "Girder" << rptNewLine << "Orientation" << rptNewLine << "(" << Sub2("m","g") << ")" << rptNewLine << "(" << strSlopeTag << "/" << strSlopeTag << ")";
   (*pTable2)(0,3)<< COLHDR("Top" << rptNewLine << "Width",rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit());
   (*pTable2)(0,4)<< COLHDR("Profile" << rptNewLine << "Effect",rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit());
   (*pTable2)(0,5)<< COLHDR("Girder" << rptNewLine << "Orientation" << rptNewLine << "Effect",rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit());
   (*pTable2)(0,6)<< COLHDR("Required" << rptNewLine << "Slab" << rptNewLine << "Offset" << ("*"),rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit());
   (*pTable2)(0,7)<< COLHDR("Top" << rptNewLine << "Girder" << rptNewLine << "Elevation" << Super("**"),rptLengthUnitTag,pDisplayUnits->GetSpanLengthUnit());
   (*pTable2)(0,8)<< COLHDR("Actual" << rptNewLine << "Depth" << Super("***"),rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit());

   Float64 end_size = pBridge->GetGirderStartConnectionLength(span,gdr);

   RowIndexType row1 = pTable1->GetNumberOfHeaderRows();
   RowIndexType row2 = pTable2->GetNumberOfHeaderRows();
   std::vector<SECTIONHAUNCH>::iterator iter;
   for ( iter = haunch_details.Haunch.begin(); iter != haunch_details.Haunch.end(); iter++ )
   {
      SECTIONHAUNCH& haunch = *iter;

      (*pTable1)(row1,0) << location.SetValue( haunch.PointOfInterest, end_size );
      (*pTable1)(row1,1) << rptRcStation(haunch.Station, &pDisplayUnits->GetStationFormat() );

      (*pTable1)(row1,2) << dim.SetValue( fabs(haunch.Offset) );
      if ( haunch.Offset < 0 )
         (*pTable1)(row1,2) << " (L)";
      else if ( 0 < haunch.Offset )
         (*pTable1)(row1,2) << " (R)";

      (*pTable1)(row1,3) << dim.SetValue( haunch.ElevAlignment );
      (*pTable1)(row1,4) << dim.SetValue( haunch.ElevGirder );
      (*pTable1)(row1,5) << comp.SetValue( haunch.tSlab );
      (*pTable1)(row1,6) << comp.SetValue( haunch.Fillet );
      (*pTable1)(row1,7) << comp.SetValue( haunch.CamberEffect);

      row1++;

      (*pTable2)(row2,0) << location.SetValue( haunch.PointOfInterest, end_size );
      (*pTable2)(row2,1) << haunch.CrownSlope;
      (*pTable2)(row2,2) << haunch.GirderOrientation;
      (*pTable2)(row2,3) << comp.SetValue( haunch.Wtop );
      (*pTable2)(row2,4) << comp.SetValue( haunch.ProfileEffect );
      (*pTable2)(row2,5) << comp.SetValue( haunch.GirderOrientationEffect );
      (*pTable2)(row2,6) << comp.SetValue( haunch.RequiredHaunchDepth );
      (*pTable2)(row2,7) << dim.SetValue( haunch.ElevTopGirder);
      (*pTable2)(row2,8) << comp.SetValue( haunch.ActualHaunchDepth );

      row2++;
   }

   comp.ShowUnitTag(true);

   // table footnotes
   GET_IFACE2(pBroker,IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   pgsTypes::SlabOffsetType slabOffsetType = pBridgeDesc->GetSlabOffsetType();

   pPara = new rptParagraph(pgsReportStyleHolder::GetFootnoteStyle());
   *pChapter << pPara;
   *pPara << Super("*") << " required slab offset from top of girder to top of slab at centerline bearing for geometric effects at this point. (Slab Thickness + Fillet + Camber Effect + Profile Effect + Girder Orientation Effect)" << rptNewLine;

   if ( slabOffsetType == pgsTypes::sotBridge )
   {
      // one "A" dimension for whole bridge
      Float64 A = pBridgeDesc->GetSlabOffset();
      *pPara << Super("**") << " includes the effects of camber and based on Slab Offset of " << comp.SetValue(A) << "." << rptNewLine;
      *pPara << Super("***") << " top of girder to top of slab based on Slab Offset of " << comp.SetValue(A) << ". (Deck Elevation - Top Girder Elevation)" << rptNewLine;
   }
   else
   {
      // "A" dim is span by span or per girder... doesn't really matter... get it by girder
      const CGirderTypes* pGirderTypes = pBridgeDesc->GetSpan(span)->GetGirderTypes();
      Float64 Astart = pGirderTypes->GetSlabOffset(gdr,pgsTypes::metStart);
      Float64 Aend   = pGirderTypes->GetSlabOffset(gdr,pgsTypes::metEnd);
      *pPara << Super("**") << " includes the effects of camber and based on Slab Offset at the start of the girder of " << comp.SetValue(Astart);
      *pPara << " and a Slab Offset at the end of the girder of " << comp.SetValue(Aend) << "." << rptNewLine;

      *pPara << Super("***") << " actual depth from top of girder to top of slab based on Slab Offset at the start of the girder of " << comp.SetValue(Astart);
      *pPara << " and a Slab Offset at the end of the girder of " << comp.SetValue(Aend) << ". (Top Slab Elevation - Top Girder Elevation)" << rptNewLine;
   }

   pPara = new rptParagraph;
   *pChapter << pPara;

   *pPara << "Required Slab Offset at intersection of centerline bearing and centerline girder (\"A\" Dimension): " << comp.SetValue(haunch_details.RequiredSlabOffset) << rptNewLine;
   *pPara << "Maximum Increase in Haunch Depth between Bearings: " << comp.SetValue(haunch_details.HaunchDiff) << rptNewLine;
   *pPara << "WSDOT stirrup and deck panel leveling bolt lengths are set based on the \"A\" dimension. If the haunch depth increases more than " << comp.SetValue(::ConvertToSysUnits(2.0,unitMeasure::Inch)) << " stirrup and/or bolt lengths may need to be adjusted on the plan sheets." << rptNewLine;

   *pPara << rptNewLine << rptNewLine;

   *pPara << "Top Slab Elevation = Elevation of the roadway surface directly above the centerline of the girder." << rptNewLine;
   *pPara << "Girder Chord Elevation = Elevation of an imaginary chord paralleling the top of the undeformed girder that intersects the roadway surface above the point of bearing at the left end of the girder." << rptNewLine;

   *pPara << "Profile Effect = Deck Elevation - Girder Chord Elevation" << rptNewLine;
   *pPara << rptRcImage(pgsReportStyleHolder::GetImagePath() + "ProfileEffect.gif");
   *pPara << rptRcImage(pgsReportStyleHolder::GetImagePath() + "GirderOrientationEffect.gif")  << rptNewLine;
   *pPara << rptRcImage(pgsReportStyleHolder::GetImagePath() + "GirderOrientationEffectEquation.gif")  << rptNewLine;

   return pChapter;
}

CChapterBuilder* CADimChapterBuilder::Clone() const
{
   return new CADimChapterBuilder;
}

//======================== ACCESS     =======================================
//======================== INQUIRY    =======================================

////////////////////////// PROTECTED  ///////////////////////////////////////

//======================== LIFECYCLE  =======================================
//======================== OPERATORS  =======================================
//======================== OPERATIONS =======================================
//======================== ACCESS     =======================================
//======================== INQUIRY    =======================================

////////////////////////// PRIVATE    ///////////////////////////////////////

//======================== LIFECYCLE  =======================================
//======================== OPERATORS  =======================================
//======================== OPERATIONS =======================================
//======================== ACCESS     =======================================
//======================== INQUERY    =======================================
