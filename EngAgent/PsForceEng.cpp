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
#include "PsForceEng.h"
#include <IFace\Bridge.h>
#include <IFace\Project.h>
#include <IFace\PrestressForce.h>
#include <IFace\AnalysisResults.h>
#include <IFace\MomentCapacity.h>
#include <IFace\Intervals.h>
#include <IFace\BeamFactory.h>

#include "..\PGSuperException.h"

#include <PsgLib\SpecLibraryEntry.h>
#include <PsgLib\GirderLibraryEntry.h>

#include <PgsExt\BridgeDescription2.h>
#include <PgsExt\LoadFactors.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/****************************************************************************
CLASS
   pgsPsForceEng
****************************************************************************/


pgsPsForceEng::pgsPsForceEng()
{
   // Set to bogus value so we can update after our agents are set up
   m_PrestressTransferComputationType=(pgsTypes::PrestressTransferComputationType)-1; 
}

pgsPsForceEng::pgsPsForceEng(const pgsPsForceEng& rOther)
{
   MakeCopy(rOther);
}

pgsPsForceEng::~pgsPsForceEng()
{
}

pgsPsForceEng& pgsPsForceEng::operator= (const pgsPsForceEng& rOther)
{
   if( this != &rOther )
   {
      MakeAssignment(rOther);
   }

   return *this;
}

void pgsPsForceEng::SetBroker(IBroker* pBroker)
{
   m_pBroker = pBroker;
}

void pgsPsForceEng::SetStatusGroupID(StatusGroupIDType statusGroupID)
{
   m_StatusGroupID = statusGroupID;
}

void pgsPsForceEng::CreateLossEngineer(const CGirderKey& girderKey)
{
   if (m_LossEngineer )
   {
      return;
   }

   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CGirderGroupData* pGroup = pBridgeDesc->GetGirderGroup(girderKey.groupIndex);
   const CSplicedGirderData* pGirder = pGroup->GetGirder(girderKey.girderIndex);
   const GirderLibraryEntry* pGdr = pGirder->GetGirderLibraryEntry();

   CComPtr<IBeamFactory> beamFactory;
   pGdr->GetBeamFactory(&beamFactory);

   beamFactory->CreatePsLossEngineer(m_pBroker,m_StatusGroupID,girderKey,&m_LossEngineer);
}

void pgsPsForceEng::Invalidate()
{
   m_LossEngineer.Release();

   // Set transfer comp type to invalid
   m_PrestressTransferComputationType=(pgsTypes::PrestressTransferComputationType)-1; 
}     

void pgsPsForceEng::ClearDesignLosses()
{
   if (m_LossEngineer)
   {
      m_LossEngineer->ClearDesignLosses();
   }
}

void pgsPsForceEng::ReportLosses(const CGirderKey& girderKey,rptChapter* pChapter,IEAFDisplayUnits* pDisplayUnits)
{
   CreateLossEngineer(girderKey);
   m_LossEngineer->BuildReport(girderKey,pChapter,pDisplayUnits);
}

void pgsPsForceEng::ReportFinalLosses(const CGirderKey& girderKey,rptChapter* pChapter,IEAFDisplayUnits* pDisplayUnits)
{
   CreateLossEngineer(girderKey);
   m_LossEngineer->ReportFinalLosses(girderKey,pChapter,pDisplayUnits);
}

const ANCHORSETDETAILS* pgsPsForceEng::GetAnchorSetDetails(const CGirderKey& girderKey,DuctIndexType ductIdx)
{
   CreateLossEngineer(girderKey);
   return m_LossEngineer->GetAnchorSetDetails(girderKey,ductIdx);
}

Float64 pgsPsForceEng::GetElongation(const CGirderKey& girderKey,DuctIndexType ductIdx,pgsTypes::MemberEndType endType)
{
   CreateLossEngineer(girderKey);
   return m_LossEngineer->GetElongation(girderKey,ductIdx,endType);
}

Float64 pgsPsForceEng::GetAverageFrictionLoss(const CGirderKey& girderKey,DuctIndexType ductIdx)
{
   CreateLossEngineer(girderKey);
   Float64 dfpF, dfpA;
   m_LossEngineer->GetAverageFrictionAndAnchorSetLoss(girderKey,ductIdx,&dfpF,&dfpA);
   return dfpF;
}

Float64 pgsPsForceEng::GetAverageAnchorSetLoss(const CGirderKey& girderKey,DuctIndexType ductIdx)
{
   CreateLossEngineer(girderKey);
   Float64 dfpF, dfpA;
   m_LossEngineer->GetAverageFrictionAndAnchorSetLoss(girderKey,ductIdx,&dfpF,&dfpA);
   return dfpA;
}

const LOSSDETAILS* pgsPsForceEng::GetLosses(const pgsPointOfInterest& poi,IntervalIndexType intervalIdx)
{
   CreateLossEngineer(poi.GetSegmentKey());
   return m_LossEngineer->GetLosses(poi,intervalIdx);
}

const LOSSDETAILS* pgsPsForceEng::GetLosses(const pgsPointOfInterest& poi,const GDRCONFIG& config,IntervalIndexType intervalIdx)
{
   CreateLossEngineer(poi.GetSegmentKey());
   return m_LossEngineer->GetLosses(poi,config,intervalIdx);
}

Float64 pgsPsForceEng::GetPjackMax(const CSegmentKey& segmentKey,pgsTypes::StrandType strandType,StrandIndexType nStrands)
{
   GET_IFACE(ISegmentData,pSegmentData);
   const matPsStrand* pStrand = pSegmentData->GetStrandMaterial(segmentKey,strandType);
   ATLASSERT(pStrand != 0);

   return GetPjackMax(segmentKey,*pStrand,nStrands);
}

Float64 pgsPsForceEng::GetPjackMax(const CSegmentKey& segmentKey,const matPsStrand& strand,StrandIndexType nStrands)
{
   GET_IFACE( ISpecification, pSpec );
   std::_tstring spec_name = pSpec->GetSpecification();

   GET_IFACE( ILibrary, pLib );
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( spec_name.c_str() );

   Float64 Pjack = 0.0;
   if ( pSpecEntry->CheckStrandStress(CSS_AT_JACKING) )
   {
      Float64 coeff;
      if ( strand.GetType() == matPsStrand::LowRelaxation )
      {
         coeff = pSpecEntry->GetStrandStressCoefficient(CSS_AT_JACKING,LOW_RELAX);
      }
      else
      {
         coeff = pSpecEntry->GetStrandStressCoefficient(CSS_AT_JACKING,STRESS_REL);
      }

      Float64 fpu;
      Float64 aps;

      fpu = strand.GetUltimateStrength();
      aps = strand.GetNominalArea();

      Float64 Fu = fpu * aps * nStrands;

      Pjack = coeff*Fu;
   }
   else
   {
      Float64 coeff;
      if ( strand.GetType() == matPsStrand::LowRelaxation )
      {
         coeff = pSpecEntry->GetStrandStressCoefficient(CSS_BEFORE_TRANSFER,LOW_RELAX);
      }
      else
      {
         coeff = pSpecEntry->GetStrandStressCoefficient(CSS_BEFORE_TRANSFER,STRESS_REL);
      }

      // fake up some data so losses are computed before transfer
      GET_IFACE(IPointOfInterest,pPoi);
      std::vector<pgsPointOfInterest> vPoi( pPoi->GetPointsOfInterest(segmentKey, POI_5L | POI_RELEASED_SEGMENT) );
      pgsPointOfInterest poi( vPoi[0] );

      GET_IFACE(IIntervals,pIntervals);
      IntervalIndexType stressStrandsIntervalIdx = pIntervals->GetStressStrandInterval(segmentKey);
      Float64 loss = GetPrestressLoss(poi,pgsTypes::Permanent,stressStrandsIntervalIdx,pgsTypes::End);

      Float64 fpu = strand.GetUltimateStrength();
      Float64 aps = strand.GetNominalArea();

      Pjack = (coeff*fpu + loss) * aps * nStrands;
   }

   return Pjack;
}

