///////////////////////////////////////////////////////////////////////
// PGSuper - Prestressed Girder SUPERstructure Design and Analysis
// Copyright � 1999-2023  Washington State Department of Transportation
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
#include "BearingDesignEngineer.h"
#include <Units\Convert.h>
#include <PGSuperException.h>
#include <IFace\Bridge.h>
#include <IFace\Project.h>
#include <IFace\AnalysisResults.h>
#include <IFace\ShearCapacity.h>
#include <IFace\PrestressForce.h> 
#include <IFace\MomentCapacity.h>
#include <IFace\StatusCenter.h>
#include <IFace\ResistanceFactors.h>
#include <IFace\EditByUI.h>
#include <IFace\Intervals.h>
#include <IFace/Limits.h>
#include <LRFD\Rebar.h>

#include <PsgLib\SpecLibraryEntry.h>
#include <psgLib/ShearCapacityCriteria.h>
#include <psgLib/LimitStateConcreteStrengthCriteria.h>

#include <PgsExt\statusitem.h>
#include <PgsExt\DesignConfigUtil.h>
#include <PgsExt\GirderLabel.h>

#if defined _DEBUG
#include <IFace\DocumentType.h>
#endif

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


/****************************************************************************
CLASS
   pgsBearingDesignEngineer
****************************************************************************/

////////////////////////// PUBLIC     ///////////////////////////////////////

//======================== LIFECYCLE  =======================================
pgsBearingDesignEngineer::pgsBearingDesignEngineer(IBroker* pBroker)
{
   m_pBroker = pBroker;
}

//======================== OPERATIONS =======================================
void pgsBearingDesignEngineer::SetBroker(IBroker* pBroker)
{
   m_pBroker = pBroker;
}


void pgsBearingDesignEngineer::GetBearingDesignProperties(DESIGNPROPERTIES* pDetails) const
{
}


Float64 pgsBearingDesignEngineer::BearingSkewFactor(const ReactionLocation& reactionLocation, bool isFlexural) const
{
    GET_IFACE(IBridge, pBridge);

    //skew factor
    CComPtr<IAngle> pSkew;
    pBridge->GetPierSkew(reactionLocation.PierIdx, &pSkew);
    Float64 skew;
    pSkew->get_Value(&skew);

    Float64 skewFactor;

    if (isFlexural)
    {
        skewFactor = 1.0;
    }
    else
    {
        skewFactor = tan(skew);
    }

    return skewFactor;
}


Float64 pgsBearingDesignEngineer::GetBearingCyclicRotation(pgsTypes::AnalysisType analysisType, const pgsPointOfInterest& poi,
    const ReactionLocation& reactionLocation, bool bIncludeImpact, bool bIncludeLLDF) const
{

    GET_IFACE(IProductForces, pProductForces);
    pgsTypes::BridgeAnalysisType maxBAT = pProductForces->GetBridgeAnalysisType(analysisType, pgsTypes::Maximize);

    GET_IFACE(IIntervals, pIntervals);
    IntervalIndexType overlayIntervalIdx = pIntervals->GetOverlayInterval();
    IntervalIndexType lastIntervalIdx = pIntervals->GetIntervalCount() - 1;
    Float64 min, max;
    VehicleIndexType minConfig, maxConfig;

    pProductForces->GetLiveLoadRotation(lastIntervalIdx, pgsTypes::lltDesign, poi, maxBAT, bIncludeImpact, bIncludeLLDF, &min, &max, &minConfig, &maxConfig);
    Float64 cyclic_rotation = max + pProductForces->GetRotation(lastIntervalIdx, pgsTypes::pftUserLLIM, poi, maxBAT, rtCumulative, false);
    return cyclic_rotation;

}


