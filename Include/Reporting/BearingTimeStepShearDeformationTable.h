/////////////////////////////////////////////////////////////////////////
//// PGSuper - Prestressed Girder SUPERstructure Design and Analysis
//// Copyright � 1999-2024  Washington State Department of Transportation
////                        Bridge and Structures Office
////
//// This program is free software; you can redistribute it and/or modify
//// it under the terms of the Alternate Route Open Source License as 
//// published by the Washington State Department of Transportation, 
//// Bridge and Structures Office.
////
//// This program is distributed in the hope that it will be useful, but 
//// distribution is AS IS, WITHOUT ANY WARRANTY; without even the implied 
//// warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See 
//// the Alternate Route Open Source License for more details.
////
//// You should have received a copy of the Alternate Route Open Source 
//// License along with this program; if not, write to the Washington 
//// State Department of Transportation, Bridge and Structures Office, 
//// P.O. Box  47340, Olympia, WA 98503, USA or e-mail 
//// Bridge_Support@wsdot.wa.gov
/////////////////////////////////////////////////////////////////////////
#pragma once


#include <Reporting\ReportingExp.h>
#include <IFace\AnalysisResults.h>
#include <IFace\BearingDesignParameters.h>


interface IEAFDisplayUnits;

/*****************************************************************************
CLASS 
   CTimeStepShearDeformationTable

   Encapsulates the construction of the timestep shear deformation table.


DESCRIPTION
   Encapsulates the construction of the timestep shear deformation table.

LOG
   rab : 11.05.1998 : Created file
*****************************************************************************/

class REPORTINGCLASS CTimeStepShearDeformationTable
{
public:
   // GROUP: LIFECYCLE

   //------------------------------------------------------------------------
   // Default constructor
   CTimeStepShearDeformationTable();

   //------------------------------------------------------------------------
   // Copy constructor
   CTimeStepShearDeformationTable(const CTimeStepShearDeformationTable& rOther);

   //------------------------------------------------------------------------
   // Destructor
   virtual ~CTimeStepShearDeformationTable();

   // GROUP: OPERATORS
   //------------------------------------------------------------------------
   // Assignment operator
   CTimeStepShearDeformationTable& operator = (const CTimeStepShearDeformationTable& rOther);

   // GROUP: OPERATIONS

   //------------------------------------------------------------------------
   // 

   // Builds ......
   virtual rptRcTable* BuildTimeStepShearDeformationTable(IBroker* pBroker, TDSHEARDEFORMATIONDETAILS* pDetails) const;
   // GROUP: ACCESS
   // GROUP: INQUIRY

protected:
   // GROUP: DATA MEMBERS
   // GROUP: LIFECYCLE
   // GROUP: OPERATORS
   // GROUP: OPERATIONS
   //------------------------------------------------------------------------
   void MakeCopy(const CTimeStepShearDeformationTable& rOther);

   //------------------------------------------------------------------------
   void MakeAssignment(const CTimeStepShearDeformationTable& rOther);

   // GROUP: ACCESS
   // GROUP: INQUIRY

private:
    //GROUP: DATA MEMBERS
   // GROUP: LIFECYCLE
   // GROUP: OPERATORS
   // GROUP: OPERATIONS
   // GROUP: ACCESS
   // GROUP: INQUIRY
};

// INLINE METHODS
//

// EXTERNAL REFERENCES
//


