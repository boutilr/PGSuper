// SpecHaulingMethodPropertyPage.cpp : implementation file
//

#include "stdafx.h"
#include "resource.h"
#include "SpecHaulingMethodPropertyPage.h"
#include "SpecPropertySheet.h"
#include <EAF\EAFDocument.h>

// CSpecLiftingModulusOfRupturePropertyPage

IMPLEMENT_DYNAMIC(CSpecHaulingMethodPropertyPage, CMFCPropertyPage)

CSpecHaulingMethodPropertyPage::CSpecHaulingMethodPropertyPage() :
   CMFCPropertyPage(IDD_SPEC_HAULING_METHOD)
{
}

CSpecHaulingMethodPropertyPage::~CSpecHaulingMethodPropertyPage()
{
}


BEGIN_MESSAGE_MAP(CSpecHaulingMethodPropertyPage, CMFCPropertyPage)
   ON_BN_CLICKED(ID_HELP, OnHelp)
END_MESSAGE_MAP()



// CSpecHaulingMethodPropertyPage message handlers

void CSpecHaulingMethodPropertyPage::DoDataExchange(CDataExchange* pDX)
{
   __super::DoDataExchange(pDX);
   //{{AFX_DATA_MAP(CSpecDescrPage)
   //}}AFX_DATA_MAP

   CSpecPropertySheet* pParent = (CSpecPropertySheet*)GetParent();
   pParent->ExchangeHaulingMethodData(pDX);
}


BOOL CSpecHaulingMethodPropertyPage::OnInitDialog()
{
   __super::OnInitDialog();

   return TRUE;  // return TRUE unless you set the focus to a control
                 // EXCEPTION: OCX Property Pages should return FALSE
}

void CSpecHaulingMethodPropertyPage::OnHelp()
{
#pragma Reminder("WORKING HERE - Dialogs - Need to update help")
   EAFHelp(EAFGetDocument()->GetDocumentationSetName(), IDH_PROJECT_CRITERIA_GENERAL);
}