void pgsBearingDesignEngineer::GetBearingRotationDetails(pgsTypes::AnalysisType analysisType, const pgsPointOfInterest& poi,
    const ReactionLocation& reactionLocation, bool bIncludeImpact, bool bIncludeLLDF, bool isFlexural, ROTATIONDETAILS* pDetails) const
{

    Float64 pMin, pMax;
    Float64 min, max;
    VehicleIndexType minConfig, maxConfig;

    GET_IFACE(IProductForces, pProductForces);
    pgsTypes::BridgeAnalysisType maxBAT = pProductForces->GetBridgeAnalysisType(analysisType, pgsTypes::Maximize);
    pgsTypes::BridgeAnalysisType minBAT = pProductForces->GetBridgeAnalysisType(analysisType, pgsTypes::Minimize);

    GET_IFACE(IIntervals, pIntervals);
    IntervalIndexType overlayIntervalIdx = pIntervals->GetOverlayInterval();
    IntervalIndexType lastIntervalIdx = pIntervals->GetIntervalCount() - 1;

    GET_IFACE(ILimitStateForces, limitForces);
    limitForces->GetRotation(lastIntervalIdx, pgsTypes::ServiceI, poi, maxBAT, 
        true, true, true, true, true, &pMin, &pMax);

    pProductForces->GetLiveLoadRotation(lastIntervalIdx, pgsTypes::lltDesign, poi, maxBAT, bIncludeImpact, bIncludeLLDF, &min, &max, &minConfig, &maxConfig);

    const CSegmentKey& segmentKey(poi.GetSegmentKey());
    IntervalIndexType releaseIntervalIdx = pIntervals->GetPrestressReleaseInterval(segmentKey);
    IntervalIndexType erectSegmentIntervalIdx = pIntervals->GetErectSegmentInterval(poi.GetSegmentKey());

    auto skewFactor = this->BearingSkewFactor(reactionLocation, isFlexural);


    pDetails->cyclicRotation = skewFactor * this->GetBearingCyclicRotation(analysisType, poi, reactionLocation, bIncludeImpact, bIncludeLLDF);
    pDetails->maxUserDCRotation = skewFactor * pProductForces->GetRotation(lastIntervalIdx, pgsTypes::pftUserDC, poi, maxBAT, rtCumulative, false);
    pDetails->minUserDCRotation = skewFactor * pProductForces->GetRotation(lastIntervalIdx, pgsTypes::pftUserDC, poi, minBAT, rtCumulative, false);
    pDetails->maxUserDWRotation = skewFactor * pProductForces->GetRotation(lastIntervalIdx, pgsTypes::pftUserDW, poi, maxBAT, rtCumulative, false);
    pDetails->minUserDWRotation = skewFactor * pProductForces->GetRotation(lastIntervalIdx, pgsTypes::pftUserDW, poi, minBAT, rtCumulative, false);
    pDetails->maxUserLLrotation = skewFactor * pProductForces->GetRotation(lastIntervalIdx, pgsTypes::pftUserLLIM, poi, maxBAT, rtCumulative, false);
    pDetails->minUserLLrotation = skewFactor * pProductForces->GetRotation(lastIntervalIdx, pgsTypes::pftUserLLIM, poi, minBAT, rtCumulative, false);
    pDetails->preTensionRotation = skewFactor * pProductForces->GetRotation(releaseIntervalIdx, pgsTypes::pftPretension, poi, maxBAT, rtCumulative, false);
   


    GET_IFACE_NOCHECK(ICamber, pCamber);
    Float64 Dcreep1, Rcreep1;
    pCamber->GetCreepDeflection(poi, ICamber::cpReleaseToDeck, pgsTypes::CreepTime::Max, pgsTypes::pddErected, nullptr, &Dcreep1, &Rcreep1);

    GET_IFACE(ILossParameters, pLossParams);
    bool bTimeStep = (pLossParams->GetLossMethod() == PrestressLossCriteria::LossMethodType::TIME_STEP ? true : false);

    if (bTimeStep)
    {
        pDetails->creepRotation = skewFactor * pProductForces->GetRotation(lastIntervalIdx, pgsTypes::pftCreep, poi, maxBAT, rtCumulative, false);
        pDetails->shrinkageRotation = skewFactor * pProductForces->GetRotation(lastIntervalIdx, pgsTypes::pftShrinkage, poi, maxBAT, rtCumulative, false);
        pDetails->relaxationRotation = skewFactor * pProductForces->GetRotation(lastIntervalIdx, pgsTypes::pftRelaxation, poi, maxBAT, rtCumulative, false);
        pDetails->postTensionRotation = skewFactor * pProductForces->GetRotation(releaseIntervalIdx, pgsTypes::pftPostTensioning, poi, maxBAT, rtCumulative, false);

    }
    else
    {
        pDetails->creepRotation = skewFactor * Rcreep1;
        pDetails->shrinkageRotation = 0.0;
        pDetails->relaxationRotation = 0.0;
        pDetails->postTensionRotation = 0.0;
    }

    pDetails->totalRotation = skewFactor * (pMax + pDetails->preTensionRotation + pDetails->creepRotation +
        pDetails->shrinkageRotation + pDetails->relaxationRotation + pDetails->postTensionRotation);

    pDetails->staticRotation = pDetails->totalRotation - pDetails->cyclicRotation;


    pDetails->maxDesignLLrotation = skewFactor * max;
    pDetails->minDesignLLrotation = skewFactor * min;
    pDetails->maxConfig = maxConfig;
    pDetails->minConfig = minConfig;

    pProductForces->GetLiveLoadRotation(lastIntervalIdx, pgsTypes::lltPedestrian, poi, minBAT, bIncludeImpact, true, &min, &max);

    pDetails->maxPedRotation = max;

    pProductForces->GetLiveLoadRotation(lastIntervalIdx, pgsTypes::lltPedestrian, poi, maxBAT, bIncludeImpact, true, &min, &max);

    pDetails->minPedRotation = min;

    pDetails->erectedSegmentRotation = skewFactor * pProductForces->GetRotation(erectSegmentIntervalIdx, pgsTypes::pftGirder, poi, maxBAT, rtCumulative, false);

    pDetails->maxShearKeyRotation = 0.0;

    pDetails->maxGirderRotation = skewFactor * pProductForces->GetRotation(erectSegmentIntervalIdx, pgsTypes::pftGirder, poi, maxBAT, rtCumulative, false);
    pDetails->maxGirderRotation = skewFactor * pProductForces->GetRotation(erectSegmentIntervalIdx, pgsTypes::pftGirder, poi, minBAT, rtCumulative, false);
    pDetails->diaphragmRotation = skewFactor * pProductForces->GetRotation(lastIntervalIdx, pgsTypes::pftDiaphragm, poi, maxBAT, rtCumulative, false);
    pDetails->maxSlabRotation = skewFactor * pProductForces->GetRotation(lastIntervalIdx, pgsTypes::pftSlab, poi, maxBAT, rtCumulative, false);
    pDetails->minSlabRotation = skewFactor * pProductForces->GetRotation(lastIntervalIdx, pgsTypes::pftSlab, poi, minBAT, rtCumulative, false);
    pDetails->maxHaunchRotation = skewFactor * pProductForces->GetRotation(lastIntervalIdx, pgsTypes::pftSlab, poi, maxBAT, rtCumulative, false);
    pDetails->minHaunchRotation = skewFactor * pProductForces->GetRotation(lastIntervalIdx, pgsTypes::pftSlab, poi, minBAT, rtCumulative, false);
    pDetails->maxRailingSystemRotation = skewFactor * pProductForces->GetRotation(lastIntervalIdx, pgsTypes::pftTrafficBarrier, poi, maxBAT, rtCumulative, false);
    pDetails->minRailingSystemRotation = skewFactor * pProductForces->GetRotation(lastIntervalIdx, pgsTypes::pftTrafficBarrier, poi, minBAT, rtCumulative, false);
    pDetails->maxFutureOverlayRotation = skewFactor * pProductForces->GetRotation(lastIntervalIdx, pgsTypes::pftOverlay, poi, maxBAT, rtCumulative, false);
    pDetails->minFutureOverlayRotation = skewFactor * pProductForces->GetRotation(lastIntervalIdx, pgsTypes::pftOverlay, poi, minBAT, rtCumulative, false);
    pDetails->maxLongitudinalJointRotation = skewFactor * pProductForces->GetRotation(lastIntervalIdx, pgsTypes::pftLongitudinalJoint, poi, maxBAT, rtCumulative, false);
    pDetails->maxLongitudinalJointRotation = skewFactor * pProductForces->GetRotation(lastIntervalIdx, pgsTypes::pftLongitudinalJoint, poi, minBAT, rtCumulative, false);
    pDetails->maxConstructionRotation = skewFactor * pProductForces->GetRotation(lastIntervalIdx, pgsTypes::pftConstruction, poi, maxBAT, rtCumulative, false);
    pDetails->minConstructionRotation = skewFactor * pProductForces->GetRotation(lastIntervalIdx, pgsTypes::pftConstruction, poi, minBAT, rtCumulative, false);
    pDetails->maxSlabPanelRotation = skewFactor * pProductForces->GetRotation(lastIntervalIdx, pgsTypes::pftSlabPanel, poi, maxBAT, rtCumulative, false);
    pDetails->minSlabPanelRotation = skewFactor * pProductForces->GetRotation(lastIntervalIdx, pgsTypes::pftSlabPanel, poi, minBAT, rtCumulative, false);
    pDetails->maxSidewalkRotation = skewFactor * pProductForces->GetRotation(lastIntervalIdx, pgsTypes::pftSidewalk, poi, maxBAT, rtCumulative, false);
    pDetails->minSidewalkRotation = skewFactor * pProductForces->GetRotation(lastIntervalIdx, pgsTypes::pftSidewalk, poi, minBAT, rtCumulative, false);


}


