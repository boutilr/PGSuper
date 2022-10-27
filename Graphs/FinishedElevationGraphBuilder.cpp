///////////////////////////////////////////////////////////////////////
// PGSuper - Prestressed Girder SUPERstructure Design and Analysis
// Copyright © 1999-2022  Washington State Department of Transportation
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
#include <Graphs/FinishedElevationGraphBuilder.h>
#include <Graphs/DrawBeamTool.h>
#include <Graphs/ExportGraphXYTool.h>
#include "FinishedElevationGraphController.h"
#include "FinishedElevationGraphViewControllerImp.h"
#include "..\Documentation\PGSuper.hh"

#include <EAF\EAFUtilities.h>
#include <EAF\EAFDisplayUnits.h>
#include <EAF\EAFAutoProgress.h>
#include <Units\UnitValueNumericalFormatTools.h>

#include <IFace\Intervals.h>
#include <IFace\Bridge.h>
#include <IFace\Project.h>
#include <IFace\DocumentType.h>
#include <IFace\Alignment.h>

#include <EAF\EAFGraphView.h>
#include <EAF\EAFDocument.h>

#include <MFCTools\MFCTools.h>

#include <algorithm>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

static const int PenWeight = GRAPH_PEN_WEIGHT;
static const int PglWeight = GRAPH_PEN_WEIGHT + 1; // heavier weight for PGL

BEGIN_MESSAGE_MAP(CFinishedElevationGraphBuilder, CGirderGraphBuilderBase)
END_MESSAGE_MAP()

CFinishedElevationGraphBuilder::CFinishedElevationGraphBuilder() :
   CGirderGraphBuilderBase(),
   m_bShowPGL(TRUE),
   m_bShowFinishedDeck(FALSE),
   m_bShowFinishedDeckBottom(FALSE),
   m_bShowFinishedGirderBottom(FALSE),
   m_bShowFinishedGirderTop(FALSE),
   m_ShowGirderChord(FALSE),
   m_GraphType(gtElevations),
   m_GraphSide(gsCenterLine)
{
   SetName(_T("Finished Elevations"));
   
   InitDocumentation(EAFGetDocument()->GetDocumentationSetName(),IDH_FINISHED_ELEVATION_VIEW);
}

CFinishedElevationGraphBuilder::~CFinishedElevationGraphBuilder()
{
}

BOOL CFinishedElevationGraphBuilder::CreateGraphController(CWnd* pParent,UINT nID)
{
   AFX_MANAGE_STATE(AfxGetStaticModuleState());
   ATLASSERT(m_pGraphController != nullptr);
   return m_pGraphController->Create(pParent,IDD_FINISHED_ELEVATION_GRAPH_CONTROLLER, CBRS_LEFT, nID);
}

void CFinishedElevationGraphBuilder::CreateViewController(IEAFViewController** ppController)
{
   CComPtr<IEAFViewController> stdController;
   __super::CreateViewController(&stdController);

   CComObject<CFinishedElevationGraphViewController>* pController;
   CComObject<CFinishedElevationGraphViewController>::CreateInstance(&pController);
   pController->Init((CFinishedElevationGraphController*)m_pGraphController, stdController);

   (*ppController) = pController;
   (*ppController)->AddRef();
}

std::unique_ptr<WBFL::Graphing::GraphBuilder> CFinishedElevationGraphBuilder::Clone() const
{
   // set the module state or the commands wont route to the
   // the graph control window
   AFX_MANAGE_STATE(AfxGetStaticModuleState());
   return std::make_unique<CFinishedElevationGraphBuilder>(*this);
}

void CFinishedElevationGraphBuilder::UpdateXAxis()
{
   CGirderGraphBuilderBase::UpdateXAxis();
   m_Graph.SetXAxisTitle(std::_tstring(_T("Distance From Left End of Girder (")+m_pXFormat->UnitTag()+_T(")")).c_str());
}

