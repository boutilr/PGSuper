///////////////////////////////////////////////////////////////////////
// PGSuper - Prestressed Girder SUPERstructure Design and Analysis
// Copyright � 1999-2013  Washington State Department of Transportation
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

// SpecAgentImp.cpp : Implementation of CSpecAgentImp
#include "stdafx.h"
#include "SpecAgent.h"
#include "SpecAgent_i.h"
#include "SpecAgentImp.h"

#include <PgsExt\BridgeDescription2.h>
#include <PsgLib\SpecLibraryEntry.h>
#include <Lrfd\PsStrand.h>
#include <Lrfd\Rebar.h>

#include <IFace\StatusCenter.h>
#include <IFace\PrestressForce.h>
#include <IFace\RatingSpecification.h>
#include <IFace\Intervals.h>

#include <Units\SysUnits.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

DECLARE_LOGFILE;

/////////////////////////////////////////////////////////////////////////////
// CSpecAgentImp


/////////////////////////////////////////////////////////////////////////////
// IAgent
//
STDMETHODIMP CSpecAgentImp::SetBroker(IBroker* pBroker)
{
   AGENT_SET_BROKER(pBroker);
   return S_OK;
}

STDMETHODIMP CSpecAgentImp::RegInterfaces()
{
   CComQIPtr<IBrokerInitEx2,&IID_IBrokerInitEx2> pBrokerInit(m_pBroker);

   pBrokerInit->RegInterface( IID_IAllowableStrandStress,       this );
   pBrokerInit->RegInterface( IID_IAllowableTendonStress,       this );
   pBrokerInit->RegInterface( IID_IAllowableConcreteStress,     this );
   pBrokerInit->RegInterface( IID_ITransverseReinforcementSpec, this );
   pBrokerInit->RegInterface( IID_IPrecastIGirderDetailsSpec,   this );
   pBrokerInit->RegInterface( IID_IGirderLiftingSpecCriteria,   this );
   pBrokerInit->RegInterface( IID_IGirderHaulingSpecCriteria,   this );
   pBrokerInit->RegInterface( IID_IKdotGirderHaulingSpecCriteria, this );
   pBrokerInit->RegInterface( IID_IDebondLimits,                this );
   pBrokerInit->RegInterface( IID_IResistanceFactors,           this );

   return S_OK;
}

STDMETHODIMP CSpecAgentImp::Init()
{
   CREATE_LOGFILE("SpecAgent");
   AGENT_INIT;
   return S_OK;
}

STDMETHODIMP CSpecAgentImp::Init2()
{
   return S_OK;
}

STDMETHODIMP CSpecAgentImp::GetClassID(CLSID* pCLSID)
{
   *pCLSID = CLSID_SpecAgent;
   return S_OK;
}

STDMETHODIMP CSpecAgentImp::Reset()
{
   return S_OK;
}

STDMETHODIMP CSpecAgentImp::ShutDown()
{
   CLOSE_LOGFILE;
   AGENT_CLEAR_INTERFACE_CACHE;
   return S_OK;
}

/////////////////////////////////////////////////////////////////////////////
// IAllowableStrandStress
//
bool CSpecAgentImp::CheckStressAtJacking()
{
   const SpecLibraryEntry* pSpec = GetSpec();
   return pSpec->CheckStrandStress(AT_JACKING);
}

bool CSpecAgentImp::CheckStressBeforeXfer()
{
   const SpecLibraryEntry* pSpec = GetSpec();
   return pSpec->CheckStrandStress(BEFORE_TRANSFER);
}

bool CSpecAgentImp::CheckStressAfterXfer()
{
   const SpecLibraryEntry* pSpec = GetSpec();
   return pSpec->CheckStrandStress(AFTER_TRANSFER);
}

bool CSpecAgentImp::CheckStressAfterLosses()
{
   const SpecLibraryEntry* pSpec = GetSpec();
   return pSpec->CheckStrandStress(AFTER_ALL_LOSSES);
}

Float64 CSpecAgentImp::GetAllowableAtJacking(const CSegmentKey& segmentKey,pgsTypes::StrandType strandType)
{
   if ( !CheckStressAtJacking() )
      return 0.0;

   GET_IFACE(IMaterials,pMaterial);
   const matPsStrand* pStrand = pMaterial->GetStrandMaterial(segmentKey,strandType);

   Float64 fpu = lrfdPsStrand::GetUltimateStrength(pStrand->GetGrade());

   const SpecLibraryEntry* pSpec = GetSpec();
   Float64 coeff = pSpec->GetStrandStressCoefficient(AT_JACKING,pStrand->GetType() == matPsStrand::LowRelaxation ? LOW_RELAX : STRESS_REL);

   return coeff*fpu;
}

Float64 CSpecAgentImp::GetAllowableBeforeXfer(const CSegmentKey& segmentKey,pgsTypes::StrandType strandType)
{
   if ( !CheckStressBeforeXfer() )
      return 0.0;

   GET_IFACE(IMaterials,pMaterial);
   const matPsStrand* pStrand = pMaterial->GetStrandMaterial(segmentKey,strandType);

   Float64 fpu = lrfdPsStrand::GetUltimateStrength(pStrand->GetGrade());

   const SpecLibraryEntry* pSpec = GetSpec();
   Float64 coeff = pSpec->GetStrandStressCoefficient(BEFORE_TRANSFER,pStrand->GetType() == matPsStrand::LowRelaxation ? LOW_RELAX : STRESS_REL);

   return coeff*fpu;
}

Float64 CSpecAgentImp::GetAllowableAfterXfer(const CSegmentKey& segmentKey,pgsTypes::StrandType strandType)
{
   if ( !CheckStressAfterXfer() )
      return 0.0;

   GET_IFACE(IMaterials,pMaterial);
   const matPsStrand* pStrand = pMaterial->GetStrandMaterial(segmentKey,strandType);

   Float64 fpu = lrfdPsStrand::GetUltimateStrength(pStrand->GetGrade());

   const SpecLibraryEntry* pSpec = GetSpec();
   Float64 coeff = pSpec->GetStrandStressCoefficient(AFTER_TRANSFER,pStrand->GetType() == matPsStrand::LowRelaxation ? LOW_RELAX : STRESS_REL);

   return coeff*fpu;
}

Float64 CSpecAgentImp::GetAllowableAfterLosses(const CSegmentKey& segmentKey,pgsTypes::StrandType strandType)
{
   if ( !CheckStressAfterLosses() )
      return 0.0;

   GET_IFACE(IMaterials,pMaterial);
   const matPsStrand* pStrand = pMaterial->GetStrandMaterial(segmentKey,strandType);

   Float64 fpy = lrfdPsStrand::GetYieldStrength(pStrand->GetGrade(),pStrand->GetType());

   const SpecLibraryEntry* pSpec = GetSpec();
   Float64 coeff = pSpec->GetStrandStressCoefficient(AFTER_ALL_LOSSES,pStrand->GetType() == matPsStrand::LowRelaxation ? LOW_RELAX : STRESS_REL);

   return coeff*fpy;
}

