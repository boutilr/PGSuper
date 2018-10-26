///////////////////////////////////////////////////////////////////////
// PGSuper - Prestressed Girder SUPERstructure Design and Analysis
// Copyright � 1999-2014  Washington State Department of Transportation
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
#include "resource.h"
#include <Graphing\TendonStressGraphBuilder.h>
#include <Graphing\DrawBeamTool.h>
#include "TendonStressGraphController.h"
#include "GraphColor.h"

#include "GraphColor.h"

#include <EAF\EAFUtilities.h>
#include <EAF\EAFDisplayUnits.h>
#include <EAF\EAFAutoProgress.h>
#include <PgsExt\PhysicalConverter.h>

#include <IFace\Intervals.h>
#include <IFace\Bridge.h>
#include <IFace\Project.h>
#include <IFace\PrestressForce.h>

#include <EAF\EAFGraphView.h>

#include <MFCTools\MFCTools.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

BEGIN_MESSAGE_MAP(CTendonStressGraphBuilder, CGirderGraphBuilderBase)
END_MESSAGE_MAP()


CTendonStressGraphBuilder::CTendonStressGraphBuilder() :
CGirderGraphBuilderBase()
{
   SetName(_T("Effective Prestress"));
}

CTendonStressGraphBuilder::CTendonStressGraphBuilder(const CTendonStressGraphBuilder& other) :
CGirderGraphBuilderBase(other)
{
}

CTendonStressGraphBuilder::~CTendonStressGraphBuilder()
{
}

BOOL CTendonStressGraphBuilder::CreateGraphController(CWnd* pParent,UINT nID)
{
   AFX_MANAGE_STATE(AfxGetStaticModuleState());
   ATLASSERT(m_pGraphController != NULL);
   return m_pGraphController->Create(pParent,IDD_TENDONSTRESS_GRAPH_CONTROLLER, CBRS_LEFT, nID);
}

CGraphBuilder* CTendonStressGraphBuilder::Clone()
{
   // set the module state or the commands wont route to the
   // the graph control window
   AFX_MANAGE_STATE(AfxGetStaticModuleState());
   return new CTendonStressGraphBuilder(*this);
}

void CTendonStressGraphBuilder::UpdateXAxis()
{
   CGirderGraphBuilderBase::UpdateXAxis();
   m_Graph.SetXAxisTitle(_T("Distance From Left End of Girder (")+m_pXFormat->UnitTag()+_T(")"));
}

void CTendonStressGraphBuilder::UpdateYAxis()
{
   if ( m_pYFormat )
   {
      delete m_pYFormat;
      m_pYFormat = NULL;
   }

   m_Graph.SetYAxisNiceRange(true);
   m_Graph.SetYAxisNumberOfMinorTics(5);
   m_Graph.SetYAxisNumberOfMajorTics(21);
   m_Graph.SetPinYAxisAtZero(true);

   GET_IFACE(IEAFDisplayUnits,pDisplayUnits);
   if ( ((CTendonStressGraphController*)m_pGraphController)->IsStressGraph() )
   {
      const unitmgtStressData& stressUnit = pDisplayUnits->GetStressUnit();
      m_pYFormat = new StressTool(stressUnit);
      m_Graph.SetYAxisValueFormat(*m_pYFormat);
      m_Graph.SetYAxisTitle(_T("Stress (")+m_pYFormat->UnitTag()+_T(")"));
   }
   else
   {
      const unitmgtForceData& forceUnit = pDisplayUnits->GetGeneralForceUnit();
      m_pYFormat = new ForceTool(forceUnit);
      m_Graph.SetYAxisValueFormat(*m_pYFormat);
      m_Graph.SetYAxisTitle(_T("Force (")+m_pYFormat->UnitTag()+_T(")"));
   }
}

CGirderGraphControllerBase* CTendonStressGraphBuilder::CreateGraphController()
{
   AFX_MANAGE_STATE(AfxGetStaticModuleState());
   return new CTendonStressGraphController;
}

