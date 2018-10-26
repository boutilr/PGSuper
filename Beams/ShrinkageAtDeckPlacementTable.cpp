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

// ShrinkageAtDeckPlacementTable.cpp : Implementation of CShrinkageAtDeckPlacementTable
#include "stdafx.h"
#include "ShrinkageAtDeckPlacementTable.h"
#include <IFace\Bridge.h>
#include <IFace\Project.h>
#include <PsgLib\SpecLibraryEntry.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CShrinkageAtDeckPlacementTable::CShrinkageAtDeckPlacementTable(ColumnIndexType NumColumns, IEAFDisplayUnits* pDisplayUnits) :
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
   DEFINE_UV_PROTOTYPE( time,        pDisplayUnits->GetLongTimeUnit(),        false );

   scalar.SetFormat( sysNumericFormatTool::Automatic );
   scalar.SetWidth(6);
   scalar.SetPrecision(3);

   strain.SetFormat( sysNumericFormatTool::Automatic );
   strain.SetWidth(6);
   strain.SetPrecision(3);
}

CShrinkageAtDeckPlacementTable* CShrinkageAtDeckPlacementTable::PrepareTable(rptChapter* pChapter,IBroker* pBroker,SpanIndexType span,GirderIndexType gdr,LOSSDETAILS& details,IEAFDisplayUnits* pDisplayUnits,Uint16 level)
{
   GET_IFACE2(pBroker,ISpecification,pSpec);
   std::string strSpecName = pSpec->GetSpecification();

   GET_IFACE2(pBroker,ILibrary,pLib);
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( strSpecName.c_str() );

   // Create and configure the table
   ColumnIndexType numColumns = 8;
   CShrinkageAtDeckPlacementTable* table = new CShrinkageAtDeckPlacementTable( numColumns, pDisplayUnits );
   pgsReportStyleHolder::ConfigureTable(table);


   std::string strImagePath(pgsReportStyleHolder::GetImagePath());
   
   rptParagraph* pParagraph = new rptParagraph(pgsReportStyleHolder::GetHeadingStyle());
   *pChapter << pParagraph;

   *pParagraph << "[5.9.5.4.2a] Shrinkage of Girder Concrete : " << symbol(DELTA) << Sub2("f","pSR") << rptNewLine;
   *pParagraph << rptRcImage(strImagePath + "Delta_FpSR.png") << rptNewLine;

   if ( pSpecEntry->GetSpecificationType() <= lrfdVersionMgr::ThirdEditionWith2005Interims )
   {
      if ( IS_SI_UNITS(pDisplayUnits) )
         *pParagraph << rptRcImage(strImagePath + "VSFactor_SI_2005.png") << rptNewLine;
      else
         *pParagraph << rptRcImage(strImagePath + "VSFactor_US_2005.png") << rptNewLine;
   }
#if defined IGNORE_2007_CHANGES
   else
   {
      if ( IS_SI_UNITS(pDisplayUnits) )
         *pParagraph << rptRcImage(strImagePath + "VSFactor_SI_2006.png") << rptNewLine;
      else
         *pParagraph << rptRcImage(strImagePath + "VSFactor_US_2006.png") << rptNewLine;
   }
#else
   else if ( pSpecEntry->GetSpecificationType() == lrfdVersionMgr::ThirdEditionWith2006Interims )
   {
      if ( pDisplayUnits->GetUnitDisplayMode() == pgsTypes::umSI )
         *pParagraph << rptRcImage(strImagePath + "VSFactor_SI_2006.png") << rptNewLine;
      else
         *pParagraph << rptRcImage(strImagePath + "VSFactor_US_2006.png") << rptNewLine;
   }
   else
   {
      if ( pDisplayUnits->GetUnitDisplayMode() == pgsTypes::umSI )
         *pParagraph << rptRcImage(strImagePath + "VSFactor_SI_2007.png") << rptNewLine;
      else
         *pParagraph << rptRcImage(strImagePath + "VSFactor_US_2007.png") << rptNewLine;
   }
#endif // IGNORE_2007_CHANGES
   *pParagraph << rptRcImage(strImagePath + "HumidityFactor.png") << rptNewLine;
   if ( IS_SI_UNITS(pDisplayUnits) )
      *pParagraph << rptRcImage(strImagePath + "ConcreteFactors_SI.png") << rptNewLine;
   else
      *pParagraph << rptRcImage(strImagePath + "ConcreteFactors_US.png") << rptNewLine;

   // parameters for calculations (two tables to keep the width printable)
   rptRcTable* pParamTable = pgsReportStyleHolder::CreateDefaultTable(6,"");
   *pParagraph << pParamTable << rptNewLine;
   (*pParamTable)(0,0) << "H" << rptNewLine << "(%)";
   (*pParamTable)(0,1) << COLHDR("V/S",rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit() );
   (*pParamTable)(0,2) << COLHDR(RPT_FCI,rptStressUnitTag,pDisplayUnits->GetStressUnit());
   (*pParamTable)(0,3) << COLHDR(Sub2("t","i"),rptTimeUnitTag,pDisplayUnits->GetLongTimeUnit());
   (*pParamTable)(0,4) << COLHDR(Sub2("t","d"),rptTimeUnitTag,pDisplayUnits->GetLongTimeUnit());
   (*pParamTable)(0,5) << COLHDR(Sub2("t","f"),rptTimeUnitTag,pDisplayUnits->GetLongTimeUnit());

   (*pParamTable)(1,0) << details.RefinedLosses2005.GetRelHumidity();
   (*pParamTable)(1,1) << table->ecc.SetValue(details.RefinedLosses2005.GetVolume()/details.RefinedLosses2005.GetSurfaceArea());
   (*pParamTable)(1,2) << table->stress.SetValue(details.RefinedLosses2005.GetFci());
   (*pParamTable)(1,3) << table->time.SetValue(details.RefinedLosses2005.GetAdjustedInitialAge());
   (*pParamTable)(1,4) << table->time.SetValue(details.RefinedLosses2005.GetAgeAtDeckPlacement());
   (*pParamTable)(1,5) << table->time.SetValue(details.RefinedLosses2005.GetFinalAge());

   // intermediate results
   pParamTable = pgsReportStyleHolder::CreateDefaultTable(6,"");
   *pParagraph << pParamTable << rptNewLine;
#if defined IGNORE_2007_CHANGES
   (*pParamTable)(0,0) << Sub2("k","vs");
#else
   if ( pSpecEntry->GetSpecificationType() <= lrfdVersionMgr::ThirdEditionWith2006Interims )
      (*pParamTable)(0,0) << Sub2("k","vs");
   else
      (*pParamTable)(0,0) << Sub2("k","s");
#endif // IGNORE_2007_CHANGES

   (*pParamTable)(0,1) << Sub2("k","hs");
   (*pParamTable)(0,2) << Sub2("k","hc");
   (*pParamTable)(0,3) << Sub2("k","f");

   table->time.ShowUnitTag(true);
   (*pParamTable)(0,4) << Sub2("k","td") << rptNewLine << "t = " << table->time.SetValue(details.RefinedLosses2005.GetAgeAtDeckPlacement());
   (*pParamTable)(0,5) << Sub2("k","td") << rptNewLine << "t = " << table->time.SetValue(details.RefinedLosses2005.GetFinalAge());
   table->time.ShowUnitTag(false);

   (*pParamTable)(1,0) << table->scalar.SetValue(details.RefinedLosses2005.GetCreepInitialToFinal().GetKvs());
   (*pParamTable)(1,1) << table->scalar.SetValue(details.RefinedLosses2005.Getkhs());
   (*pParamTable)(1,2) << table->scalar.SetValue(details.RefinedLosses2005.GetCreepInitialToFinal().GetKhc());
   (*pParamTable)(1,3) << table->scalar.SetValue(details.RefinedLosses2005.GetCreepInitialToFinal().GetKf());
   (*pParamTable)(1,4) << table->scalar.SetValue(details.RefinedLosses2005.GetCreepInitialToDeck().GetKtd());
   (*pParamTable)(1,5) << table->scalar.SetValue(details.RefinedLosses2005.GetCreepInitialToFinal().GetKtd());

   pParamTable = pgsReportStyleHolder::CreateDefaultTable(8,"");
   *pParagraph << pParamTable << rptNewLine;
   pParamTable->SetNumberOfHeaderRows(2);
   pParamTable->SetRowSpan(0,0,2);
   pParamTable->SetRowSpan(1,0,-1);
   (*pParamTable)(0,0) << COLHDR(Sub2("E","p"), rptStressUnitTag, pDisplayUnits->GetStressUnit());
   pParamTable->SetRowSpan(0,1,2);
   pParamTable->SetRowSpan(1,1,-1);
   (*pParamTable)(0,1) << COLHDR(Sub2("E","ci"), rptStressUnitTag, pDisplayUnits->GetStressUnit());
   pParamTable->SetColumnSpan(0,2,3);
   pParamTable->SetColumnSpan(0,3,-1);
   pParamTable->SetColumnSpan(0,4,-1);
   (*pParamTable)(0,2) << "Shrinkage";
   (*pParamTable)(1,2) << Sub2("K","1");
   (*pParamTable)(1,3) << Sub2("K","2");
   (*pParamTable)(1,4) << Sub2(symbol(epsilon),"bid") << "x 1000";
   pParamTable->SetColumnSpan(0,5,3);
   pParamTable->SetColumnSpan(0,6,-1);
   pParamTable->SetColumnSpan(0,7,-1);
   (*pParamTable)(0,5) << "Creep";
   (*pParamTable)(1,5) << Sub2("K","1");
   (*pParamTable)(1,6) << Sub2("K","2");
   (*pParamTable)(1,7) << Sub2(symbol(psi),"b") << "(" << Sub2("t","f") << "," << Sub2("t","i") << ")";

   (*pParamTable)(2,0) << table->mod_e.SetValue(details.RefinedLosses2005.GetEp());
   (*pParamTable)(2,1) << table->mod_e.SetValue(details.RefinedLosses2005.GetEci());
   (*pParamTable)(2,2) << details.RefinedLosses2005.GetGdrK1Shrinkage();
   (*pParamTable)(2,3) << details.RefinedLosses2005.GetGdrK2Shrinkage();
   (*pParamTable)(2,4) << table->strain.SetValue(details.RefinedLosses2005.Get_ebid() * 1000);
   (*pParamTable)(2,5) << details.RefinedLosses2005.GetGdrK1Creep();
   (*pParamTable)(2,6) << details.RefinedLosses2005.GetGdrK2Creep();
   (*pParamTable)(2,7) << table->scalar.SetValue(details.RefinedLosses2005.GetCreepInitialToFinal().GetCreepCoefficient());

   // shrinkage loss   
   *pParagraph << table << rptNewLine;
   (*table)(0,0) << COLHDR("Location from"<<rptNewLine<<"Left Support",rptLengthUnitTag,  pDisplayUnits->GetSpanLengthUnit() );
   (*table)(0,1) << COLHDR(Sub2("A","ps"), rptAreaUnitTag, pDisplayUnits->GetAreaUnit());
   (*table)(0,2) << COLHDR(Sub2("A","g"), rptAreaUnitTag, pDisplayUnits->GetAreaUnit());
   (*table)(0,3) << COLHDR(Sub2("I","g"), rptLength4UnitTag, pDisplayUnits->GetMomentOfInertiaUnit());
   (*table)(0,4) << COLHDR(Sub2("e","ps"), rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit());
   (*table)(0,5) << COLHDR(Sub2("e","p"), rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit());
   (*table)(0,6) << Sub2("K","id");
   (*table)(0,7) << COLHDR(symbol(DELTA) << Sub2("f","pSR"), rptStressUnitTag, pDisplayUnits->GetStressUnit() );

   return table;
}

