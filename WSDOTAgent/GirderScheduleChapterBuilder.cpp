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
#include "GirderScheduleChapterBuilder.h"
#include <PGSuperTypes.h>

#include <Reporting\SpanGirderReportSpecification.h>
#include <PgsExt\PointOfInterest.h>
#include <PgsExt\BridgeDescription2.h>
#include <EAF\EAFDisplayUnits.h>
#include <IFace\AnalysisResults.h>
#include <IFace\Bridge.h>
#include <IFace\Intervals.h>
#include <IFace\Artifact.h>
#include <IFace\Project.h>
#include <IFace\BeamFactory.h>
#include <IFace\GirderHandling.h>
#include <PgsExt\GirderArtifact.h>
#include <psgLib\SpecLibraryEntry.h>

#include <psgLib\ConnectionLibraryEntry.h>

#include <WBFLCogo.h>

#include <Plugins\BeamFamilyCLSID.h>

#if defined _DEBUG
#include <IFace\DocumentType.h>
#endif

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define STIRRUP_ERROR_NONE        0
#define STIRRUP_ERROR            -1
#define STIRRUP_ERROR_BARSIZE    -2
#define STIRRUP_ERROR_ZONES      -3
#define STIRRUP_ERROR_SYMMETRIC  -4
#define STIRRUP_ERROR_STARTZONE  -5
#define STIRRUP_ERROR_LASTZONE   -6
#define STIRRUP_ERROR_V6         -7

#define DEBOND_ERROR_NONE        0
#define DEBOND_ERROR_SYMMETRIC   -1

/****************************************************************************
CLASS
   CGirderScheduleChapterBuilder
****************************************************************************/


////////////////////////// PUBLIC     ///////////////////////////////////////

//======================== LIFECYCLE  =======================================
CGirderScheduleChapterBuilder::CGirderScheduleChapterBuilder(bool bSelect) :
CPGSuperChapterBuilder(bSelect)
{
}

//======================== OPERATORS  =======================================
//======================== OPERATIONS =======================================
LPCTSTR CGirderScheduleChapterBuilder::GetName() const
{
   return TEXT("Girder Schedule");
}

