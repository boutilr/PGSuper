///////////////////////////////////////////////////////////////////////
// PGSuper - Prestressed Girder SUPERstructure Design and Analysis
// Copyright � 1999-2010  Washington State Department of Transportation
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

#ifndef INCLUDED_PGSEXT_BRIDGEDESCRIPTION_H_
#define INCLUDED_PGSEXT_BRIDGEDESCRIPTION_H_

#include <WBFLCore.h>

#if !defined INCLUDED_PGSEXTEXP_H_
#include <PgsExt\PgsExtExp.h>
#endif

#include <PgsExt\DeckDescription.h>
#include <PgsExt\RailingSystem.h>
#include <PgsExt\PierData.h>
#include <PgsExt\SpanData.h>

#include <PsgLib\GirderLibraryEntry.h>

/*****************************************************************************
CLASS 
   CBridgeDescription

   Utility class for managing the bridge description data

DESCRIPTION
   Utility class for managing the bridge description data


COPYRIGHT
   Copyright � 1997-2008
   Washington State Department Of Transportation
   All Rights Reserved

LOG
   rab : 04.29.2008 : Created file
*****************************************************************************/

class PGSEXTCLASS CBridgeDescription
{
public:
   CBridgeDescription();
   CBridgeDescription(const CBridgeDescription& rOther);
   ~CBridgeDescription();

   CBridgeDescription& operator = (const CBridgeDescription& rOther);
   bool operator==(const CBridgeDescription& rOther) const;
   bool operator!=(const CBridgeDescription& rOther) const;

   HRESULT Load(IStructuredLoad* pStrLoad,IProgress* pProgress);
   HRESULT Save(IStructuredSave* pStrSave,IProgress* pProgress);

   CDeckDescription* GetDeckDescription();
   const CDeckDescription* GetDeckDescription() const;

   CRailingSystem* GetLeftRailingSystem();
   const CRailingSystem* GetLeftRailingSystem() const;

   CRailingSystem* GetRightRailingSystem();
   const CRailingSystem* GetRightRailingSystem() const;

   void CreateFirstSpan(const CPierData* pFirstPier=NULL,const CSpanData* pFirstSpan=NULL,const CPierData* pNextPier=NULL);
   void AppendSpan(const CSpanData* pSpanData=NULL,const CPierData* pPierData=NULL);
   void InsertSpan(PierIndexType refPierIdx,pgsTypes::PierFaceType pierFace,Float64 spanLength,const CSpanData* pSpanData=NULL,const CPierData* pPierData=NULL);
   void RemoveSpan(SpanIndexType spanIdx,pgsTypes::RemovePierType rmPierType);

   PierIndexType GetPierCount() const;
   SpanIndexType GetSpanCount() const;

   CPierData* GetPier(PierIndexType pierIdx);
   const CPierData* GetPier(PierIndexType pierIdx) const;
   
   CSpanData* GetSpan(SpanIndexType spanIdx);
   const CSpanData* GetSpan(SpanIndexType spanIdx) const;

   void UseSameNumberOfGirdersInAllSpans(bool bSame);
   bool UseSameNumberOfGirdersInAllSpans() const;
   void SetGirderCount(GirderIndexType nGirders);
   GirderIndexType GetGirderCount() const;

   void SetGirderFamilyName(const char* strName);
   const char* GetGirderFamilyName() const;

   void UseSameGirderForEntireBridge(bool bSame);
   bool UseSameGirderForEntireBridge() const;
   const char* GetGirderName() const;
   void RenameGirder(const char* strName);
   void SetGirderName(const char* strName);
   const GirderLibraryEntry* GetGirderLibraryEntry() const;
   void SetGirderLibraryEntry(const GirderLibraryEntry* pEntry);

   void SetGirderOrientation(pgsTypes::GirderOrientationType gdrOrientation);
   pgsTypes::GirderOrientationType GetGirderOrientation() const;

   void SetGirderSpacingType(pgsTypes::SupportedBeamSpacing sbs);
   pgsTypes::SupportedBeamSpacing GetGirderSpacingType() const;
   double GetGirderSpacing() const;
   void SetGirderSpacing(double spacing);

   void SetMeasurementType(pgsTypes::MeasurementType mt);
   pgsTypes::MeasurementType GetMeasurementType() const;

