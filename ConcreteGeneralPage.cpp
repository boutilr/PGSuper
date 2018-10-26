///////////////////////////////////////////////////////////////////////
// PGSuper - Prestressed Girder SUPERstructure Design and Analysis
// Copyright � 1999-2013  Washington State Department of Transportation
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

// ConcreteGeneralPage.cpp : implementation file
//

#include "PGSuperAppPlugin\stdafx.h"
#include "PGSuperAppPlugin\resource.h"
#include "PGSuperDoc.h"
#include "PGSuperUnits.h"
#include "ConcreteDetailsDlg.h"
#include "ConcreteGeneralPage.h"
#include "HtmlHelp\HelpTopics.hh"

#include <System\Tokenizer.h>
#include "CopyConcreteEntry.h"
#include <Lrfd\Lrfd.h>
#include <EAF\EAFDisplayUnits.h>
#include <IFace\Bridge.h>

#include <PGSuperColors.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CConcreteGeneralPage dialog


CConcreteGeneralPage::CConcreteGeneralPage() : CPropertyPage(CConcreteGeneralPage::IDD)
{
	//{{AFX_DATA_INIT(CConcreteGeneralPage)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
   EAFGetBroker(&m_pBroker);
   GET_IFACE(IMaterials,pMaterial);
   m_MinNWCDensity = pMaterial->GetNWCDensityLimit();
   m_MaxLWCDensity = pMaterial->GetLWCDensityLimit();
}


void CConcreteGeneralPage::DoDataExchange(CDataExchange* pDX)
{
   m_bErrorInDDX = false;
   try
   {
	   CPropertyPage::DoDataExchange(pDX);
	   //{{AFX_DATA_MAP(CConcreteDetailsDlg)
	   DDX_Control(pDX, IDC_DS_UNIT, m_ctrlStrengthDensityUnit);
	   DDX_Control(pDX, IDC_DS_TITLE, m_ctrlStrengthDensityTitle);
	   //}}AFX_DATA_MAP

      DDX_CBItemData(pDX, IDC_CONCRETE_TYPE, m_Type);

	   DDX_Control(pDX, IDC_EC,      m_ctrlEc);
	   DDX_Control(pDX, IDC_MOD_E,   m_ctrlEcCheck);
	   DDX_Control(pDX, IDC_FC,      m_ctrlFc);
	   DDX_Control(pDX, IDC_DS,      m_ctrlStrengthDensity);

      GET_IFACE(IEAFDisplayUnits,pDisplayUnits);

      DDX_UnitValueAndTag(pDX, IDC_FC, IDC_FC_UNIT, m_Fc, pDisplayUnits->GetStressUnit() );
      DDV_UnitValueGreaterThanZero(pDX,IDC_FC, m_Fc, pDisplayUnits->GetStressUnit() );

      DDX_Check_Bool(pDX, IDC_MOD_E, m_bUserEc);
      if (m_bUserEc || !pDX->m_bSaveAndValidate)
      {
         DDX_UnitValueAndTag(pDX, IDC_EC, IDC_EC_UNIT, m_Ec, pDisplayUnits->GetModEUnit() );
         DDV_UnitValueGreaterThanZero(pDX, IDC_EC, m_Ec, pDisplayUnits->GetModEUnit() );
      }

      DDX_UnitValueAndTag(pDX, IDC_DS, IDC_DS_UNIT, m_Ds, pDisplayUnits->GetDensityUnit() );
      DDV_UnitValueGreaterThanZero(pDX, IDC_DS, m_Ds, pDisplayUnits->GetDensityUnit() );

      DDX_UnitValueAndTag(pDX, IDC_DW, IDC_DW_UNIT, m_Dw, pDisplayUnits->GetDensityUnit() );
      DDV_UnitValueGreaterThanZero(pDX, IDC_DW, m_Dw, pDisplayUnits->GetDensityUnit() );

      // Ds <= Dw
      DDV_UnitValueLimitOrMore(pDX, IDC_DW, m_Dw, m_Ds, pDisplayUnits->GetDensityUnit() );

      DDX_UnitValueAndTag(pDX, IDC_AGG_SIZE, IDC_AGG_SIZE_UNIT, m_AggSize, pDisplayUnits->GetComponentDimUnit() );
      DDV_UnitValueGreaterThanZero(pDX, IDC_AGG_SIZE, m_AggSize, pDisplayUnits->GetComponentDimUnit() );
      
      if ( pDX->m_bSaveAndValidate && m_ctrlEcCheck.GetCheck() == 1 )
      {
         m_ctrlEc.GetWindowText(m_strUserEc);
      }

      if (!pDX->m_bSaveAndValidate)
      {
         ShowStrengthDensity(!m_bUserEc);
      }
   }
   catch(...)
   {
      m_bErrorInDDX = true;
      throw;
   }
}


