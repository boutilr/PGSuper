///////////////////////////////////////////////////////////////////////
// PGSuper - Prestressed Girder SUPERstructure Design and Analysis
// Copyright (C) 2002  Washington State Department of Transportation
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

// PsLossEngineer.h : Declaration of the CPsLossEngineer

#ifndef __PSLOSSENGINEER_H_
#define __PSLOSSENGINEER_H_

#include "resource.h"       // main symbols
#include <Details.h>
#include <IFace\DisplayUnits.h>

class lrfdLosses;

/////////////////////////////////////////////////////////////////////////////
// CPsLossEngineer
class CPsLossEngineer
{
   enum LossAgency{laWSDOT, laTxDOT, laAASHTO};
public:
 	CPsLossEngineer()
	{
      m_bComputingLossesForDesign = false;
	}

public:
   enum BeamType { IBeam, UBeam, SolidSlab, BoxBeam, SingleT };

   virtual LOSSDETAILS ComputeLosses(IBroker* pBroker,long agentID,BeamType beamType,const pgsPointOfInterest& poi);
   virtual LOSSDETAILS ComputeLossesForDesign(IBroker* pBroker,long agentID,BeamType beamType,const pgsPointOfInterest& poi,const GDRCONFIG& config);
   virtual void BuildReport(IBroker* pBroker,BeamType beamType,SpanIndexType span,GirderIndexType gdr,rptChapter* pChapter,IDisplayUnits* pDispUnit);

private:
   LOSSDETAILS ComputeLosses(IBroker* pBroker,long agentID,BeamType beamType,const pgsPointOfInterest& poi,const GDRCONFIG& config);

   void LossesByRefinedEstimate(IBroker* pBroker,long agentID,BeamType beamType,const pgsPointOfInterest& poi,const GDRCONFIG& config,LOSSDETAILS* pLosses,LossAgency lossAgency);
   void LossesByRefinedEstimateBefore2005(IBroker* pBroker,long agentID,BeamType beamType,const pgsPointOfInterest& poi,const GDRCONFIG& config,LOSSDETAILS* pLosses);
   void LossesByRefinedEstimate2005(IBroker* pBroker,long agentID,BeamType beamType,const pgsPointOfInterest& poi,const GDRCONFIG& config,LOSSDETAILS* pLosses,LossAgency lossAgency);
   void LossesByApproxLumpSum(IBroker* pBroker,long agentID,BeamType beamType,const pgsPointOfInterest& poi,const GDRCONFIG& config,LOSSDETAILS* pLosses,bool isWsdot);
   void LossesByGeneralLumpSum(IBroker* pBroker,long agentID,BeamType beamType,const pgsPointOfInterest& poi,const GDRCONFIG& config,LOSSDETAILS* pLosses);

   void ReportRefinedMethod(IBroker* pBroker,BeamType beamType,SpanIndexType span,GirderIndexType gdr,rptChapter* pChapter,IDisplayUnits* pDispUnit,Uint16 level,LossAgency lossAgency);
   void ReportApproxLumpSumMethod(IBroker* pBroker,BeamType beamType,SpanIndexType span,GirderIndexType gdr,rptChapter* pChapter,IDisplayUnits* pDispUnit,Uint16 level,bool isWsdot);
   void ReportGeneralLumpSumMethod(IBroker* pBroker,BeamType beamType,SpanIndexType span,GirderIndexType gdr,rptChapter* pChapter,IDisplayUnits* pDispUnit,Uint16 level);
   void ReportLumpSumMethod(rptChapter* pChapter,IBroker* pBroker,CPsLossEngineer::BeamType beamType,SpanIndexType span,GirderIndexType gdr,IDisplayUnits* pDispUnit,Uint16 level);

   void ReportRefinedMethodBefore2005(rptChapter* pChapter,IBroker* pBroker,CPsLossEngineer::BeamType beamType,SpanIndexType span,GirderIndexType gdr,IDisplayUnits* pDispUnit,Uint16 level);
   void ReportRefinedMethod2005(rptChapter* pChapter,IBroker* pBroker,BeamType beamType,SpanIndexType span,GirderIndexType gdr,IDisplayUnits* pDispUnit,Uint16 level);
   void ReportApproxMethod(rptChapter* pChapter,IBroker* pBroker,CPsLossEngineer::BeamType beamType,SpanIndexType span,GirderIndexType gdr,IDisplayUnits* pDispUnit,Uint16 level,bool isWsdot);
   void ReportApproxMethod2005(rptChapter* pChapter,IBroker* pBroker,CPsLossEngineer::BeamType beamType,SpanIndexType span,GirderIndexType gdr,IDisplayUnits* pDispUnit,Uint16 level);

