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

#pragma once

#include <WbflTypes.h>
#include <LRFD\LiveLoadDistributionFactorBase.h>
#include <PgsExt\PointLoadData.h>
#include <PgsExt\DistributedLoadData.h>
#include <PgsExt\MomentLoadData.h>

#define EAD_ROADWAY        0
#define EAD_PROFILE        1
#define EAD_SECTION        2

#define EBD_GENERAL        0
#define EBD_FRAMING        1
#define EBD_RAILING        2
#define EBD_DECK           3
#define EBD_DECKREBAR      4
#define EBD_ENVIRONMENT    5

// Pier Editing
#define EPD_GENERAL        0
#define EPD_LAYOUT         1
#define EPD_SPACING        2
#define EPD_CONNECTION     3
#define EPD_BEARINGS       4

// Temporary support editing
#define ETS_GENERAL        0
#define ETS_CONNECTION     1
#define ETS_SPACING        2

// Span Editing
#define ESD_GENERAL        0
#define ESD_CONNECTION     1
#define ESD_SPACING        2


// PGSuper Edit Girder Details Pages
#define EGD_GENERAL        0
#define EGD_CONCRETE       EGD_GENERAL
#define EGD_PRESTRESSING   1
#define EGD_DEBONDING      2
#define EGD_LONG_REINF     3
#define EGD_STIRRUPS       4
#define EGD_TRANSPORTATION 5
#define EGD_CONDITION      6

// PGSplice Edit Girder Segment Pages
#define EGS_GENERAL        0
#define EGS_PRESTRESSING   1
#define EGS_TENDONS        2
#define EGS_LONG_REINF     3
#define EGS_STIRRUPS       4
#define EGS_TRANSPORTATION 5

/*****************************************************************************
INTERFACE
   IEditByUI

   Interface used to invoke elements of the user interface so that the user
   can edit input data.

DESCRIPTION
   Interface used to invoke elements of the user interface so that the user
   can edit input data.

   Methods on this interface should only be called by core agents (not extension agents)

   Future versions of PGSuper will permit Agents to supply their own user interface
   and this will likely eliminate the need for this interface. This interface
   may be removed in the future.
*****************************************************************************/
// {E1CF3EAA-3E85-450a-9A67-D68FF321DC16}
DEFINE_GUID(IID_IEditByUI, 
0xe1cf3eaa, 0x3e85, 0x450a, 0x9a, 0x67, 0xd6, 0x8f, 0xf3, 0x21, 0xdc, 0x16);
/// @brief Interface used to invoke elements of the user interface so that the user can edit input data.
///
/// Methods on this interface should only be called by core agents(not extension agents)
///
/// Future versions of PGSuper will permit Agents to supply their own user interface
/// and this will likely eliminate the need for this interface. This interface
/// may be removed in the future.
interface __declspec(uuid("{E1CF3EAA-3E85-450a-9A67-D68FF321DC16}")) IEditByUI : IUnknown
{
   /// @brief Opens the Bridge Description dialog to the specified page number
   virtual void EditBridgeDescription(int nPage) = 0;

   /// @brief Opens the Alignment Description dialog to the specifed page number
   virtual void EditAlignmentDescription(int nPage) = 0;

   /// @brief Opens the Segment Description dialog
   /// @return Returns true if editing was successful
   virtual bool EditSegmentDescription(const CSegmentKey& segmentKey, int nPage) = 0;

   /// @brief Opens the Segment Description dialog for the currently selected segment
   /// @return Returns true if editing was successful
   virtual bool EditSegmentDescription() = 0;

   /// @brief Opens the Closure Joint Description dialog for the specifed closure joint
   /// @return Returns true if editing was successful
   virtual bool EditClosureJointDescription(const CClosureKey& closureKey, int nPage) = 0;

   /// @brief Opens the Girder Description dialog for the specified girder
   /// @return Returns true if editing was successful
   virtual bool EditGirderDescription(const CGirderKey& girderKey, int nPage) = 0;

   /// @brief Opens the Girder Description dialog for the currently selected girder
   /// @return Returns true if editing was successful
   virtual bool EditGirderDescription() = 0;

   /// @brief Opens the Span Description dialog for the specified span
   /// @return Returns true if editing was successful
   virtual bool EditSpanDescription(SpanIndexType spanIdx, int nPage) = 0;

   /// @brief Opens the Pier Description dialog for the specified pier
   /// @return Returns true if editing was successful
   virtual bool EditPierDescription(PierIndexType pierIdx, int nPage) = 0;

   /// @brief Opens the Temporary Support Dialog for the specified temporary support
   /// @return Returns true if editing was successful
   virtual bool EditTemporarySupportDescription(SupportIndexType tsIdx, int nPage) = 0;