/////////////////////////////////////////////////////////
// IAllowableTendonStress
//
bool CSpecAgentImp::CheckTendonStressAtJacking()
{
   const SpecLibraryEntry* pSpec = GetSpec();
   return pSpec->CheckTendonStressAtJacking();
}

bool CSpecAgentImp::CheckTendonStressPriorToSeating()
{
   const SpecLibraryEntry* pSpec = GetSpec();
   return pSpec->CheckTendonStressPriorToSeating();
}

Float64 CSpecAgentImp::GetAllowableAtJacking(const CGirderKey& girderKey)
{
   if ( !CheckTendonStressAtJacking() )
      return 0.0;

   GET_IFACE(IMaterials,pMaterial);
   const matPsStrand* pStrand = pMaterial->GetTendonMaterial(girderKey);

   Float64 fpu = lrfdPsStrand::GetUltimateStrength(pStrand->GetGrade());

   const SpecLibraryEntry* pSpec = GetSpec();
   Float64 coeff = pSpec->GetTendonStressCoefficient(AT_JACKING,pStrand->GetType() == matPsStrand::LowRelaxation ? LOW_RELAX : STRESS_REL);

   return coeff*fpu;
}

Float64 CSpecAgentImp::GetAllowablePriorToSeating(const CGirderKey& girderKey)
{
   if ( !CheckTendonStressPriorToSeating() )
      return 0.0;

   GET_IFACE(IMaterials,pMaterial);
   const matPsStrand* pStrand = pMaterial->GetTendonMaterial(girderKey);

   Float64 fpy = lrfdPsStrand::GetYieldStrength(pStrand->GetGrade(),pStrand->GetType());

   const SpecLibraryEntry* pSpec = GetSpec();
   Float64 coeff = pSpec->GetTendonStressCoefficient(PRIOR_TO_SEATING,pStrand->GetType() == matPsStrand::LowRelaxation ? LOW_RELAX : STRESS_REL);

   return coeff*fpy;
}

Float64 CSpecAgentImp::GetAllowableAfterAnchorSetAtAnchorage(const CGirderKey& girderKey)
{
   GET_IFACE(IMaterials,pMaterial);
   const matPsStrand* pStrand = pMaterial->GetTendonMaterial(girderKey);

   Float64 fpu = lrfdPsStrand::GetUltimateStrength(pStrand->GetGrade());

   const SpecLibraryEntry* pSpec = GetSpec();
   Float64 coeff = pSpec->GetTendonStressCoefficient(ANCHORAGES_AFTER_SEATING,pStrand->GetType() == matPsStrand::LowRelaxation ? LOW_RELAX : STRESS_REL);

   return coeff*fpu;
}

Float64 CSpecAgentImp::GetAllowableAfterAnchorSet(const CGirderKey& girderKey)
{
   GET_IFACE(IMaterials,pMaterial);
   const matPsStrand* pStrand = pMaterial->GetTendonMaterial(girderKey);

   Float64 fpu = lrfdPsStrand::GetUltimateStrength(pStrand->GetGrade());

   const SpecLibraryEntry* pSpec = GetSpec();
   Float64 coeff = pSpec->GetTendonStressCoefficient(ELSEWHERE_AFTER_SEATING,pStrand->GetType() == matPsStrand::LowRelaxation ? LOW_RELAX : STRESS_REL);

   return coeff*fpu;
}

Float64 CSpecAgentImp::GetAllowableAfterLosses(const CGirderKey& girderKey)
{
   GET_IFACE(IMaterials,pMaterial);
   const matPsStrand* pStrand = pMaterial->GetTendonMaterial(girderKey);

   Float64 fpy = lrfdPsStrand::GetYieldStrength(pStrand->GetGrade(),pStrand->GetType());

   const SpecLibraryEntry* pSpec = GetSpec();
   Float64 coeff = pSpec->GetTendonStressCoefficient(AFTER_ALL_LOSSES,pStrand->GetType() == matPsStrand::LowRelaxation ? LOW_RELAX : STRESS_REL);

   return coeff*fpy;
}

/////////////////////////////////////////////////////////////////////////////
// IAllowableConcreteStress
//
Float64 CSpecAgentImp::GetAllowableStress(const pgsPointOfInterest& poi,IntervalIndexType intervalIdx,pgsTypes::LimitState ls,pgsTypes::StressType type,bool bWithBondededReinforcement)
{
   const CSegmentKey& segmentKey = poi.GetSegmentKey();

   GET_IFACE(IIntervals,pIntervals);
   IntervalIndexType liveLoadIntervalIdx = pIntervals->GetLiveLoadInterval();

   // if the interval under consideration is at or after live load is applied
   // and one of the rating limit states is being considered, then return
   // the allowable stress from the rating specifications
   if ( liveLoadIntervalIdx <= intervalIdx && ( ls == pgsTypes::ServiceIII_Inventory ||
                                                ls == pgsTypes::ServiceIII_Operating ||
                                                ls == pgsTypes::ServiceIII_LegalRoutine ||
                                                ls == pgsTypes::ServiceIII_LegalSpecial ) )
   {
      GET_IFACE(IRatingSpecification,pRatingSpec);
      if ( ls == pgsTypes::ServiceIII_Inventory )
         return pRatingSpec->GetAllowableTension(pgsTypes::lrDesign_Inventory,segmentKey);

      if ( ls == pgsTypes::ServiceIII_Operating )
         return pRatingSpec->GetAllowableTension(pgsTypes::lrDesign_Operating,segmentKey);

      if ( ls == pgsTypes::ServiceIII_LegalRoutine )
         return pRatingSpec->GetAllowableTension(pgsTypes::lrLegal_Routine,segmentKey);

      if ( ls == pgsTypes::ServiceIII_LegalSpecial )
         return pRatingSpec->GetAllowableTension(pgsTypes::lrLegal_Special,segmentKey);

      ATLASSERT(false); // should never get here
      return -1;
   }
   else
   {
      // This is a design/check case, so use the regular specifications
      GET_IFACE(IMaterials,pMaterials);
      Float64 fc;
      if (poi.HasAttribute(POI_CLOSURE))
         fc = pMaterials->GetClosurePourFc(segmentKey,intervalIdx);
      else
         fc = pMaterials->GetSegmentFc(segmentKey,intervalIdx);

      Float64 fAllow = GetAllowableStress(poi,intervalIdx,ls,type,fc,bWithBondededReinforcement);
      return fAllow;
   }
}

std::vector<Float64> CSpecAgentImp::GetAllowableStress(const std::vector<pgsPointOfInterest>& vPoi,IntervalIndexType intervalIdx,pgsTypes::LimitState ls,pgsTypes::StressType type,bool bWithBondededReinforcement)
{
   std::vector<Float64> vStress;
   std::vector<pgsPointOfInterest>::const_iterator iter(vPoi.begin());
   std::vector<pgsPointOfInterest>::const_iterator end(vPoi.end());
   for ( ; iter != end; iter++ )
   {
      const pgsPointOfInterest& poi = *iter;
      vStress.push_back( GetAllowableStress(poi,intervalIdx,ls,type,bWithBondededReinforcement));
   }

   return vStress;
}

