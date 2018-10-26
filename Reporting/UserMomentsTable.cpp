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

#include "StdAfx.h"
#include <Reporting\UserMomentsTable.h>
#include <Reporting\ReportNotes.h>

#include <PgsExt\GirderPointOfInterest.h>

#include <IFace\Bridge.h>
#include <IFace\AnalysisResults.h>
#include <IFace\Intervals.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/****************************************************************************
CLASS
   CUserMomentsTable
****************************************************************************/


////////////////////////// PUBLIC     ///////////////////////////////////////

//======================== LIFECYCLE  =======================================
CUserMomentsTable::CUserMomentsTable()
{
}

CUserMomentsTable::CUserMomentsTable(const CUserMomentsTable& rOther)
{
   MakeCopy(rOther);
}

CUserMomentsTable::~CUserMomentsTable()
{
}

//======================== OPERATORS  =======================================
CUserMomentsTable& CUserMomentsTable::operator= (const CUserMomentsTable& rOther)
{
   if( this != &rOther )
   {
      MakeAssignment(rOther);
   }

   return *this;
}

//======================== OPERATIONS =======================================
rptRcTable* CUserMomentsTable::Build(IBroker* pBroker,const CGirderKey& girderKey,pgsTypes::AnalysisType analysisType,IntervalIndexType intervalIdx,
                                      IEAFDisplayUnits* pDisplayUnits) const
{
   // Build table
   INIT_UV_PROTOTYPE( rptPointOfInterest, location, pDisplayUnits->GetSpanLengthUnit(), false );
   INIT_UV_PROTOTYPE( rptMomentSectionValue, moment, pDisplayUnits->GetMomentUnit(), false );
   location.IncludeSpanAndGirder(girderKey.groupIndex == ALL_GROUPS);

   GET_IFACE2(pBroker,IIntervals,pIntervals);
   CString strTitle;
   strTitle.Format(_T("Moment due to User Defined Loads in Interval %d: %s"),LABEL_INTERVAL(intervalIdx),pIntervals->GetDescription(girderKey,intervalIdx));
   rptRcTable* p_table = CreateUserLoadHeading<rptMomentUnitTag,unitmgtMomentData>(strTitle.GetBuffer(),false,analysisType,intervalIdx,pDisplayUnits,pDisplayUnits->GetMomentUnit());

   if (girderKey.groupIndex == ALL_GROUPS)
   {
      p_table->SetColumnStyle(0,pgsReportStyleHolder::GetTableCellStyle(CB_NONE | CJ_LEFT));
      p_table->SetStripeRowColumnStyle(0,pgsReportStyleHolder::GetTableStripeRowCellStyle(CB_NONE | CJ_LEFT));
   }

   // Get the interface pointers we need
   GET_IFACE2(pBroker,IPointOfInterest,pIPoi);
   GET_IFACE2(pBroker,IProductForces2,pForces2);
   GET_IFACE2(pBroker,IBridge,pBridge);

   GET_IFACE2(pBroker,IProductForces,pForces);
   pgsTypes::BridgeAnalysisType maxBAT = pForces->GetBridgeAnalysisType(analysisType,pgsTypes::Maximize);
   pgsTypes::BridgeAnalysisType minBAT = pForces->GetBridgeAnalysisType(analysisType,pgsTypes::Minimize);

   GroupIndexType nGroups = pBridge->GetGirderGroupCount();
   GroupIndexType startGroupIdx = (girderKey.groupIndex == ALL_GROUPS ? 0 : girderKey.groupIndex);
   GroupIndexType endGroupIdx   = (girderKey.groupIndex == ALL_GROUPS ? nGroups-1 : startGroupIdx);

   RowIndexType row = p_table->GetNumberOfHeaderRows();
   for ( GroupIndexType grpIdx = startGroupIdx; grpIdx <= endGroupIdx; grpIdx++ )
   {
      GirderIndexType nGirders = pBridge->GetGirderCount(grpIdx);
      GirderIndexType gdrIdx = (nGirders <= girderKey.girderIndex ? nGirders-1 : girderKey.girderIndex);

      std::vector<pgsPointOfInterest> vPoi( pIPoi->GetPointsOfInterest(CSegmentKey(grpIdx,gdrIdx,ALL_SEGMENTS)) );

      Float64 end_size = pBridge->GetSegmentStartEndDistance(CSegmentKey(grpIdx,gdrIdx,0));

      std::vector<Float64> minDC, maxDC;
      std::vector<Float64> minDW, maxDW;
      std::vector<Float64> minLLIM, maxLLIM;


      maxDC = pForces2->GetMoment( intervalIdx, pftUserDC, vPoi, maxBAT, ctIncremental );
      minDC = pForces2->GetMoment( intervalIdx, pftUserDC, vPoi, minBAT, ctIncremental );

      maxDW = pForces2->GetMoment( intervalIdx, pftUserDW, vPoi, maxBAT, ctIncremental );
      minDW = pForces2->GetMoment( intervalIdx, pftUserDW, vPoi, minBAT, ctIncremental );

      maxLLIM = pForces2->GetMoment( intervalIdx, pftUserLLIM, vPoi, maxBAT, ctIncremental );
      minLLIM = pForces2->GetMoment( intervalIdx, pftUserLLIM, vPoi, minBAT, ctIncremental );

      // Fill up the table
      IndexType index = 0;
      std::vector<pgsPointOfInterest>::const_iterator i(vPoi.begin());
      std::vector<pgsPointOfInterest>::const_iterator end(vPoi.end());
      for ( ; i != end; i++, index++ )
      {
         ColumnIndexType col = 0;
         const pgsPointOfInterest& poi = *i;

         (*p_table)(row,col++) << location.SetValue( POI_ERECTED_SEGMENT, poi, end_size );

         if ( analysisType == pgsTypes::Envelope )
         {
            (*p_table)(row,col++) << moment.SetValue( maxDC[index] );
            (*p_table)(row,col++) << moment.SetValue( minDC[index] );
            (*p_table)(row,col++) << moment.SetValue( maxDW[index] );
            (*p_table)(row,col++) << moment.SetValue( minDW[index] );
            (*p_table)(row,col++) << moment.SetValue( maxLLIM[index] );
            (*p_table)(row,col++) << moment.SetValue( minLLIM[index] );
         }
         else
         {
            (*p_table)(row,col++) << moment.SetValue( maxDC[index] );
            (*p_table)(row,col++) << moment.SetValue( maxDW[index] );
            (*p_table)(row,col++) << moment.SetValue( maxLLIM[index] );
         }

         row++;
      }
   }

   return p_table;
}