void CFinishedElevationGraphBuilder::UpdateYAxis()
{
   if ( m_pYFormat )
   {
      delete m_pYFormat;
      m_pYFormat = nullptr;
   }

   m_Graph.YAxisNiceRange(true);
   m_Graph.SetYAxisNumberOfMinorTics(5);
   m_Graph.SetYAxisNumberOfMajorTics(21);
   m_Graph.PinYAxisAtZero(false);

   GET_IFACE(IEAFDisplayUnits,pDisplayUnits);
   const WBFL::Units::LengthData& lengthUnit = pDisplayUnits->GetSpanLengthUnit();
   m_pYFormat = new WBFL::Units::LengthTool(lengthUnit);
   m_Graph.SetYAxisValueFormat(*m_pYFormat);

   if (gtElevationDifferential == m_GraphType)
   {
      m_Graph.SetYAxisTitle(std::_tstring(_T("Elevation Differential (") + m_pYFormat->UnitTag() + _T(")")).c_str());
   }
   else
   {
      m_Graph.SetYAxisTitle(std::_tstring(_T("Elevation (") + m_pYFormat->UnitTag() + _T(")")).c_str());
   }
}

CGirderGraphControllerBase* CFinishedElevationGraphBuilder::CreateGraphController()
{
   AFX_MANAGE_STATE(AfxGetStaticModuleState());
   return new CFinishedElevationGraphController;
}

bool CFinishedElevationGraphBuilder::UpdateNow()
{
   GET_IFACE(IProgress,pProgress);
   CEAFAutoProgress ap(pProgress);

   pProgress->UpdateMessage(_T("Building Graph"));

   CWaitCursor wait;

   CGirderGraphBuilderBase::UpdateNow();

   // Update graph properties
   GroupIndexType    grpIdx      = m_pGraphController->GetGirderGroup();
   GirderIndexType   gdrIdx      = m_pGraphController->GetGirder();
   UpdateGraphTitle(grpIdx,gdrIdx);

   UpdateYAxis();

   // get data to graph
   UpdateGraphData(grpIdx,gdrIdx);

   return true;
}

void CFinishedElevationGraphBuilder::UpdateGraphTitle(GroupIndexType grpIdx,GirderIndexType gdrIdx)
{
   if (gtElevationDifferential == m_GraphType)
   {
      m_Graph.SetTitle(_T("Elevation Differentials from PGL"));
   }
   else
   {
      m_Graph.SetTitle(_T("Design and Finished Roadway Elevations"));
   }

   CString strGraphSubTitle;
   strGraphSubTitle.Format(_T("Group %d Girder %s"), LABEL_GROUP(grpIdx), LABEL_GIRDER(gdrIdx));
   if (gsLeftEdge == m_GraphSide)
   {
      strGraphSubTitle += _T(" - along Left Top Edge of Girder");
   }
   else if (gsCenterLine == m_GraphSide)
   {
      strGraphSubTitle += _T(" - along Centerline of Girder");
   }
   else
   {
      strGraphSubTitle += _T(" - along Right Top Edge of Girder");
   }

   m_Graph.SetSubtitle(strGraphSubTitle);
}