Float64 pgsPsForceEng::GetXferLength(const CSegmentKey& segmentKey,pgsTypes::StrandType strandType)
{ 
   // Make sure our computation type is valid before we try to use it
   if ( (pgsTypes::PrestressTransferComputationType)-1 == m_PrestressTransferComputationType)
   {
      // Get value from libary if it hasn't been set up 
      GET_IFACE( ISpecification, pSpec );
      std::_tstring spec_name = pSpec->GetSpecification();
      GET_IFACE( ILibrary, pLib );
      const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( spec_name.c_str() );

      m_PrestressTransferComputationType = pSpecEntry->GetPrestressTransferComputationType();
   }

   if (m_PrestressTransferComputationType==pgsTypes::ptMinuteValue)
   {
      // Model zero prestress transfer length. 0.1 inches seems to give
      // good designs and spec checks. This does not happen if the value is reduced to 0.0;
      // because pgsuper is not set up to deal with moment point loads.
      //
      return ::ConvertToSysUnits(0.1,unitMeasure::Inch);
   }
   else
   {
      GET_IFACE(ISegmentData,pSegmentData);
      const matPsStrand* pStrand = pSegmentData->GetStrandMaterial(segmentKey,strandType);
      ATLASSERT(pStrand!=0);

      return lrfdPsStrand::GetXferLength( *pStrand );
   }
}

Float64 pgsPsForceEng::GetXferLengthAdjustment(const pgsPointOfInterest& poi,pgsTypes::StrandType strandType)
{
   ////////////////////////////////////////////////////////////////////////////////
   //******************************************************************************
   // NOTE: This method is almost identical to the other GetXferLengthAdjustment method.
   // Any changes done here will likely need to be done there as well
   //******************************************************************************
   ////////////////////////////////////////////////////////////////////////////////
   const CSegmentKey& segmentKey = poi.GetSegmentKey();

   GET_IFACE(IStrandGeometry,pStrandGeom);
   StrandIndexType Ns = pStrandGeom->GetStrandCount(segmentKey,strandType);

   // Quick check to make sure there is even an adjustment to be made
   // If there are no strands, just leave
   if ( Ns == 0 )
   {
      return 1.0;
   }

   // Compute a scaling factor to apply to the basic prestress force to adjust for transfer length
   // and debonded strands
   Float64 xfer_length = GetXferLength(segmentKey,strandType);

   GET_IFACE(IBridge,pBridge);
   Float64 gdr_length;
   Float64 left_end_size;
   Float64 right_end_size;
   Float64 dist_from_left_end;
   Float64 dist_from_right_end;

   gdr_length      = pBridge->GetSegmentLength(segmentKey);
   left_end_size   = pBridge->GetSegmentStartEndDistance(segmentKey);
   right_end_size  = pBridge->GetSegmentEndEndDistance(segmentKey);

   dist_from_left_end  = poi.GetDistFromStart();
   dist_from_right_end = gdr_length - dist_from_left_end;

   StrandIndexType nDebond = pStrandGeom->GetNumDebondedStrands(segmentKey,strandType);
   ATLASSERT(nDebond <= Ns); // must be true!

   // Determine effectiveness of the debonded strands
   Float64 nDebondedEffective = 0; // number of effective debonded strands
   std::vector<DEBONDCONFIG>::const_iterator iter;

   pgsTypes::StrandType st1, st2;
   if ( strandType == pgsTypes::Permanent )
   {
      st1 = pgsTypes::Straight;
      st2 = pgsTypes::Harped;
   }
   else
   {
      st1 = strandType;
      st2 = strandType;
   }

   const GDRCONFIG& config = pBridge->GetSegmentConfiguration(segmentKey);
   for ( int i = (int)st1; i <= (int)st2; i++ )
   {
      pgsTypes::StrandType st = (pgsTypes::StrandType)i;

      for ( iter = config.PrestressConfig.Debond[st].begin(); iter != config.PrestressConfig.Debond[st].end(); iter++ )
      {
         const DEBONDCONFIG& debond_info = *iter;

         Float64 right_db_from_left = gdr_length - debond_info.DebondLength[pgsTypes::metEnd];

         // see if bonding occurs at all here
         if ( debond_info.DebondLength[pgsTypes::metStart]<dist_from_left_end && dist_from_left_end<right_db_from_left)
         {
            // compute minimum bonded length from poi
            Float64 left_len = dist_from_left_end - debond_info.DebondLength[pgsTypes::metStart];
            Float64 rgt_len  = right_db_from_left - dist_from_left_end;
            Float64 min_db_len = Min(left_len, rgt_len);

            if (min_db_len<xfer_length)
            {
               nDebondedEffective += min_db_len/xfer_length;
            }
            else
            {
               nDebondedEffective += 1.0;
            }

         }
         else
         {
            // strand is not bonded at the location of the POI
            nDebondedEffective += 0.0;
         }
      }
   }

   // Determine effectiveness of bonded strands
   Float64 nBondedEffective = 0; // number of effective bonded strands
   if ( InRange(0.0, dist_from_left_end, xfer_length) )
   {
      // from the left end of the girder, POI is in transfer zone
      nBondedEffective = dist_from_left_end / xfer_length;
   }
   else if ( InRange(0.0, dist_from_right_end,xfer_length) )
   {
      // from the right end of the girder, POI is in transfer zone
      nBondedEffective = dist_from_right_end / xfer_length;
   }
   else
   {
      // strand is fully bonded at the location of the POI
      nBondedEffective = 1.0;
   }

   // nBondedEffective is for 1 strand... make it for all the bonded strands
   nBondedEffective *= (Ns-nDebond);

   Float64 adjust = (nBondedEffective + nDebondedEffective)/Ns;

   return adjust;
}

