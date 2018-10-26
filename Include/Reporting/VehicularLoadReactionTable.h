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

#ifndef INCLUDED_VEHICULARLOADREACTIONTABLE_H_
#define INCLUDED_VEHICULARLOADREACTIONTABLE_H_

#include <Reporting\ReportingExp.h>
#include <IFace\AnalysisResults.h>

interface IEAFDisplayUnits;

/*****************************************************************************
CLASS 
   CVehicularLoadReactionTable

   Encapsulates the construction of the vehicular load reaction table.


DESCRIPTION
   Encapsulates the construction of the vehicular load reaction table.


COPYRIGHT
   Copyright � 1997-2008
   Washington State Department Of Transportation
   All Rights Reserved

LOG
   rab : 01.09.2008 : Created file
*****************************************************************************/

class REPORTINGCLASS CVehicularLoadReactionTable
{
public:
   // GROUP: LIFECYCLE

   //------------------------------------------------------------------------
   // Default constructor
   CVehicularLoadReactionTable();

   //------------------------------------------------------------------------
   // Copy constructor
   CVehicularLoadReactionTable(const CVehicularLoadReactionTable& rOther);

   //------------------------------------------------------------------------
   // Destructor
   virtual ~CVehicularLoadReactionTable();

   // GROUP: OPERATORS
   //------------------------------------------------------------------------
   // Assignment operator
   CVehicularLoadReactionTable& operator = (const CVehicularLoadReactionTable& rOther);

   // GROUP: OPERATIONS

   //------------------------------------------------------------------------
   // Builds the strand eccentricity table.
   virtual rptRcTable* Build(IBroker* pBroker,
                             SpanIndexType span,
                             GirderIndexType girder,
                             pgsTypes::LiveLoadType llType,
                             const std::_tstring& strLLName,
                             VehicleIndexType vehicleIndex, 
                             pgsTypes::AnalysisType analysisType,
                             bool bReportTruckConfig,
                             bool bIncludeRotations,
                             IEAFDisplayUnits* pDisplayUnits) const;

   // GROUP: ACCESS
   // GROUP: INQUIRY

protected:
   // GROUP: DATA MEMBERS
   // GROUP: LIFECYCLE
   // GROUP: OPERATORS
   // GROUP: OPERATIONS
   //------------------------------------------------------------------------
   void MakeCopy(const CVehicularLoadReactionTable& rOther);

   //------------------------------------------------------------------------
   void MakeAssignment(const CVehicularLoadReactionTable& rOther);

   // GROUP: ACCESS
   // GROUP: INQUIRY

private:
   // GROUP: DATA MEMBERS
   // GROUP: LIFECYCLE
   // GROUP: OPERATORS
   // GROUP: OPERATIONS
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

#endif // INCLUDED_VEHICULARLOADREACTIONTABLE_H_
