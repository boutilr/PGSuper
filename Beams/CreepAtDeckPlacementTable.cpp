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

// CreepAtDeckPlacementTable.cpp : Implementation of CCreepAtDeckPlacementTable
#include "stdafx.h"
#include "CreepAtDeckPlacementTable.h"
#include <IFace\Bridge.h>
#include <IFace\Project.h>
#include <PsgLib\SpecLibraryEntry.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CCreepAtDeckPlacementTable::CCreepAtDeckPlacementTable(ColumnIndexType NumColumns, IEAFDisplayUnits* pDisplayUnits) :
rptRcTable(NumColumns,0)
{
   DEFINE_UV_PROTOTYPE( spanloc,     pDisplayUnits->GetSpanLengthUnit(),      false );
   DEFINE_UV_PROTOTYPE( gdrloc,      pDisplayUnits->GetSpanLengthUnit(),      false );
   DEFINE_UV_PROTOTYPE( offset,      pDisplayUnits->GetSpanLengthUnit(),      false );
   DEFINE_UV_PROTOTYPE( mod_e,       pDisplayUnits->GetModEUnit(),            false );
   DEFINE_UV_PROTOTYPE( force,       pDisplayUnits->GetGeneralForceUnit(),    false );
   DEFINE_UV_PROTOTYPE( area,        pDisplayUnits->GetAreaUnit(),            false );
   DEFINE_UV_PROTOTYPE( mom_inertia, pDisplayUnits->GetMomentOfInertiaUnit(), false );
   DEFINE_UV_PROTOTYPE( ecc,         pDisplayUnits->GetComponentDimUnit(),    false );
   DEFINE_UV_PROTOTYPE( moment,      pDisplayUnits->GetMomentUnit(),          false );
   DEFINE_UV_PROTOTYPE( stress,      pDisplayUnits->GetStressUnit(),          false );
   DEFINE_UV_PROTOTYPE( time,        pDisplayUnits->GetWholeDaysUnit(),        false );

   scalar.SetFormat( sysNumericFormatTool::Automatic );
   scalar.SetWidth(6);
   scalar.SetPrecision(3);
}

CCreepAtDeckPlacementTable* CCreepAtDeckPlacementTable::PrepareTable(rptChapter* pChapter,IBroker* pBroker,const CSegmentKey& segmentKey,IEAFDisplayUnits* pDisplayUnits,Uint16 level)
{
   GET_IFACE2(pBroker,ISegmentData,pSegmentData);
   const CStrandData* pStrands = pSegmentData->GetStrandData(segmentKey);

   std::_tstring strImagePath(rptStyleManager::GetImagePath());

   // Create and configure the table
   ColumnIndexType numColumns = 8;

   if (pStrands->GetTemporaryStrandUsage() != pgsTypes::ttsPretensioned)
   {
      numColumns += 2;
   }

   CCreepAtDeckPlacementTable* table = new CCreepAtDeckPlacementTable( numColumns, pDisplayUnits );
   rptStyleManager::ConfigureTable(table);


   table->m_pStrands = pStrands;

   rptParagraph* pParagraph = new rptParagraph(rptStyleManager::GetHeadingStyle());
   *pChapter << pParagraph;
   *pParagraph << _T("[")<< LrfdCw8th(_T("5.9.5.4.2b"),_T("5.9.3.4.2b")) <<_T("] Creep of Girder Concrete : ") << symbol(DELTA) << RPT_STRESS(_T("pCR")) << rptNewLine;

   if (pStrands->GetTemporaryStrandUsage() != pgsTypes::ttsPretensioned)
   {
      *pParagraph << rptRcImage(strImagePath + _T("Delta_FpCR_PT.png")) << rptNewLine;
   }
   else
   {
      *pParagraph << rptRcImage(strImagePath + _T("Delta_FpCR.png")) << rptNewLine;
   }

   // creep loss 
   ColumnIndexType col = 0;
   *pParagraph << table << rptNewLine;
   (*table)(0, col++) << COLHDR(_T("Location from")<<rptNewLine<<_T("Left Support"),rptLengthUnitTag,  pDisplayUnits->GetSpanLengthUnit() );

   if (pStrands->GetTemporaryStrandUsage() == pgsTypes::ttsPretensioned)
   {
      (*table)(0, col++) << COLHDR(RPT_STRESS(_T("cgp")), rptStressUnitTag, pDisplayUnits->GetStressUnit());
   }
   else
   {
      (*table)(0, col++) << COLHDR(RPT_STRESS(_T("cgp")), rptStressUnitTag, pDisplayUnits->GetStressUnit());
      (*table)(0, col++) << COLHDR(symbol(DELTA) << RPT_STRESS(_T("pp")), rptStressUnitTag, pDisplayUnits->GetStressUnit());
      (*table)(0, col++) << COLHDR(RPT_STRESS(_T("cgp")) << _T(" + ") << symbol(DELTA) << RPT_STRESS(_T("pp")), rptStressUnitTag, pDisplayUnits->GetStressUnit());
   }

   (*table)(0, col++) << Sub2(_T("k"),_T("td"));
   (*table)(0, col++) << COLHDR(Sub2(_T("t"),_T("i")),rptTimeUnitTag,pDisplayUnits->GetWholeDaysUnit());
   (*table)(0, col++) << COLHDR(Sub2(_T("t"),_T("d")),rptTimeUnitTag,pDisplayUnits->GetWholeDaysUnit());
   (*table)(0, col++) << Sub2(symbol(psi),_T("b")) << _T("(") << Sub2(_T("t"),_T("d")) << _T(",") << Sub2(_T("t"),_T("i")) << _T(")");
   (*table)(0, col++) << Sub2(_T("K"),_T("id"));
   (*table)(0, col++) << COLHDR(symbol(DELTA) << RPT_STRESS(_T("pCR")), rptStressUnitTag, pDisplayUnits->GetStressUnit() );

   return table;
}