//======================== ACCESS     =======================================
//======================== INQUIRY    =======================================

////////////////////////// PROTECTED  ///////////////////////////////////////

//======================== LIFECYCLE  =======================================
//======================== OPERATORS  =======================================
//======================== OPERATIONS =======================================
void CUserMomentsTable::MakeCopy(const CUserMomentsTable& rOther)
{
   // Add copy code here...
}

void CUserMomentsTable::MakeAssignment(const CUserMomentsTable& rOther)
{
   MakeCopy( rOther );
}

//======================== ACCESS     =======================================
//======================== INQUIRY    =======================================

////////////////////////// PRIVATE    ///////////////////////////////////////

//======================== LIFECYCLE  =======================================
//======================== OPERATORS  =======================================
//======================== OPERATIONS =======================================
//======================== ACCESS     =======================================
//======================== INQUERY    =======================================

//======================== DEBUG      =======================================
#if defined _DEBUG
bool CUserMomentsTable::AssertValid() const
{
   return true;
}

void CUserMomentsTable::Dump(dbgDumpContext& os) const
{
   os << _T("Dump for CUserMomentsTable") << endl;
}
#endif // _DEBUG

#if defined _UNITTEST
bool CUserMomentsTable::TestMe(dbgLog& rlog)
{
   TESTME_PROLOGUE("CUserMomentsTable");

   TEST_NOT_IMPLEMENTED("Unit Tests Not Implemented for CUserMomentsTable");

   TESTME_EPILOG("CUserMomentsTable");
}
#endif // _UNITTEST
