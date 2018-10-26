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

#include "StdAfx.h"

#include <Reporting\LongReinfShearCheck.h>
#include <Reporting\ReportNotes.h>

#include <IFace\Bridge.h>
#include <IFace\Artifact.h>
#include <IFace\DisplayUnits.h>

#include <PgsExt\GirderArtifact.h>
#include <PgsExt\PointOfInterest.h>
#include <PgsExt\PointOfInterest.h>

#include <PsgLib\SpecLibraryEntry.h>

#include <Reporter\ReportingUtils.h>


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/****************************************************************************
CLASS
   CLongReinfShearCheck
****************************************************************************/

////////////////////////// PUBLIC     ///////////////////////////////////////

//======================== LIFECYCLE  =======================================
CLongReinfShearCheck::CLongReinfShearCheck()
{
}

CLongReinfShearCheck::CLongReinfShearCheck(const CLongReinfShearCheck& rOther)
{
   MakeCopy(rOther);
}

CLongReinfShearCheck::~CLongReinfShearCheck()
{
}

//======================== OPERATORS  =======================================
CLongReinfShearCheck& CLongReinfShearCheck::operator= (const CLongReinfShearCheck& rOther)
{
   if( this != &rOther )
   {
      MakeAssignment(rOther);
   }

   return *this;
}

