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
#include <Reporting\CombinedAxialTable.h>
#include <Reporting\CombinedMomentsTable.h>
#include <Reporting\MVRChapterBuilder.h>
#include <Reporting\ReportNotes.h>

#include <PgsExt\ReportPointOfInterest.h>
#include <IFace\Project.h>
#include <IFace\Bridge.h>
#include <EAF\EAFDisplayUnits.h>
#include <IFace\AnalysisResults.h>
#include <IFace\RatingSpecification.h>
#include <IFace\Intervals.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/****************************************************************************
CLASS
   CCombinedAxialTable
****************************************************************************/

CCombinedAxialTable::CCombinedAxialTable()
{
}

CCombinedAxialTable::CCombinedAxialTable(const CCombinedAxialTable& rOther)
{
   MakeCopy(rOther);
}

CCombinedAxialTable::~CCombinedAxialTable()
{
}

CCombinedAxialTable& CCombinedAxialTable::operator= (const CCombinedAxialTable& rOther)
{
   if( this != &rOther )
   {
      MakeAssignment(rOther);
   }

   return *this;
}

void CCombinedAxialTable::Build(IBroker* pBroker, rptChapter* pChapter,
                                         const CGirderKey& girderKey,
                                         IEAFDisplayUnits* pDisplayUnits,
                                         IntervalIndexType intervalIdx,pgsTypes::AnalysisType analysisType,
                                         bool bDesign,bool bRating) const
{
   GET_IFACE2(pBroker,IIntervals,pIntervals);
   IntervalIndexType liveLoadIntervalIdx = pIntervals->GetLiveLoadInterval();

   BuildCombinedDeadTable(pBroker, pChapter, girderKey, pDisplayUnits, intervalIdx, analysisType, bDesign, bRating);

   if (liveLoadIntervalIdx <= intervalIdx)
   {
      if (bDesign)
      {
         BuildCombinedLiveTable(pBroker, pChapter, girderKey, pDisplayUnits, analysisType, true, false);
      }

      if (bRating)
      {
         BuildCombinedLiveTable(pBroker, pChapter, girderKey, pDisplayUnits, analysisType, false, true);
      }

      if (bDesign)
      {
         BuildLimitStateTable(pBroker, pChapter, girderKey, pDisplayUnits, intervalIdx, analysisType, true, false);
      }

      if (bRating)
      {
         BuildLimitStateTable(pBroker, pChapter, girderKey, pDisplayUnits, intervalIdx, analysisType, false, true);
      }
   }
}