Float64 pgsPsForceEng::GetXferLengthAdjustment(const pgsPointOfInterest& poi,
                                               pgsTypes::StrandType strandType,
                                               const GDRCONFIG& config)
{
   ////////////////////////////////////////////////////////////////////////////////
   //******************************************************************************
   // NOTE: This method is almost identical to the other GetXferLengthAdjustment method.
   // Any changes done here will likely need to be done there as well
   //******************************************************************************
   ////////////////////////////////////////////////////////////////////////////////
   const CSegmentKey& segmentKey = poi.GetSegmentKey();

   // Compute a scaling factor to apply to the basic prestress force to adjust for transfer length
   // and debonded strands
   Float64 xfer_length = GetXferLength(segmentKey,strandType);

   GET_IFACE(IBridge,pBridge);
   Float64 gdr_length;
   Float64 left_end_size;
   Float64 right_end_size;
   Float64 dist_from_left_end;
   Float64 dist_from_right_end;

   gdr_length      = pBridge->GetSegmentLength(segmentKey);
   left_end_size   = pBridge->GetSegmentStartEndDistance(segmentKey);
   right_end_size  = pBridge->GetSegmentEndEndDistance(segmentKey);

   dist_from_left_end  = poi.GetDistFromStart();
   dist_from_right_end = gdr_length - dist_from_left_end;

   StrandIndexType Ns = (strandType == pgsTypes::Permanent ? config.PrestressConfig.GetStrandCount(pgsTypes::Straight) + config.PrestressConfig.GetStrandCount(pgsTypes::Harped) : config.PrestressConfig.GetStrandCount(strandType));
   StrandIndexType nDebond = (strandType == pgsTypes::Permanent ? config.PrestressConfig.Debond[pgsTypes::Straight].size() + config.PrestressConfig.Debond[pgsTypes::Harped].size() : config.PrestressConfig.Debond[strandType].size());
   ATLASSERT(nDebond <= Ns); // must be true!

   // Quick check to make sure there is even an adjustment to be made
   // If there are no strands, just leave
   if ( Ns == 0 )
   {
      return 1.0;
   }

   // Determine effectiveness of the debonded strands
   Float64 nDebondedEffective = 0; // number of effective debonded strands
   std::vector<DEBONDCONFIG>::const_iterator iter;

   pgsTypes::StrandType st1, st2;
   if ( strandType == pgsTypes::Permanent )
   {
      st1 = pgsTypes::Straight;
      st2 = pgsTypes::Harped;
   }
   else
   {
      st1 = strandType;
      st2 = strandType;
   }

   for ( int i = (int)st1; i <= (int)st2; i++ )
   {
      pgsTypes::StrandType st = (pgsTypes::StrandType)i;

      for ( iter = config.PrestressConfig.Debond[st].begin(); iter != config.PrestressConfig.Debond[st].end(); iter++ )
      {
         const DEBONDCONFIG& debond_info = *iter;

         Float64 right_db_from_left = gdr_length - debond_info.DebondLength[pgsTypes::metEnd];

         // see if bonding occurs at all here
         if ( debond_info.DebondLength[pgsTypes::metStart]<dist_from_left_end && dist_from_left_end<right_db_from_left)
         {
            // compute minimum bonded length from poi
            Float64 left_len = dist_from_left_end - debond_info.DebondLength[pgsTypes::metStart];
            Float64 rgt_len  = right_db_from_left - dist_from_left_end;
            Float64 min_db_len = Min(left_len, rgt_len);

            if (min_db_len<xfer_length)
            {
               nDebondedEffective += min_db_len/xfer_length;
            }
            else
            {
               nDebondedEffective += 1.0;
            }

         }
         else
         {
            // strand is not bonded at the location of the POI
            nDebondedEffective += 0.0;
         }
      }
   }

   // Determine effectiveness of bonded strands
   Float64 nBondedEffective = 0; // number of effective bonded strands
   if ( InRange(0.0, dist_from_left_end, xfer_length) )
   {
      // from the left end of the girder, POI is in transfer zone
      nBondedEffective = dist_from_left_end / xfer_length;
   }
   else if ( InRange(0.0, dist_from_right_end,xfer_length) )
   {
      // from the right end of the girder, POI is in transfer zone
      nBondedEffective = dist_from_right_end / xfer_length;
   }
   else
   {
      // strand is fully bonded at the location of the POI
      nBondedEffective = 1.0;
   }

   // nBondedEffective is for 1 strand... make it for all the bonded strands
   nBondedEffective *= (Ns-nDebond);

   Float64 adjust = (nBondedEffective + nDebondedEffective)/Ns;

   return adjust;
}

Float64 pgsPsForceEng::GetDevLength(const pgsPointOfInterest& poi,bool bDebonded)
{
   STRANDDEVLENGTHDETAILS details = GetDevLengthDetails(poi,bDebonded);
   return details.ld;
}

Float64 pgsPsForceEng::GetDevLength(const pgsPointOfInterest& poi,bool bDebonded,const GDRCONFIG& config)
{
   STRANDDEVLENGTHDETAILS details = GetDevLengthDetails(poi,bDebonded,config);
   return details.ld;
}

STRANDDEVLENGTHDETAILS pgsPsForceEng::GetDevLengthDetails(const pgsPointOfInterest& poi,bool bDebonded)
{
   const CSegmentKey& segmentKey = poi.GetSegmentKey();

   GET_IFACE(IIntervals,pIntervals);
   IntervalIndexType nIntervals = pIntervals->GetIntervalCount(segmentKey);
   IntervalIndexType intervalIdx = nIntervals-1;

   GET_IFACE(ISegmentData,pSegmentData);
   const matPsStrand* pStrand = pSegmentData->GetStrandMaterial(segmentKey,pgsTypes::Permanent);

   GET_IFACE(IGirder,pGirder);
   Float64 mbrDepth = pGirder->GetHeight(poi);

   GET_IFACE(IPretensionForce,pPrestressForce);
   Float64 fpe = pPrestressForce->GetEffectivePrestress(poi,pgsTypes::Permanent,intervalIdx,pgsTypes::End);

   GET_IFACE(IMomentCapacity,pMomCap);
   MOMENTCAPACITYDETAILS mcd;
   pMomCap->GetMomentCapacityDetails(intervalIdx,poi,true,&mcd); // positive moment

   STRANDDEVLENGTHDETAILS details;
   details.db = pStrand->GetNominalDiameter();
   details.fpe = fpe;
   details.fps = mcd.fps;
   details.k = lrfdPsStrand::GetDevLengthFactor(mbrDepth,bDebonded);
   details.ld = lrfdPsStrand::GetDevLength( *pStrand, details.fps, details.fpe, mbrDepth, bDebonded );
   details.lt = GetXferLength(segmentKey,pgsTypes::Permanent);

   return details;
}

STRANDDEVLENGTHDETAILS pgsPsForceEng::GetDevLengthDetails(const pgsPointOfInterest& poi,bool bDebonded,Float64 fps,Float64 fpe)
{
   const CSegmentKey& segmentKey = poi.GetSegmentKey();

   GET_IFACE(ISegmentData,pSegmentData);
   const matPsStrand* pStrand = pSegmentData->GetStrandMaterial(segmentKey,pgsTypes::Permanent);

   GET_IFACE(IGirder,pGirder);
   Float64 mbrDepth = pGirder->GetHeight(poi);

   STRANDDEVLENGTHDETAILS details;
   details.db = pStrand->GetNominalDiameter();
   details.fpe = fpe;
   details.fps = fps;
   details.k = lrfdPsStrand::GetDevLengthFactor(mbrDepth,bDebonded);
   details.ld = lrfdPsStrand::GetDevLength( *pStrand, details.fps, details.fpe, mbrDepth, bDebonded );
   details.lt = GetXferLength(segmentKey,pgsTypes::Permanent);

   return details;
}

