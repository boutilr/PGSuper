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

// PostTensionTimeDependentLossesAtShippingTable.cpp : Implementation of CPostTensionTimeDependentLossesAtShippingTable
#include "stdafx.h"
#include "PostTensionTimeDependentLossesAtShippingTable.h"
#include <IFace\Bridge.h>
#include <IFace\Project.h>
#include <PsgLib\SpecLibraryEntry.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CPostTensionTimeDependentLossesAtShippingTable::CPostTensionTimeDependentLossesAtShippingTable(ColumnIndexType NumColumns, IDisplayUnits* pDispUnit) :
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
}

CPostTensionTimeDependentLossesAtShippingTable* CPostTensionTimeDependentLossesAtShippingTable::PrepareTable(rptChapter* pChapter,IBroker* pBroker,SpanIndexType span,GirderIndexType gdr,IDisplayUnits* pDispUnit,Uint16 level)
{
   std::string strImagePath(pgsReportStyleHolder::GetImagePath());

   CPostTensionTimeDependentLossesAtShippingTable* table = NULL;

   GET_IFACE2(pBroker,IGirderData,pGirderData);
   CGirderData girderData = pGirderData->GetGirderData(span,gdr);

   if ( girderData.TempStrandUsage == pgsTypes::ttsPTBeforeLifting ||
        girderData.TempStrandUsage == pgsTypes::ttsPTAfterLifting 
      ) 
   {
      ColumnIndexType numColumns = 7;
      table = new CPostTensionTimeDependentLossesAtShippingTable( numColumns, pDispUnit );
      pgsReportStyleHolder::ConfigureTable(table);

      table->m_GirderData = girderData;

      rptParagraph* pParagraph = new rptParagraph(pgsReportStyleHolder::GetHeadingStyle());
      *pChapter << pParagraph;

      *pParagraph << "Post-Tensioned Temporary Strands" << rptNewLine;
      
      pParagraph = new rptParagraph;
      *pChapter << pParagraph;

      *pParagraph << table << rptNewLine;

      (*table)(0,0) << COLHDR("Location from"<<rptNewLine<<"End of Girder",rptLengthUnitTag,  pDispUnit->GetSpanLengthUnit() );
      (*table)(0,1) << COLHDR("Location from"<<rptNewLine<<"Left Support",rptLengthUnitTag,  pDispUnit->GetSpanLengthUnit() );
      (*table)(0,2) << COLHDR(symbol(DELTA) << Sub2("f","pF"), rptStressUnitTag, pDispUnit->GetStressUnit() );
      (*table)(0,3) << COLHDR(symbol(DELTA) << Sub2("f","pA"), rptStressUnitTag, pDispUnit->GetStressUnit() );
      (*table)(0,4) << COLHDR(symbol(DELTA) << Sub2("f","pt avg"), rptStressUnitTag, pDispUnit->GetStressUnit() );
      (*table)(0,5) << COLHDR(symbol(DELTA) << Sub2("f","pLTH"), rptStressUnitTag, pDispUnit->GetStressUnit() );
      (*table)(0,6) << COLHDR(symbol(DELTA) << Sub2("f","ptH"), rptStressUnitTag, pDispUnit->GetStressUnit() );

      *pParagraph << rptRcImage(strImagePath + "PTLossAtHauling.gif") << rptNewLine;
   }

   return table;
}

void CPostTensionTimeDependentLossesAtShippingTable::AddRow(rptChapter* pChapter,IBroker* pBroker,RowIndexType row,LOSSDETAILS& details,IDisplayUnits* pDispUnit,Uint16 level)
{
   (*this)(row,2) << stress.SetValue(details.pLosses->FrictionLoss());
   (*this)(row,3) << stress.SetValue(details.pLosses->AnchorSetLoss());
   (*this)(row,4) << stress.SetValue(details.pLosses->GetDeltaFptAvg());
   (*this)(row,5) << stress.SetValue(details.pLosses->TemporaryStrand_TimeDependentLossesAtShipping() );
   (*this)(row,6) << stress.SetValue(details.pLosses->TemporaryStrand_AtShipping());
}
