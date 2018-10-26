///////////////////////////////////////////////////////////////////////
// PGSuper - Prestressed Girder SUPERstructure Design and Analysis
// Copyright (C) 1999  Washington State Department of Transportation
//                     Bridge and Structures Office
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

#ifndef INCLUDED_PGSEXT_DECKREBARDATA_H_
#define INCLUDED_PGSEXT_DECKREBARDATA_H_

// SYSTEM INCLUDES
//
#include <WBFLCore.h>


// PROJECT INCLUDES
//
#if !defined INCLUDED_PGSEXTEXP_H_
#include <PgsExt\PgsExtExp.h>
#endif

#include <StrData.h>

// LOCAL INCLUDES
//

// FORWARD DECLARATIONS
//

// MISCELLANEOUS
//

/*****************************************************************************
CLASS 
   CDeckRebarData

   Utility class for deck reinforcement description data.

DESCRIPTION
   Utility class for deck reinforcment description data. This class encapsulates all
   the input data for deck reinforcment and implements the IStructuredLoad and 
   IStructuredSave persistence interfaces.


COPYRIGHT
   Copyright � 1997-2007
   Washington State Department Of Transportation
   All Rights Reserved

LOG
   rab : 01.11.2007 : Created file
*****************************************************************************/
class PGSEXTCLASS CDeckRebarData
{
public:
   double TopCover;
   double BottomCover;

   BarSizeType TopRebarKey;
   BarSizeType BottomRebarKey;

   double TopSpacing;
   double BottomSpacing;

   double TopLumpSum;
   double BottomLumpSum;

   std::string strRebarMaterial;

   enum RebarMat { TopMat, BottomMat };
   struct NegMomentRebarData
   {
      PierIndexType PierIdx;
      RebarMat Mat;
      double LumpSum;
      BarSizeType RebarKey;
      double Spacing;
      double LeftCutoff;
      double RightCutoff;

      bool operator!=(const NegMomentRebarData& rOther) const;
      bool operator==(const NegMomentRebarData& rOther) const;
   };
   std::vector<NegMomentRebarData> NegMomentRebar;

   // GROUP: LIFECYCLE

   //------------------------------------------------------------------------
   // Default constructor
   CDeckRebarData();

   //------------------------------------------------------------------------
   // Copy constructor
   CDeckRebarData(const CDeckRebarData& rOther);

   //------------------------------------------------------------------------
   // Destructor
   ~CDeckRebarData();

   // GROUP: OPERATORS
   //------------------------------------------------------------------------
   // Assignment operator
   CDeckRebarData& operator = (const CDeckRebarData& rOther);

   bool operator!=(const CDeckRebarData& rOther) const;
   bool operator==(const CDeckRebarData& rOther) const;

   // GROUP: OPERATIONS

	HRESULT Load(IStructuredLoad* pStrLoad,IProgress* pProgress);
	HRESULT Save(IStructuredSave* pStrSave,IProgress* pProgress);

   // GROUP: ACCESS
   // GROUP: INQUIRY

protected:
   // GROUP: DATA MEMBERS
   // GROUP: LIFECYCLE
   // GROUP: OPERATORS
   // GROUP: OPERATIONS
   //------------------------------------------------------------------------
   void MakeCopy(const CDeckRebarData& rOther);

   //------------------------------------------------------------------------
   void MakeAssignment(const CDeckRebarData& rOther);

   // GROUP: ACCESS
   // GROUP: INQUIRY

private:
   // GROUP: DATA MEMBERS
   // GROUP: LIFECYCLE
   // GROUP: OPERATORS
   // GROUP: OPERATIONS
   // GROUP: ACCESS
   // GROUP: INQUIRY
};


#endif // INCLUDED_PGSEXT_DECKREBARDATA_H_
