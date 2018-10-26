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

#include "stdafx.h"
#include "TxDOTAgent_i.h"
#include "TxDOTComponentInfo.h"
#include "resource.h"
#include <MFCTools\VersionInfo.h>

HRESULT CTxDOTComponentInfo::FinalConstruct()
{
   return S_OK;
}

void CTxDOTComponentInfo::FinalRelease()
{
}

BOOL CTxDOTComponentInfo::Init(CEAFApp* pApp)
{
   return TRUE;
}

void CTxDOTComponentInfo::Terminate()
{
}

CString CTxDOTComponentInfo::GetName()
{
   return _T("TxDOT PGSuper Extensions");
}

CString CTxDOTComponentInfo::GetDescription()
{
   CString strDesc;
   strDesc.Format(_T("TxDOT-specific features for PGSuper"));
   return strDesc;
}

HICON CTxDOTComponentInfo::GetIcon()
{
   AFX_MANAGE_STATE(AfxGetStaticModuleState());
   return AfxGetApp()->LoadIcon(IDI_TXDOT);
}

bool CTxDOTComponentInfo::HasMoreInfo()
{
   return false;
}

void CTxDOTComponentInfo::OnMoreInfo()
{
}
