///////////////////////////////////////////////////////////////////////
// PGSuper - Prestressed Girder SUPERstructure Design and Analysis
// Copyright � 1999-2014  Washington State Department of Transportation
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

// ChangeOfConcreteStressTable.h : Declaration of the CChangeOfConcreteStressTable

#ifndef __CHANGEOFCONCRETESTRESSTABLE_H_
#define __CHANGEOFCONCRETESTRESSTABLE_H_

#include "resource.h"       // main symbols
#include <Details.h>
#include <EAF\EAFDisplayUnits.h>
#include <PgsExt\ReportStyleHolder.h>

class lrfdLosses;

/////////////////////////////////////////////////////////////////////////////
// CChangeOfConcreteStressTable
class CChangeOfConcreteStressTable : public rptRcTable
{
public:
	static CChangeOfConcreteStressTable* PrepareTable(rptChapter* pChapter,IBroker* pBroker,SpanIndexType span,GirderIndexType gdr,IEAFDisplayUnits* pDisplayUnits,Uint16 level);
   void AddRow(rptChapter* pChapter,IBroker* pBroker,const pgsPointOfInterest& poi,RowIndexType row,LOSSDETAILS& details,IEAFDisplayUnits* pDisplayUnits,Uint16 level);

private:
   CChangeOfConcreteStressTable(ColumnIndexType NumColumns, IEAFDisplayUnits* pDisplayUnits);

   DECLARE_UV_PROTOTYPE( rptPointOfInterest,  spanloc );
   DECLARE_UV_PROTOTYPE( rptPointOfInterest,  gdrloc );
   DECLARE_UV_PROTOTYPE( rptLength4UnitValue, mom_inertia );
   DECLARE_UV_PROTOTYPE( rptLengthUnitValue,  dim );
   DECLARE_UV_PROTOTYPE( rptMomentUnitValue,  moment );
   DECLARE_UV_PROTOTYPE( rptStressUnitValue,  stress );
};

#endif //__CHANGEOFCONCRETESTRESSTABLE_H_
