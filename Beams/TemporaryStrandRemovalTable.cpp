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

// TemporaryStrandRemovalTable.cpp : Implementation of CTemporaryStrandRemovalTable
#include "stdafx.h"
#include "TemporaryStrandRemovalTable.h"
#include <IFace\Bridge.h>
#include <IFace\Project.h>
#include <PsgLib\SpecLibraryEntry.h>
#include <PgsExt\GirderData.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CTemporaryStrandRemovalTable::CTemporaryStrandRemovalTable(ColumnIndexType NumColumns, IEAFDisplayUnits* pDisplayUnits) :
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
}

CTemporaryStrandRemovalTable* CTemporaryStrandRemovalTable::PrepareTable(rptChapter* pChapter,IBroker* pBroker,SpanIndexType span,GirderIndexType gdr,IEAFDisplayUnits* pDisplayUnits,Uint16 level)
{
   // Create and configure the table
   ColumnIndexType numColumns = 7;
   CTemporaryStrandRemovalTable* table = new CTemporaryStrandRemovalTable( numColumns, pDisplayUnits );
   pgsReportStyleHolder::ConfigureTable(table);

   GET_IFACE2(pBroker,IBridgeMaterial,pMaterial);
   double Ec  = pMaterial->GetEcGdr(span,gdr);
   double Ep  = pMaterial->GetStrand(span,gdr,pgsTypes::Temporary)->GetE();

   GET_IFACE2(pBroker,IStrandGeometry,pStrandGeom);
   double nEffectiveStrands;
   double ept = pStrandGeom->GetTempEccentricity( pgsPointOfInterest(span,gdr,0), &nEffectiveStrands);
   double Apt = pStrandGeom->GetStrandArea(span,gdr,pgsTypes::Temporary);


   GET_IFACE2(pBroker,IGirderData,pGirderData);
   CGirderData girderData = pGirderData->GetGirderData(span,gdr);

   std::string strImagePath(pgsReportStyleHolder::GetImagePath());

   ///////////////////////////////////////////////////////////////////////////////////////
   // Change in stress due to removal of temporary strands
   rptParagraph* pParagraph = new rptParagraph(pgsReportStyleHolder::GetHeadingStyle());
   *pChapter << pParagraph;
   *pParagraph << "Effect of temporary strand removal on permanent strands" << rptNewLine;

   pParagraph = new rptParagraph;
   *pChapter << pParagraph;

   GET_IFACE2(pBroker,ISpecification,pSpec);
   std::string strSpecName = pSpec->GetSpecification();

   GET_IFACE2(pBroker,ILibrary,pLib);
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( strSpecName.c_str() );

   int method = pSpecEntry->GetLossMethod();
   bool bIgnoreInitialRelaxation = ( method == LOSSES_WSDOT_REFINED || method == LOSSES_WSDOT_LUMPSUM ) ? false : true;
   
   if ( girderData.TempStrandUsage == pgsTypes::ttsPretensioned ) 
   {
      if ( bIgnoreInitialRelaxation )
         *pParagraph << rptRcImage(strImagePath + "Ptr_LRFD.png") << rptNewLine;
      else
         *pParagraph << rptRcImage(strImagePath + "Ptr_WSDOT.png") << rptNewLine;
   }
   else if (girderData.TempStrandUsage == pgsTypes::ttsPTBeforeShipping )
   {
      *pParagraph << rptRcImage(strImagePath + "Ptr_PTBeforeShipping.png") << rptNewLine;
   }
   else
   {
      *pParagraph << rptRcImage(strImagePath + "Ptr_PT.png") << rptNewLine;
   }

   *pParagraph << rptRcImage(strImagePath + "Delta_Fptr.png") << rptNewLine;

   table->mod_e.ShowUnitTag(true);
   table->ecc.ShowUnitTag(true);
   table->area.ShowUnitTag(true);

   *pParagraph << Sub2("E","p") << " = " << table->mod_e.SetValue(Ep) << rptNewLine;
   *pParagraph << Sub2("E","c") << " = " << table->mod_e.SetValue(Ec) << rptNewLine;
   *pParagraph << Sub2("A","pt") << " = " << table->area.SetValue(Apt) << rptNewLine;
   *pParagraph << Sub2("e","pt") << " = " << table->ecc.SetValue(ept) << rptNewLine;

   table->mod_e.ShowUnitTag(false);
   table->ecc.ShowUnitTag(false);
   table->area.ShowUnitTag(false);

   *pParagraph << table << rptNewLine;

   (*table)(0,0) << COLHDR("Location from"<<rptNewLine<<"Left Support",rptLengthUnitTag,  pDisplayUnits->GetSpanLengthUnit() );
   (*table)(0,1) << COLHDR(Sub2("P","tr"),rptForceUnitTag,pDisplayUnits->GetGeneralForceUnit());
   (*table)(0,2) << COLHDR(Sub2("A","g"), rptAreaUnitTag, pDisplayUnits->GetAreaUnit());
   (*table)(0,3) << COLHDR(Sub2("I","g"), rptLength4UnitTag, pDisplayUnits->GetMomentOfInertiaUnit());
   (*table)(0,4) << COLHDR(Sub2("e","perm"), rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit());
   (*table)(0,5) << COLHDR(RPT_STRESS("ptr"), rptStressUnitTag, pDisplayUnits->GetStressUnit() );
   (*table)(0,6) << COLHDR(symbol(DELTA) << RPT_STRESS("ptr"), rptStressUnitTag, pDisplayUnits->GetStressUnit() );

   return table;
}

void CTemporaryStrandRemovalTable::AddRow(rptChapter* pChapter,IBroker* pBroker,RowIndexType row,LOSSDETAILS& details,IEAFDisplayUnits* pDisplayUnits,Uint16 level)
{
//   (*this)(row,0) << spanloc.SetValue(poi,end_size);
   (*this)(row,1) << force.SetValue(details.pLosses->GetPtr());
   (*this)(row,2) << area.SetValue(details.pLosses->GetAg());
   (*this)(row,3) << mom_inertia.SetValue(details.pLosses->GetIg());
   (*this)(row,4) << ecc.SetValue(details.pLosses->GetEccPermanent());
   (*this)(row,5) << stress.SetValue( details.pLosses->GetFptr() );
   (*this)(row,6) << stress.SetValue( details.pLosses->GetDeltaFptr() );
}
