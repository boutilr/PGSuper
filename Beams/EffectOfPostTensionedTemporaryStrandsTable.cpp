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

// EffectOfPostTensionedTemporaryStrandsTable.cpp : Implementation of CEffectOfPostTensionedTemporaryStrandsTable
#include "stdafx.h"
#include "EffectOfPostTensionedTemporaryStrandsTable.h"
#include <IFace\Bridge.h>
#include <IFace\Project.h>
#include <PsgLib\SpecLibraryEntry.h>
#include <PgsExt\GirderData.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CEffectOfPostTensionedTemporaryStrandsTable::CEffectOfPostTensionedTemporaryStrandsTable(ColumnIndexType NumColumns, IEAFDisplayUnits* pDisplayUnits) :
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

CEffectOfPostTensionedTemporaryStrandsTable* CEffectOfPostTensionedTemporaryStrandsTable::PrepareTable(rptChapter* pChapter,IBroker* pBroker,SpanIndexType span,GirderIndexType gdr,IEAFDisplayUnits* pDisplayUnits,Uint16 level)
{
   // create and configure the table object
   ColumnIndexType numColumns = 10;
   CEffectOfPostTensionedTemporaryStrandsTable* table = new CEffectOfPostTensionedTemporaryStrandsTable( numColumns, pDisplayUnits );
   pgsReportStyleHolder::ConfigureTable(table);

   std::_tstring strImagePath(pgsReportStyleHolder::GetImagePath());

   GET_IFACE2(pBroker,IGirderData,pGirderData);
   CGirderData girderData = pGirderData->GetGirderData(span,gdr);
   pgsTypes::TTSUsage tempStrandUsage = girderData.TempStrandUsage;

   // gather some data
   GET_IFACE2(pBroker,IBridgeMaterial,pMaterial);
   double Eci = (tempStrandUsage == pgsTypes::ttsPTBeforeShipping ? pMaterial->GetEcGdr(span,gdr) : pMaterial->GetEciGdr(span,gdr));
   double Ep  = pMaterial->GetStrand(span,gdr,pgsTypes::Temporary)->GetE();

   GET_IFACE2(pBroker,IStrandGeometry,pStrandGeom);
   double nEffectiveStrands;
   double ept = pStrandGeom->GetTempEccentricity( pgsPointOfInterest(span,gdr,0), &nEffectiveStrands);
   double Apt = pStrandGeom->GetStrandArea(span,gdr,pgsTypes::Temporary);
   StrandIndexType Npt = pStrandGeom->GetNumStrands(span,gdr,pgsTypes::Temporary);

   // add content to chapter
   rptParagraph* pParagraph = new rptParagraph(pgsReportStyleHolder::GetHeadingStyle());
   *pChapter << pParagraph;
   *pParagraph << _T("Effect of post-tensioned strands on permanent strands") << rptNewLine;

   pParagraph = new rptParagraph;
   *pChapter << pParagraph;

   if ( tempStrandUsage == pgsTypes::ttsPTBeforeShipping )
      *pParagraph << rptRcImage(strImagePath + _T("Delta_Fpp_BeforeShipping.png")) << rptNewLine;
   else
      *pParagraph << rptRcImage(strImagePath + _T("Delta_Fpp.png")) << rptNewLine;

   table->mod_e.ShowUnitTag(true);
   table->ecc.ShowUnitTag(true);
   table->area.ShowUnitTag(true);

   *pParagraph << Sub2(_T("E"),_T("p")) << _T(" = ") << table->mod_e.SetValue(Ep) << rptNewLine;

   if ( tempStrandUsage == pgsTypes::ttsPTBeforeShipping )
      *pParagraph << Sub2(_T("E"),_T("c")) << _T(" = ") << table->mod_e.SetValue(Eci) << rptNewLine;
   else
      *pParagraph << Sub2(_T("E"),_T("ci")) << _T(" = ") << table->mod_e.SetValue(Eci) << rptNewLine;

   *pParagraph << Sub2(_T("e"),_T("pt")) << _T(" = ") << table->ecc.SetValue(ept) << rptNewLine;
   *pParagraph << Sub2(_T("N"),_T("pt")) << _T(" = ") << Npt << rptNewLine;
   *pParagraph << Sub2(_T("A"),_T("pt")) << _T(" = ") << table->area.SetValue(Apt) << rptNewLine;

   table->mod_e.ShowUnitTag(false);
   table->ecc.ShowUnitTag(false);
   table->area.ShowUnitTag(false);

   *pParagraph << table << rptNewLine;

   (*table)(0,0) << COLHDR(_T("Location from")<<rptNewLine<<_T("End of Girder"),rptLengthUnitTag,  pDisplayUnits->GetSpanLengthUnit() );
   (*table)(0,1) << COLHDR(_T("Location from")<<rptNewLine<<_T("Left Support"),rptLengthUnitTag,  pDisplayUnits->GetSpanLengthUnit() );
   (*table)(0,2) << COLHDR(_T("x"),rptLengthUnitTag,pDisplayUnits->GetSpanLengthUnit());
   (*table)(0,3) << COLHDR(RPT_STRESS(_T("pt max")), rptStressUnitTag, pDisplayUnits->GetStressUnit() );
   (*table)(0,4) << COLHDR(RPT_STRESS(_T("pt min")), rptStressUnitTag, pDisplayUnits->GetStressUnit() );
   (*table)(0,5) << COLHDR(RPT_STRESS(_T("pt avg")), rptStressUnitTag, pDisplayUnits->GetStressUnit() );
   (*table)(0,6) << COLHDR(Sub2(_T("P"),_T("pt avg")), rptForceUnitTag, pDisplayUnits->GetGeneralForceUnit() );
   (*table)(0,7) << COLHDR(Sub2(_T("e"),_T("perm")), rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit() );
   (*table)(0,8) << COLHDR(RPT_STRESS(_T("cgpp")), rptStressUnitTag, pDisplayUnits->GetStressUnit() );
   (*table)(0,9) << COLHDR(symbol(DELTA) << RPT_STRESS(_T("pp")), rptStressUnitTag, pDisplayUnits->GetStressUnit() );

   return table;
}

void CEffectOfPostTensionedTemporaryStrandsTable::AddRow(rptChapter* pChapter,IBroker* pBroker,RowIndexType row,LOSSDETAILS& details,IEAFDisplayUnits* pDisplayUnits,Uint16 level)
{
   (*this)(row,2) << offset.SetValue( details.pLosses->GetLocation() );
   (*this)(row,3) << stress.SetValue( details.pLosses->GetFptMax() );
   (*this)(row,4) << stress.SetValue( details.pLosses->GetFptMin() );
   (*this)(row,5) << stress.SetValue( details.pLosses->GetFptAvg() );
   (*this)(row,6) << force.SetValue( details.pLosses->GetPptAvg() );
   (*this)(row,7) << ecc.SetValue( details.pLosses->GetEccPermanent() );
   (*this)(row,8) << stress.SetValue( details.pLosses->GetFcgpp() );
   (*this)(row,9) << stress.SetValue( details.pLosses->GetDeltaFpp() );
}
