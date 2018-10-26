///////////////////////////////////////////////////////////////////////
// PGSuper - Prestressed Girder SUPERstructure Design and Analysis
// Copyright � 1999-2016  Washington State Department of Transportation
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
#include <IFace\Constructability.h>
#include <IFace\Bridge.h>
#include <IFace\Project.h>

#include <PgsExt\BridgeDescription2.h>

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
CADimChapterBuilder::CADimChapterBuilder(bool bSelect) :
CPGSuperChapterBuilder(bSelect)
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
   CGirderReportSpecification* pGirderRptSpec = dynamic_cast<CGirderReportSpecification*>(pRptSpec);
   CComPtr<IBroker> pBroker;
   pGirderRptSpec->GetBroker(&pBroker);
   const CGirderKey& girderKey(pGirderRptSpec->GetGirderKey());

   rptChapter* pChapter = CPGSuperChapterBuilder::Build(pRptSpec,level);

   GET_IFACE2(pBroker,ILibrary, pLib );
   GET_IFACE2(pBroker,ISpecification, pSpec );
   std::_tstring spec_name = pSpec->GetSpecification();
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( spec_name.c_str() );

   GET_IFACE2_NOCHECK(pBroker,IBridge,pBridge);
   if ( !pSpecEntry->IsSlabOffsetCheckEnabled() )
   {
      rptParagraph* pPara = new rptParagraph;
      *pChapter << pPara;

      *pPara << _T("Slab Offset check disabled in Project Criteria library entry. No analysis performed.") << rptNewLine;
      return pChapter;
   }
   else if ( pBridge->GetDeckType() == pgsTypes::sdtNone )
   {
      rptParagraph* pPara = new rptParagraph;
      *pChapter << pPara;

      *pPara << _T("This bridge does not have a deck. No analysis performed.") << rptNewLine;
      return pChapter;
   }

   GET_IFACE2(pBroker,IEAFDisplayUnits,pDisplayUnits);

   INIT_UV_PROTOTYPE( rptPointOfInterest, location, pDisplayUnits->GetSpanLengthUnit(),   false );
   location.IncludeSpanAndGirder(girderKey.groupIndex == ALL_GROUPS);

   INIT_UV_PROTOTYPE( rptLengthUnitValue, dim, pDisplayUnits->GetSpanLengthUnit(), false );
   INIT_UV_PROTOTYPE( rptLengthUnitValue, comp, pDisplayUnits->GetComponentDimUnit(), false );
   INIT_UV_PROTOTYPE( rptLengthUnitValue, defl, pDisplayUnits->GetDeflectionUnit(), false );

   GET_IFACE2(pBroker,IGirderHaunch,pGdrHaunch);

   // Loop over spans in girder
   SpanIndexType startSpanIdx, endSpanIdx;
   pBridge->GetGirderGroupSpans(girderKey.groupIndex, &startSpanIdx, &endSpanIdx);

   for (SpanIndexType spanIdx=startSpanIdx; spanIdx<=endSpanIdx; spanIdx++)
   {
      CSpanKey spanKey(spanIdx, girderKey.groupIndex);

      HAUNCHDETAILS haunch_details;
      pGdrHaunch->GetHaunchDetails(spanKey,&haunch_details);

      rptParagraph* pPara = new rptParagraph;
      *pChapter << pPara;
      
      rptRcTable* pTable1 = pgsReportStyleHolder::CreateDefaultTable(10,_T("Haunch Details - Part 1"));
      *pPara << pTable1;

      //if ( segmentKey.groupIndex == ALL_SPANS )
      //{
      //   pTable1->SetColumnStyle(0,pgsReportStyleHolder::GetTableCellStyle(CB_NONE | CJ_LEFT));
      //   pTable1->SetStripeRowColumnStyle(0,pgsReportStyleHolder::GetTableStripeRowCellStyle(CB_NONE | CJ_LEFT));
      //}

      rptRcTable* pTable2 = pgsReportStyleHolder::CreateDefaultTable(10,_T("Haunch Details - Part 2"));
      *pPara << pTable2;

      //if ( segmentKey.groupIndex == ALL_SPANS )
      //{
      //   pTable2->SetColumnStyle(0,pgsReportStyleHolder::GetTableCellStyle(CB_NONE | CJ_LEFT));
      //   pTable2->SetStripeRowColumnStyle(0,pgsReportStyleHolder::GetTableStripeRowCellStyle(CB_NONE | CJ_LEFT));
      //}

      std::_tstring strSlopeTag = pDisplayUnits->GetAlignmentLengthUnit().UnitOfMeasure.UnitTag();

      (*pTable1)(0,0) << COLHDR(RPT_LFT_SUPPORT_LOCATION, rptLengthUnitTag, pDisplayUnits->GetSpanLengthUnit());
      (*pTable1)(0,1) << _T("Station");
      (*pTable1)(0,2) << COLHDR(_T("Offset"), rptLengthUnitTag, pDisplayUnits->GetSpanLengthUnit());
      (*pTable1)(0,3) << COLHDR(_T("Top") << rptNewLine << _T("Slab") << rptNewLine << _T("Elevation"), rptLengthUnitTag, pDisplayUnits->GetSpanLengthUnit());
      (*pTable1)(0,4) << COLHDR(_T("Girder") << rptNewLine << _T("Chord") << rptNewLine << _T("Elevation"),rptLengthUnitTag, pDisplayUnits->GetSpanLengthUnit());
      (*pTable1)(0,5) << COLHDR(_T("Slab") << rptNewLine << _T("Thickness"), rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit());
      (*pTable1)(0,6) << COLHDR(_T("Fillet"),rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit());
      (*pTable1)(0,7) << COLHDR(_T("D"),rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit());
      (*pTable1)(0,8) << COLHDR(_T("C"),rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit());
      (*pTable1)(0,9) << COLHDR(_T("Excess") << rptNewLine << _T("Camber"),rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit());

      (*pTable2)(0,0) << COLHDR(RPT_LFT_SUPPORT_LOCATION, rptLengthUnitTag, pDisplayUnits->GetSpanLengthUnit());
      (*pTable2)(0,1) << _T("Crown") << rptNewLine << _T("Slope") << rptNewLine << _T("(") << Sub2(_T("m"),_T("d")) << _T(")") << rptNewLine << _T("(") << strSlopeTag << _T("/") << strSlopeTag << _T(")");
      (*pTable2)(0,2) << _T("Girder") << rptNewLine << _T("Orientation") << rptNewLine << _T("(") << Sub2(_T("m"),_T("g")) << _T(")") << rptNewLine << _T("(") << strSlopeTag << _T("/") << strSlopeTag << _T(")");
      (*pTable2)(0,3)<< COLHDR(_T("Top") << rptNewLine << _T("Width"),rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit());
      (*pTable2)(0,4)<< COLHDR(_T("Profile") << rptNewLine << _T("Effect"),rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit());
      (*pTable2)(0,5)<< COLHDR(_T("Girder") << rptNewLine << _T("Orientation") << rptNewLine << _T("Effect"),rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit());
      (*pTable2)(0,6)<< COLHDR(_T("Required") << rptNewLine << _T("Slab") << rptNewLine << _T("Offset") << (_T("*")),rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit());
      (*pTable2)(0,7)<< COLHDR(_T("Top") << rptNewLine << _T("Girder") << rptNewLine << _T("Elevation") << Super(_T("**")),rptLengthUnitTag,pDisplayUnits->GetSpanLengthUnit());
      (*pTable2)(0,8)<< COLHDR(_T("Actual") << rptNewLine << _T("Depth") << Super(_T("***")),rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit());
      (*pTable2)(0,9)<< COLHDR(_T("Haunch") << rptNewLine << _T("Depth"),rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit());

      RowIndexType row1 = pTable1->GetNumberOfHeaderRows();
      RowIndexType row2 = pTable2->GetNumberOfHeaderRows();
      std::vector<SECTIONHAUNCH>::iterator haunchIter(haunch_details.Haunch.begin());
      std::vector<SECTIONHAUNCH>::iterator haunchIterEnd(haunch_details.Haunch.end());
      for ( ; haunchIter != haunchIterEnd; haunchIter++ )
      {
         SECTIONHAUNCH& haunch = *haunchIter;

         (*pTable1)(row1,0) << location.SetValue( POI_ERECTED_SEGMENT, haunch.PointOfInterest );
         (*pTable1)(row1,1) << rptRcStation(haunch.Station, &pDisplayUnits->GetStationFormat() );
         (*pTable1)(row2,2) << RPT_OFFSET(haunch.Offset,dim);

         (*pTable1)(row1,3) << dim.SetValue( haunch.ElevAlignment );
         (*pTable1)(row1,4) << dim.SetValue( haunch.ElevGirder );
         (*pTable1)(row1,5) << comp.SetValue( haunch.tSlab );
         (*pTable1)(row1,6) << comp.SetValue( haunch.Fillet );
         (*pTable1)(row1,7) << defl.SetValue( haunch.D );
         (*pTable1)(row1,8) << defl.SetValue( haunch.C );
         (*pTable1)(row1,9) << defl.SetValue( haunch.CamberEffect);

         row1++;

         (*pTable2)(row2,0) << location.SetValue( POI_ERECTED_SEGMENT, haunch.PointOfInterest );
         (*pTable2)(row2,1) << haunch.CrownSlope;
         (*pTable2)(row2,2) << haunch.GirderOrientation;
         (*pTable2)(row2,3) << comp.SetValue( haunch.Wtop );
         (*pTable2)(row2,4) << comp.SetValue( haunch.ProfileEffect );
         (*pTable2)(row2,5) << comp.SetValue( haunch.GirderOrientationEffect );
         (*pTable2)(row2,6) << comp.SetValue( haunch.RequiredHaunchDepth );
         (*pTable2)(row2,7) << dim.SetValue( haunch.ElevTopGirder);
         (*pTable2)(row2,8) << comp.SetValue( haunch.ActualHaunchDepth );
         (*pTable2)(row2,9) << comp.SetValue( haunch.ActualHaunchDepth-haunch.tSlab );

         row2++;
      }

      comp.ShowUnitTag(true);

      // table footnotes
      GET_IFACE2(pBroker,IBridgeDescription,pIBridgeDesc);
      const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
      pgsTypes::SlabOffsetType slabOffsetType = pBridgeDesc->GetSlabOffsetType();

      pPara = new rptParagraph(pgsReportStyleHolder::GetFootnoteStyle());
      *pChapter << pPara;
      *pPara << Super(_T("*")) << _T(" required slab offset from top of girder to top of slab at centerline bearing for geometric effects at this point. (Slab Thickness + Fillet + Camber Effect + Profile Effect + Girder Orientation Effect)") << rptNewLine;

      if ( slabOffsetType == pgsTypes::sotBridge )
      {
         // one _T("A") dimension for whole bridge
         Float64 A = pBridgeDesc->GetSlabOffset();
         *pPara << Super(_T("**")) << _T(" includes the effects of camber and based on Slab Offset of ") << comp.SetValue(A) << _T(".") << rptNewLine;
         *pPara << Super(_T("***")) << _T(" top of girder to top of slab based on Slab Offset of ") << comp.SetValue(A) << _T(". (Deck Elevation - Top Girder Elevation)") << rptNewLine;
      }
      else
      {
         PierIndexType startPierIdx = spanIdx;
         PierIndexType endPierIdx   = spanIdx+1;
         Float64 Astart = pIBridgeDesc->GetSlabOffset(girderKey.groupIndex, startPierIdx, girderKey.girderIndex);
         Float64 Aend   = pIBridgeDesc->GetSlabOffset(girderKey.groupIndex, endPierIdx, girderKey.girderIndex);
         *pPara << Super(_T("**")) << _T(" includes the effects of camber and based on Slab Offset at the start of the girder of ") << comp.SetValue(Astart);
         *pPara << _T(" and a Slab Offset at the end of the girder of ") << comp.SetValue(Aend) << _T(".") << rptNewLine;

         *pPara << Super(_T("***")) << _T(" actual depth from top of girder to top of slab based on Slab Offset at the start of the girder of ") << comp.SetValue(Astart);
         *pPara << _T(" and a Slab Offset at the end of the girder of ") << comp.SetValue(Aend) << _T(". (Top Slab Elevation - Top Girder Elevation)") << rptNewLine;
      }


      comp.ShowUnitTag(true);
      *pPara << _T("Required Slab Offset at intersection of centerline bearing and centerline girder (\"A\" Dimension): ") << comp.SetValue(haunch_details.RequiredSlabOffset) << rptNewLine;
      *pPara << _T("Maximum Change in Haunch Depth between Bearings: ") << comp.SetValue(haunch_details.HaunchDiff) << rptNewLine;
   }

   rptParagraph* pPara = new rptParagraph;
   *pChapter << pPara;
   *pPara << rptNewLine << rptNewLine;

   *pPara << _T("Top Slab Elevation = Elevation of the roadway surface directly above the centerline of the girder.") << rptNewLine;
   *pPara << _T("Girder Chord Elevation = Elevation of an imaginary chord paralleling the top of the undeformed girder that intersects the roadway surface above the point of bearing at the left end of the girder.") << rptNewLine;

   *pPara << _T("Profile Effect = Deck Elevation - Girder Chord Elevation") << rptNewLine;
   *pPara << rptRcImage(pgsReportStyleHolder::GetImagePath() + _T("ProfileEffect.gif"));
   *pPara << rptRcImage(pgsReportStyleHolder::GetImagePath() + _T("GirderOrientationEffect.gif"))  << rptNewLine;
   *pPara << rptRcImage(pgsReportStyleHolder::GetImagePath() + _T("GirderOrientationEffectEquation.png"))  << rptNewLine;

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
