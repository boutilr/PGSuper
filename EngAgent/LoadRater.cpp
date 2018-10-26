///////////////////////////////////////////////////////////////////////
// PGSuper - Prestressed Girder SUPERstructure Design and Analysis
// Copyright � 1999-2015  Washington State Department of Transportation
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

#include "stdafx.h"
#include "LoadRater.h"
#include "Designer2.h"

#include <IFace\Project.h> // for ISpecification
#include <IFace\RatingSpecification.h>
#include <IFace\Bridge.h>
#include <IFace\MomentCapacity.h>
#include <IFace\ShearCapacity.h>
#include <IFace\CrackedSection.h>
#include <IFace\PrestressForce.h>
#include <IFace\DistributionFactors.h>

#define RF_MAX 9999999
Float64 sum_values(Float64 a,Float64 b) { return a + b; }
void special_transform(IBridge* pBridge,
                       std::vector<pgsPointOfInterest>::const_iterator poiBeginIter,
                       std::vector<pgsPointOfInterest>::const_iterator poiEndIter,
                       std::vector<Float64>::iterator forceBeginIter,
                       std::vector<Float64>::iterator resultBeginIter,
                       std::vector<Float64>::iterator outputBeginIter);

bool AxleHasWeight(AxlePlacement& placement)
{
   return !IsZero(placement.Weight);
}

pgsLoadRater::pgsLoadRater(void)
{
   CREATE_LOGFILE("LoadRating");
}

pgsLoadRater::~pgsLoadRater(void)
{
   CLOSE_LOGFILE;
}

void pgsLoadRater::SetBroker(IBroker* pBroker)
{
   m_pBroker = pBroker;
}

pgsRatingArtifact pgsLoadRater::Rate(GirderIndexType gdrLineIdx,pgsTypes::LoadRatingType ratingType,VehicleIndexType vehicleIdx)
{
   GET_IFACE(IRatingSpecification,pRatingSpec);

   pgsRatingArtifact ratingArtifact;

   // Rate for positive moment - flexure
   MomentRating(gdrLineIdx,true,ratingType,vehicleIdx,ratingArtifact);

   // Rate for negative moment - flexure, if applicable
   GET_IFACE(IBridge,pBridge);
   if ( pBridge->ProcessNegativeMoments(ALL_SPANS) )
      MomentRating(gdrLineIdx,false,ratingType,vehicleIdx,ratingArtifact);

   // Rate for shear if applicable
   if ( pRatingSpec->RateForShear(ratingType) )
      ShearRating(gdrLineIdx,ratingType,vehicleIdx,ratingArtifact);

   // Rate for stress if applicable
   if ( pRatingSpec->RateForStress(ratingType) )
   {
      if ( ratingType == pgsTypes::lrPermit_Routine || ratingType == pgsTypes::lrPermit_Special )
      {
         // Service I reinforcement yield check if permit rating
         CheckReinforcementYielding(gdrLineIdx,ratingType,vehicleIdx,true,ratingArtifact);

         if ( pBridge->ProcessNegativeMoments(ALL_SPANS) )
            CheckReinforcementYielding(gdrLineIdx,ratingType,vehicleIdx,false,ratingArtifact);
      }
      else
      {
         // Service III flexure if other rating type
         StressRating(gdrLineIdx,ratingType,vehicleIdx,ratingArtifact);
      }
   }

   return ratingArtifact;
}

void pgsLoadRater::MomentRating(GirderIndexType gdrLineIdx,bool bPositiveMoment,pgsTypes::LoadRatingType ratingType,VehicleIndexType vehicleIdx,pgsRatingArtifact& ratingArtifact)
{
   GET_IFACE(IPointOfInterest,pPOI);
   std::vector<pgsPointOfInterest> vPOI = pPOI->GetPointsOfInterest(ALL_SPANS,gdrLineIdx,pgsTypes::BridgeSite3,POI_ALL, POIFIND_OR);

   std::vector<Float64> vDCmin, vDCmax;
   std::vector<Float64> vDWmin, vDWmax;
   std::vector<Float64> vLLIMmin,vLLIMmax;
   std::vector<Float64> vPLmin,vPLmax;
   std::vector<VehicleIndexType> vMinTruckIndex, vMaxTruckIndex;
   GetMoments(gdrLineIdx,bPositiveMoment,ratingType, vehicleIdx, vPOI, vDCmin, vDCmax, vDWmin, vDWmax, vLLIMmin, vMinTruckIndex, vLLIMmax, vMaxTruckIndex,vPLmin,vPLmax);

   GET_IFACE(IMomentCapacity,pMomentCapacity);
   std::vector<MOMENTCAPACITYDETAILS> vM = pMomentCapacity->GetMomentCapacityDetails(pgsTypes::BridgeSite3,vPOI,bPositiveMoment);
   std::vector<MINMOMENTCAPDETAILS> vMmin = pMomentCapacity->GetMinMomentCapacityDetails(pgsTypes::BridgeSite3,vPOI,bPositiveMoment);

   ATLASSERT(vPOI.size()     == vDCmax.size());
   ATLASSERT(vPOI.size()     == vDWmax.size());
   ATLASSERT(vPOI.size()     == vLLIMmax.size());
   ATLASSERT(vPOI.size()     == vM.size());
   ATLASSERT(vDCmin.size()   == vDCmax.size());
   ATLASSERT(vDWmin.size()   == vDWmax.size());
   ATLASSERT(vLLIMmin.size() == vLLIMmax.size());

   GET_IFACE(IRatingSpecification,pRatingSpec);
   Float64 system_factor    = pRatingSpec->GetSystemFactorFlexure();
   bool bIncludePL = pRatingSpec->IncludePedestrianLiveLoad();

   pgsTypes::LimitState ls = GetStrengthLimitStateType(ratingType);

   Float64 gDC = pRatingSpec->GetDeadLoadFactor(ls);
   Float64 gDW = pRatingSpec->GetWearingSurfaceFactor(ls);

   GET_IFACE(IProductLoads,pProductLoads);
   pgsTypes::LiveLoadType llType = GetLiveLoadType(ratingType);
   std::vector<std::_tstring> strLLNames = pProductLoads->GetVehicleNames(llType,gdrLineIdx);

   GET_IFACE(IProductForces,pProductForces);
   GET_IFACE(ISpecification,pSpec);
   pgsTypes::AnalysisType analysisType = pSpec->GetAnalysisType();

   CollectionIndexType nPOI = vPOI.size();
   for ( CollectionIndexType i = 0; i < nPOI; i++ )
   {
      pgsPointOfInterest& poi = vPOI[i];

      SpanIndexType spanIdx = poi.GetSpan();
      GirderIndexType gdrIdx = poi.GetGirder();
      Float64 condition_factor = (bPositiveMoment ? pRatingSpec->GetGirderConditionFactor(spanIdx,gdrIdx) 
                                                  : pRatingSpec->GetDeckConditionFactor() );

      Float64 DC   = (bPositiveMoment ? vDCmax[i]   : vDCmin[i]);
      Float64 DW   = (bPositiveMoment ? vDWmax[i]   : vDWmin[i]);
      Float64 LLIM = (bPositiveMoment ? vLLIMmax[i] : vLLIMmin[i]);
      Float64 PL   = (bIncludePL ? (bPositiveMoment ? vPLmax[i]   : vPLmin[i]) : 0.0);

      VehicleIndexType truck_index = vehicleIdx;
      if ( vehicleIdx == INVALID_INDEX )
         truck_index = (bPositiveMoment ? vMaxTruckIndex[i] : vMinTruckIndex[i]);

      Float64 gLL = pRatingSpec->GetLiveLoadFactor(ls,true);
      if ( gLL < 0 )
      {
         if ( ::IsStrengthLimitState(ls) )
         {
            Float64 Mmin, Mmax, Dummy;
            AxleConfiguration MinAxleConfig, MaxAxleConfig, DummyAxleConfig;
            if ( analysisType == pgsTypes::Envelope )
            {
               pProductForces->GetVehicularLiveLoadMoment(llType,truck_index,pgsTypes::BridgeSite3,poi,MinSimpleContinuousEnvelope,true,true,&Mmin,&Dummy,&MinAxleConfig,&DummyAxleConfig);
               pProductForces->GetVehicularLiveLoadMoment(llType,truck_index,pgsTypes::BridgeSite3,poi,MaxSimpleContinuousEnvelope,true,true,&Dummy,&Mmax,&DummyAxleConfig,&MaxAxleConfig);
            }
            else
            {
               pProductForces->GetVehicularLiveLoadMoment(llType,truck_index,pgsTypes::BridgeSite3,poi,analysisType == pgsTypes::Simple ? SimpleSpan : ContinuousSpan,true,true,&Mmin,&Mmax,&MinAxleConfig,&MaxAxleConfig);
            }

            gLL = GetStrengthLiveLoadFactor(ratingType,bPositiveMoment ? MaxAxleConfig : MinAxleConfig);
         }
         else
         {
            gLL = GetServiceLiveLoadFactor(ratingType);
         }
      }

      Float64 phi_moment = vM[i].Phi; 
      Float64 Mn = vM[i].Mn;

      // NOTE: K can be less than zero when we are rating for negative moment and the minumum moment demand (Mu)
      // is positive. This happens near the simple ends of spans. For example Mr < 0 because we are rating for
      // negative moment and Mmin = min (1.2Mcr and 1.33Mu)... Mcr < 0 because we are looking at negative moment
      // and Mu > 0.... Since we are looking at the negative end of things, Mmin = 1.33Mu. +/- = -... it doesn't
      // make since for K to be negative... K < 0 indicates that the section is most definate NOT under reinforced.
      // No adjustment needs to be made for underreinforcement so take K = 1.0
      Float64 K = (IsZero(vMmin[i].MrMin) ? 1.0 : vMmin[i].Mr/vMmin[i].MrMin);
      if ( K < 0.0 || 1.0 < K )
         K = 1.0;


      std::_tstring strVehicleName = strLLNames[truck_index];
      Float64 W = pProductLoads->GetVehicleWeight(llType,truck_index);

      pgsMomentRatingArtifact momentArtifact;
      momentArtifact.SetRatingType(ratingType);
      momentArtifact.SetPointOfInterest(poi);
      momentArtifact.SetVehicleIndex(truck_index);
      momentArtifact.SetVehicleWeight(W);
      momentArtifact.SetVehicleName(strVehicleName.c_str());
      momentArtifact.SetSystemFactor(system_factor);
      momentArtifact.SetConditionFactor(condition_factor);
      momentArtifact.SetCapacityReductionFactor(phi_moment);
      momentArtifact.SetMinimumReinforcementFactor(K);
      momentArtifact.SetNominalMomentCapacity(Mn);
      momentArtifact.SetDeadLoadFactor(gDC);
      momentArtifact.SetDeadLoadMoment(DC);
      momentArtifact.SetWearingSurfaceFactor(gDW);
      momentArtifact.SetWearingSurfaceMoment(DW);
      momentArtifact.SetLiveLoadFactor(gLL);
      momentArtifact.SetLiveLoadMoment(LLIM+PL);

      ratingArtifact.AddArtifact(poi,momentArtifact,bPositiveMoment);
   }
}

