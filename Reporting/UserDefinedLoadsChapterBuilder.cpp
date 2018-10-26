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
#include <Reporting\UserDefinedLoadsChapterBuilder.h>

#include <EAF\EAFDisplayUnits.h>
#include <IFace\Bridge.h>


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


/****************************************************************************
CLASS
   CUserDefinedLoadsChapterBuilder
****************************************************************************/


////////////////////////// PUBLIC     ///////////////////////////////////////

//======================== LIFECYCLE  =======================================

CUserDefinedLoadsChapterBuilder::CUserDefinedLoadsChapterBuilder(bool bSelect, bool SimplifiedVersion) :
CPGSuperChapterBuilder(bSelect),
m_bSimplifiedVersion(SimplifiedVersion)
{
}


//======================== OPERATORS  =======================================
//======================== OPERATIONS =======================================
LPCTSTR CUserDefinedLoadsChapterBuilder::GetName() const
{
   return TEXT("User Defined Loads");
}

rptChapter* CUserDefinedLoadsChapterBuilder::Build(CReportSpecification* pRptSpec,Uint16 level) const
{
   CComPtr<IBroker> pBroker;
   SpanIndexType span;
   GirderIndexType gdr;

   CSpanGirderReportSpecification* pSGRptSpec = dynamic_cast<CSpanGirderReportSpecification*>(pRptSpec);
   if ( pSGRptSpec )
   {
      pSGRptSpec->GetBroker(&pBroker);
      GET_IFACE2(pBroker,IBridge,pBridge);

      // Don't report loads on adjacent spans unless continuous connection
      span = pSGRptSpec->GetSpan();

      PierIndexType prevPier = span;
      bool bContinuousOnLeft, bContinuousOnRight;
      pBridge->IsContinuousAtPier(prevPier,&bContinuousOnLeft,&bContinuousOnRight);
      if(bContinuousOnRight)
      {
         span = ALL_SPANS;
      }
      else
      {
         PierIndexType nextPier = span+1;
         pBridge->IsContinuousAtPier(nextPier,&bContinuousOnLeft,&bContinuousOnRight);
         if(bContinuousOnLeft)
         {
            span = ALL_SPANS;
         }
      }

      gdr = pSGRptSpec->GetGirder();
   }
   else
   {
      CGirderReportSpecification* pGdrRptSpec    = dynamic_cast<CGirderReportSpecification*>(pRptSpec);
      if(pGdrRptSpec)
      {
         pGdrRptSpec->GetBroker(&pBroker);
         span = ALL_SPANS;
         gdr = pGdrRptSpec->GetGirder();
      }
      else
      {
         ATLASSERT(0);
         return NULL; // no hope going further
      }
   }

   GET_IFACE2(pBroker,IEAFDisplayUnits,pDisplayUnits);

   rptChapter* pChapter = CPGSuperChapterBuilder::Build(pRptSpec,level);

   GET_IFACE2(pBroker,IBridge,pBridge);
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
         rptParagraph* pParagraph;

         // Only print span and girder if we are in a multi span or multi girder loop
         if (lastSpanIdx!=firstSpanIdx+1 || lastGirderIdx!=firstGirderIdx+1)
         {
            pParagraph = new rptParagraph(pgsReportStyleHolder::GetHeadingStyle());
            *pChapter << pParagraph;
            *pParagraph <<_T("Span ")<<LABEL_SPAN(spanIdx)<<_T(" Girder ")<<LABEL_GIRDER(gdrIdx)<<rptNewLine;
         }

         pParagraph = new rptParagraph;
         *pChapter << pParagraph;
         *pParagraph <<_T("Locations are measured from left support.")<<rptNewLine;

         // tables of details - point loads first
         rptParagraph* ppar1 = CreatePointLoadTable(pBroker, spanIdx, gdrIdx, pDisplayUnits, level, m_bSimplifiedVersion);
         *pChapter <<ppar1;

         // distributed loads
         ppar1 = CreateDistributedLoadTable(pBroker, spanIdx, gdrIdx, pDisplayUnits, level, m_bSimplifiedVersion);
         *pChapter <<ppar1;

         // moments loads
         ppar1 = CreateMomentLoadTable(pBroker, spanIdx, gdrIdx, pDisplayUnits, level, m_bSimplifiedVersion);
         *pChapter <<ppar1;
      } // gdrIdx
   } // spanIdx

   return pChapter;
}