void CCreepAtDeckPlacementTable::AddRow(rptChapter* pChapter,IBroker* pBroker,const pgsPointOfInterest& poi,RowIndexType row,const LOSSDETAILS* pDetails,IEAFDisplayUnits* pDisplayUnits,Uint16 level)
{
  // Typecast to our known type (eating own doggy food)
   std::shared_ptr<const lrfdRefinedLosses2005> ptl = std::dynamic_pointer_cast<const lrfdRefinedLosses2005>(pDetails->pLosses);
   if (!ptl)
   {
      ATLASSERT(false); // made a bad cast? Bail...
      return;
   }

   ColumnIndexType col = 1;
   RowIndexType rowOffset = GetNumberOfHeaderRows() - 1;

   if (m_pStrands->GetTemporaryStrandUsage() == pgsTypes::ttsPretensioned)
   {
      (*this)(row+rowOffset, col++) << stress.SetValue(pDetails->pLosses->ElasticShortening().PermanentStrand_Fcgp());
   }
   else
   {
      (*this)(row+rowOffset, col++) << stress.SetValue(pDetails->pLosses->ElasticShortening().PermanentStrand_Fcgp());
      (*this)(row+rowOffset, col++) << stress.SetValue(ptl->GetDeltaFpp());
      (*this)(row+rowOffset, col++) << stress.SetValue(pDetails->pLosses->ElasticShortening().PermanentStrand_Fcgp() + ptl->GetDeltaFpp());
   }

   (*this)(row+rowOffset, col++) << scalar.SetValue(ptl->GetCreepInitialToDeck().GetKtd());
   (*this)(row+rowOffset, col++) << time.SetValue(ptl->GetInitialAge());
   (*this)(row+rowOffset, col++) << time.SetValue(ptl->GetAgeAtDeckPlacement());
   (*this)(row+rowOffset, col++) << scalar.SetValue(ptl->GetCreepInitialToDeck().GetCreepCoefficient());
   (*this)(row+rowOffset, col++) << scalar.SetValue(ptl->GetKid());
   (*this)(row+rowOffset, col++) << stress.SetValue(ptl->CreepLossBeforeDeckPlacement() );
}