   void SetMeasurementLocation(pgsTypes::MeasurementLocation ml);
   pgsTypes::MeasurementLocation GetMeasurementLocation() const;

   // set/get the reference girder index. if index is < 0 then
   // the center of the girder group is the reference point
   void SetRefGirder(GirderIndexType refGdrIdx);
   GirderIndexType GetRefGirder() const;

   // offset to the reference girder, measured from either the CL bridge or alignment
   // as indicated by RegGirderOffsetDatum, MeasurementType, and LocationType
   void SetRefGirderOffset(double offset);
   double GetRefGirderOffset() const;

   // indicated what the reference girder offset is measured relative to
   void SetRefGirderOffsetType(pgsTypes::OffsetMeasurementType offsetDatum);
   pgsTypes::OffsetMeasurementType GetRefGirderOffsetType() const;

   void SetAlignmentOffset(double alignmentOffset);
   double GetAlignmentOffset() const;

   // set/get the slab offset type
   void SetSlabOffsetType(pgsTypes::SlabOffsetType slabOffsetType);
   pgsTypes::SlabOffsetType GetSlabOffsetType() const;

   // Set/get the slab offset. Has no net effect if slab offset type is not sotBridge
   // Get method returns invalid data if slab offset type is not sotBridge
   void SetSlabOffset(Float64 slabOffset);
   Float64 GetSlabOffset() const;

   // returns the greatest slab offset defined for the bridge
   Float64 GetMaxSlabOffset() const;

   bool SetSpanLength(SpanIndexType spanIdx,double newLength);
   bool MovePier(PierIndexType pierIdx,double newStation,pgsTypes::MovePierOption moveOption);

   void SetDistributionFactorMethod(pgsTypes::DistributionFactorMethod method);
   pgsTypes::DistributionFactorMethod GetDistributionFactorMethod() const;

   void CopyDown(bool bGirderCount,bool bGirderType,bool bSpacing,bool bSlabOffset); 
                    // takes all the data defined at the bridge level and copies
                    // it down to the spans and girders (only for this parameters set to true)

   // It's pretty much impossible to take care of all data in this tree from the editing dialog
   // so the function below takes half-baked edits, compares what changed from the original
   // version, and finishes them.
   void ReconcileEdits(IBroker* pBroker, const CBridgeDescription* pOriginalDesc);

   void Clear();

protected:
   void MakeCopy(const CBridgeDescription& rOther);
   void MakeAssignment(const CBridgeDescription& rOther);

private:
   CDeckDescription m_Deck;
   CRailingSystem m_LeftRailingSystem;
   CRailingSystem m_RightRailingSystem;

   std::vector<CPierData*> m_Piers;
   std::vector<CSpanData*> m_Spans;

   bool m_bSameNumberOfGirders;
   bool m_bSameGirderName;

   double m_AlignmentOffset; // offset from Alignment to CL Bridge (< 0 = right of alignment)

   double m_SlabOffset;
   pgsTypes::SlabOffsetType m_SlabOffsetType;

   GirderIndexType m_nGirders;
   double m_GirderSpacing;

   GirderIndexType m_RefGirderIdx;
   double m_RefGirderOffset;
   pgsTypes::OffsetMeasurementType m_RefGirderOffsetType;

   pgsTypes::MeasurementType m_MeasurementType;
   pgsTypes::MeasurementLocation m_MeasurementLocation;

   pgsTypes::SupportedBeamSpacing m_GirderSpacingType;

   const GirderLibraryEntry* m_pGirderLibraryEntry;

   pgsTypes::GirderOrientationType m_GirderOrientation;
   std::string m_strGirderName;
   std::string m_strGirderFamilyName;

   pgsTypes::DistributionFactorMethod m_LLDFMethod;

   bool MoveBridge(PierIndexType pierIdx,double newStation);
   bool MoveBridgeAdjustPrevSpan(PierIndexType pierIdx,double newStation);
   bool MoveBridgeAdjustNextSpan(PierIndexType pierIdx,double newStation);
   bool MoveBridgeAdjustAdjacentSpans(PierIndexType pierIdx,double newStation);

   void RenumberSpans();
   void AssertValid();
};

#endif // INCLUDED_PGSEXT_BRIDGEDESCRIPTION_H_
