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

// RelaxationAtHaulingTable.cpp : Implementation of CRelaxationAtHaulingTable
#include "stdafx.h"
#include "RelaxationAtHaulingTable.h"
#include <IFace\Bridge.h>
#include <IFace\Project.h>
#include <PsgLib\SpecLibraryEntry.h>
#include <PgsExt\GirderData.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CRelaxationAtHaulingTable::CRelaxationAtHaulingTable(ColumnIndexType NumColumns, IDisplayUnits* pDispUnit) :
rptRcTable(NumColumns,0)
{
   DEFINE_UV_PROTOTYPE( spanloc,     pDispUnit->GetSpanLengthUnit(),      false );
   DEFINE_UV_PROTOTYPE( gdrloc,      pDispUnit->GetSpanLengthUnit(),      false );
   DEFINE_UV_PROTOTYPE( offset,      pDispUnit->GetSpanLengthUnit(),      false );
   DEFINE_UV_PROTOTYPE( mod_e,       pDispUnit->GetModEUnit(),            false );
   DEFINE_UV_PROTOTYPE( force,       pDispUnit->GetGeneralForceUnit(),    false );
   DEFINE_UV_PROTOTYPE( area,        pDispUnit->GetAreaUnit(),            false );
   DEFINE_UV_PROTOTYPE( mom_inertia, pDispUnit->GetMomentOfInertiaUnit(), false );
   DEFINE_UV_PROTOTYPE( ecc,         pDispUnit->GetComponentDimUnit(),    false );
   DEFINE_UV_PROTOTYPE( moment,      pDispUnit->GetMomentUnit(),          false );
   DEFINE_UV_PROTOTYPE( stress,      pDispUnit->GetStressUnit(),          false );
   DEFINE_UV_PROTOTYPE( time,        pDispUnit->GetLongTimeUnit(),        false );

   scalar.SetFormat( sysNumericFormatTool::Automatic );
   scalar.SetWidth(6);
   scalar.SetPrecision(2);
}