STRANDDEVLENGTHDETAILS pgsPsForceEng::GetDevLengthDetails(const pgsPointOfInterest& poi,bool bDebonded,const GDRCONFIG& config)
{
   const CSegmentKey& segmentKey = poi.GetSegmentKey();

   GET_IFACE(IIntervals,pIntervals);
   IntervalIndexType nIntervals = pIntervals->GetIntervalCount(segmentKey);
   IntervalIndexType intervalIdx = nIntervals-1;

   GET_IFACE(ISegmentData,pSegmentData);
   const matPsStrand* pStrand = pSegmentData->GetStrandMaterial(segmentKey,pgsTypes::Permanent);

   GET_IFACE(IGirder,pGirder);
   Float64 mbrDepth = pGirder->GetHeight(poi);

   GET_IFACE(IPretensionForce,pPrestressForce);
   Float64 fpe = pPrestressForce->GetEffectivePrestress(poi,config,pgsTypes::Permanent,intervalIdx,pgsTypes::End);

   GET_IFACE(IMomentCapacity,pMomCap);
   MOMENTCAPACITYDETAILS mcd;
   pMomCap->GetMomentCapacityDetails(intervalIdx,poi,config,true,&mcd); // positive moment

   STRANDDEVLENGTHDETAILS details;
   details.db = pStrand->GetNominalDiameter();
   details.fpe = fpe;
   details.fps = mcd.fps;
   details.k = lrfdPsStrand::GetDevLengthFactor(mbrDepth,bDebonded);
   details.ld = lrfdPsStrand::GetDevLength( *pStrand, details.fps, details.fpe, mbrDepth, bDebonded );
   details.lt = GetXferLength(segmentKey,pgsTypes::Permanent);

   return details;
}

STRANDDEVLENGTHDETAILS pgsPsForceEng::GetDevLengthDetails(const pgsPointOfInterest& poi,bool bDebonded,const GDRCONFIG& config,Float64 fps,Float64 fpe)
{
   const CSegmentKey& segmentKey = poi.GetSegmentKey();

   GET_IFACE(ISegmentData,pSegmentData);
   const matPsStrand* pStrand = pSegmentData->GetStrandMaterial(segmentKey,pgsTypes::Permanent);

   GET_IFACE(IGirder,pGirder);
   Float64 mbrDepth = pGirder->GetHeight(poi);

   STRANDDEVLENGTHDETAILS details;
   details.db = pStrand->GetNominalDiameter();
   details.fpe = fpe;
   details.fps = fps;
   details.k = lrfdPsStrand::GetDevLengthFactor(mbrDepth,bDebonded);
   details.ld = lrfdPsStrand::GetDevLength( *pStrand, details.fps, details.fpe, mbrDepth, bDebonded );
   details.lt = GetXferLength(segmentKey,pgsTypes::Permanent);

   return details;
}

Float64 pgsPsForceEng::GetDevLengthAdjustment(const pgsPointOfInterest& poi,StrandIndexType strandIdx,pgsTypes::StrandType strandType)
{
   STRANDDEVLENGTHDETAILS details = GetDevLengthDetails(poi,false);
   return GetDevLengthAdjustment(poi,strandIdx,strandType,details.fps,details.fpe);
}

Float64 pgsPsForceEng::GetDevLengthAdjustment(const pgsPointOfInterest& poi,StrandIndexType strandIdx,pgsTypes::StrandType strandType,Float64 fps,Float64 fpe)
{
   const CSegmentKey& segmentKey = poi.GetSegmentKey();

   GET_IFACE(IBridge,pBridge);
   GDRCONFIG config = pBridge->GetSegmentConfiguration(segmentKey);
   return GetDevLengthAdjustment(poi,strandIdx,strandType,config,fps,fpe);
}

Float64 pgsPsForceEng::GetDevLengthAdjustment(const pgsPointOfInterest& poi,StrandIndexType strandIdx,pgsTypes::StrandType strandType,const GDRCONFIG& config)
{
   STRANDDEVLENGTHDETAILS details = GetDevLengthDetails(poi,false,config);
   return GetDevLengthAdjustment(poi,strandIdx,strandType,config,details.fps,details.fpe);
}

Float64 pgsPsForceEng::GetDevLengthAdjustment(const pgsPointOfInterest& poi,StrandIndexType strandIdx,pgsTypes::StrandType strandType,const GDRCONFIG& config,Float64 fps,Float64 fpe)
{
   // Compute a scaling factor to apply to the basic prestress force to
   // adjust for prestress force in the development
   const CSegmentKey& segmentKey = poi.GetSegmentKey();


   Float64 poi_loc = poi.GetDistFromStart();

   GET_IFACE(IStrandGeometry,pStrandGeom);
   Float64 bond_start, bond_end;
   bool bDebonded = pStrandGeom->IsStrandDebonded(segmentKey,strandIdx,strandType,config,&bond_start,&bond_end);
   bool bExtendedStrand = pStrandGeom->IsExtendedStrand(poi,strandIdx,strandType,config);

   // determine minimum bonded length from poi
   Float64 left_bonded_length, right_bonded_length;
   if ( bDebonded )
   {
      // measure bonded length
      left_bonded_length = poi_loc - bond_start;
      right_bonded_length = bond_end - poi_loc;
   }
   else if ( bExtendedStrand )
   {
      // strand is extended into end diaphragm... the development lenght adjustment is 1.0
      return 1.0;
   }
   else
   {
      // no debonding, bond length is to ends of girder
      GET_IFACE(IBridge,pBridge);
      Float64 gdr_length  = pBridge->GetSegmentLength(segmentKey);

      left_bonded_length  = poi_loc;
      right_bonded_length = gdr_length - poi_loc;
   }

   Float64 lpx = Min(left_bonded_length, right_bonded_length);

   if (lpx <= 0.0)
   {
      // strand is unbonded at location, no more to do
      return 0.0;
   }
   else
   {
      STRANDDEVLENGTHDETAILS details = GetDevLengthDetails(poi,bDebonded,config,fps,fpe);
      Float64 xfer_length = details.lt;
      Float64 dev_length  = details.ld;

      Float64 adjust = -999; // dummy value, helps with debugging

      if ( lpx <= xfer_length)
      {
         adjust = (fpe < fps ? (lpx*fpe) / (xfer_length*fps) : 1.0);
      }
      else if ( lpx < dev_length )
      {
         adjust = (fpe + (lpx - xfer_length)*(fps-fpe)/(dev_length - xfer_length))/fps;
      }
      else
      {
         adjust = 1.0;
      }

      adjust = IsZero(adjust) ? 0 : adjust;
      adjust = ::ForceIntoRange(0.0,adjust,1.0);
      return adjust;
   }
}