bool CTendonStressGraphBuilder::UpdateNow()
{
   GET_IFACE(IProgress,pProgress);
   CEAFAutoProgress ap(pProgress);

   pProgress->UpdateMessage(_T("Building Graph"));

   CWaitCursor wait;

   CGirderGraphBuilderBase::UpdateNow();

   // Update graph properties
   GroupIndexType    grpIdx      = m_pGraphController->GetGirderGroup();
   GirderIndexType   gdrIdx      = m_pGraphController->GetGirder();
   DuctIndexType     ductIdx     = ((CTendonStressGraphController*)m_pGraphController)->GetDuct();

   UpdateGraphTitle(grpIdx,gdrIdx,ductIdx);

   UpdateYAxis();

   // get data to graph
   UpdateGraphData(grpIdx,gdrIdx,ductIdx);

   return true;
}

void CTendonStressGraphBuilder::UpdateGraphTitle(GroupIndexType grpIdx,GirderIndexType gdrIdx,DuctIndexType ductIdx)
{
   CString strGraphSubTitle;
   if ( ductIdx == INVALID_INDEX )
      strGraphSubTitle.Format(_T("Group %d Girder %s"),LABEL_GROUP(grpIdx),LABEL_GIRDER(gdrIdx));
   else
      strGraphSubTitle.Format(_T("Group %d Girder %s Tendon %d"),LABEL_GROUP(grpIdx),LABEL_GIRDER(gdrIdx),LABEL_DUCT(ductIdx));
   
   m_Graph.SetTitle(_T("Effective Prestress"));
   m_Graph.SetSubtitle(std::_tstring(strGraphSubTitle));
}

void CTendonStressGraphBuilder::UpdateGraphData(GroupIndexType grpIdx,GirderIndexType gdrIdx,DuctIndexType ductIdx)
{
   if ( ductIdx == INVALID_INDEX )
   {
      UpdatePretensionGraphData(grpIdx,gdrIdx);
   }
   else
   {
      UpdatePosttensionGraphData(grpIdx,gdrIdx,ductIdx);
   }
}