CRelaxationAtHaulingTable* CRelaxationAtHaulingTable::PrepareTable(rptChapter* pChapter,IBroker* pBroker,SpanIndexType span,GirderIndexType gdr,bool bTemporaryStrands,LOSSDETAILS& details,IDisplayUnits* pDispUnit,Uint16 level)
{
   GET_IFACE2(pBroker,IGirderData,pGirderData);
   CGirderData girderData = pGirderData->GetGirderData(span,gdr);

   GET_IFACE2(pBroker,ISpecification,pSpec);
   std::string strSpecName = pSpec->GetSpecification();

   GET_IFACE2(pBroker,ILibrary,pLib);
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( strSpecName.c_str() );

   // Create and configure the table
   ColumnIndexType numColumns = 7;
   if ( bTemporaryStrands )
      numColumns += 5;

   CRelaxationAtHaulingTable* table = new CRelaxationAtHaulingTable( numColumns, pDispUnit );
   table->m_bTemporaryStrands = bTemporaryStrands;

   pgsReportStyleHolder::ConfigureTable(table);

   std::string strImagePath(pgsReportStyleHolder::GetImagePath());
   
   rptParagraph* pParagraph = new rptParagraph(pgsReportStyleHolder::GetHeadingStyle());
   *pChapter << pParagraph;
   *pParagraph << "[5.9.5.4.2c] Relaxation of Prestressing Strands : " << symbol(DELTA) << Sub2("f","pR1H") << rptNewLine;

   pParagraph = new rptParagraph;
   *pChapter << pParagraph;
   *pParagraph << rptRcImage(strImagePath + "Delta_FpR1H.gif") << rptNewLine;

   table->stress.ShowUnitTag(true);
   table->time.ShowUnitTag(true);

   *pParagraph << Sub2("f","py") << " = " << table->stress.SetValue(details.RefinedLosses2005.GetFpy())        << rptNewLine;
   *pParagraph << Sub2("K'","L") << " = " << details.RefinedLosses2005.GetKL()                          << rptNewLine;
   *pParagraph << Sub2("t","i")  << " = " << table->time.SetValue(details.RefinedLosses2005.GetInitialAge())   << rptNewLine;
   *pParagraph << Sub2("t","h")  << " = " << table->time.SetValue(details.RefinedLosses2005.GetAgeAtHauling()) << rptNewLine;

   table->stress.ShowUnitTag(false);
   table->time.ShowUnitTag(false);

   *pParagraph << table << rptNewLine;
   (*table)(0,0) << COLHDR("Location from"<<rptNewLine<<"End of Girder",rptLengthUnitTag,  pDispUnit->GetSpanLengthUnit() );
   (*table)(0,1) << COLHDR("Location from"<<rptNewLine<<"Left Support",rptLengthUnitTag,  pDispUnit->GetSpanLengthUnit() );

   if ( bTemporaryStrands )
   {
      table->m_RowOffset = 1;
      table->SetNumberOfHeaderRows(table->m_RowOffset+1);

      table->SetRowSpan(0,0,2);
      table->SetRowSpan(0,1,2);
      table->SetRowSpan(1,0,-1);
      table->SetRowSpan(1,1,-1);

      table->SetColumnSpan(0,2,5);
      (*table)(0,2) << "Permanent Strands";

      table->SetColumnSpan(0,3,5);
      (*table)(0,3) << "Temporary Strands";

      table->SetColumnSpan(0,4,-1);
      table->SetColumnSpan(0,5,-1);
      table->SetColumnSpan(0,6,-1);
      table->SetColumnSpan(0,7,-1);
      table->SetColumnSpan(0,8,-1);
      table->SetColumnSpan(0,9,-1);
      table->SetColumnSpan(0,10,-1);
      table->SetColumnSpan(0,11,-1);

      (*table)(1,2) << COLHDR(Sub2("f","pt"), rptStressUnitTag, pDispUnit->GetStressUnit() );
      (*table)(1,3) << COLHDR(symbol(DELTA) << Sub2("f","pSRH"), rptStressUnitTag, pDispUnit->GetStressUnit() );
      (*table)(1,4) << COLHDR(symbol(DELTA) << Sub2("f","pCRH"), rptStressUnitTag, pDispUnit->GetStressUnit() );
      (*table)(1,5) << Sub2("K","ih");
      (*table)(1,6) << COLHDR(symbol(DELTA) << Sub2("f","pR1H"), rptStressUnitTag, pDispUnit->GetStressUnit() );

      (*table)(1,7) << COLHDR(Sub2("f","pt"), rptStressUnitTag, pDispUnit->GetStressUnit() );
      (*table)(1,8) << COLHDR(symbol(DELTA) << Sub2("f","pSRH"), rptStressUnitTag, pDispUnit->GetStressUnit() );
      (*table)(1,9) << COLHDR(symbol(DELTA) << Sub2("f","pCRH"), rptStressUnitTag, pDispUnit->GetStressUnit() );
      (*table)(1,10) << Sub2("K","ih");
      (*table)(1,11) << COLHDR(symbol(DELTA) << Sub2("f","pR1H"), rptStressUnitTag, pDispUnit->GetStressUnit() );
   }
   else
   {
      table->m_RowOffset = 0;
      (*table)(0,2) << COLHDR(Sub2("f","pt"), rptStressUnitTag, pDispUnit->GetStressUnit() );
      (*table)(0,3) << COLHDR(symbol(DELTA) << Sub2("f","pSRH"), rptStressUnitTag, pDispUnit->GetStressUnit() );
      (*table)(0,4) << COLHDR(symbol(DELTA) << Sub2("f","pCRH"), rptStressUnitTag, pDispUnit->GetStressUnit() );
      (*table)(0,5) << Sub2("K","ih");
      (*table)(0,6) << COLHDR(symbol(DELTA) << Sub2("f","pR1H"), rptStressUnitTag, pDispUnit->GetStressUnit() );
   }

   return table;
}

void CRelaxationAtHaulingTable::AddRow(rptChapter* pChapter,IBroker* pBroker,RowIndexType row,LOSSDETAILS& details,IDisplayUnits* pDispUnit,Uint16 level)
{
   ColumnIndexType col = 2;
   (*this)(row+m_RowOffset,col++) << stress.SetValue(details.RefinedLosses2005.GetPermanentStrandFpt());
   (*this)(row+m_RowOffset,col++) << stress.SetValue(details.RefinedLosses2005.PermanentStrand_ShrinkageLossAtShipping());
   (*this)(row+m_RowOffset,col++) << stress.SetValue(details.RefinedLosses2005.PermanentStrand_CreepLossAtShipping());
   (*this)(row+m_RowOffset,col++) << scalar.SetValue(details.RefinedLosses2005.GetPermanentStrandKih());
   (*this)(row+m_RowOffset,col++) << stress.SetValue(details.RefinedLosses2005.PermanentStrand_RelaxationLossAtShipping());

   if ( m_bTemporaryStrands )
   {
      (*this)(row+m_RowOffset,col++) << stress.SetValue(details.RefinedLosses2005.GetTemporaryStrandFpt());
      (*this)(row+m_RowOffset,col++) << stress.SetValue(details.RefinedLosses2005.TemporaryStrand_ShrinkageLossAtShipping());
      (*this)(row+m_RowOffset,col++) << stress.SetValue(details.RefinedLosses2005.TemporaryStrand_CreepLossAtShipping());
      (*this)(row+m_RowOffset,col++) << scalar.SetValue(details.RefinedLosses2005.GetTemporaryStrandKih());
      (*this)(row+m_RowOffset,col++) << stress.SetValue(details.RefinedLosses2005.TemporaryStrand_RelaxationLossAtShipping());
   }
}
