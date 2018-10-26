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

// FinalPrestressLossTable.cpp : Implementation of CFinalPrestressLossTable
#include "stdafx.h"
#include "FinalPrestressLossTable.h"
#include <IFace\Bridge.h>
#include <IFace\Project.h>
#include <PsgLib\SpecLibraryEntry.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CFinalPrestressLossTable::CFinalPrestressLossTable(ColumnIndexType NumColumns, IEAFDisplayUnits* pDisplayUnits) :
rptRcTable(NumColumns,0)
{
   DEFINE_UV_PROTOTYPE( spanloc,     pDisplayUnits->GetSpanLengthUnit(),      false );
   DEFINE_UV_PROTOTYPE( gdrloc,      pDisplayUnits->GetSpanLengthUnit(),      false );
   DEFINE_UV_PROTOTYPE( cg,          pDisplayUnits->GetSpanLengthUnit(),      false );
   DEFINE_UV_PROTOTYPE( mod_e,       pDisplayUnits->GetModEUnit(),            false );
   DEFINE_UV_PROTOTYPE( force,       pDisplayUnits->GetGeneralForceUnit(),    false );
   DEFINE_UV_PROTOTYPE( area,        pDisplayUnits->GetAreaUnit(),            false );
   DEFINE_UV_PROTOTYPE( mom_inertia, pDisplayUnits->GetMomentOfInertiaUnit(), false );
   DEFINE_UV_PROTOTYPE( ecc,         pDisplayUnits->GetComponentDimUnit(),    false );
   DEFINE_UV_PROTOTYPE( moment,      pDisplayUnits->GetMomentUnit(),          false );
   DEFINE_UV_PROTOTYPE( stress,      pDisplayUnits->GetStressUnit(),          false );
}

