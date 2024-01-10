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
#include <PgsExt\Keys.h>

/////////////////////////////////////////////////////////////////////////////////////////
// View event callback interfaces are defined in this header file. Extension agents
// should implement these interfaces, and attached them to their corresponding views,
// to receive event notifications.
//
// These are prototype interfaces. That means they are subject to change. It is unclear
// as to exactly which events extensions need to receive and how they are to be broadcast.
//
// Be warned that these interfaces may change in the future and your extension agents
// are likely to break.
//

/// @brief Callback interface for Bridge Plan View
interface IBridgePlanViewEventCallback
{
   /// @brief called when a context menu is created in the background of the view
   virtual void OnBackgroundContextMenu(CEAFMenu* pMenu) = 0;

   /// @brief called when a context menu is created for a pier
   virtual void OnPierContextMenu(PierIndexType pierIdx,CEAFMenu* pMenu) = 0;

   /// @brief called when a context menu is created for a span
   virtual void OnSpanContextMenu(SpanIndexType spanIdx,CEAFMenu* pMenu) = 0;

   /// @brief called when a context menu is created for a girder
   virtual void OnGirderContextMenu(const CGirderKey& girderKey,CEAFMenu* pMenu) = 0;

   /// @brief called when a context menu is created for the deck
   virtual void OnDeckContextMenu(CEAFMenu* pMenu) = 0;

   /// @brief called when a context menu is created for the alignment
   virtual void OnAlignmentContextMenu(CEAFMenu* pMenu) = 0;

   /// @brief called when a context menu is created for the section cut object
   virtual void OnSectionCutContextMenu(CEAFMenu* pMenu) = 0;

   /// @brief called when a context menu is created for a temporary support
   virtual void OnTemporarySupportContextMenu(SupportIDType tsID,CEAFMenu* pMenu) = 0;

   /// @brief called when a context menu is created for a girder segment
   virtual void OnGirderSegmentContextMenu(const CSegmentKey& segmentKey,CEAFMenu* pMenu) = 0;

   /// @brief called when a context menu is created for a closure joint
   virtual void OnClosureJointContextMenu(const CSegmentKey& closureKey,CEAFMenu* pMenu) = 0;
};


/// @brief Callback interface for the Bridge Section View
interface IBridgeSectionViewEventCallback
{
   /// @brief called when a context menu is created in the view background
   virtual void OnBackgroundContextMenu(CEAFMenu* pMenu) = 0;

   /// @brief called when a context menu is created for a girder
   virtual void OnGirderContextMenu(const CGirderKey& girderKey,CEAFMenu* pMenu) = 0;

   /// @brief called when a context menu is created for the bridge deck
   virtual void OnDeckContextMenu(CEAFMenu* pMenu) = 0;
};

/// @brief callback interface for the Alignment Plan View
interface IAlignmentPlanViewEventCallback
{
   /// @brief called when a context menu is created in the view background
   virtual void OnBackgroundContextMenu(CEAFMenu* pMenu) = 0;

   /// @brief called when a context menu is created for the alignment
   virtual void OnAlignmentContextMenu(CEAFMenu* pMenu) = 0;

   /// @brief called when a context menu is created for the bridge
   virtual void OnBridgeContextMenu(CEAFMenu* pMenu) = 0;
};

/// @brief callback interface for the Alignment Profile View
interface IAlignmentProfileViewEventCallback
{
   /// @brief called when a context menu is created in the view background
   virtual void OnBackgroundContextMenu(CEAFMenu* pMenu) = 0;

   /// @brief called when a context menu is created for the profile
   virtual void OnProfileContextMenu(CEAFMenu* pMenu) = 0;

   /// @brief called when a context menu is created for the bridge
   virtual void OnBridgeContextMenu(CEAFMenu* pMenu) = 0;
};


/// @brief callback interface for the Girder Elevation View
interface IGirderElevationViewEventCallback
{
   /// @brief called when a context menu is created in the view background
   virtual void OnBackgroundContextMenu(CEAFMenu* pMenu) = 0;

