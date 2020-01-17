///////////////////////////////////////////////////////////////////////
// PGSuper - Prestressed Girder SUPERstructure Design and Analysis
// Copyright � 1999-2020  Washington State Department of Transportation
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

// SpecShearPage.cpp : implementation file
//

#include "stdafx.h"
#include "psglib\psglib.h"
#include "SpecShearPage.h"
#include "SpecMainSheet.h"
#include <EAF\EAFDocument.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CSpecShearPage property page

IMPLEMENT_DYNCREATE(CSpecShearPage, CPropertyPage)

CSpecShearPage::CSpecShearPage() : CPropertyPage(CSpecShearPage::IDD)
{
	//{{AFX_DATA_INIT(CSpecShearPage)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}

CSpecShearPage::~CSpecShearPage()
{
}

void CSpecShearPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CSpecBridgeSitePage)
	//}}AFX_DATA_MAP

   CSpecMainSheet* pDad = (CSpecMainSheet*)GetParent();
   // dad is a friend of the entry. use him to transfer data.
   pDad->ExchangeShearCapacityData(pDX);
}


BEGIN_MESSAGE_MAP(CSpecShearPage, CPropertyPage)
	//{{AFX_MSG_MAP(CSpecShearPage)
	//}}AFX_MSG_MAP
	ON_BN_CLICKED(ID_HELP,OnHelp)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CSpecShearPage message handlers

void CSpecShearPage::OnHelp()
{
   EAFHelp( EAFGetDocument()->GetDocumentationSetName(), IDH_PROJECT_CRITERIA_SHEAR_CAPACITY );
}