   typedef std::set< std::pair<pgsPointOfInterest,pgsTypes::Stage> > PoiSet;
   typedef std::vector<pgsPointOfInterest> PoiVector;

   void ReportLocation(rptRcTable* pTable,int row,const pgsPointOfInterest& poi,pgsTypes::Stage stage,Float64 endsize,IDisplayUnits* pDispUnit);
   void ReportLocation(rptRcTable* pTable,int row,const pgsPointOfInterest& poi,Float64 endsize,IDisplayUnits* pDispUnit);

   void ReportInitialRelaxation(rptChapter* pChapter,IBroker* pBroker,bool bTemporaryStrands,const lrfdLosses* pLosses,IDisplayUnits* pDispUnit,Uint16 level);
   void ReportLumpSumTimeDependentLossesAtShipping(rptChapter* pChapter,IBroker* pBroker,const LOSSDETAILS& details,IDisplayUnits* pDispUnit,Uint16 level);
   void ReportLumpSumTimeDependentLosses(rptChapter* pChapter,IBroker* pBroker,const LOSSDETAILS& details,IDisplayUnits* pDispUnit,Uint16 level);

//   void ReportTimeDependentLossesAtShipping(rptChapter* pChapter,IBroker* pBroker,SpanIndexType span,GirderIndexType gdr,PoiSet::iterator begin,PoiSet::iterator end,IDisplayUnits* pDispUnit,Uint16 level);
//   void ReportEffectOfTemporaryStrandRemoval(rptChapter* pChapter,IBroker* pBroker,SpanIndexType span,GirderIndexType gdr,PoiVector::iterator begin,PoiVector::iterator end,IDisplayUnits* pDispUnit,Uint16 level);
//   void ReportElasticGainDueToDeckPlacement(rptChapter* pChapter,IBroker* pBroker,SpanIndexType span,GirderIndexType gdr,PoiVector::iterator begin,PoiVector::iterator end,IDisplayUnits* pDispUnit,Uint16 level);
   void ReportTotalPrestressLoss(rptChapter* pChapter,IBroker* pBroker,SpanIndexType span,GirderIndexType gdr,PoiVector::iterator begin,PoiVector::iterator end,IDisplayUnits* pDispUnit,Uint16 level);

   void GetLossParameters(IBroker* pBroker,const pgsPointOfInterest& poi,const GDRCONFIG& config,
                           matPsStrand::Grade* pGrade,
                           matPsStrand::Type* pType,
                           Float64* pFpjPerm,
                           Float64* pFpjTTS,
                           Float64* pPerimeter,
                           Float64* pAg,
                           Float64* pIg,
                           Float64* pYbg,
                           Float64* pAc,
                           Float64* pIc,
                           Float64* pYbc,
                           Float64* pAd,
                           Float64* ped,
                           Float64* peperm,// eccentricity of the permanent strands on the non-composite section
                           Float64* petemp,
                           Float64* paps,  // area of one prestress strand
                           Float64* pApsPerm,
                           Float64* pApsTTS,
                           Float64* pMdlg,
                           Float64* pMadlg,
                           Float64* pMsidl,
                           Float64* prh,
                           Float64* pti,
                           Float64* pth,
                           Float64* ptd,
                           Float64* ptf,
                           Float64* pPjS,
                           Float64* pPjH,
                           Float64* pPjT,
                           StrandIndexType* pNs,
                           StrandIndexType* pNh,
                           StrandIndexType* pNt,
                           Float64* pFci,
                           Float64* pFc,
                           Float64* pFcSlab,
                           Float64* pEci,
                           Float64* pEc,
                           Float64* pEcSlab,
                           Float64* pGirderLength,
                           Float64* pSpanLength,
                           Float64* pEndSize,
                           Float64* pAslab,
                           Float64* pPslab,
                           lrfdLosses::TempStrandUsage* pUsage,
                           Float64* pAnchorSet,
                           Float64* pWobble,
                           Float64* pCoeffFriction,
                           Float64* pAngleChange
                           );

   bool m_bComputingLossesForDesign; 
};

#endif //__PSLOSSENGINEER_H_