Float64 CSpecAgentImp::GetAllowableStress(const pgsPointOfInterest& poi,IntervalIndexType intervalIdx,pgsTypes::LimitState ls,pgsTypes::StressType type,Float64 fc,bool bWithBondedReinforcement)
{
   Float64 fAllow;

   switch( type )
   {
   case pgsTypes::Tension:
      fAllow = GetAllowableTensileStress(poi,intervalIdx,ls,fc,bWithBondedReinforcement);
      break;

   case pgsTypes::Compression:
      fAllow = GetAllowableCompressiveStress(poi,intervalIdx,ls,fc);
      break;
   }

   return fAllow;
}

Float64 CSpecAgentImp::GetLiftingWithMildRebarAllowableStressFactor()
{
   const SpecLibraryEntry* pSpec = GetSpec();
   Float64 x = pSpec->GetMaxConcreteTensWithRebarLifting();
   return x;
}

Float64 CSpecAgentImp::GetLiftingWithMildRebarAllowableStress(const CSegmentKey& segmentKey)
{
#pragma Reminder("UPDATE: considate with other allowable stress method")
   GET_IFACE(IIntervals,pIntervals);
   IntervalIndexType liftSegmentIntervalIdx = pIntervals->GetLiftSegmentInterval(segmentKey);

   GET_IFACE(IMaterials,pMaterial);
   Float64 fci = pMaterial->GetSegmentFc(segmentKey,liftSegmentIntervalIdx);

   Float64 x = GetLiftingWithMildRebarAllowableStressFactor();

   return x*sqrt(fci);
}

Float64 CSpecAgentImp::GetHaulingWithMildRebarAllowableStressFactor()
{
   const SpecLibraryEntry* pSpec = GetSpec();
   Float64 x = pSpec->GetMaxConcreteTensWithRebarHauling();
   return x;
}

Float64 CSpecAgentImp::GetHaulingWithMildRebarAllowableStress(const CSegmentKey& segmentKey)
{
#pragma Reminder("UPDATE: considate with other allowable stress method")
   GET_IFACE(IIntervals,pIntervals);
   IntervalIndexType haulSegmentIntervalIdx = pIntervals->GetHaulSegmentInterval(segmentKey);

   GET_IFACE(IMaterials,pMaterial);
   Float64 fc = pMaterial->GetSegmentFc(segmentKey,haulSegmentIntervalIdx);

   Float64 x = GetHaulingWithMildRebarAllowableStressFactor();

   return x*sqrt(fc);
}

Float64 CSpecAgentImp::GetHaulingModulusOfRupture(const CSegmentKey& segmentKey)
{
   GET_IFACE(IIntervals,pIntervals);
   IntervalIndexType intervalIdx = pIntervals->GetHaulSegmentInterval(segmentKey);

   GET_IFACE(IMaterials,pMaterials);
   Float64 fc = pMaterials->GetSegmentFc(segmentKey,intervalIdx);

   GET_IFACE(IMaterials,pMaterial);
   pgsTypes::ConcreteType type = pMaterial->GetSegmentConcreteType(segmentKey);

   return GetHaulingModulusOfRupture(fc,type);
}

Float64 CSpecAgentImp::GetHaulingModulusOfRupture(Float64 fc,pgsTypes::ConcreteType concType)
{
   Float64 x = GetHaulingModulusOfRuptureCoefficient(concType);
   return x*sqrt(fc);
}

Float64 CSpecAgentImp::GetHaulingModulusOfRuptureCoefficient(pgsTypes::ConcreteType concType)
{
   const SpecLibraryEntry* pSpec = GetSpec();
   return pSpec->GetHaulingModulusOfRuptureCoefficient(concType);
}

Float64 CSpecAgentImp::GetAllowableCompressiveStressCoefficient(const pgsPointOfInterest& poi,IntervalIndexType intervalIdx,pgsTypes::LimitState ls)
{
   const SpecLibraryEntry* pSpec = GetSpec();
   Float64 x = -999999;

   const CSegmentKey& segmentKey = poi.GetSegmentKey();


   GET_IFACE(IIntervals,pIntervals);
   IntervalIndexType releaseIntervalIdx       = pIntervals->GetPrestressReleaseInterval(segmentKey);
   IntervalIndexType liftIntervalIdx          = pIntervals->GetLiftSegmentInterval(segmentKey);
   IntervalIndexType haulIntervalIdx          = pIntervals->GetHaulSegmentInterval(segmentKey);
   IntervalIndexType tempStrandRemovalIdx     = pIntervals->GetTemporaryStrandRemovalInterval(segmentKey);
   IntervalIndexType castDeckIntervalIdx      = pIntervals->GetCastDeckInterval();
   IntervalIndexType compositeDeckIntervalIdx = pIntervals->GetCompositeDeckInterval();
   IntervalIndexType liveLoadIntervalIdx      = pIntervals->GetLiveLoadInterval();

   if ( poi.HasAttribute(POI_CLOSURE) )
   {
      IntervalIndexType compositeClosurePourIntervalIdx = pIntervals->GetCompositeClosurePourInterval(segmentKey);
      if ( intervalIdx <= compositeClosurePourIntervalIdx )
         x = pSpec->GetCyCompStressService();
      else if ( liveLoadIntervalIdx <= intervalIdx )
         x = (ls == pgsTypes::ServiceIA || ls == pgsTypes::FatigueI ? pSpec->GetBs3CompStressService1A() : pSpec->GetBs3CompStressService() );
      else
         x = pSpec->GetBs2CompStress();
   }
   else
   {
      if ( intervalIdx == releaseIntervalIdx )
      {
         ATLASSERT( ls == pgsTypes::ServiceI );
         x = pSpec->GetCyCompStressService();
      }
      else if ( intervalIdx == liftIntervalIdx )
      {
         ATLASSERT( ls == pgsTypes::ServiceI );
         x = pSpec->GetCyCompStressLifting();
      }
      else if ( intervalIdx == haulIntervalIdx )
      {
         ATLASSERT( ls == pgsTypes::ServiceI );
         x = pSpec->GetHaulingCompStress();
      }
      else if ( intervalIdx == tempStrandRemovalIdx )
      {
         ATLASSERT( ls == pgsTypes::ServiceI );
         x = pSpec->GetTempStrandRemovalCompStress();
      }
      else if ( tempStrandRemovalIdx < intervalIdx && intervalIdx < compositeDeckIntervalIdx )
      {
         ATLASSERT( ls == pgsTypes::ServiceI );
         x = pSpec->GetBs1CompStress();
      }
      else if ( compositeDeckIntervalIdx <= intervalIdx && intervalIdx < liveLoadIntervalIdx )
      {
         ATLASSERT( ls == pgsTypes::ServiceI );
         x = pSpec->GetBs2CompStress();
      }
      else if ( liveLoadIntervalIdx <= intervalIdx )
      {
         ATLASSERT( (ls == pgsTypes::ServiceI) || (ls == pgsTypes::ServiceIA) || (ls == pgsTypes::FatigueI));
         x = (ls == pgsTypes::ServiceI ? pSpec->GetBs3CompStressService() : pSpec->GetBs3CompStressService1A());
      }
      else
      {
         ATLASSERT(false); // unexpected interval
      }
   }

   return x;
}

