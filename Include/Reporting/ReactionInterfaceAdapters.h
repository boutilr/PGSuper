///////////////////////////////////////////////////////////////////////
// PGSuper - Prestressed Girder SUPERstructure Design and Analysis
// Copyright � 1999-2011  Washington State Department of Transportation
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

#include <Reporting\ReportingExp.h>
#include <PGSuperTypes.h>
#include <IFace\Bridge.h>
#include <IFace\AnalysisResults.h>
#include <IFace\Intervals.h>


/*****************************************************************************
CLASS 
   ReactionLocation and IReactionLocationContainer

   Utility classes for defining reaction locations, iterating them, and reporting the location

DESCRIPTION
***************************************************************************/

// Enum that describes the type of reaction report to be generated
enum ReactionTableType { PierReactionsTable, BearingReactionsTable};

// Simple span bearing reactions can occur at back/ahead. Continuous and pier reactions are at mid
enum PierReactionFaceType {rftBack, rftMid, rftAhead};

class ReactionLocation
{
public:
   PierIndexType        PierIdx;   // Index of the pier where reactions are reported
   PierReactionFaceType Face;      // Face of pier that reaction applies to
   CGirderKey           GirderKey; // GirderKey for the girder that provides the reaction
   std::_tstring        PierLabel; // Label (Abutment 1, Pier 2, etc)
};

typedef std::vector<ReactionLocation> ReactionLocationContainer;
typedef std::vector<ReactionLocation>::const_iterator ReactionLocationIterator;

class ReactionLocationIter
{
public:
   ReactionLocationIter(const ReactionLocationContainer& container);

   void First();
   void Next();
   bool IsDone();
   const ReactionLocation& CurrentItem();

private:
   ReactionLocationIter();
   const ReactionLocationContainer& m_rContainer;
   ReactionLocationIterator m_Iter;
};


/****************************************************************************
CLASS
   IProductReactionAdapter
****************************************************************************/
// Use a local adapter so we can use either the IProductForces or IBearingDesign interfaces
// to provide results for the table.
// First the pure virtual adapter class:
class IProductReactionAdapter
{
public:
   virtual ReactionLocationIter GetReactionLocations(IBridge* pBridge)=0;
   virtual bool DoReportAtPier(PierIndexType pier,const CGirderKey& girderKey)=0;

   virtual Float64 GetReaction(IntervalIndexType intervalIdx, const ReactionLocation& rLocation, ProductForceType type, pgsTypes::BridgeAnalysisType bat) = 0;
   virtual void GetLiveLoadReaction(pgsTypes::LiveLoadType llType,IntervalIndexType intervalIdx, const ReactionLocation& rLocation,pgsTypes::BridgeAnalysisType bat,
                                    bool bIncludeImpact,bool bIncludeLLDF,Float64* pRmin,Float64* pRmax,
                                    VehicleIndexType* pMinConfig = NULL,VehicleIndexType* pMaxConfig = NULL) = 0;
};

/////////////////////////////////////////
// ProductForcesReactionAdapter
//
// Adapter class to return pier total reactions

class REPORTINGCLASS ProductForcesReactionAdapter: public IProductReactionAdapter
{
public:
   ProductForcesReactionAdapter(IProductForces* pForces, const CGirderKey& girderKey);

   virtual ReactionLocationIter GetReactionLocations(IBridge* pBridge);
   virtual bool DoReportAtPier(PierIndexType pier,const CGirderKey& girderKey);
   virtual Float64 GetReaction(IntervalIndexType intervalIdx,const ReactionLocation& rLocation,ProductForceType type,pgsTypes::BridgeAnalysisType bat);
   virtual void GetLiveLoadReaction(pgsTypes::LiveLoadType llType,IntervalIndexType intervalIdx, const ReactionLocation& rLocation,pgsTypes::BridgeAnalysisType bat,
                                    bool bIncludeImpact,bool bIncludeLLDF,Float64* pRmin,Float64* pRmax,
                                    VehicleIndexType* pMinConfig=NULL, VehicleIndexType* pMaxConfig=NULL);

private:
   IProductForces* m_Pointer;
   CGirderKey m_GirderKey;
   ReactionLocationContainer m_Locations;
};

///////////////////////////////////////////////
//   BearingDesignProductReactionAdapter
//
// Adapter to get bearing reactions. Note we have to play a game here because IProductForces wants reactions by pier
// and IBearingDesign returns them by span
class REPORTINGCLASS BearingDesignProductReactionAdapter: public IProductReactionAdapter
{
public:
   BearingDesignProductReactionAdapter(IBearingDesign* pForces, IntervalIndexType intervalIdx, const CGirderKey& girderKey);

   virtual ReactionLocationIter GetReactionLocations(IBridge* pBridge);
   virtual bool DoReportAtPier(PierIndexType pier,const CGirderKey& girderKey);
   virtual Float64 GetReaction(IntervalIndexType intervalIdx,const ReactionLocation& rLocation,ProductForceType type,pgsTypes::BridgeAnalysisType bat);
   virtual void GetLiveLoadReaction(pgsTypes::LiveLoadType llType,IntervalIndexType intervalIdx, const ReactionLocation& rLocation,pgsTypes::BridgeAnalysisType bat,
                                    bool bIncludeImpact,bool bIncludeLLDF,Float64* pRmin,Float64* pRmax,
                                    VehicleIndexType* pMinConfig=NULL, VehicleIndexType* pMaxConfig=NULL);
private:
   IBearingDesign* m_pBearingDesign;
   CGirderKey m_GirderKey;
   ReactionLocationContainer m_Locations;
   IntervalIndexType m_IntervalIdx;
};

