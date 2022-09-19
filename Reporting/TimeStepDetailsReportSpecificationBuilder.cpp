///////////////////////////////////////////////////////////////////////
// PGSuper - Prestressed Girder SUPERstructure Design and Analysis
// Copyright � 1999-2022  Washington State Department of Transportation
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
#include <Reporting\TimeStepDetailsReportSpecificationBuilder.h>
#include <Reporting\TimeStepDetailsReportSpecification.h>
#include "TimeStepDetailsDlg.h"

#include <IFace\Selection.h>
#include <IFace\PointOfInterest.h>


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CTimeStepDetailsReportSpecificationBuilder::CTimeStepDetailsReportSpecificationBuilder(IBroker* pBroker) :
CBrokerReportSpecificationBuilder(pBroker)
{
}

CTimeStepDetailsReportSpecificationBuilder::~CTimeStepDetailsReportSpecificationBuilder(void)
{
}

std::shared_ptr<WBFL::Reporting::ReportSpecification> CTimeStepDetailsReportSpecificationBuilder::CreateReportSpec(const WBFL::Reporting::ReportDescription& rptDesc,std::shared_ptr<WBFL::Reporting::ReportSpecification>& pOldRptSpec) const
{
   AFX_MANAGE_STATE(AfxGetStaticModuleState());

   // Prompt for span, girder, and chapter list
   // initialize dialog for the current cut location
   std::shared_ptr<CTimeStepDetailsReportSpecification> pInitRptSpec( std::dynamic_pointer_cast<CTimeStepDetailsReportSpecification>(pOldRptSpec) );

   pgsPointOfInterest initial_poi;
   if ( pInitRptSpec )
   {
      initial_poi = pInitRptSpec->GetPointOfInterest();
   }
   else
   {
      GET_IFACE(ISelection,pSelection);
      CSelection selection = pSelection->GetSelection();
      CGirderKey girderKey;
      if ( selection.Type == CSelection::Girder || selection.Type == CSelection::Segment )
      {
         girderKey.groupIndex   = selection.GroupIdx;
         girderKey.girderIndex  = selection.GirderIdx;
      }
      else
      {
         girderKey.groupIndex   = 0;
         girderKey.girderIndex  = 0;
      }

      GET_IFACE(IPointOfInterest,pPoi);
      PoiList vPoi;
      pPoi->GetPointsOfInterest(CSegmentKey(girderKey, ALL_SEGMENTS), POI_5L | POI_SPAN, &vPoi);
      ATLASSERT(0 < vPoi.size());
      initial_poi = vPoi.front().get();
   }

   CTimeStepDetailsDlg dlg(m_pBroker,pInitRptSpec,initial_poi,INVALID_INDEX);

   if ( dlg.DoModal() == IDOK )
   {
      std::shared_ptr<WBFL::Reporting::ReportSpecification> pNewRptSpec;
      if(pInitRptSpec)
      {
         std::shared_ptr<CTimeStepDetailsReportSpecification> pNewGRptSpec(std::make_shared<CTimeStepDetailsReportSpecification>(*pInitRptSpec) );

         pNewGRptSpec->SetOptions(dlg.UseAllLocations(),dlg.GetPOI(),dlg.GetInterval());

         pNewRptSpec = std::static_pointer_cast<WBFL::Reporting::ReportSpecification>(pNewGRptSpec);
      }
      else
      {
         pNewRptSpec = std::make_shared<CTimeStepDetailsReportSpecification>(rptDesc.GetReportName(),m_pBroker,dlg.UseAllLocations(),dlg.GetPOI(),dlg.GetInterval());
      }

      rptDesc.ConfigureReportSpecification(pNewRptSpec);

      return pNewRptSpec;
   }

   return nullptr;
}

std::shared_ptr<WBFL::Reporting::ReportSpecification> CTimeStepDetailsReportSpecificationBuilder::CreateDefaultReportSpec(const WBFL::Reporting::ReportDescription& rptDesc) const
{
   // always prompt
   return CreateReportSpec(rptDesc,std::shared_ptr<WBFL::Reporting::ReportSpecification>());
}
