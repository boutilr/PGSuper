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

// SpecMomentPage.cpp : implementation file
//

#include "stdafx.h"
#include "psglib\psglib.h"
#include "SpecMomentPage.h"
#include "SpecMainSheet.h"
#include "..\htmlhelp\HelpTopics.hh"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CSpecMomentPage property page

IMPLEMENT_DYNCREATE(CSpecMomentPage, CPropertyPage)

CSpecMomentPage::CSpecMomentPage() : CPropertyPage(CSpecMomentPage::IDD)
{
	//{{AFX_DATA_INIT(CSpecMomentPage)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}

CSpecMomentPage::~CSpecMomentPage()
{
}

void CSpecMomentPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CSpecBridgeSitePage)
	//}}AFX_DATA_MAP

   CSpecMainSheet* pDad = (CSpecMainSheet*)GetParent();
   // dad is a friend of the entry. use him to transfer data.
   pDad->ExchangeMomentCapacityData(pDX);
}


BEGIN_MESSAGE_MAP(CSpecMomentPage, CPropertyPage)
	//{{AFX_MSG_MAP(CSpecMomentPage)
		// NOTE: the ClassWizard will add message map macros here
	//}}AFX_MSG_MAP
	ON_MESSAGE(WM_COMMANDHELP, OnCommandHelp)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CSpecMomentPage message handlers

LRESULT CSpecMomentPage::OnCommandHelp(WPARAM, LPARAM lParam)
{
   ::HtmlHelp( *this, AfxGetApp()->m_pszHelpFilePath, HH_HELP_CONTEXT, IDH_MOMENT_TAB );
   return TRUE;
}

BOOL CSpecMomentPage::OnInitDialog()
{
   CComboBox* pCB = (CComboBox*)GetDlgItem(IDC_NEG_MOMENT);
   pCB->SetItemData(pCB->AddString(_T("Include noncomposite moments in Mu")),(DWORD_PTR)true);
   pCB->SetItemData(pCB->AddString(_T("Exclude noncomposite moments from Mu")),(DWORD_PTR)false);
   CPropertyPage::OnInitDialog();

   return TRUE;  // return TRUE unless you set the focus to a control
   // EXCEPTION: OCX Property Pages should return FALSE
}

BOOL CSpecMomentPage::OnSetActive()
{
   // move windows based on current spec here
   CSpecMainSheet* pDad = (CSpecMainSheet*)GetParent();
   CWnd* wndMoment = GetDlgItem(IDC_MOMENT);
   if ( lrfdVersionMgr::ThirdEditionWith2005Interims < pDad->m_Entry.GetSpecificationType() )
   {
      wndMoment->ShowWindow(SW_HIDE);
   }
   else
   {
      wndMoment->ShowWindow(SW_SHOW);
   }

   return CPropertyPage::OnSetActive();
}
