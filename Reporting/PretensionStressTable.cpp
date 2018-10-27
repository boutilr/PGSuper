///////////////////////////////////////////////////////////////////////
// PGSuper - Prestressed Girder SUPERstructure Design and Analysis
// Copyright � 1999-2018  Washington State Department of Transportation
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
#include <Reporting\PretensionStressTable.h>
#include <Reporting\ReportNotes.h>

#include <PgsExt\ReportPointOfInterest.h>
#include <PgsExt\TimelineEvent.h>

#include <IFace\Bridge.h>
#include <IFace\AnalysisResults.h>
#include <IFace\Project.h>
#include <IFace\Intervals.h>
#include <IFace\PrestressForce.h>
#include <IFace\RatingSpecification.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/****************************************************************************
CLASS
   CPretensionStressTable
****************************************************************************/


////////////////////////// PUBLIC     ///////////////////////////////////////

//======================== LIFECYCLE  =======================================
CPretensionStressTable::CPretensionStressTable()
{
}

CPretensionStressTable::CPretensionStressTable(const CPretensionStressTable& rOther)
{
   MakeCopy(rOther);
}

CPretensionStressTable::~CPretensionStressTable()
{
}

//======================== OPERATORS  =======================================
CPretensionStressTable& CPretensionStressTable::operator= (const CPretensionStressTable& rOther)
{
   if( this != &rOther )
   {
      MakeAssignment(rOther);
   }

   return *this;
}