void CCombinedAxialTable::BuildCombinedDeadTable(IBroker* pBroker, rptChapter* pChapter,
                                         const CGirderKey& girderKey,
                                         IEAFDisplayUnits* pDisplayUnits,
                                         IntervalIndexType intervalIdx,pgsTypes::AnalysisType analysisType,
                                         bool bDesign,bool bRating) const
{
   // Build table
   INIT_UV_PROTOTYPE( rptPointOfInterest, location, pDisplayUnits->GetSpanLengthUnit(), false );
   INIT_UV_PROTOTYPE( rptForceUnitValue, axial, pDisplayUnits->GetGeneralForceUnit(), false );

   location.IncludeSpanAndGirder(girderKey.groupIndex == ALL_GROUPS);

   rptParagraph* p = new rptParagraph;
   *pChapter << p;

   rptRcTable* p_table;

   GET_IFACE2(pBroker,IBridge,pBridge);
   GroupIndexType nGroups = pBridge->GetGirderGroupCount();

   GET_IFACE2(pBroker,IIntervals,pIntervals);
   IntervalIndexType liveLoadIntervalIdx = pIntervals->GetLiveLoadInterval();


   GET_IFACE2(pBroker,ILibrary,pLib);
   GET_IFACE2(pBroker,ISpecification,pSpec);
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( pSpec->GetSpecification().c_str() );

   bool bTimeStepMethod = pSpecEntry->GetLossMethod() == LOSSES_TIME_STEP;

   GroupIndexType startGroupIdx = girderKey.groupIndex == ALL_GROUPS ? 0 : girderKey.groupIndex;
   GroupIndexType endGroupIdx   = girderKey.groupIndex == ALL_GROUPS ? nGroups-1 : startGroupIdx;

   RowIndexType row = CreateCombinedDeadLoadingTableHeading<rptForceUnitTag,unitmgtForceData>(&p_table,pBroker,CGirderKey(startGroupIdx,girderKey.girderIndex),_T("Axial"),false,bRating,intervalIdx,
                                                                                    analysisType,pDisplayUnits,pDisplayUnits->GetGeneralForceUnit());
   *p << p_table;

   if ( girderKey.groupIndex == ALL_GROUPS )
   {
      p_table->SetColumnStyle(0,rptStyleManager::GetTableCellStyle(CB_NONE | CJ_LEFT));
      p_table->SetStripeRowColumnStyle(0,rptStyleManager::GetTableStripeRowCellStyle(CB_NONE | CJ_LEFT));
   }

   RowIndexType row2 = 1;

   // Get the interface pointers we need
   GET_IFACE2(pBroker,ICombinedForces2,  pForces2);
   GET_IFACE2_NOCHECK(pBroker,ILimitStateForces2,pLsForces2); // only used if interval is after live load interval

   GET_IFACE2(pBroker,IProductForces,pProdForces);
   pgsTypes::BridgeAnalysisType minBAT = pProdForces->GetBridgeAnalysisType(analysisType,pgsTypes::Minimize);
   pgsTypes::BridgeAnalysisType maxBAT = pProdForces->GetBridgeAnalysisType(analysisType,pgsTypes::Maximize);

   // Fill up the table
   for ( GroupIndexType grpIdx = startGroupIdx; grpIdx <= endGroupIdx; grpIdx++ )
   {
      CGirderKey thisGirderKey(grpIdx,girderKey.girderIndex);

      PoiAttributeType poiRefAttribute;
      std::vector<pgsPointOfInterest> vPoi;
      GetCombinedResultsPoi(pBroker,thisGirderKey,intervalIdx,&vPoi,&poiRefAttribute);

      std::vector<Float64> dummy;
      std::vector<Float64> minServiceI, maxServiceI;
      std::vector<Float64> minDCinc, maxDCinc;
      std::vector<Float64> minDCcum, maxDCcum;
      std::vector<Float64> minDWinc, maxDWinc;
      std::vector<Float64> minDWcum, maxDWcum;
      std::vector<Float64> minDWRatinginc, maxDWRatinginc;
      std::vector<Float64> minDWRatingcum, maxDWRatingcum;
      std::vector<Float64> minCRinc, maxCRinc;
      std::vector<Float64> minCRcum, maxCRcum;
      std::vector<Float64> minSHinc, maxSHinc;
      std::vector<Float64> minSHcum, maxSHcum;
      std::vector<Float64> minREinc, maxREinc;
      std::vector<Float64> minREcum, maxREcum;
      std::vector<Float64> minPSinc, maxPSinc;
      std::vector<Float64> minPScum, maxPScum;


      maxDCinc = pForces2->GetAxial( intervalIdx, lcDC, vPoi, maxBAT, rtIncremental );
      minDCinc = pForces2->GetAxial( intervalIdx, lcDC, vPoi, minBAT, rtIncremental );
      maxDWinc = pForces2->GetAxial( intervalIdx, lcDW, vPoi, maxBAT, rtIncremental );
      minDWinc = pForces2->GetAxial( intervalIdx, lcDW, vPoi, minBAT, rtIncremental );
      maxDWRatinginc = pForces2->GetAxial( intervalIdx, lcDWRating, vPoi, maxBAT, rtIncremental );
      minDWRatinginc = pForces2->GetAxial( intervalIdx, lcDWRating, vPoi, minBAT, rtIncremental );
      maxDCcum = pForces2->GetAxial( intervalIdx, lcDC, vPoi, maxBAT, rtCumulative );
      minDCcum = pForces2->GetAxial( intervalIdx, lcDC, vPoi, minBAT, rtCumulative );
      maxDWcum = pForces2->GetAxial( intervalIdx, lcDW, vPoi, maxBAT, rtCumulative );
      minDWcum = pForces2->GetAxial( intervalIdx, lcDW, vPoi, minBAT, rtCumulative );
      maxDWRatingcum = pForces2->GetAxial( intervalIdx, lcDWRating, vPoi, maxBAT, rtCumulative );
      minDWRatingcum = pForces2->GetAxial( intervalIdx, lcDWRating, vPoi, minBAT, rtCumulative );

      if ( bTimeStepMethod )
      {
         maxCRinc = pForces2->GetAxial( intervalIdx, lcCR, vPoi, maxBAT, rtIncremental );
         minCRinc = pForces2->GetAxial( intervalIdx, lcCR, vPoi, minBAT, rtIncremental );
         maxCRcum = pForces2->GetAxial( intervalIdx, lcCR, vPoi, maxBAT, rtCumulative );
         minCRcum = pForces2->GetAxial( intervalIdx, lcCR, vPoi, minBAT, rtCumulative );

         maxSHinc = pForces2->GetAxial( intervalIdx, lcSH, vPoi, maxBAT, rtIncremental );
         minSHinc = pForces2->GetAxial( intervalIdx, lcSH, vPoi, minBAT, rtIncremental );
         maxSHcum = pForces2->GetAxial( intervalIdx, lcSH, vPoi, maxBAT, rtCumulative );
         minSHcum = pForces2->GetAxial( intervalIdx, lcSH, vPoi, minBAT, rtCumulative );

         maxREinc = pForces2->GetAxial( intervalIdx, lcRE, vPoi, maxBAT, rtIncremental );
         minREinc = pForces2->GetAxial( intervalIdx, lcRE, vPoi, minBAT, rtIncremental );
         maxREcum = pForces2->GetAxial( intervalIdx, lcRE, vPoi, maxBAT, rtCumulative );
         minREcum = pForces2->GetAxial( intervalIdx, lcRE, vPoi, minBAT, rtCumulative );

         maxPSinc = pForces2->GetAxial( intervalIdx, lcPS, vPoi, maxBAT, rtIncremental );
         minPSinc = pForces2->GetAxial( intervalIdx, lcPS, vPoi, minBAT, rtIncremental );
         maxPScum = pForces2->GetAxial( intervalIdx, lcPS, vPoi, maxBAT, rtCumulative );
         minPScum = pForces2->GetAxial( intervalIdx, lcPS, vPoi, minBAT, rtCumulative );
      }

      if ( intervalIdx < liveLoadIntervalIdx )
      {
         pLsForces2->GetAxial( intervalIdx, pgsTypes::ServiceI, vPoi, maxBAT, &dummy, &maxServiceI );
         pLsForces2->GetAxial( intervalIdx, pgsTypes::ServiceI, vPoi, minBAT, &minServiceI, &dummy );
      }

      IndexType index = 0;
      std::vector<pgsPointOfInterest>::const_iterator i(vPoi.begin());
      std::vector<pgsPointOfInterest>::const_iterator end(vPoi.end());
      for ( ; i != end; i++, index++ )
      {
         const pgsPointOfInterest& poi = *i;
         const CSegmentKey& thisSegmentKey = poi.GetSegmentKey();

         IntervalIndexType releaseIntervalIdx       = pIntervals->GetPrestressReleaseInterval(thisSegmentKey);
         IntervalIndexType erectSegmentIntervalIdx  = pIntervals->GetErectSegmentInterval(thisSegmentKey);
         IntervalIndexType tsRemovalIntervalIdx     = pIntervals->GetTemporaryStrandRemovalInterval(thisSegmentKey);

         ColumnIndexType col = 0;

         (*p_table)(row,col++) << location.SetValue( poiRefAttribute, poi );

         if ( analysisType == pgsTypes::Envelope )
         {
            (*p_table)(row,col++) << axial.SetValue( maxDCinc[index] );
            (*p_table)(row,col++) << axial.SetValue( minDCinc[index] );
            (*p_table)(row,col++) << axial.SetValue( maxDWinc[index] );
            (*p_table)(row,col++) << axial.SetValue( minDWinc[index] );

            if(bRating)
            {
               (*p_table)(row,col++) << axial.SetValue( maxDWRatinginc[index] );
               (*p_table)(row,col++) << axial.SetValue( minDWRatinginc[index] );
            }

            if ( bTimeStepMethod )
            {
               (*p_table)(row,col++) << axial.SetValue( maxCRinc[index] );
               (*p_table)(row,col++) << axial.SetValue( minCRinc[index] );
               (*p_table)(row,col++) << axial.SetValue( maxSHinc[index] );
               (*p_table)(row,col++) << axial.SetValue( minSHinc[index] );
               (*p_table)(row,col++) << axial.SetValue( maxREinc[index] );
               (*p_table)(row,col++) << axial.SetValue( minREinc[index] );
               (*p_table)(row,col++) << axial.SetValue( maxPSinc[index] );
               (*p_table)(row,col++) << axial.SetValue( minPSinc[index] );
            }

            (*p_table)(row,col++) << axial.SetValue( maxDCcum[index] );
            (*p_table)(row,col++) << axial.SetValue( minDCcum[index] );
            (*p_table)(row,col++) << axial.SetValue( maxDWcum[index] );
            (*p_table)(row,col++) << axial.SetValue( minDWcum[index] );

            if(bRating)
            {
               (*p_table)(row,col++) << axial.SetValue( maxDWRatingcum[index] );
               (*p_table)(row,col++) << axial.SetValue( minDWRatingcum[index] );
            }

            if ( bTimeStepMethod )
            {
               (*p_table)(row,col++) << axial.SetValue( maxCRcum[index] );
               (*p_table)(row,col++) << axial.SetValue( minCRcum[index] );
               (*p_table)(row,col++) << axial.SetValue( maxSHcum[index] );
               (*p_table)(row,col++) << axial.SetValue( minSHcum[index] );
               (*p_table)(row,col++) << axial.SetValue( maxREcum[index] );
               (*p_table)(row,col++) << axial.SetValue( minREcum[index] );
               (*p_table)(row,col++) << axial.SetValue( maxPScum[index] );
               (*p_table)(row,col++) << axial.SetValue( minPScum[index] );
            }

            if ( intervalIdx < liveLoadIntervalIdx )
            {
               (*p_table)(row,col++) << axial.SetValue( maxServiceI[index] );
               (*p_table)(row,col++) << axial.SetValue( minServiceI[index] );
            }
         }
         else
         {
            (*p_table)(row,col++) << axial.SetValue( maxDCinc[index] );
            (*p_table)(row,col++) << axial.SetValue( maxDWinc[index] );

            if(bRating)
            {
               (*p_table)(row,col++) << axial.SetValue( maxDWRatinginc[index] );
            }

            if ( bTimeStepMethod )
            {
               (*p_table)(row,col++) << axial.SetValue( maxCRinc[index] );
               (*p_table)(row,col++) << axial.SetValue( maxSHinc[index] );
               (*p_table)(row,col++) << axial.SetValue( maxREinc[index] );
               (*p_table)(row,col++) << axial.SetValue( maxPSinc[index] );
            }

            (*p_table)(row,col++) << axial.SetValue( maxDCcum[index] );
            (*p_table)(row,col++) << axial.SetValue( maxDWcum[index] );

            if(bRating)
            {
               (*p_table)(row,col++) << axial.SetValue( maxDWRatingcum[index] );
            }

            if ( bTimeStepMethod )
            {
               (*p_table)(row,col++) << axial.SetValue( maxCRcum[index] );
               (*p_table)(row,col++) << axial.SetValue( maxSHcum[index] );
               (*p_table)(row,col++) << axial.SetValue( maxREcum[index] );
               (*p_table)(row,col++) << axial.SetValue( maxPScum[index] );
            }

            if ( intervalIdx < liveLoadIntervalIdx )
            {
               (*p_table)(row,col++) << axial.SetValue( maxServiceI[index] );
            }
         }

         row++;
      }
   }
}