Float64 pgsPsForceEng::GetHoldDownForce(const CSegmentKey& segmentKey)
{
#pragma Reminder("STRAND SLOPE/CANTILEVER: need to account for left/right harp point and strands are not symmetrical")
   GET_IFACE(IStrandGeometry,pStrandGeom);

   StrandIndexType Nh = pStrandGeom->GetStrandCount(segmentKey,pgsTypes::Harped);
   if (0 < Nh)
   {
      GET_IFACE(IPointOfInterest,pPoi);
      std::vector<pgsPointOfInterest> vPoi( pPoi->GetPointsOfInterest(segmentKey,POI_HARPINGPOINT) );
   
      // no hold down force if there aren't any harped strands
      if ( vPoi.size() == 0 )
      {
         return 0;
      }

      pgsPointOfInterest& poi(vPoi.front());
   
      GET_IFACE(IIntervals,pIntervals);
      IntervalIndexType intervalIdx = pIntervals->GetStressStrandInterval(segmentKey);
      Float64 harped = GetPrestressForce(poi,pgsTypes::Harped,intervalIdx,pgsTypes::Start);

      // Adjust for slope
      Float64 slope = pStrandGeom->GetAvgStrandSlope( poi );

      Float64 F;
      F = harped / sqrt( 1*1 + slope*slope );

      return F;
   }
   else
   {
      return 0;
   }
}

Float64 pgsPsForceEng::GetHoldDownForce(const CSegmentKey& segmentKey,const GDRCONFIG& config)
{
   if (0 < config.PrestressConfig.GetStrandCount(pgsTypes::Harped))
   {
      GET_IFACE(IPointOfInterest,pPoi);
      std::vector<pgsPointOfInterest> vPoi( pPoi->GetPointsOfInterest(segmentKey,POI_HARPINGPOINT) );
   
      // no hold down force if there aren't any harped strands
      if ( vPoi.size() == 0 )
      {
         return 0;
      }

      pgsPointOfInterest& poi(vPoi.front());
   
      GET_IFACE(IIntervals,pIntervals);
      IntervalIndexType intervalIdx = pIntervals->GetStressStrandInterval(segmentKey);
      Float64 harped = GetPrestressForce(poi,pgsTypes::Harped,intervalIdx,pgsTypes::Start,config);

      // Adjust for slope
      GET_IFACE(IStrandGeometry,pStrandGeom);
      Float64 slope = pStrandGeom->GetAvgStrandSlope( poi,config.PrestressConfig.GetStrandCount(pgsTypes::Harped),config.PrestressConfig.EndOffset,config.PrestressConfig.HpOffset );

      Float64 F;
      F = harped / sqrt( 1*1 + slope*slope );

      return F;
   }
   else
   {
      return 0;
   }
}

void pgsPsForceEng::MakeCopy(const pgsPsForceEng& rOther)
{
   m_pBroker = rOther.m_pBroker;
   m_StatusGroupID = rOther.m_StatusGroupID;

   m_PrestressTransferComputationType = rOther.m_PrestressTransferComputationType;
}

void pgsPsForceEng::MakeAssignment(const pgsPsForceEng& rOther)
{
   MakeCopy( rOther );
}

Float64 pgsPsForceEng::GetPrestressForce(const pgsPointOfInterest& poi,pgsTypes::StrandType strandType,IntervalIndexType intervalIdx,pgsTypes::IntervalTimeType intervalTime)
{
   const CSegmentKey& segmentKey = poi.GetSegmentKey();

   GET_IFACE(IStrandGeometry,pStrandGeom);
   StrandIndexType N = pStrandGeom->GetStrandCount(segmentKey,strandType);
   
   GET_IFACE(ISegmentData,pSegmentData );
   const matPsStrand* pStrand = pSegmentData->GetStrandMaterial(segmentKey,strandType);

   Float64 fpe = GetEffectivePrestress(poi,strandType,intervalIdx,intervalTime);

   Float64 aps = pStrand->GetNominalArea();

   Float64 P = aps*N*fpe;

   return P;
}

Float64 pgsPsForceEng::GetPrestressForce(const pgsPointOfInterest& poi,pgsTypes::StrandType strandType,IntervalIndexType intervalIdx,pgsTypes::IntervalTimeType intervalTime,const GDRCONFIG& config)
{
   const CSegmentKey& segmentKey = poi.GetSegmentKey();

   StrandIndexType N = (strandType == pgsTypes::Permanent ? config.PrestressConfig.GetStrandCount(pgsTypes::Straight) + config.PrestressConfig.GetStrandCount(pgsTypes::Harped) : config.PrestressConfig.GetStrandCount(strandType));
   
   GET_IFACE(ISegmentData,pSegmentData );
   const matPsStrand* pStrand = pSegmentData->GetStrandMaterial(segmentKey,strandType);

   Float64 fpe = GetEffectivePrestress(poi,strandType,intervalIdx,intervalTime,&config);

   Float64 aps = pStrand->GetNominalArea();

   Float64 P = aps*N*fpe;

   return P;
}

Float64 pgsPsForceEng::GetEffectivePrestress(const pgsPointOfInterest& poi,pgsTypes::StrandType strandType,IntervalIndexType intervalIdx,pgsTypes::IntervalTimeType intervalTime)
{
   return GetEffectivePrestress(poi,strandType,intervalIdx,intervalTime,NULL);
}