void CSpecAgentImp::GetAllowableTensionStressCoefficient(const pgsPointOfInterest& poi,IntervalIndexType intervalIdx,pgsTypes::LimitState ls,bool bWithBondedReinforcement,Float64* pCoeff,bool* pbMax,Float64* pMaxValue)
{
#pragma Reminder("UPDATE: need to deal with allowable stress at closure pour")
   // if there are different allowable stresses for segments and closures then we need to check the poi for the POI_CLOSURE attribute 
   // and get the correct coefficients

   const SpecLibraryEntry* pSpec = GetSpec();
   Float64 x = -999999;
   bool bCheckMax = false;
   Float64 fmax = -99999;

   const CSegmentKey& segmentKey = poi.GetSegmentKey();

   GET_IFACE(IEnvironment,pEnv);

   GET_IFACE(IIntervals,pIntervals);
   IntervalIndexType releaseIntervalIdx  = pIntervals->GetPrestressReleaseInterval(segmentKey);
   IntervalIndexType liftIntervalIdx     = pIntervals->GetLiftSegmentInterval(segmentKey);
   IntervalIndexType haulIntervalIdx     = pIntervals->GetHaulSegmentInterval(segmentKey);
   IntervalIndexType tempStrandRemovalIdx     = pIntervals->GetTemporaryStrandRemovalInterval(segmentKey);
   IntervalIndexType castDeckIntervalIdx = pIntervals->GetCastDeckInterval();
   IntervalIndexType liveLoadIntervalIdx = pIntervals->GetLiveLoadInterval();

   if ( intervalIdx == releaseIntervalIdx )
   {
      ATLASSERT( ls == pgsTypes::ServiceI );
      if ( bWithBondedReinforcement )
      {
         x = pSpec->GetCyMaxConcreteTensWithRebar();
      }
      else
      {
         x = pSpec->GetCyMaxConcreteTens();
         pSpec->GetCyAbsMaxConcreteTens(&bCheckMax,&fmax);
      }
   }
   else if ( intervalIdx == liftIntervalIdx )
   {
      ATLASSERT( ls == pgsTypes::ServiceI );
      x = pSpec->GetCyMaxConcreteTensLifting();
      pSpec->GetCyAbsMaxConcreteTensLifting(&bCheckMax,&fmax);
   }
   else if ( intervalIdx == haulIntervalIdx )
   {
      ATLASSERT( ls == pgsTypes::ServiceI );
      x = pSpec->GetMaxConcreteTensHauling();
      pSpec->GetAbsMaxConcreteTensHauling(&bCheckMax,&fmax);
   }
   else if ( intervalIdx == tempStrandRemovalIdx )
   {
      ATLASSERT( ls == pgsTypes::ServiceI );
      x = pSpec->GetTempStrandRemovalMaxConcreteTens();
      pSpec->GetTempStrandRemovalAbsMaxConcreteTens(&bCheckMax,&fmax);
   }
   else if ( tempStrandRemovalIdx < intervalIdx && intervalIdx < liveLoadIntervalIdx )
   {
      // any interval after temporary strands are removed and before the bridge is open to traffic
      ATLASSERT( ls == pgsTypes::ServiceI );
      x = pSpec->GetBs1MaxConcreteTens();
      pSpec->GetBs1AbsMaxConcreteTens(&bCheckMax,&fmax);
   }
   else if ( liveLoadIntervalIdx <= intervalIdx )
   {
      ATLASSERT( (ls == pgsTypes::ServiceIII)  );
      if ( pEnv->GetExposureCondition() == expNormal )
      {
         x = pSpec->GetBs3MaxConcreteTensNc();
         pSpec->GetBs3AbsMaxConcreteTensNc(&bCheckMax,&fmax);
      }
      else
      {
         x = pSpec->GetBs3MaxConcreteTensSc();
         pSpec->GetBs3AbsMaxConcreteTensSc(&bCheckMax,&fmax);
      }
   }
   else
   {
      ATLASSERT(false); // unexpected interval
   }

   *pCoeff    = x;
   *pbMax     = bCheckMax;
   *pMaxValue = fmax;
}

/////////////////////////////////////////////////////////////////////////////
// ITransverseReinforcementSpec
//
Float64 CSpecAgentImp::GetMaxSplittingStress(Float64 fyRebar)
{
   return lrfdRebar::GetMaxBurstingStress(fyRebar);
}

Float64 CSpecAgentImp::GetSplittingZoneLengthFactor()
{
   GET_IFACE(ILibrary,pLib);
   GET_IFACE(ISpecification,pSpec);

   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry(pSpec->GetSpecification().c_str());
   Float64 bzlf = pSpecEntry->GetSplittingZoneLengthFactor();
   return bzlf;
}

Float64 CSpecAgentImp::GetSplittingZoneLength( Float64 girderHeight )
{
   return girderHeight / GetSplittingZoneLengthFactor();
}

matRebar::Size CSpecAgentImp::GetMinConfinmentBarSize()
{
   return lrfdRebar::GetMinConfinmentBarSize();
}

Float64 CSpecAgentImp::GetMaxConfinmentBarSpacing()
{
   return lrfdRebar::GetMaxConfinmentBarSpacing();
}

Float64 CSpecAgentImp::GetMinConfinmentAvS()
{
   return lrfdRebar::GetMinConfinmentAvS();
}

void CSpecAgentImp::GetMaxStirrupSpacing(Float64* sUnderLimit, Float64* sOverLimit)
{
   lrfdRebar::GetMaxStirrupSpacing(sUnderLimit, sOverLimit);

   // check to see if this has been overridden by spec library entry.
   const SpecLibraryEntry* pSpec = GetSpec();
   Float64 max_spac = pSpec->GetMaxStirrupSpacing();

   *sUnderLimit = min(*sUnderLimit, max_spac);
   *sOverLimit = min(*sOverLimit, max_spac);
}

Float64 CSpecAgentImp::GetMinStirrupSpacing(Float64 maxAggregateSize, Float64 barDiameter)
{
   CHECK(maxAggregateSize>0.0);
   CHECK(barDiameter>0.0);

   Float64 min_spc = max(1.33*maxAggregateSize, barDiameter);

   const SpecLibraryEntry* pSpec = GetSpec();
   Float64 abs_min_spc;
   if (pSpec->GetSpecificationUnits()==lrfdVersionMgr::SI)
      abs_min_spc = ::ConvertToSysUnits(25., unitMeasure::Millimeter);
   else
      abs_min_spc = ::ConvertToSysUnits(1., unitMeasure::Inch);

   // lrfd requirements are for clear distance, we want cl-to-cl spacing
   min_spc += barDiameter;
   abs_min_spc += barDiameter;

   return max(min_spc, abs_min_spc);
}


