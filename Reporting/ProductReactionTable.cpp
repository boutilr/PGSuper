///////////////////////////////////////////////////////////////////////
// PGSuper - Prestressed Girder SUPERstructure Design and Analysis
// Copyright � 1999-2014  Washington State Department of Transportation
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
#include <Reporting\ProductReactionTable.h>
#include <Reporting\ProductMomentsTable.h>

#include <IFace\Bridge.h>
#include <EAF\EAFDisplayUnits.h>
#include <IFace\AnalysisResults.h>
#include <IFace\Project.h>
#include <IFace\RatingSpecification.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


////////////////////////// PUBLIC     ///////////////////////////////////////

//======================== LIFECYCLE  =======================================
CProductReactionTable::CProductReactionTable()
{
}

CProductReactionTable::CProductReactionTable(const CProductReactionTable& rOther)
{
   MakeCopy(rOther);
}

CProductReactionTable::~CProductReactionTable()
{
}

//======================== OPERATORS  =======================================
CProductReactionTable& CProductReactionTable::operator= (const CProductReactionTable& rOther)
{
   if( this != &rOther )
   {
      MakeAssignment(rOther);
   }

   return *this;
}

//======================== OPERATIONS =======================================
rptRcTable* CProductReactionTable::Build(IBroker* pBroker,const CGirderKey& girderKey,pgsTypes::AnalysisType analysisType,
                                         ReactionTableType tableType, bool bIncludeImpact, bool bIncludeLLDF,bool bDesign,bool bRating,bool bIndicateControllingLoad,
                                         IEAFDisplayUnits* pDisplayUnits) const
{
   // Build table
   INIT_UV_PROTOTYPE( rptLengthUnitValue, location, pDisplayUnits->GetSpanLengthUnit(), false );
   INIT_UV_PROTOTYPE( rptForceSectionValue, reaction, pDisplayUnits->GetShearUnit(), false );

   GET_IFACE2(pBroker,IBridge,pBridge);
   bool bFutureOverlay = pBridge->IsFutureOverlay();
   PierIndexType nPiers = pBridge->GetPierCount();

   GET_IFACE2(pBroker,IIntervals,pIntervals);
   IntervalIndexType castDeckIntervalIdx      = pIntervals->GetCastDeckInterval(girderKey);
   IntervalIndexType compositeDeckIntervalIdx = pIntervals->GetCompositeDeckInterval(girderKey);
   IntervalIndexType railingSystemIntervalIdx = pIntervals->GetInstallRailingSystemInterval(girderKey);
   IntervalIndexType liveLoadIntervalIdx      = pIntervals->GetLiveLoadInterval(girderKey);
   IntervalIndexType loadRatingIntervalIdx    = pIntervals->GetLoadRatingInterval(girderKey);
   IntervalIndexType overlayIntervalIdx       = pIntervals->GetOverlayInterval(girderKey);
   IntervalIndexType erectSegmentIntervalIdx  = pIntervals->GetFirstSegmentErectionInterval(girderKey);

   bool bConstruction, bDeckPanels, bPedLoading, bSidewalk, bShearKey, bPermit;
   GroupIndexType startGroup, nGroups;
   IntervalIndexType continuityIntervalIdx;

   GET_IFACE2(pBroker, IRatingSpecification, pRatingSpec);

   ColumnIndexType nCols = GetProductLoadTableColumnCount(pBroker,girderKey,analysisType,bDesign,bRating,&bConstruction,&bDeckPanels,&bSidewalk,&bShearKey,&bPedLoading,&bPermit,&continuityIntervalIdx,&startGroup,&nGroups);
   
   rptRcTable* p_table = pgsReportStyleHolder::CreateDefaultTable(nCols,
                         tableType==PierReactionsTable ?_T("Total Girderline Reactions at Abutments and Piers"): _T("Girder Bearing Reactions") );
   RowIndexType row = ConfigureProductLoadTableHeading<rptForceUnitTag,unitmgtForceData>(pBroker,p_table,true,false,bConstruction,bDeckPanels,bSidewalk,bShearKey,bFutureOverlay,bDesign,bPedLoading,bPermit,bRating,analysisType,continuityIntervalIdx,castDeckIntervalIdx,pRatingSpec,pDisplayUnits,pDisplayUnits->GetShearUnit());

   GET_IFACE2(pBroker,IProductForces,pProductForces);
   GET_IFACE2(pBroker,IBearingDesign,pBearingDesign);
  
   GET_IFACE2(pBroker,IProductForces,pProdForces);
   pgsTypes::BridgeAnalysisType maxBAT = pProdForces->GetBridgeAnalysisType(analysisType,pgsTypes::Maximize);
   pgsTypes::BridgeAnalysisType minBAT = pProdForces->GetBridgeAnalysisType(analysisType,pgsTypes::Minimize);

   // TRICKY: use adapter class to get correct reaction interfaces
   std::auto_ptr<IProductReactionAdapter> pForces;
   if( tableType == PierReactionsTable )
   {
      pForces =  std::auto_ptr<ProductForcesReactionAdapter>(new ProductForcesReactionAdapter(pProductForces,girderKey));
   }
   else
   {
      pForces =  std::auto_ptr<BearingDesignProductReactionAdapter>(new BearingDesignProductReactionAdapter(pBearingDesign, compositeDeckIntervalIdx, girderKey) );
   }

   ReactionLocationIter iter = pForces->GetReactionLocations(pBridge);

   for (iter.First(); !iter.IsDone(); iter.Next())
   {
      ColumnIndexType col = 0;

      const ReactionLocation& reactionLocation( iter.CurrentItem() );

     (*p_table)(row,col++) << reactionLocation.PierLabel;

     ReactionDecider reactionDecider(tableType,reactionLocation,girderKey,pBridge,pIntervals);
   
     (*p_table)(row,col++) << reaction.SetValue( pForces->GetReaction( erectSegmentIntervalIdx, reactionLocation, pftGirder,    analysisType == pgsTypes::Simple ? pgsTypes::SimpleSpan : pgsTypes::ContinuousSpan ) );

     if ( reactionDecider.DoReport(castDeckIntervalIdx) )
        (*p_table)(row,col++) << reaction.SetValue( pForces->GetReaction( castDeckIntervalIdx,     reactionLocation, pftDiaphragm, analysisType == pgsTypes::Simple ? pgsTypes::SimpleSpan : pgsTypes::ContinuousSpan ) );
     else
        (*p_table)(row,col++) << RPT_NA;

      if ( bShearKey )
      {
         if ( analysisType == pgsTypes::Envelope )
         {
            if ( reactionDecider.DoReport(castDeckIntervalIdx) )
            {
               (*p_table)(row,col++) << reaction.SetValue( pForces->GetReaction( castDeckIntervalIdx, reactionLocation, pftShearKey, maxBAT ) );
               (*p_table)(row,col++) << reaction.SetValue( pForces->GetReaction( castDeckIntervalIdx, reactionLocation, pftShearKey, minBAT ) );
            }
            else
            {
               (*p_table)(row,col++) << RPT_NA;
               (*p_table)(row,col++) << RPT_NA;
            }
         }
         else
         {
            if ( reactionDecider.DoReport(castDeckIntervalIdx) )
            {
               (*p_table)(row,col++) << reaction.SetValue( pForces->GetReaction( castDeckIntervalIdx, reactionLocation, pftShearKey, analysisType == pgsTypes::Simple ? pgsTypes::SimpleSpan : pgsTypes::ContinuousSpan ) );
            }
            else
            {
               (*p_table)(row,col++) << RPT_NA;
            }
         }
      }

      if ( bConstruction )
      {
         if ( analysisType == pgsTypes::Envelope && continuityIntervalIdx == castDeckIntervalIdx )
         {
            if ( reactionDecider.DoReport(castDeckIntervalIdx) )
            {
               (*p_table)(row,col++) << reaction.SetValue( pForces->GetReaction( castDeckIntervalIdx, reactionLocation, pftConstruction,   maxBAT ) );
               (*p_table)(row,col++) << reaction.SetValue( pForces->GetReaction( castDeckIntervalIdx, reactionLocation, pftConstruction, minBAT ) );
            }
            else
            {
               (*p_table)(row,col++) << RPT_NA;
               (*p_table)(row,col++) << RPT_NA;
            }
         }
         else
         {
            if ( reactionDecider.DoReport(castDeckIntervalIdx) )
            {
               (*p_table)(row,col++) << reaction.SetValue( pForces->GetReaction( castDeckIntervalIdx, reactionLocation, pftConstruction, analysisType == pgsTypes::Simple ? pgsTypes::SimpleSpan : pgsTypes::ContinuousSpan ) );
            }
            else
            {
               (*p_table)(row,col++) << RPT_NA;
            }
         }
      }

      if ( analysisType == pgsTypes::Envelope && continuityIntervalIdx == castDeckIntervalIdx )
      {
         if ( reactionDecider.DoReport(castDeckIntervalIdx) )
         {
            (*p_table)(row,col++) << reaction.SetValue( pForces->GetReaction( castDeckIntervalIdx, reactionLocation, pftSlab, maxBAT ) );
            (*p_table)(row,col++) << reaction.SetValue( pForces->GetReaction( castDeckIntervalIdx, reactionLocation, pftSlab, minBAT ) );

            (*p_table)(row,col++) << reaction.SetValue( pForces->GetReaction( castDeckIntervalIdx, reactionLocation, pftSlabPad, maxBAT ) );
            (*p_table)(row,col++) << reaction.SetValue( pForces->GetReaction( castDeckIntervalIdx, reactionLocation, pftSlabPad, minBAT ) );
         }
         else
         {
            (*p_table)(row,col++) << RPT_NA;
            (*p_table)(row,col++) << RPT_NA;
            (*p_table)(row,col++) << RPT_NA;
            (*p_table)(row,col++) << RPT_NA;
         }
      }
      else
      {
         if ( reactionDecider.DoReport(castDeckIntervalIdx) )
         {
            (*p_table)(row,col++) << reaction.SetValue( pForces->GetReaction( castDeckIntervalIdx, reactionLocation, pftSlab,    analysisType == pgsTypes::Simple ? pgsTypes::SimpleSpan : pgsTypes::ContinuousSpan  ) );
            (*p_table)(row,col++) << reaction.SetValue( pForces->GetReaction( castDeckIntervalIdx, reactionLocation, pftSlabPad, analysisType == pgsTypes::Simple ? pgsTypes::SimpleSpan : pgsTypes::ContinuousSpan  ) );
         }
         else
         {
            (*p_table)(row,col++) << RPT_NA;
            (*p_table)(row,col++) << RPT_NA;
         }
      }

      if ( bDeckPanels )
      {
         if ( analysisType == pgsTypes::Envelope && continuityIntervalIdx == castDeckIntervalIdx )
         {
            if ( reactionDecider.DoReport(castDeckIntervalIdx) )
            {
               (*p_table)(row,col++) << reaction.SetValue( pForces->GetReaction( castDeckIntervalIdx, reactionLocation, pftSlabPanel, maxBAT ) );
               (*p_table)(row,col++) << reaction.SetValue( pForces->GetReaction( castDeckIntervalIdx, reactionLocation, pftSlabPanel, minBAT ) );
            }
            else
            {
               (*p_table)(row,col++) << RPT_NA;
               (*p_table)(row,col++) << RPT_NA;
            }
         }
         else
         {
            if ( reactionDecider.DoReport(castDeckIntervalIdx) )
            {
               (*p_table)(row,col++) << reaction.SetValue( pForces->GetReaction( castDeckIntervalIdx, reactionLocation, pftSlabPanel, analysisType == pgsTypes::Simple ? pgsTypes::SimpleSpan : pgsTypes::ContinuousSpan ) );
            }
            else
            {
               (*p_table)(row,col++) << RPT_NA;
            }
         }
      }

      if ( analysisType == pgsTypes::Envelope )
      {
         if ( bSidewalk )
         {
            if ( reactionDecider.DoReport(railingSystemIntervalIdx) )
            {
               (*p_table)(row,col++) << reaction.SetValue( pForces->GetReaction( railingSystemIntervalIdx, reactionLocation, pftSidewalk, maxBAT ) );
               (*p_table)(row,col++) << reaction.SetValue( pForces->GetReaction( railingSystemIntervalIdx, reactionLocation, pftSidewalk, minBAT ) );
            }
            else
            {
               (*p_table)(row,col++) << RPT_NA;
               (*p_table)(row,col++) << RPT_NA;
            }
         }

         if ( reactionDecider.DoReport(railingSystemIntervalIdx) )
         {
            (*p_table)(row,col++) << reaction.SetValue( pForces->GetReaction( railingSystemIntervalIdx, reactionLocation, pftTrafficBarrier, maxBAT ) );
            (*p_table)(row,col++) << reaction.SetValue( pForces->GetReaction( railingSystemIntervalIdx, reactionLocation, pftTrafficBarrier, minBAT ) );
         }
         else
         {
            (*p_table)(row,col++) << RPT_NA;
            (*p_table)(row,col++) << RPT_NA;
         }

         if ( reactionDecider.DoReport(overlayIntervalIdx) )
         {
            (*p_table)(row,col++) << reaction.SetValue( pForces->GetReaction( overlayIntervalIdx, reactionLocation, bRating && !bDesign ? pftOverlayRating : pftOverlay, maxBAT ) );
            (*p_table)(row,col++) << reaction.SetValue( pForces->GetReaction( overlayIntervalIdx, reactionLocation, bRating && !bDesign ? pftOverlayRating : pftOverlay, minBAT ) );
         }
         else
         {
            (*p_table)(row,col++) << RPT_NA;
            (*p_table)(row,col++) << RPT_NA;
         }

         Float64 min, max;
         VehicleIndexType minConfig, maxConfig;

         if ( bPedLoading )
         {
            if ( reactionDecider.DoReport(liveLoadIntervalIdx) )
            {
               pForces->GetLiveLoadReaction( pgsTypes::lltPedestrian, liveLoadIntervalIdx, reactionLocation, maxBAT, bIncludeImpact, true, &min, &max );
               (*p_table)(row,col++) << reaction.SetValue( max );

               pForces->GetLiveLoadReaction( pgsTypes::lltPedestrian, liveLoadIntervalIdx, reactionLocation, minBAT, bIncludeImpact, true, &min, &max );
               (*p_table)(row,col++) << reaction.SetValue( min );
            }
            else
            {
               (*p_table)(row,col++) << RPT_NA;
               (*p_table)(row,col++) << RPT_NA;
            }
         }

         if ( bDesign )
         {
            if ( reactionDecider.DoReport(liveLoadIntervalIdx) )
            {
               pForces->GetLiveLoadReaction( pgsTypes::lltDesign, liveLoadIntervalIdx, reactionLocation, maxBAT, bIncludeImpact, bIncludeLLDF, &min, &max, &minConfig, &maxConfig );
               (*p_table)(row,col) << reaction.SetValue( max );

               if ( bIndicateControllingLoad && minConfig!=INVALID_INDEX )
               {
                  (*p_table)(row,col) << rptNewLine <<  _T("(") << LiveLoadPrefix(pgsTypes::lltDesign) << maxConfig << _T(")");
               }

               col++;

               pForces->GetLiveLoadReaction( pgsTypes::lltDesign, liveLoadIntervalIdx, reactionLocation, minBAT, bIncludeImpact, bIncludeLLDF, &min, &max, &minConfig, &maxConfig  );
               (*p_table)(row,col) << reaction.SetValue( min );

               if ( bIndicateControllingLoad && minConfig!=INVALID_INDEX )
               {
                  (*p_table)(row,col) << rptNewLine <<  _T("(") << LiveLoadPrefix(pgsTypes::lltDesign)<< minConfig << _T(")");
               }

               col++;
            }
            else
            {
               (*p_table)(row,col++) << RPT_NA;
               (*p_table)(row,col++) << RPT_NA;
            }

            if ( lrfdVersionMgr::FourthEditionWith2009Interims <= lrfdVersionMgr::GetVersion() )
            {
               if ( reactionDecider.DoReport(liveLoadIntervalIdx) )
               {
                  pForces->GetLiveLoadReaction( pgsTypes::lltFatigue, liveLoadIntervalIdx, reactionLocation, maxBAT, bIncludeImpact, bIncludeLLDF, &min, &max, &minConfig, &maxConfig );
                  (*p_table)(row,col) << reaction.SetValue( max );

                  if ( bIndicateControllingLoad && maxConfig!=INVALID_INDEX )
                  {
                     (*p_table)(row,col) << rptNewLine <<  _T("(") << LiveLoadPrefix(pgsTypes::lltFatigue) << maxConfig << _T(")");
                  }

                  col++;

                  pForces->GetLiveLoadReaction( pgsTypes::lltFatigue, liveLoadIntervalIdx, reactionLocation, minBAT, bIncludeImpact, bIncludeLLDF, &min, &max, &minConfig, &maxConfig  );
                  (*p_table)(row,col) << reaction.SetValue( min );

                  if ( bIndicateControllingLoad && minConfig!=INVALID_INDEX )
                  {
                     (*p_table)(row,col) << rptNewLine <<  _T("(") << LiveLoadPrefix(pgsTypes::lltFatigue) << minConfig << _T(")");
                  }

                  col++;
               }
               else
               {
                  (*p_table)(row,col++) << RPT_NA;
                  (*p_table)(row,col++) << RPT_NA;
               }
            }

            if ( bPermit )
            {
               if ( reactionDecider.DoReport(liveLoadIntervalIdx) )
               {
                  pForces->GetLiveLoadReaction( pgsTypes::lltPermit, liveLoadIntervalIdx, reactionLocation, maxBAT, bIncludeImpact, bIncludeLLDF, &min, &max, &minConfig, &maxConfig );
                  (*p_table)(row,col) << reaction.SetValue( max );

                  if ( bIndicateControllingLoad && maxConfig!=INVALID_INDEX )
                  {
                     (*p_table)(row,col) << rptNewLine <<  _T("(") << LiveLoadPrefix(pgsTypes::lltPermit) << maxConfig << _T(")");
                  }

                  col++;

                  pForces->GetLiveLoadReaction( pgsTypes::lltPermit, liveLoadIntervalIdx, reactionLocation, minBAT, bIncludeImpact, bIncludeLLDF, &min, &max, &minConfig, &maxConfig );
                  (*p_table)(row,col) << reaction.SetValue( min );

                  if ( bIndicateControllingLoad && minConfig!=INVALID_INDEX )
                  {
                     (*p_table)(row,col) << rptNewLine <<  _T("(") << LiveLoadPrefix(pgsTypes::lltPermit) << minConfig << _T(")");
                  }

                  col++;
               }
               else
               {
                  (*p_table)(row,col++) << RPT_NA;
                  (*p_table)(row,col++) << RPT_NA;
               }
            }
         }

         if ( bRating )
         {
            if ( !bDesign && (pRatingSpec->IsRatingEnabled(pgsTypes::lrDesign_Inventory) || pRatingSpec->IsRatingEnabled(pgsTypes::lrDesign_Operating)) )
            {
               if ( reactionDecider.DoReport(liveLoadIntervalIdx) )
               {
                  pForces->GetLiveLoadReaction( pgsTypes::lltDesign, liveLoadIntervalIdx, reactionLocation, maxBAT, bIncludeImpact, bIncludeLLDF, &min, &max, &minConfig, &maxConfig );
                  (*p_table)(row,col) << reaction.SetValue( max );

                  if ( bIndicateControllingLoad && maxConfig!=INVALID_INDEX )
                  {
                     (*p_table)(row,col) << rptNewLine <<  _T("(") << LiveLoadPrefix(pgsTypes::lltDesign) << maxConfig << _T(")");
                  }

                  col++;

                  pForces->GetLiveLoadReaction( pgsTypes::lltDesign, liveLoadIntervalIdx, reactionLocation, minBAT, bIncludeImpact, bIncludeLLDF, &min, &max, &minConfig, &maxConfig  );
                  (*p_table)(row,col) << reaction.SetValue( min );

                  if ( bIndicateControllingLoad && minConfig!=INVALID_INDEX )
                  {
                     (*p_table)(row,col) << rptNewLine <<  _T("(") << LiveLoadPrefix(pgsTypes::lltDesign)<< minConfig << _T(")");
                  }

                  col++;
               }
               else
               {
                  (*p_table)(row,col++) << RPT_NA;
                  (*p_table)(row,col++) << RPT_NA;
               }
            }

            // Legal - Routine
            if ( pRatingSpec->IsRatingEnabled(pgsTypes::lrLegal_Routine) )
            {
               if ( reactionDecider.DoReport(loadRatingIntervalIdx) )
               {
                  pForces->GetLiveLoadReaction( pgsTypes::lltLegalRating_Routine, loadRatingIntervalIdx, reactionLocation, maxBAT, bIncludeImpact, bIncludeLLDF, &min, &max, &minConfig, &maxConfig );
                  (*p_table)(row,col) << reaction.SetValue( max );

                  if ( bIndicateControllingLoad && maxConfig!=INVALID_INDEX )
                  {
                     (*p_table)(row,col) << rptNewLine << _T("(") << LiveLoadPrefix(pgsTypes::lltLegalRating_Routine) << maxConfig << _T(")");
                  }

                  col++;

                  pForces->GetLiveLoadReaction( pgsTypes::lltLegalRating_Routine, loadRatingIntervalIdx, reactionLocation, minBAT, bIncludeImpact, bIncludeLLDF, &min, &max, &minConfig, &maxConfig );
                  (*p_table)(row,col) << reaction.SetValue( min );

                  if ( bIndicateControllingLoad && minConfig!=INVALID_INDEX )
                  {
                     (*p_table)(row,col) << rptNewLine << _T("(") << LiveLoadPrefix(pgsTypes::lltLegalRating_Routine) << minConfig << _T(")");
                  }

                  col++;
               }
               else
               {
                  (*p_table)(row,col++) << RPT_NA;
                  (*p_table)(row,col++) << RPT_NA;
               }
            }

            // Legal - Special
            if ( pRatingSpec->IsRatingEnabled(pgsTypes::lrLegal_Special) )
            {
               if ( reactionDecider.DoReport(loadRatingIntervalIdx) )
               {
                  pForces->GetLiveLoadReaction( pgsTypes::lltLegalRating_Special, loadRatingIntervalIdx, reactionLocation, maxBAT, bIncludeImpact, bIncludeLLDF, &min, &max, &minConfig, &maxConfig );
                  (*p_table)(row,col) << reaction.SetValue( max );

                  if ( bIndicateControllingLoad && maxConfig!=INVALID_INDEX )
                  {
                     (*p_table)(row,col) << rptNewLine << _T("(") << LiveLoadPrefix(pgsTypes::lltLegalRating_Special) << maxConfig << _T(")");
                  }

                  col++;

                  pForces->GetLiveLoadReaction( pgsTypes::lltLegalRating_Special, loadRatingIntervalIdx, reactionLocation, minBAT, bIncludeImpact, bIncludeLLDF, &min, &max, &minConfig, &maxConfig );
                  (*p_table)(row,col) << reaction.SetValue( min );

                  if ( bIndicateControllingLoad && minConfig!=INVALID_INDEX )
                  {
                     (*p_table)(row,col) << rptNewLine << _T("(") << LiveLoadPrefix(pgsTypes::lltLegalRating_Special) << minConfig << _T(")");
                  }

                  col++;
               }
               else
               {
                  (*p_table)(row,col++) << RPT_NA;
                  (*p_table)(row,col++) << RPT_NA;
               }
            }

            // Permit Rating - Routine
            if ( pRatingSpec->IsRatingEnabled(pgsTypes::lrPermit_Routine) )
            {
               if ( reactionDecider.DoReport(loadRatingIntervalIdx) )
               {
                  pForces->GetLiveLoadReaction( pgsTypes::lltPermitRating_Routine, loadRatingIntervalIdx, reactionLocation, maxBAT, bIncludeImpact, bIncludeLLDF, &min, &max, &minConfig, &maxConfig );
                  (*p_table)(row,col) << reaction.SetValue( max );

                  if ( bIndicateControllingLoad && maxConfig!=INVALID_INDEX )
                  {
                     (*p_table)(row,col) << rptNewLine << _T("(") << LiveLoadPrefix(pgsTypes::lltPermitRating_Routine) << maxConfig << _T(")");
                  }

                  col++;

                  pForces->GetLiveLoadReaction( pgsTypes::lltPermitRating_Routine, loadRatingIntervalIdx, reactionLocation, minBAT, bIncludeImpact, bIncludeLLDF, &min, &max, &minConfig, &maxConfig );
                  (*p_table)(row,col) << reaction.SetValue( min );

                  if ( bIndicateControllingLoad && minConfig != INVALID_INDEX )
                  {
                     (*p_table)(row,col) << rptNewLine << _T("(") << LiveLoadPrefix(pgsTypes::lltPermitRating_Routine) << minConfig << _T(")");
                  }

                  col++;
               }
               else
               {
                  (*p_table)(row,col++) << RPT_NA;
                  (*p_table)(row,col++) << RPT_NA;
               }
            }

            // Permit Rating - Special
            if ( pRatingSpec->IsRatingEnabled(pgsTypes::lrPermit_Special) )
            {
               if ( reactionDecider.DoReport(loadRatingIntervalIdx) )
               {
                  pForces->GetLiveLoadReaction( pgsTypes::lltPermitRating_Special, loadRatingIntervalIdx, reactionLocation, maxBAT, bIncludeImpact, bIncludeLLDF, &min, &max, &minConfig, &maxConfig );
                  (*p_table)(row,col) << reaction.SetValue( max );

                  if ( bIndicateControllingLoad && maxConfig != INVALID_INDEX )
                  {
                     (*p_table)(row,col) << rptNewLine << _T("(") << LiveLoadPrefix(pgsTypes::lltPermitRating_Special) << maxConfig << _T(")");
                  }

                  col++;

                  pForces->GetLiveLoadReaction( pgsTypes::lltPermitRating_Special, loadRatingIntervalIdx, reactionLocation, minBAT, bIncludeImpact, bIncludeLLDF, &min, &max, &minConfig, &maxConfig );
                  (*p_table)(row,col) << reaction.SetValue( min );

                  if ( bIndicateControllingLoad && minConfig != INVALID_INDEX )
                  {
                     (*p_table)(row,col) << rptNewLine << _T("(") << LiveLoadPrefix(pgsTypes::lltPermitRating_Special) << minConfig << _T(")");
                  }

                  col++;
               }
               else
               {
                  (*p_table)(row,col++) << RPT_NA;
                  (*p_table)(row,col++) << RPT_NA;
               }
            }
         }
      }
      else
      {
         if ( bSidewalk )
         {
            if ( reactionDecider.DoReport(railingSystemIntervalIdx) )
            {
               (*p_table)(row,col++) << reaction.SetValue( pForces->GetReaction( railingSystemIntervalIdx, reactionLocation, pftSidewalk, analysisType == pgsTypes::Simple ? pgsTypes::SimpleSpan : pgsTypes::ContinuousSpan ) );
            }
            else
            {
               (*p_table)(row,col++) << RPT_NA;
            }
         }

         if ( reactionDecider.DoReport(railingSystemIntervalIdx) )
         {
            (*p_table)(row,col++) << reaction.SetValue( pForces->GetReaction( railingSystemIntervalIdx, reactionLocation, pftTrafficBarrier, analysisType == pgsTypes::Simple ? pgsTypes::SimpleSpan : pgsTypes::ContinuousSpan ) );
         }
         else
         {
            (*p_table)(row,col++) << RPT_NA;
         }

         if ( reactionDecider.DoReport(overlayIntervalIdx) )
         {
            (*p_table)(row,col++) << reaction.SetValue( pForces->GetReaction( overlayIntervalIdx, reactionLocation, bRating && !bDesign ? pftOverlayRating : pftOverlay, analysisType == pgsTypes::Simple ? pgsTypes::SimpleSpan : pgsTypes::ContinuousSpan ) );
         }
         else
         {
            (*p_table)(row,col++) << RPT_NA;
         }

         Float64 min, max;
         VehicleIndexType minConfig, maxConfig;
         if ( bPedLoading )
         {
            if ( reactionDecider.DoReport(liveLoadIntervalIdx) )
            {
               pForces->GetLiveLoadReaction( pgsTypes::lltPedestrian, liveLoadIntervalIdx, reactionLocation, analysisType == pgsTypes::Simple ? pgsTypes::SimpleSpan : pgsTypes::ContinuousSpan, bIncludeImpact, true, &min, &max );
               (*p_table)(row,col++) << reaction.SetValue( max );
               (*p_table)(row,col++) << reaction.SetValue( min );
            }
            else
            {
               (*p_table)(row,col++) << RPT_NA;
               (*p_table)(row,col++) << RPT_NA;
            }
         }

         if ( bDesign )
         {
            if ( reactionDecider.DoReport(liveLoadIntervalIdx) )
            {
               pForces->GetLiveLoadReaction( pgsTypes::lltDesign, liveLoadIntervalIdx, reactionLocation, analysisType == pgsTypes::Simple ? pgsTypes::SimpleSpan : pgsTypes::ContinuousSpan, bIncludeImpact, bIncludeLLDF, &min, &max, &minConfig, &maxConfig );
               (*p_table)(row,col) << reaction.SetValue( max );
               if ( bIndicateControllingLoad && maxConfig!=INVALID_INDEX)
               {
                  (*p_table)(row,col) << rptNewLine << _T("(") << LiveLoadPrefix(pgsTypes::lltDesign) << maxConfig << _T(")");
               }

               col++;

               (*p_table)(row,col) << reaction.SetValue( min );
               if ( bIndicateControllingLoad && minConfig != INVALID_INDEX )
               {
                  (*p_table)(row,col) << rptNewLine << _T("(") << LiveLoadPrefix(pgsTypes::lltDesign) << minConfig << _T(")");
               }

               col++;
            }
            else
            {
               (*p_table)(row,col++) << RPT_NA;
               (*p_table)(row,col++) << RPT_NA;
            }

            if ( lrfdVersionMgr::FourthEditionWith2009Interims <= lrfdVersionMgr::GetVersion() )
            {
               if ( reactionDecider.DoReport(liveLoadIntervalIdx) )
               {
                  pForces->GetLiveLoadReaction( pgsTypes::lltFatigue, liveLoadIntervalIdx, reactionLocation, analysisType == pgsTypes::Simple ? pgsTypes::SimpleSpan : pgsTypes::ContinuousSpan, bIncludeImpact, bIncludeLLDF, &min, &max, &minConfig, &maxConfig );
                  (*p_table)(row,col) << reaction.SetValue( max );
                  if ( bIndicateControllingLoad && maxConfig != INVALID_INDEX )
                  {
                     (*p_table)(row,col) << rptNewLine << _T("(") << LiveLoadPrefix(pgsTypes::lltFatigue) << maxConfig << _T(")");
                  }

                  col++;

                  (*p_table)(row,col) << reaction.SetValue( min );
                  if ( bIndicateControllingLoad && minConfig != INVALID_INDEX )
                  {
                     (*p_table)(row,col) << rptNewLine << _T("(") << LiveLoadPrefix(pgsTypes::lltFatigue) << minConfig << _T(")");
                  }

                  col++;
               }
               else
               {
                  (*p_table)(row,col++) << RPT_NA;
                  (*p_table)(row,col++) << RPT_NA;
               }
            }

            if ( bPermit )
            {
               if ( reactionDecider.DoReport(liveLoadIntervalIdx) )
               {
                  pForces->GetLiveLoadReaction( pgsTypes::lltPermit, liveLoadIntervalIdx, reactionLocation, analysisType == pgsTypes::Simple ? pgsTypes::SimpleSpan : pgsTypes::ContinuousSpan, bIncludeImpact, bIncludeLLDF, &min, &max, &minConfig, &maxConfig );
                  (*p_table)(row,col) << reaction.SetValue( max );
                  if ( bIndicateControllingLoad && maxConfig!=INVALID_INDEX )
                  {
                     (*p_table)(row,col) << rptNewLine << _T("(") << LiveLoadPrefix(pgsTypes::lltPermit) << maxConfig << _T(")");
                  }
                  col++;

                  (*p_table)(row,col) << reaction.SetValue( min );
                  if ( bIndicateControllingLoad && minConfig!=INVALID_INDEX )
                  {
                     (*p_table)(row,col) << rptNewLine << _T("(") << LiveLoadPrefix(pgsTypes::lltPermit)<< minConfig << _T(")");
                  }
                  col++;
               }
               else
               {
                  (*p_table)(row,col++) << RPT_NA;
                  (*p_table)(row,col++) << RPT_NA;
               }
            }
         }

         if ( bRating )
         {
            if ( !bDesign && (pRatingSpec->IsRatingEnabled(pgsTypes::lrDesign_Inventory) || pRatingSpec->IsRatingEnabled(pgsTypes::lrDesign_Operating))  )
            {
               if ( reactionDecider.DoReport(loadRatingIntervalIdx) )
               {
                  pForces->GetLiveLoadReaction( pgsTypes::lltDesign, loadRatingIntervalIdx, reactionLocation, analysisType == pgsTypes::Simple ? pgsTypes::SimpleSpan : pgsTypes::ContinuousSpan, bIncludeImpact, bIncludeLLDF, &min, &max, &minConfig, &maxConfig );
                  (*p_table)(row,col) << reaction.SetValue( max );
                  if ( bIndicateControllingLoad && maxConfig != INVALID_INDEX )
                  {
                     (*p_table)(row,col) << rptNewLine << _T("(") << LiveLoadPrefix(pgsTypes::lltDesign) << maxConfig << _T(")");
                  }

                  col++;

                  (*p_table)(row,col) << reaction.SetValue( min );
                  if ( bIndicateControllingLoad && minConfig != INVALID_INDEX )
                  {
                     (*p_table)(row,col) << rptNewLine << _T("(") << LiveLoadPrefix(pgsTypes::lltDesign) << minConfig << _T(")");
                  }

                  col++;
               }
               else
               {
                  (*p_table)(row,col++) << RPT_NA;
                  (*p_table)(row,col++) << RPT_NA;
               }
            }

            // Legal - Routine
            if ( pRatingSpec->IsRatingEnabled(pgsTypes::lrLegal_Routine) )
            {
               if ( reactionDecider.DoReport(loadRatingIntervalIdx) )
               {
                  pForces->GetLiveLoadReaction( pgsTypes::lltLegalRating_Routine, loadRatingIntervalIdx, reactionLocation, analysisType == pgsTypes::Simple ? pgsTypes::SimpleSpan : pgsTypes::ContinuousSpan, bIncludeImpact, bIncludeLLDF, &min, &max, &minConfig, &maxConfig );
                  (*p_table)(row,col) << reaction.SetValue( max );
                  if ( bIndicateControllingLoad && maxConfig != INVALID_INDEX )
                  {
                     (*p_table)(row,col) << rptNewLine << _T("(") << LiveLoadPrefix(pgsTypes::lltLegalRating_Routine) << maxConfig << _T(")");
                  }
                  col++;

                  (*p_table)(row,col) << reaction.SetValue( min );
                  if ( bIndicateControllingLoad && minConfig != INVALID_INDEX )
                  {
                     (*p_table)(row,col) << rptNewLine << _T("(") << LiveLoadPrefix(pgsTypes::lltLegalRating_Routine)<< minConfig << _T(")");
                  }
                  col++;
               }
               else
               {
                  (*p_table)(row,col++) << RPT_NA;
                  (*p_table)(row,col++) << RPT_NA;
               }
            }

            // Legal - Special
            if ( pRatingSpec->IsRatingEnabled(pgsTypes::lrLegal_Special) )
            {
               if ( reactionDecider.DoReport(loadRatingIntervalIdx) )
               {
                  pForces->GetLiveLoadReaction( pgsTypes::lltLegalRating_Special, loadRatingIntervalIdx, reactionLocation, analysisType == pgsTypes::Simple ? pgsTypes::SimpleSpan : pgsTypes::ContinuousSpan, bIncludeImpact, bIncludeLLDF, &min, &max, &minConfig, &maxConfig );
                  (*p_table)(row,col) << reaction.SetValue( max );
                  if ( bIndicateControllingLoad && maxConfig != INVALID_INDEX )
                  {
                     (*p_table)(row,col) << rptNewLine << _T("(") << LiveLoadPrefix(pgsTypes::lltLegalRating_Special) << maxConfig << _T(")");
                  }
                  col++;

                  (*p_table)(row,col) << reaction.SetValue( min );
                  if ( bIndicateControllingLoad && minConfig != INVALID_INDEX )
                  {
                     (*p_table)(row,col) << rptNewLine << _T("(") << LiveLoadPrefix(pgsTypes::lltLegalRating_Special)<< minConfig << _T(")");
                  }
                  col++;
               }
               else
               {
                  (*p_table)(row,col++) << RPT_NA;
                  (*p_table)(row,col++) << RPT_NA;
               }
            }

            // Permit Rating - Routine
            if ( pRatingSpec->IsRatingEnabled(pgsTypes::lrPermit_Routine) )
            {
               if ( reactionDecider.DoReport(loadRatingIntervalIdx) )
               {
                  pForces->GetLiveLoadReaction( pgsTypes::lltPermitRating_Routine, loadRatingIntervalIdx, reactionLocation, analysisType == pgsTypes::Simple ? pgsTypes::SimpleSpan : pgsTypes::ContinuousSpan, bIncludeImpact, bIncludeLLDF, &min, &max, &minConfig, &maxConfig );
                  (*p_table)(row,col) << reaction.SetValue( max );
                  if ( bIndicateControllingLoad && maxConfig != INVALID_INDEX )
                  {
                     (*p_table)(row,col) << rptNewLine << _T("(") << LiveLoadPrefix(pgsTypes::lltPermitRating_Routine) << maxConfig << _T(")");
                  }
                  col++;

                  (*p_table)(row,col) << reaction.SetValue( min );
                  if ( bIndicateControllingLoad && minConfig != INVALID_INDEX )
                  {
                     (*p_table)(row,col) << rptNewLine << _T("(") << LiveLoadPrefix(pgsTypes::lltPermitRating_Routine)<< minConfig << _T(")");
                  }
                  col++;
               }
               else
               {
                  (*p_table)(row,col++) << RPT_NA;
                  (*p_table)(row,col++) << RPT_NA;
               }
            }

            // Permit Rating - Special
            if ( pRatingSpec->IsRatingEnabled(pgsTypes::lrPermit_Special) )
            {
               if ( reactionDecider.DoReport(loadRatingIntervalIdx) )
               {
                  pForces->GetLiveLoadReaction( pgsTypes::lltPermitRating_Special, loadRatingIntervalIdx, reactionLocation, analysisType == pgsTypes::Simple ? pgsTypes::SimpleSpan : pgsTypes::ContinuousSpan, bIncludeImpact, bIncludeLLDF, &min, &max, &minConfig, &maxConfig );
                  (*p_table)(row,col) << reaction.SetValue( max );
                  if ( bIndicateControllingLoad && maxConfig != INVALID_INDEX )
                  {
                     (*p_table)(row,col) << rptNewLine << _T("(") << LiveLoadPrefix(pgsTypes::lltPermitRating_Special) << maxConfig << _T(")");
                  }
                  col++;

                  (*p_table)(row,col) << reaction.SetValue( min );
                  if ( bIndicateControllingLoad && minConfig != INVALID_INDEX )
                  {
                     (*p_table)(row,col) << rptNewLine << _T("(") << LiveLoadPrefix(pgsTypes::lltPermitRating_Special)<< minConfig << _T(")");
                  }
                  col++;
               }
               else
               {
                  (*p_table)(row,col++) << RPT_NA;
                  (*p_table)(row,col++) << RPT_NA;
               }
            }
         }
      }

      row++;
   }

   return p_table;
}