BEGIN_MESSAGE_MAP(CConcreteGeneralPage, CPropertyPage)
	//{{AFX_MSG_MAP(CConcreteGeneralPage)
	ON_BN_CLICKED(ID_HELP, OnHelp)
	ON_BN_CLICKED(IDC_MOD_E, OnUserEc)
	ON_EN_CHANGE(IDC_FC, OnChangeFc)
	ON_EN_CHANGE(IDC_DS, OnChangeDs)
	ON_BN_CLICKED(IDC_COPY_MATERIAL, OnCopyMaterial)
   ON_CBN_SELCHANGE(IDC_CONCRETE_TYPE,OnConcreteType)
	//}}AFX_MSG_MAP
   ON_WM_CTLCOLOR()
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CConcreteDetailsDlg message handlers

BOOL CConcreteGeneralPage::OnInitDialog() 
{
   CComboBox* pcbConcreteType = (CComboBox*)GetDlgItem(IDC_CONCRETE_TYPE);
   int idx = pcbConcreteType->AddString(_T("Normal weight"));
   pcbConcreteType->SetItemData(idx,(DWORD_PTR)pgsTypes::Normal);

   idx = pcbConcreteType->AddString(_T("All lightweight"));
   pcbConcreteType->SetItemData(idx,(DWORD_PTR)pgsTypes::AllLightweight);

   idx = pcbConcreteType->AddString(_T("Sand lightweight"));
   pcbConcreteType->SetItemData(idx,(DWORD_PTR)pgsTypes::SandLightweight);

	CPropertyPage::OnInitDialog();
	
   BOOL bEnable = m_ctrlEcCheck.GetCheck();
   GetDlgItem(IDC_EC)->EnableWindow(bEnable);
   GetDlgItem(IDC_EC_UNIT)->EnableWindow(bEnable);

   OnConcreteType();

   return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CConcreteGeneralPage::OnHelp() 
{
   ::HtmlHelp( *this, AfxGetApp()->m_pszHelpFilePath, HH_HELP_CONTEXT, IDH_CONCRETE_DETAILS );
}

void CConcreteGeneralPage::OnUserEc()
{
   // Ec check box was clicked
   BOOL bIsChecked = m_ctrlEcCheck.GetCheck();

   if (bIsChecked == FALSE)
   {
      // unchecked... compute Ec based on parameters
      m_ctrlEc.GetWindowText(m_strUserEc);
      UpdateEc();
      ShowStrengthDensity(true);
   }
   else
   {
      // check, restore user input value
      m_ctrlEc.SetWindowText(m_strUserEc);
      ShowStrengthDensity(false);
   }

   // enable/disable the Ec input and unit controls
   GetDlgItem(IDC_EC)->EnableWindow(bIsChecked);
   GetDlgItem(IDC_EC_UNIT)->EnableWindow(bIsChecked);
}

void CConcreteGeneralPage::ShowStrengthDensity(bool enable)
{
   int code = enable ? SW_SHOW : SW_HIDE;
   m_ctrlStrengthDensity.ShowWindow(code);
   m_ctrlStrengthDensityUnit.ShowWindow(code);
   m_ctrlStrengthDensityTitle.ShowWindow(code);
}

void CConcreteGeneralPage::OnChangeFc() 
{
   UpdateEc();
}

void CConcreteGeneralPage::OnChangeDs() 
{
   UpdateEc();
}

void CConcreteGeneralPage::UpdateEc()
{
   // update modulus
   if (m_ctrlEcCheck.GetCheck() == 0)
   {
      // need to manually parse strength and density values
      CString strFc, strDensity, strK1, strK2;
      m_ctrlFc.GetWindowText(strFc);
      m_ctrlStrengthDensity.GetWindowText(strDensity);

      CConcreteDetailsDlg* pParent = (CConcreteDetailsDlg*)GetParent();

      strK1.Format(_T("%f"),pParent->m_AASHTO.m_EccK1);
      strK2.Format(_T("%f"),pParent->m_AASHTO.m_EccK2);

      CString strEc = CConcreteDetailsDlg::UpdateEc(strFc,strDensity,strK1,strK2);
      m_ctrlEc.SetWindowText(strEc);
   }
}

void CConcreteGeneralPage::OnCopyMaterial() 
{
	CCopyConcreteEntry dlg(false, this);
   INT_PTR result = dlg.DoModal();
   if ( result < 0 )
   {
      ::AfxMessageBox(_T("The Concrete library is empty"),MB_OK);
   }
   else if ( result == IDOK )
   {
      const ConcreteLibraryEntry* entry = dlg.m_ConcreteEntry;

      if (entry != NULL)
      {
         CConcreteDetailsDlg* pParent = (CConcreteDetailsDlg*)GetParent();

         m_Fc      = entry->GetFc();
         m_Dw      = entry->GetWeightDensity();
         m_Ds      = entry->GetStrengthDensity();
         m_AggSize = entry->GetAggregateSize();
         m_bUserEc = entry->UserEc();
         m_Ec      = entry->GetEc();
         m_Type    = entry->GetType();

         pParent->m_AASHTO.m_EccK1       = entry->GetModEK1();
         pParent->m_AASHTO.m_EccK2       = entry->GetModEK2();
         pParent->m_AASHTO.m_CreepK1     = entry->GetCreepK1();
         pParent->m_AASHTO.m_CreepK2     = entry->GetCreepK2();
         pParent->m_AASHTO.m_ShrinkageK1 = entry->GetShrinkageK1();
         pParent->m_AASHTO.m_ShrinkageK2 = entry->GetShrinkageK2();
         pParent->m_AASHTO.m_Fct         = entry->GetAggSplittingStrength();
         pParent->m_AASHTO.m_bHasFct     = entry->HasAggSplittingStrength();

         pParent->m_ACI.m_bUserParameters = entry->UserACIParameters();
         pParent->m_ACI.m_A               = entry->GetAlpha();
         pParent->m_ACI.m_B               = entry->GetBeta();
         pParent->m_ACI.m_CureMethod      = entry->GetCureMethod();
         pParent->m_ACI.m_CementType      = entry->GetCementType();

#pragma Reminder("UPDATE: copy CEB-FIP model data here")

         CDataExchange dx(this,FALSE);
         DoDataExchange(&dx);
         OnConcreteType();
         OnUserEc();
      }
   }
}

pgsTypes::ConcreteType CConcreteGeneralPage::GetConreteType()
{
   CComboBox* pcbConcreteType = (CComboBox*)GetDlgItem(IDC_CONCRETE_TYPE);
   pgsTypes::ConcreteType type = (pgsTypes::ConcreteType)pcbConcreteType->GetItemData(pcbConcreteType->GetCurSel());
   return type;
}

void CConcreteGeneralPage::OnConcreteType()
{
   CComboBox* pcbConcreteType = (CComboBox*)GetDlgItem(IDC_CONCRETE_TYPE);
   pgsTypes::ConcreteType type = (pgsTypes::ConcreteType)pcbConcreteType->GetItemData(pcbConcreteType->GetCurSel());

   //BOOL bEnable = (type == pgsTypes::Normal ? FALSE : TRUE);
   //GetDlgItem(IDC_HAS_AGG_STRENGTH)->EnableWindow(bEnable);
   //GetDlgItem(IDC_AGG_STRENGTH)->EnableWindow(bEnable);
   //GetDlgItem(IDC_AGG_STRENGTH_T)->EnableWindow(bEnable);

   GetDlgItem(IDC_DS)->Invalidate();
   GetDlgItem(IDC_DW)->Invalidate();

#pragma Reminder("UPDATE: need to deal with thsi on the AASHTO page")
   //if ( bEnable )
   //   OnAggSplittingStrengthClicked();
}

HBRUSH CConcreteGeneralPage::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
   GET_IFACE(IEAFDisplayUnits,pDisplayUnits);
   HBRUSH hbr = CDialog::OnCtlColor(pDC, pWnd, nCtlColor);

   if ( pWnd->GetDlgCtrlID() == IDC_DS && 0 < pWnd->GetWindowTextLength())
   {
      try
      {
         // if the user enters a number like .125, the first keystroke is ".". DDX_UnitValue calls
         // into MFC DDX_Text which calls AfxTextFloatFormat. This "." does not evaluate to a number
         // so an error message is displayed... this gets the user caught in an infinte loop of
         // pressing the OK button. The only way out is to crash the program.
         //
         // To work around this issue, we need to determine if the value in the field evaluates to
         // a number. If not, assume the density is not consistent with NWC and color the text red
         // otherwise, go on to normal processing.
	      const int TEXT_BUFFER_SIZE = 400;
	      TCHAR szBuffer[TEXT_BUFFER_SIZE];
         ::GetWindowText(GetDlgItem(IDC_DS)->GetSafeHwnd(), szBuffer, _countof(szBuffer));
		   Float64 d;
   		if (_sntscanf_s(szBuffer, _countof(szBuffer), _T("%lf"), &d) != 1)
         {
            pDC->SetTextColor( RED );
         }
         else
         {
            CDataExchange dx(this,TRUE);

            Float64 value;
            DDX_UnitValue(&dx, IDC_DS, value, pDisplayUnits->GetDensityUnit() );

            if ( !IsDensityInRange(value,GetConreteType()) )
            {
               pDC->SetTextColor( RED );
            }
         }
      }
      catch(...)
      {
      }
   }

   return hbr;
}

void CConcreteGeneralPage::OnOK()
{
   CPropertyPage::OnOK();

   if ( !m_bErrorInDDX && !IsDensityInRange(m_Ds,m_Type) )
   {
      if (m_Type == pgsTypes::Normal)
         AfxMessageBox(IDS_NWC_MESSAGE,MB_OK | MB_ICONINFORMATION);
      else
         AfxMessageBox(IDS_LWC_MESSAGE,MB_OK | MB_ICONINFORMATION);
   }
}

bool CConcreteGeneralPage::IsDensityInRange(Float64 density,pgsTypes::ConcreteType type)
{
   if ( type == pgsTypes::Normal )
   {
      return ( IsLE(m_MinNWCDensity,density) );
   }
   else
   {
      return ( IsLE(density,m_MaxLWCDensity) );
   }
}

void CConcreteGeneralPage::SetFc(Float64 fc)
{
   UpdateData(TRUE);
   m_Fc = fc;
   UpdateData(FALSE);
}
