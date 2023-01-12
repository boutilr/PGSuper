///////////////////////////////////////////////////////////////////////
// PGSuper - Prestressed Girder SUPERstructure Design and Analysis
// Copyright � 1999-2023  Washington State Department of Transportation
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

#include <PgsExt\PgsExtLib.h>
#include <PgsExt\SplittingCheckEngineer.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

pgsSplittingCheckEngineer::pgsSplittingCheckEngineer()
{
}

pgsSplittingCheckEngineer::pgsSplittingCheckEngineer(IBroker* pBroker) :
   m_pBroker(pBroker)
{
}

pgsSplittingCheckEngineer::~pgsSplittingCheckEngineer()
{
}

void pgsSplittingCheckEngineer::SetBroker(IBroker* pBroker)
{
   m_pBroker = pBroker;
}

LPCTSTR pgsSplittingCheckEngineer::GetCheckName()
{
   if (lrfdVersionMgr::FourthEditionWith2008Interims <= lrfdVersionMgr::GetVersion())
   {
      return _T("Splitting");
   }
   else
   {
      return _T("Bursting");
   }
}
