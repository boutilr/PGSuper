// SpecGeneralSpecificationPropertyPage.cpp : implementation file
//

#include "stdafx.h"
#include "resource.h"
#include "SpecGeneralSpecificationPropertyPage.h"
#include "SpecPropertySheet.h"
#include <EAF\EAFDocument.h>

// CSpecGeneralSpecificationPropertyPage

IMPLEMENT_DYNAMIC(CSpecGeneralSpecificationPropertyPage, CMFCPropertyPage)

CSpecGeneralSpecificationPropertyPage::CSpecGeneralSpecificationPropertyPage() :
   CMFCPropertyPage(IDD_SPEC_GENERAL_SPECIFICATION)
{
}

CSpecGeneralSpecificationPropertyPage::~CSpecGeneralSpecificationPropertyPage()
{
}


BEGIN_MESSAGE_MAP(CSpecGeneralSpecificationPropertyPage, CMFCPropertyPage)
   ON_CBN_SELCHANGE(IDC_SPECIFICATION, OnSpecificationChanged)
   ON_BN_CLICKED(ID_HELP, OnHelp)
END_MESSAGE_MAP()



// CSpecGeneralSpecificationPropertyPage message handlers

void CSpecGeneralSpecificationPropertyPage::DoDataExchange(CDataExchange* pDX)
{
   __super::DoDataExchange(pDX);
   //{{AFX_DATA_MAP(CSpecDescrPage)
   //}}AFX_DATA_MAP

   CSpecPropertySheet* pParent = (CSpecPropertySheet*)GetParent();
   pParent->ExchangeGeneralSpecificationData(pDX);
}


BOOL CSpecGeneralSpecificationPropertyPage::OnInitDialog()
{
   CComboBox* pSpec = (CComboBox*)GetDlgItem(IDC_SPECIFICATION);
   int idx;
   for (int i = 1; i < (int)lrfdVersionMgr::LastVersion; i++)
   {
      idx = pSpec->AddString(lrfdVersionMgr::GetVersionString((lrfdVersionMgr::Version)(i)));
      pSpec->SetItemData(idx, (DWORD)(i));
   }

   CSpecPropertySheet* pParent = (CSpecPropertySheet*)GetParent();
   lrfdVersionMgr::Version version = pParent->GetSpecVersion();

   __super::OnInitDialog();

   // enable/disable si setting for before 2007
   OnSpecificationChanged();

   return TRUE;  // return TRUE unless you set the focus to a control
                 // EXCEPTION: OCX Property Pages should return FALSE
}

lrfdVersionMgr::Version CSpecGeneralSpecificationPropertyPage::GetSpecVersion()
{
   CComboBox* pSpec = (CComboBox*)GetDlgItem(IDC_SPECIFICATION);
   int idx = pSpec->GetCurSel();
   return (lrfdVersionMgr::Version)(pSpec->GetItemData(idx));
}

void CSpecGeneralSpecificationPropertyPage::OnSpecificationChanged()
{
   CComboBox* pSpec = (CComboBox*)GetDlgItem(IDC_SPECIFICATION);
   int idx = pSpec->GetCurSel();
   DWORD_PTR id = pSpec->GetItemData(idx);

   BOOL enable_si = TRUE;
   if (id > (DWORD)lrfdVersionMgr::ThirdEditionWith2006Interims)
   {
      CheckRadioButton(IDC_SPEC_UNITS_SI, IDC_SPEC_UNITS_US, IDC_SPEC_UNITS_US);
      enable_si = FALSE;
   }

   CButton* pSi = (CButton*)GetDlgItem(IDC_SPEC_UNITS_SI);
   pSi->EnableWindow(enable_si);

   // Vci/Vcw method was removed from spec in 2017
   CSpecPropertySheet* pParent = (CSpecPropertySheet*)GetParent();
   lrfdVersionMgr::Version version = pParent->GetSpecVersion();

   if (version >= lrfdVersionMgr::EighthEdition2017)
   {
      ShearCapacityMethod method = pParent->m_Entry.GetShearCapacityMethod();
      if (method == scmVciVcw)
      {
         ::AfxMessageBox(_T("The Vci/Vcw method is currently selected for computing shear capacity, and this method was removed from the LRFD Bridge Design Specifications in the 8th Edition, 2017. The shear capacity method will be changed to compute in accordance with the General Method per LRFD 5.7.3.5.\nVisit the Shear Capacity tab for more options."), MB_OK | MB_ICONWARNING);
         pParent->m_Entry.SetShearCapacityMethod(scmBTEquations);
      }
   }
}

void CSpecGeneralSpecificationPropertyPage::OnHelp()
{
#pragma Reminder("WORKING HERE - Dialogs - Need to update help")
   EAFHelp(EAFGetDocument()->GetDocumentationSetName(), IDH_PROJECT_CRITERIA_GENERAL);
}
