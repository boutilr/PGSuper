///////////////////////////////////////////////////////////////////////
// PGSuper - Prestressed Girder SUPERstructure Design and Analysis
// Copyright � 1999-2023  Washington State Department of Transportation
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
#include <Reporting\ACI209ShrinkageStrainChapterBuilder.h>

#include <IFace\Bridge.h>
#include <IFace\Intervals.h>
#include <IFace\Project.h>
#include <Material\ConcreteBase.h>
#include <Material\ACI209Concrete.h>
#include <PgsExt\TimelineEvent.h>
#include <PgsExt\CastDeckActivity.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/****************************************************************************
CLASS
   CACI209ShrinkageStrainChapterBuilder
****************************************************************************/

CACI209ShrinkageStrainChapterBuilder::CACI209ShrinkageStrainChapterBuilder(bool bSelect) :
CPGSuperChapterBuilder(bSelect)
{
}

LPCTSTR CACI209ShrinkageStrainChapterBuilder::GetName() const
{
   return TEXT("Shrinkage Strain Details");
}

rptChapter* CACI209ShrinkageStrainChapterBuilder::Build(CReportSpecification* pRptSpec,Uint16 level) const
{
   CGirderReportSpecification* pGirderRptSpec = dynamic_cast<CGirderReportSpecification*>(pRptSpec);
   CComPtr<IBroker> pBroker;
   pGirderRptSpec->GetBroker(&pBroker);
   const CGirderKey& girderKey(pGirderRptSpec->GetGirderKey());

   GET_IFACE2(pBroker,IEAFDisplayUnits,pDisplayUnits);
   GET_IFACE2(pBroker,IBridge,pBridge);
   GET_IFACE2(pBroker,IMaterials,pMaterials);

   INIT_UV_PROTOTYPE( rptLengthUnitValue, vsRatio, pDisplayUnits->GetComponentDimUnit(), false );

   std::_tstring strImagePath(rptStyleManager::GetImagePath());

   rptChapter* pChapter = CPGSuperChapterBuilder::Build(pRptSpec,level);
   rptParagraph* pPara = new rptParagraph;
   *pChapter << pPara;

   *pPara << _T("Taken from \"Prediction of Creep, Shrinkage, and Temperature Effects in Concrete Structures\" by ACI Committee 209") << rptNewLine;
   *pPara << _T("ACI 209R-92, Reapproved 2008") << rptNewLine;
   *pPara << rptNewLine;

   rptRcTable* pTable = rptStyleManager::CreateDefaultTable(2);

   *pPara << pTable << rptNewLine;
   (*pTable)(0,0) << _T("Moist Curing");
   (*pTable)(0,1) << _T("Steam Curing");
   (*pTable)(1,0) << rptRcImage(strImagePath + _T("ACI209_Shrinkage_MoistCured.png")) << rptNewLine;
   (*pTable)(1,0) << rptRcImage(strImagePath + _T("ACI209_Shrinkage_MoistCured_CPFactor.png"));
   (*pTable)(1,1) << rptRcImage(strImagePath + _T("ACI209_Shrinkage_SteamCured.png"));

   pTable = rptStyleManager::CreateDefaultTable(7);
   *pPara << pTable << rptNewLine;

   pTable->SetColumnStyle(0,rptStyleManager::GetTableCellStyle(CB_NONE | CJ_LEFT));
   pTable->SetStripeRowColumnStyle(0,rptStyleManager::GetTableStripeRowCellStyle(CB_NONE | CJ_LEFT));


   RowIndexType rowIdx = 0;
   ColumnIndexType colIdx = 0;
   (*pTable)(rowIdx,colIdx++) << _T("Element");
   (*pTable)(rowIdx,colIdx++) << _T("Curing") << rptNewLine << _T("Method");
   (*pTable)(rowIdx,colIdx++) << Sub2(symbol(gamma),_T("cp"));
   (*pTable)(rowIdx,colIdx++) << COLHDR(_T("V/S"),rptLengthUnitTag,pDisplayUnits->GetComponentDimUnit());
   (*pTable)(rowIdx,colIdx++) << Sub2(symbol(gamma),_T("vs"));
   (*pTable)(rowIdx,colIdx++) << _T("H (%)");
   (*pTable)(rowIdx,colIdx++) << Sub2(symbol(gamma),_T("hs"));


   std::_tstring strCuring[] = { _T("Moist"), _T("Steam") };

   rowIdx = pTable->GetNumberOfHeaderRows();
   SegmentIndexType nSegments = pBridge->GetSegmentCount(girderKey);
   for ( SegmentIndexType segIdx = 0; segIdx < nSegments; segIdx++ )
   {
      ColumnIndexType colIdx = 0;

      CSegmentKey segmentKey(girderKey,segIdx);
      const matConcreteBase* pConcrete = pMaterials->GetSegmentConcrete(segmentKey);
      const matACI209Concrete* pACIConcrete = dynamic_cast<const matACI209Concrete*>(pConcrete);

      (*pTable)(rowIdx,colIdx++) << _T("Segment ") << LABEL_SEGMENT(segIdx);
      (*pTable)(rowIdx,colIdx++) << strCuring[pConcrete->GetCureMethod()];
      if ( pConcrete->GetCureMethod() == pgsTypes::Moist )
      {
         (*pTable)(rowIdx,colIdx++) << pACIConcrete->GetInitialMoistCureFactor();
      }
      else
      {
         (*pTable)(rowIdx,colIdx++) << _T("");
      }
      (*pTable)(rowIdx,colIdx++) << vsRatio.SetValue(pConcrete->GetVSRatio());
      (*pTable)(rowIdx,colIdx++) << pACIConcrete->GetSizeFactorShrinkage();
      (*pTable)(rowIdx,colIdx++) << pConcrete->GetRelativeHumidity();
      (*pTable)(rowIdx,colIdx++) << pACIConcrete->GetRelativeHumidityFactorShrinkage();

      rowIdx++;

      if ( segIdx != nSegments-1 )
      {
         CClosureKey closureKey(segmentKey);
         const matConcreteBase* pConcrete = pMaterials->GetClosureJointConcrete(closureKey);
         const matACI209Concrete* pACIConcrete = dynamic_cast<const matACI209Concrete*>(pConcrete);
   
         colIdx = 0;

         (*pTable)(rowIdx,colIdx++) << _T("Closure Joint");
         (*pTable)(rowIdx,colIdx++) << strCuring[pConcrete->GetCureMethod()];
         if ( pConcrete->GetCureMethod() == pgsTypes::Moist )
         {
            (*pTable)(rowIdx,colIdx++) << pACIConcrete->GetInitialMoistCureFactor();
         }
         else
         {
            (*pTable)(rowIdx,colIdx++) << _T("");
         }
         (*pTable)(rowIdx,colIdx++) << vsRatio.SetValue(pConcrete->GetVSRatio());
         (*pTable)(rowIdx,colIdx++) << pACIConcrete->GetSizeFactorShrinkage();
         (*pTable)(rowIdx,colIdx++) << pConcrete->GetRelativeHumidity();
         (*pTable)(rowIdx,colIdx++) << pACIConcrete->GetRelativeHumidityFactorShrinkage();

         rowIdx++;
      }
   }

   if (IsStructuralDeck(pBridge->GetDeckType()) )
   {
      GET_IFACE2(pBroker, IBridgeDescription, pIBridgeDesc);
      EventIndexType castDeckEventIdx = pIBridgeDesc->GetCastDeckEventIndex();
      const auto& castDeckActivity = pIBridgeDesc->GetEventByIndex(castDeckEventIdx)->GetCastDeckActivity();
      IndexType nCastings = castDeckActivity.GetCastingCount();
      for (IndexType castingIdx = 0; castingIdx < nCastings; castingIdx++)
      {
         std::vector<IndexType> vRegions = castDeckActivity.GetRegions(castingIdx);
         IndexType deckCastingRegionIdx = vRegions.front();
         const matConcreteBase* pConcrete = pMaterials->GetDeckConcrete(deckCastingRegionIdx);
         const matACI209Concrete* pACIConcrete = dynamic_cast<const matACI209Concrete*>(pConcrete);

         ColumnIndexType colIdx = 0;

         CString strTitle(_T("Deck"));
         if (1 < nCastings)
         {
            strTitle += _T(" Regions ");
            auto begin = std::begin(vRegions);
            auto iter = begin;
            auto end = std::end(vRegions);
            for (; iter != end; iter++)
            {
               if (iter != begin)
               {
                  strTitle += _T(", ");
               }
               CString strRegion;
               strRegion.Format(_T("%d"), LABEL_INDEX(*iter));
               strTitle += strRegion;
            }
         }


         (*pTable)(rowIdx, colIdx++) << strTitle;
         (*pTable)(rowIdx, colIdx++) << strCuring[pConcrete->GetCureMethod()];
         if (pConcrete->GetCureMethod() == pgsTypes::Moist)
         {
            (*pTable)(rowIdx, colIdx++) << pACIConcrete->GetInitialMoistCureFactor();
         }
         else
         {
            (*pTable)(rowIdx, colIdx++) << _T("");
         }
         (*pTable)(rowIdx, colIdx++) << vsRatio.SetValue(pConcrete->GetVSRatio());
         (*pTable)(rowIdx, colIdx++) << pACIConcrete->GetSizeFactorShrinkage();
         (*pTable)(rowIdx, colIdx++) << pConcrete->GetRelativeHumidity();
         (*pTable)(rowIdx, colIdx++) << pACIConcrete->GetRelativeHumidityFactorShrinkage();

         rowIdx++;
      } // next casting stage
   }

#if defined _DEBUG || defined _BETA_VERSION
   GET_IFACE2(pBroker, IBridgeDescription, pIBridgeDesc);
   EventIndexType castDeckIndex = pIBridgeDesc->GetCastDeckEventIndex();
   const auto* pEvent = pIBridgeDesc->GetEventByIndex(castDeckIndex);
   const auto& castDeckActivity = pEvent->GetCastDeckActivity();
   IndexType nCastings = castDeckActivity.GetCastingCount();
   IndexType nClosures = nSegments - 1;

   pTable = rptStyleManager::CreateDefaultTable(3 * (nSegments + nClosures + nCastings) + 1, _T("Debugging Table"));
   *pPara << pTable << rptNewLine;

   pTable->SetNumberOfHeaderRows(2);
   pTable->SetColumnStyle(0,rptStyleManager::GetTableCellStyle(CB_NONE | CJ_LEFT));
   pTable->SetStripeRowColumnStyle(0,rptStyleManager::GetTableStripeRowCellStyle(CB_NONE | CJ_LEFT));

   rowIdx = 0;
   colIdx = 0;

   pTable->SetRowSpan(rowIdx,colIdx,2);
   (*pTable)(rowIdx,colIdx++) << _T("Interval");
   for ( SegmentIndexType segIdx = 0; segIdx < nSegments; segIdx++ )
   {
      pTable->SetColumnSpan(rowIdx,colIdx,3);
      (*pTable)(rowIdx,colIdx) << _T("Segment ") << LABEL_SEGMENT(segIdx);
      (*pTable)(rowIdx+1,colIdx++) << _T("t") << rptNewLine << _T("(days)");
      (*pTable)(rowIdx+1,colIdx++) << symbol(DELTA) << Sub2(symbol(epsilon),_T("sh")) << _T("x10") << Super(_T("6"));
      (*pTable)(rowIdx+1,colIdx++) << Sub2(symbol(epsilon),_T("sh")) << _T("x10") << Super(_T("6"));
      if ( segIdx != nSegments-1 )
      {
         pTable->SetColumnSpan(rowIdx,colIdx,3);
         (*pTable)(rowIdx,colIdx) << _T("Closure Joint ") << LABEL_SEGMENT(segIdx);
         (*pTable)(rowIdx+1,colIdx++) << _T("t") << rptNewLine << _T("(days)");
         (*pTable)(rowIdx+1,colIdx++) << symbol(DELTA) << Sub2(symbol(epsilon),_T("sh")) << _T("x10") << Super(_T("6"));
         (*pTable)(rowIdx+1,colIdx++) << Sub2(symbol(epsilon),_T("sh")) << _T("x10") << Super(_T("6"));
      }
   }

   for (IndexType castingIdx = 0; castingIdx < nCastings; castingIdx++)
   {
      pTable->SetColumnSpan(rowIdx, colIdx, 3);

      CString strTitle(_T("Deck"));
      if (1 < nCastings)
      {
         std::vector<IndexType> vRegions = castDeckActivity.GetRegions(castingIdx);

         strTitle += _T(" Regions ");
         auto begin = std::begin(vRegions);
         auto iter = begin;
         auto end = std::end(vRegions);
         for (; iter != end; iter++)
         {
            if (iter != begin)
            {
               strTitle += _T(", ");
            }
            CString strRegion;
            strRegion.Format(_T("%d"), LABEL_INDEX(*iter));
            strTitle += strRegion;
         }
      }

      (*pTable)(rowIdx, colIdx) << strTitle;
      (*pTable)(rowIdx + 1, colIdx++) << _T("t") << rptNewLine << _T("(days)");
      (*pTable)(rowIdx + 1, colIdx++) << symbol(DELTA) << Sub2(symbol(epsilon), _T("sh")) << _T("x10") << Super(_T("6"));
      (*pTable)(rowIdx + 1, colIdx++) << Sub2(symbol(epsilon), _T("sh")) << _T("x10") << Super(_T("6"));
   }

   GET_IFACE2(pBroker,IIntervals,pIntervals);
   IntervalIndexType nIntervals = pIntervals->GetIntervalCount();

   rowIdx = pTable->GetNumberOfHeaderRows();
   for ( IntervalIndexType intervalIdx = 0; intervalIdx < nIntervals; intervalIdx++, rowIdx++ )
   {
      colIdx = 0;
      (*pTable)(rowIdx,colIdx++) << LABEL_INTERVAL(intervalIdx);

      for ( SegmentIndexType segIdx = 0; segIdx < nSegments; segIdx++ )
      {
         CSegmentKey segmentKey(girderKey,segIdx);
         IntervalIndexType releaseIntervalIdx = pIntervals->GetPrestressReleaseInterval(segmentKey);
         if ( releaseIntervalIdx <= intervalIdx )
         {
            Float64 t  = pMaterials->GetSegmentConcreteAge(segmentKey,intervalIdx,pgsTypes::End);
            const matConcreteBase* pConcrete = pMaterials->GetSegmentConcrete(segmentKey);
            const matACI209Concrete* pACIConcrete = dynamic_cast<const matACI209Concrete*>(pConcrete);
            Float64 cure = pACIConcrete->GetCureTime();
            (*pTable)(rowIdx,colIdx++) << t - cure;
            (*pTable)(rowIdx,colIdx++) << 1E6*pMaterials->GetIncrementalSegmentFreeShrinkageStrain(segmentKey,intervalIdx);
            (*pTable)(rowIdx,colIdx++) << 1E6*pMaterials->GetTotalSegmentFreeShrinkageStrain(segmentKey,intervalIdx,pgsTypes::End);
         }
         else
         {
            (*pTable)(rowIdx,colIdx++) << _T("");
            (*pTable)(rowIdx,colIdx++) << _T("");
            (*pTable)(rowIdx,colIdx++) << _T("");
         }

         if ( segIdx < nSegments-1 )
         {
            CClosureKey closureKey(segmentKey);
            IntervalIndexType compositeClosureIntervalIdx = pIntervals->GetCompositeClosureJointInterval(closureKey);
            if ( compositeClosureIntervalIdx <= intervalIdx )
            {
               Float64 t  = pMaterials->GetClosureJointConcreteAge(closureKey,intervalIdx,pgsTypes::End);
               const matConcreteBase* pConcrete = pMaterials->GetClosureJointConcrete(closureKey);
               const matACI209Concrete* pACIConcrete = dynamic_cast<const matACI209Concrete*>(pConcrete);
               Float64 cure = pACIConcrete->GetCureTime();
               (*pTable)(rowIdx,colIdx++) << t - cure;
               (*pTable)(rowIdx,colIdx++) << 1E6*pMaterials->GetIncrementalClosureJointFreeShrinkageStrain(closureKey,intervalIdx);
               (*pTable)(rowIdx,colIdx++) << 1E6*pMaterials->GetTotalClosureJointFreeShrinkageStrain(closureKey,intervalIdx,pgsTypes::End);
            }
            else
            {
               (*pTable)(rowIdx,colIdx++) << _T("");
               (*pTable)(rowIdx,colIdx++) << _T("");
               (*pTable)(rowIdx,colIdx++) << _T("");
            }
         }
      }

      for (IndexType castingIdx = 0; castingIdx < nCastings; castingIdx++)
      {
         std::vector<IndexType> vRegions = castDeckActivity.GetRegions(castingIdx);
         IndexType deckCastingRegionIdx = vRegions.front();
         IntervalIndexType compositeDeckIntervalIdx = pIntervals->GetCompositeDeckInterval(deckCastingRegionIdx);
         if ( compositeDeckIntervalIdx <= intervalIdx )
         {
            Float64 t  = pMaterials->GetDeckConcreteAge(deckCastingRegionIdx,intervalIdx,pgsTypes::End);
            const matConcreteBase* pConcrete = pMaterials->GetDeckConcrete(deckCastingRegionIdx);
            const matACI209Concrete* pACIConcrete = dynamic_cast<const matACI209Concrete*>(pConcrete);
            Float64 cure = pACIConcrete->GetCureTime();
            (*pTable)(rowIdx,colIdx++) << t - cure;
            (*pTable)(rowIdx,colIdx++) << 1E6*pMaterials->GetIncrementalDeckFreeShrinkageStrain(deckCastingRegionIdx,intervalIdx);
            (*pTable)(rowIdx,colIdx++) << 1E6*pMaterials->GetTotalDeckFreeShrinkageStrain(deckCastingRegionIdx,intervalIdx,pgsTypes::End);
         }
         else
         {
            (*pTable)(rowIdx,colIdx++) << _T("");
            (*pTable)(rowIdx,colIdx++) << _T("");
            (*pTable)(rowIdx,colIdx++) << _T("");
         }
      }
   }
#endif //#if defined _DEBUG || defined _BETA_VERSION


   return pChapter;
}

CChapterBuilder* CACI209ShrinkageStrainChapterBuilder::Clone() const
{
   return new CACI209ShrinkageStrainChapterBuilder;
}