void pgsLoadRater::ShearRating(GirderIndexType gdrLineIdx,pgsTypes::LoadRatingType ratingType,VehicleIndexType vehicleIdx,pgsRatingArtifact& ratingArtifact)
{
   GET_IFACE(ISpecification,pSpec);
   pgsTypes::AnalysisType analysisType = pSpec->GetAnalysisType();

   pgsTypes::LimitState ls = GetStrengthLimitStateType(ratingType);

   GET_IFACE(IPointOfInterest,pPOI);
   std::vector<pgsPointOfInterest> vPOI = pPOI->GetPointsOfInterest(ALL_SPANS,gdrLineIdx,pgsTypes::BridgeSite3,POI_ALL, POIFIND_OR);

   std::vector<sysSectionValue> vDCmin, vDCmax;
   std::vector<sysSectionValue> vDWmin, vDWmax;
   std::vector<sysSectionValue> vLLIMmin,vLLIMmax;
   std::vector<sysSectionValue> vUnused;
   std::vector<VehicleIndexType> vMinTruckIndex, vMaxTruckIndex, vUnusedIndex;
   std::vector<sysSectionValue> vPLmin, vPLmax;

   pgsTypes::LiveLoadType llType = GetLiveLoadType(ratingType);

   GET_IFACE(ICombinedForces2,pCombinedForces);
   GET_IFACE(IProductForces2,pProductForces);
   if ( analysisType == pgsTypes::Envelope )
   {
      vDCmin = pCombinedForces->GetShear(lcDC,pgsTypes::BridgeSite2,vPOI,ctCummulative,MinSimpleContinuousEnvelope);
      vDCmax = pCombinedForces->GetShear(lcDC,pgsTypes::BridgeSite2,vPOI,ctCummulative,MaxSimpleContinuousEnvelope);
      vDWmin = pCombinedForces->GetShear(lcDWRating,pgsTypes::BridgeSite2,vPOI,ctCummulative,MinSimpleContinuousEnvelope);
      vDWmax = pCombinedForces->GetShear(lcDWRating,pgsTypes::BridgeSite2,vPOI,ctCummulative,MaxSimpleContinuousEnvelope);

      if ( vehicleIdx == INVALID_INDEX )
      {
         pProductForces->GetLiveLoadShear( llType, pgsTypes::BridgeSite3, vPOI, MinSimpleContinuousEnvelope, true, true, &vLLIMmin, &vUnused, &vMinTruckIndex, &vUnusedIndex );
         pProductForces->GetLiveLoadShear( llType, pgsTypes::BridgeSite3, vPOI, MaxSimpleContinuousEnvelope, true, true, &vUnused, &vLLIMmax, &vUnusedIndex, &vMaxTruckIndex );
      }
      else
      {
         pProductForces->GetVehicularLiveLoadShear( llType, vehicleIdx, pgsTypes::BridgeSite3, vPOI, MinSimpleContinuousEnvelope, true, true, &vLLIMmin, &vUnused, NULL,NULL,NULL,NULL);
         pProductForces->GetVehicularLiveLoadShear( llType, vehicleIdx, pgsTypes::BridgeSite3, vPOI, MaxSimpleContinuousEnvelope, true, true, &vUnused, &vLLIMmax, NULL,NULL,NULL,NULL);
      }

      pCombinedForces->GetCombinedLiveLoadShear( pgsTypes::lltPedestrian, pgsTypes::BridgeSite3, vPOI, MaxSimpleContinuousEnvelope, false, &vUnused, &vPLmax );
      pCombinedForces->GetCombinedLiveLoadShear( pgsTypes::lltPedestrian, pgsTypes::BridgeSite3, vPOI, MinSimpleContinuousEnvelope, false, &vPLmin, &vUnused );
   }
   else
   {
      vDCmax = pCombinedForces->GetShear(lcDC,pgsTypes::BridgeSite2,vPOI,ctCummulative,analysisType == pgsTypes::Simple ? SimpleSpan : ContinuousSpan);
      vDCmin = vDCmax;
      vDWmax = pCombinedForces->GetShear(lcDWRating,pgsTypes::BridgeSite2,vPOI,ctCummulative,analysisType == pgsTypes::Simple ? SimpleSpan : ContinuousSpan);
      vDWmin = vDWmax;

      if ( vehicleIdx == INVALID_INDEX )
         pProductForces->GetLiveLoadShear( llType, pgsTypes::BridgeSite3, vPOI, analysisType == pgsTypes::Simple ? SimpleSpan : ContinuousSpan, true, true, &vLLIMmin, &vLLIMmax, &vMinTruckIndex, &vMaxTruckIndex );
      else
         pProductForces->GetVehicularLiveLoadShear( llType, vehicleIdx, pgsTypes::BridgeSite3, vPOI, analysisType == pgsTypes::Simple ? SimpleSpan : ContinuousSpan, true, true, &vLLIMmin, &vLLIMmax, NULL, NULL, NULL, NULL);

      pCombinedForces->GetCombinedLiveLoadShear( pgsTypes::lltPedestrian, pgsTypes::BridgeSite3, vPOI, analysisType == pgsTypes::Simple ? SimpleSpan : ContinuousSpan, false, &vPLmin, &vPLmax );
   }

   GET_IFACE(IShearCapacity,pShearCapacity);
   std::vector<SHEARCAPACITYDETAILS> vVn = pShearCapacity->GetShearCapacityDetails(ls,pgsTypes::BridgeSite3,vPOI);

   ATLASSERT(vPOI.size()     == vDCmax.size());
   ATLASSERT(vPOI.size()     == vDWmax.size());
   ATLASSERT(vPOI.size()     == vLLIMmax.size());
   ATLASSERT(vPOI.size()     == vVn.size());
   ATLASSERT(vDCmin.size()   == vDCmax.size());
   ATLASSERT(vDWmin.size()   == vDWmax.size());
   ATLASSERT(vLLIMmin.size() == vLLIMmax.size());
   ATLASSERT(vPLmin.size()   == vPLmax.size());

   GET_IFACE(IRatingSpecification,pRatingSpec);
   Float64 system_factor    = pRatingSpec->GetSystemFactorShear();
   bool bIncludePL = pRatingSpec->IncludePedestrianLiveLoad();

   Float64 gDC = pRatingSpec->GetDeadLoadFactor(ls);
   Float64 gDW = pRatingSpec->GetWearingSurfaceFactor(ls);

   GET_IFACE(IProductLoads,pProductLoads);
   std::vector<std::_tstring> strLLNames = pProductLoads->GetVehicleNames(llType,gdrLineIdx);

   GET_IFACE(IProductForces,pProductForce);

   CollectionIndexType nPOI = vPOI.size();
   for ( CollectionIndexType i = 0; i < nPOI; i++ )
   {
      pgsPointOfInterest& poi = vPOI[i];

      SpanIndexType spanIdx = poi.GetSpan();
      GirderIndexType gdrIdx = poi.GetGirder();
      Float64 condition_factor = pRatingSpec->GetGirderConditionFactor(spanIdx,gdrIdx);

      Float64 DCmin   = min(vDCmin[i].Left(),  vDCmin[i].Right());
      Float64 DCmax   = max(vDCmax[i].Left(),  vDCmax[i].Right());
      Float64 DWmin   = min(vDWmin[i].Left(),  vDWmin[i].Right());
      Float64 DWmax   = max(vDWmax[i].Left(),  vDWmax[i].Right());
      Float64 LLIMmin = min(vLLIMmin[i].Left(),vLLIMmin[i].Right());
      Float64 LLIMmax = max(vLLIMmax[i].Left(),vLLIMmax[i].Right());
      Float64 PLmin   = min(vPLmin[i].Left(),  vPLmin[i].Right());
      Float64 PLmax   = max(vPLmax[i].Left(),  vPLmax[i].Right());

      Float64 DC   = MaxMagnitude(DCmin,DCmax);
      Float64 DW   = MaxMagnitude(DWmin,DWmax);
      Float64 LLIM = MaxMagnitude(LLIMmin,LLIMmax);
      Float64 PL   = (bIncludePL ? MaxMagnitude(PLmin,PLmax) : 0);
      VehicleIndexType truck_index = vehicleIdx;
      if ( vehicleIdx == INVALID_INDEX )
         truck_index = (fabs(LLIMmin) < fabs(LLIMmax) ? vMaxTruckIndex[i] : vMinTruckIndex[i]);

      Float64 gLL = pRatingSpec->GetLiveLoadFactor(ls,true);
      if ( gLL < 0 )
      {
         // need to compute gLL based on axle weights
         if ( ::IsStrengthLimitState(ls) )
         {
            sysSectionValue Vmin, Vmax, Dummy;
            AxleConfiguration MinLeftAxleConfig, MaxLeftAxleConfig, MinRightAxleConfig, MaxRightAxleConfig, DummyLeftAxleConfig, DummyRightAxleConfig;
            if ( analysisType == pgsTypes::Envelope )
            {
               pProductForce->GetVehicularLiveLoadShear(llType,truck_index,pgsTypes::BridgeSite3,poi,MinSimpleContinuousEnvelope,true,true,&Vmin,&Dummy,&MinLeftAxleConfig,&MinRightAxleConfig,&DummyLeftAxleConfig,&DummyRightAxleConfig);
               pProductForce->GetVehicularLiveLoadShear(llType,truck_index,pgsTypes::BridgeSite3,poi,MaxSimpleContinuousEnvelope,true,true,&Dummy,&Vmax,&DummyLeftAxleConfig,&DummyRightAxleConfig,&MaxLeftAxleConfig,&MaxRightAxleConfig);
            }
            else
            {
               pProductForce->GetVehicularLiveLoadShear(llType,truck_index,pgsTypes::BridgeSite3,poi,analysisType == pgsTypes::Simple ? SimpleSpan : ContinuousSpan,true,true,&Vmin,&Vmax,&MinLeftAxleConfig,&MinRightAxleConfig,&MaxLeftAxleConfig,&MaxRightAxleConfig);
            }

            if ( fabs(LLIMmin) < fabs(LLIMmax) )
            {
               if (IsEqual(fabs(vLLIMmax[i].Left()),fabs(LLIMmax)))
                  gLL = GetStrengthLiveLoadFactor(ratingType,MaxLeftAxleConfig);
               else
                  gLL = GetStrengthLiveLoadFactor(ratingType,MaxRightAxleConfig);
            }
            else
            {
               if (IsEqual(fabs(vLLIMmin[i].Left()),fabs(LLIMmin)))
                  gLL = GetStrengthLiveLoadFactor(ratingType,MinLeftAxleConfig);
               else
                  gLL = GetStrengthLiveLoadFactor(ratingType,MinRightAxleConfig);
            }
         }
         else
         {
            gLL = GetServiceLiveLoadFactor(ratingType);
         }
      }

      Float64 phi_shear = vVn[i].Phi; 
      Float64 Vn = vVn[i].Vn;

      std::_tstring strVehicleName = strLLNames[truck_index];
      Float64 W = pProductLoads->GetVehicleWeight(llType,truck_index);

      pgsShearRatingArtifact shearArtifact;
      shearArtifact.SetRatingType(ratingType);
      shearArtifact.SetPointOfInterest(poi);
      shearArtifact.SetVehicleIndex(truck_index);
      shearArtifact.SetVehicleWeight(W);
      shearArtifact.SetVehicleName(strVehicleName.c_str());
      shearArtifact.SetSystemFactor(system_factor);
      shearArtifact.SetConditionFactor(condition_factor);
      shearArtifact.SetCapacityReductionFactor(phi_shear);
      shearArtifact.SetNominalShearCapacity(Vn);
      shearArtifact.SetDeadLoadFactor(gDC);
      shearArtifact.SetDeadLoadShear(DC);
      shearArtifact.SetWearingSurfaceFactor(gDW);
      shearArtifact.SetWearingSurfaceShear(DW);
      shearArtifact.SetLiveLoadFactor(gLL);
      shearArtifact.SetLiveLoadShear(LLIM+PL);

      // longitudinal steel check
      pgsLongReinfShearArtifact l_artifact;
      SHEARCAPACITYDETAILS scd;
      pgsDesigner2 designer;
      designer.SetBroker(m_pBroker);
      pShearCapacity->GetShearCapacityDetails(ls,pgsTypes::BridgeSite3,poi,&scd);
      designer.InitShearCheck(spanIdx,gdrIdx,vPOI,ls,NULL);
      designer.CheckLongReinfShear(poi,pgsTypes::BridgeSite3,ls,scd,NULL,&l_artifact);
      shearArtifact.SetLongReinfShearArtifact(l_artifact);

      ratingArtifact.AddArtifact(poi,shearArtifact);
   }
}

