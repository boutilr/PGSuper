///////////////////////////////////////////////////////////////////////
// PGSuper - Prestressed Girder SUPERstructure Design and Analysis
// Copyright � 1999-2021  Washington State Department of Transportation
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

#include "stdafx.h"
#include "AutogenousShrinkageTable.h"
#include <IFace\Bridge.h>
#include <IFace\Intervals.h>

#include <LRFD\PCIUHPCLosses.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CAutogenousShrinkageTable::CAutogenousShrinkageTable(ColumnIndexType NumColumns, IEAFDisplayUnits* pDisplayUnits) :
rptRcTable(NumColumns,0)
{
   DEFINE_UV_PROTOTYPE( spanloc,     pDisplayUnits->GetSpanLengthUnit(),      false );
   DEFINE_UV_PROTOTYPE( gdrloc,      pDisplayUnits->GetSpanLengthUnit(),      false );
   DEFINE_UV_PROTOTYPE( mod_e,       pDisplayUnits->GetModEUnit(),            false );
   DEFINE_UV_PROTOTYPE( area,        pDisplayUnits->GetAreaUnit(),            false );
   DEFINE_UV_PROTOTYPE( mom_inertia, pDisplayUnits->GetMomentOfInertiaUnit(), false );
   DEFINE_UV_PROTOTYPE( ecc,         pDisplayUnits->GetComponentDimUnit(),    false );
   DEFINE_UV_PROTOTYPE( stress,      pDisplayUnits->GetStressUnit(),          false );
}