void CFinishedElevationGraphBuilder::UpdateGraphData(GroupIndexType grpIdx,GirderIndexType gdrIdx)
{
   GET_IFACE_NOCHECK(IBridge,pBridge);
   GET_IFACE_NOCHECK(IGirder,pGirder);

   // clear graph
   m_Graph.ClearData();

   // Get the points of interest we need.
   GET_IFACE(IPointOfInterest,pIPoi);
   CSegmentKey segmentKey(grpIdx,gdrIdx,ALL_SEGMENTS);
   PoiList vPoi;
   pIPoi->GetPointsOfInterest(segmentKey, &vPoi);

   // Map POI coordinates to X-values for the graph
   std::vector<Float64> xVals;
   GetXValues(vPoi,&xVals);

   // Tackle simpler stuff first
   // Most graphs will want the PGL, so just store it
   GET_IFACE(IRoadway,pRoadway);
   std::vector<Float64> PglElevations;
   PglElevations.reserve(vPoi.size());
   for (auto poi : vPoi)
   {
      Float64 station,offset;
      pBridge->GetStationAndOffset(poi,&station,&offset);

      Float64 deltaOffset(0.0);
      if (m_GraphSide != gsCenterLine)
      {
         Float64 leftEdge,rightEdge;
         pGirder->GetTopWidth(poi,&leftEdge,&rightEdge);
         deltaOffset = m_GraphSide == gsLeftEdge ? -1.0 *leftEdge : rightEdge;
      }

      Float64 elev = pRoadway->GetElevation(station,offset+deltaOffset);
      PglElevations.push_back(elev);
   }

   if (m_bShowPGL)
   {
      CString strRWLabel(_T("Design Roadway (PGL)"));
      IndexType dataRwSeries = m_Graph.CreateDataSeries(strRWLabel,PS_SOLID,PglWeight,BLUE);

      auto iter(PglElevations.begin());
      auto end(PglElevations.end());
      auto xIter(xVals.begin());
      for (; iter != end; iter++,xIter++)
      {
         Float64 elev = *iter;
         Float64 X = *xIter;
         AddGraphPoint(dataRwSeries,X,elev);
      }
   }

   WBFL::Graphing::GraphColor graphColor;
   if (m_ShowGirderChord)
   {
      CString strGCLabel(_T("Girder CL Top Chord"));
      IndexType dataGcSeries = m_Graph.CreateDataSeries(strGCLabel,PS_DASH,PenWeight,GREEN);
      auto xIter(xVals.begin());
      auto pglIter(PglElevations.begin());
      for (auto poi : vPoi)
      {
         // Chord is always at CL girder
         Float64 girder_chord_elevation = pGirder->GetTopGirderChordElevation(poi);
         Float64 X = *xIter++;

         if (gtElevationDifferential == m_GraphType)
         {
            Float64 pglElev = *pglIter++;
            AddGraphPoint(dataGcSeries,X,girder_chord_elevation-pglElev);
         }
         else
         {
            AddGraphPoint(dataGcSeries,X,girder_chord_elevation);
         }
      }
   }

   // Don't compute values unless needed
   if (m_bShowFinishedDeck || m_bShowFinishedDeckBottom || m_bShowFinishedGirderBottom || m_bShowFinishedGirderTop)
   {
      std::vector<IntervalIndexType> vIntervals = ((CMultiIntervalGirderGraphControllerBase*)m_pGraphController)->GetSelectedIntervals();

      pgsTypes::SupportedDeckType deckType = pBridge->GetDeckType();

      GET_IFACE_NOCHECK(IDeformedGirderGeometry,pDeformedGirderGeometry);
      pgsTypes::HaunchInputDepthType haunchInputType = pBridge->GetHaunchInputDepthType();
      if (deckType == pgsTypes::sdtNone)
      {
         // No deck
         for (auto intervalIdx : vIntervals)
         {
            // Need this data for all possible graphs
            std::vector<Float64> finishedDeckElevations;
            finishedDeckElevations.reserve(vPoi.size());
            for (const auto& poi : vPoi)
            {
               Float64 lftElev,clElev,rgtElev;
               pDeformedGirderGeometry->GetFinishedElevation(poi,nullptr,true/*bIncludeOverlay*/,&lftElev,&clElev,&rgtElev);

               if (m_GraphSide == gsCenterLine)
               {
                  finishedDeckElevations.push_back(clElev);
               }
               else if (m_GraphSide == gsLeftEdge)
               {
                  finishedDeckElevations.push_back(lftElev);
               }
               else
               {
                  finishedDeckElevations.push_back(rgtElev);
               }
            }

            if (m_bShowFinishedDeck)
            {
               COLORREF color = graphColor.GetColor(intervalIdx+50);

               CString strLabel;
               strLabel.Format(_T("Finished Deck - Interval %d"),LABEL_INTERVAL(intervalIdx));
               IndexType fdDataSeries = m_Graph.CreateDataSeries(strLabel,PS_SOLID,PenWeight,color);

               auto iter(finishedDeckElevations.begin());
               auto end(finishedDeckElevations.end());
               auto xIter(xVals.begin());
               auto pglIter(PglElevations.begin());
               for (; iter != end; iter++,xIter++)
               {
                  Float64 x = *xIter;
                  Float64 elev = *iter;
                  if (gtElevationDifferential == m_GraphType)
                  {
                     Float64 pglElev = *pglIter++;
                     AddGraphPoint(fdDataSeries,x,elev - pglElev);
                  }
                  else
                  {
                     AddGraphPoint(fdDataSeries,x,elev);
                  }
               }
            }

            if (m_bShowFinishedDeckBottom)
            {
               ATLASSERT(0); // there is no deck
            }

            if (m_bShowFinishedGirderTop)
            {
               ATLASSERT(0); // there is no deck
            }

            if (m_bShowFinishedGirderBottom)
            {
               COLORREF color = graphColor.GetColor(intervalIdx + 150);

               CString strLabel;
               strLabel.Format(_T("Bottom of Girder - Interval %d"),LABEL_INTERVAL(intervalIdx));
               IndexType fdbDataSeries = m_Graph.CreateDataSeries(strLabel,PS_SOLID,PenWeight,color);

               auto iter(vPoi.begin());
               auto end(vPoi.end());
               auto xIter(xVals.begin());
               auto fditer(finishedDeckElevations.begin());
               auto pglIter(PglElevations.begin());
               for (; iter != end; iter++,xIter++,fditer++)
               {
                  const pgsPointOfInterest& poi = *iter;
                  Float64 x = *xIter;
                  Float64 fdelev = *fditer;
                  Float64 gdrDepth = pGirder->GetHeight(poi);
                  Float64 elev = fdelev - gdrDepth;

                  if (gtElevationDifferential == m_GraphType)
                  {
                     Float64 pglElev = *pglIter++;
                     AddGraphPoint(fdbDataSeries,x,elev - pglElev);
                  }
                  else
                  {
                     AddGraphPoint(fdbDataSeries,x,elev);
                  }
               }
            }
         } // next interval
      }
      else if (haunchInputType == pgsTypes::hidACamber)
      {
         // Haunch defined using "A"
         // For this case, the haunch depth floats between the PGL and the A's at the ends of the girder
         for (auto intervalIdx : vIntervals)
         {
            // Finished deck elevations "are" the PGL for this case
            std::vector<Float64> finishedDeckElevations = PglElevations;

            if (m_bShowFinishedDeck)
            {
               COLORREF color = graphColor.GetColor(intervalIdx);

               CString strLabel;
               strLabel.Format(_T("Finished Deck - Interval %d"),LABEL_INTERVAL(intervalIdx));
               IndexType fdDataSeries = m_Graph.CreateDataSeries(strLabel,PS_SOLID,PenWeight,color);

               auto iter(finishedDeckElevations.begin());
               auto end(finishedDeckElevations.end());
               auto xIter(xVals.begin());
               auto pglIter(PglElevations.begin());
               for (; iter != end; iter++,xIter++)
               {
                  Float64 x = *xIter;
                  Float64 elev = *iter;
                  if (gtElevationDifferential == m_GraphType)
                  {
                     Float64 pglElev = *pglIter++;
                     AddGraphPoint(fdDataSeries,x,elev - pglElev);
                  }
                  else
                  {
                     AddGraphPoint(fdDataSeries,x,elev);
                  }
               }
            }

            if (m_bShowFinishedDeckBottom)
            {
               COLORREF color = graphColor.GetColor(intervalIdx+50);

               CString strLabel;
               strLabel.Format(_T("Bottom of Finished Deck - Interval %d"),LABEL_INTERVAL(intervalIdx));
               IndexType fdDataSeries = m_Graph.CreateDataSeries(strLabel,PS_SOLID,PenWeight,color);

               auto iterp(vPoi.begin());
               auto endp(vPoi.end());
               auto iter(finishedDeckElevations.begin());
               auto end(finishedDeckElevations.end());
               auto xIter(xVals.begin());
               auto pglIter(PglElevations.begin());
               for (; iter != end; iter++,xIter++,iterp++)
               {
                  const pgsPointOfInterest& poi = *iterp;
                  Float64 tDeck = pBridge->GetGrossSlabDepth(poi);

                  Float64 x = *xIter;
                  Float64 elev = *iter - tDeck;
                  if (gtElevationDifferential == m_GraphType)
                  {
                     Float64 pglElev = *pglIter++;
                     AddGraphPoint(fdDataSeries,x,elev - pglElev);
                  }
                  else
                  {
                     AddGraphPoint(fdDataSeries,x,elev);
                  }
               }
            }

            if (m_bShowFinishedGirderTop || m_bShowFinishedGirderBottom)
            {
               // Top and bottom of girder are defined by "A" and girder deflections
               std::vector<Float64> TopGirderElevations;
               TopGirderElevations.reserve(vPoi.size());
               for (const auto& poi : vPoi)
               {
                  Float64 eLeft,eCenter,eRight;
                  pDeformedGirderGeometry->GetTopGirderElevation(poi,nullptr,&eLeft,&eCenter,&eRight);
                  if (m_GraphSide == gsCenterLine)
                  {
                     TopGirderElevations.push_back(eCenter);
                  }
                  else if (m_GraphSide == gsLeftEdge)
                  {
                     TopGirderElevations.push_back(eLeft);
                  }
                  else
                  {
                     TopGirderElevations.push_back(eRight);
                  }
               }

               if (m_bShowFinishedGirderTop)
               {
                  COLORREF color = graphColor.GetColor(intervalIdx + 100);

                  CString strLabel;
                  strLabel.Format(_T("Top of Girder - Interval %d"),LABEL_INTERVAL(intervalIdx));
                  IndexType fdbDataSeries = m_Graph.CreateDataSeries(strLabel,PS_SOLID,PenWeight,color);

                  auto iter(TopGirderElevations.begin());
                  auto end(TopGirderElevations.end());
                  auto xIter(xVals.begin());
                  for (; iter != end; iter++,xIter++)
                  {
                     Float64 x = *xIter;
                     Float64 elev = *iter;
                     AddGraphPoint(fdbDataSeries,x,elev);
                  }
               }

               if (m_bShowFinishedGirderBottom)
               {
                  COLORREF color = graphColor.GetColor(intervalIdx + 150);

                  CString strLabel;
                  strLabel.Format(_T("Bottom of Girder - Interval %d"),LABEL_INTERVAL(intervalIdx));
                  IndexType fdbDataSeries = m_Graph.CreateDataSeries(strLabel,PS_SOLID,PenWeight,color);

                  auto iter(vPoi.begin());
                  auto end(vPoi.end());
                  auto xIter(xVals.begin());
                  auto tgiter(TopGirderElevations.begin());
                  auto pglIter(PglElevations.begin());
                  for (; iter != end; iter++,xIter++,tgiter++)
                  {
                     const pgsPointOfInterest& poi = *iter;
                     Float64 x = *xIter;
                     Float64 tgelev = *tgiter;

                     Float64 gdrDepth = pGirder->GetHeight(poi);
                     Float64 elev = tgelev - gdrDepth;

                     if (gtElevationDifferential == m_GraphType)
                     {
                        Float64 pglElev = *pglIter++;
                        AddGraphPoint(fdbDataSeries,x,elev - pglElev);
                     }
                     else
                     {
                        AddGraphPoint(fdbDataSeries,x,elev);
                     }
                  }
               }
            }
         } // next interval
      }
      else
      {
         // Direct haunch input. For this case, we build elevations from top of girder up/down
         for (auto intervalIdx : vIntervals)
         {
            // Need this data for all possible graphs
            std::vector<Float64> topGirderElevations;
            topGirderElevations.reserve(vPoi.size());
            std::vector<Float64> haunchDepths;
            haunchDepths.reserve(vPoi.size());
            for (const auto& poi : vPoi)
            {
               Float64 lftHaunch,clHaunch,rgtHaunch;
               Float64 finished_elevation_cl = pDeformedGirderGeometry->GetFinishedElevation(poi,intervalIdx,true /*include overlay depth*/,&lftHaunch,&clHaunch,&rgtHaunch);

               Float64 tgLeft,tgCL,tgRight;
               pDeformedGirderGeometry->GetTopGirderElevationEx(poi,intervalIdx,nullptr,&tgLeft,&tgCL,&tgRight);

               if (m_GraphSide == gsCenterLine)
               {
                  topGirderElevations.push_back(tgCL);
                  haunchDepths.push_back(clHaunch);
               }
               else if (m_GraphSide == gsLeftEdge)
               {
                  topGirderElevations.push_back(tgLeft);
                  haunchDepths.push_back(lftHaunch);
               }
               else
               {
                  topGirderElevations.push_back(tgRight);
                  haunchDepths.push_back(rgtHaunch);
               }
            }

            if (m_bShowFinishedDeck || m_bShowFinishedDeckBottom)
            {
               if (m_bShowFinishedDeck) // top
               {
                  COLORREF color = graphColor.GetColor(intervalIdx);

                  CString strLabel;
                  strLabel.Format(_T("Finished Deck - Interval %d"),LABEL_INTERVAL(intervalIdx));
                  IndexType fdDataSeries = m_Graph.CreateDataSeries(strLabel,PS_SOLID,PenWeight,color);

                  auto iter(topGirderElevations.begin());
                  auto end(topGirderElevations.end());
                  auto poiIter(vPoi.begin());
                  auto xIter(xVals.begin());
                  auto hdIter(haunchDepths.begin());
                  auto pglIter(PglElevations.begin());
                  for (; iter != end; iter++,poiIter++,xIter++,hdIter++)
                  {
                     Float64 x = *xIter;
                     Float64 tgelev = *iter;
                     Float64 haunchDepth = *hdIter;
                     const auto& poi = *poiIter;
                     Float64 deckDepth = pBridge->GetGrossSlabDepth(poi);
                     Float64 elev = tgelev + haunchDepth + deckDepth;
                     if (gtElevationDifferential == m_GraphType)
                     {
                        Float64 pglElev = *pglIter++;
                        AddGraphPoint(fdDataSeries,x,elev - pglElev);
                     }
                     else
                     {
                        AddGraphPoint(fdDataSeries,x,elev);
                     }
                  }
               }

               if (m_bShowFinishedDeckBottom)
               {
                  COLORREF color = graphColor.GetColor(intervalIdx + 50);

                  CString strLabel;
                  strLabel.Format(_T("Finished Deck Bottom - Interval %d"),LABEL_INTERVAL(intervalIdx));
                  IndexType fdbDataSeries = m_Graph.CreateDataSeries(strLabel,PS_SOLID,PenWeight,color);

                  auto iter(topGirderElevations.begin());
                  auto end(topGirderElevations.end());
                  auto poiIter(vPoi.begin());
                  auto xIter(xVals.begin());
                  auto hdIter(haunchDepths.begin());
                  auto pglIter(PglElevations.begin());
                  for (; iter != end; iter++,poiIter++,xIter++,hdIter++)
                  {
                     Float64 x = *xIter;
                     Float64 tgelev = *iter;
                     Float64 haunchDepth = *hdIter;
                     const auto& poi = *poiIter;
                     Float64 elev = tgelev + haunchDepth;
                     if (gtElevationDifferential == m_GraphType)
                     {
                        Float64 pglElev = *pglIter++;
                        AddGraphPoint(fdbDataSeries,x,elev - pglElev);
                     }
                     else
                     {
                        AddGraphPoint(fdbDataSeries,x,elev);
                     }
                  }
               }
            }

            if (m_bShowFinishedGirderTop)
            {
               COLORREF color = graphColor.GetColor(intervalIdx+100);

               CString strLabel;
               strLabel.Format(_T("Top of Girder - Interval %d"),LABEL_INTERVAL(intervalIdx));
               IndexType fdbDataSeries = m_Graph.CreateDataSeries(strLabel,PS_SOLID,PenWeight,color);

               auto iter(topGirderElevations.begin());
               auto end(topGirderElevations.end());
               auto xIter(xVals.begin());
               auto pglIter(PglElevations.begin());
               for (; iter != end; iter++,xIter++)
               {
                  Float64 elev = *iter;
                  Float64 x = *xIter;

                  if (gtElevationDifferential == m_GraphType)
                  {
                     Float64 pglElev = *pglIter++;
                     AddGraphPoint(fdbDataSeries,x, elev - pglElev);
                  }
                  else
                  {
                     AddGraphPoint(fdbDataSeries,x,elev);
                  }
               }
            }

            if (m_bShowFinishedGirderBottom)
            {
               COLORREF color = graphColor.GetColor(intervalIdx+150);

               CString strLabel;
               strLabel.Format(_T("Bottom of Girder - Interval %d"),LABEL_INTERVAL(intervalIdx));
               IndexType fdbDataSeries = m_Graph.CreateDataSeries(strLabel,PS_SOLID,PenWeight,color);

               auto iter(vPoi.begin());
               auto end(vPoi.end());
               auto xIter(xVals.begin());
               auto tgiter(topGirderElevations.begin());
               auto pglIter(PglElevations.begin());
               for (; iter != end; iter++,xIter++,tgiter++)
               {
                  const pgsPointOfInterest& poi = *iter;
                  Float64 x = *xIter;
                  Float64 tgelev = *tgiter;
                  Float64 gdrDepth = pGirder->GetHeight(poi);
                  Float64 elev = tgelev - gdrDepth;

                  if (gtElevationDifferential == m_GraphType)
                  {
                     Float64 pglElev = *pglIter++;
                     AddGraphPoint(fdbDataSeries,x, elev - pglElev);
                  }
                  else
                  {
                     AddGraphPoint(fdbDataSeries,x,elev);
                  }
               }
            }
         } // next interval
      }
   }
}