void CTendonStressGraphBuilder::UpdatePosttensionGraphData(GroupIndexType grpIdx,GirderIndexType gdrIdx,DuctIndexType ductIdx)
{
   // clear graph
   m_Graph.ClearData();

   // Get the points of interest we need.
   GET_IFACE(IPointOfInterest,pIPoi);
   CSegmentKey segmentKey(grpIdx,gdrIdx,ALL_SEGMENTS);
   std::vector<pgsPointOfInterest> vPoi( pIPoi->GetPointsOfInterest( segmentKey ) );

   // Map POI coordinates to X-values for the graph
   std::vector<Float64> xVals;
   GetXValues(vPoi,xVals);


   IntervalIndexType firstIntervalIdx = ((CMultiIntervalGirderGraphControllerBase*)m_pGraphController)->GetFirstInterval();
   IntervalIndexType lastIntervalIdx  = ((CMultiIntervalGirderGraphControllerBase*)m_pGraphController)->GetLastInterval();
   IntervalIndexType nGraphs = lastIntervalIdx - firstIntervalIdx + 1;
   CGraphColor graphColor(nGraphs);

   std::vector<IntervalIndexType> vIntervals = ((CMultiIntervalGirderGraphControllerBase*)m_pGraphController)->GetSelectedIntervals();

   GET_IFACE(IIntervals,pIntervals);
   IntervalIndexType stressTendonIntervalIdx = pIntervals->GetStressTendonInterval(CGirderKey(grpIdx,gdrIdx),ductIdx);

   GET_IFACE(IPosttensionForce,pPTForce); // going to need this in the loop... get it once


   bool bStresses = ((CTendonStressGraphController*)m_pGraphController)->IsStressGraph();

   // remove all intervals that occur before the tendon is stressed
   vIntervals.erase(std::remove_if(vIntervals.begin(),vIntervals.end(),std::bind2nd(std::less<IntervalIndexType>(),stressTendonIntervalIdx)),vIntervals.end());

   std::vector<IntervalIndexType>::iterator intervalIter(vIntervals.begin());
   std::vector<IntervalIndexType>::iterator intervalIterEnd(vIntervals.end());
   for ( ; intervalIter != intervalIterEnd; intervalIter++ )
   {
      IntervalIndexType intervalIdx = *intervalIter;

      COLORREF color = graphColor.GetColor(intervalIdx-firstIntervalIdx);

      IndexType dataSeries1, dataSeries2;
      if ( intervalIdx == stressTendonIntervalIdx )
      {
         dataSeries1 = m_Graph.CreateDataSeries(_T("fpt prior to anchor set"),PS_SOLID,1,ORANGE);
         dataSeries2 = m_Graph.CreateDataSeries(_T("fpt after anchor set"),PS_SOLID,1,GREEN);
      }
      else
      {
         CString strLabel;
         strLabel.Format(_T("Interval %d"),LABEL_INTERVAL(intervalIdx));
         dataSeries1 = m_Graph.CreateDataSeries(strLabel,PS_SOLID,1,color);
      }

      std::vector<pgsPointOfInterest>::iterator iter(vPoi.begin());
      std::vector<pgsPointOfInterest>::iterator end(vPoi.end());
      std::vector<Float64>::iterator xIter(xVals.begin());
      for ( ; iter != end; iter++, xIter++ )
      {
         pgsPointOfInterest& poi = *iter;

         Float64 X = *xIter;

         if ( stressTendonIntervalIdx == intervalIdx )
         {
            if ( bStresses )
            {
               Float64 fpbt = pPTForce->GetTendonStress(poi,stressTendonIntervalIdx,pgsTypes::Start,ductIdx);
               Float64 fpat = pPTForce->GetTendonStress(poi,stressTendonIntervalIdx,pgsTypes::End,  ductIdx);

               AddGraphPoint(dataSeries1,X,fpbt);
               AddGraphPoint(dataSeries2,X,fpat);
            }
            else
            {
               Float64 Fpbt = pPTForce->GetTendonForce(poi,stressTendonIntervalIdx,pgsTypes::Start,ductIdx);
               Float64 Fpat = pPTForce->GetTendonForce(poi,stressTendonIntervalIdx,pgsTypes::End,  ductIdx);

               AddGraphPoint(dataSeries1,X,Fpbt);
               AddGraphPoint(dataSeries2,X,Fpat);
            }
         }
         else
         {
            if ( bStresses )
            {
               Float64 fpe  = pPTForce->GetTendonStress(poi,intervalIdx,pgsTypes::End,  ductIdx);
               AddGraphPoint(dataSeries1,X,fpe);
            }
            else
            {
               Float64 Fpe  = pPTForce->GetTendonForce(poi,intervalIdx,pgsTypes::End,  ductIdx);
               AddGraphPoint(dataSeries1,X,Fpe);
            }
         }
      }
   } // next interval
}