//======================== OPERATIONS =======================================
rptRcTable* CPretensionStressTable::Build(IBroker* pBroker,const CSegmentKey& segmentKey,
                                            bool bDesign,IEAFDisplayUnits* pDisplayUnits) const
{
   // Build table
   INIT_UV_PROTOTYPE( rptPointOfInterest, rptReleasePoi, pDisplayUnits->GetSpanLengthUnit(), false );
   INIT_UV_PROTOTYPE( rptPointOfInterest, rptErectedPoi, pDisplayUnits->GetSpanLengthUnit(), false );
   INIT_UV_PROTOTYPE( rptStressUnitValue, stress, pDisplayUnits->GetStressUnit(), true );
   INIT_UV_PROTOTYPE( rptForceUnitValue, force, pDisplayUnits->GetGeneralForceUnit(), true );
   INIT_UV_PROTOTYPE(rptLengthUnitValue, ecc, pDisplayUnits->GetComponentDimUnit(), true);

   //gdrpoi.IncludeSpanAndGirder(span == ALL_SPANS);
   //spanpoi.IncludeSpanAndGirder(span == ALL_SPANS);

   GET_IFACE2(pBroker,ILossParameters,pLossParams);
   bool bTimeStepAnalysis = pLossParams->GetLossMethod() == pgsTypes::TIME_STEP ? true : false;

   // for transformed section analysis, stresses are computed with prestress force P at the start of the interval because elastic effects are intrinsic to the stress analysis
   // for gross section analysis, stresses are computed with prestress force P at the end of the interval because the elastic effects during the interval must be included in the prestress force
   GET_IFACE2(pBroker,ISectionProperties,pSectProps);
   pgsTypes::IntervalTimeType intervalTime = (pSectProps->GetSectionPropertiesMode() == pgsTypes::spmTransformed && !bTimeStepAnalysis ? pgsTypes::Start : pgsTypes::End);

   GET_IFACE2(pBroker,IIntervals,pIntervals);
   std::vector<IntervalIndexType> vIntervals(pIntervals->GetSpecCheckIntervals(segmentKey));
   IntervalIndexType nIntervals = vIntervals.size();
   IntervalIndexType liveLoadIntervalIdx = pIntervals->GetLiveLoadInterval();
   IntervalIndexType loadRatingIntervalIdx = pIntervals->GetLoadRatingInterval();
   IntervalIndexType releaseIntervalIdx = pIntervals->GetPrestressReleaseInterval(segmentKey);

   if ( bTimeStepAnalysis )
   {
      // for time step analysis, we want the stress in the girder due to pretensioning
      // at release (see description below)
      loadRatingIntervalIdx = releaseIntervalIdx;
   }

   ColumnIndexType nColumns;
   if ( bDesign )
   {
      nColumns = 2 // two location columns
               + nIntervals; // one for each interval
   }
   else
   {
      // Load Rating
      nColumns = 2; // location column and column for live load stage
   }

   rptRcTable* p_table = rptStyleManager::CreateDefaultTable(nColumns,_T("Girder Stresses"));

   if ( segmentKey.groupIndex == ALL_GROUPS )
   {
      p_table->SetColumnStyle(0,rptStyleManager::GetTableCellStyle(CB_NONE | CJ_LEFT));
      p_table->SetStripeRowColumnStyle(0,rptStyleManager::GetTableStripeRowCellStyle(CB_NONE | CJ_LEFT));

      if ( bDesign )
      {
         p_table->SetColumnStyle(1,rptStyleManager::GetTableCellStyle(CB_NONE | CJ_LEFT));
         p_table->SetStripeRowColumnStyle(1,rptStyleManager::GetTableStripeRowCellStyle(CB_NONE | CJ_LEFT));
      }
   }

   // Set up table headings
   ColumnIndexType col = 0;
   if ( bDesign )
   {
      (*p_table)(0,col++) << COLHDR(RPT_GDR_END_LOCATION, rptLengthUnitTag, pDisplayUnits->GetSpanLengthUnit() );
      (*p_table)(0,col++) << COLHDR(RPT_LFT_SUPPORT_LOCATION, rptLengthUnitTag, pDisplayUnits->GetSpanLengthUnit() );

      std::vector<IntervalIndexType>::iterator iter(vIntervals.begin());
      std::vector<IntervalIndexType>::iterator end(vIntervals.end());
      for ( ; iter != end; iter++ )
      {
         IntervalIndexType intervalIdx = *iter;
         (*p_table)(0,col++) << _T("Interval ") << LABEL_INTERVAL(intervalIdx) << rptNewLine << pIntervals->GetDescription(intervalIdx);
      }
   }
   else
   {
      (*p_table)(0,col++) << COLHDR(RPT_LFT_SUPPORT_LOCATION, rptLengthUnitTag, pDisplayUnits->GetSpanLengthUnit() );
      (*p_table)(0,col++) << _T("Interval ") << LABEL_INTERVAL(loadRatingIntervalIdx) << rptNewLine << pIntervals->GetDescription(loadRatingIntervalIdx);
   }

   GET_IFACE2(pBroker,IPointOfInterest,pPoi);
   std::vector<pgsPointOfInterest> vPoi( pPoi->GetPointsOfInterest(segmentKey,POI_RELEASED_SEGMENT) );
   std::vector<pgsPointOfInterest> vPoi2( pPoi->GetPointsOfInterest(segmentKey,POI_ERECTED_SEGMENT) );
   std::vector<pgsPointOfInterest> vPoi3( pPoi->GetPointsOfInterest(segmentKey,POI_START_FACE | POI_END_FACE | POI_HARPINGPOINT | POI_PSXFER | POI_DEBOND,POIFIND_OR) );
   vPoi.insert(vPoi.end(),vPoi2.begin(),vPoi2.end());
   vPoi.insert(vPoi.end(),vPoi3.begin(),vPoi3.end());
   std::sort(vPoi.begin(),vPoi.end());
   vPoi.erase(std::unique(vPoi.begin(),vPoi.end()),vPoi.end());
   pPoi->RemovePointsOfInterest(vPoi,POI_CLOSURE);
   pPoi->RemovePointsOfInterest(vPoi,POI_BOUNDARY_PIER);

   GET_IFACE2(pBroker,IPretensionStresses,pPrestress);
   GET_IFACE2(pBroker,IPretensionForce,pForce);
   GET_IFACE2(pBroker, IStrandGeometry, pStrandGeom);

   StrandIndexType Nt = pStrandGeom->GetStrandCount(segmentKey, pgsTypes::Temporary);

   // Fill up the table
   RowIndexType row = p_table->GetNumberOfHeaderRows();

   for (const auto& poi : vPoi)
   {
      col = 0;

      Float64 Ns;
      Float64 ep = pStrandGeom->GetEccentricity(releaseIntervalIdx, poi, pgsTypes::Permanent, &Ns);
      Float64 et = pStrandGeom->GetEccentricity(releaseIntervalIdx, poi, pgsTypes::Temporary, &Ns);

      if ( bDesign )
      {
         (*p_table)(row,col++) << rptReleasePoi.SetValue( POI_RELEASED_SEGMENT, poi );
      }
      (*p_table)(row,col++) << rptErectedPoi.SetValue( POI_ERECTED_SEGMENT, poi  );

      if ( bDesign )
      {
         for (auto intervalIdx : vIntervals)
         {
            if ( bTimeStepAnalysis )
            {
               // if we are doing a time-step analysis the stress in the girder
               // due to pretensioning is that at release. Girder stresses already
               // include time-dependent effects (CR, SH, RE). We don't want to
               // report stresses in the girder computed using an effective prestress
               // that also includes (is reduced by) creep, shrinkage, and relaxation
               intervalIdx = releaseIntervalIdx;
            }

            if (intervalIdx < liveLoadIntervalIdx)
            {
               Float64 Fp = pForce->GetPrestressForce(poi, pgsTypes::Permanent, intervalIdx, intervalTime);
               Float64 Ft = pForce->GetPrestressForce(poi, pgsTypes::Temporary, intervalIdx, intervalTime);
               (*p_table)(row, col) << Sub2(_T("P"),_T("e")) << _T(" (permanent) = ") << force.SetValue(Fp) << _T(" ") << Sub2(_T("e"), _T("p")) << _T(" = ") << ecc.SetValue(ep) << rptNewLine;
               if (0 < Nt)
               {
                  (*p_table)(row, col) << Sub2(_T("P"), _T("e")) << _T(" (temporary) = ") << force.SetValue(Ft) << _T(" ") << Sub2(_T("e"), _T("t")) << _T(" = ") << ecc.SetValue(et) << rptNewLine;
               }

               Float64 fTop = pPrestress->GetStress(intervalIdx, poi, pgsTypes::TopGirder, true/*include live load if applicable*/, pgsTypes::ServiceI);
               Float64 fBot = pPrestress->GetStress(intervalIdx, poi, pgsTypes::BottomGirder, true/*include live load if applicable*/, pgsTypes::ServiceI);
               (*p_table)(row, col) << RPT_FTOP << _T(" = ") << stress.SetValue(fTop) << rptNewLine;
               (*p_table)(row, col) << RPT_FBOT << _T(" = ") << stress.SetValue(fBot);
            }
            else
            {
               Float64 Fp = pForce->GetPrestressForceWithLiveLoad(poi, pgsTypes::Permanent, pgsTypes::ServiceI);
               Float64 Ft = pForce->GetPrestressForceWithLiveLoad(poi, pgsTypes::Temporary, pgsTypes::ServiceI);
               (*p_table)(row, col) << _T("Service I") << rptNewLine;
               (*p_table)(row, col) << Sub2(_T("P"),_T("e")) << _T(" (permanent) = ") << force.SetValue(Fp) << _T(" ") << Sub2(_T("e"), _T("p")) << _T(" = ") << ecc.SetValue(ep) << rptNewLine;
               if (0 < Nt)
               {
                  (*p_table)(row, col) << Sub2(_T("P"), _T("e")) << _T(" (temporary) = ") << force.SetValue(Ft) << _T(" ") << Sub2(_T("e"), _T("t")) << _T(" = ") << ecc.SetValue(et) << rptNewLine;
               }

               Float64 fTop = pPrestress->GetStress(intervalIdx, poi, pgsTypes::TopGirder, true/*include live load if applicable*/, pgsTypes::ServiceI);
               Float64 fBot = pPrestress->GetStress(intervalIdx, poi, pgsTypes::BottomGirder, true/*include live load if applicable*/, pgsTypes::ServiceI);
               (*p_table)(row, col) << RPT_FTOP << _T(" = ") << stress.SetValue(fTop) << rptNewLine;
               (*p_table)(row, col) << RPT_FBOT << _T(" = ") << stress.SetValue(fBot) << rptNewLine;

               (*p_table)(row, col) << rptNewLine;

               Fp = pForce->GetPrestressForceWithLiveLoad(poi, pgsTypes::Permanent, pgsTypes::ServiceIII);
               Ft = pForce->GetPrestressForceWithLiveLoad(poi, pgsTypes::Temporary, pgsTypes::ServiceIII);
               (*p_table)(row, col) << _T("Service III") << rptNewLine;
               (*p_table)(row, col) << Sub2(_T("P"),_T("e")) << _T(" (permanent) = ") << force.SetValue(Fp) << _T(" ") << Sub2(_T("e"), _T("p")) << _T(" = ") << ecc.SetValue(ep) << rptNewLine;
               if (0 < Nt)
               {
                  (*p_table)(row, col) << Sub2(_T("P"), _T("e")) << _T(" (temporary) = ") << force.SetValue(Ft) << _T(" ") << Sub2(_T("e"), _T("t")) << _T(" = ") << ecc.SetValue(et) << rptNewLine;
               }

               fTop = pPrestress->GetStress(intervalIdx, poi, pgsTypes::TopGirder, true/*include live load if applicable*/, pgsTypes::ServiceIII);
               fBot = pPrestress->GetStress(intervalIdx, poi, pgsTypes::BottomGirder, true/*include live load if applicable*/, pgsTypes::ServiceIII);
               (*p_table)(row, col) << RPT_FTOP << _T(" = ") << stress.SetValue(fTop) << rptNewLine;
               (*p_table)(row, col) << RPT_FBOT << _T(" = ") << stress.SetValue(fBot) << rptNewLine;

               (*p_table)(row, col) << rptNewLine;

               pgsTypes::LimitState ls = (lrfdVersionMgr::GetVersion() < lrfdVersionMgr::FourthEditionWith2009Interims ? pgsTypes::ServiceIA : pgsTypes::FatigueI);
               std::_tstring strLS(ls == pgsTypes::ServiceIA ? _T(" (Service IA)") : _T(" (Fatigue I)"));

               Fp = pForce->GetPrestressForceWithLiveLoad(poi, pgsTypes::Permanent, ls);
               Ft = pForce->GetPrestressForceWithLiveLoad(poi, pgsTypes::Temporary, ls);
               (*p_table)(row, col) << strLS << rptNewLine;
               (*p_table)(row, col) << Sub2(_T("P"),_T("e")) << _T(" (permanent) = ") << force.SetValue(Fp) << _T(" ") << Sub2(_T("e"), _T("p")) << _T(" = ") << ecc.SetValue(ep) << rptNewLine;
               if (0 < Nt)
               {
                  (*p_table)(row, col) << Sub2(_T("P"), _T("e")) << _T(" (temporary) = ") << force.SetValue(Ft) << _T(" ") << Sub2(_T("e"), _T("t")) << _T(" = ") << ecc.SetValue(et) << rptNewLine;
               }

               fTop = pPrestress->GetStress(intervalIdx, poi, pgsTypes::TopGirder, true/*include live load if applicable*/, ls);
               fBot = pPrestress->GetStress(intervalIdx, poi, pgsTypes::BottomGirder, true/*include live load if applicable*/, ls);
               (*p_table)(row, col) << RPT_FTOP << _T(" = ") << stress.SetValue(fTop) << rptNewLine;
               (*p_table)(row, col) << RPT_FBOT << _T(" = ") << stress.SetValue(fBot) << rptNewLine;
            }

            col++;
         }
      }
      else
      {
         // Rating
         GET_IFACE2(pBroker, IProductLoads, pProductLoads);
         GET_IFACE2(pBroker, IRatingSpecification, pRatingSpec);
         int nReported = 0;
         int nRatingTypes = (int)pgsTypes::lrLoadRatingTypeCount;
         for (int i = 0; i < nRatingTypes; i++)
         {
            pgsTypes::LoadRatingType ratingType = (pgsTypes::LoadRatingType)i;
            if (pRatingSpec->IsRatingEnabled(ratingType) && pRatingSpec->RateForStress(ratingType))
            {
               pgsTypes::LimitState limitState = ::GetServiceLimitStateType(ratingType);
               ATLASSERT(IsServiceIIILimitState(limitState)); // must be one of the Service III limit states

               if (IsDesignRatingType(ratingType))
               {
                  Float64 Fp = pForce->GetPrestressForceWithLiveLoad(poi, pgsTypes::Permanent, limitState);
                  (*p_table)(row, col) << GetLimitStateString(limitState) << rptNewLine;
                  (*p_table)(row, col) << Sub2(_T("P"),_T("e")) << _T(" (permanent) = ") << force.SetValue(Fp) << _T(" ") << Sub2(_T("e"), _T("p")) << _T(" = ") << ecc.SetValue(ep) << rptNewLine;

                  Float64 fTop = pPrestress->GetStress(loadRatingIntervalIdx, poi, pgsTypes::TopGirder, true/*include live load if applicable*/, limitState);
                  Float64 fBot = pPrestress->GetStress(loadRatingIntervalIdx, poi, pgsTypes::BottomGirder, true/*include live load if applicable*/, limitState);
                  (*p_table)(row, col) << RPT_FTOP << _T(" = ") << stress.SetValue(fTop) << rptNewLine;
                  (*p_table)(row, col) << RPT_FBOT << _T(" = ") << stress.SetValue(fBot) << rptNewLine;
               }
               else
               {
                  pgsTypes::LiveLoadType llType = LiveLoadTypeFromLimitState(limitState);
                  VehicleIndexType nVehicles = pProductLoads->GetVehicleCount(llType);
                  for (VehicleIndexType vehicleIdx = 0; vehicleIdx < nVehicles; vehicleIdx++)
                  {
                     if (pProductLoads->GetLiveLoadApplicability(llType, vehicleIdx) == pgsTypes::llaNegMomentAndInteriorPierReaction)
                     {
                        continue;
                     }
                     Float64 Fp = pForce->GetPrestressForceWithLiveLoad(poi, pgsTypes::Permanent, limitState, vehicleIdx);
                     std::_tstring name = pProductLoads->GetLiveLoadName(llType, vehicleIdx);
                     (*p_table)(row, col) << GetLimitStateString(limitState) << _T(", ") << name << rptNewLine;
                     (*p_table)(row, col) << Sub2(_T("P"),_T("e")) << _T(" (permanent) = ") << force.SetValue(Fp) << _T(" ") << Sub2(_T("e"), _T("p")) << _T(" = ") << ecc.SetValue(ep) << rptNewLine;

                     Float64 fTop = pPrestress->GetStress(loadRatingIntervalIdx, poi, pgsTypes::TopGirder, true/*include live load if applicable*/, limitState, vehicleIdx);
                     Float64 fBot = pPrestress->GetStress(loadRatingIntervalIdx, poi, pgsTypes::BottomGirder, true/*include live load if applicable*/, limitState, vehicleIdx);
                     (*p_table)(row, col) << RPT_FTOP << _T(" = ") << stress.SetValue(fTop) << rptNewLine;
                     (*p_table)(row, col) << RPT_FBOT << _T(" = ") << stress.SetValue(fBot);

                     if ( vehicleIdx != nVehicles-1)
                     {
                        (*p_table)(row, col) << rptNewLine;
                     }
                  }
               }

               if (i != nRatingTypes - 1)
               {
                  (*p_table)(row, col) << rptNewLine;
               }

               nReported++;
            }
         }

         if (nReported == 0)
         {
            (*p_table)(row, col) << _T("");
         }
         col++;
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
void CPretensionStressTable::MakeCopy(const CPretensionStressTable& rOther)
{
   // Add copy code here...
}

void CPretensionStressTable::MakeAssignment(const CPretensionStressTable& rOther)
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