   /// @brief called when a context menu is created for a girder segment
   virtual void OnGirderSegmentContextMenu(const CSegmentKey& segmentKey, CEAFMenu* pMenu) = 0;
};

/// @brief callback interface for the Girder Section View
interface IGirderSectionViewEventCallback
{
   /// @brief called when a context menu is created in the view background
   virtual void OnBackgroundContextMenu(CEAFMenu* pMenu) = 0;

   /// @brief called when a context menu is created in the girder section
   virtual void OnGirderSectionContextMenu(const pgsPointOfInterest& poi, CEAFMenu* pMenu) = 0;
};

// {EB057BFE-3A37-48af-8F19-80465DBA2A14}
DEFINE_GUID(IID_IRegisterViewEvents, 
0xeb057bfe, 0x3a37, 0x48af, 0x8f, 0x19, 0x80, 0x46, 0x5d, 0xba, 0x2a, 0x14);
struct __declspec(uuid("{EB057BFE-3A37-48af-8F19-80465DBA2A14}")) IRegisterViewEvents;
/// @brief Interface used to register view event callbacks
interface IRegisterViewEvents : IUnknown
{
   /// @brief Registers a Bridge Plan View callback
   /// @param pCallback The callback object
   /// @return ID of the callback
   virtual IDType RegisterBridgePlanViewCallback(IBridgePlanViewEventCallback* pCallback) = 0;

   /// @brief Registers a Bridge Section View callback
   /// @param pCallback The callback object
   /// @return ID of the callback
   virtual IDType RegisterBridgeSectionViewCallback(IBridgeSectionViewEventCallback* pCallback) = 0;

   /// @brief Registers a Alignment Plan View callback
   /// @param pCallback The callback object
   /// @return ID of the callback
   virtual IDType RegisterAlignmentPlanViewCallback(IAlignmentPlanViewEventCallback* pCallback) = 0;

   /// @brief Registers a Alignment Profile View callback
   /// @param pCallback The callback object
   /// @return ID of the callback
   virtual IDType RegisterAlignmentProfileViewCallback(IAlignmentProfileViewEventCallback* pCallback) = 0;

   /// @brief Registers a Girder Elevation View callback
   /// @param pCallback The callback object
   /// @return ID of the callback
   virtual IDType RegisterGirderElevationViewCallback(IGirderElevationViewEventCallback* pCallback) = 0;

   /// @brief Registers a Girder Section View callback
   /// @param pCallback The callback object
   /// @return ID of the callback
   virtual IDType RegisterGirderSectionViewCallback(IGirderSectionViewEventCallback* pCallback) = 0;

   /// @brief Unregisters a callback from the Bridge Plan View
   /// @param ID The callback ID
   /// @return true if successful
   virtual bool UnregisterBridgePlanViewCallback(IDType ID) = 0;

   /// @brief Unregisters a callback from the Bridge Section View
   /// @param ID The callback ID
   /// @return true if successful
   virtual bool UnregisterBridgeSectionViewCallback(IDType ID) = 0;

   /// @brief Unregisters a callback from the Alignment Plan View
   /// @param ID The callback ID
   /// @return true if successful
   virtual bool UnregisterAlignmentPlanViewCallback(IDType ID) = 0;

   /// @brief Unregisters a callback from the Alignment Element View
   /// @param ID The callback ID
   /// @return true if successful
   virtual bool UnregisterAlignmentProfileViewCallback(IDType ID) = 0;

   /// @brief Unregisters a callback from the Girder Elevation View
   /// @param ID The callback ID
   /// @return true if successful
   virtual bool UnregisterGirderElevationViewCallback(IDType ID) = 0;

   /// @brief Unregisters a callback from the Girder Section View
   /// @param ID The callback ID
   /// @return true if successful
   virtual bool UnregisterGirderSectionViewCallback(IDType ID) = 0;
};