CFinalPrestressLossTable* CFinalPrestressLossTable::PrepareTable(rptChapter* pChapter,IBroker* pBroker,SpanIndexType span,GirderIndexType gdr,IEAFDisplayUnits* pDisplayUnits,Uint16 level)
{
   GET_IFACE2(pBroker,ISpecification,pSpec);
   std::string strSpecName = pSpec->GetSpecification();

   GET_IFACE2(pBroker,ILibrary,pLib);
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( strSpecName.c_str() );

   bool bIgnoreElasticGain = ( pSpecEntry->GetSpecificationType() <= lrfdVersionMgr::ThirdEdition2004 ) ? true : false;
   
   // Create and configure the table
   ColumnIndexType numColumns = 8;

   GET_IFACE2(pBroker,IStrandGeometry,pStrandGeom);
   StrandIndexType NtMax = pStrandGeom->GetMaxStrands(span,gdr,pgsTypes::Temporary);

   GET_IFACE2(pBroker,IGirderData,pGirderData);
   CGirderData girderData = pGirderData->GetGirderData(span,gdr);
   if ( 0 < NtMax )
   {
      if ( girderData.TempStrandUsage == pgsTypes::ttsPretensioned ) 
      {
         numColumns++;
      }
      else
      {
         numColumns += 2;
      }
   }

   if ( bIgnoreElasticGain )
      numColumns--;

   CFinalPrestressLossTable* table = new CFinalPrestressLossTable( numColumns, pDisplayUnits );
   pgsReportStyleHolder::ConfigureTable(table);

   std::string strImagePath(pgsReportStyleHolder::GetImagePath());

   rptParagraph* pParagraph = new rptParagraph(pgsReportStyleHolder::GetHeadingStyle());
   *pChapter << pParagraph;

   std::string strYear = (bIgnoreElasticGain ? "" : "_2005");
   if ( 0 < NtMax )
   {
      if ( girderData.TempStrandUsage == pgsTypes::ttsPretensioned ) 
      {
         *pParagraph << "Total Prestress Loss " << rptNewLine << rptRcImage(strImagePath + "DeltaFpt_PS_TTS" + strYear + ".gif") << rptNewLine;
      }
      else
      {
         *pParagraph << "Total Prestress Loss " << rptNewLine << rptRcImage(strImagePath + "DeltaFpt_PT_TTS" + strYear + ".gif") << rptNewLine;
      }
   }
   else
   {
      *pParagraph << "Total Prestress Loss " << rptNewLine << rptRcImage(strImagePath + "DeltaFpt" + strYear + ".gif") << rptNewLine;
   }
      

   *pParagraph << table << rptNewLine;

   ColumnIndexType col = 0;
   (*table)(0,col++) << COLHDR("Location from"<<rptNewLine<<"Left Support",rptLengthUnitTag,  pDisplayUnits->GetSpanLengthUnit() );
   (*table)(0,col++) << COLHDR(symbol(DELTA) << "f" << Sub("pR1"), rptStressUnitTag, pDisplayUnits->GetStressUnit() );
   (*table)(0,col++) << COLHDR(symbol(DELTA) << "f" << Sub("pES"), rptStressUnitTag, pDisplayUnits->GetStressUnit() );
   
   if ( 0 < NtMax && girderData.TempStrandUsage != pgsTypes::ttsPretensioned ) 
   {
      (*table)(0,col++) << COLHDR(symbol(DELTA) << "f" << Sub("pp"), rptStressUnitTag, pDisplayUnits->GetStressUnit() );
   }

   (*table)(0,col++) << COLHDR(symbol(DELTA) << "f" << Sub("pSR"), rptStressUnitTag, pDisplayUnits->GetStressUnit() );
   (*table)(0,col++) << COLHDR(symbol(DELTA) << "f" << Sub("pCR"), rptStressUnitTag, pDisplayUnits->GetStressUnit() );
   (*table)(0,col++) << COLHDR(symbol(DELTA) << "f" << Sub("pR2"), rptStressUnitTag, pDisplayUnits->GetStressUnit() );

   if ( 0 < NtMax ) 
   {
      (*table)(0,col++) << COLHDR(symbol(DELTA) << "f" << Sub("ptr"), rptStressUnitTag, pDisplayUnits->GetStressUnit() );
   }

   if ( !bIgnoreElasticGain )
      (*table)(0,col++) << COLHDR(symbol(DELTA) << "f" << Sub("pED"), rptStressUnitTag, pDisplayUnits->GetStressUnit() );


   (*table)(0,col++) << COLHDR(symbol(DELTA) << "f" << Sub("pT"), rptStressUnitTag, pDisplayUnits->GetStressUnit() );

   table->m_GirderData = girderData;
   table->m_NtMax = NtMax;
   table->m_bIgnoreElasticGain = bIgnoreElasticGain;
   
   return table;
}

void CFinalPrestressLossTable::AddRow(rptChapter* pChapter,IBroker* pBroker,RowIndexType row,LOSSDETAILS& details,IEAFDisplayUnits* pDisplayUnits,Uint16 level)
{
   ColumnIndexType col = 1;
   (*this)(row,col++) << stress.SetValue( details.RefinedLosses.PermanentStrand_RelaxationLossesAtXfer() );
   (*this)(row,col++) << stress.SetValue( details.pLosses->PermanentStrand_ElasticShorteningLosses() );
   
   if ( 0 < m_NtMax && m_GirderData.TempStrandUsage != pgsTypes::ttsPretensioned ) 
   {
      (*this)(row,col++) << stress.SetValue(details.pLosses->GetDeltaFpp());
   }

   (*this)(row,col++) << stress.SetValue( details.RefinedLosses.ShrinkageLosses() );
   (*this)(row,col++) << stress.SetValue( details.RefinedLosses.CreepLosses() );
   (*this)(row,col++) << stress.SetValue( details.RefinedLosses.RelaxationLossesAfterXfer() );

   if ( 0 < m_NtMax ) 
   {
      (*this)(row,col++) << stress.SetValue( details.pLosses->GetDeltaFptr() );
   }

   if ( !m_bIgnoreElasticGain )
      (*this)(row,col++) << stress.SetValue( details.pLosses->ElasticGainDueToDeckPlacement() );


   (*this)(row,col++) << stress.SetValue( details.pLosses->PermanentStrand_Final() );
}