CChapterBuilder* CUserDefinedLoadsChapterBuilder::Clone() const
{
   return new CUserDefinedLoadsChapterBuilder;
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

rptParagraph* CUserDefinedLoadsChapterBuilder::CreatePointLoadTable(IBroker* pBroker,
                           SpanIndexType span, GirderIndexType girder,
                           IEAFDisplayUnits* pDisplayUnits,
                           Uint16 level, bool bSimplifiedVersion)
{
   rptParagraph* pParagraph = new rptParagraph();

   INIT_UV_PROTOTYPE( rptLengthUnitValue,    dim,   pDisplayUnits->GetSpanLengthUnit(),  false );
   INIT_UV_PROTOTYPE( rptForceUnitValue,   shear,   pDisplayUnits->GetShearUnit(), false );

   GET_IFACE2(pBroker,IBridge,pBridge);
   Float64 span_length = pBridge->GetSpanLength(span,girder);

   std::_tostringstream os;
   rptRcTable* table = pgsReportStyleHolder::CreateDefaultTable(5,_T("Point Loads"));

   (*table)(0,0)  << _T("Stage");
   (*table)(0,1)  << _T("Load") << rptNewLine << _T("Case");
   (*table)(0,2)  << COLHDR(_T("Location"),rptLengthUnitTag, pDisplayUnits->GetSpanLengthUnit() );
   (*table)(0,3)  << COLHDR(_T("Magnitude"),rptForceUnitTag, pDisplayUnits->GetShearUnit() );
   (*table)(0,4)  << _T("Description");

   GET_IFACE2(pBroker, IUserDefinedLoads, pUdl );

   bool loads_exist = false;
   RowIndexType row = table->GetNumberOfHeaderRows();
   for ( int i=0; i<3; i++ )
   {
      std::_tstring stagenm;
      pgsTypes::Stage stage;
      if (i==0)
      {
         stage = pgsTypes::BridgeSite1;
         stagenm = _T("Bridge Site 1");
      }
      else if (i==1)
      {
         stage = pgsTypes::BridgeSite2;
         stagenm = _T("Bridge Site 2");
      }
      else
      {
         stage = pgsTypes::BridgeSite3;
         stagenm = _T("Bridge Site 3");
      }

      const std::vector<IUserDefinedLoads::UserPointLoad>* ppl = pUdl->GetPointLoads(stage, span, girder);
      if (ppl!=0)
      {
         IndexType npl = ppl->size();

         for (IndexType ipl=0; ipl<npl; ipl++)
         {
            loads_exist = true;

            IUserDefinedLoads::UserPointLoad upl = ppl->at(ipl);

             std::_tstring strlcn;
             if (upl.m_LoadCase==IUserDefinedLoads::userDC)
                strlcn = _T("DC");
             else if (upl.m_LoadCase==IUserDefinedLoads::userDW)
                strlcn = _T("DW");
             else if (upl.m_LoadCase==IUserDefinedLoads::userLL_IM)
                strlcn = _T("LL+IM");
             else
                ATLASSERT(0);

            (*table)(row,0) << stagenm;
            (*table)(row,1) << strlcn;
            (*table)(row,2) << dim.SetValue( upl.m_Location );
            (*table)(row,3) << shear.SetValue( upl.m_Magnitude );
            (*table)(row,4) << upl.m_Description;

            row++;
         }
      }
   }

   if (loads_exist)
   {
      *pParagraph << table;
   }
   else
   {
      delete table;

      if (!bSimplifiedVersion)
         *pParagraph << _T("Point loads were not defined for this girder")<<rptNewLine;
   }

   return pParagraph;
}

rptParagraph* CUserDefinedLoadsChapterBuilder::CreateDistributedLoadTable(IBroker* pBroker,
                           SpanIndexType span, GirderIndexType girder,
                           IEAFDisplayUnits* pDisplayUnits,
                           Uint16 level, bool bSimplifiedVersion)
{
   rptParagraph* pParagraph = new rptParagraph();

   INIT_UV_PROTOTYPE( rptLengthUnitValue,    dim,   pDisplayUnits->GetSpanLengthUnit(),  false );
   INIT_UV_PROTOTYPE( rptForcePerLengthUnitValue,   fplu,   pDisplayUnits->GetForcePerLengthUnit(), false );

   GET_IFACE2(pBroker,IBridge,pBridge);
   Float64 span_length = pBridge->GetSpanLength(span,girder);

   rptRcTable* table = pgsReportStyleHolder::CreateDefaultTable(7,_T("Distributed Loads"));

   (*table)(0,0)  << _T("Stage");
   (*table)(0,1)  << _T("Load") << rptNewLine << _T("Case");
   (*table)(0,2)  << COLHDR(_T("Start") << rptNewLine << _T("Location"),rptLengthUnitTag, pDisplayUnits->GetSpanLengthUnit() );
   (*table)(0,3)  << COLHDR(_T("End") << rptNewLine << _T("Location"),rptLengthUnitTag, pDisplayUnits->GetSpanLengthUnit() );
   (*table)(0,4)  << COLHDR(_T("Start") << rptNewLine << _T("Magnitude"),rptForcePerLengthUnitTag, pDisplayUnits->GetForcePerLengthUnit() );
   (*table)(0,5)  << COLHDR(_T("End") << rptNewLine << _T("Magnitude"),rptForcePerLengthUnitTag, pDisplayUnits->GetForcePerLengthUnit() );
   (*table)(0,6)  << _T("Description");

   GET_IFACE2(pBroker, IUserDefinedLoads, pUdl );

   bool loads_exist = false;
   RowIndexType row = table->GetNumberOfHeaderRows();
   for ( int i=0; i<3; i++ )
   {
      std::_tstring stagenm;
      pgsTypes::Stage stage;
      if (i==0)
      {
         stage = pgsTypes::BridgeSite1;
         stagenm = _T("Bridge Site 1");
      }
      else if (i==1)
      {
         stage = pgsTypes::BridgeSite2;
         stagenm = _T("Bridge Site 2");
      }
      else
      {
         stage = pgsTypes::BridgeSite3;
         stagenm = _T("Bridge Site 3");
      }

      const std::vector<IUserDefinedLoads::UserDistributedLoad>* ppl = pUdl->GetDistributedLoads(stage, span, girder);
      if (ppl!=0)
      {
         IndexType npl = ppl->size();
         for (IndexType ipl=0; ipl<npl; ipl++)
         {
            loads_exist = true;

            IUserDefinedLoads::UserDistributedLoad upl = ppl->at(ipl);

             std::_tstring strlcn;
             if (upl.m_LoadCase==IUserDefinedLoads::userDC)
                strlcn = _T("DC");
             else if (upl.m_LoadCase==IUserDefinedLoads::userDW)
                strlcn = _T("DW");
             else if (upl.m_LoadCase==IUserDefinedLoads::userLL_IM)
                strlcn = _T("LL+IM");
             else
                ATLASSERT(0);

            (*table)(row,0) << stagenm;
            (*table)(row,1) << strlcn;
            (*table)(row,2) << dim.SetValue( upl.m_StartLocation );
            (*table)(row,3) << dim.SetValue( upl.m_EndLocation );
            (*table)(row,4) << fplu.SetValue( upl.m_WStart );
            (*table)(row,5) << fplu.SetValue( upl.m_WEnd );
            (*table)(row,6) << upl.m_Description;

            row++;
         }
      }
   }

   if (loads_exist)
   {
      *pParagraph << table;
   }
   else
   {
      delete table;

      if (! bSimplifiedVersion)
         *pParagraph << _T("Distributed loads were not defined for this girder")<<rptNewLine;
   }

   return pParagraph;
}


rptParagraph* CUserDefinedLoadsChapterBuilder::CreateMomentLoadTable(IBroker* pBroker,
                           SpanIndexType span, GirderIndexType girder,
                           IEAFDisplayUnits* pDisplayUnits,
                           Uint16 level, bool bSimplifiedVersion)
{
   rptParagraph* pParagraph = new rptParagraph();

   INIT_UV_PROTOTYPE( rptLengthUnitValue,    dim,   pDisplayUnits->GetSpanLengthUnit(),  false );
   INIT_UV_PROTOTYPE( rptMomentUnitValue,   moment,   pDisplayUnits->GetMomentUnit(), false );

   GET_IFACE2(pBroker,IBridge,pBridge);
   Float64 span_length = pBridge->GetSpanLength(span,girder);

   rptRcTable* table = pgsReportStyleHolder::CreateDefaultTable(5,_T("End Moments"));

   (*table)(0,0)  << _T("Stage");
   (*table)(0,1)  << _T("Load") << rptNewLine << _T("Case");
   (*table)(0,2)  << _T("Location");
   (*table)(0,3)  << COLHDR(_T("Magnitude"),rptMomentUnitTag, pDisplayUnits->GetMomentUnit() );
   (*table)(0,4)  << _T("Description");

   GET_IFACE2(pBroker, IUserDefinedLoads, pUdl );

   bool loads_exist = false;
   RowIndexType row = table->GetNumberOfHeaderRows();
   for ( int i=0; i<3; i++ )
   {
      std::_tstring stagenm;
      pgsTypes::Stage stage;
      if (i==0)
      {
         stage = pgsTypes::BridgeSite1;
         stagenm = _T("Bridge Site 1");
      }
      else if (i==1)
      {
         stage = pgsTypes::BridgeSite2;
         stagenm = _T("Bridge Site 2");
      }
      else
      {
         stage = pgsTypes::BridgeSite3;
         stagenm = _T("Bridge Site 3");
      }

      const std::vector<IUserDefinedLoads::UserMomentLoad>* ppl = pUdl->GetMomentLoads(stage, span, girder);
      if (ppl!=0)
      {
         IndexType npl = ppl->size();

         for (IndexType ipl=0; ipl<npl; ipl++)
         {
            loads_exist = true;

            IUserDefinedLoads::UserMomentLoad upl = ppl->at(ipl);

             std::_tstring strlcn;
             if (upl.m_LoadCase==IUserDefinedLoads::userDC)
                strlcn = _T("DC");
             else if (upl.m_LoadCase==IUserDefinedLoads::userDW)
                strlcn = _T("DW");
             else if (upl.m_LoadCase==IUserDefinedLoads::userLL_IM)
                strlcn = _T("LL+IM");
             else
                ATLASSERT(0);

            (*table)(row,0) << stagenm;
            (*table)(row,1) << strlcn;

            if ( IsZero(upl.m_Location) )
               (*table)(row,2) << _T("Start of span");
            else
               (*table)(row,2) << _T("End of span");

            (*table)(row,3) << moment.SetValue( upl.m_Magnitude );
            (*table)(row,4) << upl.m_Description;

            row++;
         }
      }
   }

   if (loads_exist)
   {
      *pParagraph << table;
   }
   else
   {
      delete table;

      if (!bSimplifiedVersion)
         *pParagraph << _T("Moment loads were not defined for this girder")<<rptNewLine;
   }

   return pParagraph;
}

