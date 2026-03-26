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

// PierLayoutPage.cpp : implementation file
//

#include "stdafx.h"
#include "PGSuperDoc.h"
#include "PierLayoutPage.h"
#include "PierDetailsDlg.h"
#include <PgsExt\ConcreteDetailsDlg.h>

#include <IFace/Tools.h>
#include <EAF\EAFDisplayUnits.h>
#include <MFCTools\CustomDDX.h>





/////////////////////////////////////////////////////////////////////////////
// CPierLayoutPage property page

IMPLEMENT_DYNCREATE(CPierLayoutPage, CPropertyPage)

CPierLayoutPage::CPierLayoutPage() : CPropertyPage(CPierLayoutPage::IDD)
{
	//{{AFX_DATA_INIT(CPierLayoutPage)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
   
   
}

CPierLayoutPage::~CPierLayoutPage()
{
}

void CPierLayoutPage::Init(CPierData2* pPier)
{
   m_pPier = pPier;

   m_PierIdx = pPier->GetIndex();
}

void CPierLayoutPage::DoDataExchange(CDataExchange* pDX)
{
     CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CPierConnectionsPage)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
	DDX_Control(pDX, IDC_EC,           m_ctrlEc);
	DDX_Control(pDX, IDC_EC_LABEL,     m_ctrlEcCheck);
	DDX_Control(pDX, IDC_FC,           m_ctrlFc);
   

   
   auto pBroker = EAFGetBroker();
   GET_IFACE2(pBroker,IEAFDisplayUnits,pDisplayUnits);

   // Pier Model
   DDX_CBItemData(pDX,IDC_PIER_MODEL_TYPE,m_PierModelType);

   CConcreteMaterial& concrete = m_pPier->GetConcrete();
   DDX_UnitValueAndTag(pDX,IDC_FC,IDC_FC_UNIT,concrete.Fc,pDisplayUnits->GetStressUnit());
   DDX_UnitValueAndTag(pDX,IDC_EC,IDC_EC_UNIT,concrete.Ec,pDisplayUnits->GetModEUnit());
   DDX_Check_Bool(pDX,IDC_EC_LABEL,concrete.bUserEc);



   if ( pDX->m_bSaveAndValidate )
   {
      // all of the data has been extracted from the dialog controls and it has been validated
      // set the values on the actual pier object
      m_pPier->SetPierModelType(m_PierModelType);

   }
}

BEGIN_MESSAGE_MAP(CPierLayoutPage, CPropertyPage)
	//{{AFX_MSG_MAP(CPierLayoutPage)
   ON_BN_CLICKED(IDC_EC_LABEL,OnUserEc)
	ON_EN_CHANGE(IDC_FC, OnChangeFc)
   ON_BN_CLICKED(IDC_MORE_PROPERTIES, OnMoreProperties)
   ON_CBN_SELCHANGE(IDC_PIER_MODEL_TYPE, OnPierModelTypeChanged)
	ON_COMMAND(ID_HELP, OnHelp)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CPierLayoutPage message handlers

