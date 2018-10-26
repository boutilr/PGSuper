///////////////////////////////////////////////////////////////////////
// PGSuper - Prestressed Girder SUPERstructure Design and Analysis
// Copyright (C) 2001  Washington State Department of Transportation
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
// 4500 3rd AVE SE - P.O. Box  47340, Olympia, WA 98503, USA or e-mail 
// Bridge_Support@wsdot.wa.gov
///////////////////////////////////////////////////////////////////////

#ifndef INCLUDED_DECKELEVATIONSCHAPTERBUILDER_H_
#define INCLUDED_DECKELEVATIONSCHAPTERBUILDER_H_

// SYSTEM INCLUDES
//

// PROJECT INCLUDES
//
#include "ChapterBuilder.h"

// LOCAL INCLUDES
//

// FORWARD DECLARATIONS
//
interface IDisplayUnits;

// MISCELLANEOUS
//

/*****************************************************************************
CLASS 
   CDeckElevationsChapterBuilder

   Geometry Chapter Builder


DESCRIPTION
   Reports some important geometrical properties of the bridge

COPYRIGHT
   Copyright � 1997-2001
   Washington State Department Of Transportation
   All Rights Reserved

LOG
   rab : 10.23.2001 : Created file
*****************************************************************************/

class CDeckElevationsChapterBuilder : public CChapterBuilder
{
public:
   // GROUP: LIFECYCLE
   CDeckElevationsChapterBuilder();

   // GROUP: OPERATORS
   // GROUP: OPERATIONS

   //------------------------------------------------------------------------
   virtual LPCTSTR GetName() const;
   virtual Uint32 GetMaxLevel() const;

   //------------------------------------------------------------------------
   virtual rptChapter* Build(IBroker* pBroker,Int16 span,Int16 girder,
                             IDisplayUnits* pDispUnits,
                             Int16 level) const;

   //------------------------------------------------------------------------
   virtual CChapterBuilder* Clone() const;

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
   // GROUP: LIFECYCLE

   // Prevent accidental copying and assignment
   CDeckElevationsChapterBuilder(const CDeckElevationsChapterBuilder&);
   CDeckElevationsChapterBuilder& operator=(const CDeckElevationsChapterBuilder&);

   // GROUP: OPERATORS
   // GROUP: OPERATIONS
   // GROUP: ACCESS
   // GROUP: INQUIRY
};

// INLINE METHODS
//

// EXTERNAL REFERENCES
//

#endif // INCLUDED_DECKELEVATIONSCHAPTERBUILDER_H_