Float64 pgsPsForceEng::GetEffectivePrestress(const pgsPointOfInterest& poi,pgsTypes::StrandType strandType,IntervalIndexType intervalIdx,pgsTypes::IntervalTimeType intervalTime,const GDRCONFIG* pConfig)
{
   GET_IFACE(IPointOfInterest,pPoi);
   if ( pPoi->IsOffSegment(poi) )
   {
      return 0;
   }

   const CSegmentKey& segmentKey = poi.GetSegmentKey();

   // Get the prestressing input information
   GET_IFACE(ISegmentData,pSegmentData );
   const matPsStrand* pStrand = pSegmentData->GetStrandMaterial(segmentKey,strandType);

   GET_IFACE(IIntervals,pIntervals);
   IntervalIndexType releaseIntervalIdx        = pIntervals->GetPrestressReleaseInterval(segmentKey);
   IntervalIndexType castDeckIntervalIdx       = pIntervals->GetCastDeckInterval(segmentKey);
   IntervalIndexType tsInstallationIntervalIdx = pIntervals->GetTemporaryStrandInstallationInterval(segmentKey);
   IntervalIndexType tsRemovalIntervalIdx      = pIntervals->GetTemporaryStrandRemovalInterval(segmentKey);

   Float64 Pj;
   StrandIndexType N;

   if ( pConfig )
   {
      if ( strandType == pgsTypes::Permanent )
      {
         Pj = pConfig->PrestressConfig.Pjack[pgsTypes::Straight]          + pConfig->PrestressConfig.Pjack[pgsTypes::Harped];
         N  = pConfig->PrestressConfig.GetStrandCount(pgsTypes::Straight) + pConfig->PrestressConfig.GetStrandCount(pgsTypes::Harped);
      }
      else
      {
         Pj = pConfig->PrestressConfig.Pjack[strandType];
         N  = pConfig->PrestressConfig.GetStrandCount(strandType);
      }
   }
   else
   {
      GET_IFACE(IStrandGeometry,pStrandGeom);
      Pj = pStrandGeom->GetPjack(segmentKey,strandType);
      N  = pStrandGeom->GetStrandCount(segmentKey,strandType);
   }

   if ( strandType == pgsTypes::Temporary )
   {
      if ( pConfig )
      {
         if ( intervalIdx < tsInstallationIntervalIdx || tsRemovalIntervalIdx <= intervalIdx )
         {
            N = 0;
            Pj = 0;
         }
      }
      else
      {
         if ( castDeckIntervalIdx <= intervalIdx )
         {
            N = 0;
            Pj = 0;
         }
      }
   }

   if ( strandType == pgsTypes::Temporary && N == 0)
   {
      // if we are after temporary strand stress and Nt is zero... the result is zero
      return 0;
   }

   if ( IsZero(Pj) && N == 0 )
   {
      // no strand, no jack force... the strand stress is 0
      return 0;
   }

   Float64 aps = pStrand->GetNominalArea();

   // Compute the jacking stress 
   Float64 fpj = Pj/(aps*N);

   // Compute the requested strand stress
   Float64 loss = GetPrestressLoss(poi,strandType,intervalIdx,intervalTime,pConfig);
   Float64 fps = fpj - loss;

   ATLASSERT( 0 <= fps ); // strand stress must be greater than or equal to zero.

   // Reduce for transfer effect (no transfer effect if the strands aren't released
   if ( releaseIntervalIdx <= intervalIdx )
   {
      Float64 adjust = (pConfig ? GetXferLengthAdjustment(poi,strandType,*pConfig) : GetXferLengthAdjustment(poi,strandType));
      fps *= adjust;
   }

   return fps;
}

Float64 pgsPsForceEng::GetPrestressForceWithLiveLoad(const pgsPointOfInterest& poi,pgsTypes::StrandType strandType,const GDRCONFIG* pConfig)
{
   GET_IFACE(IPointOfInterest,pPoi);
   if ( pPoi->IsOffSegment(poi) )
   {
      return 0;
   }

   const CSegmentKey& segmentKey = poi.GetSegmentKey();

   GET_IFACE(IStrandGeometry,pStrandGeom);
   StrandIndexType N;
   if ( pConfig )
   {
       N = pConfig->PrestressConfig.GetStrandCount(strandType);
   }
   else
   {
      N = pStrandGeom->GetStrandCount(segmentKey,strandType);
   }
   
   GET_IFACE(ISegmentData,pSegmentData );
   const matPsStrand* pStrand = pSegmentData->GetStrandMaterial(segmentKey,strandType);

   Float64 fpe = GetEffectivePrestressWithLiveLoad(poi,strandType,pConfig);

   Float64 aps = pStrand->GetNominalArea();

   Float64 P = aps*N*fpe;

   return P;
}

Float64 pgsPsForceEng::GetEffectivePrestressWithLiveLoad(const pgsPointOfInterest& poi,pgsTypes::StrandType strandType,const GDRCONFIG* pConfig)
{
   const CSegmentKey& segmentKey = poi.GetSegmentKey();

   // Get the prestressing input information
   GET_IFACE(IStrandGeometry,pStrandGeom);
   GET_IFACE(ISegmentData,pSegmentData );
   const matPsStrand* pStrand = pSegmentData->GetStrandMaterial(segmentKey,strandType);

   Float64 Pj;
   StrandIndexType N;

   if ( pConfig )
   {
      if ( strandType == pgsTypes::Permanent )
      {
         Pj = pConfig->PrestressConfig.Pjack[pgsTypes::Straight]          + pConfig->PrestressConfig.Pjack[pgsTypes::Harped];
         N  = pConfig->PrestressConfig.GetStrandCount(pgsTypes::Straight) + pConfig->PrestressConfig.GetStrandCount(pgsTypes::Harped);
      }
      else
      {
         Pj = pConfig->PrestressConfig.Pjack[strandType];
         N  = pConfig->PrestressConfig.GetStrandCount(strandType);
      }
   }
   else
   {
      Pj = pStrandGeom->GetPjack(segmentKey,strandType);
      N  = pStrandGeom->GetStrandCount(segmentKey,strandType);
   }

   if ( strandType == pgsTypes::Temporary )
   {
      N = 0;
      Pj = 0;
   }

   if ( strandType == pgsTypes::Temporary && N == 0)
   {
      // if we are after temporary strand stress and Nt is zero... the result is zero
      return 0;
   }

   if ( IsZero(Pj) && N == 0 )
   {
      // no strand, no jack force... the strand stress is 0
      return 0;
   }

   Float64 aps = pStrand->GetNominalArea();

   // Compute the jacking stress 
   Float64 fpj = Pj/(aps*N);

   // Compute the requested strand stress
   Float64 loss = GetPrestressLossWithLiveLoad(poi,strandType,pConfig);
   Float64 fps = fpj - loss;

   ATLASSERT( 0 <= fps ); // strand stress must be greater than or equal to zero.

   // Reduce for transfer effect
   Float64 adjust;
   if ( pConfig )
   {
      adjust = GetXferLengthAdjustment(poi,strandType,*pConfig);
   }
   else
   {
      adjust = GetXferLengthAdjustment(poi,strandType);
   }

   fps *= adjust;

   return fps;
}

Float64 pgsPsForceEng::GetPrestressLoss(const pgsPointOfInterest& poi,pgsTypes::StrandType strandType,IntervalIndexType intervalIdx,pgsTypes::IntervalTimeType intervalTime)
{
   GET_IFACE(ILosses,pLosses);
   const LOSSDETAILS* pDetails = pLosses->GetLossDetails(poi,intervalIdx);
   return GetPrestressLoss(poi,strandType,intervalIdx,intervalTime,pDetails);
}

Float64 pgsPsForceEng::GetPrestressLoss(const pgsPointOfInterest& poi,pgsTypes::StrandType strandType,IntervalIndexType intervalIdx,pgsTypes::IntervalTimeType intervalTime,const GDRCONFIG* pConfig)
{
   GET_IFACE(ILosses,pLosses);
   const LOSSDETAILS* pDetails;
   if ( pConfig )
   {
      pDetails = pLosses->GetLossDetails(poi,*pConfig);
   }
   else
   {
      pDetails = pLosses->GetLossDetails(poi,intervalIdx);
   }

   return GetPrestressLoss(poi,strandType,intervalIdx,intervalTime,pDetails);
}

