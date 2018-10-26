///////////////////////////////////////////////////////////////////////
// PGSuper - Prestressed Girder SUPERstructure Design and Analysis
// Copyright � 1999-2013  Washington State Department of Transportation
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

// SYSTEM INCLUDES
//

// PROJECT INCLUDES
//
#include <PgsExt\PgsExtExp.h>
#include <PgsExt\LiftingAnalysisArtifact.h>
#include <PgsExt\PoiMap.h>

#include <IFace\PointOfInterest.h>

// LOCAL INCLUDES
//

// FORWARD DECLARATIONS
//

// MISCELLANEOUS
//

/*****************************************************************************
CLASS 
   pgsGirderLiftingChecker

   Design Checker for girder lifting


DESCRIPTION
   Design Checker for girder lifting


COPYRIGHT
   Copyright � 1997-1998
   Washington State Department Of Transportation
   All Rights Reserved

LOG
   rdp : 06.25.2013 : Created file
*****************************************************************************/

class pgsGirderLiftingChecker
{
public:
   // GROUP: LIFECYCLE

   //------------------------------------------------------------------------
   // Constructor
   pgsGirderLiftingChecker(IBroker* pBroker,StatusGroupIDType statusGroupID);

   //------------------------------------------------------------------------
   // Destructor
   virtual ~pgsGirderLiftingChecker();

   // GROUP: OPERATORS
   // GROUP: OPERATIONS
   void CheckLifting(const CSegmentKey& segmentKey,pgsLiftingAnalysisArtifact* pArtifact);
   void AnalyzeLifting(const CSegmentKey& segmentKey,Float64 supportLoc,pgsLiftingAnalysisArtifact* pArtifact);
   void AnalyzeLifting(const CSegmentKey& segmentKey,const HANDLINGCONFIG& config,IGirderLiftingDesignPointsOfInterest* pPOId, pgsLiftingAnalysisArtifact* pArtifact);
   pgsDesignCodes::OutcomeType DesignLifting(const CSegmentKey& segmentKey,const GDRCONFIG& config,IGirderLiftingDesignPointsOfInterest* pPOId,pgsLiftingAnalysisArtifact* pArtifact,SHARED_LOGFILE LOGFILE);

   // GROUP: ACCESS
   // GROUP: INQUIRY

protected:
   // GROUP: DATA MEMBERS
   // GROUP: LIFECYCLE
   // GROUP: OPERATORS
   // GROUP: OPERATIONS
   // GROUP: ACCESS
   // GROUP: INQUIRY

private:
   // GROUP: DATA MEMBERS
   IBroker* m_pBroker;
   StatusGroupIDType m_StatusGroupID;
   StatusCallbackIDType m_scidLiftingSupportLocationError;
   StatusCallbackIDType m_scidLiftingSupportLocationWarning;

   // GROUP: LIFECYCLE
   // can't construct without a broker
   pgsGirderLiftingChecker();

   // Prevent accidental copying and assignment
   pgsGirderLiftingChecker(const pgsGirderLiftingChecker&);
   pgsGirderLiftingChecker& operator=(const pgsGirderLiftingChecker&);

   // GROUP: OPERATORS
   // GROUP: OPERATIONS
   void AnalyzeLifting(const CSegmentKey& segmentKey,bool bUseConfig,const HANDLINGCONFIG& liftConfig,IGirderLiftingDesignPointsOfInterest* pPoiD,pgsLiftingAnalysisArtifact* pArtifact);
   void PrepareLiftingAnalysisArtifact(const CSegmentKey& segmentKey,Float64 Loh,Float64 Roh,Float64 Fci,Float64 Eci,pgsTypes::ConcreteType concType,pgsLiftingAnalysisArtifact* pArtifact);
   void ComputeLiftingMoments(const CSegmentKey& segmentKey,
                              const pgsLiftingAnalysisArtifact& rArtifact, 
                              const std::vector<pgsPointOfInterest>& rpoiVec,
                              std::vector<Float64>* pmomVec, 
                              Float64* pMidSpanDeflection);
   void ComputeLiftingStresses(const CSegmentKey& segmentKey,bool bUseConfig,
                               const HANDLINGCONFIG& liftConfig,
                               const std::vector<pgsPointOfInterest>& rpoiVec,
                               const std::vector<Float64>& momVec,
                               pgsLiftingAnalysisArtifact* pArtifact);
   void ComputeLiftingFsAgainstFailure(const CSegmentKey& segmentKey,pgsLiftingAnalysisArtifact* pArtifact);
   bool ComputeLiftingFsAgainstCracking(const CSegmentKey& segmentKey,bool bUseConfig,
                                        const HANDLINGCONFIG& liftConfig,
                                        const std::vector<pgsPointOfInterest>& rpoiVec,
                                        const std::vector<Float64>& momVec,
                                        Float64 midSpanDeflection,
                                        pgsLiftingAnalysisArtifact* pArtifact);

   // GROUP: ACCESS
   // GROUP: INQUIRY
public:
   // GROUP: DEBUG
   #if defined _DEBUG
   //------------------------------------------------------------------------
   // Returns true if the object is in a valid state, otherwise returns false.
   virtual bool AssertValid() const;

   //------------------------------------------------------------------------
   // Dumps the contents of the object to the given dump context.
   virtual void Dump(dbgDumpContext& os) const;
   #endif // _DEBUG

   #if defined _UNITTEST
   //------------------------------------------------------------------------
   // Runs a self-diagnostic test.  Returns true if the test passed,
   // otherwise false.
   static bool TestMe(dbgLog& rlog);
   #endif // _UNITTEST
};

// INLINE METHODS
//

// EXTERNAL REFERENCES
//
