///////////////////////////////////////////////////////////////////////
// PGSuper - Prestressed Girder SUPERstructure Design and Analysis
// Copyright � 1999-2016  Washington State Department of Transportation
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
#include <PgsExt\KdotHaulingAnalysisArtifact.h>
#include <PgsExt\PoiMap.h>

#include <IFace\GirderHandlingPointOfInterest.h>

// LOCAL INCLUDES
//

// FORWARD DECLARATIONS
//

// MISCELLANEOUS
//

/*****************************************************************************
CLASS 
   pgsKdotGirderHaulingChecker

   Design Checker for girder hauling

DESCRIPTION
   Design Checker for girder lifting and hauling


COPYRIGHT
   Copyright � 1997-1998
   Washington State Department Of Transportation
   All Rights Reserved

LOG
   rdp : 06.25.2013 : Created file
*****************************************************************************/

class pgsKdotGirderHaulingChecker: public pgsGirderHaulingChecker
{
public:
   // GROUP: LIFECYCLE

   //------------------------------------------------------------------------
   // Constructor
   pgsKdotGirderHaulingChecker(IBroker* pBroker,StatusGroupIDType statusGroupID);

   //------------------------------------------------------------------------
   // Destructor
   virtual ~pgsKdotGirderHaulingChecker();

   // GROUP: OPERATORS
   // GROUP: OPERATIONS
   virtual pgsHaulingAnalysisArtifact* CheckHauling(SpanIndexType span,GirderIndexType gdr, SHARED_LOGFILE LOGFILE);
   virtual pgsHaulingAnalysisArtifact* AnalyzeHauling(SpanIndexType span,GirderIndexType gdr);
   virtual pgsHaulingAnalysisArtifact* AnalyzeHauling(SpanIndexType span,GirderIndexType gdr,const HANDLINGCONFIG& config,IGirderHaulingDesignPointsOfInterest* pPOId);
   virtual pgsHaulingAnalysisArtifact* DesignHauling(SpanIndexType span,GirderIndexType gdr,const GDRCONFIG& config,bool bDesignForEqualOverhangs,bool bIgnoreConfigurationLimits,IGirderHaulingDesignPointsOfInterest* pPOId,bool* bSuccess, SHARED_LOGFILE LOGFILE);

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
   StatusCallbackIDType m_scidBunkPointLocation;

   // GROUP: LIFECYCLE
   // can't construct without a broker
   pgsKdotGirderHaulingChecker();

   // Prevent accidental copying and assignment
   pgsKdotGirderHaulingChecker(const pgsKdotGirderHaulingChecker&);
   pgsKdotGirderHaulingChecker& operator=(const pgsKdotGirderHaulingChecker&);

   // GROUP: OPERATORS
   // GROUP: OPERATIONS
   void AnalyzeHauling(SpanIndexType span,GirderIndexType gdr,bool bUseConfig,const HANDLINGCONFIG& config,IGirderHaulingDesignPointsOfInterest* pPOId,pgsKdotHaulingAnalysisArtifact* pArtifact);
   void PrepareHaulingAnalysisArtifact(SpanIndexType span,GirderIndexType gdr,Float64 Loh,Float64 Roh,Float64 Fc,Float64 Ec,pgsTypes::ConcreteType concType,pgsKdotHaulingAnalysisArtifact* pArtifact);

   void ComputeHaulingMoments(SpanIndexType span,GirderIndexType gdr,
                              const pgsKdotHaulingAnalysisArtifact& rArtifact, 
                              const std::vector<pgsPointOfInterest>& rpoiVec,
                              std::vector<Float64>* pmomVec, Float64* pMidSpanDeflection);

   void ComputeHaulingStresses(SpanIndexType span,GirderIndexType gdr,bool bUseConfig,
                               const HANDLINGCONFIG& haulConfig,
                               const std::vector<pgsPointOfInterest>& rpoiVec,
                               const std::vector<Float64>& momVec,
                               pgsKdotHaulingAnalysisArtifact* pArtifact);
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