void CCombinedAxialTable::BuildCombinedLiveTable(IBroker* pBroker, rptChapter* pChapter,
                                                  const CGirderKey& girderKey,
                                                  IEAFDisplayUnits* pDisplayUnits,
                                                  pgsTypes::AnalysisType analysisType,
                                                  bool bDesign,bool bRating) const
{
   ATLASSERT(!(bDesign&&bRating)); // these are separate tables, can't do both

   GET_IFACE2(pBroker,IIntervals,pIntervals);

   // Build table
   INIT_UV_PROTOTYPE( rptPointOfInterest, location, pDisplayUnits->GetSpanLengthUnit(), false );
   INIT_UV_PROTOTYPE( rptForceUnitValue, axial, pDisplayUnits->GetGeneralForceUnit(), false );

   location.IncludeSpanAndGirder(girderKey.groupIndex == ALL_GROUPS);

   GET_IFACE2(pBroker,ILibrary,pLib);
   GET_IFACE2(pBroker,ISpecification,pSpec);
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( pSpec->GetSpecification().c_str() );
   
   GET_IFACE2(pBroker,IBridge,pBridge);
   GroupIndexType nGroups = pBridge->GetGirderGroupCount();

   GroupIndexType startGroupIdx = (girderKey.groupIndex == ALL_GROUPS ? 0 : girderKey.groupIndex);
   GroupIndexType endGroupIdx   = (girderKey.groupIndex == ALL_GROUPS ? nGroups-1 : startGroupIdx);
 
   GET_IFACE2(pBroker,ILimitStateForces,pLimitStateForces);
   bool bPermit = pLimitStateForces->IsStrengthIIApplicable(girderKey);

   GET_IFACE2(pBroker,IProductLoads,pProductLoads);
   bool bPedLoading = pProductLoads->HasPedestrianLoad(girderKey);

   GET_IFACE2(pBroker,IRatingSpecification,pRatingSpec);

   LPCTSTR strLabel = bDesign ? _T("Axial - Design Vehicles") : _T("Axial - Rating Vehicles");

   rptParagraph* p = new rptParagraph;
   *pChapter << p;

   rptRcTable* p_table;
   RowIndexType Nhrows = CreateCombinedLiveLoadingTableHeading<rptForceUnitTag,unitmgtForceData>(&p_table,strLabel,false,bDesign,bPermit,bPedLoading,bRating,false,true,
                           analysisType,pRatingSpec,pDisplayUnits,pDisplayUnits->GetGeneralForceUnit());

   RowIndexType row = Nhrows;

   *p << p_table;

   if ( girderKey.groupIndex == ALL_GROUPS )
   {
      p_table->SetColumnStyle(0,rptStyleManager::GetTableCellStyle(CB_NONE | CJ_LEFT));
      p_table->SetStripeRowColumnStyle(0,rptStyleManager::GetTableStripeRowCellStyle(CB_NONE | CJ_LEFT));
   }

   rptParagraph* pNote = new rptParagraph(rptStyleManager::GetFootnoteStyle());
   *pChapter << pNote;
   *pNote << LIVELOAD_PER_GIRDER << rptNewLine;

   // Get the interface pointers we need
   GET_IFACE2(pBroker,ICombinedForces2,  pForces2);
   GET_IFACE2(pBroker,IProductForces,pProdForces);
   pgsTypes::BridgeAnalysisType minBAT = pProdForces->GetBridgeAnalysisType(analysisType,pgsTypes::Maximize);
   pgsTypes::BridgeAnalysisType maxBAT = pProdForces->GetBridgeAnalysisType(analysisType,pgsTypes::Maximize);

   // Fill up the table
   for ( GroupIndexType grpIdx = startGroupIdx; grpIdx <= endGroupIdx; grpIdx++ )
   {
      CGirderKey thisGirderKey(grpIdx,girderKey.girderIndex);

      IntervalIndexType liveLoadIntervalIdx = pIntervals->GetLiveLoadInterval();

      PoiAttributeType poiRefAttribute;
      std::vector<pgsPointOfInterest> vPoi;
      GetCombinedResultsPoi(pBroker,thisGirderKey,liveLoadIntervalIdx,&vPoi,&poiRefAttribute);

      std::vector<Float64> dummy;
      std::vector<Float64> minPedestrianLL,    maxPedestrianLL;
      std::vector<Float64> minDesignLL,        maxDesignLL;
      std::vector<Float64> minFatigueLL,       maxFatigueLL;
      std::vector<Float64> minPermitLL,        maxPermitLL;
      std::vector<Float64> minLegalRoutineLL,  maxLegalRoutineLL;
      std::vector<Float64> minLegalSpecialLL,  maxLegalSpecialLL;
      std::vector<Float64> minPermitRoutineLL, maxPermitRoutineLL;
      std::vector<Float64> minPermitSpecialLL, maxPermitSpecialLL;

      if ( bDesign )
      {
         if ( bPedLoading )
         {
            pForces2->GetCombinedLiveLoadAxial( liveLoadIntervalIdx, pgsTypes::lltPedestrian, vPoi, maxBAT, &dummy, &maxPedestrianLL );
            pForces2->GetCombinedLiveLoadAxial( liveLoadIntervalIdx, pgsTypes::lltPedestrian, vPoi, minBAT, &minPedestrianLL, &dummy );
         }

         pForces2->GetCombinedLiveLoadAxial( liveLoadIntervalIdx, pgsTypes::lltDesign, vPoi, maxBAT, &dummy, &maxDesignLL );
         pForces2->GetCombinedLiveLoadAxial( liveLoadIntervalIdx, pgsTypes::lltDesign, vPoi, minBAT, &minDesignLL, &dummy );

         if ( lrfdVersionMgr::FourthEditionWith2009Interims <= lrfdVersionMgr::GetVersion() )
         {
            pForces2->GetCombinedLiveLoadAxial( liveLoadIntervalIdx, pgsTypes::lltFatigue, vPoi, maxBAT, &dummy, &maxFatigueLL );
            pForces2->GetCombinedLiveLoadAxial( liveLoadIntervalIdx, pgsTypes::lltFatigue, vPoi, minBAT, &minFatigueLL, &dummy );
         }

         if ( bPermit )
         {
            pForces2->GetCombinedLiveLoadAxial( liveLoadIntervalIdx, pgsTypes::lltPermit, vPoi, maxBAT, &dummy, &maxPermitLL );
            pForces2->GetCombinedLiveLoadAxial( liveLoadIntervalIdx, pgsTypes::lltPermit, vPoi, minBAT, &minPermitLL, &dummy );
         }
      }

      if ( bRating )
      {
         IntervalIndexType ratingIntervalIdx = pIntervals->GetLoadRatingInterval();
         if(pRatingSpec->IncludePedestrianLiveLoad())
         {
            pForces2->GetCombinedLiveLoadAxial( ratingIntervalIdx, pgsTypes::lltPedestrian, vPoi, maxBAT, &dummy, &maxPedestrianLL );
            pForces2->GetCombinedLiveLoadAxial( ratingIntervalIdx, pgsTypes::lltPedestrian, vPoi, minBAT, &minPedestrianLL, &dummy );
         }

         if ( !bDesign && (pRatingSpec->IsRatingEnabled(pgsTypes::lrDesign_Inventory) || pRatingSpec->IsRatingEnabled(pgsTypes::lrDesign_Operating)) )
         {
            pForces2->GetCombinedLiveLoadAxial( ratingIntervalIdx, pgsTypes::lltDesign, vPoi, maxBAT, &dummy, &maxDesignLL );
            pForces2->GetCombinedLiveLoadAxial( ratingIntervalIdx, pgsTypes::lltDesign, vPoi, minBAT, &minDesignLL, &dummy );
         }

         if ( pRatingSpec->IsRatingEnabled(pgsTypes::lrLegal_Routine) )
         {
            pForces2->GetCombinedLiveLoadAxial( ratingIntervalIdx, pgsTypes::lltLegalRating_Routine, vPoi, maxBAT, &dummy, &maxLegalRoutineLL );
            pForces2->GetCombinedLiveLoadAxial( ratingIntervalIdx, pgsTypes::lltLegalRating_Routine, vPoi, minBAT, &minLegalRoutineLL, &dummy );
         }

         if ( pRatingSpec->IsRatingEnabled(pgsTypes::lrLegal_Routine) )
         {
            pForces2->GetCombinedLiveLoadAxial( ratingIntervalIdx, pgsTypes::lltLegalRating_Special, vPoi, maxBAT, &dummy, &maxLegalSpecialLL );
            pForces2->GetCombinedLiveLoadAxial( ratingIntervalIdx, pgsTypes::lltLegalRating_Special, vPoi, minBAT, &minLegalSpecialLL, &dummy );
         }

         if ( pRatingSpec->IsRatingEnabled(pgsTypes::lrPermit_Routine) )
         {
            pForces2->GetCombinedLiveLoadAxial( ratingIntervalIdx, pgsTypes::lltPermitRating_Routine, vPoi, maxBAT, &dummy, &maxPermitRoutineLL );
            pForces2->GetCombinedLiveLoadAxial( ratingIntervalIdx, pgsTypes::lltPermitRating_Routine, vPoi, minBAT, &minPermitRoutineLL, &dummy );
         }

         if ( pRatingSpec->IsRatingEnabled(pgsTypes::lrPermit_Special) )
         {
            pForces2->GetCombinedLiveLoadAxial( ratingIntervalIdx, pgsTypes::lltPermitRating_Special, vPoi, maxBAT, &dummy, &maxPermitSpecialLL );
            pForces2->GetCombinedLiveLoadAxial( ratingIntervalIdx, pgsTypes::lltPermitRating_Special, vPoi, minBAT, &minPermitSpecialLL, &dummy );
         }
      }

      // fill table (first half if design/ped load)
      std::vector<pgsPointOfInterest>::const_iterator i(vPoi.begin());
      std::vector<pgsPointOfInterest>::const_iterator end(vPoi.end());
      IndexType index = 0;
      ColumnIndexType col = 0;

      for ( ; i != end; i++, index++ )
      {
         const pgsPointOfInterest& poi = *i;
         col = 0;

         (*p_table)(row,col++) << location.SetValue( poiRefAttribute, poi );

         if ( bDesign )
         {
            if ( bPedLoading )
            {
               (*p_table)(row,col++) << axial.SetValue( maxPedestrianLL[index] );
               (*p_table)(row,col++) << axial.SetValue( minPedestrianLL[index] );
            }

            (*p_table)(row,col++) << axial.SetValue( maxDesignLL[index] );
            (*p_table)(row,col++) << axial.SetValue( minDesignLL[index] );

            if ( lrfdVersionMgr::FourthEditionWith2009Interims <= lrfdVersionMgr::GetVersion() )
            {
               (*p_table)(row,col++) << axial.SetValue( maxFatigueLL[index] );
               (*p_table)(row,col++) << axial.SetValue( minFatigueLL[index] );
            }

            if ( bPermit )
            {
               (*p_table)(row,col++) << axial.SetValue( maxPermitLL[index] );
               (*p_table)(row,col++) << axial.SetValue( minPermitLL[index] );
            }
         }

         if ( bRating )
         {
            if(bPedLoading && pRatingSpec->IncludePedestrianLiveLoad())
            {
               (*p_table)(row,col++) << axial.SetValue( maxPedestrianLL[index] );
               (*p_table)(row,col++) << axial.SetValue( minPedestrianLL[index] );
            }

            if ( !bDesign && (pRatingSpec->IsRatingEnabled(pgsTypes::lrDesign_Inventory) || pRatingSpec->IsRatingEnabled(pgsTypes::lrDesign_Operating)))
            {
               (*p_table)(row,col++) << axial.SetValue( maxDesignLL[index] );
               (*p_table)(row,col++) << axial.SetValue( minDesignLL[index] );
            }

            if ( pRatingSpec->IsRatingEnabled(pgsTypes::lrLegal_Routine) )
            {
               (*p_table)(row,col++) << axial.SetValue( maxLegalRoutineLL[index] );
               (*p_table)(row,col++) << axial.SetValue( minLegalRoutineLL[index] );
            }

            if ( pRatingSpec->IsRatingEnabled(pgsTypes::lrLegal_Special) )
            {
               (*p_table)(row,col++) << axial.SetValue( maxLegalSpecialLL[index] );
               (*p_table)(row,col++) << axial.SetValue( minLegalSpecialLL[index] );
            }

            if ( pRatingSpec->IsRatingEnabled(pgsTypes::lrPermit_Routine) )
            {
               (*p_table)(row,col++) << axial.SetValue( maxPermitRoutineLL[index] );
               (*p_table)(row,col++) << axial.SetValue( minPermitRoutineLL[index] );
            }

            if ( pRatingSpec->IsRatingEnabled(pgsTypes::lrPermit_Special) )
            {
               (*p_table)(row,col++) << axial.SetValue( maxPermitSpecialLL[index] );
               (*p_table)(row,col++) << axial.SetValue( minPermitSpecialLL[index] );
            }
         }

         row++;
      }


      // fill second half of table if design & ped load
      if ( bDesign && bPedLoading )
      {
         // Sum or envelope pedestrian values with live loads to give final LL

         GET_IFACE2(pBroker,ILiveLoads,pLiveLoads);
         ILiveLoads::PedestrianLoadApplicationType DesignPedLoad = pLiveLoads->GetPedestrianLoadApplication(pgsTypes::lltDesign);
         ILiveLoads::PedestrianLoadApplicationType FatiguePedLoad = pLiveLoads->GetPedestrianLoadApplication(pgsTypes::lltFatigue);
         ILiveLoads::PedestrianLoadApplicationType PermitPedLoad = pLiveLoads->GetPedestrianLoadApplication(pgsTypes::lltPermit);

         SumPedAndLiveLoad(DesignPedLoad, minDesignLL, maxDesignLL, minPedestrianLL, maxPedestrianLL);

         if ( lrfdVersionMgr::FourthEditionWith2009Interims <= lrfdVersionMgr::GetVersion() )
         {
            SumPedAndLiveLoad(FatiguePedLoad, minFatigueLL, maxFatigueLL, minPedestrianLL, maxPedestrianLL);
         }

         if ( bPermit )
         {
            SumPedAndLiveLoad(PermitPedLoad, minPermitLL, maxPermitLL, minPedestrianLL, maxPedestrianLL);
         }

         // Now we can fill table
         //RowIndexType    row = Nhrows;
         ColumnIndexType recCol = col;
         IndexType psiz = (IndexType)vPoi.size();
         for ( index=0; index<psiz; index++ )
         {
            col = recCol;

            (*p_table)(row,col++) << axial.SetValue( maxDesignLL[index] );
            (*p_table)(row,col++) << axial.SetValue( minDesignLL[index] );

            if ( lrfdVersionMgr::FourthEditionWith2009Interims <= lrfdVersionMgr::GetVersion() )
            {
               (*p_table)(row,col++) << axial.SetValue( maxFatigueLL[index] );
               (*p_table)(row,col++) << axial.SetValue( minFatigueLL[index] );
            }

            if ( bPermit )
            {
               (*p_table)(row,col++) << axial.SetValue( maxPermitLL[index] );
               (*p_table)(row,col++) << axial.SetValue( minPermitLL[index] );
            }

            row++;
         }

         // footnotes for pedestrian loads
         int lnum=1;
         *pNote<< lnum++ << PedestrianFootnote(DesignPedLoad) << rptNewLine;

         if ( lrfdVersionMgr::FourthEditionWith2009Interims <= lrfdVersionMgr::GetVersion() )
         {
            *pNote << lnum++ << PedestrianFootnote(FatiguePedLoad) << rptNewLine;
         }

         if ( bPermit )
         {
            *pNote << lnum++ << PedestrianFootnote(PermitPedLoad) << rptNewLine;
         }
      }

      if ( bRating && pRatingSpec->IncludePedestrianLiveLoad())
      {
         // Note for rating and pedestrian load
         *pNote << _T("$ Pedestrian load results will be summed with vehicular load results at time of load combination.");
      }
   }
}