void pgsLoadRater::StressRating(GirderIndexType gdrLineIdx,pgsTypes::LoadRatingType ratingType,VehicleIndexType vehicleIdx,pgsRatingArtifact& ratingArtifact)
{
   ATLASSERT(ratingType == pgsTypes::lrDesign_Inventory || 
             ratingType == pgsTypes::lrLegal_Routine    ||
             ratingType == pgsTypes::lrLegal_Special ); // see MBE C6A.5.4.1

   GET_IFACE(ISpecification,pSpec);
   pgsTypes::AnalysisType analysisType = pSpec->GetAnalysisType();

   GET_IFACE(IPointOfInterest,pPOI);
   std::vector<pgsPointOfInterest> vPOI = pPOI->GetPointsOfInterest(ALL_SPANS,gdrLineIdx,pgsTypes::BridgeSite3,POI_ALL, POIFIND_OR);

   std::vector<Float64> vDCTopMin, vDCBotMin, vDCTopMax, vDCBotMax;
   std::vector<Float64> vDWTopMin, vDWBotMin, vDWTopMax, vDWBotMax;
   std::vector<Float64> vLLIMTopMin, vLLIMBotMin, vLLIMTopMax, vLLIMBotMax;
   std::vector<Float64> vUnused1,vUnused2;
   std::vector<VehicleIndexType> vTruckIndexTopMin, vTruckIndexTopMax, vTruckIndexBotMin, vTruckIndexBotMax;
   std::vector<VehicleIndexType> vUnusedIndex1, vUnusedIndex2;
   std::vector<Float64> vPLTopMin, vPLBotMin, vPLTopMax, vPLBotMax;

   pgsTypes::LiveLoadType llType = GetLiveLoadType(ratingType);

   GET_IFACE(ICombinedForces2,pCombinedForces);
   GET_IFACE(IProductForces2,pProductForces);
   if ( analysisType == pgsTypes::Envelope )
   {
      pCombinedForces->GetStress(lcDC,pgsTypes::BridgeSite2,vPOI,ctCummulative,MinSimpleContinuousEnvelope,&vDCTopMin,&vDCBotMin);
      pCombinedForces->GetStress(lcDC,pgsTypes::BridgeSite2,vPOI,ctCummulative,MaxSimpleContinuousEnvelope,&vDCTopMax,&vDCBotMax);
      pCombinedForces->GetStress(lcDWRating,pgsTypes::BridgeSite2,vPOI,ctCummulative,MinSimpleContinuousEnvelope,&vDWTopMin,&vDWBotMin);
      pCombinedForces->GetStress(lcDWRating,pgsTypes::BridgeSite2,vPOI,ctCummulative,MaxSimpleContinuousEnvelope,&vDWTopMax,&vDWBotMax);

      if ( vehicleIdx == INVALID_INDEX )
      {
         pProductForces->GetLiveLoadStress(llType,pgsTypes::BridgeSite3,vPOI,MinSimpleContinuousEnvelope,true,true,&vLLIMTopMin,&vUnused1,&vLLIMBotMin,&vUnused2,&vTruckIndexTopMin, &vUnusedIndex1, &vTruckIndexBotMin, &vUnusedIndex2);
         pProductForces->GetLiveLoadStress(llType,pgsTypes::BridgeSite3,vPOI,MaxSimpleContinuousEnvelope,true,true,&vUnused1,&vLLIMTopMax,&vUnused2,&vLLIMBotMax,&vUnusedIndex1, &vTruckIndexTopMax, &vUnusedIndex2, &vTruckIndexBotMax);
      }
      else
      {
         pProductForces->GetVehicularLiveLoadStress(llType,vehicleIdx,pgsTypes::BridgeSite3,vPOI,MinSimpleContinuousEnvelope,true,true,&vLLIMTopMin,&vUnused1,&vLLIMBotMin,&vUnused2,NULL,NULL,NULL,NULL);
         pProductForces->GetVehicularLiveLoadStress(llType,vehicleIdx,pgsTypes::BridgeSite3,vPOI,MaxSimpleContinuousEnvelope,true,true,&vUnused1,&vLLIMTopMax,&vUnused2,&vLLIMBotMax,NULL,NULL,NULL,NULL);
      }

      pCombinedForces->GetCombinedLiveLoadStress( pgsTypes::lltPedestrian, pgsTypes::BridgeSite3, vPOI, MaxSimpleContinuousEnvelope, &vUnused1,  &vPLTopMax, &vUnused2, &vPLBotMax );
      pCombinedForces->GetCombinedLiveLoadStress( pgsTypes::lltPedestrian, pgsTypes::BridgeSite3, vPOI, MinSimpleContinuousEnvelope, &vPLTopMin, &vUnused2,  &vPLBotMin, &vUnused2 );
   }
   else
   {
      pCombinedForces->GetStress(lcDC,pgsTypes::BridgeSite2,vPOI,ctCummulative,analysisType == pgsTypes::Simple ? SimpleSpan : ContinuousSpan,&vDCTopMin,&vDCBotMin);
      vDCTopMax = vDCTopMin;
      vDCBotMax = vDCBotMin;

      pCombinedForces->GetStress(lcDWRating,pgsTypes::BridgeSite2,vPOI,ctCummulative,analysisType == pgsTypes::Simple ? SimpleSpan : ContinuousSpan,&vDWTopMin,&vDWBotMin);
      vDWTopMax = vDWTopMin;
      vDWBotMax = vDWBotMin;

      if ( vehicleIdx == INVALID_INDEX )
         pProductForces->GetLiveLoadStress(llType,pgsTypes::BridgeSite3,vPOI,analysisType == pgsTypes::Simple ? SimpleSpan : ContinuousSpan,true,true,&vLLIMTopMin,&vLLIMTopMax,&vLLIMBotMin,&vLLIMBotMax,&vTruckIndexTopMin, &vTruckIndexTopMax, &vTruckIndexBotMin, &vTruckIndexBotMax);
      else
         pProductForces->GetVehicularLiveLoadStress(llType,vehicleIdx,pgsTypes::BridgeSite3,vPOI,analysisType == pgsTypes::Simple ? SimpleSpan : ContinuousSpan,true,true,&vLLIMTopMin,&vLLIMTopMax,&vLLIMBotMin,&vLLIMBotMax,NULL,NULL,NULL,NULL);

      pCombinedForces->GetCombinedLiveLoadStress( pgsTypes::lltPedestrian, pgsTypes::BridgeSite3, vPOI, analysisType == pgsTypes::Simple ? SimpleSpan : ContinuousSpan, &vPLTopMin, &vPLTopMax,  &vPLBotMin, &vPLBotMax );
   }

   ATLASSERT(vPOI.size()     == vDCTopMin.size());
   ATLASSERT(vPOI.size()     == vDCTopMax.size());
   ATLASSERT(vPOI.size()     == vDCBotMin.size());
   ATLASSERT(vPOI.size()     == vDCBotMax.size());
   ATLASSERT(vPOI.size()     == vDWTopMin.size());
   ATLASSERT(vPOI.size()     == vDWTopMax.size());
   ATLASSERT(vPOI.size()     == vDWBotMin.size());
   ATLASSERT(vPOI.size()     == vDWBotMax.size());
   ATLASSERT(vPOI.size()     == vLLIMTopMin.size());
   ATLASSERT(vPOI.size()     == vLLIMTopMax.size());
   ATLASSERT(vPOI.size()     == vLLIMBotMin.size());
   ATLASSERT(vPOI.size()     == vLLIMBotMax.size());
   ATLASSERT(vPOI.size()     == vPLTopMin.size());
   ATLASSERT(vPOI.size()     == vPLTopMax.size());
   ATLASSERT(vPOI.size()     == vPLBotMin.size());
   ATLASSERT(vPOI.size()     == vPLBotMax.size());

   GET_IFACE(IPrestressStresses,pPrestress);
   std::vector<Float64> vPS = pPrestress->GetStress(pgsTypes::BridgeSite3,vPOI,pgsTypes::BottomGirder);

   GET_IFACE(IRatingSpecification,pRatingSpec);

   Float64 system_factor    = pRatingSpec->GetSystemFactorFlexure();
   bool bIncludePL = pRatingSpec->IncludePedestrianLiveLoad();

   pgsTypes::LimitState ls = GetServiceLimitStateType(ratingType);

   Float64 gDC = pRatingSpec->GetDeadLoadFactor(ls);
   Float64 gDW = pRatingSpec->GetWearingSurfaceFactor(ls);

   GET_IFACE(IProductLoads,pProductLoads);
   std::vector<std::_tstring> strLLNames = pProductLoads->GetVehicleNames(llType,gdrLineIdx);

   GET_IFACE(IProductForces,pProductForce);

   CollectionIndexType nPOI = vPOI.size();
   for ( CollectionIndexType i = 0; i < nPOI; i++ )
   {
      pgsPointOfInterest& poi = vPOI[i];

      SpanIndexType spanIdx = poi.GetSpan();
      GirderIndexType gdrIdx = poi.GetGirder();

      Float64 condition_factor = pRatingSpec->GetGirderConditionFactor(spanIdx,gdrIdx);
      Float64 fr               = pRatingSpec->GetAllowableTension(ratingType,spanIdx,gdrIdx);

      Float64 DC   = vDCBotMax[i];
      Float64 DW   = vDWBotMax[i];
      Float64 LLIM = vLLIMBotMax[i];
      Float64 PL   = (bIncludePL ? vPLBotMax[i] : 0);
      Float64 PS   = vPS[i];

      VehicleIndexType truck_index = vehicleIdx;
      if ( vehicleIdx == INVALID_INDEX )
         truck_index = vTruckIndexBotMax[i];

      Float64 gLL = pRatingSpec->GetLiveLoadFactor(ls,true);
      if ( gLL < 0 )
      {
         if ( ::IsStrengthLimitState(ls) )
         {
            // need to compute gLL based on axle weights
            Float64 fMinTop, fMinBot, fMaxTop, fMaxBot, fDummyTop, fDummyBot;
            AxleConfiguration MinAxleConfigTop, MinAxleConfigBot, MaxAxleConfigTop, MaxAxleConfigBot, DummyAxleConfigTop, DummyAxleConfigBot;
            if ( analysisType == pgsTypes::Envelope )
            {
               pProductForce->GetVehicularLiveLoadStress(llType,truck_index,pgsTypes::BridgeSite3,poi,MinSimpleContinuousEnvelope,true,true,&fMinTop,&fDummyTop,&fMinBot,&fDummyBot,&MinAxleConfigTop,&DummyAxleConfigTop,&MinAxleConfigBot,&DummyAxleConfigBot);
               pProductForce->GetVehicularLiveLoadStress(llType,truck_index,pgsTypes::BridgeSite3,poi,MaxSimpleContinuousEnvelope,true,true,&fDummyTop,&fMaxTop,&fDummyBot,&fMaxBot,&DummyAxleConfigTop,&MaxAxleConfigTop,&DummyAxleConfigBot,&MaxAxleConfigBot);
            }
            else
            {
               pProductForce->GetVehicularLiveLoadStress(llType,truck_index,pgsTypes::BridgeSite3,poi,analysisType == pgsTypes::Simple ? SimpleSpan : ContinuousSpan,true,true,&fMinTop,&fMaxTop,&fMaxTop,&fMaxBot,&MinAxleConfigTop,&MaxAxleConfigTop,&MinAxleConfigBot,&MaxAxleConfigBot);
            }

            gLL = GetStrengthLiveLoadFactor(ratingType,MaxAxleConfigBot);
         }
         else
         {
            gLL = GetServiceLiveLoadFactor(ratingType);
         }
      }


      std::_tstring strVehicleName = strLLNames[truck_index];
      Float64 W = pProductLoads->GetVehicleWeight(llType,truck_index);

      pgsStressRatingArtifact stressArtifact;
      stressArtifact.SetRatingType(ratingType);
      stressArtifact.SetPointOfInterest(poi);
      stressArtifact.SetVehicleIndex(truck_index);
      stressArtifact.SetVehicleWeight(W);
      stressArtifact.SetVehicleName(strVehicleName.c_str());
      stressArtifact.SetAllowableStress(fr);
      stressArtifact.SetDeadLoadFactor(gDC);
      stressArtifact.SetDeadLoadStress(DC);
      stressArtifact.SetPrestressStress(PS);
      stressArtifact.SetWearingSurfaceFactor(gDW);
      stressArtifact.SetWearingSurfaceStress(DW);
      stressArtifact.SetLiveLoadFactor(gLL);
      stressArtifact.SetLiveLoadStress(LLIM+PL);
   
      ratingArtifact.AddArtifact(poi,stressArtifact);
   }
}