rptChapter* CGirderScheduleChapterBuilder::Build(CReportSpecification* pRptSpec,Uint16 level) const
{
   CGirderReportSpecification* pGdrRptSpec = dynamic_cast<CGirderReportSpecification*>(pRptSpec);
   CComPtr<IBroker> pBroker;
   pGdrRptSpec->GetBroker(&pBroker);
   const CGirderKey& girderKey( pGdrRptSpec->GetGirderKey() );

   rptChapter* pChapter = CPGSuperChapterBuilder::Build(pRptSpec,level);

#if defined _DEBUG
   GET_IFACE2(pBroker,IDocumentType,pDocType);
   ATLASSERT(pDocType->IsPGSuperDocument());
   // This chapter builder assumes a precast girder bridge
   // it must be updated for use with PGSplice.
#endif
   SpanIndexType span = girderKey.groupIndex;
   GirderIndexType girder = girderKey.girderIndex;

   GET_IFACE2(pBroker,IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CGirderGroupData* pGroup = pBridgeDesc->GetGirderGroup(girderKey.groupIndex);
   const CSplicedGirderData* pGirder = pGroup->GetGirder(girderKey.girderIndex);
   const CPrecastSegmentData* pSegment = pGirder->GetSegment(0);

   CSegmentKey segmentKey(pSegment->GetSegmentKey());

   GET_IFACE2(pBroker,ISectionProperties,pSectProp);
   GET_IFACE2(pBroker,IEAFDisplayUnits,pDisplayUnits);
   GET_IFACE2(pBroker,IIntervals,pIntervals);
   IntervalIndexType releaseIntervalIdx = pIntervals->GetPrestressReleaseInterval(segmentKey);
   IntervalIndexType finalIntervalIdx   = pIntervals->GetIntervalCount()-1;

   const GirderLibraryEntry* pGdrLibEntry = pGirder->GetGirderLibraryEntry();
   CComPtr<IBeamFactory> factory;
   pGdrLibEntry->GetBeamFactory(&factory);
   CLSID familyCLSID = factory->GetFamilyCLSID();
   if ( CLSID_WFBeamFamily != familyCLSID && CLSID_UBeamFamily != familyCLSID && CLSID_DeckBulbTeeBeamFamily != familyCLSID && CLSID_SlabBeamFamily != familyCLSID )
   {
      rptParagraph* pPara = new rptParagraph;
      *pPara << _T("WSDOT girder schedules can only be created for WF, U, Slab, and Deck Bulb Tee sections") << rptNewLine;
      *pChapter << pPara;
      return pChapter;
   }

   if ( !factory->IsSymmetric(segmentKey) )
   {
      rptParagraph* pPara = new rptParagraph;
      *pPara << _T("WSDOT girder schedules can only be created for constant depth sections.") << rptNewLine;
      *pChapter << pPara;
      return pChapter;
   }

   GET_IFACE2( pBroker, IStrandGeometry, pStrandGeometry );
   StrandIndexType Nh = pStrandGeometry->GetStrandCount(segmentKey,pgsTypes::Harped);
   if ( CLSID_SlabBeamFamily == familyCLSID && 0 < Nh )
   {
      rptParagraph* pPara = new rptParagraph;
      *pPara << _T("Cannot create girder schedule. WSDOT Slab Beams do not use harped strands and this girder has harped strands") << rptNewLine;
      *pChapter << pPara;
      return pChapter;
   }


   GET_IFACE2(pBroker,IArtifact,pIArtifact);
   const pgsGirderArtifact* pArtifact = pIArtifact->GetGirderArtifact(girderKey);
   const pgsSegmentArtifact* pSegmentArtifact = pIArtifact->GetSegmentArtifact(segmentKey);

   bool bCanReportPrestressInformation = true;

   // WsDOT reports don't support Straight-Web strand option (except for slab beams)
   if (pStrandGeometry->GetAreHarpedStrandsForcedStraight(segmentKey) && CLSID_SlabBeamFamily != familyCLSID)
   {
      bCanReportPrestressInformation = false;
   }

   if (pSegment->Strands.GetStrandDefinitionType() == CStrandData::sdtDirectSelection)
   {
      bCanReportPrestressInformation = false;
   }

   if( pArtifact->Passed() )
   {
      rptParagraph* pPara = new rptParagraph;
      *pChapter << pPara;
      *pPara << color(Green) << _T("The Specification Check was Successful") << color(Black) << rptNewLine;
   }
   else
   {
      rptParagraph* pPara = new rptParagraph;
      *pChapter << pPara;
      *pPara << color(Red) << _T("The Specification Check Was Not Successful") << color(Black) << rptNewLine;
   }

   INIT_UV_PROTOTYPE( rptStressUnitValue, stress,         pDisplayUnits->GetStressUnit(),        true );
   INIT_UV_PROTOTYPE( rptAngleUnitValue,  angle,          pDisplayUnits->GetAngleUnit(),         true );
   INIT_UV_PROTOTYPE( rptMomentPerAngleUnitValue, spring,   pDisplayUnits->GetMomentPerAngleUnit(), true );

   INIT_FRACTIONAL_LENGTH_PROTOTYPE( gdim,      IS_US_UNITS(pDisplayUnits), 8, pDisplayUnits->GetComponentDimUnit(), true, true );
   INIT_FRACTIONAL_LENGTH_PROTOTYPE( glength,   IS_US_UNITS(pDisplayUnits), 8, pDisplayUnits->GetSpanLengthUnit(),   true, true );

   rptParagraph* p = new rptParagraph;
   *pChapter << p;

   GET_IFACE2(pBroker,ICamber,pCamber);

   // create pois at the start of girder and mid-span
   pgsPointOfInterest poiStart(segmentKey,0.0);

   GET_IFACE2( pBroker, ILibrary, pLib );
   GET_IFACE2( pBroker, ISpecification, pSpec );
   std::_tstring spec_name = pSpec->GetSpecification();
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( spec_name.c_str() );
   Float64 min_days =  ::ConvertFromSysUnits(pSpecEntry->GetCreepDuration2Min(), unitMeasure::Day);
   Float64 max_days =  ::ConvertFromSysUnits(pSpecEntry->GetCreepDuration2Max(), unitMeasure::Day);

   GET_IFACE2(pBroker, IPointOfInterest, pPointOfInterest );
   PoiList pmid;
   pPointOfInterest->GetPointsOfInterest(segmentKey, POI_5L | POI_ERECTED_SEGMENT, &pmid);
   ATLASSERT(pmid.size()==1);
   const pgsPointOfInterest& poiMidSpan(pmid.front());

   GET_IFACE2(pBroker,IBridge,pBridge);
   GET_IFACE2(pBroker,IGirder,pIGirder);

   rptRcTable* pTable = rptStyleManager::CreateTableNoHeading(2);
   *p << pTable;

   RowIndexType row = 0;
   (*pTable)(row,0) << _T("Span");
   (*pTable)(row,1) << LABEL_SPAN(span);

   (*pTable)(++row,0) << _T("Girder");
   (*pTable)(row  ,1) << LABEL_GIRDER(girder);

   if ( familyCLSID == CLSID_SlabBeamFamily )
   {
      Float64 Hg = pSectProp->GetHg(releaseIntervalIdx,poiMidSpan);
      (*pTable)(++row,0) << _T("Girder Height, H");
      (*pTable)(row,1) << gdim.SetValue(Hg);

      GET_IFACE2(pBroker, IGirder, pGdr);
      Float64 W = pGdr->GetTopWidth(poiMidSpan);
      (*pTable)(++row, 0) << _T("Girder Width, W");
      (*pTable)(row, 1) << gdim.SetValue(W);
   }
   else
   {
      (*pTable)(++row,0) << _T("Girder Series");
      (*pTable)(row,  1) << pGirder->GetGirderName();
   }

   if (familyCLSID == CLSID_DeckBulbTeeBeamFamily)
   {
      GET_IFACE2(pBroker, IGirder, pIGirder);
      Float64 W = pIGirder->GetTopWidth(poiMidSpan);
      (*pTable)(++row, 0) << _T("W");
      (*pTable)(row, 1) << glength.SetValue(W);
   }

   (*pTable)(++row,0) << _T("Plan Length (Along Girder Grade)");
   (*pTable)(row  ,1) << glength.SetValue(pBridge->GetSegmentPlanLength(segmentKey));

   if ( familyCLSID == CLSID_WFBeamFamily )
   {
      // only applies to WF sections
      (*pTable)(++row,0) << _T("Intermediate Diaphragm Type");
      (*pTable)(row,  1) << _T("**");
   }

   (*pTable)(++row,0) << _T("End 1 Type");
   (*pTable)(row  ,1) << _T("*");

   (*pTable)(++row,0) << _T("End 2 Type");
   (*pTable)(row  ,1) << _T("*");

   (*pTable)(++row,0) << Sub2(_T("L"),_T("d"));
   (*pTable)(row,  1) << _T("***");

   CComPtr<IAngle> objAngle1, objAngle2;
   pBridge->GetSegmentSkewAngle(segmentKey,pgsTypes::metStart,&objAngle1);
   pBridge->GetSegmentSkewAngle(segmentKey,pgsTypes::metEnd,  &objAngle2);
   Float64 t1,t2;
   objAngle1->get_Value(&t1);
   objAngle2->get_Value(&t2);

   (*pTable)(++row,0) << Sub2(symbol(theta),_T("1"));
   (*pTable)(row  ,1) << angle.SetValue(t1);

   (*pTable)(++row,0) << Sub2(symbol(theta),_T("2"));
   (*pTable)(row  ,1) << angle.SetValue(t2);

   if (familyCLSID != CLSID_SlabBeamFamily)
   {
      // P1 & P2...
      // does not apply to slabs
      PierIndexType prevPierIdx = (PierIndexType)span;
      PierIndexType nextPierIdx = prevPierIdx+1;
      bool bContinuousLeft, bContinuousRight, bIntegralLeft, bIntegralRight;
	
      pBridge->IsContinuousAtPier(prevPierIdx,&bContinuousLeft,&bContinuousRight);
      pBridge->IsIntegralAtPier(prevPierIdx,&bIntegralLeft,&bIntegralRight);

      (*pTable)(++row,0) << Sub2(_T("P"),_T("1"));
      if ( bContinuousLeft || bIntegralLeft )
      {
	      (*pTable)(row,1) << _T("-");
      }
      else
      {
	      Float64 P1 = pBridge->GetSegmentStartEndDistance(segmentKey);
	      (*pTable)(row  ,1) << gdim.SetValue(P1);
      }
	
      pBridge->IsContinuousAtPier(nextPierIdx,&bContinuousLeft,&bContinuousRight);
      pBridge->IsIntegralAtPier(nextPierIdx,&bIntegralLeft,&bIntegralRight);
      (*pTable)(++row,0) << Sub2(_T("P"),_T("2"));
      if ( bContinuousRight || bIntegralRight )
      {
	      (*pTable)(row,1) << _T("-");
      }
      else
      {
	      Float64 P2 = pBridge->GetSegmentEndEndDistance(segmentKey);
	      (*pTable)(row  ,1) << gdim.SetValue(P2);
      }
   }

   GET_IFACE2(pBroker, IMaterials, pMaterial);
   (*pTable)(++row,0) << RPT_FC << _T(" (at 28 days)");
   (*pTable)(row  ,1) << stress.SetValue(pMaterial->GetSegmentDesignFc(segmentKey,finalIntervalIdx));

   (*pTable)(++row,0) << RPT_FCI << _T(" (at Release)");
   (*pTable)(row  ,1) << stress.SetValue(pMaterial->GetSegmentDesignFc(segmentKey,releaseIntervalIdx));

   
   StrandIndexType Ns = pStrandGeometry->GetStrandCount(segmentKey,pgsTypes::Straight);
   if ( bCanReportPrestressInformation )
   {
      (*pTable)(++row,0) << _T("Number of Straight Strands");
      (*pTable)(row  ,1) << Ns;

      if ( CLSID_SlabBeamFamily != familyCLSID )
      {
         StrandIndexType nDebonded = pStrandGeometry->GetNumDebondedStrands(segmentKey,pgsTypes::Straight,pgsTypes::dbetEither);
         if ( nDebonded != 0 )
         {
            (*pTable)(row,1) << _T(" (") << nDebonded << _T(" debonded)");
         }

         (*pTable)(++row,0) << _T("Number of Harped Strands");
         (*pTable)(row  ,1) << Nh;
      }

      if ( 0 < pStrandGeometry->GetMaxStrands(segmentKey,pgsTypes::Temporary ) )
      {
         StrandIndexType Nt = pStrandGeometry->GetStrandCount(segmentKey,pgsTypes::Temporary);
         (*pTable)(++row,0) << _T("Number of Temporary Strands");

         switch ( pSegment->Strands.GetTemporaryStrandUsage() )
         {
         case pgsTypes::ttsPTAfterLifting:
            (*pTable)(row,0) << rptNewLine << _T("Temporary strands post-tensioned immediately after lifting");
            break;

         case pgsTypes::ttsPTBeforeShipping:
            (*pTable)(row,0) << rptNewLine << _T("Temporary strands post-tensioned immediately before shipping");
            break;
         }

         (*pTable)(row  ,1) << Nt;
      }
   }
   else
   {
      (*pTable)(++row,0) << _T("Number of Straight Strands");
      (*pTable)(row  ,1) << _T("#");

      if ( CLSID_SlabBeamFamily != familyCLSID )
      {
         (*pTable)(++row,0) << _T("Number of Harped Strands");
         (*pTable)(row  ,1) << _T("#");
      }

      (*pTable)(++row,0) << _T("Number of Temporary Strands");
      (*pTable)(row  ,1) << _T("#");
   }

   Float64 ybg = pSectProp->GetY(releaseIntervalIdx,poiMidSpan,pgsTypes::BottomGirder);
   Float64 nEff;
   Float64 sse = pStrandGeometry->GetEccentricity(releaseIntervalIdx,poiMidSpan,pgsTypes::Straight, &nEff);
   (*pTable)(++row,0) << _T("E");
   if (0 < Ns)
   {
      (*pTable)(row,1) << gdim.SetValue(ybg-sse);
   }
   else
   {
      (*pTable)(row,1) << RPT_NA;
   }

   if ( CLSID_SlabBeamFamily != familyCLSID )
   {
      Float64 hse = pStrandGeometry->GetEccentricity(releaseIntervalIdx,poiMidSpan,pgsTypes::Harped, &nEff);
      (*pTable)(++row,0) << Sub2(_T("F"),_T("C.L."));
      if (0 < Nh)
      {
         (*pTable)(row,1) << gdim.SetValue(ybg-hse);
      }
      else
      {
         (*pTable)(row,1) << RPT_NA;
      }


      Float64 ytg = pSectProp->GetY(releaseIntervalIdx,poiStart,pgsTypes::TopGirder);
      Float64 hss = pStrandGeometry->GetEccentricity(releaseIntervalIdx,poiStart,pgsTypes::Harped,&nEff);
      (*pTable)(++row,0) << Sub2(_T("F"),_T("o"));
      if (0 < Nh)
      {
         (*pTable)(row,1) << gdim.SetValue(ytg+hss);
      }
      else
      {
         (*pTable)(row,1) << RPT_NA;
      }
   }

   // Strand Extensions
   (*pTable)(++row,0) << _T("Straight Strands to Extend, End 1");
   StrandIndexType nExtended = 0;
   for ( StrandIndexType strandIdx = 0; strandIdx < Ns; strandIdx++ )
   {
      if ( pStrandGeometry->IsExtendedStrand(segmentKey,pgsTypes::metStart,strandIdx,pgsTypes::Straight) )
      {
         nExtended++;
         (*pTable)(row,1) << _T(" ") << (strandIdx+1);
      }
   }
   if ( nExtended == 0 )
   {
      (*pTable)(row,1) << _T("");
   }

   (*pTable)(++row,0) << _T("Straight Strands to Extend, End 2");
   nExtended = 0;
   for ( StrandIndexType strandIdx = 0; strandIdx < Ns; strandIdx++ )
   {
      if ( pStrandGeometry->IsExtendedStrand(segmentKey,pgsTypes::metEnd,strandIdx,pgsTypes::Straight) )
      {
         nExtended++;
         (*pTable)(row,1) << _T(" ") << (strandIdx+1);
      }
   }
   if ( nExtended == 0 )
   {
      (*pTable)(row,1) << _T("");
   }

   int debondResults = DEBOND_ERROR_NONE;
   if ( CLSID_SlabBeamFamily == familyCLSID )
   {
      // Debonding for slab beams
      pTable->SetColumnSpan(++row,0,2);
      (*pTable)(row,0) << _T("Straight Strands to Debond");

      std::vector<DebondInformation> debondInfo;
      debondResults = GetDebondDetails(pBroker,segmentKey,debondInfo);

      if ( debondResults < 0 )
      {
         for ( int i = 0; i < 3; i++ )
         {
            pTable->SetColumnSpan(++row,0,2);
            (*pTable)(row,0) << _T("Group ") << (i+1);

            (*pTable)(++row,0) << _T("Strands to Debond");
            (*pTable)(row,1) << _T("%");
            (*pTable)(++row,0) << _T("Sleeved Length at Ends to Prevent Bond");
            (*pTable)(row,1) << _T("%");
         }
      }
      else
      {
         int groupCount = 0;
         std::vector<DebondInformation>::iterator iter(debondInfo.begin());
         std::vector<DebondInformation>::iterator end(debondInfo.end());
         for ( ; iter != end; iter++, groupCount++ )
         {
            DebondInformation& dbInfo = *iter;

            pTable->SetColumnSpan(++row,0,2);
            (*pTable)(row,0) << _T("Group ") << (groupCount+1);

            (*pTable)(++row,0) << _T("Strands to Debond");
            std::vector<StrandIndexType>::iterator strandIter(dbInfo.Strands.begin());
            std::vector<StrandIndexType>::iterator strandIterEnd(dbInfo.Strands.end());
            for ( ; strandIter != strandIterEnd; strandIter++ )
            {
               StrandIndexType strandIdx = *strandIter;
               (*pTable)(row,1) << _T(" ") << (strandIdx+1);
            }

            (*pTable)(++row,0) << _T("Sleeved Length at Ends to Prevent Bond");
            (*pTable)(row,1) << glength.SetValue(dbInfo.Length);
         }

         for ( int i = groupCount; i < 3; i++ )
         {
            pTable->SetColumnSpan(++row,0,2);
            (*pTable)(row,0) << _T("Group ") << (i+1);

            (*pTable)(++row,0) << _T("Strands to Debond");
            (*pTable)(row,1) << _T("-");
            (*pTable)(++row,0) << _T("Sleeved Length at Ends to Prevent Bond");
            (*pTable)(row,1) << _T("-");
         }
      }
   }

   pgsTypes::SupportedDeckType deckType = pBridgeDesc->GetDeckDescription()->GetDeckType();
   Float64 C;
   if (IsNonstructuralDeck(deckType))
   {
      C = pCamber->GetExcessCamber(poiMidSpan, CREEP_MAXTIME);
   }
   else
   {
      if (pBridgeDesc->GetSlabOffsetType() == pgsTypes::sotBridge)
      {
         (*pTable)(++row, 0) << _T("\"A\" Dimension at CL Bearings");
         (*pTable)(row, 1) << gdim.SetValue(pBridgeDesc->GetSlabOffset());
      }
      else
      {
         (*pTable)(++row, 0) << _T("\"A\" Dimension at CL Bearing End 1");
         (*pTable)(row, 1) << gdim.SetValue(pSegment->GetSlabOffset(pgsTypes::metStart));

         (*pTable)(++row, 0) << _T("\"A\" Dimension at CL Bearing End 2");
         (*pTable)(row, 1) << gdim.SetValue(pSegment->GetSlabOffset(pgsTypes::metEnd));
      }

      C = pCamber->GetScreedCamber(poiMidSpan, CREEP_MAXTIME);
      (*pTable)(++row, 0) << _T("Screed Camber, C");
      (*pTable)(row, 1) << gdim.SetValue(C);
   }

   // get # of days for creep
   Float64 Dmax_UpperBound, Dmax_Average, Dmax_LowerBound;
   Float64 Dmin_UpperBound, Dmin_Average, Dmin_LowerBound;
   pCamber->GetDCamberForGirderScheduleEx(poiMidSpan, CREEP_MAXTIME, &Dmax_UpperBound, &Dmax_Average, &Dmax_LowerBound);
   pCamber->GetDCamberForGirderScheduleEx(poiMidSpan, CREEP_MINTIME, &Dmin_UpperBound, &Dmin_Average, &Dmin_LowerBound);


   (*pTable)(++row,0) << _T("Lower bound @ ")<< min_days<<_T(" days");
   if ( Dmin_LowerBound < 0 )
   {
      (*pTable)(row,1) << color(Red) << gdim.SetValue(Dmin_LowerBound) << color(Black);
   }
   else
   {
      (*pTable)(row,1) << gdim.SetValue(Dmin_LowerBound);
   }

   (*pTable)(++row,0) << _T("Upper bound @ ")<< max_days<<_T(" days");
   if ( Dmax_UpperBound < 0 )
   {
      (*pTable)(row,1) << color(Red) << gdim.SetValue(Dmax_UpperBound) << color(Black);
   }
   else
   {
      (*pTable)(row,1) << gdim.SetValue(Dmax_UpperBound);
   }

   // Stirrups
   Float64 z1Spacing, z1Length;
   Float64 z2Spacing, z2Length;
   Float64 z3Spacing, z3Length;
   int reinfDetailsResult = GetReinforcementDetails(pBroker,segmentKey,familyCLSID,&z1Spacing,&z1Length,&z2Spacing,&z2Length,&z3Spacing,&z3Length);
   if (reinfDetailsResult < 0)
   {
      (*pTable)(++row,0) << _T("Zone 1 Spacing");
      (*pTable)(row,1) << _T("###");
      (*pTable)(++row,0) << _T("Zone 1 Length");
      (*pTable)(row,1) << _T("###");
      (*pTable)(++row,0) << _T("Zone 2 Spacing");
      (*pTable)(row,1) << _T("###");
      (*pTable)(++row,0) << _T("Zone 2 Length");
      (*pTable)(row,1) << _T("###");
      (*pTable)(++row,0) << _T("Zone 3 Spacing");
      (*pTable)(row,1) << _T("###");
      (*pTable)(++row,0) << _T("Zone 3 Length");
      (*pTable)(row,1) << _T("###");
   }
   else
   {
      (*pTable)(++row,0) << _T("Zone 1 Spacing");
      (*pTable)(row,1) << gdim.SetValue(z1Spacing);
      (*pTable)(++row,0) << _T("Zone 1 Length");
      (*pTable)(row,1) << glength.SetValue(z1Length);
      (*pTable)(++row,0) << _T("Zone 2 Spacing");
      (*pTable)(row,1) << gdim.SetValue(z2Spacing);
      (*pTable)(++row,0) << _T("Zone 2 Length");
      (*pTable)(row,1) << glength.SetValue(z2Length);
      (*pTable)(++row,0) << _T("Zone 3 Spacing");
      (*pTable)(row,1) << gdim.SetValue(z3Spacing);
      (*pTable)(++row,0) << _T("Zone 3 Length");
      (*pTable)(row,1) << glength.SetValue(z3Length);
   }

   // Stirrup Height

   if ( familyCLSID == CLSID_WFBeamFamily || familyCLSID == CLSID_UBeamFamily )
   {
      // H1 (Hg + "A" + 3")
      if ( pBridgeDesc->GetSlabOffsetType() == pgsTypes::sotBridge )
      {
         (*pTable)(++row,0) << _T("H1 (##)");
         Float64 Hg = pSectProp->GetHg(releaseIntervalIdx,poiStart);
         Float64 H1 = pBridgeDesc->GetSlabOffset() + Hg + ::ConvertToSysUnits(3.0,unitMeasure::Inch);
         (*pTable)(row,  1) << gdim.SetValue(H1);
      }
      else
      {
         (*pTable)(++row,0) << _T("H1 at End 1 (##)");
         Float64 Hg = pSectProp->GetHg(releaseIntervalIdx,poiStart);
         Float64 H1 = pSegment->GetSlabOffset(pgsTypes::metStart) + Hg + ::ConvertToSysUnits(3.0,unitMeasure::Inch);
         (*pTable)(row,  1) << gdim.SetValue(H1);

         (*pTable)(++row,0) << _T("H1 at End 2 (##)");
         pgsPointOfInterest poiEnd(poiStart);
         poiEnd.SetDistFromStart(pBridge->GetSegmentLength(segmentKey));
         Hg = pSectProp->GetHg(releaseIntervalIdx,poiEnd);
         H1 = pSegment->GetSlabOffset(pgsTypes::metEnd) + Hg + ::ConvertToSysUnits(3.0,unitMeasure::Inch);
         (*pTable)(row,  1) << gdim.SetValue(H1);
      }
   }

   const pgsHaulingAnalysisArtifact* pHaulingArtifact = pSegmentArtifact->GetHaulingAnalysisArtifact();
   if ( pHaulingArtifact != nullptr )
   {
      const stbHaulingStabilityProblem* pHaulProblem = pIGirder->GetSegmentHaulingStabilityProblem(segmentKey);
      bool bDirectCamber;
      Float64 camber;
      pHaulProblem->GetCamber(&bDirectCamber,&camber);
      (*pTable)(++row,0) << _T("Maximum midspan vertical deflection, shipping");
      if ( bDirectCamber )
      {
         (*pTable)(row,  1) << gdim.SetValue(camber);
      }
      else
      {
         (*pTable)(row,  1) << _T("");
      }
   }

   const stbLiftingCheckArtifact* pLiftArtifact = pSegmentArtifact->GetLiftingCheckArtifact();
   if (pLiftArtifact!=nullptr)
   {
      GET_IFACE2(pBroker,ISegmentLifting,pSegmentLifting);
      Float64 L = pSegmentLifting->GetLeftLiftingLoopLocation(segmentKey);
      (*pTable)(++row,0) << _T("Location of Lifting Loops, L");
      (*pTable)(row  ,1) << glength.SetValue(L);
   }

   if ( pHaulingArtifact != nullptr )
   {
      GET_IFACE2(pBroker,IIntervals,pIntervals);
      IntervalIndexType releaseIntervalIdx = pIntervals->GetPrestressReleaseInterval(segmentKey);
      IntervalIndexType storageIntervalIdx = pIntervals->GetStorageInterval(segmentKey);

      GET_IFACE2(pBroker,IProductForces,pProduct);
      pgsTypes::BridgeAnalysisType bat = pProduct->GetBridgeAnalysisType(pgsTypes::Minimize);

      GET_IFACE2(pBroker,ISegmentHauling, pSegmentHauling);
      Float64 trailingOverhang = pSegmentHauling->GetTrailingOverhang(segmentKey);
      Float64 leadingOverhang = pSegmentHauling->GetLeadingOverhang(segmentKey);

      (*pTable)(++row,0) << _T("Location of Lead Shipping Support, ") << Sub2(_T("L"),_T("1"));
      (*pTable)(row  ,1) << glength.SetValue(leadingOverhang);
      
      (*pTable)(++row,0) << _T("Location of Trailing Shipping Support, ") << Sub2(_T("L"),_T("2"));
      (*pTable)(row  ,1) << glength.SetValue(trailingOverhang);

      (*pTable)(++row,0) << _T("Minimum shipping support rotational spring constant, ") << Sub2(_T("K"),symbol(theta));
      if ( pSegment->HandlingData.pHaulTruckLibraryEntry )
      {
         (*pTable)(row,  1) << spring.SetValue(pSegment->HandlingData.pHaulTruckLibraryEntry->GetRollStiffness());
      }
      else
      {
         (*pTable)(row,  1) << _T("++");
      }

      (*pTable)(++row,0) << _T("Minimum shipping support cntr-to-cntr wheel spacing, ") << Sub2(_T("W"),_T("cc"));
      if ( pSegment->HandlingData.pHaulTruckLibraryEntry )
      {
         (*pTable)(row,  1) << gdim.SetValue(pSegment->HandlingData.pHaulTruckLibraryEntry->GetAxleWidth());
      }
      else
      {
         (*pTable)(row,  1) << _T("++");
      }
   }


   if ( pSpecEntry->CheckGirderSag() )
   {
      if (IsNonstructuralDeck(deckType))
      {
         if (C < 0)
         {
            rptParagraph* p = new rptParagraph;
            *pChapter << p;
            *p << color(Red) << _T("WARNING: Final camber (") << Sub2(_T("C"), _T("F")) << _T(") is downward. The girder may end up with a sag.") << color(Black) << rptNewLine;
         }
      }
      else
      {
         std::_tstring camberType;
         Float64 D = 0;

         switch (pSpecEntry->GetSagCamberType())
         {
         case pgsTypes::LowerBoundCamber:
            D = Dmin_LowerBound;
            camberType = _T("lower bound");
            break;
         case pgsTypes::AverageCamber:
            D = Dmin_Average;
            camberType = _T("average");
            break;
         case pgsTypes::UpperBoundCamber:
            D = Dmin_UpperBound;
            camberType = _T("upper bound");
            break;
         }

         if (D < C)
         {
            rptParagraph* p = new rptParagraph;
            *pChapter << p;

            *p << color(Red) << _T("WARNING: Screed camber (C) is greater than the ") << camberType.c_str() << _T(" camber at time of deck casting, D. The girder may end up with a sag.") << color(Black) << rptNewLine;
         }
         else if (IsEqual(C, D, ::ConvertToSysUnits(0.25, unitMeasure::Inch)))
         {
            rptParagraph* p = new rptParagraph;
            *pChapter << p;

            *p << color(Red) << _T("WARNING: Screed camber (C) is nearly equal to the ") << camberType.c_str() << _T(" camber at time of deck casting, D. The girder may end up with a sag.") << color(Black) << rptNewLine;
         }

         if (Dmin_LowerBound < C && pSpecEntry->GetSagCamberType() != pgsTypes::LowerBoundCamber)
         {
            rptParagraph* p = new rptParagraph;
            *pChapter << p;

            Float64 Cfactor = pCamber->GetLowerBoundCamberVariabilityFactor();
            *p << _T("Screed camber (C) is greater than the lower bound camber at time of deck casting (") << Cfactor * 100 << _T("% of D") << Sub(min_days) << _T("). The girder may end up with a sag if the deck is placed at day ") << min_days << _T(" and the actual camber is a lower bound value.") << rptNewLine;
         }
      }
   }

   p = new rptParagraph(rptStyleManager::GetFootnoteStyle());
   *pChapter << p;
   *p << _T("* End Types to be determined by the Designer.") << rptNewLine;
   *p << _T("** Intermediate Diaphragm Type to be determined by the Designer.") << rptNewLine;
   *p << _T("*** ") << Sub2(_T("L"),_T("d")) << _T(" to be determined by the Designer.") << rptNewLine;

   if ( debondResults == DEBOND_ERROR_SYMMETRIC )
   {
      *p << _T("% Debonding must be symmetric for this girder schedule.") << rptNewLine;
   }

   if ( !bCanReportPrestressInformation )
   {
      *p << _T("# Prestressing information could not be included in the girder schedule because strand input is not consistent with the standard WSDOT details.") << rptNewLine;
   }

   if ( familyCLSID == CLSID_WFBeamFamily || familyCLSID == CLSID_UBeamFamily )
   {
      *p << _T("## H1 is computed as the height of the girder H + 3\" + \"A\" Dimension. Designers shall check H1 for the effect of vertical curve and increase as necessary.") << rptNewLine;
   }

   if (reinfDetailsResult == STIRRUP_ERROR_ZONES)
   {
      *p << _T("### Reinforcement details could not be listed because the number of transverse reinforcement zones is not consistent with the girder schedule.") << rptNewLine;
   }
   else if ( reinfDetailsResult == STIRRUP_ERROR_SYMMETRIC )
   {
      *p << _T("### Reinforcement details could not be listed because the girder schedule is for symmetric transverse reinforcement layouts.") << rptNewLine;
   }
   else if ( reinfDetailsResult == STIRRUP_ERROR_STARTZONE )
   {
      *p << _T("### Reinforcement details could not be listed because first zone of transverse reinforcement is not consistent with standard WSDOT details.") << rptNewLine;
   }
   else if ( reinfDetailsResult == STIRRUP_ERROR_LASTZONE )
   {
      if ( familyCLSID == CLSID_SlabBeamFamily )
      {
         *p << _T("### Reinforcement details could not be listed because spacing in the last zone is not equal to the smaller of H-3\" or 1'-6\" per the standard WSDOT details.") << rptNewLine;
      }
      else
      {
         *p << _T("### Reinforcement details could not be listed because spacing in the last zone is not 1'-6\" per the standard WSDOT details.") << rptNewLine;
      }
   }
   else if ( reinfDetailsResult == STIRRUP_ERROR_BARSIZE )
   {
      *p << _T("### Reinforcement details could not be listed because the bar size is not #5 in one or more zones.") << rptNewLine;
   }
   else if ( reinfDetailsResult == STIRRUP_ERROR_V6 )
   {
      *p << _T("### Reinforcement details could not be listed because two different values are needed for V6.") << rptNewLine;
   }
   else if ( reinfDetailsResult < 0 )
   {
      *p << _T("### Reinforcement details could not be listed.") << rptNewLine;
   }

   if ( pSegment->HandlingData.pHaulTruckLibraryEntry == nullptr )
   {
      *p << _T("++ Shipping analysis not performed") << rptNewLine;
   }

   return pChapter;
}

CChapterBuilder* CGirderScheduleChapterBuilder::Clone() const
{
   return new CGirderScheduleChapterBuilder;
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
int CGirderScheduleChapterBuilder::GetReinforcementDetails(IBroker* pBroker,const CSegmentKey& segmentKey,CLSID& familyCLSID,Float64* pz1Spacing,Float64 *pz1Length,Float64 *pz2Spacing,Float64* pz2Length,Float64 *pz3Spacing,Float64* pz3Length) const
{
   GET_IFACE2(pBroker,IStirrupGeometry,pStirrupGeometry);
   if ( !pStirrupGeometry->AreStirrupZonesSymmetrical(segmentKey) )
   {
      return STIRRUP_ERROR_SYMMETRIC;
   }

   // Check if the number of zones is consistent with the girder schedule
   ZoneIndexType nZones = pStirrupGeometry->GetPrimaryZoneCount(segmentKey); // this is total number of zones
   nZones = nZones/2 + 1; // this is the input number of zones (and it must be symmetric)
   if ( nZones != 5 && nZones != 6 )
   {
      return STIRRUP_ERROR_ZONES;
   }

   // Check first zone... it must be 1-1/2" long with 1-1/2" spacing... one space
   matRebar::Size barSize;
   Float64 count, spacing;
   pStirrupGeometry->GetPrimaryVertStirrupBarInfo(segmentKey,0, &barSize, &count, &spacing);

   Float64 zoneStart,zoneEnd;
   pStirrupGeometry->GetPrimaryZoneBounds(segmentKey,0,&zoneStart,&zoneEnd);
   pStirrupGeometry->GetPrimaryVertStirrupBarInfo(segmentKey,0, &barSize, &count, &spacing);
   Float64 zoneLength = zoneEnd - zoneStart;
   Float64 v = zoneLength/spacing;

   if ( barSize != matRebar::bs5 )
   {
      return STIRRUP_ERROR_BARSIZE;
   }

   if ( !IsEqual(v,1.0) && !IsEqual(spacing,::ConvertToSysUnits(1.5,unitMeasure::Inch)) )
   {
      return STIRRUP_ERROR_STARTZONE;
   }

   // So far, so good... start figuring out the V values

   // Zone 1 (V1 & V2)
   pStirrupGeometry->GetPrimaryZoneBounds(segmentKey,1,&zoneStart,&zoneEnd);
   pStirrupGeometry->GetPrimaryVertStirrupBarInfo(segmentKey,1, &barSize, &count, &spacing);
   zoneLength = zoneEnd - zoneStart;
   v = zoneLength/spacing;
   if ( !IsEqual(v,Round(v)) )
   {
      return STIRRUP_ERROR_ZONES;
   }

   if ( barSize != matRebar::bs5 )
   {
      return STIRRUP_ERROR_BARSIZE;
   }

   *pz1Spacing = spacing;
   *pz1Length = zoneLength;

   // Zone 2 (V3 & V4)
   pStirrupGeometry->GetPrimaryZoneBounds(segmentKey,2,&zoneStart,&zoneEnd);
   pStirrupGeometry->GetPrimaryVertStirrupBarInfo(segmentKey,2, &barSize, &count, &spacing);
   zoneLength = zoneEnd - zoneStart;
   v = zoneLength/spacing;
   if ( !IsEqual(v,Round(v)) )
   {
      return STIRRUP_ERROR_ZONES;
   }

   if ( barSize != matRebar::bs5 )
   {
      return STIRRUP_ERROR_BARSIZE;
   }

   *pz2Spacing = spacing;
   *pz2Length = zoneLength;

   // Zone 3 (either V6 or V5 & V6);
   Float64 v6Spacing;
   if ( nZones == 6 )
   {
      // this small zone that is labeled V6 on the stirrup layout is modeled
      pStirrupGeometry->GetPrimaryZoneBounds(segmentKey,3,&zoneStart,&zoneEnd);
      pStirrupGeometry->GetPrimaryVertStirrupBarInfo(segmentKey,3, &barSize, &count, &spacing);
      zoneLength = zoneEnd - zoneStart;
      v = zoneLength/spacing;
      if ( !IsEqual(v,Round(v)) )
      {
         return STIRRUP_ERROR_ZONES;
      }

      if ( familyCLSID == CLSID_UBeamFamily )
      {
         if ( barSize != matRebar::bs4 )
         {
            return STIRRUP_ERROR_BARSIZE;
         }
      }
      else
      {
         if ( barSize != matRebar::bs5 )
         {
            return STIRRUP_ERROR_BARSIZE;
         }
      }

      if ( !IsEqual(zoneLength,spacing) )
      {
         return STIRRUP_ERROR_V6;
      }

      v6Spacing = spacing;
   }

   ZoneIndexType zoneIdx = (nZones == 5 ? 3 : 4);
   pStirrupGeometry->GetPrimaryZoneBounds(segmentKey,zoneIdx,&zoneStart,&zoneEnd);
   pStirrupGeometry->GetPrimaryVertStirrupBarInfo(segmentKey,zoneIdx, &barSize, &count, &spacing);
   zoneLength = zoneEnd - zoneStart;
   v = zoneLength/spacing;
   if ( !IsEqual(v,Round(v)) )
   {
      return STIRRUP_ERROR_ZONES;
   }

   if ( familyCLSID == CLSID_UBeamFamily )
   {
      if ( barSize != matRebar::bs4 )
      {
         return STIRRUP_ERROR_BARSIZE;
      }
   }
   else
   {
      if ( barSize != matRebar::bs5 )
      {
         return STIRRUP_ERROR_BARSIZE;
      }
   }

   *pz3Spacing = spacing;
   *pz3Length = zoneLength;

   if ( nZones == 6 && !IsEqual(spacing,v6Spacing) )
   {
      return STIRRUP_ERROR_V6;
   }

   zoneIdx = (nZones == 5 ? 4 : 5);
   pStirrupGeometry->GetPrimaryVertStirrupBarInfo(segmentKey,zoneIdx, &barSize, &count, &spacing);

   if ( familyCLSID == CLSID_WFBeamFamily || familyCLSID == CLSID_UBeamFamily )
   {
      if ( !IsEqual(spacing,::ConvertToSysUnits(18.0,unitMeasure::Inch)) )
      {
         return STIRRUP_ERROR_LASTZONE;
      }
   }
   else if (familyCLSID == CLSID_SlabBeamFamily)
   {
      GET_IFACE2(pBroker, IPointOfInterest, pPoi);
      pgsPointOfInterest poi = pPoi->GetPointOfInterest(segmentKey, 0.0);

      GET_IFACE2(pBroker,IGirder, pGirder);
      Float64 H = pGirder->GetHeight(poi);

      Float64 lastZoneSpacing = Min(H - ::ConvertToSysUnits(3.0, unitMeasure::Inch), ::ConvertToSysUnits(18.0, unitMeasure::Inch));
      if (!IsEqual(spacing, lastZoneSpacing))
      {
         return STIRRUP_ERROR_LASTZONE;
      }
   }
   else
   {
      if ( !IsEqual(spacing,::ConvertToSysUnits(9.0,unitMeasure::Inch)) )
      {
         return STIRRUP_ERROR_LASTZONE;
      }
   }


   if ( familyCLSID == CLSID_UBeamFamily || familyCLSID == CLSID_SlabBeamFamily )
   {
      if ( barSize != matRebar::bs4 )
      {
         return STIRRUP_ERROR_BARSIZE;
      }
   }
   else
   {
      if ( barSize != matRebar::bs5 )
      {
         return STIRRUP_ERROR_BARSIZE;
      }
   }

   return STIRRUP_ERROR_NONE;
}

int CGirderScheduleChapterBuilder::GetDebondDetails(IBroker* pBroker,const CSegmentKey& segmentKey,std::vector<DebondInformation>& debondInfo) const
{
   GET_IFACE2( pBroker, IStrandGeometry, pStrandGeometry );
   if ( !pStrandGeometry->IsDebondingSymmetric(segmentKey) )
   {
      return DEBOND_ERROR_SYMMETRIC;
   }

   // fail if not symmetric
   SectionIndexType nSections = pStrandGeometry->GetNumDebondSections(segmentKey,pgsTypes::metStart,pgsTypes::Straight);
   for ( SectionIndexType sectionIdx = 0; sectionIdx < nSections; sectionIdx++ )
   {
      DebondInformation dbInfo;
      dbInfo.Length = pStrandGeometry->GetDebondSection(segmentKey,pgsTypes::metStart,sectionIdx,pgsTypes::Straight);

      dbInfo.Strands = pStrandGeometry->GetDebondedStrandsAtSection(segmentKey,pgsTypes::metStart,sectionIdx,pgsTypes::Straight);
       
      debondInfo.push_back(dbInfo);
   }

   return DEBOND_ERROR_NONE;
}
