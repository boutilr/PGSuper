// SpecLiftingModulusOfRupturePropertyPage.cpp : implementation file
//

#include "stdafx.h"
#include "resource.h"
#include "SpecLiftingModulusOfRupturePropertyPage.h"
#include "SpecPropertySheet.h"
#include <EAF\EAFDocument.h>

// CSpecLiftingModulusOfRupturePropertyPage

IMPLEMENT_DYNAMIC(CSpecLiftingModulusOfRupturePropertyPage, CMFCPropertyPage)

CSpecLiftingModulusOfRupturePropertyPage::CSpecLiftingModulusOfRupturePropertyPage() :
   CMFCPropertyPage(IDD_SPEC_LIFTING_MOR)
{
}

CSpecLiftingModulusOfRupturePropertyPage::~CSpecLiftingModulusOfRupturePropertyPage()
{
}


BEGIN_MESSAGE_MAP(CSpecLiftingModulusOfRupturePropertyPage, CMFCPropertyPage)
   ON_BN_CLICKED(ID_HELP, OnHelp)
END_MESSAGE_MAP()



// CSpecLiftingModulusOfRupturePropertyPage message handlers

void CSpecLiftingModulusOfRupturePropertyPage::DoDataExchange(CDataExchange* pDX)
{
   __super::DoDataExchange(pDX);
   //{{AFX_DATA_MAP(CSpecDescrPage)
   //}}AFX_DATA_MAP

   CSpecPropertySheet* pParent = (CSpecPropertySheet*)GetParent();
   pParent->ExchangeLiftingModulusOfRuptureData(pDX);
}


BOOL CSpecLiftingModulusOfRupturePropertyPage::OnInitDialog()
{
   __super::OnInitDialog();

   return TRUE;  // return TRUE unless you set the focus to a control
                 // EXCEPTION: OCX Property Pages should return FALSE
}

BOOL CSpecLiftingModulusOfRupturePropertyPage::OnSetActive()
{
   // Disable controls if hauling not enabled
   CSpecPropertySheet* pDad = (CSpecPropertySheet*)GetParent();

   if (lrfdVersionMgr::SeventhEditionWith2016Interims <= pDad->m_Entry.GetSpecificationType())
   {
      GetDlgItem(IDC_SLWC_FR_TXT)->SetWindowText(_T("Lightweight concrete"));
      GetDlgItem(IDC_ALWC_FR_TXT)->ShowWindow(SW_HIDE);
      GetDlgItem(IDC_ALWC_FR)->ShowWindow(SW_HIDE);
      GetDlgItem(IDC_ALWC_FR_UNIT)->ShowWindow(SW_HIDE);
   }
   else
   {
      GetDlgItem(IDC_SLWC_FR_TXT)->SetWindowText(_T("Sand lightweight concrete"));
      GetDlgItem(IDC_ALWC_FR_TXT)->ShowWindow(SW_SHOW);
      GetDlgItem(IDC_ALWC_FR)->ShowWindow(SW_SHOW);
      GetDlgItem(IDC_ALWC_FR_UNIT)->ShowWindow(SW_SHOW);
   }

   return __super::OnSetActive();
}

void CSpecLiftingModulusOfRupturePropertyPage::OnHelp()
{
#pragma Reminder("WORKING HERE - Dialogs - Need to update help")
   EAFHelp(EAFGetDocument()->GetDocumentationSetName(), IDH_PROJECT_CRITERIA_GENERAL);
}
