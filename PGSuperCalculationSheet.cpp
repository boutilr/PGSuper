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

/****************************************************************************
CLASS
   PGSuperCalculationSheet
****************************************************************************/

#include "PGSuperAppPlugin\stdafx.h"
#include "PGSuperCalculationSheet.h"
#include <IFace\Project.h>
#include <IFace\VersionInfo.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

////////////////////////// PUBLIC     ///////////////////////////////////////

//======================== LIFECYCLE  =======================================
PGSuperCalculationSheet::PGSuperCalculationSheet(IBroker* pBroker) :
WsdotCalculationSheet()
{
   CHECK(pBroker!=0);
   m_pBroker=pBroker;
   GET_IFACE(IProjectProperties,pProj);

   // set pgsuper-specific properties
   SetBridgeName(pProj->GetBridgeName().c_str());
   SetBridgeID(pProj->GetBridgeId().c_str());
   SetJobNumber(pProj->GetJobNumber().c_str());
   SetEngineer(pProj->GetEngineer().c_str());
   SetCompany(pProj->GetCompany().c_str());

   GET_IFACE(IVersionInfo,pVerInfo);

   CString strBottomTitle;
   strBottomTitle.Format(_T("PGSuper� Version %s, Copyright � %4d, WSDOT, All rights reserved"),pVerInfo->GetVersion(true),sysDate().Year());
   SetTitle(strBottomTitle);
}

PGSuperCalculationSheet::PGSuperCalculationSheet(const PGSuperCalculationSheet& rOther) :
WsdotCalculationSheet(rOther)
{
   MakeCopy(rOther);
}

PGSuperCalculationSheet::~PGSuperCalculationSheet()
{
}

//======================== OPERATORS  =======================================
PGSuperCalculationSheet& PGSuperCalculationSheet::operator= (const PGSuperCalculationSheet& rOther)
{
   if( this != &rOther )
   {
      MakeAssignment(rOther);
   }

   return *this;
}

//======================== OPERATIONS =======================================
//======================== ACCESS     =======================================
//======================== INQUIRY    =======================================

////////////////////////// PROTECTED  ///////////////////////////////////////

//======================== LIFECYCLE  =======================================
//======================== OPERATORS  =======================================
//======================== OPERATIONS =======================================
void PGSuperCalculationSheet::MakeCopy(const PGSuperCalculationSheet& rOther)
{
   m_pBroker=rOther.m_pBroker;
}

void PGSuperCalculationSheet::MakeAssignment(const PGSuperCalculationSheet& rOther)
{
   WsdotCalculationSheet::MakeAssignment( rOther );
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
