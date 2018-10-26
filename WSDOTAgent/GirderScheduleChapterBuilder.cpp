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

#include "StdAfx.h"
#include "GirderScheduleChapterBuilder.h"
#include <PGSuperTypes.h>
#include <Reporting\ReportStyleHolder.h>
#include <Reporting\SpanGirderReportSpecification.h>
#include <PgsExt\PointOfInterest.h>
#include <PgsExt\BridgeDescription.h>
#include <PgsExt\GirderData.h>
#include <EAF\EAFDisplayUnits.h>
#include <IFace\AnalysisResults.h>
#include <IFace\Bridge.h>
#include <IFace\Artifact.h>
#include <IFace\Project.h>
#include <PgsExt\GirderArtifact.h>
#include <psgLib\SpecLibraryEntry.h>

#include <psgLib\ConnectionLibraryEntry.h>

#include <WBFLCogo.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


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
   CSpanGirderReportSpecification* pSGRptSpec = dynamic_cast<CSpanGirderReportSpecification*>(pRptSpec);
   CComPtr<IBroker> pBroker;
   pSGRptSpec->GetBroker(&pBroker);
   SpanIndexType span = pSGRptSpec->GetSpan();
   GirderIndexType girder = pSGRptSpec->GetGirder();

   GET_IFACE2(pBroker,IEAFDisplayUnits,pDisplayUnits);

   GET_IFACE2(pBroker,IArtifact,pIArtifact);
   const pgsGirderArtifact* pArtifact = pIArtifact->GetArtifact(span,girder);

   rptChapter* pChapter = CPGSuperChapterBuilder::Build(pRptSpec,level);

   GET_IFACE2( pBroker, IStrandGeometry, pStrandGeometry );

   // WsDOT reports don't support Straight-Web strand option
   if (pStrandGeometry->GetAreHarpedStrandsForcedStraight(span, girder))
   {
      rptParagraph* p;
      
      p = new rptParagraph;
      *pChapter << p;
      *p << color(Red) << Bold(_T("A WSDOT Girder Schedule could not be generated because this girder utilizes straight web strands. WSDOT Standard Girders utilize harped strands.")) << color(Black) << rptNewLine;
      return pChapter;
   }

   GET_IFACE2(pBroker,IGirderData,pGirderData);
   const CGirderData* pgirderData = pGirderData->GetGirderData(span,girder);
   if (pgirderData->PrestressData.GetNumPermStrandsType() == NPS_DIRECT_SELECTION)
   {
      rptParagraph* p;
      
      p = new rptParagraph;
      *pChapter << p;
      *p << color(Red) << Bold(_T("A WSDOT Girder Schedule could not be generated because this girder utilizes Direct Strand Fill. WSDOT Standard Girders utilize sequentially filled strands.")) << color(Black) << rptNewLine;
      return pChapter;
   }

   if( pArtifact->Passed() )
   {
      rptParagraph* pPara = new rptParagraph;
      *pChapter << pPara;
      *pPara << _T("The Specification Check was Successful") << rptNewLine;
   }
   else
   {
      rptParagraph* pPara = new rptParagraph;
      *pChapter << pPara;
      *pPara << color(Red) << _T("The Specification Check Was Not Successful") << color(Black) << rptNewLine;
   }

   rptRcScalar scalar;
   scalar.SetFormat( pDisplayUnits->GetScalarFormat().Format );
   scalar.SetWidth( pDisplayUnits->GetScalarFormat().Width );
   scalar.SetPrecision( pDisplayUnits->GetScalarFormat().Precision );
   INIT_UV_PROTOTYPE( rptLengthUnitValue, loc,            pDisplayUnits->GetSpanLengthUnit(),    true );
   INIT_UV_PROTOTYPE( rptForceUnitValue,  force,          pDisplayUnits->GetShearUnit(),         true );
   INIT_UV_PROTOTYPE( rptStressUnitValue, stress,         pDisplayUnits->GetStressUnit(),        true );
   INIT_UV_PROTOTYPE( rptStressUnitValue, mod_e,          pDisplayUnits->GetModEUnit(),          true );
   INIT_UV_PROTOTYPE( rptMomentUnitValue, moment,         pDisplayUnits->GetMomentUnit(),        true );
   INIT_UV_PROTOTYPE( rptAngleUnitValue,  angle,          pDisplayUnits->GetAngleUnit(),         true );
   INIT_UV_PROTOTYPE( rptForcePerLengthUnitValue, wt_len, pDisplayUnits->GetForcePerLengthUnit(),true );
   INIT_UV_PROTOTYPE( rptMomentPerAngleUnitValue, spring, pDisplayUnits->GetMomentPerAngleUnit(),true );

   INIT_FRACTIONAL_LENGTH_PROTOTYPE( gdim,      IS_US_UNITS(pDisplayUnits), 8, pDisplayUnits->GetComponentDimUnit(), true, false );
   INIT_FRACTIONAL_LENGTH_PROTOTYPE( glength,   IS_US_UNITS(pDisplayUnits), 4, pDisplayUnits->GetSpanLengthUnit(),   true, false );

   rptParagraph* p = new rptParagraph;
   *pChapter << p;

   GET_IFACE2(pBroker,IArtifact,pArtifacts);
   const pgsGirderArtifact* pGdrArtifact = pArtifacts->GetArtifact(span,girder);
   const pgsConstructabilityArtifact* pConstArtifact = pGdrArtifact->GetConstructabilityArtifact();

   GET_IFACE2(pBroker,ICamber,pCamber);

   // create pois at the start of girder and mid-span
   pgsPointOfInterest poiStart(span,girder,0.0);

   GET_IFACE2( pBroker, ILibrary, pLib );
   GET_IFACE2( pBroker, ISpecification, pSpec );
   std::_tstring spec_name = pSpec->GetSpecification();
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( spec_name.c_str() );
   Float64 min_days =  ::ConvertFromSysUnits(pSpecEntry->GetCreepDuration2Min(), unitMeasure::Day);
   Float64 max_days =  ::ConvertFromSysUnits(pSpecEntry->GetCreepDuration2Max(), unitMeasure::Day);

   GET_IFACE2(pBroker, IPointOfInterest, pPointOfInterest );
   std::vector<pgsPointOfInterest> pmid = pPointOfInterest->GetPointsOfInterest(span, girder,pgsTypes::BridgeSite1, POI_MIDSPAN);
   ATLASSERT(pmid.size()==1);

   GET_IFACE2(pBroker,IBridge,pBridge);

   GET_IFACE2(pBroker,IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CGirderTypes* pGirderTypes = pBridgeDesc->GetSpan(span)->GetGirderTypes();

   rptRcTable* p_table = pgsReportStyleHolder::CreateTableNoHeading(2,_T(""));
   *p << p_table;

   RowIndexType row = 0;
   (*p_table)(row,0) << _T("Span");
   (*p_table)(row,1) << LABEL_SPAN(span);

   (*p_table)(++row,0) << _T("Girder");
   (*p_table)(row  ,1) << LABEL_GIRDER(girder);

   (*p_table)(++row,0) << _T("Girder Series");
   (*p_table)(row,  1) << pGirderTypes->GetGirderName(girder);

   (*p_table)(++row,0) << _T("End 1 Type");
   (*p_table)(row  ,1) << _T("*");

   (*p_table)(++row,0) << _T("End 2 Type");
   (*p_table)(row  ,1) << _T("*");

   if ( pBridgeDesc->GetSlabOffsetType() == pgsTypes::sotBridge )
   {
      (*p_table)(++row,0) << _T("\"A\" Dimension at CL Bearings");
      (*p_table)(row,  1) << gdim.SetValue(pBridgeDesc->GetSlabOffset());
   }
   else
   {
      (*p_table)(++row,0) << _T("\"A\" Dimension at CL Bearing End 1");
      (*p_table)(row,  1) << gdim.SetValue(pGirderTypes->GetSlabOffset(girder,pgsTypes::metStart));

      (*p_table)(++row,0) << _T("\"A\" Dimension at CL Bearing End 2");
      (*p_table)(row,  1) << gdim.SetValue(pGirderTypes->GetSlabOffset(girder,pgsTypes::metEnd));
   }

   (*p_table)(++row,0) << _T("Intermediate Diaphragm Type");
   (*p_table)(row,  1) << _T("**");

   const pgsLiftingCheckArtifact* pLiftArtifact = pGdrArtifact->GetLiftingCheckArtifact();
   if (pLiftArtifact!=NULL)
   {
      Float64 L = (pLiftArtifact->GetGirderLength() - pLiftArtifact->GetClearSpanBetweenPickPoints())/2.0;
      (*p_table)(++row,0) << _T("Location of Lifting Loops, L");
      (*p_table)(row  ,1) << loc.SetValue(L);
   }

   (*p_table)(++row,0) << Sub2(_T("L"),_T("d"));
   (*p_table)(row,  1) << _T("***");

   const pgsHaulingCheckArtifact* pHaulingArtifact = pGdrArtifact->GetHaulingCheckArtifact();
   if ( pHaulingArtifact != NULL )
   {
      Float64 L = pHaulingArtifact->GetLeadingOverhang();
      (*p_table)(++row,0) << _T("Location of Lead Shipping Support, ") << Sub2(_T("L"),_T("L"));
      (*p_table)(row  ,1) << loc.SetValue(L);
      L = pHaulingArtifact->GetTrailingOverhang();
      (*p_table)(++row,0) << _T("Location of Trailing Shipping Support, ") << Sub2(_T("L"),_T("T"));
      (*p_table)(row  ,1) << loc.SetValue(L);
   }

   CComPtr<IDirection> objDirGirder;
   pBridge->GetGirderBearing(span,girder,&objDirGirder);
   Float64 dirGdr;
   objDirGirder->get_Value(&dirGdr);

   CComPtr<IDirection> objDir1;
   pBridge->GetPierDirection(span,&objDir1);
   Float64 dir1;
   objDir1->get_Value(&dir1);

   CComPtr<IDirection> objDir2;
   pBridge->GetPierDirection(span+1,&objDir2);
   Float64 dir2;
   objDir1->get_Value(&dir2);

   CComPtr<IAngle> objAngle1;
   objDirGirder->AngleBetween(objDir1,&objAngle1);
   Float64 t1;
   objAngle1->get_Value(&t1);

   CComPtr<IAngle> objAngle2;
   objDirGirder->AngleBetween(objDir2,&objAngle2);
   Float64 t2;
   objAngle2->get_Value(&t2);

   t1 -= M_PI;
   t2 -= M_PI;

   (*p_table)(++row,0) << Sub2(symbol(theta),_T("1"));
   (*p_table)(row  ,1) << angle.SetValue(t1);

   (*p_table)(++row,0) << Sub2(symbol(theta),_T("2"));
   (*p_table)(row  ,1) << angle.SetValue(t2);


   PierIndexType prevPierIdx = (PierIndexType)span;
   PierIndexType nextPierIdx = prevPierIdx+1;
   bool bContinuousLeft, bContinuousRight, bIntegralLeft, bIntegralRight;

   pBridge->IsContinuousAtPier(prevPierIdx,&bContinuousLeft,&bContinuousRight);
   pBridge->IsIntegralAtPier(prevPierIdx,&bIntegralLeft,&bIntegralRight);

   (*p_table)(++row,0) << Sub2(_T("P"),_T("1"));
   if ( bContinuousLeft || bIntegralLeft )
   {
      (*p_table)(row,1) << _T("-");
   }
   else
   {
      Float64 P1 = pBridge->GetGirderStartConnectionLength(span,girder);
      (*p_table)(row  ,1) << gdim.SetValue(P1);
   }

   pBridge->IsContinuousAtPier(nextPierIdx,&bContinuousLeft,&bContinuousRight);
   pBridge->IsIntegralAtPier(nextPierIdx,&bIntegralLeft,&bIntegralRight);
   (*p_table)(++row,0) << Sub2(_T("P"),_T("2"));
   if ( bContinuousRight || bIntegralRight )
   {
      (*p_table)(row,1) << _T("-");
   }
   else
   {
      Float64 P2 = pBridge->GetGirderEndConnectionLength(span,girder);
      (*p_table)(row  ,1) << gdim.SetValue(P2);
   }

   (*p_table)(++row,0) << _T("Plan Length (Along Girder Grade)");
   (*p_table)(row  ,1) << glength.SetValue(pBridge->GetGirderPlanLength(span,girder));

   GET_IFACE2(pBroker, IBridgeMaterial, pMaterial);
   (*p_table)(++row,0) << RPT_FC << _T(" (at 28 days)");
   (*p_table)(row  ,1) << stress.SetValue(pMaterial->GetFcGdr(span,girder));

   (*p_table)(++row,0) << RPT_FCI << _T(" (at Release)");
   (*p_table)(row  ,1) << stress.SetValue(pMaterial->GetFciGdr(span,girder));

   StrandIndexType Nh = pStrandGeometry->GetNumStrands(span,girder,pgsTypes::Harped);
   (*p_table)(++row,0) << _T("Number of Harped Strands");
   (*p_table)(row  ,1) << Nh;

   StrandIndexType Ns = pStrandGeometry->GetNumStrands(span,girder,pgsTypes::Straight);
   (*p_table)(++row,0) << _T("Number of Straight Strands");
   (*p_table)(row  ,1) << Ns;
   StrandIndexType nDebonded = pStrandGeometry->GetNumDebondedStrands(span,girder,pgsTypes::Straight);
   if ( nDebonded != 0 )
      (*p_table)(row,1) << _T(" (") << nDebonded << _T(" debonded)");

   if ( 0 < pStrandGeometry->GetMaxStrands(span,girder,pgsTypes::Temporary ) )
   {
      StrandIndexType Nt = pStrandGeometry->GetNumStrands(span,girder,pgsTypes::Temporary);
      (*p_table)(++row,0) << _T("Number of Temporary Strands");

      switch ( pgirderData->PrestressData.TempStrandUsage )
      {
      case pgsTypes::ttsPTAfterLifting:
         (*p_table)(row,0) << rptNewLine << _T("Temporary strands post-tensioned immediately after lifting");
         break;

      case pgsTypes::ttsPTBeforeShipping:
         (*p_table)(row,0) << rptNewLine << _T("Temporary strands post-tensioned immediately before shipping");
         break;
      }

      (*p_table)(row  ,1) << Nt;
   }

   GET_IFACE2(pBroker, ISectProp2, pSectProp2 );
   Float64 ybg = pSectProp2->GetYb(pgsTypes::CastingYard,pmid[0]);
   Float64 nEff;
   Float64 sse = pStrandGeometry->GetSsEccentricity(pmid[0], &nEff);
   (*p_table)(++row,0) << _T("E");
   if (0 < Ns)
      (*p_table)(row,1) << gdim.SetValue(ybg-sse);
   else
      (*p_table)(row,1) << RPT_NA;

   Float64 hse = pStrandGeometry->GetHsEccentricity(pmid[0], &nEff);
   (*p_table)(++row,0) << Sub2(_T("F"),_T("C.L."));
   if (0 < Nh)
      (*p_table)(row,1) << gdim.SetValue(ybg-hse);
   else
      (*p_table)(row,1) << RPT_NA;


   Float64 ytg = pSectProp2->GetYtGirder(pgsTypes::CastingYard,poiStart);
   Float64 hss = pStrandGeometry->GetHsEccentricity(poiStart, &nEff);
   (*p_table)(++row,0) << Sub2(_T("F"),_T("o"));
   if (0 < Nh)
   {
      (*p_table)(row,1) << gdim.SetValue(ytg+hss);
   }
   else
   {
      (*p_table)(row,1) << RPT_NA;
   }

   // Strand Extensions
   (*p_table)(++row,0) << _T("Straight Strands to Extend, End 1");
   StrandIndexType nExtended = 0;
   for ( StrandIndexType strandIdx = 0; strandIdx < Ns; strandIdx++ )
   {
      if ( pStrandGeometry->IsExtendedStrand(span,girder,pgsTypes::metStart,strandIdx,pgsTypes::Straight) )
      {
         nExtended++;
         (*p_table)(row,1) << _T(" ") << (strandIdx+1);
      }
   }
   if ( nExtended == 0 )
   {
      (*p_table)(row,1) << _T("");
   }

   (*p_table)(++row,0) << _T("Straight Strands to Extend, End 2");
   nExtended = 0;
   for ( StrandIndexType strandIdx = 0; strandIdx < Ns; strandIdx++ )
   {
      if ( pStrandGeometry->IsExtendedStrand(span,girder,pgsTypes::metEnd,strandIdx,pgsTypes::Straight) )
      {
         nExtended++;
         (*p_table)(row,1) << _T(" ") << (strandIdx+1);
      }
   }
   if ( nExtended == 0 )
   {
      (*p_table)(row,1) << _T("");
   }

   (*p_table)(++row,0) << _T("Screed Camber, C");
   (*p_table)(row  ,1) << gdim.SetValue(pCamber->GetScreedCamber( pmid[0] ) );

   // get # of days for creep
   (*p_table)(++row,0) << _T("Lower bound camber at ")<< min_days<<_T(" days, 50% of D") <<Sub(min_days);
   (*p_table)(row  ,1) << gdim.SetValue(0.5*pCamber->GetDCamberForGirderSchedule( pmid[0], CREEP_MINTIME) );
   (*p_table)(++row,0) << _T("Upper bound camber at ")<< max_days<<_T(" days, D") << Sub(max_days);
   (*p_table)(row  ,1) << gdim.SetValue(pCamber->GetDCamberForGirderSchedule( pmid[0], CREEP_MAXTIME) );


#pragma Reminder("RAB: Update Add V")
   // Stirrups
   (*p_table)(++row,0) << _T("V1");
   (*p_table)(++row,0) << _T("V2");
   (*p_table)(++row,0) << _T("V3");
   (*p_table)(++row,0) << _T("V4");
   (*p_table)(++row,0) << _T("V5");
   (*p_table)(++row,0) << _T("V6");

   // Stirrup Zones

   // H1 (Hg + "A" + 3")
   if ( pBridgeDesc->GetSlabOffsetType() == pgsTypes::sotBridge )
   {
      (*p_table)(++row,0) << _T("H1 (#)");
      Float64 Hg = pSectProp2->GetHg(pgsTypes::CastingYard,poiStart);
      Float64 H1 = pBridgeDesc->GetSlabOffset() + Hg + ::ConvertToSysUnits(3.0,unitMeasure::Inch);
      (*p_table)(row,  1) << gdim.SetValue(H1);
   }
   else
   {
      (*p_table)(++row,0) << _T("H1 at End 1 (#)");
      Float64 Hg = pSectProp2->GetHg(pgsTypes::CastingYard,poiStart);
      Float64 H1 = pGirderTypes->GetSlabOffset(girder,pgsTypes::metStart) + Hg + ::ConvertToSysUnits(3.0,unitMeasure::Inch);
      (*p_table)(row,  1) << gdim.SetValue(H1);

      (*p_table)(++row,0) << _T("H1 at End 2 (#)");
      pgsPointOfInterest poiEnd(poiStart);
      poiEnd.SetDistFromStart(pBridge->GetGirderLength(span,girder));
      Hg = pSectProp2->GetHg(pgsTypes::CastingYard,poiEnd);
      H1 = pGirderTypes->GetSlabOffset(girder,pgsTypes::metEnd) + Hg + ::ConvertToSysUnits(3.0,unitMeasure::Inch);
      (*p_table)(row,  1) << gdim.SetValue(H1);
   }


   p = new rptParagraph(pgsReportStyleHolder::GetFootnoteStyle());
   *pChapter << p;
   *p << _T("* End Types to be determined by the Designer") << rptNewLine;
   *p << _T("** Intermediate Diaphragm Type to be determined by the Designer") << rptNewLine;
   *p << _T("*** ") << Sub2(_T("L"),_T("d")) << _T(" to be determined by the Designer") << rptNewLine;
   *p << _T("# H1 is computed as the height of the girder H + 3\" + \"A\" Dimension. Designers shall check H1 for the effect of vertical curve and increase as necessary");

   // Figure
   p = new rptParagraph;
   *pChapter << p;
   *p << rptRcImage(pgsReportStyleHolder::GetImagePath() + _T("GirderSchedule.jpg")) << rptNewLine;

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
