///////////////////////////////////////////////////////////////////////
// PGSuper - Prestressed Girder SUPERstructure Design and Analysis
// Copyright � 1999-2020  Washington State Department of Transportation
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

#include <Reporting\ReportingExp.h>
#include <Reporter\Chapter.h>
#include <Reporting\PGSuperChapterBuilder.h>


interface IEAFDisplayUnits;

class REPORTINGCLASS CTimeStepCamberChapterBuilder : public CPGSuperChapterBuilder
{
public:
   CTimeStepCamberChapterBuilder(bool bSelect = true);

   virtual LPCTSTR GetName() const;
   virtual rptChapter* Build(CReportSpecification* pRptSpec,Uint16 level) const;
   virtual CChapterBuilder* Clone() const;

private:
   // Prevent accidental copying and assignment
   CTimeStepCamberChapterBuilder(const CTimeStepCamberChapterBuilder&) = delete;
   CTimeStepCamberChapterBuilder& operator=(const CTimeStepCamberChapterBuilder&) = delete;

   rptRcTable* CreateStorageDeflectionTable(IBroker* pBroker,const CGirderKey& girderKey) const;
   rptRcTable* CreateAfterErectionDeflectionTable(IBroker* pBroker,const CGirderKey& girderKey) const;

   rptRcTable* CreateTable(IBroker* pBroker,const CSegmentKey& segmentKey,IntervalIndexType intervalIdx) const;


   rptRcTable* CreateBeforeSlabCastingDeflectionTable(IBroker* pBroker,const CGirderKey& girderKey) const;
   rptRcTable* CreateScreedCamberDeflectionTable(IBroker* pBroker,const CGirderKey& girderKey) const;
   rptRcTable* CreateExcessCamberTable(IBroker* pBroker,const CGirderKey& girderKey) const;
   rptRcTable* CreateFinalDeflectionTable(IBroker* pBroker,const CGirderKey& girderKey) const;
};