void CTendonStressGraphBuilder::UpdatePretensionGraphData(GroupIndexType grpIdx,GirderIndexType gdrIdx)
{
   // clear graph
   m_Graph.ClearData();

   // Get the points of interest we need.
   GET_IFACE(IPointOfInterest,pIPoi);
   CSegmentKey segmentKey(grpIdx,gdrIdx,ALL_SEGMENTS);
   std::vector<pgsPointOfInterest> vPoi( pIPoi->GetPointsOfInterest( segmentKey ) );

   // Map POI coordinates to X-values for the graph
   std::vector<Float64> xVals;
   GetXValues(vPoi,xVals);

   GET_IFACE(IPretensionForce,pPSForce);
   GET_IFACE(IIntervals,pIntervals);

   bool bStresses = ((CTendonStressGraphController*)m_pGraphController)->IsStressGraph();

   IntervalIndexType firstIntervalIdx = ((CMultiIntervalGirderGraphControllerBase*)m_pGraphController)->GetFirstInterval();
   IntervalIndexType lastIntervalIdx  = ((CMultiIntervalGirderGraphControllerBase*)m_pGraphController)->GetLastInterval();
   IntervalIndexType nGraphs = lastIntervalIdx - firstIntervalIdx + 1;
   CGraphColor graphColor(nGraphs);

   std::vector<IntervalIndexType> vIntervals = ((CMultiIntervalGirderGraphControllerBase*)m_pGraphController)->GetSelectedIntervals();

   std::vector<IntervalIndexType>::iterator intervalIter(vIntervals.begin());
   std::vector<IntervalIndexType>::iterator intervalIterEnd(vIntervals.end());
   for ( ; intervalIter != intervalIterEnd; intervalIter++ )
   {
      IntervalIndexType intervalIdx = *intervalIter;

      COLORREF color = graphColor.GetColor(intervalIdx-firstIntervalIdx);

      CString strLabel;
      strLabel.Format(_T("Interval %d"),LABEL_INTERVAL(intervalIdx));
      IndexType dataSeries = m_Graph.CreateDataSeries(strLabel,PS_SOLID,1,color);
      IndexType dataSeries2 = m_Graph.CreateDataSeries(_T(""),PS_SOLID,1,color);

      std::vector<pgsPointOfInterest>::iterator iter(vPoi.begin());
      std::vector<pgsPointOfInterest>::iterator end(vPoi.end());
      std::vector<Float64>::iterator xIter(xVals.begin());
      for ( ; iter != end; iter++, xIter++ )
      {
         pgsPointOfInterest& poi = *iter;
         const CSegmentKey& thisSegmentKey(poi.GetSegmentKey());

         IntervalIndexType stressStrandIntervalIdx = pIntervals->GetStressStrandInterval(thisSegmentKey);
         IntervalIndexType releaseIntervalIdx      = pIntervals->GetPrestressReleaseInterval(thisSegmentKey);

         Float64 X = *xIter;

         pgsTypes::IntervalTimeType time = pgsTypes::End; // always plot fpe at end of interval
         if (intervalIdx == stressStrandIntervalIdx || intervalIdx == releaseIntervalIdx)
         {
            time = pgsTypes::Start; // except if this is the strand stress or release intervals
                                    // we want to capture fpj and strand stress just before
                                    // release so there isn't any elastic shortening effect, only initial relaxation
         }

#pragma Reminder("UPDATE: add plot for temporary strands if they are modeled")
         
         if ( bStresses )
         {
            Float64 fpe = pPSForce->GetEffectivePrestress(poi,pgsTypes::Permanent,intervalIdx,time);
            AddGraphPoint(dataSeries,X,fpe);
         }
         else
         {
            Float64 Fpe = pPSForce->GetPrestressForce(poi,pgsTypes::Permanent,intervalIdx,time);
            AddGraphPoint(dataSeries,X,Fpe);
         }

         if ( intervalIdx == releaseIntervalIdx )
         {
            // if this is at release, also plot at the end of the interval so we capture the
            // elastic shortening that occurs during this interval
            if ( bStresses )
            {
               Float64 fpe = pPSForce->GetEffectivePrestress(poi,pgsTypes::Permanent,intervalIdx,pgsTypes::End);
               AddGraphPoint(dataSeries2,X,fpe);
            }
            else
            {
               Float64 Fpe = pPSForce->GetPrestressForce(poi,pgsTypes::Permanent,intervalIdx,pgsTypes::End);
               AddGraphPoint(dataSeries2,X,Fpe);
            }
         }
      } // next poi
   } // next interval
}

IntervalIndexType CTendonStressGraphBuilder::GetBeamDrawInterval()
{
   std::vector<IntervalIndexType> vIntervals(((CTendonStressGraphController*)m_pGraphController)->GetSelectedIntervals());
   if ( 0 < vIntervals.size() )
      return vIntervals.back();

   return 0;
}
