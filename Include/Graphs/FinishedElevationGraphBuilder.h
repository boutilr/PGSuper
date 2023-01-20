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

#pragma once

#include <Graphs/GraphsExp.h>
#include <Graphs\GirderGraphBuilderBase.h>

class GRAPHCLASS CFinishedElevationGraphBuilder : public CGirderGraphBuilderBase
{
public:
   enum GraphType { gtElevations,gtElevationDifferential };
   enum GraphSide { gsLeftEdge,gsCenterLine,gsRightEdge };

   CFinishedElevationGraphBuilder();
   CFinishedElevationGraphBuilder(const CFinishedElevationGraphBuilder& other) = default;
   virtual ~CFinishedElevationGraphBuilder();

   virtual BOOL CreateGraphController(CWnd* pParent,UINT nID) override;
   virtual std::unique_ptr<WBFL::Graphing::GraphBuilder> Clone() const override;

   virtual void CreateViewController(IEAFViewController** ppController) override;

   virtual void UpdateXAxis() override;
   virtual void UpdateYAxis() override;

   void ExportGraphData(LPCTSTR rstrDefaultFileName);

   void ShowFinishedDeck(BOOL show,bool bUpdate);
   BOOL ShowFinishedDeck() const;

   void ShowFinishedDeckBottom(BOOL show,bool bUpdate);
   BOOL ShowFinishedDeckBottom() const;
   void ShowPGL(BOOL show,bool bUpdate);
   BOOL ShowPGL() const;
   void ShowFinishedGirderBottom(BOOL show,bool bUpdate);
   BOOL ShowFinishedGirderBottom() const;
   void ShowFinishedGirderTop(BOOL show,bool bUpdate);
   BOOL ShowFinishedGirderTop() const;
   void ShowGirderChord(BOOL show,bool bUpdate);
   BOOL ShowGirderChord() const;
   void ShowHaunchDepth(BOOL show,bool bUpdate);
   BOOL ShowHaunchDepth() const;

   void SetGraphType(GraphType type,bool bUpdate);
   GraphType GetGraphType()const;
   void SetGraphSide(GraphSide side,bool bUpdate);
   GraphSide GetGraphSide()const;
protected:
   virtual CGirderGraphControllerBase* CreateGraphController() override;
   virtual bool UpdateNow() override;

   DECLARE_MESSAGE_MAP()

   void UpdateGraphTitle(GroupIndexType grpIdx,GirderIndexType gdrIdx);
   void UpdateGraphData(GroupIndexType grpIdx,GirderIndexType gdrIdx);

   virtual void GetBeamDrawIntervals(IntervalIndexType* pFirstIntervalIdx, IntervalIndexType* pLastIntervalIdx) override;

   BOOL m_bShowPGL;
   BOOL m_bShowFinishedDeck;
   BOOL m_bShowFinishedDeckBottom;
   BOOL m_bShowFinishedGirderBottom;
   BOOL m_bShowFinishedGirderTop;
   BOOL m_ShowGirderChord;
   BOOL m_ShowHaunchDepth;
   GraphType m_GraphType;
   GraphSide m_GraphSide;
};