void pgsLoadRater::CheckReinforcementYielding(GirderIndexType gdrLineIdx,pgsTypes::LoadRatingType ratingType,VehicleIndexType vehicleIdx,bool bPositiveMoment,pgsRatingArtifact& ratingArtifact)
{
   pgsTypes::LiveLoadType llType = GetLiveLoadType(ratingType);

   if ( bPositiveMoment )
   {
      GET_IFACE(IProductLoads,pProductLoads);
      VehicleIndexType nVehicles = pProductLoads->GetVehicleCount(llType);
      VehicleIndexType startVehicleIdx = (vehicleIdx == INVALID_INDEX ? 0 : vehicleIdx);
      VehicleIndexType endVehicleIdx   = (vehicleIdx == INVALID_INDEX ? nVehicles-1: startVehicleIdx);
      for ( VehicleIndexType vehIdx = startVehicleIdx; vehIdx <= endVehicleIdx; vehIdx++ )
      {
         pgsTypes::LiveLoadApplicabilityType applicability = pProductLoads->GetLiveLoadApplicability(llType,vehIdx);
         if ( applicability == pgsTypes::llaNegMomentAndInteriorPierReaction )
         {
            // we are processing positive moments and the live load vehicle is only applicable to negative moments
            return;
         }
      }
   }

   ATLASSERT(ratingType == pgsTypes::lrPermit_Routine || ratingType == pgsTypes::lrPermit_Special);
   GET_IFACE(IPointOfInterest,pPOI);
   std::vector<pgsPointOfInterest> vPOI = pPOI->GetPointsOfInterest(ALL_SPANS,gdrLineIdx,pgsTypes::BridgeSite3,POI_ALL, POIFIND_OR);

   std::vector<Float64> vDCmin, vDCmax;
   std::vector<Float64> vDWmin, vDWmax;
   std::vector<Float64> vLLIMmin,vLLIMmax;
   std::vector<VehicleIndexType> vMinTruckIndex, vMaxTruckIndex;
   std::vector<Float64> vPLmin,vPLmax;
   GetMoments(gdrLineIdx,bPositiveMoment,ratingType, vehicleIdx, vPOI, vDCmin, vDCmax, vDWmin, vDWmax, vLLIMmin, vMinTruckIndex, vLLIMmax, vMaxTruckIndex, vPLmin, vPLmax);

   pgsTypes::LimitState ls = GetServiceLimitStateType(ratingType);

   GET_IFACE(IRatingSpecification,pRatingSpec);
   bool bIncludePL = pRatingSpec->IncludePedestrianLiveLoad();

   GET_IFACE(IMomentCapacity,pMomentCapacity);
   std::vector<CRACKINGMOMENTDETAILS> vMcr = pMomentCapacity->GetCrackingMomentDetails(pgsTypes::BridgeSite3,vPOI,bPositiveMoment);
   std::vector<MOMENTCAPACITYDETAILS> vM = pMomentCapacity->GetMomentCapacityDetails(pgsTypes::BridgeSite3,vPOI,bPositiveMoment);

   GET_IFACE(ICrackedSection,pCrackedSection);
   std::vector<CRACKEDSECTIONDETAILS> vCrackedSection = pCrackedSection->GetCrackedSectionDetails(vPOI,bPositiveMoment);
   
   ATLASSERT(vPOI.size()     == vDCmax.size());
   ATLASSERT(vPOI.size()     == vDWmax.size());
   ATLASSERT(vPOI.size()     == vLLIMmax.size());
   ATLASSERT(vPOI.size()     == vMcr.size());
   ATLASSERT(vPOI.size()     == vM.size());
   ATLASSERT(vPOI.size()     == vCrackedSection.size());
   ATLASSERT(vDCmin.size()   == vDCmax.size());
   ATLASSERT(vDWmin.size()   == vDWmax.size());
   ATLASSERT(vLLIMmin.size() == vLLIMmax.size());
   ATLASSERT(vPLmin.size()   == vPLmax.size());


   // Get load factors
   Float64 gDC = pRatingSpec->GetDeadLoadFactor(ls);
   Float64 gDW = pRatingSpec->GetWearingSurfaceFactor(ls);

   // parameter needed for negative moment evalation
   // (get it outside the loop so we don't have to get it over and over)
   GET_IFACE(ILongRebarGeometry,pLongRebar);
   Float64 top_slab_cover = pLongRebar->GetCoverTopMat();

   GET_IFACE(IProductLoads,pProductLoads);
   std::vector<std::_tstring> strLLNames = pProductLoads->GetVehicleNames(llType,gdrLineIdx);

   GET_IFACE(IProductForces,pProductForces);
   GET_IFACE(ISpecification,pSpec);
   pgsTypes::AnalysisType analysisType = pSpec->GetAnalysisType();

   // Create artifacts
   GET_IFACE(IPrestressForce,pPrestressForce);
   GET_IFACE(ISectProp2,pSectProp);
   GET_IFACE(ILiveLoadDistributionFactors,pLLDF);
   CollectionIndexType nPOI = vPOI.size();
   for ( CollectionIndexType i = 0; i < nPOI; i++ )
   {
      pgsPointOfInterest& poi = vPOI[i];

      SpanIndexType spanIdx = poi.GetSpan();
      GirderIndexType gdrIdx = poi.GetGirder();

      // Get material properties
      GET_IFACE(IBridgeMaterial,pMaterial);
      Float64 Eg = pMaterial->GetEcGdr(spanIdx,gdrIdx);

      Float64 Es, fy, fu;
      if ( bPositiveMoment )
      {
         Es = pMaterial->GetStrand(spanIdx,gdrIdx,pgsTypes::Permanent)->GetE();
         fy = pMaterial->GetStrand(spanIdx,gdrIdx,pgsTypes::Permanent)->GetYieldStrength();
         fu = pMaterial->GetStrand(spanIdx,gdrIdx,pgsTypes::Permanent)->GetUltimateStrength();
      }
      else
      {
         pMaterial->GetDeckRebarProperties(&Es,&fy,&fu);
      }

      // Get allowable
      Float64 K = pRatingSpec->GetYieldStressLimitCoefficient();
      Float64 fr = K*fy; // 6A.5.4.2.2b 

      Float64 gM;
      if ( ratingType == pgsTypes::lrPermit_Special ) // if it is any of the special permit types
      {
         // The live load distribution factor used for special permits is one loaded lane without multiple presense factor.
         // However, when evaluating the reinforcement yielding in the Service I limit state (the thing this function does)
         // the controlling of one loaded lane and two or more loaded lanes live load distribution factors is to be used.
         // (See MBE 6A.4.5.4.2b and C6A.5.4.2.2b).
         //
         // vLLIMmin and vLLIMmax includes the one lane LLDF... divide out this LLDF and multiply by the correct LLDF

         Float64 gpM_Service, gnM_Service, gV;
         Float64 gpM_Fatigue, gnM_Fatigue;

         pLLDF->GetDistributionFactors(poi,pgsTypes::FatigueI,              &gpM_Fatigue,&gnM_Fatigue,&gV);
         pLLDF->GetDistributionFactors(poi,pgsTypes::ServiceI_PermitSpecial,&gpM_Service,&gnM_Service,&gV);

         if ( bPositiveMoment )
         {
            gM = gpM_Fatigue;
            vLLIMmax[i] *= gpM_Fatigue/gpM_Service;
         }
         else
         {
            gM = gnM_Fatigue;
            vLLIMmin[i] *= gnM_Fatigue/gnM_Service;
         }
      }
      else
      {
         ATLASSERT(ratingType == pgsTypes::lrPermit_Routine);
         Float64 gpM, gnM, gV;
         pLLDF->GetDistributionFactors(poi,pgsTypes::ServiceI_PermitRoutine,&gpM,&gnM,&gV);
         gM = (bPositiveMoment ? gpM : gnM);
      }

      Float64 DC   = (bPositiveMoment ? vDCmax[i]   : vDCmin[i]);
      Float64 DW   = (bPositiveMoment ? vDWmax[i]   : vDWmin[i]);
      Float64 LLIM = (bPositiveMoment ? vLLIMmax[i] : vLLIMmin[i]);
      Float64 PL   = (bIncludePL ? (bPositiveMoment ? vPLmax[i]   : vPLmin[i]) : 0.0);
      VehicleIndexType truck_index = vehicleIdx;
      if ( vehicleIdx == INVALID_INDEX )
         truck_index = (bPositiveMoment ? vMaxTruckIndex[i] : vMinTruckIndex[i]);


      Float64 gLL = pRatingSpec->GetLiveLoadFactor(ls,true);
      if ( gLL < 0 )
      {
         if ( ::IsStrengthLimitState(ls) )
         {
            // need to compute gLL based on axle weights
            Float64 fMinTop, fMinBot, fMaxTop, fMaxBot, fDummyTop, fDummyBot;
            AxleConfiguration MinAxleConfigTop, MinAxleConfigBot, MaxAxleConfigTop, MaxAxleConfigBot, DummyAxleConfigTop, DummyAxleConfigBot;
            if ( analysisType == pgsTypes::Envelope )
            {
               pProductForces->GetVehicularLiveLoadStress(llType,truck_index,pgsTypes::BridgeSite3,poi,MinSimpleContinuousEnvelope,true,true,&fMinTop,&fDummyTop,&fMinBot,&fDummyBot,&MinAxleConfigTop,&DummyAxleConfigTop,&MinAxleConfigBot,&DummyAxleConfigBot);
               pProductForces->GetVehicularLiveLoadStress(llType,truck_index,pgsTypes::BridgeSite3,poi,MaxSimpleContinuousEnvelope,true,true,&fDummyTop,&fMaxTop,&fDummyBot,&fMaxBot,&DummyAxleConfigTop,&MaxAxleConfigTop,&DummyAxleConfigBot,&MaxAxleConfigBot);
            }
            else
            {
               pProductForces->GetVehicularLiveLoadStress(llType,truck_index,pgsTypes::BridgeSite3,poi,analysisType == pgsTypes::Simple ? SimpleSpan : ContinuousSpan,true,true,&fMinTop,&fMaxTop,&fMaxTop,&fMaxBot,&MinAxleConfigTop,&MaxAxleConfigTop,&MinAxleConfigBot,&MaxAxleConfigBot);
            }

            gLL = GetStrengthLiveLoadFactor(ratingType,MaxAxleConfigBot);
         }
         else
         {
            gLL = GetServiceLiveLoadFactor(ratingType);
         }
      }


      Float64 Mcr = vMcr[i].Mcr;

      Float64 Icr = vCrackedSection[i].Icr;
      Float64 c   = vCrackedSection[i].c;

      Float64 dps = vM[i].dt;

      Float64 fpe; // stress in reinforcement before cracking
      if ( bPositiveMoment )
      {
         // positive moment - use fpe (effective prestress)
         fpe = pPrestressForce->GetStrandStress(poi,pgsTypes::Permanent,pgsTypes::AfterLosses,ls);
      }
      else
      {
         // negative moment - compute stress in deck rebar for uncracked section
         Float64 I = pSectProp->GetIx(pgsTypes::BridgeSite3,poi);
         Float64 y = pSectProp->GetYt(pgsTypes::BridgeSite3,poi);
         
         y -= top_slab_cover; 

         fpe = -(Es/Eg)*Mcr*y/I; // - sign is to make the stress come out positive (tension) because Mcr is < 0
      }

      std::_tstring strVehicleName = strLLNames[truck_index];
      Float64 W = pProductLoads->GetVehicleWeight(llType,truck_index);

      pgsYieldStressRatioArtifact stressRatioArtifact;
      stressRatioArtifact.SetRatingType(ratingType);
      stressRatioArtifact.SetPointOfInterest(poi);
      stressRatioArtifact.SetVehicleIndex(truck_index);
      stressRatioArtifact.SetVehicleWeight(W);
      stressRatioArtifact.SetVehicleName(strVehicleName.c_str());
      stressRatioArtifact.SetAllowableStress(fr);
      stressRatioArtifact.SetDeadLoadFactor(gDC);
      stressRatioArtifact.SetDeadLoadMoment(DC);
      stressRatioArtifact.SetWearingSurfaceFactor(gDW);
      stressRatioArtifact.SetWearingSurfaceMoment(DW);
      stressRatioArtifact.SetLiveLoadDistributionFactor(gM);
      stressRatioArtifact.SetLiveLoadFactor(gLL);
      stressRatioArtifact.SetLiveLoadMoment(LLIM+PL);
      stressRatioArtifact.SetCrackingMoment(Mcr);
      stressRatioArtifact.SetIcr(Icr);
      stressRatioArtifact.SetCrackDepth(c);
      stressRatioArtifact.SetReinforcementDepth(dps);
      stressRatioArtifact.SetEffectivePrestress(fpe);
      stressRatioArtifact.SetEs(Es);
      stressRatioArtifact.SetEg(Eg);

      ratingArtifact.AddArtifact(poi,stressRatioArtifact,bPositiveMoment);
   }
}

