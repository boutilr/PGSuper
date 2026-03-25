///////////////////////////////////////////////////////////////////////
// PGSuper - Prestressed Girder SUPERstructure Design and Analysis
// Copyright © 1999-2026  Washington State Department of Transportation
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

// CommonPierLayoutDlg.cpp : implementation file
//

///////////////////////////////////////////////////////////////////////////
// NOTE: Duplicate code warning
//
// This dialog along with all its property pages are basically repeated in
// the PGSuperLibrary project. I could not get a single implementation to
// work because of issues with the module resources.
//
// If changes are made here, the same changes are likely needed in
// the other location.

#include "stdafx.h"
#include "CommonPierLayoutDlg.h"
#include <EAF\EAFDisplayUnits.h>
#include <IFace\Project.h>
#include <AgentTools.h>


/////////////////////////////////////////////////////////////////////////////
// CBearingDetailsDlg dialog


CCommonPierLayoutDlg::CCommonPierLayoutDlg(CWnd* pParent)
	:CDialog(IDD_PIER_LAYOUT, pParent)
{

   Init();

}

BEGIN_MESSAGE_MAP(CCommonPierLayoutDlg, CDialog)
	//ON_EN_CHANGE(IDC_EDIT_BEARING_LENGTH, &CBearingDetailsDlg::OnEnChangeBearingInput)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CCommonPierLayoutDlg message handlers

BOOL CCommonPierLayoutDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	//auto pBroker = EAFGetBroker();
	//GET_IFACE2(pBroker, ISpecification, pSpec);
	//GET_IFACE2(pBroker, ILibrary, pLib);
	//pgsTypes::AnalysisType analysisType = pSpec->GetAnalysisType();
	//const auto pSpecEntry = pLib->GetSpecEntry(pSpec->GetSpecification().c_str());
	//const WBFL::EngTools::BearingProjectCriteria& criteria = pSpecEntry->GetBearingCriteria();

	//if (criteria.AnalysisMethod == WBFL::EngTools::BearingAnalysisMethod::MethodA)
	//{
	//	MethodAControls(SW_SHOW);
	//	MethodBControls(SW_HIDE);
	//}
	//else
	//{
	//	MethodAControls(SW_HIDE);
	//	MethodBControls(SW_SHOW);
	//}

	//UpdateOptimizationResults();

    return TRUE;
}


void CCommonPierLayoutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);

	auto pBroker = EAFGetBroker();
	GET_IFACE2(pBroker, IEAFDisplayUnits, pDisplayUnits);

}

void CCommonPierLayoutDlg::Init()
{

}