   /// @brief Opens the Live Load editing dialog
   virtual void EditLiveLoads() = 0;

   /// @brief Opens the live load distribution factors dialog
   /// @param method Default value for distribution factor method
   /// @param roaAction Default value for range of applicability action
   virtual void EditLiveLoadDistributionFactors(pgsTypes::DistributionFactorMethod method,WBFL::LRFD::RangeOfApplicabilityAction roaAction) = 0;

   /// @brief Opens the Point Load editing dialog
   /// @param loadIdx Index of load to be edited
   /// @return Returns true if editing was successful
   virtual bool EditPointLoad(IndexType loadIdx) = 0;

   /// @brief Opens the Point Load editing dialog
   /// @param loadID ID of load to be edited
   /// @return Returns true if editing was successful
   virtual bool EditPointLoadByID(LoadIDType loadID) = 0;

   /// @brief Opens the Distributed Load editing dialog
   /// @param loadIdx Index of the load to be edited
   /// @return Returns true if editing was successful
   virtual bool EditDistributedLoad(IndexType loadIdx) = 0;

   /// @brief Opens the Distributed Load editing dialog
   /// @param loadID ID of the load to be edited
   /// @return Returns true if editing was successful
   virtual bool EditDistributedLoadByID(LoadIDType loadID) = 0;

   /// @brief Opens the Moment Load editing dialog
   /// @param loadIdx Index of the load to be edited
   /// @return Returns true if editing was successful
   virtual bool EditMomentLoad(IndexType loadIdx) = 0;
   
   /// @brief Opens the Moment Load editing dialog
   /// @param loadID ID of the load to be edited
   /// @return Returns true if editing was successful
   virtual bool EditMomentLoadByID(LoadIDType loadID) = 0;

   /// @brief Opens the timeline editor dialog
   /// @return Returns true if editing was successful
   virtual bool EditTimeline() = 0;

   /// @brief Opens the Cast Deck Activity dialog
   /// @return Returns true if editing was successful
   virtual bool EditCastDeckActivity() = 0;

   /// @brief Returns the ID of the standard toolbar
   virtual UINT GetStdToolBarID() = 0;

   /// @brief Returns the ID of the library editor toolbar
   virtual UINT GetLibToolBarID() = 0;

   /// @brief Returns the ID of the help toolbar
   virtual UINT GetHelpToolBarID() = 0;

   /// @brief Opens the Direct Section Prestressing editing dialog. NOTE: Strand fill type must be pgsTypes::sdtDirectSelection before entering this dialog
   /// @return Returns true if editing was successful
   virtual bool EditDirectSelectionPrestressing(const CSegmentKey& segmentKey) = 0;

   /// @brief Opens the Direct Row Input Prestressing editing dialog. NOTE: Strand fill type must be pgsTypes::sdtDirectRowInput before entering this dialog
   /// @return Returns true if editing was successful
   virtual bool EditDirectRowInputPrestressing(const CSegmentKey& segmentKey) = 0;

   /// @brief Opens the Direct Strand Input Prestressing editing dialog. NOTE: Strand fill type must be pgsTypes::sdtDirectStrandInput before entering this dialog
   /// @return Returns true if editing was successful
   virtual bool EditDirectStrandInputPrestressing(const CSegmentKey& segmentKey) = 0;

   /// @brief Adds a Point Load to the model
   /// @param loadData 
   virtual void AddPointLoad(const CPointLoadData& loadData) = 0;

   /// @brief Deletes a Point Load from the model
   /// @param loadIdx 
   virtual void DeletePointLoad(IndexType loadIdx) = 0;

   /// @brief Adds a Distributed Load to the model
   /// @param loadData 
   virtual void AddDistributedLoad(const CDistributedLoadData& loadData) = 0;

   /// @brief Deletes a Distributed Load from the model
   /// @param loadIdx 
   virtual void DeleteDistributedLoad(IndexType loadIdx) = 0;

   /// @brief Adds a Moment Load to the model
   /// @param loadData 
   virtual void AddMomentLoad(const CMomentLoadData& loadData) = 0;

   /// @brief Removes a Moment Load from the model
   /// @param loadIdx 
   virtual void DeleteMomentLoad(IndexType loadIdx) = 0;

   /// @brief Opens the Effective Flange Width dialog
   /// @return Returns true if editing was successful
   virtual bool EditEffectiveFlangeWidth() = 0;

   /// @brief Opens the project criteria selection dialog
   /// @return Returns true if editing was successful
   virtual bool SelectProjectCriteria() = 0;

   /// @brief Opens the Edit Bearings dialog
   /// @return Returns true if editing was successful
   virtual bool EditBearings() = 0;

   /// @brief Opens the Edit Haunch dialog
   /// @return Returns true if editing was successful
   virtual bool EditHaunch() = 0;
};