CAutogenousShrinkageTable* CAutogenousShrinkageTable::PrepareTable(rptChapter* pChapter,IBroker* pBroker,const CSegmentKey& segmentKey,bool bTemporaryStrands,const LOSSDETAILS* pDetails,IEAFDisplayUnits* pDisplayUnits,Uint16 level)
{
   std::_tstring strImagePath(rptStyleManager::GetImagePath());

   GET_IFACE2(pBroker,IIntervals,pIntervals);
   IntervalIndexType releaseIntervalIdx = pIntervals->GetPrestressReleaseInterval(segmentKey);

   GET_IFACE2(pBroker,IMaterials,pMaterials);
   Float64 Epp = pMaterials->GetStrandMaterial(segmentKey,pgsTypes::Straight)->GetE(); // OK to use Straight since we just want E
   Float64 Ept = pMaterials->GetStrandMaterial(segmentKey,pgsTypes::Temporary)->GetE();

   rptParagraph* pParagraph = new rptParagraph(rptStyleManager::GetHeadingStyle());
   *pChapter << pParagraph;
   pParagraph->SetName(_T("Autogenous Shrinkage"));
   *pParagraph << pParagraph->GetName() << rptNewLine;

   pParagraph = new rptParagraph;
   *pChapter << pParagraph;

   *pParagraph << _T("PCI UHPC SDG E.9.2.2.2") << rptNewLine;

   GET_IFACE2(pBroker,ISectionProperties,pSectProp);
   pgsTypes::SectionPropertyMode spMode = pSectProp->GetSectionPropertiesMode();

   // create and configure the table
   GET_IFACE2(pBroker, IGirder, pGirder);
   bool bIsPrismatic = pGirder->IsPrismatic(releaseIntervalIdx, segmentKey);

   GET_IFACE2(pBroker, IBridge, pBridge);
   bool bIsAsymmetric = pBridge->HasAsymmetricGirders() || pBridge->HasAsymmetricPrestressing() ? true : false;

   ColumnIndexType numColumns = 3; // location, location, Aps_Perm

   // columns for section properties and total ps eccentricty
   if (bIsPrismatic)
   {
      if (bIsAsymmetric)
      {
         numColumns += 2; // epsx, epsy
      }
      else
      {
         numColumns += 1; // eps
      }
   }
   else
   {
      // for prismatic girders, properties are the same along the length of the girder so they are reported
      // above the table... for non-prismatic girders, section properties change at each location
      if (bIsAsymmetric)
      {
         numColumns += 6; // A, Ixx, Iyy, Ixy, epsx, epsy
      }
      else
      {
         numColumns += 3; // A, Ixx, eps
      }
   }

   // permanent strands
   if (bIsAsymmetric)
   {
      numColumns += 3; // epx, epy, dfpAS
   }
   else
   {
      numColumns += 2; // ep, dfpAS
   }
   
   if ( bTemporaryStrands )
   {
      numColumns++; // Aps_Temp
      if (bIsAsymmetric)
      {
         numColumns += 3; // etx, ety, dfpAS
      }
      else
      {
         numColumns += 2; // et, dfpAS
      }
   }
    
   CAutogenousShrinkageTable* table = new CAutogenousShrinkageTable( numColumns, pDisplayUnits );
   rptStyleManager::ConfigureTable(table);
   
   table->m_bTemporaryStrands = bTemporaryStrands;
   table->m_bIsPrismatic      = bIsPrismatic;
   table->m_bIsAsymmetric = bIsAsymmetric;

   if (bIsAsymmetric)
   {
      if (spMode == pgsTypes::spmGross)
      {
         *pParagraph << rptRcImage(strImagePath + _T("Delta_fpAS_Gross_Asymmetric.png")) << rptNewLine;
      }
      else
      {
         *pParagraph << rptRcImage(strImagePath + _T("Delta_fpAS_Transformed_Asymmetric.png")) << rptNewLine;
      }
   }
   else
   {
      if (spMode == pgsTypes::spmGross)
      {
         *pParagraph << rptRcImage(strImagePath + _T("Delta_fpAS_Gross.png")) << rptNewLine;
      }
      else
      {
         *pParagraph << rptRcImage(strImagePath + _T("Delta_fpAS_Transformed.png")) << rptNewLine;
      }
   }
   
   *pParagraph << Sub2(symbol(epsilon), _T("AS")) << _T(" = ") << pMaterials->GetSegmentAutogenousShrinkage(segmentKey) << rptNewLine;

   table->mod_e.ShowUnitTag(true);
   table->area.ShowUnitTag(true);
   table->mom_inertia.ShowUnitTag(true);
   table->stress.ShowUnitTag(true);
   if ( bIsPrismatic )
   {
      GET_IFACE2(pBroker, IPointOfInterest, pPoi);
      PoiList vPoi;
      pPoi->GetPointsOfInterest(segmentKey, POI_5L | POI_RELEASED_SEGMENT, &vPoi);
      ATLASSERT(vPoi.size() == 1);
      const pgsPointOfInterest& poi(vPoi.front());
      Float64 Ag = pSectProp->GetAg(releaseIntervalIdx, poi);
      Float64 Ixx = pSectProp->GetIxx(releaseIntervalIdx, poi);
      if (bIsAsymmetric)
      {
         Float64 Iyy = pSectProp->GetIyy(releaseIntervalIdx, poi);
         Float64 Ixy = pSectProp->GetIxy(releaseIntervalIdx, poi);
         if (spMode == pgsTypes::spmGross)
         {
            *pParagraph << Sub2(_T("A"), _T("g")) << _T(" = ") << table->area.SetValue(Ag) << rptNewLine;
            *pParagraph << Sub2(_T("I"), _T("xx")) << _T(" = ") << table->mom_inertia.SetValue(Ixx) << rptNewLine;
            *pParagraph << Sub2(_T("I"), _T("yy")) << _T(" = ") << table->mom_inertia.SetValue(Iyy) << rptNewLine;
            *pParagraph << Sub2(_T("I"), _T("xy")) << _T(" = ") << table->mom_inertia.SetValue(Ixy) << rptNewLine;
         }
         else
         {
            *pParagraph << Sub2(_T("A"), _T("gt")) << _T(" = ") << table->area.SetValue(Ag) << rptNewLine;
            *pParagraph << Sub2(_T("I"), _T("xxt")) << _T(" = ") << table->mom_inertia.SetValue(Ixx) << rptNewLine;
            *pParagraph << Sub2(_T("I"), _T("yyt")) << _T(" = ") << table->mom_inertia.SetValue(Iyy) << rptNewLine;
            *pParagraph << Sub2(_T("I"), _T("xyt")) << _T(" = ") << table->mom_inertia.SetValue(Ixy) << rptNewLine;
         }

      }
      else
      {
         if (spMode == pgsTypes::spmGross)
         {
            *pParagraph << Sub2(_T("A"), _T("g")) << _T(" = ") << table->area.SetValue(Ag) << rptNewLine;
            *pParagraph << Sub2(_T("I"), _T("g")) << _T(" = ") << table->mom_inertia.SetValue(Ixx) << rptNewLine;
         }
         else
         {
            *pParagraph << Sub2(_T("A"), _T("gt")) << _T(" = ") << table->area.SetValue(Ag) << rptNewLine;
            *pParagraph << Sub2(_T("I"), _T("gt")) << _T(" = ") << table->mom_inertia.SetValue(Ixx) << rptNewLine;
         }
      }
   }

   if ( bTemporaryStrands )
   {
      *pParagraph << Sub2(_T("E"),_T("p")) << _T(" (Permanent) = ") << table->mod_e.SetValue(Epp) << rptNewLine;
      *pParagraph << Sub2(_T("E"),_T("p")) << _T(" (Temporary) = ") << table->mod_e.SetValue(Ept) << rptNewLine;
   }
   else
   {
      *pParagraph << Sub2(_T("E"),_T("p")) << _T(" = ") << table->mod_e.SetValue(Epp) << rptNewLine;
   }

   table->mod_e.ShowUnitTag(false);
   table->area.ShowUnitTag(false);
   table->mom_inertia.ShowUnitTag(false);
   table->stress.ShowUnitTag(false);
   
   *pParagraph << table << rptNewLine;
   
   ColumnIndexType col = 0;
   (*table)(0,col++) << COLHDR(_T("Location from")<<rptNewLine<<_T("End of Girder"),rptLengthUnitTag,  pDisplayUnits->GetSpanLengthUnit() );
   (*table)(0,col++) << COLHDR(_T("Location from")<<rptNewLine<<_T("Left Support"),rptLengthUnitTag,  pDisplayUnits->GetSpanLengthUnit() );
   
   if (bIsPrismatic)
   {
      if (bIsAsymmetric)
      {
         if (spMode == pgsTypes::spmGross)
         {
            (*table)(0, col++) << COLHDR(Sub2(_T("e"), _T("psx")), rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit());
            (*table)(0, col++) << COLHDR(Sub2(_T("e"), _T("psy")), rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit());
         }
         else
         {
            (*table)(0, col++) << COLHDR(Sub2(_T("e"), _T("psxt")), rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit());
            (*table)(0, col++) << COLHDR(Sub2(_T("e"), _T("psyt")), rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit());
         }
      }
      else
      {
         if (spMode == pgsTypes::spmGross)
         {
            (*table)(0, col++) << COLHDR(Sub2(_T("e"), _T("ps")), rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit());
         }
         else
         {
            (*table)(0, col++) << COLHDR(Sub2(_T("e"), _T("pst")), rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit());
         }
      }
   }
   else
   {
      if (bIsAsymmetric)
      {
         if (spMode == pgsTypes::spmGross)
         {
            (*table)(0, col++) << COLHDR(Sub2(_T("A"), _T("g")), rptAreaUnitTag, pDisplayUnits->GetAreaUnit());
            (*table)(0, col++) << COLHDR(Sub2(_T("I"), _T("xx")), rptLength4UnitTag, pDisplayUnits->GetMomentOfInertiaUnit());
            (*table)(0, col++) << COLHDR(Sub2(_T("I"), _T("yy")), rptLength4UnitTag, pDisplayUnits->GetMomentOfInertiaUnit());
            (*table)(0, col++) << COLHDR(Sub2(_T("I"), _T("xy")), rptLength4UnitTag, pDisplayUnits->GetMomentOfInertiaUnit());
            (*table)(0, col++) << COLHDR(Sub2(_T("e"), _T("psx")), rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit());
            (*table)(0, col++) << COLHDR(Sub2(_T("e"), _T("psy")), rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit());
         }
         else
         {
            (*table)(0, col++) << COLHDR(Sub2(_T("A"), _T("gt")), rptAreaUnitTag, pDisplayUnits->GetAreaUnit());
            (*table)(0, col++) << COLHDR(Sub2(_T("I"), _T("xxt")), rptLength4UnitTag, pDisplayUnits->GetMomentOfInertiaUnit());
            (*table)(0, col++) << COLHDR(Sub2(_T("I"), _T("yyt")), rptLength4UnitTag, pDisplayUnits->GetMomentOfInertiaUnit());
            (*table)(0, col++) << COLHDR(Sub2(_T("I"), _T("xyt")), rptLength4UnitTag, pDisplayUnits->GetMomentOfInertiaUnit());
            (*table)(0, col++) << COLHDR(Sub2(_T("e"), _T("psxt")), rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit());
            (*table)(0, col++) << COLHDR(Sub2(_T("e"), _T("psyt")), rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit());
         }
      }
      else
      {
         if (spMode == pgsTypes::spmGross)
         {
            (*table)(0, col++) << COLHDR(Sub2(_T("A"), _T("g")), rptAreaUnitTag, pDisplayUnits->GetAreaUnit());
            (*table)(0, col++) << COLHDR(Sub2(_T("I"), _T("g")), rptLength4UnitTag, pDisplayUnits->GetMomentOfInertiaUnit());
            (*table)(0, col++) << COLHDR(Sub2(_T("e"), _T("ps")), rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit());
         }
         else
         {
            (*table)(0, col++) << COLHDR(Sub2(_T("A"), _T("gt")), rptAreaUnitTag, pDisplayUnits->GetAreaUnit());
            (*table)(0, col++) << COLHDR(Sub2(_T("I"), _T("gt")), rptLength4UnitTag, pDisplayUnits->GetMomentOfInertiaUnit());
            (*table)(0, col++) << COLHDR(Sub2(_T("e"), _T("pst")), rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit());
         }
      }
   }
   
  
   if ( bTemporaryStrands )
   {
      // we have temporary strands so we need two header rows
      table->SetNumberOfHeaderRows(2);
   
      // all preceding columns need to span 2 rows
      for (ColumnIndexType c = 0; c < col; c++)
      {
         table->SetRowSpan(0, c, 2);
      }
   
      table->SetColumnSpan(0,col,bIsAsymmetric ? 4 : 3);
      (*table)(0,col) << _T("Permanent Strands");
   
      (*table)(1, col++) << COLHDR(Sub2(_T("A"),_T("p")), rptAreaUnitTag, pDisplayUnits->GetAreaUnit());
      if (bIsAsymmetric)
      {
         if (spMode == pgsTypes::spmGross)
         {
            (*table)(1, col++) << COLHDR(Sub2(_T("e"), _T("px")), rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit());
            (*table)(1, col++) << COLHDR(Sub2(_T("e"), _T("py")), rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit());
         }
         else
         {
            (*table)(1, col++) << COLHDR(Sub2(_T("e"), _T("pxt")), rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit());
            (*table)(1, col++) << COLHDR(Sub2(_T("e"), _T("pyt")), rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit());
         }
      }
      else
      {
         if (spMode == pgsTypes::spmGross)
         {
            (*table)(1, col++) << COLHDR(Sub2(_T("e"), _T("p")), rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit());
         }
         else
         {
            (*table)(1, col++) << COLHDR(Sub2(_T("e"), _T("pt")), rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit());
         }
      }
   
      (*table)(1,col++) << COLHDR(symbol(DELTA) << RPT_STRESS(_T("pAS")), rptStressUnitTag, pDisplayUnits->GetStressUnit() );
   
      // temp
      table->SetColumnSpan(0, col, bIsAsymmetric ? 4 : 3);
      (*table)(0, col) << _T("Temporary Strands");
      (*table)(1, col++) << COLHDR(Sub2(_T("A"), _T("t")), rptAreaUnitTag, pDisplayUnits->GetAreaUnit());
      if (bIsAsymmetric)
      {
         if (spMode == pgsTypes::spmGross)
         {
            (*table)(1, col++) << COLHDR(Sub2(_T("e"), _T("tx")), rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit());
            (*table)(1, col++) << COLHDR(Sub2(_T("e"), _T("ty")), rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit());
         }
         else
         {
            (*table)(1, col++) << COLHDR(Sub2(_T("e"), _T("txt")), rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit());
            (*table)(1, col++) << COLHDR(Sub2(_T("e"), _T("tyt")), rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit());
         }
      }
      else
      {
         if (spMode == pgsTypes::spmGross)
         {
            (*table)(1, col++) << COLHDR(Sub2(_T("e"), _T("t")), rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit());
         }
         else
         {
            (*table)(1, col++) << COLHDR(Sub2(_T("e"), _T("tt")), rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit());
         }
      }
   
      (*table)(1,col++) << COLHDR(symbol(DELTA) << RPT_STRESS(_T("pAS")), rptStressUnitTag, pDisplayUnits->GetStressUnit() );
   }
   else
   {
      (*table)(0, col++) << COLHDR(RPT_APS, rptAreaUnitTag, pDisplayUnits->GetAreaUnit());
      if (bIsAsymmetric)
      {
         if (spMode == pgsTypes::spmGross)
         {
            (*table)(0, col++) << COLHDR(Sub2(_T("e"), _T("px")), rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit());
            (*table)(0, col++) << COLHDR(Sub2(_T("e"), _T("py")), rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit());
         }
         else
         {
            (*table)(0, col++) << COLHDR(Sub2(_T("e"), _T("pxt")), rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit());
            (*table)(0, col++) << COLHDR(Sub2(_T("e"), _T("pyt")), rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit());
         }
      }
      else
      {
         if (spMode == pgsTypes::spmGross)
         {
            (*table)(0, col++) << COLHDR(Sub2(_T("e"), _T("p")), rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit());
         }
         else
         {
            (*table)(0, col++) << COLHDR(Sub2(_T("e"), _T("pt")), rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit());
         }
      }
   
   
      (*table)(0,col++) << COLHDR(symbol(DELTA) << RPT_STRESS(_T("pAS")), rptStressUnitTag, pDisplayUnits->GetStressUnit() );
   }
   
   *pParagraph << rptNewLine;
   
   return table;
}