//======================== ACCESS     =======================================
//======================== INQUIRY    =======================================

////////////////////////// PROTECTED  ///////////////////////////////////////

//======================== LIFECYCLE  =======================================
//======================== OPERATORS  =======================================
//======================== OPERATIONS =======================================
void CProductReactionTable::MakeCopy(const CProductReactionTable& rOther)
{
   // Add copy code here...
}

void CProductReactionTable::MakeAssignment(const CProductReactionTable& rOther)
{
   MakeCopy( rOther );
}

//======================== ACCESS     =======================================
//======================== INQUIRY    =======================================

////////////////////////// PRIVATE    ///////////////////////////////////////

//======================== LIFECYCLE  =======================================
//======================== OPERATORS  =======================================
//======================== OPERATIONS =======================================
//======================== ACCESS     =======================================
//======================== INQUERY    =======================================

//======================== DEBUG      =======================================
#if defined _DEBUG
bool CProductReactionTable::AssertValid() const
{
   return true;
}

void CProductReactionTable::Dump(dbgDumpContext& os) const
{
   os << _T("Dump for CProductReactionTable") << endl;
}
#endif // _DEBUG

#if defined _UNITTEST
bool CProductReactionTable::TestMe(dbgLog& rlog)
{
   TESTME_PROLOGUE("CProductReactionTable");

   TEST_NOT_IMPLEMENTED("Unit Tests Not Implemented for CProductReactionTable");

   TESTME_EPILOG("CProductReactionTable");
}
#endif // _UNITTEST