Float64 CSpecAgentImp::GetMinTopFlangeThickness() const
{
   const SpecLibraryEntry* pSpec = GetSpec();

   Float64 dim;
   if (pSpec->GetSpecificationUnits()==lrfdVersionMgr::SI)
      dim = ::ConvertToSysUnits(50., unitMeasure::Millimeter);
   else
      dim = ::ConvertToSysUnits(2., unitMeasure::Inch);

   return dim;
}

Float64 CSpecAgentImp::GetMinWebThickness() const
{
   const SpecLibraryEntry* pSpec = GetSpec();

   Float64 dim;
   if (pSpec->GetSpecificationUnits()==lrfdVersionMgr::SI)
      dim = ::ConvertToSysUnits(125., unitMeasure::Millimeter);
   else
      dim = ::ConvertToSysUnits(5., unitMeasure::Inch);

   return dim;
}

Float64 CSpecAgentImp::GetMinBottomFlangeThickness() const
{
   const SpecLibraryEntry* pSpec = GetSpec();

   Float64 dim;
   if (pSpec->GetSpecificationUnits()==lrfdVersionMgr::SI)
      dim = ::ConvertToSysUnits(125., unitMeasure::Millimeter);
   else
      dim = ::ConvertToSysUnits(5., unitMeasure::Inch);

   return dim;
}

/////////////////////////////////////////////////////////////////////
//  IGirderLiftingSpecCriteria
bool CSpecAgentImp::IsLiftingAnalysisEnabled() const
{
   const SpecLibraryEntry* pSpec = GetSpec();
   return pSpec->IsLiftingAnalysisEnabled();
}

void  CSpecAgentImp::GetLiftingImpact(Float64* pDownward, Float64* pUpward) const
{
   const SpecLibraryEntry* pSpec = GetSpec();
   *pDownward = pSpec->GetCyLiftingDownwardImpact();
   *pUpward   = pSpec->GetCyLiftingUpwardImpact();
}

Float64 CSpecAgentImp::GetLiftingCrackingFs() const
{
   const SpecLibraryEntry* pSpec = GetSpec();
   return pSpec->GetCyLiftingCrackFs();
}

Float64 CSpecAgentImp::GetLiftingFailureFs() const
{
   const SpecLibraryEntry* pSpec = GetSpec();
   return pSpec->GetCyLiftingFailFs();
}

void CSpecAgentImp::GetLiftingAllowableTensileConcreteStressParameters(Float64* factor,bool* pbMax,Float64* fmax)
{
   const SpecLibraryEntry* pSpec = GetSpec();
   *factor = GetLiftingAllowableTensionFactor();
   pSpec->GetCyAbsMaxConcreteTensLifting(pbMax,fmax);
}

Float64 CSpecAgentImp::GetLiftingAllowableTensileConcreteStress(const CSegmentKey& segmentKey)
{
   Float64 factor = GetLiftingAllowableTensionFactor();

   GET_IFACE(IIntervals,pIntervals);
   IntervalIndexType liftSegmentIntervalIdx = pIntervals->GetLiftSegmentInterval(segmentKey);

   GET_IFACE(IMaterials,pMaterial);
   Float64 fci = pMaterial->GetSegmentFc(segmentKey,liftSegmentIntervalIdx);

   Float64 f = factor * sqrt( fci );

   bool is_max;
   Float64 maxval;
   const SpecLibraryEntry* pSpec = GetSpec();

   pSpec->GetCyAbsMaxConcreteTensLifting(&is_max,&maxval);
   if (is_max)
      f = min(f, maxval);

   return f;
}

Float64 CSpecAgentImp::GetLiftingAllowableTensionFactor()
{
   const SpecLibraryEntry* pSpec = GetSpec();

   Float64 factor = pSpec->GetCyMaxConcreteTensLifting();
   return factor;
}

Float64 CSpecAgentImp::GetLiftingAllowableCompressiveConcreteStress(const CSegmentKey& segmentKey)
{
   Float64 factor = GetLiftingAllowableCompressionFactor();

   GET_IFACE(IIntervals,pIntervals);
   IntervalIndexType liftSegmentIntervalIdx = pIntervals->GetLiftSegmentInterval(segmentKey);

   GET_IFACE(IMaterials,pMaterial);
   Float64 fci = pMaterial->GetSegmentFc(segmentKey,liftSegmentIntervalIdx);

   Float64 allowable = factor * fci;

   return allowable;
}

Float64 CSpecAgentImp::GetLiftingAllowableCompressionFactor()
{
   const SpecLibraryEntry* pSpec = GetSpec();

   Float64 factor = pSpec->GetCyCompStressLifting();
   return -factor;
}

Float64 CSpecAgentImp::GetLiftingAllowableTensileConcreteStressEx(Float64 fci, bool withMinRebar)
{
   if (withMinRebar)
   {
      Float64 x = GetLiftingWithMildRebarAllowableStressFactor();

      Float64 f = x * sqrt( fci );
      return f;
   }
   else
   {
      Float64 x; 
      bool bCheckMax;
      Float64 fmax;

      GetLiftingAllowableTensileConcreteStressParameters(&x,&bCheckMax,&fmax);

      Float64 f = x * sqrt( fci );

      if ( bCheckMax )
         f = min(f,fmax);

      return f;
   }
}

Float64 CSpecAgentImp::GetLiftingAllowableCompressiveConcreteStressEx(Float64 fci)
{
   Float64 x = GetLiftingAllowableCompressionFactor();

   return x*fci;
}

Float64 CSpecAgentImp::GetHeightOfPickPointAboveGirderTop() const
{
   const SpecLibraryEntry* pSpec = GetSpec();

   return pSpec->GetPickPointHeight();
}

Float64 CSpecAgentImp::GetLiftingLoopPlacementTolerance() const
{
   const SpecLibraryEntry* pSpec = GetSpec();

   return pSpec->GetLiftingLoopTolerance();
}

Float64 CSpecAgentImp::GetLiftingCableMinInclination() const
{
   const SpecLibraryEntry* pSpec = GetSpec();

   return pSpec->GetMinCableInclination();
}

Float64 CSpecAgentImp::GetLiftingSweepTolerance()const
{
   const SpecLibraryEntry* pSpec = GetSpec();

   return pSpec->GetMaxGirderSweepLifting();
}

Float64 CSpecAgentImp::GetLiftingModulusOfRupture(const CSegmentKey& segmentKey)
{
   GET_IFACE(IIntervals,pIntervals);
   IntervalIndexType intervalIdx = pIntervals->GetLiftSegmentInterval(segmentKey);

   GET_IFACE(IMaterials,pMaterials);
   Float64 fci = pMaterials->GetSegmentFc(segmentKey,intervalIdx);
   pgsTypes::ConcreteType type = pMaterials->GetSegmentConcreteType(segmentKey);

   return GetLiftingModulusOfRupture(fci,type);
}

