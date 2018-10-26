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

#include "StdAfx.h"
#include "TexasStressChecksChapterBuilder.h"
#include <Reporting\ReportStyleHolder.h>
#include <Reporting\SpanGirderReportSpecification.h>
#include <Reporting\StrandStressCheckTable.h>
#include <Reporting\FlexuralStressCheckTable.h>
#include <Reporting\FlexuralCapacityCheckTable.h>
#include <Reporting\ShearCheckTable.h>
#include <Reporting\InterfaceShearTable.h>
#include <Reporting\StrandSlopeCheck.h>
#include <Reporting\HoldDownForceCheck.h>
#include <Reporting\ConstructabilityCheckTable.h>
#include <Reporting\CamberTable.h>
#include <Reporting\GirderDetailingCheck.h>
#include <Reporting\LiftingCheck.h>
#include <Reporting\HaulingCheck.h>
#include <Reporting\LongReinfShearCheck.h>
#include <Reporting\OptionalDeflectionCheck.h>
#include <Reporting\DebondCheckTable.h>

#include <IFace\Bridge.h>
#include <EAF\EAFDisplayUnits.h>
#include <IFace\AnalysisResults.h>
#include <IFace\Artifact.h>
#include <IFace\Project.h>
#include <IFace\Allowables.h>

#include <PgsExt\GirderArtifact.h>


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/****************************************************************************
CLASS
   CTexasStressChecksChapterBuilder
****************************************************************************/


////////////////////////// PUBLIC     ///////////////////////////////////////

//======================== LIFECYCLE  =======================================
CTexasStressChecksChapterBuilder::CTexasStressChecksChapterBuilder(bool bSelect) :
CPGSuperChapterBuilder(bSelect)
{
}

//======================== OPERATORS  =======================================
//======================== OPERATIONS =======================================
LPCTSTR CTexasStressChecksChapterBuilder::GetName() const
{
   return TEXT("Stress Checks");
}

rptChapter* CTexasStressChecksChapterBuilder::Build(CReportSpecification* pRptSpec,Uint16 level) const
{
   CSpanGirderReportSpecification* pSGRptSpec = dynamic_cast<CSpanGirderReportSpecification*>(pRptSpec);
   CComPtr<IBroker> pBroker;
   pSGRptSpec->GetBroker(&pBroker);
   SpanIndexType span = pSGRptSpec->GetSpan();
   GirderIndexType girder = pSGRptSpec->GetGirder();

   GET_IFACE2(pBroker,IEAFDisplayUnits,pDisplayUnits);

   rptChapter* pChapter = CPGSuperChapterBuilder::Build(pRptSpec,level);

   rptParagraph* pPara = new rptParagraph;
   *pChapter << pPara;

   GET_IFACE2(pBroker,ISpecification,pSpec);
   *pPara << _T("Specification = ") << pSpec->GetSpecification() << rptNewLine;


   GET_IFACE2(pBroker,IArtifact,pArtifacts);
   const pgsGirderArtifact* pArtifact = pArtifacts->GetArtifact(span,girder);
   rptParagraph* p = new rptParagraph;
   *pChapter << p;

   CFlexuralStressCheckTable().Build(pChapter,pBroker,span,girder,pDisplayUnits,pgsTypes::CastingYard,pgsTypes::ServiceI);
   CFlexuralStressCheckTable().Build(pChapter,pBroker,span,girder,pDisplayUnits,pgsTypes::BridgeSite1,pgsTypes::ServiceI);
   CFlexuralStressCheckTable().Build(pChapter,pBroker,span,girder,pDisplayUnits,pgsTypes::BridgeSite2,pgsTypes::ServiceI,pgsTypes::Compression);
   CFlexuralStressCheckTable().Build(pChapter,pBroker,span,girder,pDisplayUnits,pgsTypes::BridgeSite3,pgsTypes::ServiceI,pgsTypes::Compression);

   if ( lrfdVersionMgr::GetVersion() < lrfdVersionMgr::FourthEditionWith2009Interims )
      CFlexuralStressCheckTable().Build(pChapter,pBroker,span,girder,pDisplayUnits,pgsTypes::BridgeSite3,pgsTypes::ServiceIA,pgsTypes::Compression);

   CFlexuralStressCheckTable().Build(pChapter,pBroker,span,girder,pDisplayUnits,pgsTypes::BridgeSite3,pgsTypes::ServiceIII,pgsTypes::Tension);

   if ( lrfdVersionMgr::FourthEditionWith2009Interims <= lrfdVersionMgr::GetVersion() )
      CFlexuralStressCheckTable().Build(pChapter,pBroker,span,girder,pDisplayUnits,pgsTypes::BridgeSite3,pgsTypes::FatigueI,pgsTypes::Compression);


 
   return pChapter;
}

CChapterBuilder* CTexasStressChecksChapterBuilder::Clone() const
{
   return new CTexasStressChecksChapterBuilder;
}

//======================== ACCESS     =======================================
//======================== INQUIRY    =======================================

////////////////////////// PROTECTED  ///////////////////////////////////////

//======================== LIFECYCLE  =======================================
//======================== OPERATORS  =======================================
//======================== OPERATIONS =======================================
//======================== ACCESS     =======================================
//======================== INQUIRY    =======================================

////////////////////////// PRIVATE    ///////////////////////////////////////

//======================== LIFECYCLE  =======================================
//======================== OPERATORS  =======================================
//======================== OPERATIONS =======================================
//======================== ACCESS     =======================================
//======================== INQUERY    =======================================

