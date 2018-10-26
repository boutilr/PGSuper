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

#ifndef INCLUDED_PGSEXT_LONGREBARDATA_H_
#define INCLUDED_PGSEXT_LONGREBARDATA_H_

// SYSTEM INCLUDES
//
#include <WBFLCore.h>

#if !defined INCLUDED_MATHEX_H_
#include <MathEx.h>
#endif

// PROJECT INCLUDES
//
#if !defined INCLUDED_PGSEXTEXP_H_
#include <PgsExt\PgsExtExp.h>
#endif

#include <StrData.h>
#include <Material\Rebar.h>

// LOCAL INCLUDES
//

// FORWARD DECLARATIONS
//
class GirderLibraryEntry;

// MISCELLANEOUS
//

/*****************************************************************************
CLASS 
   CLongitudinalRebarData

   Utility class for longitudinal rebar description data.

DESCRIPTION
   Utility class for longitudinal rebar. This class encapsulates 
   the input data a row of rebar and implements the IStructuredLoad 
   and IStructuredSave persistence interfaces.

COPYRIGHT
   Copyright � 1997-2007
   Washington State Department Of Transportation
   All Rights Reserved

LOG
   rab : 02.08.2007 : Created file
*****************************************************************************/

class PGSEXTCLASS CLongitudinalRebarData
{
public:
   struct RebarRow 
   {
      pgsTypes::GirderFace  Face;
      matRebar::Size BarSize;
      CollectionIndexType NumberOfBars;
      Float64     Cover;
      Float64     BarSpacing;

      RebarRow():
      Face(pgsTypes::GirderTop), BarSize(matRebar::bsNone), NumberOfBars(0), Cover(0), BarSpacing(0)
      {;}

      bool operator==(const RebarRow& other) const
      {
         if(Face != other.Face) return false;
         if(BarSize != other.BarSize) return false;
         if ( !IsEqual(Cover,  other.Cover) ) return false;
         if ( !IsEqual(BarSpacing,  other.BarSpacing) ) return false;
         if ( NumberOfBars != other.NumberOfBars ) return false;

         return true;
      };
   };

   matRebar::Type BarType;
   matRebar::Grade BarGrade;
   std::vector<RebarRow> RebarRows;


   // GROUP: LIFECYCLE

   //------------------------------------------------------------------------
   // Constructor
   CLongitudinalRebarData();

   //------------------------------------------------------------------------
   // Copy constructor
   CLongitudinalRebarData(const CLongitudinalRebarData& rOther);

   //------------------------------------------------------------------------
   // Destructor
   ~CLongitudinalRebarData();

   // GROUP: OPERATORS
   //------------------------------------------------------------------------
   // Assignment operator
   CLongitudinalRebarData& operator = (const CLongitudinalRebarData& rOther);
   bool operator == (const CLongitudinalRebarData& rOther) const;
   bool operator != (const CLongitudinalRebarData& rOther) const;

   // GROUP: OPERATIONS

	HRESULT Load(IStructuredLoad* pStrLoad,IProgress* pProgress);
	HRESULT Save(IStructuredSave* pStrSave,IProgress* pProgress);

   // copy shear data from a girder entry
   void CopyGirderEntryData(const GirderLibraryEntry& rGird);

   // GROUP: ACCESS
   // GROUP: INQUIRY

protected:
   // GROUP: DATA MEMBERS
   // GROUP: LIFECYCLE
   // GROUP: OPERATORS
   // GROUP: OPERATIONS
   //------------------------------------------------------------------------
   void MakeCopy(const CLongitudinalRebarData& rOther);

   //------------------------------------------------------------------------
   void MakeAssignment(const CLongitudinalRebarData& rOther);

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

// INLINE METHODS
//

// EXTERNAL REFERENCES
//

#endif // INCLUDED_PGSEXT_LONGREBARDATA_H_