Float64 CSpecAgentImp::GetLiftingModulusOfRupture(Float64 fci,pgsTypes::ConcreteType concType)
{
   Float64 x = GetLiftingModulusOfRuptureCoefficient(concType);
   return x*sqrt(fci);
}

Float64 CSpecAgentImp::GetLiftingModulusOfRuptureCoefficient(pgsTypes::ConcreteType concType)
{
   const SpecLibraryEntry* pSpec = GetSpec();
   return pSpec->GetLiftingModulusOfRuptureCoefficient(concType);
}

Float64 CSpecAgentImp::GetMinimumLiftingPointLocation(const CSegmentKey& segmentKey,pgsTypes::MemberEndType end) const
{
   const SpecLibraryEntry* pSpec = GetSpec();
   Float64 min_lift_point = pSpec->GetMininumLiftingPointLocation();

   // if less than zero, then use H from the end of the girder
   if ( min_lift_point < 0 )
   {
      GET_IFACE(IBridge,pBridge);
      pgsPointOfInterest poi(segmentKey,0.0);

      GET_IFACE(IIntervals,pIntervals);
      IntervalIndexType releaseIntervalIdx = pIntervals->GetPrestressReleaseInterval(segmentKey);

      if ( end == pgsTypes::metEnd )
      {
         poi.SetDistFromStart( pBridge->GetSegmentLength(segmentKey) );
      }
      GET_IFACE(ISectionProperties,pSectProp);
      min_lift_point = pSectProp->GetHg( releaseIntervalIdx, poi );
   }

   return min_lift_point;
}

Float64 CSpecAgentImp::GetLiftingPointLocationAccuracy() const
{
   const SpecLibraryEntry* pSpec = GetSpec();
   return pSpec->GetLiftingPointLocationAccuracy();
}

//////////////////////////////////////////////////////////////////////
// IGirderHaulingSpecCriteria
bool CSpecAgentImp::IsHaulingAnalysisEnabled() const
{
   const SpecLibraryEntry* pSpec = GetSpec();
   return pSpec->IsHaulingAnalysisEnabled();
}

pgsTypes::HaulingAnalysisMethod CSpecAgentImp::GetHaulingAnalysisMethod() const
{
   const SpecLibraryEntry* pSpec = GetSpec();
   return pSpec->GetHaulingAnalysisMethod();
}

void CSpecAgentImp::GetHaulingImpact(Float64* pDownward, Float64* pUpward) const
{
   const SpecLibraryEntry* pSpec = GetSpec();
   *pDownward = pSpec->GetHaulingDownwardImpact();
   *pUpward   = pSpec->GetHaulingUpwardImpact();
}

Float64 CSpecAgentImp::GetHaulingCrackingFs() const
{
   const SpecLibraryEntry* pSpec = GetSpec();
   return pSpec->GetHaulingCrackFs();
}

Float64 CSpecAgentImp::GetHaulingRolloverFs() const
{
   const SpecLibraryEntry* pSpec = GetSpec();
   return pSpec->GetHaulingFailFs();
}

void CSpecAgentImp::GetHaulingAllowableTensileConcreteStressParameters(Float64* factor,bool* pbMax,Float64* fmax)
{
   const SpecLibraryEntry* pSpec = GetSpec();
   *factor = GetHaulingAllowableTensionFactor();
   pSpec->GetAbsMaxConcreteTensHauling(pbMax,fmax);
}

Float64 CSpecAgentImp::GetHaulingAllowableTensileConcreteStress(const CSegmentKey& segmentKey)
{
   const SpecLibraryEntry* pSpec = GetSpec();
   Float64 factor = GetHaulingAllowableTensionFactor();

   GET_IFACE(IIntervals,pIntervals);
   IntervalIndexType haulSegmentIntervalIdx = pIntervals->GetHaulSegmentInterval(segmentKey);

   GET_IFACE(IMaterials,pMaterial);
   Float64 fc = pMaterial->GetSegmentFc(segmentKey,haulSegmentIntervalIdx);

   Float64 allow = factor * sqrt( fc );

   bool is_max;
   Float64 maxval;
   pSpec->GetAbsMaxConcreteTensHauling(&is_max,&maxval);
   if (is_max)
      allow = min(allow, maxval);

   return allow;
}

Float64 CSpecAgentImp::GetHaulingAllowableCompressiveConcreteStress(const CSegmentKey& segmentKey)
{
   Float64 factor = GetHaulingAllowableCompressionFactor();

   GET_IFACE(IIntervals,pIntervals);
   IntervalIndexType haulSegmentIntervalIdx = pIntervals->GetHaulSegmentInterval(segmentKey);

   GET_IFACE(IMaterials,pMaterial);
   Float64 fc = pMaterial->GetSegmentFc(segmentKey,haulSegmentIntervalIdx);

   Float64 allowable = factor * fc;
   return allowable;
}

Float64 CSpecAgentImp::GetHaulingAllowableTensionFactor()
{
   const SpecLibraryEntry* pSpec = GetSpec();
   Float64 factor = pSpec->GetMaxConcreteTensHauling();
   return factor;
}

Float64 CSpecAgentImp::GetHaulingAllowableCompressionFactor()
{
   const SpecLibraryEntry* pSpec = GetSpec();
   Float64 factor = pSpec->GetHaulingCompStress();
   return -factor;
}

Float64 CSpecAgentImp::GetHaulingAllowableTensileConcreteStressEx(Float64 fc, bool includeRebar)
{
   if (includeRebar)
   {
      Float64 x = GetHaulingWithMildRebarAllowableStressFactor();

      Float64 f = x * sqrt( fc );
      return f;
   }
   else
   {
      Float64 x; 
      bool bCheckMax;
      Float64 fmax;

      GetHaulingAllowableTensileConcreteStressParameters(&x,&bCheckMax,&fmax);

      Float64 f = x * sqrt( fc );

      if ( bCheckMax )
         f = min(f,fmax);

      return f;
   }
}

Float64 CSpecAgentImp::GetHaulingAllowableCompressiveConcreteStressEx(Float64 fc)
{
   Float64 x = GetHaulingAllowableCompressionFactor();

   return x*fc;
}

IGirderHaulingSpecCriteria::RollStiffnessMethod CSpecAgentImp::GetRollStiffnessMethod() const
{
   const SpecLibraryEntry* pSpec = GetSpec();
   return (pSpec->GetTruckRollStiffnessMethod() == 0 ? IGirderHaulingSpecCriteria::LumpSum : IGirderHaulingSpecCriteria::PerAxle );
}

Float64 CSpecAgentImp::GetLumpSumRollStiffness() const
{
   const SpecLibraryEntry* pSpec = GetSpec();
   return pSpec->GetTruckRollStiffness();
}

