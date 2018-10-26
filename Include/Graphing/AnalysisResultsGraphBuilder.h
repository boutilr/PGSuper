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

#pragma once

#include <Graphing\GraphingExp.h>
#include <Graphing\GirderGraphBuilderBase.h>
#include <Graphing\GraphingTypes.h>

class CAnalysisResultsGraphDefinition; // use a forward declaration here. We don't want to include the header file for a non-exported class
class CAnalysisResultsGraphDefinitions;
class CGraphColor;

class GRAPHINGCLASS CAnalysisResultsGraphBuilder : public CGirderGraphBuilderBase
{
public:
   CAnalysisResultsGraphBuilder();
   CAnalysisResultsGraphBuilder(const CAnalysisResultsGraphBuilder& other);
   virtual ~CAnalysisResultsGraphBuilder();

   virtual BOOL CreateGraphController(CWnd* pParent,UINT nID);

   virtual CGraphBuilder* Clone();

   void UpdateGraphDefinitions();
   std::vector<std::pair<CString,IDType>> GetLoadings(IntervalIndexType intervalIdx,ActionType actionType);

   void DumpLBAM();

protected:
   std::auto_ptr<CGraphColor> m_pGraphColor;
   std::set<IndexType> m_UsedDataLabels; // keeps track of the graph data labels that have already been used so we don't get duplicates in the legend

   void Init();

   virtual CGirderGraphControllerBase* CreateGraphController();
   virtual bool UpdateNow();

   void UpdateYAxisUnits();
   void UpdateXAxisTitle();
   void UpdateGraphTitle();
   void UpdateGraphData();

   COLORREF GetGraphColor(IndexType graphIdx,IntervalIndexType intervalIdx);
   CString GetDataLabel(IndexType graphIdx,const CAnalysisResultsGraphDefinition& graphDef,IntervalIndexType intervalIdx);

   void InitializeGraph(IndexType graphIdx,const CAnalysisResultsGraphDefinition& graphDef,ActionType actionType,IntervalIndexType intervalIdx,bool bIsFinalShear,IndexType* pDataSeriesID,pgsTypes::BridgeAnalysisType* pBAT,pgsTypes::StressLocation* pStressLocation,IndexType* pAnalysisTypeCount);

   void ProductLoadGraph(IndexType graphIdx,const CAnalysisResultsGraphDefinition& graphDef,IntervalIndexType intervalIdx,const std::vector<pgsPointOfInterest>& vPoi,const std::vector<Float64>& xVals,bool bIsFinalShear);
   void CombinedLoadGraph(IndexType graphIdx,const CAnalysisResultsGraphDefinition& graphDef,IntervalIndexType intervalIdx,const std::vector<pgsPointOfInterest>& vPoi,const std::vector<Float64>& xVals,bool bIsFinalShear);
   void LimitStateLoadGraph(IndexType graphIdx,const CAnalysisResultsGraphDefinition& graphDef,IntervalIndexType intervalIdx,const std::vector<pgsPointOfInterest>& vPoi,const std::vector<Float64>& xVals,bool bIsFinalShear);

   void LiveLoadGraph(IndexType graphIdx,const CAnalysisResultsGraphDefinition& graphDef,IntervalIndexType intervalIdx,const std::vector<pgsPointOfInterest>& vPoi,const std::vector<Float64>& xVals,bool bIsFinalShear);
   void VehicularLiveLoadGraph(IndexType graphIdx,const CAnalysisResultsGraphDefinition& graphDef,IntervalIndexType intervalIdx,const std::vector<pgsPointOfInterest>& vPoi,const std::vector<Float64>& xVals,bool bIsFinalShear);

   void CyStressCapacityGraph(IndexType graphIdx,const CAnalysisResultsGraphDefinition& graphDef,IntervalIndexType intervalIdx,const std::vector<pgsPointOfInterest>& vPoi,const std::vector<Float64>& xVals);

   virtual IntervalIndexType GetBeamDrawInterval();

   std::auto_ptr<CAnalysisResultsGraphDefinitions> m_pGraphDefinitions;

   DECLARE_MESSAGE_MAP()

   std::vector<IntervalIndexType> AddTSRemovalIntervals(IntervalIndexType loadingIntervalIdx,const std::vector<IntervalIndexType>& vIntervals,const std::vector<IntervalIndexType>& vTSRIntervals);
   pgsTypes::AnalysisType GetAnalysisType();
};