void CFinishedElevationGraphBuilder::GetBeamDrawIntervals(IntervalIndexType* pFirstIntervalIdx, IntervalIndexType* pLastIntervalIdx)
{
   CFinishedElevationGraphController* pMyGraphController = (CFinishedElevationGraphController*)m_pGraphController;
   std::vector<IntervalIndexType> vIntervals(pMyGraphController->GetSelectedIntervals());
   if (0 < vIntervals.size())
   {
      *pFirstIntervalIdx = vIntervals.front();
      *pLastIntervalIdx = vIntervals.back();
   }
   else
   {
      GET_IFACE(IIntervals, pIntervals);
      IntervalIndexType intervalIdx = pIntervals->GetLastCastDeckInterval();
      *pFirstIntervalIdx = intervalIdx;
      *pLastIntervalIdx = *pFirstIntervalIdx;
   }
}

void CFinishedElevationGraphBuilder::ExportGraphData(LPCTSTR rstrDefaultFileName)
{
   CExportGraphXYTool::ExportGraphData(m_Graph,rstrDefaultFileName);
}

void CFinishedElevationGraphBuilder::ShowFinishedDeck(BOOL show)
{
   m_bShowFinishedDeck = show;
   Update();
}

BOOL CFinishedElevationGraphBuilder::ShowFinishedDeck() const
{
   return m_bShowFinishedDeck;
}