Float64 CSpecAgentImp::GetAxleWeightLimit() const
{
   const SpecLibraryEntry* pSpec = GetSpec();
   return pSpec->GetAxleWeightLimit();
}

Float64 CSpecAgentImp::GetAxleStiffness() const
{
   const SpecLibraryEntry* pSpec = GetSpec();
   return pSpec->GetAxleStiffness();
}

Float64 CSpecAgentImp::GetMinimumRollStiffness() const
{
   const SpecLibraryEntry* pSpec = GetSpec();
   return pSpec->GetMinRollStiffness();
}

Float64 CSpecAgentImp::GetHeightOfGirderBottomAboveRoadway() const
{
   const SpecLibraryEntry* pSpec = GetSpec();
   return pSpec->GetTruckGirderHeight();
}

Float64 CSpecAgentImp::GetHeightOfTruckRollCenterAboveRoadway() const
{
   const SpecLibraryEntry* pSpec = GetSpec();
   return pSpec->GetTruckRollCenterHeight();
}

Float64 CSpecAgentImp::GetAxleWidth() const
{
   const SpecLibraryEntry* pSpec = GetSpec();
   return pSpec->GetTruckAxleWidth();
}

Float64 CSpecAgentImp::GetMaxSuperelevation() const
{
   const SpecLibraryEntry* pSpec = GetSpec();
   return pSpec->GetRoadwaySuperelevation();
}

Float64 CSpecAgentImp::GetHaulingSweepTolerance()const
{
   const SpecLibraryEntry* pSpec = GetSpec();
   return pSpec->GetMaxGirderSweepHauling();
}

Float64 CSpecAgentImp::GetHaulingSupportPlacementTolerance() const
{
   const SpecLibraryEntry* pSpec = GetSpec();
   return pSpec->GetHaulingSupportPlacementTolerance();
}

Float64 CSpecAgentImp::GetIncreaseInCgForCamber() const
{
   const SpecLibraryEntry* pSpec = GetSpec();
   return pSpec->GetHaulingCamberPercentEstimate()/100.;
}

Float64 CSpecAgentImp::GetAllowableDistanceBetweenSupports() const
{
   const SpecLibraryEntry* pSpec = GetSpec();
   return pSpec->GetHaulingSupportDistance();
}

Float64 CSpecAgentImp::GetAllowableLeadingOverhang() const
{
   const SpecLibraryEntry* pSpec = GetSpec();
   return pSpec->GetMaxHaulingOverhang();
}

Float64 CSpecAgentImp::GetMaxGirderWgt() const
{
   const SpecLibraryEntry* pSpec = GetSpec();
   return pSpec->GetMaxGirderWeight();
}

Float64 CSpecAgentImp::GetMinimumHaulingSupportLocation(const CSegmentKey& segmentKey,pgsTypes::MemberEndType end) const
{
   const SpecLibraryEntry* pSpec = GetSpec();
   Float64 min_pick_point = pSpec->GetMininumTruckSupportLocation();

   // if less than zero, then use H from the end of the girder
   if ( min_pick_point < 0 )
   {
      GET_IFACE(IIntervals,pIntervals);
      IntervalIndexType releaseIntervalIdx = pIntervals->GetPrestressReleaseInterval(segmentKey);

      GET_IFACE(IBridge,pBridge);
      pgsPointOfInterest poi(segmentKey,0.0);
      if ( end == pgsTypes::metEnd )
      {
         poi.SetDistFromStart( pBridge->GetSegmentLength(segmentKey) );
      }
      GET_IFACE(ISectionProperties,pSectProp);
      min_pick_point = pSectProp->GetHg( releaseIntervalIdx, poi );
   }

   return min_pick_point;
}

Float64 CSpecAgentImp::GetHaulingSupportLocationAccuracy() const
{
   const SpecLibraryEntry* pSpec = GetSpec();
   return pSpec->GetTruckSupportLocationAccuracy();
}

/////////////////////////////////////////////////////////////////////
//  IKdogGirderLiftingSpecCriteria
// Spec criteria for KDOT analyses
Float64 CSpecAgentImp::GetKdotHaulingAllowableTensileConcreteStress(const CSegmentKey& segmentKey)
{
   return GetHaulingAllowableTensileConcreteStress(segmentKey);
}

Float64 CSpecAgentImp::GetKdotHaulingAllowableCompressiveConcreteStress(const CSegmentKey& segmentKey)
{
   return GetHaulingAllowableCompressiveConcreteStress(segmentKey);
}

Float64 CSpecAgentImp::GetKdotHaulingAllowableTensionFactor()
{
   return GetHaulingAllowableTensionFactor();
}

Float64 CSpecAgentImp::GetKdotHaulingAllowableCompressionFactor()
{
   return GetHaulingAllowableCompressionFactor();
}

Float64 CSpecAgentImp::GetKdotHaulingWithMildRebarAllowableStress(const CSegmentKey& segmentKey)

{
   return GetHaulingWithMildRebarAllowableStress(segmentKey);
}

Float64 CSpecAgentImp::GetKdotHaulingWithMildRebarAllowableStressFactor()
{
   return GetHaulingWithMildRebarAllowableStressFactor();
}

void CSpecAgentImp::GetKdotHaulingAllowableTensileConcreteStressParameters(Float64* factor,bool* pbMax,Float64* fmax)
{
   GetHaulingAllowableTensileConcreteStressParameters(factor, pbMax, fmax);
}

Float64 CSpecAgentImp::GetKdotHaulingAllowableTensileConcreteStressEx(Float64 fc, bool includeRebar)
{
   return GetHaulingAllowableTensileConcreteStressEx(fc, includeRebar);
}

Float64 CSpecAgentImp::GetKdotHaulingAllowableCompressiveConcreteStressEx(Float64 fc)
{
   return GetHaulingAllowableCompressiveConcreteStressEx(fc);
}

void CSpecAgentImp::GetMinimumHaulingSupportLocation(Float64* pHardDistance, bool* pUseFactoredLength, Float64* pLengthFactor) const
{
   const SpecLibraryEntry* pSpec = GetSpec();
   *pHardDistance = pSpec->GetMininumTruckSupportLocation();
   *pUseFactoredLength = pSpec->GetUseMinTruckSupportLocationFactor();
   *pLengthFactor = pSpec->GetMinTruckSupportLocationFactor();
}

Float64 CSpecAgentImp::GetHaulingDesignLocationAccuracy() const
{
   const SpecLibraryEntry* pSpec = GetSpec();
   return pSpec->GetTruckSupportLocationAccuracy();
}

void CSpecAgentImp::GetHaulingGFactors(Float64* pOverhangFactor, Float64* pInteriorFactor) const
{
   const SpecLibraryEntry* pSpec = GetSpec();

   *pOverhangFactor = pSpec->GetOverhangGFactor();
   *pInteriorFactor = pSpec->GetInteriorGFactor();
}