void CCombinedAxialTable::BuildLimitStateTable(IBroker* pBroker, rptChapter* pChapter,
                                               const CGirderKey& girderKey,
                                               IEAFDisplayUnits* pDisplayUnits,IntervalIndexType intervalIdx,
                                               pgsTypes::AnalysisType analysisType,
                                               bool bDesign,bool bRating) const
{
   ATLASSERT(!(bDesign&&bRating)); // these are separate tables, can't do both

   GET_IFACE2(pBroker,IIntervals,pIntervals);

   INIT_UV_PROTOTYPE( rptPointOfInterest, location, pDisplayUnits->GetSpanLengthUnit(), false );
   INIT_UV_PROTOTYPE( rptForceUnitValue, axial, pDisplayUnits->GetGeneralForceUnit(), false );

   location.IncludeSpanAndGirder(girderKey.groupIndex == ALL_GROUPS);

   rptParagraph* p = new rptParagraph;
   *pChapter << p;

   GET_IFACE2(pBroker,IBridge,pBridge);
   GroupIndexType nGroups = pBridge->GetGirderGroupCount();

   GET_IFACE2(pBroker,ILibrary,pLib);
   GET_IFACE2(pBroker,ISpecification,pSpec);
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( pSpec->GetSpecification().c_str() );

   GroupIndexType startGroupIdx = (girderKey.groupIndex == ALL_GROUPS ? 0 : girderKey.groupIndex);
   GroupIndexType endGroupIdx   = (girderKey.groupIndex == ALL_GROUPS ? nGroups-1 : startGroupIdx);
 
   GET_IFACE2(pBroker,ILimitStateForces,pLimitStateForces);
   bool bPermit = pLimitStateForces->IsStrengthIIApplicable(girderKey);

   GET_IFACE2(pBroker,IProductLoads,pProductLoads);
   GET_IFACE2(pBroker,IRatingSpecification,pRatingSpec);

   bool bPedLoading = pProductLoads->HasPedestrianLoad(girderKey);

   rptRcTable * p_table2;
   RowIndexType row2 = CreateLimitStateTableHeading<rptForceUnitTag,unitmgtForceData>(&p_table2,_T("Axial, Pu"),false,bDesign,bPermit,bRating,false,analysisType,pProductLoads,pRatingSpec,pDisplayUnits,pDisplayUnits->GetGeneralForceUnit());
   *p << p_table2;

   if ( girderKey.groupIndex == ALL_GROUPS )
   {
      p_table2->SetColumnStyle(0,rptStyleManager::GetTableCellStyle(CB_NONE | CJ_LEFT));
      p_table2->SetStripeRowColumnStyle(0,rptStyleManager::GetTableStripeRowCellStyle(CB_NONE | CJ_LEFT));
   }

   // Get the interface pointers we need
   GET_IFACE2(pBroker,ILimitStateForces2,pLsForces2);
   GET_IFACE2(pBroker,IProductForces,pProdForces);
   pgsTypes::BridgeAnalysisType minBAT = pProdForces->GetBridgeAnalysisType(analysisType,pgsTypes::Maximize);
   pgsTypes::BridgeAnalysisType maxBAT = pProdForces->GetBridgeAnalysisType(analysisType,pgsTypes::Maximize);

   // Fill up the table
   for ( GroupIndexType grpIdx = startGroupIdx; grpIdx <= endGroupIdx; grpIdx++ )
   {
      CGirderKey thisGirderKey(grpIdx,girderKey.girderIndex);

      PoiAttributeType poiRefAttribute;
      std::vector<pgsPointOfInterest> vPoi;
      GetCombinedResultsPoi(pBroker,thisGirderKey,intervalIdx,&vPoi,&poiRefAttribute);

      std::vector<Float64> dummy;
      std::vector<Float64> minServiceI,   maxServiceI;
      std::vector<Float64> minServiceIA,  maxServiceIA;
      std::vector<Float64> minServiceIII, maxServiceIII;
      std::vector<Float64> minFatigueI,   maxFatigueI;
      std::vector<Float64> minStrengthI,  maxStrengthI;
      std::vector<Float64> minStrengthII, maxStrengthII;
      std::vector<Float64> minStrengthI_Inventory, maxStrengthI_Inventory;
      std::vector<Float64> minStrengthI_Operating, maxStrengthI_Operating;
      std::vector<Float64> minStrengthI_Legal_Routine, maxStrengthI_Legal_Routine;
      std::vector<Float64> minStrengthI_Legal_Special, maxStrengthI_Legal_Special;
      std::vector<Float64> minStrengthII_Permit_Routine, maxStrengthII_Permit_Routine;
      std::vector<Float64> minStrengthII_Permit_Special, maxStrengthII_Permit_Special;
      std::vector<Float64> minServiceI_Permit_Routine, maxServiceI_Permit_Routine;
      std::vector<Float64> minServiceI_Permit_Special, maxServiceI_Permit_Special;

      if ( minBAT == maxBAT )
      {
         if ( bDesign )
         {
            pLsForces2->GetAxial( intervalIdx, pgsTypes::ServiceI, vPoi, maxBAT, &minServiceI, &maxServiceI );

            if ( lrfdVersionMgr::GetVersion() < lrfdVersionMgr::FourthEditionWith2009Interims )
            {
               pLsForces2->GetAxial( intervalIdx, pgsTypes::ServiceIA, vPoi, maxBAT, &minServiceIA, &maxServiceIA );
            }
            else
            {
               pLsForces2->GetAxial( intervalIdx, pgsTypes::FatigueI, vPoi, maxBAT, &minFatigueI, &maxFatigueI );
            }

            pLsForces2->GetAxial( intervalIdx, pgsTypes::ServiceIII, vPoi, maxBAT, &minServiceIII, &maxServiceIII );

            pLsForces2->GetAxial( intervalIdx, pgsTypes::StrengthI, vPoi, maxBAT, &minStrengthI, &maxStrengthI );

            if ( bPermit )
            {
               pLsForces2->GetAxial( intervalIdx, pgsTypes::StrengthII, vPoi, maxBAT, &minStrengthII, &maxStrengthII );
            }
         }

         if ( bRating )
         {
            // Design - Inventory
            if ( pRatingSpec->IsRatingEnabled(pgsTypes::lrDesign_Inventory) )
            {
               pLsForces2->GetAxial( intervalIdx, pgsTypes::StrengthI_Inventory, vPoi, maxBAT, &minStrengthI_Inventory, &maxStrengthI_Inventory );
            }

            // Design - Operating
            if ( pRatingSpec->IsRatingEnabled(pgsTypes::lrDesign_Operating) )
            {
               pLsForces2->GetAxial( intervalIdx, pgsTypes::StrengthI_Operating, vPoi, maxBAT, &minStrengthI_Operating, &maxStrengthI_Operating );
            }

            // Legal - Routine
            if ( pRatingSpec->IsRatingEnabled(pgsTypes::lrLegal_Routine) )
            {
               pLsForces2->GetAxial( intervalIdx, pgsTypes::StrengthI_LegalRoutine, vPoi, maxBAT, &minStrengthI_Legal_Routine, &maxStrengthI_Legal_Routine );
            }

            // Legal - Special
            if ( pRatingSpec->IsRatingEnabled(pgsTypes::lrLegal_Special) )
            {
               pLsForces2->GetAxial( intervalIdx, pgsTypes::StrengthI_LegalSpecial, vPoi, maxBAT, &minStrengthI_Legal_Special, &maxStrengthI_Legal_Special );
            }

            // Permit Rating - Routine
            if ( pRatingSpec->IsRatingEnabled(pgsTypes::lrPermit_Routine) )
            {
               pLsForces2->GetAxial( intervalIdx, pgsTypes::ServiceI_PermitRoutine,   vPoi, maxBAT, &minServiceI_Permit_Routine, &maxServiceI_Permit_Routine );
               pLsForces2->GetAxial( intervalIdx, pgsTypes::StrengthII_PermitRoutine, vPoi, maxBAT, &minStrengthII_Permit_Routine, &maxStrengthII_Permit_Routine );
            }

            // Permit Rating - Special
            if ( pRatingSpec->IsRatingEnabled(pgsTypes::lrPermit_Special) )
            {
               pLsForces2->GetAxial( intervalIdx, pgsTypes::ServiceI_PermitSpecial,   vPoi, maxBAT, &minServiceI_Permit_Special, &maxServiceI_Permit_Special );
               pLsForces2->GetAxial( intervalIdx, pgsTypes::StrengthII_PermitSpecial, vPoi, maxBAT, &minStrengthII_Permit_Special, &maxStrengthII_Permit_Special );
            }
         }
      }
      else
      {
         if ( bDesign )
         {
            pLsForces2->GetAxial( intervalIdx, pgsTypes::ServiceI, vPoi, maxBAT, &dummy, &maxServiceI );
            pLsForces2->GetAxial( intervalIdx, pgsTypes::ServiceI, vPoi, minBAT, &minServiceI, &dummy );

            if ( lrfdVersionMgr::GetVersion() < lrfdVersionMgr::FourthEditionWith2009Interims )
            {
               pLsForces2->GetAxial( intervalIdx, pgsTypes::ServiceIA, vPoi, maxBAT, &dummy, &maxServiceIA );
               pLsForces2->GetAxial( intervalIdx, pgsTypes::ServiceIA, vPoi, minBAT, &minServiceIA, &dummy );
            }
            else
            {
               pLsForces2->GetAxial( intervalIdx, pgsTypes::FatigueI, vPoi, maxBAT, &dummy, &maxFatigueI );
               pLsForces2->GetAxial( intervalIdx, pgsTypes::FatigueI, vPoi, minBAT, &minFatigueI, &dummy );
            }

            pLsForces2->GetAxial( intervalIdx, pgsTypes::ServiceIII, vPoi, maxBAT, &dummy, &maxServiceIII );
            pLsForces2->GetAxial( intervalIdx, pgsTypes::ServiceIII, vPoi, minBAT, &minServiceIII, &dummy );

            pLsForces2->GetAxial( intervalIdx, pgsTypes::StrengthI, vPoi, maxBAT, &dummy, &maxStrengthI );
            pLsForces2->GetAxial( intervalIdx, pgsTypes::StrengthI, vPoi, minBAT, &minStrengthI, &dummy );

            if ( bPermit )
            {
               pLsForces2->GetAxial( intervalIdx, pgsTypes::StrengthII, vPoi, maxBAT, &dummy, &maxStrengthII );
               pLsForces2->GetAxial( intervalIdx, pgsTypes::StrengthII, vPoi, minBAT, &minStrengthII, &dummy );
            }
         }

         if ( bRating )
         {
            // Design - Inventory
            if ( pRatingSpec->IsRatingEnabled(pgsTypes::lrDesign_Inventory) )
            {
               pLsForces2->GetAxial( intervalIdx, pgsTypes::StrengthI_Inventory, vPoi, maxBAT, &dummy, &maxStrengthI_Inventory );
               pLsForces2->GetAxial( intervalIdx, pgsTypes::StrengthI_Inventory, vPoi, maxBAT, &minStrengthI_Inventory, &dummy );
            }

            // Design - Operating
            if ( pRatingSpec->IsRatingEnabled(pgsTypes::lrDesign_Operating) )
            {
               pLsForces2->GetAxial( intervalIdx, pgsTypes::StrengthI_Operating, vPoi, maxBAT, &dummy, &maxStrengthI_Operating );
               pLsForces2->GetAxial( intervalIdx, pgsTypes::StrengthI_Operating, vPoi, minBAT, &minStrengthI_Operating, &dummy );
            }

            // Legal - Routine
            if ( pRatingSpec->IsRatingEnabled(pgsTypes::lrLegal_Routine) )
            {
               pLsForces2->GetAxial( intervalIdx, pgsTypes::StrengthI_LegalRoutine, vPoi, maxBAT, &dummy, &maxStrengthI_Legal_Routine );
               pLsForces2->GetAxial( intervalIdx, pgsTypes::StrengthI_LegalRoutine, vPoi, maxBAT, &minStrengthI_Legal_Routine, &dummy );
            }

            // Legal - Special
            if ( pRatingSpec->IsRatingEnabled(pgsTypes::lrLegal_Special) )
            {
               pLsForces2->GetAxial( intervalIdx, pgsTypes::StrengthI_LegalSpecial, vPoi, maxBAT, &dummy, &maxStrengthI_Legal_Special );
               pLsForces2->GetAxial( intervalIdx, pgsTypes::StrengthI_LegalSpecial, vPoi, minBAT, &minStrengthI_Legal_Special, &dummy );
            }

            // Permit Rating - Routine
            if ( pRatingSpec->IsRatingEnabled(pgsTypes::lrPermit_Routine) )
            {
               pLsForces2->GetAxial( intervalIdx, pgsTypes::ServiceI_PermitRoutine, vPoi, maxBAT, &dummy, &maxServiceI_Permit_Routine );
               pLsForces2->GetAxial( intervalIdx, pgsTypes::ServiceI_PermitRoutine, vPoi, minBAT, &minServiceI_Permit_Routine, &dummy );
               pLsForces2->GetAxial( intervalIdx, pgsTypes::StrengthII_PermitRoutine, vPoi, maxBAT, &dummy, &maxStrengthII_Permit_Routine );
               pLsForces2->GetAxial( intervalIdx, pgsTypes::StrengthII_PermitRoutine, vPoi, minBAT, &minStrengthII_Permit_Routine, &dummy );
            }

            // Permit Rating - Special
            if ( pRatingSpec->IsRatingEnabled(pgsTypes::lrPermit_Special) )
            {
               pLsForces2->GetAxial( intervalIdx, pgsTypes::ServiceI_PermitSpecial, vPoi, maxBAT, &dummy, &maxServiceI_Permit_Special );
               pLsForces2->GetAxial( intervalIdx, pgsTypes::ServiceI_PermitSpecial, vPoi, minBAT, &minServiceI_Permit_Special, &dummy );
               pLsForces2->GetAxial( intervalIdx, pgsTypes::StrengthII_PermitSpecial, vPoi, maxBAT, &dummy, &maxStrengthII_Permit_Special );
               pLsForces2->GetAxial( intervalIdx, pgsTypes::StrengthII_PermitSpecial, vPoi, minBAT, &minStrengthII_Permit_Special, &dummy );
            }
         }
      }

      IndexType index = 0;
      std::vector<pgsPointOfInterest>::const_iterator i(vPoi.begin());
      std::vector<pgsPointOfInterest>::const_iterator end(vPoi.end());
      for ( ; i != end; i++, index++ )
      {
         ColumnIndexType col = 0;

         const pgsPointOfInterest& poi = *i;

         IntervalIndexType releaseIntervalIdx = pIntervals->GetPrestressReleaseInterval(poi.GetSegmentKey());

         (*p_table2)(row2,col++) << location.SetValue( poiRefAttribute, poi );

         if ( analysisType == pgsTypes::Envelope )
         {
            if ( bDesign )
            {
               (*p_table2)(row2,col++) << axial.SetValue( maxServiceI[index] );
               (*p_table2)(row2,col++) << axial.SetValue( minServiceI[index] );

               if ( lrfdVersionMgr::GetVersion() < lrfdVersionMgr::FourthEditionWith2009Interims )
               {
                  (*p_table2)(row2,col++) << axial.SetValue( maxServiceIA[index] );
                  (*p_table2)(row2,col++) << axial.SetValue( minServiceIA[index] );
               }

               (*p_table2)(row2,col++) << axial.SetValue( maxServiceIII[index] );
               (*p_table2)(row2,col++) << axial.SetValue( minServiceIII[index] );

               if ( lrfdVersionMgr::FourthEditionWith2009Interims <= lrfdVersionMgr::GetVersion() )
               {
                  (*p_table2)(row2,col++) << axial.SetValue( maxFatigueI[index] );
                  (*p_table2)(row2,col++) << axial.SetValue( minFatigueI[index] );
               }

               (*p_table2)(row2,col++) << axial.SetValue( maxStrengthI[index] );
               (*p_table2)(row2,col++) << axial.SetValue( minStrengthI[index] );

               if ( bPermit )
               {
                  (*p_table2)(row2,col++) << axial.SetValue( maxStrengthII[index] );
                  (*p_table2)(row2,col++) << axial.SetValue( minStrengthII[index] );
               }
            }

            if ( bRating )
            {
               if ( pRatingSpec->IsRatingEnabled(pgsTypes::lrDesign_Inventory) )
               {
                  (*p_table2)(row2,col++) << axial.SetValue( maxStrengthI_Inventory[index]);
                  (*p_table2)(row2,col++) << axial.SetValue( minStrengthI_Inventory[index]);
               }

               if ( pRatingSpec->IsRatingEnabled(pgsTypes::lrDesign_Operating) )
               {
                  (*p_table2)(row2,col++) << axial.SetValue( maxStrengthI_Operating[index]);
                  (*p_table2)(row2,col++) << axial.SetValue( minStrengthI_Operating[index]);
               }

               if ( pRatingSpec->IsRatingEnabled(pgsTypes::lrLegal_Routine) )
               {
                  (*p_table2)(row2,col++) << axial.SetValue( maxStrengthI_Legal_Routine[index]);
                  (*p_table2)(row2,col++) << axial.SetValue( minStrengthI_Legal_Routine[index]);
               }

               if ( pRatingSpec->IsRatingEnabled(pgsTypes::lrLegal_Special) )
               {
                  (*p_table2)(row2,col++) << axial.SetValue( maxStrengthI_Legal_Special[index]);
                  (*p_table2)(row2,col++) << axial.SetValue( minStrengthI_Legal_Special[index]);
               }

               if ( pRatingSpec->IsRatingEnabled(pgsTypes::lrPermit_Routine) )
               {
                  (*p_table2)(row2,col++) << axial.SetValue( maxServiceI_Permit_Routine[index]);
                  (*p_table2)(row2,col++) << axial.SetValue( minServiceI_Permit_Routine[index]);
                  (*p_table2)(row2,col++) << axial.SetValue( maxStrengthII_Permit_Routine[index]);
                  (*p_table2)(row2,col++) << axial.SetValue( minStrengthII_Permit_Routine[index]);
               }

               if ( pRatingSpec->IsRatingEnabled(pgsTypes::lrPermit_Special) )
               {
                  (*p_table2)(row2,col++) << axial.SetValue( maxServiceI_Permit_Special[index]);
                  (*p_table2)(row2,col++) << axial.SetValue( minServiceI_Permit_Special[index]);
                  (*p_table2)(row2,col++) << axial.SetValue( maxStrengthII_Permit_Special[index]);
                  (*p_table2)(row2,col++) << axial.SetValue( minStrengthII_Permit_Special[index]);
               }
            }
         }
         else
         {
            if ( bDesign )
            {
               (*p_table2)(row2,col++) << axial.SetValue( maxServiceI[index] );

               if ( lrfdVersionMgr::GetVersion() < lrfdVersionMgr::FourthEditionWith2009Interims )
                  (*p_table2)(row2,col++) << axial.SetValue( maxServiceIA[index] );
               
               (*p_table2)(row2,col++) << axial.SetValue( maxServiceIII[index] );

               if ( lrfdVersionMgr::FourthEditionWith2009Interims <= lrfdVersionMgr::GetVersion() )
                  (*p_table2)(row2,col++) << axial.SetValue( maxFatigueI[index] );

               (*p_table2)(row2,col++) << axial.SetValue( maxStrengthI[index] );
               (*p_table2)(row2,col++) << axial.SetValue( minStrengthI[index] );

               if ( bPermit )
               {
                  (*p_table2)(row2,col++) << axial.SetValue( maxStrengthII[index] );
                  (*p_table2)(row2,col++) << axial.SetValue( minStrengthII[index] );
               }
            }

            if ( bRating )
            {
               if ( pRatingSpec->IsRatingEnabled(pgsTypes::lrDesign_Inventory) )
               {
                  (*p_table2)(row2,col++) << axial.SetValue( maxStrengthI_Inventory[index]);
                  (*p_table2)(row2,col++) << axial.SetValue( minStrengthI_Inventory[index]);
               }

               if ( pRatingSpec->IsRatingEnabled(pgsTypes::lrDesign_Operating) )
               {
                  (*p_table2)(row2,col++) << axial.SetValue( maxStrengthI_Operating[index]);
                  (*p_table2)(row2,col++) << axial.SetValue( minStrengthI_Operating[index]);
               }

               if ( pRatingSpec->IsRatingEnabled(pgsTypes::lrLegal_Routine) )
               {
                  (*p_table2)(row2,col++) << axial.SetValue( maxStrengthI_Legal_Routine[index]);
                  (*p_table2)(row2,col++) << axial.SetValue( minStrengthI_Legal_Routine[index]);
               }

               if ( pRatingSpec->IsRatingEnabled(pgsTypes::lrLegal_Special) )
               {
                  (*p_table2)(row2,col++) << axial.SetValue( maxStrengthI_Legal_Special[index]);
                  (*p_table2)(row2,col++) << axial.SetValue( minStrengthI_Legal_Special[index]);
               }

               if ( pRatingSpec->IsRatingEnabled(pgsTypes::lrPermit_Routine) )
               {
                  (*p_table2)(row2,col++) << axial.SetValue(maxServiceI_Permit_Routine[index]);
                  (*p_table2)(row2,col++) << axial.SetValue(minServiceI_Permit_Routine[index]);
                  (*p_table2)(row2,col++) << axial.SetValue(maxStrengthII_Permit_Routine[index]);
                  (*p_table2)(row2,col++) << axial.SetValue(minStrengthII_Permit_Routine[index]);
               }

               if ( pRatingSpec->IsRatingEnabled(pgsTypes::lrPermit_Special) )
               {
                  (*p_table2)(row2,col++) << axial.SetValue(maxServiceI_Permit_Special[index]);
                  (*p_table2)(row2,col++) << axial.SetValue(minServiceI_Permit_Special[index]);
                  (*p_table2)(row2,col++) << axial.SetValue(maxStrengthII_Permit_Special[index]);
                  (*p_table2)(row2,col++) << axial.SetValue(minStrengthII_Permit_Special[index]);
               }
            }
         }

         row2++;
      }
   }
}

void CCombinedAxialTable::MakeCopy(const CCombinedAxialTable& rOther)
{
   // Add copy code here...
}

void CCombinedAxialTable::MakeAssignment(const CCombinedAxialTable& rOther)
{
   MakeCopy( rOther );
}