void CAutogenousShrinkageTable::AddRow(rptChapter* pChapter,IBroker* pBroker,const pgsPointOfInterest& poi,RowIndexType row,const LOSSDETAILS* pDetails,IEAFDisplayUnits* pDisplayUnits,Uint16 level)
{
   ColumnIndexType col = 2;
   RowIndexType rowOffset = GetNumberOfHeaderRows()-1;

   Float64 Ag, Ybg, Ixx, Iyy, Ixy;
   pDetails->pLosses->GetNoncompositeProperties(&Ag, &Ybg, &Ixx, &Iyy, &Ixy);

   if (m_bIsPrismatic)
   {
      if (m_bIsAsymmetric)
      {
         (*this)(row + rowOffset, col++) << ecc.SetValue(pDetails->pLosses->GetEccpgRelease().X());
         (*this)(row + rowOffset, col++) << ecc.SetValue(pDetails->pLosses->GetEccpgRelease().Y());
      }
      else
      {
         (*this)(row + rowOffset, col++) << ecc.SetValue(pDetails->pLosses->GetEccpgRelease().Y());
      }
   }
   else
   {
      if (m_bIsAsymmetric)
      {
         (*this)(row + rowOffset, col++) << area.SetValue(Ag);
         (*this)(row + rowOffset, col++) << mom_inertia.SetValue(Ixx);
         (*this)(row + rowOffset, col++) << mom_inertia.SetValue(Iyy);
         (*this)(row + rowOffset, col++) << mom_inertia.SetValue(Ixy);
         (*this)(row + rowOffset, col++) << ecc.SetValue(pDetails->pLosses->GetEccpgRelease().X());
         (*this)(row + rowOffset, col++) << ecc.SetValue(pDetails->pLosses->GetEccpgRelease().Y());
      }
      else
      {
         (*this)(row + rowOffset, col++) << area.SetValue(Ag);
         (*this)(row + rowOffset, col++) << mom_inertia.SetValue(Ixx);
         (*this)(row + rowOffset, col++) << ecc.SetValue(pDetails->pLosses->GetEccpgRelease().Y());
      }
   }

   const std::shared_ptr<const lrfdPCIUHPCLosses> pLosses = std::dynamic_pointer_cast<const lrfdPCIUHPCLosses>(pDetails->pLosses);
   ATLASSERT(pLosses.use_count() == pDetails->pLosses.use_count());

   (*this)(row + rowOffset, col++) << area.SetValue(pDetails->pLosses->GetApsPermanent());
   if (m_bIsAsymmetric)
   {
      (*this)(row + rowOffset, col++) << ecc.SetValue(pDetails->pLosses->GetEccPermanentRelease().X());
   }
   (*this)(row+rowOffset,col++) << ecc.SetValue( pDetails->pLosses->GetEccPermanentRelease().Y() );
   (*this)(row+rowOffset,col++) << stress.SetValue( pLosses->PermanentStrand_AutogenousShrinkage() );

   if ( m_bTemporaryStrands )
   {
      (*this)(row + rowOffset, col++) << area.SetValue(pDetails->pLosses->GetApsTemporary());
      if (m_bIsAsymmetric)
      {
         (*this)(row + rowOffset, col++) << ecc.SetValue(pDetails->pLosses->GetEccTemporary().X());
      }
      (*this)(row + rowOffset, col++) << ecc.SetValue(pDetails->pLosses->GetEccTemporary().Y());
      (*this)(row + rowOffset, col++) << stress.SetValue( pLosses->TemporaryStrand_AutogenousShrinkage() );
   }
}
