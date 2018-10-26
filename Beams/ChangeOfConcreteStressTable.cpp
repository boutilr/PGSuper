///////////////////////////////////////////////////////////////////////
// PGSuper - Prestressed Girder SUPERstructure Design and Analysis
// Copyright � 1999-2010  Washington State Department of Transportation
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

// ChangeOfConcreteStressTable.cpp : Implementation of CChangeOfConcreteStressTable
#include "stdafx.h"
#include "ChangeOfConcreteStressTable.h"
#include <IFace\Bridge.h>
#include <IFace\Project.h>
#include <PsgLib\SpecLibraryEntry.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CChangeOfConcreteStressTable::CChangeOfConcreteStressTable(ColumnIndexType NumColumns, IDisplayUnits* pDisplayUnits) :
rptRcTable(NumColumns,0)
{
   DEFINE_UV_PROTOTYPE( spanloc,     pDisplayUnits->GetSpanLengthUnit(),      false );
   DEFINE_UV_PROTOTYPE( gdrloc,      pDisplayUnits->GetSpanLengthUnit(),      false );
   DEFINE_UV_PROTOTYPE( mom_inertia, pDisplayUnits->GetMomentOfInertiaUnit(), false );
   DEFINE_UV_PROTOTYPE( dim,         pDisplayUnits->GetComponentDimUnit(),    false );
   DEFINE_UV_PROTOTYPE( moment,      pDisplayUnits->GetMomentUnit(),          false );
   DEFINE_UV_PROTOTYPE( stress,      pDisplayUnits->GetStressUnit(),          false );
}

CChangeOfConcreteStressTable* CChangeOfConcreteStressTable::PrepareTable(rptChapter* pChapter,IBroker* pBroker,SpanIndexType span,GirderIndexType gdr,IDisplayUnits* pDisplayUnits,Uint16 level)
{
   // Create and configure the table
   ColumnIndexType numColumns = 9;

   CChangeOfConcreteStressTable* table = new CChangeOfConcreteStressTable( numColumns, pDisplayUnits );
   pgsReportStyleHolder::ConfigureTable(table);


   std::string strImagePath(pgsReportStyleHolder::GetImagePath());


   rptParagraph* pParagraph = new rptParagraph(pgsReportStyleHolder::GetHeadingStyle());
   *pChapter << pParagraph;
   *pParagraph << "Change in Concrete Stress at Level of Prestressing [5.9.5.4.3]" << rptNewLine;

   pParagraph = new rptParagraph;
   *pChapter << pParagraph;

   *pParagraph << rptRcImage(strImagePath + "Delta Fcdp Equation.jpg") << rptNewLine;

   *pParagraph << table << rptNewLine;

   (*table)(0,0) << COLHDR("Location from"<<rptNewLine<<"Left Support",rptLengthUnitTag,  pDisplayUnits->GetSpanLengthUnit() );
   (*table)(0,1) << COLHDR(Sub2("M","adl"),  rptMomentUnitTag, pDisplayUnits->GetMomentUnit() );
   (*table)(0,2) << COLHDR(Sub2("M","sidl"), rptMomentUnitTag, pDisplayUnits->GetMomentUnit() );
   (*table)(0,3) << COLHDR(Sub2("e","ps"),   rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit() );
   (*table)(0,4) << COLHDR(Sub2("Y","bg"),   rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit() );
   (*table)(0,5) << COLHDR(Sub2("Y","bc"),   rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit() );
   (*table)(0,6) << COLHDR(Sub2("I","g"),    rptLength4UnitTag,pDisplayUnits->GetMomentOfInertiaUnit() );
   (*table)(0,7) << COLHDR(Sub2("I","c"),    rptLength4UnitTag,pDisplayUnits->GetMomentOfInertiaUnit() );
   (*table)(0,8) << COLHDR(symbol(DELTA) << Sub2("f","cdp"), rptStressUnitTag, pDisplayUnits->GetStressUnit() );

   pParagraph = new rptParagraph(pgsReportStyleHolder::GetFootnoteStyle());
   *pChapter << pParagraph;
   *pParagraph << Sub2("M","adl") << " = Moment due to permanent loads applied to the noncomposite girder section after the prestressing force is applied" << rptNewLine;
   *pParagraph << Sub2("M","sidl") << " = Moment due to permanent loads applied to the composite girder section after the prestressing force is applied" << rptNewLine;
   
   return table;
}

void CChangeOfConcreteStressTable::AddRow(rptChapter* pChapter,IBroker* pBroker,RowIndexType row,LOSSDETAILS& details,IDisplayUnits* pDisplayUnits,Uint16 level)
{
   (*this)(row,1) << moment.SetValue( details.pLosses->GetAddlGdrMoment() );
   (*this)(row,2) << moment.SetValue( details.pLosses->GetSidlMoment() );
   (*this)(row,3) << dim.SetValue( details.pLosses->GetEccPermanent() );
   (*this)(row,4) << dim.SetValue( details.pLosses->GetYbg() );
   (*this)(row,5) << dim.SetValue( details.pLosses->GetYbc() );
   (*this)(row,6) << mom_inertia.SetValue( details.pLosses->GetIg() );
   (*this)(row,7) << mom_inertia.SetValue( details.pLosses->GetIc() );
   (*this)(row,8) << stress.SetValue( -details.pLosses->GetDeltaFcd1() );
}
