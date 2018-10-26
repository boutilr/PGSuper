///////////////////////////////////////////////////////////////////////
// PGSuper - Prestressed Girder SUPERstructure Design and Analysis
// Copyright � 1999-2012  Washington State Department of Transportation
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
#include <Reporting\PrestressForceChapterBuilder.h>
#include <Reporting\PrestressLossTable.h>

#include <PgsExt\GirderData.h>

#include <EAF\EAFDisplayUnits.h>
#include <IFace\Bridge.h>
#include <IFace\PrestressForce.h>
#include <IFace\Project.h>
#include <IFace\AnalysisResults.h>

#include <Material\PsStrand.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/****************************************************************************
CLASS
   CPrestressForceChapterBuilder
****************************************************************************/


////////////////////////// PUBLIC     ///////////////////////////////////////

//======================== LIFECYCLE  =======================================
CPrestressForceChapterBuilder::CPrestressForceChapterBuilder(bool bSelect) :
CPGSuperChapterBuilder(bSelect)
{
}

//======================== OPERATORS  =======================================
//======================== OPERATIONS =======================================
LPCTSTR CPrestressForceChapterBuilder::GetName() const
{
   return TEXT("Prestressing Force and Strand Stresses");
}

rptChapter* CPrestressForceChapterBuilder::Build(CReportSpecification* pRptSpec,Uint16 level) const
{
   CSpanGirderReportSpecification* pSGRptSpec = dynamic_cast<CSpanGirderReportSpecification*>(pRptSpec);
   CGirderReportSpecification* pGdrRptSpec    = dynamic_cast<CGirderReportSpecification*>(pRptSpec);
   CComPtr<IBroker> pBroker;
   SpanIndexType span;
   GirderIndexType gdr;

   if ( pSGRptSpec )
   {
      pSGRptSpec->GetBroker(&pBroker);
      span = pSGRptSpec->GetSpan();
      gdr = pSGRptSpec->GetGirder();
   }
   else if ( pGdrRptSpec )
   {
      pGdrRptSpec->GetBroker(&pBroker);
      span = ALL_SPANS;
      gdr = pGdrRptSpec->GetGirder();
   }
   else
   {
      span = ALL_SPANS;
      gdr  = ALL_GIRDERS;
   }

   rptChapter* pChapter = CPGSuperChapterBuilder::Build(pRptSpec,level);

   GET_IFACE2(pBroker,IEAFDisplayUnits,pDisplayUnits);
   // These are the interfaces we are going to be using
   GET_IFACE2(pBroker,IBridgeMaterial,       pMat);
   GET_IFACE2(pBroker,IStrandGeometry, pStrandGeom);
   GET_IFACE2(pBroker,IPrestressForce, pPrestressForce ); 
   GET_IFACE2(pBroker,IPointOfInterest,pIPOI);
   GET_IFACE2(pBroker,IGirderData,pGirderData);
   GET_IFACE2(pBroker,ILosses,pLosses);
   GET_IFACE2(pBroker,IPrestressStresses,pPrestressStresses);
   GET_IFACE2(pBroker,IBridge,pBridge);

   // Setup some unit-value prototypes
   INIT_UV_PROTOTYPE( rptStressUnitValue, stress, pDisplayUnits->GetStressUnit(),       true );
   INIT_UV_PROTOTYPE( rptAreaUnitValue,   area,   pDisplayUnits->GetAreaUnit(),         true );
   INIT_UV_PROTOTYPE( rptLengthUnitValue, len,    pDisplayUnits->GetComponentDimUnit(), true );
   INIT_UV_PROTOTYPE( rptForceUnitValue,  force,  pDisplayUnits->GetGeneralForceUnit(), true );

   SpanIndexType nSpans = pBridge->GetSpanCount();
   SpanIndexType firstSpanIdx = (span == ALL_SPANS ? 0 : span);
   SpanIndexType lastSpanIdx  = (span == ALL_SPANS ? nSpans : firstSpanIdx+1);
   for ( SpanIndexType spanIdx = firstSpanIdx; spanIdx < lastSpanIdx; spanIdx++ )
   {
      GirderIndexType nGirders = pBridge->GetGirderCount(spanIdx);
      GirderIndexType firstGirderIdx = min(nGirders-1,(gdr == ALL_GIRDERS ? 0 : gdr));
      GirderIndexType lastGirderIdx  = min(nGirders,  (gdr == ALL_GIRDERS ? nGirders : firstGirderIdx + 1));
      for ( GirderIndexType gdrIdx = firstGirderIdx; gdrIdx < lastGirderIdx; gdrIdx++ )
      {
         CGirderData girderData = pGirderData->GetGirderData(spanIdx,gdrIdx);

         // Write out what we have for prestressing in this girder
         rptParagraph* pPara = new rptParagraph(pgsReportStyleHolder::GetHeadingStyle());
         *pChapter << pPara;
         (*pPara) << _T("Span ") << LABEL_SPAN(spanIdx) << _T(" Girder ") << LABEL_GIRDER(gdrIdx) << rptNewLine;

         bool harpedAreStraight = pStrandGeom->GetAreHarpedStrandsForcedStraight(spanIdx,gdrIdx);

         pPara = new rptParagraph;
         *pChapter << pPara;
         StrandIndexType Ns = pStrandGeom->GetNumStrands(spanIdx,gdrIdx,pgsTypes::Straight);
         StrandIndexType Nh = pStrandGeom->GetNumStrands(spanIdx,gdrIdx,pgsTypes::Harped);
         *pPara << _T("Number of straight strands (N") << Sub(_T("s")) << _T(") = ") << Ns << _T(" ") 
            << _T("(P") << Sub(_T("jack")) << _T(" = ") << force.SetValue(pStrandGeom->GetPjack(spanIdx,gdrIdx,pgsTypes::Straight)) << _T(")") << rptNewLine;
         *pPara << _T("Number of ")<< LABEL_HARP_TYPE(harpedAreStraight) <<_T(" strands (N") << Sub(_T("h")) << _T(") = ") << Nh << _T(" ") 
            << _T("(P") << Sub(_T("jack")) << _T(" = ") << force.SetValue(pStrandGeom->GetPjack(spanIdx,gdrIdx,pgsTypes::Harped)) << _T(")") << rptNewLine;

         if ( 0 < pStrandGeom->GetMaxStrands(spanIdx,gdrIdx,pgsTypes::Temporary ) )
         {
            *pPara << _T("Number of temporary strands (N") << Sub(_T("t")) << _T(") = ") << pStrandGeom->GetNumStrands(spanIdx,gdrIdx,pgsTypes::Temporary) << _T(" ") 
               << _T("(P") << Sub(_T("jack")) << _T(" = ") << force.SetValue(pStrandGeom->GetPjack(spanIdx,gdrIdx,pgsTypes::Temporary)) << _T(")") << rptNewLine;

               
            switch(girderData.TempStrandUsage)
            {
            case pgsTypes::ttsPretensioned:
               *pPara << _T("Temporary Strands pretensioned with permanent strands") << rptNewLine;
               break;

            case pgsTypes::ttsPTBeforeShipping:
               *pPara << _T("Temporary Strands post-tensioned immedately before shipping") << rptNewLine;
               break;

            case pgsTypes::ttsPTAfterLifting:
               *pPara << _T("Temporary Strands post-tensioned immedately after lifting") << rptNewLine;
               break;

            case pgsTypes::ttsPTBeforeLifting:
               *pPara << _T("Temporary Strands post-tensioned before lifting") << rptNewLine;
               break;
            }

            *pPara << _T("Total permanent strands (N) = ") << Ns+Nh << _T(" ") 
               << _T("(P") << Sub(_T("jack")) << _T(" = ") << force.SetValue(pStrandGeom->GetPjack(spanIdx,gdrIdx,pgsTypes::Straight)+pStrandGeom->GetPjack(spanIdx,gdrIdx,pgsTypes::Harped)) << _T(")") << rptNewLine;

            *pPara << _T("Permanent Strands: ") << RPT_APS << _T(" = ") << area.SetValue(pStrandGeom->GetStrandArea(spanIdx,gdrIdx,pgsTypes::Permanent)) << rptNewLine;
            *pPara << _T("Temporary Strands: ") << RPT_APS << _T(" = ") << area.SetValue(pStrandGeom->GetStrandArea(spanIdx,gdrIdx,pgsTypes::Temporary)) << rptNewLine;
            *pPara << _T("Total Strand Area: ") << RPT_APS << _T(" = ") << area.SetValue( pStrandGeom->GetAreaPrestressStrands(spanIdx,gdrIdx,true)) << rptNewLine;
            *pPara << _T("Prestress Transfer Length (Permanent) = ") << len.SetValue( pPrestressForce->GetXferLength(spanIdx,gdrIdx,pgsTypes::Permanent) ) << rptNewLine;
            *pPara << _T("Prestress Transfer Length (Temporary) = ") << len.SetValue( pPrestressForce->GetXferLength(spanIdx,gdrIdx,pgsTypes::Temporary) ) << rptNewLine;
         }
         else
         {
            *pPara << RPT_APS << _T(" = ") << area.SetValue( pStrandGeom->GetAreaPrestressStrands(spanIdx,gdrIdx,false)) << rptNewLine;
            *pPara << Sub2(_T("P"),_T("jack")) << _T(" = ") << force.SetValue( pStrandGeom->GetPjack(spanIdx,gdrIdx,false)) << rptNewLine;
            *pPara << _T("Prestress Transfer Length = ") << len.SetValue( pPrestressForce->GetXferLength(spanIdx,gdrIdx,pgsTypes::Permanent) ) << rptNewLine;
         }

         // Write out strand forces and stresses at the various stages of prestress loss
         pPara = new rptParagraph;
         *pChapter << pPara;
         *pPara << CPrestressLossTable().Build(pBroker,spanIdx,gdrIdx,pDisplayUnits) << rptNewLine;

         pPara = new rptParagraph(pgsReportStyleHolder::GetFootnoteStyle());
         *pChapter << pPara;
         *pPara << _T("Eff. Loss: Effective prestress loss taken as the sum of the actual prestress loss and elastic gains/losses due to applied loads") << rptNewLine;
      } // gdrIdx
   } // spanIdx

   return pChapter;
}

CChapterBuilder* CPrestressForceChapterBuilder::Clone() const
{
   return new CPrestressForceChapterBuilder;
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