Float64 pgsPsForceEng::GetPrestressLoss(const pgsPointOfInterest& poi,pgsTypes::StrandType strandType,IntervalIndexType intervalIdx,pgsTypes::IntervalTimeType intervalTime,const LOSSDETAILS* pDetails)
{
   // NOTE: What we mean by "loss" is the change in prestress force from jacking to the interval in question. 
   // "Losses" include time dependent effects and elastic effects (elastic shortening and elastic gains due to superimposed dead loads)
   //
   // fpe = fpj - loss

   const CSegmentKey& segmentKey = poi.GetSegmentKey();

   // if losses were computed with the time-step method, just look up the prestress loss
   if ( pDetails->LossMethod == pgsTypes::TIME_STEP )
   {
      if ( intervalIdx == 0 && intervalTime == pgsTypes::Start )
      {
         return 0; // wanting losses at the start of the first interval.. nothing has happened yet
      }

      // time step losses are computed for the end of an interval
      IntervalIndexType theIntervalIdx = intervalIdx;
      switch(intervalTime)
      {
      case pgsTypes::Start:
         theIntervalIdx--; // losses at start of this interval are equal to losses at end of previous interval
         break;

      case pgsTypes::Middle:
         // drop through so we just use the end
      case pgsTypes::End:
         break; // do nothing... theIntervalIdx is correct
      }

#if !defined LUMP_STRANDS
      GET_IFACE(IStrandGeometry,pStrandGeom);
#endif

      if ( strandType == pgsTypes::Permanent) 
      {
#if defined LUMP_STRANDS
         return pDetails->TimeStepDetails[theIntervalIdx].Strands[pgsTypes::Straight].loss + 
                pDetails->TimeStepDetails[theIntervalIdx].Strands[pgsTypes::Harped].loss;
#else
         Float64 Aps[2];
         Float64 Aps_Loss[2];
         for ( int i = 0; i < 2; i++ )
         {
            Aps[i]      = 0;
            Aps_Loss[i] = 0;
         }

         for ( int j = 0; j < 2; j++ )
         {
            pgsTypes::StrandType st = (pgsTypes::StrandType)j;
            StrandIndexType nStrands = pStrandGeom->GetStrandCount(segmentKey,st);
            for ( StrandIndexType strandIdx = 0; strandIdx < nStrands; strandIdx++ )
            {
               const TIME_STEP_STRAND& strand = pDetails->TimeStepDetails[i].Strands[st][strandIdx];
               Aps[st]      += strand.As;
               Aps_Loss[st] += strand.As*strand.loss;
            }
         }

         Float64 ss_Loss = IsZero(Aps[pgsTypes::Straight]) ? 0 : Aps_Loss[pgsTypes::Straight]/Aps[pgsTypes::Straight];
         Float64 hs_Loss = IsZero(Aps[pgsTypes::Harped])   ? 0 : Aps_Loss[pgsTypes::Harped]/Aps[pgsTypes::Harped];

         return ss_Loss + hs_Loss;
#endif
      }
      else
      {
#if defined LUMP_STRANDS
         return pDetails->TimeStepDetails[theIntervalIdx].Strands[strandType].loss;
#else
         StrandIndexType nStrands = pStrandGeom->GetStrandCount(segmentKey,strandType);
         Float64 Aps = 0;
         Float64 Aps_loss = 0;
         for (StrandIndexType strandIdx = 0; strandIdx < nStrands; strandIdx++ )
         {
            const TIME_STEP_STRAND& strand(pDetails->TimeStepDetails[theIntervalIdx].Strands[strandType][strandIdx]);
            Aps += strand.As;
            Aps_loss += strand.As*strand.loss;
         }

         Float64 loss = IsZero(Aps) ? 0 : Aps_loss/Aps;
         return loss;
#endif
      }
   }
   else
   {
      // some method other than Time Step
      GET_IFACE(IIntervals,pIntervals);
      IntervalIndexType stressStrandIntervalIdx  = pIntervals->GetStressStrandInterval(segmentKey);
      IntervalIndexType releaseIntervalIdx       = pIntervals->GetPrestressReleaseInterval(segmentKey);
      IntervalIndexType liftSegmentIntervalIdx   = pIntervals->GetLiftSegmentInterval(segmentKey);
      IntervalIndexType storageIntervalIdx       = pIntervals->GetStorageInterval(segmentKey);
      IntervalIndexType haulSegmentIntervalIdx   = pIntervals->GetHaulSegmentInterval(segmentKey);
      IntervalIndexType erectSegmentIntervalIdx  = pIntervals->GetErectSegmentInterval(segmentKey);
      IntervalIndexType tsInstallationIntervalIdx = pIntervals->GetTemporaryStrandInstallationInterval(segmentKey);
      IntervalIndexType tsRemovalIntervalIdx     = pIntervals->GetTemporaryStrandRemovalInterval(segmentKey);
      IntervalIndexType castDeckIntervalIdx      = pIntervals->GetCastDeckInterval(segmentKey);
      IntervalIndexType compositeDeckIntervalIdx = pIntervals->GetCompositeDeckInterval(segmentKey);
      IntervalIndexType railingSystemIntervalIdx = pIntervals->GetInstallRailingSystemInterval(segmentKey);
      IntervalIndexType overlayIntervalIdx       = pIntervals->GetOverlayInterval(segmentKey);
      IntervalIndexType liveLoadIntervalIdx      = pIntervals->GetLiveLoadInterval(segmentKey);

      GET_IFACE(IBridge,pBridge);
      bool bIsFutureOverlay = pBridge->IsFutureOverlay();

      Float64 loss = 0;
      if ( intervalIdx == stressStrandIntervalIdx )
      {
         if ( intervalTime == pgsTypes::Start )
         {
            loss = 0.0;
         }
         else if ( intervalTime == pgsTypes::Middle )
         {
            if ( strandType == pgsTypes::Temporary )
            {
               loss = pDetails->pLosses->TemporaryStrand_BeforeTransfer();
            }
            else
            {
               loss = pDetails->pLosses->PermanentStrand_BeforeTransfer();
            }

            loss /= 2.0;
         }
         else if ( intervalTime == pgsTypes::End )
         {
            if ( strandType == pgsTypes::Temporary )
            {
               loss = pDetails->pLosses->TemporaryStrand_BeforeTransfer();
            }
            else
            {
               loss = pDetails->pLosses->PermanentStrand_BeforeTransfer();
            }
         }
      }
      else if ( intervalIdx == releaseIntervalIdx || intervalIdx == storageIntervalIdx )
      {
         if ( strandType == pgsTypes::Temporary )
         {
            loss = pDetails->pLosses->TemporaryStrand_AfterTransfer();
         }
         else
         {
            loss = pDetails->pLosses->PermanentStrand_AfterTransfer();
         }
      }
      else if ( intervalIdx == liftSegmentIntervalIdx )
      {
         if ( strandType == pgsTypes::Temporary )
         {
            loss = pDetails->pLosses->TemporaryStrand_AtLifting();
         }
         else
         {
            loss = pDetails->pLosses->PermanentStrand_AtLifting();
         }
      }
      else if ( intervalIdx == haulSegmentIntervalIdx )
      {
         if ( strandType == pgsTypes::Temporary )
         {
            loss = pDetails->pLosses->TemporaryStrand_AtShipping();
         }
         else
         {
            loss = pDetails->pLosses->PermanentStrand_AtShipping();
         }
      }
      else if ( intervalIdx == tsInstallationIntervalIdx )
      {
         if ( strandType == pgsTypes::Temporary )
         {
            loss = pDetails->pLosses->TemporaryStrand_AfterTemporaryStrandInstallation();
         }
         else
         {
            loss = pDetails->pLosses->PermanentStrand_AfterTemporaryStrandInstallation();
         }
      }
      else if ( intervalIdx == erectSegmentIntervalIdx )
      {
         if ( strandType == pgsTypes::Temporary )
         {
            loss = pDetails->pLosses->TemporaryStrand_BeforeTemporaryStrandRemoval();
         }
         else
         {
            loss = pDetails->pLosses->PermanentStrand_BeforeTemporaryStrandRemoval();
         }
      }
      else if ( intervalIdx == tsRemovalIntervalIdx )
      {
         if ( strandType == pgsTypes::Temporary )
         {
            loss = pDetails->pLosses->TemporaryStrand_AfterTemporaryStrandRemoval();
         }
         else
         {
            loss = pDetails->pLosses->PermanentStrand_AfterTemporaryStrandRemoval();
         }
      }
      else if ( intervalIdx == castDeckIntervalIdx || intervalIdx == compositeDeckIntervalIdx )
      {
         if ( strandType == pgsTypes::Temporary )
         {
            loss = pDetails->pLosses->TemporaryStrand_AfterDeckPlacement();
         }
         else
         {
            loss = pDetails->pLosses->PermanentStrand_AfterDeckPlacement();
         }
      }
      else if ( intervalIdx == railingSystemIntervalIdx || (intervalIdx == overlayIntervalIdx && !bIsFutureOverlay) )
      {
         if ( strandType == pgsTypes::Temporary )
         {
            loss = pDetails->pLosses->TemporaryStrand_AfterSIDL();
         }
         else
         {
            loss = pDetails->pLosses->PermanentStrand_AfterSIDL();
         }
      }
      else if ( intervalIdx == liveLoadIntervalIdx || (intervalIdx == overlayIntervalIdx && bIsFutureOverlay)  )
      {
         if ( strandType == pgsTypes::Temporary )
         {
            loss = pDetails->pLosses->TemporaryStrand_Final();
         }
         else
         {
            // this is kind of a hack... using middle to mean don't include live load
            if ( intervalTime == pgsTypes::Middle )
            {
               loss = pDetails->pLosses->PermanentStrand_Final();
            }
            else
            {
               GET_IFACE(ILoadFactors,pLoadFactors);
               const CLoadFactors* pLF = pLoadFactors->GetLoadFactors();
               Float64 gLL = pLF->LLIMmax[pgsTypes::ServiceIII];
               loss = pDetails->pLosses->PermanentStrand_FinalWithLiveLoad(gLL);
            }
         }

      }
      else
      {
         ATLASSERT(false); // didn't expect that interval....
      }

      return loss;
   }
}