void CShrinkageAtDeckPlacementTable::AddRow(rptChapter* pChapter,IBroker* pBroker,RowIndexType row,LOSSDETAILS& details,IEAFDisplayUnits* pDisplayUnits,Uint16 level)
{
   Float64 Aps = details.pLosses->GetApsPermanent();
   if ( details.pLosses->GetTempStrandUsage() == lrfdLosses::tsPretensioned ||
        details.pLosses->GetTempStrandUsage() == lrfdLosses::tsPTBeforeLifting )
   {
      Aps += details.pLosses->GetApsTemporary();
   }

   Float64 e  = details.pLosses->GetEccPermanent();
   Float64 eps = e;
   if ( details.pLosses->GetTempStrandUsage() == lrfdLosses::tsPretensioned )
   {
      eps = details.pLosses->GetEccpg();
   }

   (*this)(row,1) << area.SetValue(Aps);
   (*this)(row,2) << area.SetValue(details.pLosses->GetAg());
   (*this)(row,3) << mom_inertia.SetValue(details.pLosses->GetIg());
   (*this)(row,4) << ecc.SetValue(eps);
   (*this)(row,5) << ecc.SetValue(e);
   (*this)(row,6) << scalar.SetValue(details.RefinedLosses2005.GetKid());
   (*this)(row,7) << stress.SetValue( details.RefinedLosses2005.ShrinkageLossBeforeDeckPlacement() );
}