/////////////////////////////////////////////////////////////////////
// IDebondLimits
Float64 CSpecAgentImp::GetMaxDebondedStrands(const CSegmentKey& segmentKey)
{
   const GirderLibraryEntry* pGirderEntry = GetGirderEntry(segmentKey);

   return pGirderEntry->GetMaxTotalFractionDebondedStrands();
}

Float64 CSpecAgentImp::GetMaxDebondedStrandsPerRow(const CSegmentKey& segmentKey)
{
   const GirderLibraryEntry* pGirderEntry = GetGirderEntry(segmentKey);
   return pGirderEntry->GetMaxFractionDebondedStrandsPerRow();
}

Float64 CSpecAgentImp::GetMaxDebondedStrandsPerSection(const CSegmentKey& segmentKey)
{
   StrandIndexType nMax;
   Float64 fMax;

   const GirderLibraryEntry* pGirderEntry = GetGirderEntry(segmentKey);
   pGirderEntry->GetMaxDebondedStrandsPerSection(&nMax,&fMax);

   return fMax;
}

StrandIndexType CSpecAgentImp::GetMaxNumDebondedStrandsPerSection(const CSegmentKey& segmentKey)
{
   StrandIndexType nMax;
   Float64 fMax;

   const GirderLibraryEntry* pGirderEntry = GetGirderEntry(segmentKey);
   pGirderEntry->GetMaxDebondedStrandsPerSection(&nMax,&fMax);

   return nMax;
}

void CSpecAgentImp::GetMaxDebondLength(const CSegmentKey& segmentKey, Float64* pLen, pgsTypes::DebondLengthControl* pControl)
{
   const GirderLibraryEntry* pGirderEntry = GetGirderEntry(segmentKey);

   bool bSpanFraction, buseHard;
   Float64 spanFraction, hardDistance;
   pGirderEntry->GetMaxDebondedLength(&bSpanFraction, &spanFraction, &buseHard, &hardDistance);

   GET_IFACE(IBridge,pBridge);

   Float64 gdrlength = pBridge->GetSegmentLength(segmentKey);

   GET_IFACE(IPointOfInterest,pPOI);
   std::vector<pgsPointOfInterest> vPOI( pPOI->GetPointsOfInterest(segmentKey,POI_ERECTED_SEGMENT | POI_MIDSPAN) );
   ATLASSERT(vPOI.size() == 1);
   pgsPointOfInterest poi( vPOI[0] );

   // always use half girder length - development length
   GET_IFACE(IPretensionForce, pPrestressForce ); 
   Float64 dev_len = pPrestressForce->GetDevLength(poi,true); // set debonding to true to get max length

   Float64 min_len = gdrlength/2.0 - dev_len;
   *pControl = pgsTypes::mdbDefault;

   if (bSpanFraction)
   {
      Float64 sflen = gdrlength * spanFraction;
      if (sflen < min_len)
      {
         min_len = sflen;
         *pControl = pgsTypes::mbdFractional;
      }
   }

   if (buseHard)
   {
      if (hardDistance < min_len )
      {
         min_len = hardDistance;
         *pControl = pgsTypes::mdbHardLength;
      }
   }

   *pLen = min_len>0.0 ? min_len : 0.0; // don't return less than zero
}

Float64 CSpecAgentImp::GetMinDebondSectionDistance(const CSegmentKey& segmentKey)
{
   const GirderLibraryEntry* pGirderEntry = GetGirderEntry(segmentKey);
   return pGirderEntry->GetMinDebondSectionLength();
}

/////////////////////////////////////////////////////////////////////
// IResistanceFactors
void CSpecAgentImp::GetFlexureResistanceFactors(pgsTypes::ConcreteType type,Float64* phiTensionPS,Float64* phiTensionRC,Float64* phiCompression)
{
   const SpecLibraryEntry* pSpec = GetSpec();
   pSpec->GetFlexureResistanceFactors(type,phiTensionPS,phiTensionRC,phiCompression);
}

void CSpecAgentImp::GetFlexuralStrainLimits(matPsStrand::Grade grade,matPsStrand::Type type,Float64* pecl,Float64* petl)
{
   // The values for Grade 60 are the same as for all types of strand
   GetFlexuralStrainLimits(matRebar::Grade60,pecl,petl);
}

void CSpecAgentImp::GetFlexuralStrainLimits(matRebar::Grade rebarGrade,Float64* pecl,Float64* petl)
{
   switch (rebarGrade )
   {
   case matRebar::Grade40:
      *pecl = 0.0014;
      *petl = 0.005;
      break;

   case matRebar::Grade60:
      *pecl = 0.002;
      *petl = 0.005;
      break;

   case matRebar::Grade75:
      *pecl = 0.0028;
      *petl = 0.0050;
      break;

   case matRebar::Grade80:
      *pecl = 0.0030;
      *petl = 0.0056;
      break;

   case matRebar::Grade100:
      *pecl = 0.0040;
      *petl = 0.0080;
      break;

   default:
      ATLASSERT(false); // new rebar grade?
   }
}

Float64 CSpecAgentImp::GetShearResistanceFactor(pgsTypes::ConcreteType type)
{
   const SpecLibraryEntry* pSpec = GetSpec();
   return pSpec->GetShearResistanceFactor(type);
}

////////////////////
// Private methods
Float64 CSpecAgentImp::GetAllowableCompressiveStress(const pgsPointOfInterest& poi,IntervalIndexType intervalIdx,pgsTypes::LimitState ls,Float64 fc)
{
   Float64 x = GetAllowableCompressiveStressCoefficient(poi,intervalIdx,ls);
   // Add a minus sign because compression is negative
   return -x*fc;
}

Float64 CSpecAgentImp::GetAllowableTensileStress(const pgsPointOfInterest& poi,IntervalIndexType intervalIdx,pgsTypes::LimitState ls,Float64 fc, bool bWithBondedReinforcement)
{
   Float64 x;
   bool bCheckMax;
   Float64 fmax; // In system units
   GetAllowableTensionStressCoefficient(poi,intervalIdx,ls,bWithBondedReinforcement,&x,&bCheckMax,&fmax);

   Float64 f = x * sqrt( fc );
   if ( bCheckMax )
      f = min(f,fmax);

   return f;
}

const SpecLibraryEntry* CSpecAgentImp::GetSpec() const
{
   GET_IFACE( ISpecification, pSpec );
   GET_IFACE( ILibrary,       pLib );

   std::_tstring specName = pSpec->GetSpecification();
   return pLib->GetSpecEntry( specName.c_str() );
}

const GirderLibraryEntry* CSpecAgentImp::GetGirderEntry(const CSegmentKey& segmentKey) const
{
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CGirderGroupData* pGroup = pBridgeDesc->GetGirderGroup(segmentKey.groupIndex);
   const CSplicedGirderData* pGirder = pGroup->GetGirder(segmentKey.girderIndex);
   const GirderLibraryEntry* pGirderEntry = pGirder->GetGirderLibraryEntry();
   return pGirderEntry;
}