//======================== OPERATIONS =======================================
void CLongReinfShearCheck::Build(rptChapter* pChapter,
                              IBroker* pBroker,SpanIndexType span,GirderIndexType girder,
                              pgsTypes::Stage stage,pgsTypes::LimitState ls,
                              IDisplayUnits* pDispUnit) const
{
   INIT_UV_PROTOTYPE( rptPointOfInterest, location, pDispUnit->GetSpanLengthUnit(),   false );
   INIT_UV_PROTOTYPE( rptForceSectionValue, shear,  pDispUnit->GetShearUnit(), false );

   rptRcScalar scalar;
   scalar.SetFormat( pDispUnit->GetScalarFormat().Format );
   scalar.SetWidth( pDispUnit->GetScalarFormat().Width );
   scalar.SetPrecision( pDispUnit->GetScalarFormat().Precision );

   rptParagraph* pTitle = new rptParagraph( pgsReportStyleHolder::GetHeadingStyle() );
   *pChapter << pTitle;

   if ( ls == pgsTypes::StrengthI )
      *pTitle << "Longitudinal Reinforcement For Shear Check - Strength I Limit State [5.8.3.5]";
   else
      *pTitle << "Longitudinal Reinforcement For Shear Check - Strength II Limit State [5.8.3.5]";

   rptParagraph* pBody = new rptParagraph;
   *pChapter << pBody;

   if ( lrfdVersionMgr::ThirdEditionWith2005Interims <= lrfdVersionMgr::GetVersion() )
      *pBody <<rptRcImage(pgsReportStyleHolder::GetImagePath() + "Longitudinal Reinforcement Check Equation 2005.gif")<<rptNewLine;
   else
      *pBody <<rptRcImage(pgsReportStyleHolder::GetImagePath() + "Longitudinal Reinforcement Check Equation.gif")<<rptNewLine;

   rptRcTable* table = pgsReportStyleHolder::CreateDefaultTable(5,"");
   *pBody << table;

   if ( stage == pgsTypes::CastingYard )
      (*table)(0,0)  << COLHDR(RPT_GDR_END_LOCATION, rptLengthUnitTag, pDispUnit->GetSpanLengthUnit());
   else
      (*table)(0,0)  << COLHDR(RPT_LFT_SUPPORT_LOCATION, rptLengthUnitTag, pDispUnit->GetSpanLengthUnit());

   (*table)(0,1)  << COLHDR("Capacity",rptForceUnitTag, pDispUnit->GetShearUnit() );
   (*table)(0,2)  << COLHDR("Demand",rptForceUnitTag, pDispUnit->GetShearUnit() );
   (*table)(0,3)  << "Equation";
   (*table)(0,4)  << "Status" << rptNewLine << "(C/D)";

   // Fill up the table
   GET_IFACE2(pBroker,IBridge,pBridge);
   GET_IFACE2(pBroker,IPointOfInterest,pIPoi);
   GET_IFACE2(pBroker,IArtifact,pIArtifact);

   const pgsGirderArtifact* gdrArtifact = pIArtifact->GetArtifact(span,girder);
   const pgsStirrupCheckArtifact* pstirrup_artifact= gdrArtifact->GetStirrupCheckArtifact();
   CHECK(pstirrup_artifact);

   std::vector<pgsPointOfInterest> vPoi = pIPoi->GetPointsOfInterest( stage, span, girder, POI_TABULAR|POI_SHEAR );

   Float64 end_size = pBridge->GetGirderStartConnectionLength(span,girder);
   if ( stage == pgsTypes::CastingYard )
      end_size = 0; // don't adjust if CY stage

   bool bAddFootnote = false;

   RowIndexType row = table->GetNumberOfHeaderRows();

   std::vector<pgsPointOfInterest>::const_iterator i;
   for ( i = vPoi.begin(); i != vPoi.end(); i++ )
   {
      const pgsPointOfInterest& poi = *i;

      const pgsStirrupCheckAtPoisArtifact* psArtifact = pstirrup_artifact->GetStirrupCheckAtPoisArtifact( pgsStirrupCheckAtPoisArtifactKey(stage,ls,poi.GetDistFromStart()) );
      if ( psArtifact == NULL )
         continue;

      const pgsLongReinfShearArtifact* pArtifact = psArtifact->GetLongReinfShearArtifact();

      if ( pArtifact->IsApplicable() )
      {
         (*table)(row,0) << location.SetValue( poi, end_size );

         double C = pArtifact->GetCapacityForce();
         double D = pArtifact->GetDemandForce();
         (*table)(row,1) << shear.SetValue( C );
         (*table)(row,2) << shear.SetValue( D );

         (*table)(row,3) << "5.8.3.5-" << pArtifact->GetEquation();

         if ( pArtifact->Passed() )
            (*table)(row,4) << RPT_PASS;
         else
            (*table)(row,4) << RPT_FAIL;

         double ratio = IsZero(D) ? DBL_MAX : C/D;
         if ( pArtifact->Passed() && fabs(pArtifact->GetMu()) <= fabs(pArtifact->GetMr()) && ratio < 1.0 )
         {
            bAddFootnote = true;
            (*table)(row,4) << "*";
         }

         if ( IsZero(D) )
         {
            (*table)(row,4) << rptNewLine << "(" << symbol(INFINITY) << ")";
         }
         else
         {
            (*table)(row,4) << rptNewLine << "(" << scalar.SetValue(ratio) << ")";
         }

         row++;
      }
   }

   if ( bAddFootnote )
   {
      rptParagraph* pFootnote = new rptParagraph(pgsReportStyleHolder::GetFootnoteStyle());
      *pChapter << pFootnote;

      *pFootnote << "* The area of longitudinal reinforcement on the flexural tension side of the member need not exceed the area required to resist the maximum moment acting alone" << rptNewLine;
   }
}

//======================== ACCESS     =======================================
//======================== INQUIRY    =======================================

////////////////////////// PROTECTED  ///////////////////////////////////////

//======================== LIFECYCLE  =======================================
//======================== OPERATORS  =======================================
//======================== OPERATIONS =======================================
void CLongReinfShearCheck::MakeCopy(const CLongReinfShearCheck& rOther)
{
   // Add copy code here...
}

void CLongReinfShearCheck::MakeAssignment(const CLongReinfShearCheck& rOther)
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
bool CLongReinfShearCheck::AssertValid() const
{
   return true;
}

void CLongReinfShearCheck::Dump(dbgDumpContext& os) const
{
   os << "Dump for CLongReinfShearCheck" << endl;
}
#endif // _DEBUG

#if defined _UNITTEST
bool CLongReinfShearCheck::TestMe(dbgLog& rlog)
{
   TESTME_PROLOGUE("CLongReinfShearCheck");

   TEST_NOT_IMPLEMENTED("Unit Tests Not Implemented for CLongReinfShearCheck");

   TESTME_EPILOG("LiveLoadDistributionFactorTable");
}
#endif // _UNITTEST