pgsTypes::LimitState pgsLoadRater::GetStrengthLimitStateType(pgsTypes::LoadRatingType ratingType)
{
   pgsTypes::LimitState ls;
   switch(ratingType)
   {
   case pgsTypes::lrDesign_Inventory:
      ls = pgsTypes::StrengthI_Inventory;
      break;

   case pgsTypes::lrDesign_Operating:
      ls = pgsTypes::StrengthI_Operating;
      break;

   case pgsTypes::lrLegal_Routine:
      ls = pgsTypes::StrengthI_LegalRoutine;
      break;

   case pgsTypes::lrLegal_Special:
      ls = pgsTypes::StrengthI_LegalSpecial;
      break;

   case pgsTypes::lrPermit_Routine:
      ls = pgsTypes::StrengthII_PermitRoutine;
      break;

   case pgsTypes::lrPermit_Special:
      ls = pgsTypes::StrengthII_PermitSpecial;
      break;

   default:
      ATLASSERT(false); // SHOULD NEVER GET HERE (unless there is a new rating type)
   }

   return ls;
}

pgsTypes::LimitState pgsLoadRater::GetServiceLimitStateType(pgsTypes::LoadRatingType ratingType)
{
   pgsTypes::LimitState ls;
   switch(ratingType)
   {
   case pgsTypes::lrDesign_Inventory:
      ls = pgsTypes::ServiceIII_Inventory;
      break;

   case pgsTypes::lrDesign_Operating:
      ls = pgsTypes::ServiceIII_Operating;
      break;

   case pgsTypes::lrLegal_Routine:
      ls = pgsTypes::ServiceIII_LegalRoutine;
      break;

   case pgsTypes::lrLegal_Special:
      ls = pgsTypes::ServiceIII_LegalSpecial;
      break;

   case pgsTypes::lrPermit_Routine:
      ls = pgsTypes::ServiceI_PermitRoutine;
      break;

   case pgsTypes::lrPermit_Special:
      ls = pgsTypes::ServiceI_PermitSpecial;
      break;

   default:
      ATLASSERT(false); // SHOULD NEVER GET HERE (unless there is a new rating type)
   }

   return ls;
}