Float64 pgsPsForceEng::GetPrestressLossWithLiveLoad(const pgsPointOfInterest& poi,pgsTypes::StrandType strandType,const GDRCONFIG* pConfig)
{
   const CSegmentKey& segmentKey = poi.GetSegmentKey();

   GET_IFACE(ILosses,pLosses);
   const LOSSDETAILS* pDetails;
   if ( pConfig )
   {
      pDetails = pLosses->GetLossDetails(poi,*pConfig);
   }
   else
   {
      pDetails = pLosses->GetLossDetails(poi);
   }


   if ( pDetails->LossMethod == pgsTypes::TIME_STEP )
   {
#pragma Reminder("UPDATE: need an actual interval for this method")
      // we are using the interval when live load is FIRST applied... live load is
      // applied in this interval and all intervals that follow... which interval do
      // you want losses for?
      GET_IFACE(IIntervals,pIntervals);
      IntervalIndexType liveLoadIntervalIdx = pIntervals->GetLiveLoadInterval(segmentKey);

#if !defined LUMP_STRANDS
      GET_IFACE(IStrandGeometry,pStrandGeom);
#endif

      // time step losses use service III limit state for elastic live load effects
      if ( strandType == pgsTypes::Permanent )
      {
#if defined LUMP_STRANDS
         return pDetails->TimeStepDetails[liveLoadIntervalIdx].Strands[pgsTypes::Straight].loss + 
                pDetails->TimeStepDetails[liveLoadIntervalIdx].Strands[pgsTypes::Harped].loss;
#else
         Float64 Aps[2] = {0,0};
         Float64 Aps_Loss[2] = {0,0};
         Float64 Loss[2] = {0,0};
         for ( int i = 0; i < 2; i++ )
         {
            pgsTypes::StrandType strandType = (pgsTypes::StrandType)i;
            StrandIndexType nStrands = pStrandGeom->GetStrandCount(segmentKey,strandType);
            for ( StrandIndexType strandIdx = 0; strandIdx < nStrands; strandIdx++ )
            {
               const TIME_STEP_STRAND& strand(pDetails->TimeStepDetails[liveLoadIntervalIdx].Strands[strandType][strandIdx]);
               Aps[strandType] += strand.As;
               Aps_Loss[strandType] += strand.As*strand.loss;
            }
            Loss[strandType] = IsZero(Aps[strandType]) ? 0 : Aps_Loss[strandType]/Aps[strandType];
         }
         return Loss[pgsTypes::Straight] + Loss[pgsTypes::Harped];
#endif
      }
      else
      {
#if defined LUMP_STRANDS
         return pDetails->TimeStepDetails[liveLoadIntervalIdx].Strands[strandType].loss;
#else
         Float64 Aps = 0;
         Float64 Aps_Loss = 0;

         StrandIndexType nStrands = pStrandGeom->GetStrandCount(segmentKey,strandType);
         for ( StrandIndexType strandIdx = 0; strandIdx < nStrands; strandIdx++ )
         {
            const TIME_STEP_STRAND& strand(pDetails->TimeStepDetails[liveLoadIntervalIdx].Strands[strandType][strandIdx]);
            Aps += strand.As;
            Aps_Loss += strand.As*strand.loss;
         }
         Float64 Loss = IsZero(Aps) ? 0 : Aps_Loss/Aps;
         return Loss;
#endif
      }
   }

   GET_IFACE(ILoadFactors,pLoadFactors);
   const CLoadFactors* pLF = pLoadFactors->GetLoadFactors();
   Float64 gLL = pLF->LLIMmax[pgsTypes::ServiceIII];
   return pDetails->pLosses->PermanentStrand_FinalWithLiveLoad(gLL);
}

//LOSSDETAILS pgsPsForceEng::FindLosses(const pgsPointOfInterest& poi,const GDRCONFIG& config)
//{
//   // first see if we have cached our losses
//   LOSSDETAILS details;
//
//   if (m_DesignLosses.GetFromCache(poi,config,&details))
//   {
//      return details;
//   }
//   else
//   {
//      // not cached, compute, cache and return
//      ComputeLosses(poi,config,&details);
//
//      m_DesignLosses.SaveToCache(poi,config,details);
//   }
//
//   return details;
//}