BOOL CSpecShearPage::OnInitDialog() 
{
   FillShearMethodList();

   CComboBox* pCB = (CComboBox*)GetDlgItem(IDC_SHEAR_FLOW_METHOD);
   pCB->AddString(_T("using the LRFD simplified method: vui = Vu/(Wtf dv)"));
   pCB->AddString(_T("using the classical shear flow method: vui = (Vu Q)/(I Wtf)"));

	CPropertyPage::OnInitDialog();
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

BOOL CSpecShearPage::OnSetActive() 
{
   FillShearMethodList();

   CSpecMainSheet* pDad = (CSpecMainSheet*)GetParent();
   if ( lrfdVersionMgr::SeventhEditionWith2016Interims <= pDad->m_Entry.GetSpecificationType() )
   {
      GetDlgItem(IDC_SLWC_FR_LABEL)->SetWindowText(_T("Lightweight concrete"));
      GetDlgItem(IDC_ALWC_FR_LABEL)->ShowWindow(SW_HIDE);
      GetDlgItem(IDC_ALWC_FR)->ShowWindow(SW_HIDE);
      GetDlgItem(IDC_ALWC_FR_UNIT)->ShowWindow(SW_HIDE);
   }
   else
   {
      GetDlgItem(IDC_SLWC_FR_LABEL)->SetWindowText(_T("Sand lightweight concrete"));
      GetDlgItem(IDC_ALWC_FR_LABEL)->ShowWindow(SW_SHOW);
      GetDlgItem(IDC_ALWC_FR)->ShowWindow(SW_SHOW);
      GetDlgItem(IDC_ALWC_FR_UNIT)->ShowWindow(SW_SHOW);
   }

   // phi factors for deboned sections
   if ( lrfdVersionMgr::EighthEdition2017 <= pDad->m_Entry.GetSpecificationType() )
   {
      GetDlgItem(IDC_STATIC_PHI_DEBOND)->ShowWindow(SW_SHOW);
      GetDlgItem(IDC_NWC_PHI_DEBOND)->ShowWindow(SW_SHOW);
      GetDlgItem(IDC_LWC_PHI_DEBOND)->ShowWindow(SW_SHOW);
   }
   else
   {
      GetDlgItem(IDC_STATIC_PHIDEBOND)->ShowWindow(SW_HIDE);
      GetDlgItem(IDC_NWC_PHI_DEBOND)->ShowWindow(SW_HIDE);
      GetDlgItem(IDC_LWC_PHI_DEBOND)->ShowWindow(SW_HIDE);
   }

   // 2017 crosswalk chapter 5 reorg
   GetDlgItem(IDC_SCLOSURE)->SetWindowText(CString(_T("Closure Joint (LRFD 5.5.4.2, ")) +  pDad->LrfdCw8th(_T("5.14.1.3.2d"),_T("5.12.3.4.2d")) + _T(")"));
   GetDlgItem(IDC_SLTSPACING)->SetWindowText(CString(_T("LRFD Eq ")) +  pDad->LrfdCw8th(_T("5.8.2.7-1"),_T("5.7.2.6-1")));
   GetDlgItem(IDC_SGTSPACING)->SetWindowText(CString(_T("LRFD Eq ")) +  pDad->LrfdCw8th(_T("5.8.2.7-2"),_T("5.7.2.6-2")));
   GetDlgItem(IDC_SHIS)->SetWindowText(CString(_T("LRFD ")) + pDad->LrfdCw8th(_T("5.8.4.2"),_T("5.7.4.5")) + _T(" Spacing of interface shear connectors shall not exceed"));

   return CPropertyPage::OnSetActive();
}

void CSpecShearPage::FillShearMethodList()
{
   CComboBox* pCB = (CComboBox*)GetDlgItem(IDC_SHEAR_CAPACITY);
   int cur_sel = pCB->GetCurSel();
   ShearCapacityMethod shear_capacity_method = scmBTEquations;
   
   if ( cur_sel != CB_ERR )
      shear_capacity_method = (ShearCapacityMethod)pCB->GetItemData(cur_sel);

   pCB->ResetContent();

   CSpecMainSheet* pDad = (CSpecMainSheet*)GetParent();


   lrfdVersionMgr::Version version = pDad->GetSpecVersion();

   int idxGeneral, idxVciVcw, idxAppendixB, idxWSDOT2001, idxWSDOT2007;

   idxGeneral = pCB->AddString(CString(_T("Compute in accordance with LRFD ")) +  CString(pDad->LrfdCw8th(_T("5.8.3.4.2"),_T("5.7.3.4.2"))) + CString(_T(" (General method)")));
   if ( version <= lrfdVersionMgr::FourthEdition2007 )
      pCB->SetItemData(idxGeneral,(DWORD)scmBTTables); // 4th Edition and earlier, general method is Beta-Theta tables
   else
      pCB->SetItemData(idxGeneral,(DWORD)scmBTEquations); // After 4th Edition, general method is Beta-Theta equations

   if ( lrfdVersionMgr::FourthEdition2007 <= version  && lrfdVersionMgr::EighthEdition2017 > version)
   {
      idxVciVcw = pCB->AddString(_T("Compute in accordance with LRFD 5.8.3.4.3 (Vci and Vcw method)"));
      pCB->SetItemData(idxVciVcw,(DWORD)scmVciVcw);
   }

   if ( lrfdVersionMgr::FourthEditionWith2008Interims <= version )
   {
      idxAppendixB = pCB->AddString(_T("Compute in accordance with LRFD B5.1 (Beta-Theta Tables)"));
      pCB->SetItemData(idxAppendixB,(DWORD)scmBTTables);
   }

   if ( lrfdVersionMgr::SecondEditionWith2000Interims <= version )
   {
      idxWSDOT2001 = pCB->AddString(_T("Compute in accordance with WSDOT Bridge Design Manual (June 2001)"));
      pCB->SetItemData(idxWSDOT2001,(DWORD)scmWSDOT2001);
   }

   if ( lrfdVersionMgr::FourthEdition2007 <= version && version < lrfdVersionMgr::FourthEditionWith2008Interims )
   {
      idxWSDOT2007 = pCB->AddString(_T("Compute in accordance with WSDOT Bridge Design Manual (August 2007)"));
      pCB->SetItemData(idxWSDOT2007,(DWORD)scmWSDOT2007);
   }

   // select the right item
   switch(shear_capacity_method)
   {
   case scmBTEquations:
      pCB->SetCurSel(idxGeneral); // general method
      break;

   case scmVciVcw:
      if ( lrfdVersionMgr::FourthEdition2007 <= pDad->GetSpecVersion() )
         pCB->SetCurSel(idxVciVcw); // 4th and later, select Vci Vcw
      else
         pCB->SetCurSel(idxGeneral); // otherwise, select general
      break;

   case scmBTTables:
      if ( pDad->GetSpecVersion() <= lrfdVersionMgr::FourthEdition2007 )
         pCB->SetCurSel(idxGeneral); // 4th and earlier, tables is the general method
      else
         pCB->SetCurSel(idxAppendixB); // after 4th, this is Appendix B5.1
      break;

   case scmWSDOT2001:
      if ( lrfdVersionMgr::SecondEditionWith2000Interims <= pDad->GetSpecVersion() )
      {
         pCB->SetCurSel(idxWSDOT2001); // 2nd + 2000 and later, use 2001 BDM
      }
      else
      {
         pCB->SetCurSel(idxGeneral); // before 2nd+2000, use general method
      }
      break;

   case scmWSDOT2007:
      if ( lrfdVersionMgr::FourthEdition2007 <= pDad->GetSpecVersion() )
      {
         pCB->SetCurSel(idxWSDOT2007); // 4th and later, 2007 method ok
      }
      else if ( lrfdVersionMgr::SecondEditionWith2000Interims <= pDad->GetSpecVersion() )
      {
         pCB->SetCurSel(idxWSDOT2001); // 2nd + 2000 and later (but not 4th and later), use 2001 BDM
      }
      else
      {
         pCB->SetCurSel(idxGeneral); // before 2nd+2000, use general method
      }
      break;
   }

   if ( lrfdVersionMgr::SecondEditionWith2000Interims <= pDad->GetSpecVersion() )
   {
      GetDlgItem(IDC_SPACING_LABEL_1)->SetWindowText(_T("If vu <  0.125f'c, then: Smax ="));
      GetDlgItem(IDC_SPACING_LABEL_2)->SetWindowText(_T("If vu >= 0.125f'c, then: Smax ="));
   }
   else
   {
      GetDlgItem(IDC_SPACING_LABEL_1)->SetWindowText(_T("Vu < 0.1*f'c*bv*dv Smax = "));
      GetDlgItem(IDC_SPACING_LABEL_2)->SetWindowText(_T("Vu >= 0.1*f'c*bv*dv Smax = "));
   }


   // this text only applies for 7th Edition, 2014 and later
   if ( lrfdVersionMgr::SeventhEdition2014 <= pDad->GetSpecVersion() )
   {
      GetDlgItem(IDC_DEPTH_OF_UNIT)->ShowWindow(SW_SHOW);
   }
   else
   {
      GetDlgItem(IDC_DEPTH_OF_UNIT)->ShowWindow(SW_HIDE);
   }
}