void pgsLoadRater::GetMoments(GirderIndexType gdrLineIdx,bool bPositiveMoment,pgsTypes::LoadRatingType ratingType,VehicleIndexType vehicleIdx, const std::vector<pgsPointOfInterest>& vPOI, std::vector<Float64>& vDCmin, std::vector<Float64>& vDCmax,std::vector<Float64>& vDWmin, std::vector<Float64>& vDWmax, std::vector<Float64>& vLLIMmin, std::vector<VehicleIndexType>& vMinTruckIndex,std::vector<Float64>& vLLIMmax,std::vector<VehicleIndexType>& vMaxTruckIndex,std::vector<Float64>& vPLmin,std::vector<Float64>& vPLmax)
{
   GET_IFACE(ILibrary,pLib);
   GET_IFACE(ISpecification,pSpec);
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( pSpec->GetSpecification().c_str() );
   bool bIncludeNoncompositeMoments = pSpecEntry->IncludeNoncompositeMomentsForNegMomentDesign();

   pgsTypes::AnalysisType analysisType = pSpec->GetAnalysisType();


   pgsTypes::LiveLoadType llType = GetLiveLoadType(ratingType);

   std::vector<Float64> vUnused;
   std::vector<VehicleIndexType> vUnusedIndex;

   GET_IFACE(IBridge,pBridge);
   bool bFutureOverlay = pBridge->HasOverlay() && pBridge->IsFutureOverlay();

   GET_IFACE(ICombinedForces2,pCombinedForces);
   GET_IFACE(IProductForces2,pProductForces);
   if ( bPositiveMoment || (!bPositiveMoment && bIncludeNoncompositeMoments) )
   {
      if ( analysisType == pgsTypes::Envelope )
      {
         vDCmin = pCombinedForces->GetMoment(lcDC,pgsTypes::BridgeSite2,vPOI,ctCummulative,MinSimpleContinuousEnvelope);
         vDCmax = pCombinedForces->GetMoment(lcDC,pgsTypes::BridgeSite2,vPOI,ctCummulative,MaxSimpleContinuousEnvelope);
         vDWmin = pCombinedForces->GetMoment(lcDWRating,pgsTypes::BridgeSite2,vPOI,ctCummulative,MinSimpleContinuousEnvelope);
         vDWmax = pCombinedForces->GetMoment(lcDWRating,pgsTypes::BridgeSite2,vPOI,ctCummulative,MaxSimpleContinuousEnvelope);

         if ( vehicleIdx == INVALID_INDEX )
         {
            pProductForces->GetLiveLoadMoment(llType,pgsTypes::BridgeSite3,vPOI,MinSimpleContinuousEnvelope,true,true,&vLLIMmin,&vUnused,&vMinTruckIndex,&vUnusedIndex);
            pProductForces->GetLiveLoadMoment(llType,pgsTypes::BridgeSite3,vPOI,MaxSimpleContinuousEnvelope,true,true,&vUnused,&vLLIMmax,&vUnusedIndex,&vMaxTruckIndex);
         }
         else
         {
            pProductForces->GetVehicularLiveLoadMoment(llType,vehicleIdx,pgsTypes::BridgeSite3,vPOI,MinSimpleContinuousEnvelope,true,true,&vLLIMmin,&vUnused,NULL,NULL);
            pProductForces->GetVehicularLiveLoadMoment(llType,vehicleIdx,pgsTypes::BridgeSite3,vPOI,MaxSimpleContinuousEnvelope,true,true,&vUnused,&vLLIMmax,NULL,NULL);
         }

         pCombinedForces->GetCombinedLiveLoadMoment( pgsTypes::lltPedestrian, pgsTypes::BridgeSite3, vPOI, MaxSimpleContinuousEnvelope, &vUnused, &vPLmax );
         pCombinedForces->GetCombinedLiveLoadMoment( pgsTypes::lltPedestrian, pgsTypes::BridgeSite3, vPOI, MinSimpleContinuousEnvelope, &vPLmin, &vUnused );
      }
      else
      {
         vDCmax = pCombinedForces->GetMoment(lcDC,pgsTypes::BridgeSite2,vPOI,ctCummulative,analysisType == pgsTypes::Simple ? SimpleSpan : ContinuousSpan);
         vDCmin = vDCmax;

         vDWmax = pCombinedForces->GetMoment(lcDWRating,pgsTypes::BridgeSite2,vPOI,ctCummulative,analysisType == pgsTypes::Simple ? SimpleSpan : ContinuousSpan);
         vDWmin = vDWmax;

         if ( vehicleIdx == INVALID_INDEX )
            pProductForces->GetLiveLoadMoment(llType,pgsTypes::BridgeSite3,vPOI,analysisType == pgsTypes::Simple ? SimpleSpan : ContinuousSpan,true,true,&vLLIMmin,&vLLIMmax,&vMinTruckIndex,&vMaxTruckIndex);
         else
            pProductForces->GetVehicularLiveLoadMoment(llType,vehicleIdx,pgsTypes::BridgeSite3,vPOI,analysisType == pgsTypes::Simple ? SimpleSpan : ContinuousSpan,true,true,&vLLIMmin,&vLLIMmax,NULL,NULL);

         pCombinedForces->GetCombinedLiveLoadMoment( pgsTypes::lltPedestrian, pgsTypes::BridgeSite3, vPOI, analysisType == pgsTypes::Simple ? SimpleSpan : ContinuousSpan, &vPLmin, &vPLmax );
      }
   }
   else
   {
      // Because of the construction sequence, some loads don't contribute to the negative moment..
      // Those loads applied to the simple span girders before continuity don't contribute to the
      // negative moment resisted by the deck.
      //
      // Special load processing is required

      std::vector<Float64> vConstructionMin, vConstructionMax;
      std::vector<Float64> vSlabMin, vSlabMax;
      std::vector<Float64> vSlabPanelMin, vSlabPanelMax;
      std::vector<Float64> vDiaphragmMin, vDiaphragmMax;
      std::vector<Float64> vShearKeyMin, vShearKeyMax;
      std::vector<Float64> vUserDC1Min, vUserDC1Max;
      std::vector<Float64> vUserDW1Min, vUserDW1Max;
      std::vector<Float64> vUserDC2Min, vUserDC2Max;
      std::vector<Float64> vUserDW2Min, vUserDW2Max;
      std::vector<Float64> vTrafficBarrierMin, vTrafficBarrierMax;
      std::vector<Float64> vSidewalkMin, vSidewalkMax;
      std::vector<Float64> vOverlayMin, vOverlayMax;

      GET_IFACE(IProductLoads,pProductLoads);
      pgsTypes::Stage girderLoadStage = pProductLoads->GetGirderDeadLoadStage(gdrLineIdx);

      // Get all the product load responces
      GET_IFACE(IProductForces2,pProductForces);
      if ( analysisType == pgsTypes::Envelope )
      {
         // Get all the product loads
         vConstructionMin = pProductForces->GetMoment(pgsTypes::BridgeSite1,pftConstruction,vPOI,MinSimpleContinuousEnvelope);
         vConstructionMax = pProductForces->GetMoment(pgsTypes::BridgeSite1,pftConstruction,vPOI,MaxSimpleContinuousEnvelope);

         vSlabMin = pProductForces->GetMoment(pgsTypes::BridgeSite1,pftSlab,vPOI,MinSimpleContinuousEnvelope);
         vSlabMax = pProductForces->GetMoment(pgsTypes::BridgeSite1,pftSlab,vPOI,MaxSimpleContinuousEnvelope);

         vSlabPanelMin = pProductForces->GetMoment(pgsTypes::BridgeSite1,pftSlabPanel,vPOI,MinSimpleContinuousEnvelope);
         vSlabPanelMax = pProductForces->GetMoment(pgsTypes::BridgeSite1,pftSlabPanel,vPOI,MaxSimpleContinuousEnvelope);

         vDiaphragmMin = pProductForces->GetMoment(pgsTypes::BridgeSite1,pftDiaphragm,vPOI,MinSimpleContinuousEnvelope);
         vDiaphragmMax = pProductForces->GetMoment(pgsTypes::BridgeSite1,pftDiaphragm,vPOI,MaxSimpleContinuousEnvelope);

         vShearKeyMin = pProductForces->GetMoment(pgsTypes::BridgeSite1,pftShearKey,vPOI,MinSimpleContinuousEnvelope);
         vShearKeyMax = pProductForces->GetMoment(pgsTypes::BridgeSite1,pftShearKey,vPOI,MaxSimpleContinuousEnvelope);

         vUserDC1Min = pProductForces->GetMoment(pgsTypes::BridgeSite1,pftUserDC,vPOI,MinSimpleContinuousEnvelope);
         vUserDC1Max = pProductForces->GetMoment(pgsTypes::BridgeSite1,pftUserDC,vPOI,MaxSimpleContinuousEnvelope);

         vUserDW1Min = pProductForces->GetMoment(pgsTypes::BridgeSite1,pftUserDW,vPOI,MinSimpleContinuousEnvelope);
         vUserDW1Max = pProductForces->GetMoment(pgsTypes::BridgeSite1,pftUserDW,vPOI,MaxSimpleContinuousEnvelope);

         vUserDC2Min = pProductForces->GetMoment(pgsTypes::BridgeSite2,pftUserDC,vPOI,MinSimpleContinuousEnvelope);
         vUserDC2Max = pProductForces->GetMoment(pgsTypes::BridgeSite2,pftUserDC,vPOI,MaxSimpleContinuousEnvelope);

         vUserDW2Min = pProductForces->GetMoment(pgsTypes::BridgeSite2,pftUserDW,vPOI,MinSimpleContinuousEnvelope);
         vUserDW2Max = pProductForces->GetMoment(pgsTypes::BridgeSite2,pftUserDW,vPOI,MaxSimpleContinuousEnvelope);

         vTrafficBarrierMin = pProductForces->GetMoment(pgsTypes::BridgeSite2,pftTrafficBarrier,vPOI,MinSimpleContinuousEnvelope);
         vTrafficBarrierMax = pProductForces->GetMoment(pgsTypes::BridgeSite2,pftTrafficBarrier,vPOI,MaxSimpleContinuousEnvelope);

         vSidewalkMin = pProductForces->GetMoment(pgsTypes::BridgeSite2,pftSidewalk,vPOI,MinSimpleContinuousEnvelope);
         vSidewalkMax = pProductForces->GetMoment(pgsTypes::BridgeSite2,pftSidewalk,vPOI,MaxSimpleContinuousEnvelope);

         if ( bFutureOverlay )
         {
            vOverlayMin.resize(vPOI.size(),0);
            vOverlayMax.resize(vPOI.size(),0);
         }
         else
         {
            vOverlayMin = pProductForces->GetMoment(pgsTypes::BridgeSite2,pftOverlay,vPOI,MinSimpleContinuousEnvelope);
            vOverlayMax = pProductForces->GetMoment(pgsTypes::BridgeSite2,pftOverlay,vPOI,MaxSimpleContinuousEnvelope);
         }

         if ( vehicleIdx == INVALID_INDEX )
         {
            pProductForces->GetLiveLoadMoment( llType, pgsTypes::BridgeSite3, vPOI, MinSimpleContinuousEnvelope, true, true, &vLLIMmin, &vUnused, &vMinTruckIndex, &vUnusedIndex );
            pProductForces->GetLiveLoadMoment( llType, pgsTypes::BridgeSite3, vPOI, MaxSimpleContinuousEnvelope, true, true, &vUnused, &vLLIMmax, &vUnusedIndex, &vMaxTruckIndex );
         }
         else
         {
            pProductForces->GetVehicularLiveLoadMoment(llType,vehicleIdx,pgsTypes::BridgeSite3,vPOI,MinSimpleContinuousEnvelope,true,true,&vLLIMmin,&vUnused,NULL,NULL);
            pProductForces->GetVehicularLiveLoadMoment(llType,vehicleIdx,pgsTypes::BridgeSite3,vPOI,MaxSimpleContinuousEnvelope,true,true,&vUnused,&vLLIMmax,NULL,NULL);
         }

         pCombinedForces->GetCombinedLiveLoadMoment( pgsTypes::lltPedestrian, pgsTypes::BridgeSite3, vPOI, MaxSimpleContinuousEnvelope, &vUnused, &vPLmax );
         pCombinedForces->GetCombinedLiveLoadMoment( pgsTypes::lltPedestrian, pgsTypes::BridgeSite3, vPOI, MinSimpleContinuousEnvelope, &vPLmin, &vUnused );
      }
      else
      {
         vConstructionMin = pProductForces->GetMoment(pgsTypes::BridgeSite1,pftConstruction,vPOI,analysisType == pgsTypes::Simple ? SimpleSpan : ContinuousSpan);
         vConstructionMax = vConstructionMin;

         vSlabMin = pProductForces->GetMoment(pgsTypes::BridgeSite1,pftSlab,vPOI,analysisType == pgsTypes::Simple ? SimpleSpan : ContinuousSpan);
         vSlabMax = vSlabMin;

         vSlabPanelMin = pProductForces->GetMoment(pgsTypes::BridgeSite1,pftSlabPanel,vPOI,analysisType == pgsTypes::Simple ? SimpleSpan : ContinuousSpan);
         vSlabPanelMax = vSlabPanelMin;

         vDiaphragmMin = pProductForces->GetMoment(pgsTypes::BridgeSite1,pftDiaphragm,vPOI,analysisType == pgsTypes::Simple ? SimpleSpan : ContinuousSpan);
         vDiaphragmMax = vDiaphragmMin;

         vShearKeyMin = pProductForces->GetMoment(pgsTypes::BridgeSite1,pftShearKey,vPOI,analysisType == pgsTypes::Simple ? SimpleSpan : ContinuousSpan);
         vShearKeyMax = vShearKeyMin;

         vUserDC1Min = pProductForces->GetMoment(pgsTypes::BridgeSite1,pftUserDC,vPOI,analysisType == pgsTypes::Simple ? SimpleSpan : ContinuousSpan);
         vUserDC1Max = vUserDC1Min;

         vUserDW1Min = pProductForces->GetMoment(pgsTypes::BridgeSite1,pftUserDW,vPOI,analysisType == pgsTypes::Simple ? SimpleSpan : ContinuousSpan);
         vUserDW1Max = vUserDW1Min;

         vUserDC2Min = pProductForces->GetMoment(pgsTypes::BridgeSite2,pftUserDC,vPOI,analysisType == pgsTypes::Simple ? SimpleSpan : ContinuousSpan);
         vUserDC2Max = vUserDC2Min;

         vUserDW2Min = pProductForces->GetMoment(pgsTypes::BridgeSite2,pftUserDW,vPOI,analysisType == pgsTypes::Simple ? SimpleSpan : ContinuousSpan);
         vUserDW2Max = vUserDW2Min;

         vTrafficBarrierMin = pProductForces->GetMoment(pgsTypes::BridgeSite2,pftTrafficBarrier,vPOI,analysisType == pgsTypes::Simple ? SimpleSpan : ContinuousSpan);
         vTrafficBarrierMax = vTrafficBarrierMin;

         vSidewalkMin = pProductForces->GetMoment(pgsTypes::BridgeSite2,pftSidewalk,vPOI,analysisType == pgsTypes::Simple ? SimpleSpan : ContinuousSpan);
         vSidewalkMax = vSidewalkMin;

         if ( bFutureOverlay )
         {
            vOverlayMin.resize(vPOI.size(),0);
         }
         else
         {
            vOverlayMin = pProductForces->GetMoment(pgsTypes::BridgeSite2,pftOverlay,vPOI,analysisType == pgsTypes::Simple ? SimpleSpan : ContinuousSpan);
         }
         vOverlayMax = vOverlayMin;

         if ( vehicleIdx == INVALID_INDEX )
            pProductForces->GetLiveLoadMoment(llType,pgsTypes::BridgeSite3,vPOI,analysisType == pgsTypes::Simple ? SimpleSpan : ContinuousSpan,true,true,&vLLIMmin,&vLLIMmax,&vMinTruckIndex,&vMaxTruckIndex);
         else
            pProductForces->GetVehicularLiveLoadMoment(llType,vehicleIdx,pgsTypes::BridgeSite3,vPOI,analysisType == pgsTypes::Simple ? SimpleSpan : ContinuousSpan,true,true,&vLLIMmin,&vLLIMmax,NULL,NULL);

         pCombinedForces->GetCombinedLiveLoadMoment( pgsTypes::lltPedestrian, pgsTypes::BridgeSite3, vPOI, analysisType == pgsTypes::Simple ? SimpleSpan : ContinuousSpan, &vPLmin, &vPLmax );
      }

      // sum DC and DW

      // initialize
      vDCmin.resize(vPOI.size(),0);
      vDCmax.resize(vPOI.size(),0);
      vDWmin.resize(vPOI.size(),0);
      vDWmax.resize(vPOI.size(),0);

      special_transform(pBridge,vPOI.begin(),vPOI.end(),vConstructionMin.begin(),vDCmin.begin(),vDCmin.begin());
      special_transform(pBridge,vPOI.begin(),vPOI.end(),vConstructionMax.begin(),vDCmax.begin(),vDCmax.begin());
      special_transform(pBridge,vPOI.begin(),vPOI.end(),vSlabMin.begin(),vDCmin.begin(),vDCmin.begin());
      special_transform(pBridge,vPOI.begin(),vPOI.end(),vSlabMax.begin(),vDCmax.begin(),vDCmax.begin());
      special_transform(pBridge,vPOI.begin(),vPOI.end(),vSlabPanelMin.begin(),vDCmin.begin(),vDCmin.begin());
      special_transform(pBridge,vPOI.begin(),vPOI.end(),vSlabPanelMax.begin(),vDCmax.begin(),vDCmax.begin());
      special_transform(pBridge,vPOI.begin(),vPOI.end(),vDiaphragmMin.begin(),vDCmin.begin(),vDCmin.begin());
      special_transform(pBridge,vPOI.begin(),vPOI.end(),vDiaphragmMax.begin(),vDCmax.begin(),vDCmax.begin());
      special_transform(pBridge,vPOI.begin(),vPOI.end(),vShearKeyMin.begin(),vDCmin.begin(),vDCmin.begin());
      special_transform(pBridge,vPOI.begin(),vPOI.end(),vShearKeyMax.begin(),vDCmax.begin(),vDCmax.begin());

      std::transform(vUserDC1Min.begin(),vUserDC1Min.end(),vDCmin.begin(),vDCmin.begin(),sum_values);
      std::transform(vUserDC1Max.begin(),vUserDC1Max.end(),vDCmax.begin(),vDCmax.begin(),sum_values);

      std::transform(vUserDW1Min.begin(),vUserDW1Min.end(),vDWmin.begin(),vDWmin.begin(),sum_values);
      std::transform(vUserDW1Max.begin(),vUserDW1Max.end(),vDWmax.begin(),vDWmax.begin(),sum_values);

      std::transform(vUserDC2Min.begin(),vUserDC2Min.end(),vDCmin.begin(),vDCmin.begin(),sum_values);
      std::transform(vUserDC2Max.begin(),vUserDC2Max.end(),vDCmax.begin(),vDCmax.begin(),sum_values);

      std::transform(vUserDW2Min.begin(),vUserDW2Min.end(),vDWmin.begin(),vDWmin.begin(),sum_values);
      std::transform(vUserDW2Max.begin(),vUserDW2Max.end(),vDWmax.begin(),vDWmax.begin(),sum_values);

      std::transform(vTrafficBarrierMin.begin(),vTrafficBarrierMin.end(),vDCmin.begin(),vDCmin.begin(),sum_values);
      std::transform(vTrafficBarrierMax.begin(),vTrafficBarrierMax.end(),vDCmax.begin(),vDCmax.begin(),sum_values);

      std::transform(vSidewalkMin.begin(),vSidewalkMin.end(),vDCmin.begin(),vDCmin.begin(),sum_values);
      std::transform(vSidewalkMax.begin(),vSidewalkMax.end(),vDCmax.begin(),vDCmax.begin(),sum_values);

      std::transform(vOverlayMin.begin(),vOverlayMin.end(),vDWmin.begin(),vDWmin.begin(),sum_values);
      std::transform(vOverlayMax.begin(),vOverlayMax.end(),vDWmax.begin(),vDWmax.begin(),sum_values);
   }
}