void pgsBearingDesignEngineer::GetBearingReactionDetails(pgsTypes::AnalysisType analysisType, const pgsPointOfInterest& poi,
    const ReactionLocation& reactionLocation, bool bIncludeImpact, bool bIncludeLLDF, REACTIONDETAILS* pDetails) const
{
    pDetails->creepReaction = 0.0;
    pDetails->relaxationReaction = 0.0;
    pDetails->preTensionReaction = 0.0;
    pDetails->postTensionReaction = 0.0;
    pDetails->shrinkageReaction = 0.0;
    pDetails->diaphragmReaction = 0.0;

    pDetails->erectedSegmentReaction = 0.0;

    pDetails->maxConfig = 0.0;
    pDetails->maxConstructionReaction = 0.0;
    pDetails->maxDesignLLReaction = 0.0;
    pDetails->maxFutureOverlayReaction = 0.0;
    pDetails->maxGirderReaction = 0.0;
    pDetails->maxHaunchReaction = 0.0;
    pDetails->maxLongitudinalJointReaction = 0.0;
    pDetails->maxPedReaction = 0.0;
    pDetails->maxRailingSystemReaction = 0.0;
    pDetails->maxShearKeyReaction = 0.0;
    pDetails->maxSidewalkReaction = 0.0;
    pDetails->minSidewalkReaction = 0.0;
    pDetails->maxSlabPanelReaction = 0.0;
    pDetails->maxSlabReaction = 0.0;
    pDetails->maxUserDCReaction = 0.0;
    pDetails->maxUserDWReaction = 0.0;
    pDetails->maxUserLLReaction = 0.0;

    pDetails->minConfig = 0.0;
    pDetails->minConstructionReaction = 0.0;
    pDetails->minDesignLLReaction = 0.0;
    pDetails->minFutureOverlayReaction = 0.0;
    pDetails->minGirderReaction = 0.0;
    pDetails->minHaunchReaction = 0.0;
    pDetails->minLongitudinalJointReaction = 0.0;
    pDetails->minPedReaction = 0.0;
    pDetails->minRailingSystemReaction = 0.0;
    pDetails->minShearKeyReaction = 0.0;
    pDetails->minSidewalkReaction = 0.0;
    pDetails->minSlabPanelReaction = 0.0;
    pDetails->minSlabReaction = 0.0;
    pDetails->minUserDCReaction = 0.0;
    pDetails->minUserDWReaction = 0.0;
    pDetails->minUserLLReaction = 0.0;
}

void pgsBearingDesignEngineer::GetBearingShearDeformationDetails(pgsTypes::AnalysisType analysisType, const pgsPointOfInterest& poi,
    const ReactionLocation& reactionLocation, bool bIncludeImpact, bool bIncludeLLDF, SHEARDEFORMATIONDETAILS* pDetails) const
{
    pDetails->creep = 0.0;
    pDetails->postTension = 0.0;
    pDetails->preTension = 0.0;
    pDetails->relaxation = 0.0;
    pDetails->shrinkage = 0.0;
    pDetails->thermalBDMCold = 0.0;
    pDetails->thermalBDMWarm = 0.0;
    pDetails->thermalLRFDCold = 0.0;
    pDetails->thermalLRFDWarm = 0.0;
}