void CFinishedElevationGraphBuilder::ShowFinishedDeckBottom(BOOL show)
{
   m_bShowFinishedDeckBottom = show;
   Update();
}

BOOL CFinishedElevationGraphBuilder::ShowFinishedDeckBottom() const
{
   return m_bShowFinishedDeckBottom;
}

void CFinishedElevationGraphBuilder::ShowPGL(BOOL show)
{
   m_bShowPGL = show;
   Update();
}

BOOL CFinishedElevationGraphBuilder::ShowPGL() const
{
   return m_bShowPGL;
}

void CFinishedElevationGraphBuilder::ShowFinishedGirderBottom(BOOL show)
{
   m_bShowFinishedGirderBottom = show;
   Update();
}

BOOL CFinishedElevationGraphBuilder::ShowFinishedGirderBottom() const
{
   return m_bShowFinishedGirderBottom;
}

void CFinishedElevationGraphBuilder::ShowFinishedGirderTop(BOOL show)
{
   m_bShowFinishedGirderTop = show;
   Update();
}

BOOL CFinishedElevationGraphBuilder::ShowFinishedGirderTop() const
{
   return m_bShowFinishedGirderTop;
}

void CFinishedElevationGraphBuilder::ShowGirderChord(BOOL show)
{
   m_ShowGirderChord = show;
   Update();
}

BOOL CFinishedElevationGraphBuilder::ShowGirderChord() const
{
   return m_ShowGirderChord;
}

void CFinishedElevationGraphBuilder::SetGraphType(GraphType type)
{
   m_GraphType = type;
   Update();
}

CFinishedElevationGraphBuilder::GraphType CFinishedElevationGraphBuilder::GetGraphType() const
{
   return m_GraphType;
}

void CFinishedElevationGraphBuilder::SetGraphSide(GraphSide side)
{
   m_GraphSide = side;
   Update();
}

CFinishedElevationGraphBuilder::GraphSide CFinishedElevationGraphBuilder::GetGraphSide() const
{
   return m_GraphSide;
}