void special_transform(IBridge* pBridge,
                       std::vector<pgsPointOfInterest>::const_iterator poiBeginIter,
                       std::vector<pgsPointOfInterest>::const_iterator poiEndIter,
                       std::vector<Float64>::iterator forceBeginIter,
                       std::vector<Float64>::iterator resultBeginIter,
                       std::vector<Float64>::iterator outputBeginIter)
{
   std::vector<pgsPointOfInterest>::const_iterator poiIter = poiBeginIter;
   std::vector<Float64>::iterator forceIter = forceBeginIter;
   std::vector<Float64>::iterator resultIter = resultBeginIter;
   std::vector<Float64>::iterator outputIter = outputBeginIter;

   for ( ; poiIter != poiEndIter; poiIter++, forceIter++, resultIter++, outputIter++ )
   {
      const pgsPointOfInterest& poi = *poiIter;

      pgsTypes::Stage start,end,dummy;
      PierIndexType prevPierIdx = poi.GetSpan();
      PierIndexType nextPierIdx = prevPierIdx + 1;

      pBridge->GetContinuityStage(prevPierIdx,&dummy,&start);
      pBridge->GetContinuityStage(nextPierIdx,&end,&dummy);

      bool bIncludeSlab = false;
      if ( start == pgsTypes::BridgeSite1 || end == pgsTypes::BridgeSite1 )
      {
         *outputIter = (*forceIter + *resultIter);
      }
   }
}