/*****************************************************************************
CLASS 
   ICmbLsReactionAdapter

   Utility adapter class for redirecting reaction interfaces so they can be used by the same 
   reporting clients

DESCRIPTION

COPYRIGHT
   Copyright � 1997-2011
   Washington State Department Of Transportation
   All Rights Reserved

LOG
*****************************************************************************/


class ICmbLsReactionAdapter
{
public:
   virtual ReactionLocationIter GetReactionLocations(IBridge* pBridge)=0;
   virtual bool DoReportAtPier(PierIndexType pier,const CGirderKey& girderKey)=0;

   // From ICombinedForces
   virtual Float64 GetReaction(LoadingCombination combo,IntervalIndexType intervalIdx,const ReactionLocation& rLocation,CombinationType type,pgsTypes::BridgeAnalysisType bat) = 0;
   virtual void GetCombinedLiveLoadReaction(pgsTypes::LiveLoadType llType,IntervalIndexType intervalIdx,const ReactionLocation& rLocation,pgsTypes::BridgeAnalysisType bat,bool bIncludeImpact,Float64* pRmin,Float64* pRmax) = 0;

   // From ILimitStateForces
   virtual void GetReaction(pgsTypes::LimitState ls,IntervalIndexType intervalIdx,const ReactionLocation& rLocation,pgsTypes::BridgeAnalysisType bat,bool bIncludeImpact,Float64* pMin,Float64* pMax) = 0;
};

class REPORTINGCLASS CombinedLsForcesReactionAdapter: public ICmbLsReactionAdapter
{
public:
   CombinedLsForcesReactionAdapter(ICombinedForces* pCmbForces, ILimitStateForces* pForces, const CGirderKey& girderKey);

   virtual ReactionLocationIter GetReactionLocations(IBridge* pBridge);
   virtual bool DoReportAtPier(PierIndexType pier,const CGirderKey& girderKey);
   virtual Float64 GetReaction(LoadingCombination combo,IntervalIndexType intervalIdx,const ReactionLocation& rLocation,CombinationType type,pgsTypes::BridgeAnalysisType bat);
   virtual void GetCombinedLiveLoadReaction(pgsTypes::LiveLoadType llType,IntervalIndexType intervalIdx,const ReactionLocation& rLocation,pgsTypes::BridgeAnalysisType bat,bool bIncludeImpact,Float64* pRmin,Float64* pRmax);

   // From ILimitStateForces
   virtual void GetReaction(pgsTypes::LimitState ls,IntervalIndexType intervalIdx,const ReactionLocation& rLocation,pgsTypes::BridgeAnalysisType bat,bool bIncludeImpact,Float64* pMin,Float64* pMax);

private:
   ICombinedForces*   m_CmbPointer;
   ILimitStateForces* m_LsPointer;
   CGirderKey m_GirderKey;
   ReactionLocationContainer m_Locations;
};


// Adapter to get bearing reactions. Note we have to play a game here because IProductForces wants reactions by pier
// and IBearingDesign returns them by span
class REPORTINGCLASS CmbLsBearingDesignReactionAdapter: public ICmbLsReactionAdapter
{
public:
   CmbLsBearingDesignReactionAdapter(IBearingDesign* pForces, IntervalIndexType intervalIdx, const CGirderKey& girderKey);

   virtual ReactionLocationIter GetReactionLocations(IBridge* pBridge);
   virtual bool DoReportAtPier(PierIndexType pier,const CGirderKey& girderKey);

   virtual Float64 GetReaction(LoadingCombination combo,IntervalIndexType intervalIdx,const ReactionLocation& rLocation,CombinationType type,pgsTypes::BridgeAnalysisType bat);
   virtual void GetCombinedLiveLoadReaction(pgsTypes::LiveLoadType llType,IntervalIndexType intervalIdx,const ReactionLocation& rLocation,pgsTypes::BridgeAnalysisType bat,bool bIncludeImpact,Float64* pRmin,Float64* pRmax);
   virtual void GetReaction(pgsTypes::LimitState ls,IntervalIndexType intervalIdx,const ReactionLocation& rLocation,pgsTypes::BridgeAnalysisType bat,bool bIncludeImpact,Float64* pRmin,Float64* pRmax);

private:
   IBearingDesign* m_pBearingDesign;
   CGirderKey m_GirderKey;
   ReactionLocationContainer m_Locations;
   IntervalIndexType m_IntervalIdx;
};


// Class that decides whether to print reaction data based on report type, boundary conditions,
// and interval
class ReactionDecider
{
public:
   ReactionDecider(ReactionTableType tableType, const ReactionLocation& location, IBridge* pBridge,IIntervals* pIntervals);

   // If true, report results
   bool DoReport(IntervalIndexType intervalIdx);

private:
   bool              m_bAlwaysReport;
   IntervalIndexType m_ThresholdInterval;
};