BOOL CPierLayoutPage::OnInitDialog() 
{


   m_PierModelType = m_pPier->GetPierModelType();


   FillPierModelTypeComboBox();

   CWnd* pBox = GetDlgItem(IDC_STATIC_BOUNDS);
   pBox->ShowWindow(SW_HIDE);

   CRect boxRect;
   pBox->GetWindowRect(&boxRect);
   ScreenToClient(boxRect);

   VERIFY(m_CommonPierLayoutDlg.Create(IDD_PIER_LAYOUT_COMMON, this));
   m_CommonPierLayoutDlg.SetPierModelType(m_PierModelType);
   VERIFY(m_CommonPierLayoutDlg.SetWindowPos(GetDlgItem(IDC_STATIC_BOUNDS), boxRect.left, boxRect.top, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE));//|SWP_NOMOVE));

   CPropertyPage::OnInitDialog();

   UpdateConcreteTypeLabel();
   if ( m_strUserEc == _T("") )
   {
      m_ctrlEc.GetWindowText(m_strUserEc);
   }
   OnUserEc();

   OnPierModelTypeChanged();


   return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CPierLayoutPage::UpdateConcreteTypeLabel()
{
   CString strLabel;
   if ( WBFL::LRFD::BDSManager::GetEdition() < WBFL::LRFD::BDSManager::Edition::SeventhEditionWith2016Interims )
   {  
      switch( m_pPier->GetConcrete().Type )
      {
      case pgsTypes::Normal:
         strLabel = _T("Normal Weight Concrete");
         break;

      case pgsTypes::AllLightweight:
         strLabel = _T("All Lightweight Concrete");
         break;

      case pgsTypes::SandLightweight:
         strLabel = _T("Sand Lightweight Concrete");
         break;

      case pgsTypes::PCI_UHPC: // UI should prevent UHPC for piers
      case pgsTypes::UHPC:
      default:
         ATLASSERT(false); // should never get here
         strLabel = _T("Concrete Type Label Error");
      }
   }
   else
   {
      // LRFD 2016 and later there is only normal and lightweight concrete. We
      //use the SandLightweight enum to mean "lightweight"
      ATLASSERT( m_pPier->GetConcrete().Type == pgsTypes::Normal || m_pPier->GetConcrete().Type == pgsTypes::SandLightweight );
      switch( m_pPier->GetConcrete().Type )
      {
      case pgsTypes::Normal:
         strLabel = _T("Normal Weight Concrete");
         break;

      case pgsTypes::SandLightweight:
         strLabel = _T("Lightweight Concrete");
         break;

      case pgsTypes::PCI_UHPC: // UI should prevent UHPC for piers
      case pgsTypes::UHPC:
      default:
         ATLASSERT(false); // should never get here
         strLabel = _T("Concrete Type Label Error");
      }
   }

   GetDlgItem(IDC_CONCRETE_TYPE_LABEL)->SetWindowText(strLabel);
}



void CPierLayoutPage::FillPierModelTypeComboBox()
{
   CComboBox* pcbPierModel = (CComboBox*)GetDlgItem(IDC_PIER_MODEL_TYPE);
   pcbPierModel->ResetContent();

   int idx = pcbPierModel->AddString(_T("Idealized"));
   pcbPierModel->SetItemData(idx,(DWORD_PTR)pgsTypes::pmtIdealized);

   idx = pcbPierModel->AddString(_T("Physical"));
   pcbPierModel->SetItemData(idx,(DWORD_PTR)pgsTypes::pmtPhysical);
}

void CPierLayoutPage::OnHelp() 
{
   EAFHelp( EAFGetDocument()->GetDocumentationSetName(), IDH_PIERDETAILS_LAYOUT );
}

void CPierLayoutPage::OnUserEc()
{
   BOOL bEnable = m_ctrlEcCheck.GetCheck();

   GetDlgItem(IDC_EC_LABEL)->EnableWindow(TRUE);

   if (bEnable==FALSE)
   {
      m_ctrlEc.GetWindowText(m_strUserEc);
      UpdateEc();
   }
   else
   {
      m_ctrlEc.SetWindowText(m_strUserEc);
   }

   GetDlgItem(IDC_EC)->EnableWindow(bEnable);
   GetDlgItem(IDC_EC_UNIT)->EnableWindow(bEnable);
}

void CPierLayoutPage::OnMoreProperties()
{
   UpdateData(TRUE);
   CConcreteDetailsDlg dlg(true/*f'c*/,false/*no uhpc*/, false/*don't enable Compute Time Parameters option*/);
   CConcreteMaterial& concrete = m_pPier->GetConcrete();

   dlg.m_fc28 = concrete.Fc;
   dlg.m_Ec28 = concrete.Ec;
   dlg.m_bUserEc28 = concrete.bUserEc;

   dlg.m_General.m_Type    = concrete.Type;
   dlg.m_General.m_AggSize = concrete.MaxAggregateSize;
   dlg.m_General.m_Ds      = concrete.StrengthDensity;
   dlg.m_General.m_Dw      = concrete.WeightDensity;
   dlg.m_General.m_strUserEc  = m_strUserEc;

   dlg.m_AASHTO.m_EccK1       = concrete.EcK1;
   dlg.m_AASHTO.m_EccK2       = concrete.EcK2;
   dlg.m_AASHTO.m_CreepK1     = concrete.CreepK1;
   dlg.m_AASHTO.m_CreepK2     = concrete.CreepK2;
   dlg.m_AASHTO.m_ShrinkageK1 = concrete.ShrinkageK1;
   dlg.m_AASHTO.m_ShrinkageK2 = concrete.ShrinkageK2;
   dlg.m_AASHTO.m_bHasFct     = concrete.bHasFct;
   dlg.m_AASHTO.m_Fct         = concrete.Fct;

   if ( dlg.DoModal() == IDOK )
   {
      concrete.Fc               = dlg.m_fc28;
      concrete.Ec               = dlg.m_Ec28;
      concrete.bUserEc          = dlg.m_bUserEc28;

      concrete.Type             = dlg.m_General.m_Type;
      concrete.MaxAggregateSize = dlg.m_General.m_AggSize;
      concrete.StrengthDensity  = dlg.m_General.m_Ds;
      concrete.WeightDensity    = dlg.m_General.m_Dw;
      concrete.EcK1             = dlg.m_AASHTO.m_EccK1;
      concrete.EcK2             = dlg.m_AASHTO.m_EccK2;
      concrete.CreepK1          = dlg.m_AASHTO.m_CreepK1;
      concrete.CreepK2          = dlg.m_AASHTO.m_CreepK2;
      concrete.ShrinkageK1      = dlg.m_AASHTO.m_ShrinkageK1;
      concrete.ShrinkageK2      = dlg.m_AASHTO.m_ShrinkageK2;
      concrete.bHasFct          = dlg.m_AASHTO.m_bHasFct;
      concrete.Fct              = dlg.m_AASHTO.m_Fct;

      m_strUserEc  = dlg.m_General.m_strUserEc;
      m_ctrlEc.SetWindowText(m_strUserEc);

      UpdateData(FALSE);
      OnUserEc();
      UpdateConcreteTypeLabel();
   }
}

void CPierLayoutPage::OnChangeFc() 
{
   UpdateEc();
}

void CPierLayoutPage::UpdateEc()
{
   // update modulus
   if (m_ctrlEcCheck.GetCheck() == 0)
   {
      // blank out ec
      CString strEc;
      m_ctrlEc.SetWindowText(strEc);

      // need to manually parse strength and density values
      CString strFc, strDensity, strK1, strK2;
      m_ctrlFc.GetWindowText(strFc);

      
      auto pBroker = EAFGetBroker();
      GET_IFACE2(pBroker,IEAFDisplayUnits,pDisplayUnits);

      strDensity.Format(_T("%s"),FormatDimension(m_pPier->GetConcrete().StrengthDensity,pDisplayUnits->GetDensityUnit(),false));
      strK1.Format(_T("%f"),m_pPier->GetConcrete().EcK1);
      strK2.Format(_T("%f"),m_pPier->GetConcrete().EcK2);

      strEc = CConcreteDetailsDlg::UpdateEc(m_pPier->GetConcrete().Type,strFc,strDensity,strK1,strK2);
      m_ctrlEc.SetWindowText(strEc);
   }
}

void CPierLayoutPage::OnPierModelTypeChanged()
{
   CComboBox* pcbPierModel = (CComboBox*)GetDlgItem(IDC_PIER_MODEL_TYPE);
   int curSel = pcbPierModel->GetCurSel();
   m_PierModelType = (pgsTypes::PierModelType)pcbPierModel->GetItemData(curSel);
   
   int nShow = (m_PierModelType == pgsTypes::pmtIdealized ? SW_HIDE : SW_SHOW);

   // enable/disable all the controls, except pier model type selector
   CWnd* pWnd = pcbPierModel->GetNextWindow(GW_HWNDNEXT);
   while ( pWnd )
   {
      int nID = pWnd->GetDlgCtrlID();
      if ( nID != IDC_PIER_MODEL_LABEL && nID != IDC_PIER_MODEL_TYPE )
      {
         if ( nID == IDC_EC_LABEL )
         {
            m_ctrlEcCheck.ShowWindow(nShow);
         }
         else
         {
            pWnd->ShowWindow(nShow);
         }
      }
      pWnd = pWnd->GetNextWindow(GW_HWNDNEXT);
   }
}