Float64 pgsLoadRater::GetStrengthLiveLoadFactor(pgsTypes::LoadRatingType ratingType,AxleConfiguration& axleConfig)
{
   Float64 sum_axle_weight = 0; // sum of axle weights on the bridge
   Float64 firstAxleLocation = -1;
   Float64 lastAxleLocation = 0;
   AxleConfiguration::iterator iter(axleConfig.begin());
   AxleConfiguration::iterator end(axleConfig.end());
   for ( ; iter != end; iter++ )
   {
      AxlePlacement& axle_placement = *iter;
      sum_axle_weight += axle_placement.Weight;

      if ( !IsZero(axle_placement.Weight) )
      {
         if ( firstAxleLocation < 0 )
            firstAxleLocation = axle_placement.Location;

         lastAxleLocation = axle_placement.Location;
      }
   }
   
   Float64 AL = fabs(firstAxleLocation - lastAxleLocation); // front axle to rear axle length (for axles on the bridge)

   Float64 gLL = 0;
   GET_IFACE(IRatingSpecification,pRatingSpec);
   GET_IFACE(ILibrary,pLibrary);
   const RatingLibraryEntry* pRatingEntry = pLibrary->GetRatingEntry( pRatingSpec->GetRatingSpecification().c_str() );
   if ( pRatingEntry->GetSpecificationVersion() < lrfrVersionMgr::SecondEditionWith2013Interims )
   {
      CLiveLoadFactorModel model;
      if ( ratingType == pgsTypes::lrPermit_Routine )
         model = pRatingEntry->GetLiveLoadFactorModel(pgsTypes::lrPermit_Routine);
      else if ( ratingType == pgsTypes::lrPermit_Special )
         model = pRatingEntry->GetLiveLoadFactorModel(pRatingSpec->GetSpecialPermitType());
      else
         model = pRatingEntry->GetLiveLoadFactorModel(ratingType);

      gLL = model.GetStrengthLiveLoadFactor(pRatingSpec->GetADTT(),sum_axle_weight);
   }
   else
   {
      Float64 GVW = sum_axle_weight;
      Float64 PermitWeightRatio = IsZero(AL) ? 0 : GVW/AL;
      CLiveLoadFactorModel2 model;
      if ( ratingType == pgsTypes::lrPermit_Routine )
         model = pRatingEntry->GetLiveLoadFactorModel2(pgsTypes::lrPermit_Routine);
      else if ( ratingType == pgsTypes::lrPermit_Special )
         model = pRatingEntry->GetLiveLoadFactorModel2(pRatingSpec->GetSpecialPermitType());
      else
         model = pRatingEntry->GetLiveLoadFactorModel2(ratingType);

      gLL = model.GetStrengthLiveLoadFactor(pRatingSpec->GetADTT(),PermitWeightRatio);
   }

   return gLL;
}

Float64 pgsLoadRater::GetServiceLiveLoadFactor(pgsTypes::LoadRatingType ratingType)
{
   Float64 gLL = 0;
   GET_IFACE(IRatingSpecification,pRatingSpec);
   GET_IFACE(ILibrary,pLibrary);
   const RatingLibraryEntry* pRatingEntry = pLibrary->GetRatingEntry( pRatingSpec->GetRatingSpecification().c_str() );
   if ( pRatingEntry->GetSpecificationVersion() < lrfrVersionMgr::SecondEditionWith2013Interims )
   {
      CLiveLoadFactorModel model;
      if ( ratingType == pgsTypes::lrPermit_Routine )
         model = pRatingEntry->GetLiveLoadFactorModel(pgsTypes::lrPermit_Routine);
      else if ( ratingType == pgsTypes::lrPermit_Special )
         model = pRatingEntry->GetLiveLoadFactorModel(pRatingSpec->GetSpecialPermitType());
      else
         model = pRatingEntry->GetLiveLoadFactorModel(ratingType);

      gLL = model.GetServiceLiveLoadFactor(pRatingSpec->GetADTT());
   }
   else
   {
      CLiveLoadFactorModel2 model;
      if ( ratingType == pgsTypes::lrPermit_Routine )
         model = pRatingEntry->GetLiveLoadFactorModel2(pgsTypes::lrPermit_Routine);
      else if ( ratingType == pgsTypes::lrPermit_Special )
         model = pRatingEntry->GetLiveLoadFactorModel2(pRatingSpec->GetSpecialPermitType());
      else
         model = pRatingEntry->GetLiveLoadFactorModel2(ratingType);

      gLL = model.GetServiceLiveLoadFactor(pRatingSpec->GetADTT());
   }

   return gLL;
}
