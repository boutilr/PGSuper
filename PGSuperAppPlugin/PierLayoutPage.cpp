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
   DDX_CBEnum(pDX, IDC_PIER_LAYOUT_TYPE, m_PierLayoutType);

   CConcreteMaterial& concrete = m_pPier->GetConcrete();
   DDX_UnitValueAndTag(pDX,IDC_FC,IDC_FC_UNIT,concrete.Fc,pDisplayUnits->GetStressUnit());
   DDX_UnitValueAndTag(pDX,IDC_EC,IDC_EC_UNIT,concrete.Ec,pDisplayUnits->GetModEUnit());
   DDX_Check_Bool(pDX,IDC_EC_LABEL,concrete.bUserEc);

   if ( pDX->m_bSaveAndValidate )
   {
      // all of the data has been extracted from the dialog controls and it has been validated
      // set the values on the actual pier object
      m_pPier->SetPierModelType(m_PierModelType);
      m_pPier->SetPierLayoutType(m_PierLayoutType);

      if (m_PierModelType == pgsTypes::pmtPhysical)
      {
          if (m_PierLayoutType == pgsTypes::pltCommon)
          {
              if (!m_CommonPierLayoutDlg.UpdateData(TRUE))
              {
                  pDX->Fail();
                  return;
              }
              else
              {

                  m_pPier->SetTransverseOffset(m_CommonPierLayoutDlg.m_RefColumnIdx, m_CommonPierLayoutDlg.m_TransverseOffset, m_CommonPierLayoutDlg.m_TransverseOffsetMeasurement);
                  m_pPier->SetXBeamWidth(m_CommonPierLayoutDlg.m_XBeamWidth);

                  for (int i = 0; i < 2; i++)
                  {
                      pgsTypes::SideType side = (pgsTypes::SideType)i;
                      m_pPier->SetXBeamDimensions(side, m_CommonPierLayoutDlg.m_XBeamHeight[side], m_CommonPierLayoutDlg.m_XBeamTaperHeight[side], m_CommonPierLayoutDlg.m_XBeamTaperLength[side], m_CommonPierLayoutDlg.m_XBeamEndSlopeOffset[side]);
                      m_pPier->SetXBeamOverhang(side, m_CommonPierLayoutDlg.m_XBeamOverhang[side]);
                  }

                  m_pPier->SetXBeamWidth(m_CommonPierLayoutDlg.m_XBeamWidth);

                  m_pPier->SetColumnFixity(m_CommonPierLayoutDlg.m_ColumnFixity);

                  // XBeam width, W, must be greater than zeo
                  DDV_UnitValueGreaterThanZero(pDX, IDC_W, m_CommonPierLayoutDlg.m_XBeamWidth, pDisplayUnits->GetSpanLengthUnit());

                  // H1 and H3 must be > 0
                  DDV_UnitValueGreaterThanZero(pDX, IDC_H1, m_CommonPierLayoutDlg.m_XBeamHeight[pgsTypes::stLeft], pDisplayUnits->GetSpanLengthUnit());
                  DDV_UnitValueGreaterThanZero(pDX, IDC_H3, m_CommonPierLayoutDlg.m_XBeamHeight[pgsTypes::stRight], pDisplayUnits->GetSpanLengthUnit());

                  // X2 and X4 must be >= 0
                  DDV_UnitValueZeroOrMore(pDX, IDC_X1, m_CommonPierLayoutDlg.m_XBeamTaperLength[pgsTypes::stLeft], pDisplayUnits->GetSpanLengthUnit());
                  DDV_UnitValueZeroOrMore(pDX, IDC_X2, m_CommonPierLayoutDlg.m_XBeamEndSlopeOffset[pgsTypes::stLeft], pDisplayUnits->GetSpanLengthUnit());
                  DDV_UnitValueZeroOrMore(pDX, IDC_X3, m_CommonPierLayoutDlg.m_XBeamTaperLength[pgsTypes::stRight], pDisplayUnits->GetSpanLengthUnit());
                  DDV_UnitValueZeroOrMore(pDX, IDC_X4, m_CommonPierLayoutDlg.m_XBeamEndSlopeOffset[pgsTypes::stRight], pDisplayUnits->GetSpanLengthUnit());

                  //Left end
                  if (0 < m_CommonPierLayoutDlg.m_XBeamTaperLength[pgsTypes::stLeft])
                  {
                      // if H2 > 0, then X1 must be > 0
                      if (IsZero(m_CommonPierLayoutDlg.m_XBeamTaperHeight[pgsTypes::stLeft]))
                      {
                          pDX->PrepareCtrl(IDC_H2);
                          AfxMessageBox(_T("H2 must be greater than zero when X1 is greater than zero."));
                          pDX->Fail();
                      }
                      else if (m_CommonPierLayoutDlg.m_XBeamTaperLength[pgsTypes::stLeft] < m_CommonPierLayoutDlg.m_XBeamEndSlopeOffset[pgsTypes::stLeft])
                      {
                          pDX->PrepareCtrl(IDC_X1);
                          AfxMessageBox(_T("X1 must be greater than X2 when X1 is greater than zero."));
                          pDX->Fail();
                      }
                  }
                  else if (!IsZero(m_CommonPierLayoutDlg.m_XBeamTaperHeight[pgsTypes::stLeft]))
                  {
                      // if X1 is zero, then H2 must also be zero
                      pDX->PrepareCtrl(IDC_H2);
                      AfxMessageBox(_T("H2 must be zero when X1 is zero"));
                      pDX->Fail();
                  }

                  // Right end
                  if (0 < m_CommonPierLayoutDlg.m_XBeamTaperLength[pgsTypes::stRight])
                  {
                      // if H4 > 0, then X3 must be > 0
                      if (IsZero(m_CommonPierLayoutDlg.m_XBeamTaperHeight[pgsTypes::stRight]))
                      {
                          pDX->PrepareCtrl(IDC_H4);
                          AfxMessageBox(_T("H4 must be greater than zero when X3 is greater than zero."));
                          pDX->Fail();
                      }
                      else if (m_CommonPierLayoutDlg.m_XBeamTaperLength[pgsTypes::stRight] < m_CommonPierLayoutDlg.m_XBeamEndSlopeOffset[pgsTypes::stRight])
                      {
                          pDX->PrepareCtrl(IDC_X3);
                          AfxMessageBox(_T("X3 must be greater than X4 when X3 is greater than zero."));
                          pDX->Fail();
                      }
                  }
                  else if (!IsZero(m_CommonPierLayoutDlg.m_XBeamTaperHeight[pgsTypes::stRight]))
                  {
                      // if X3 is zero, then H4 must also be zero
                      pDX->PrepareCtrl(IDC_H4);
                      AfxMessageBox(_T("H4 must be zero when X3 is zero"));
                      pDX->Fail();
                  }

				  m_CommonPierLayoutDlg.m_ColumnLayoutGrid.GetColumnData(*m_pPier);
				  ColumnIndexType nColumns = m_pPier->GetColumnCount();

                  for (ColumnIndexType colIdx = 0; colIdx < nColumns; colIdx++)
                  {
                      CColumnData column = m_pPier->GetColumnData(colIdx);
                      column.SetColumnHeightMeasurementType(m_CommonPierLayoutDlg.m_ColumnHeightMeasurementType);
                      m_pPier->SetColumnData(colIdx, column);
                  }

                  Float64 D1, D2;
                  // X5 must be >= diameter of first column
                  m_pPier->GetColumnData(0).GetColumnDimensions(&D1, &D2);
                  DDV_UnitValueLimitOrMore(pDX, IDC_X5, m_CommonPierLayoutDlg.m_XBeamOverhang[pgsTypes::stLeft], D1 / 2, pDisplayUnits->GetSpanLengthUnit());

                  // X6 must be >= diameter of first column
                  m_pPier->GetColumnData(nColumns - 1).GetColumnDimensions(&D1, &D2);
                  DDV_UnitValueLimitOrMore(pDX, IDC_X6, m_CommonPierLayoutDlg.m_XBeamOverhang[pgsTypes::stRight], D1 / 2, pDisplayUnits->GetSpanLengthUnit());

                  // X1 + X3 must be less than X5 + X6 + Sum(S)
                  ATLASSERT(1 <= nColumns);
                  Float64 S = 0;
                  for (SpacingIndexType spaIdx = 0; spaIdx < nColumns - 1; spaIdx++)
                  {
                      S += m_pPier->GetColumnSpacing(spaIdx);
                  }
                  Float64 pierWidth = m_CommonPierLayoutDlg.m_XBeamOverhang[pgsTypes::stLeft] + m_CommonPierLayoutDlg.m_XBeamOverhang[pgsTypes::stRight] + S;
                  Float64 sumOverhangs = m_CommonPierLayoutDlg.m_XBeamTaperLength[pgsTypes::stLeft] + m_CommonPierLayoutDlg.m_XBeamTaperLength[pgsTypes::stRight];
                  if (pierWidth < sumOverhangs)
                  {
                      pDX->PrepareCtrl(IDC_X5);
                      AfxMessageBox(_T("X1 + X3 cannot exceed the overall pier width (X5 + X6 + summation of S)"));
                      pDX->Fail();
                  }
              }
          }
          else if (m_PierLayoutType == pgsTypes::pltHammerhead)
          {
              if (!m_HammerheadPierLayoutDlg.UpdateData(TRUE))
              {
                  pDX->Fail();
                  return;
              }
              else
              {

                  m_pPier->SetTransverseOffset(m_HammerheadPierLayoutDlg.m_RefColumnIdx, m_HammerheadPierLayoutDlg.m_TransverseOffset, m_HammerheadPierLayoutDlg.m_TransverseOffsetMeasurement);
                  m_pPier->SetXBeamWidth(m_CommonPierLayoutDlg.m_XBeamWidth);

                  for (int i = 0; i < 2; i++)
                  {
                      pgsTypes::SideType side = (pgsTypes::SideType)i;
                      m_pPier->SetXBeamDimensions(side, m_HammerheadPierLayoutDlg.m_XBeamHeight[side], m_HammerheadPierLayoutDlg.m_XBeamTaperHeight[side], 
                      m_HammerheadPierLayoutDlg.m_XBeamTaperLength[side], m_HammerheadPierLayoutDlg.m_XBeamEndSlopeOffset[side]);
                      m_pPier->SetXBeamOverhang(side, m_HammerheadPierLayoutDlg.m_XBeamOverhang[side]);
                  }

                  m_pPier->SetXBeamWidth(m_HammerheadPierLayoutDlg.m_XBeamWidth);

                  m_pPier->SetColumnFixity(m_HammerheadPierLayoutDlg.m_ColumnFixity);

                  // XBeam width, W, must be greater than zeo
                  DDV_UnitValueGreaterThanZero(pDX, IDC_W, m_HammerheadPierLayoutDlg.m_XBeamWidth, pDisplayUnits->GetSpanLengthUnit());

                  // H1 and H3 must be > 0
                  DDV_UnitValueGreaterThanZero(pDX, IDC_H1, m_HammerheadPierLayoutDlg.m_XBeamHeight[pgsTypes::stLeft], pDisplayUnits->GetSpanLengthUnit());
                  DDV_UnitValueGreaterThanZero(pDX, IDC_H3, m_HammerheadPierLayoutDlg.m_XBeamHeight[pgsTypes::stRight], pDisplayUnits->GetSpanLengthUnit());

                  // X2 and X4 must be >= 0
                  DDV_UnitValueZeroOrMore(pDX, IDC_X1, m_HammerheadPierLayoutDlg.m_XBeamTaperLength[pgsTypes::stLeft], pDisplayUnits->GetSpanLengthUnit());
                  DDV_UnitValueZeroOrMore(pDX, IDC_X2, m_HammerheadPierLayoutDlg.m_XBeamEndSlopeOffset[pgsTypes::stLeft], pDisplayUnits->GetSpanLengthUnit());
                  DDV_UnitValueZeroOrMore(pDX, IDC_X3, m_HammerheadPierLayoutDlg.m_XBeamTaperLength[pgsTypes::stRight], pDisplayUnits->GetSpanLengthUnit());
                  DDV_UnitValueZeroOrMore(pDX, IDC_X4, m_HammerheadPierLayoutDlg.m_XBeamEndSlopeOffset[pgsTypes::stRight], pDisplayUnits->GetSpanLengthUnit());

                  //Left end
                  if (0 < m_HammerheadPierLayoutDlg.m_XBeamTaperLength[pgsTypes::stLeft])
                  {
                      // if H2 > 0, then X1 must be > 0
                      if (IsZero(m_HammerheadPierLayoutDlg.m_XBeamTaperHeight[pgsTypes::stLeft]))
                      {
                          pDX->PrepareCtrl(IDC_H2);
                          AfxMessageBox(_T("H2 must be greater than zero when X1 is greater than zero."));
                          pDX->Fail();
                      }
                      else if (m_HammerheadPierLayoutDlg.m_XBeamTaperLength[pgsTypes::stLeft] < m_HammerheadPierLayoutDlg.m_XBeamEndSlopeOffset[pgsTypes::stLeft])
                      {
                          pDX->PrepareCtrl(IDC_X1);
                          AfxMessageBox(_T("X1 must be greater than X2 when X1 is greater than zero."));
                          pDX->Fail();
                      }
                  }
                  else if (!IsZero(m_HammerheadPierLayoutDlg.m_XBeamTaperHeight[pgsTypes::stLeft]))
                  {
                      // if X1 is zero, then H2 must also be zero
                      pDX->PrepareCtrl(IDC_H2);
                      AfxMessageBox(_T("H2 must be zero when X1 is zero"));
                      pDX->Fail();
                  }

                  // Right end
                  if (0 < m_HammerheadPierLayoutDlg.m_XBeamTaperLength[pgsTypes::stRight])
                  {
                      // if H4 > 0, then X3 must be > 0
                      if (IsZero(m_HammerheadPierLayoutDlg.m_XBeamTaperHeight[pgsTypes::stRight]))
                      {
                          pDX->PrepareCtrl(IDC_H4);
                          AfxMessageBox(_T("H4 must be greater than zero when X3 is greater than zero."));
                          pDX->Fail();
                      }
                      else if (m_HammerheadPierLayoutDlg.m_XBeamTaperLength[pgsTypes::stRight] < m_HammerheadPierLayoutDlg.m_XBeamEndSlopeOffset[pgsTypes::stRight])
                      {
                          pDX->PrepareCtrl(IDC_X3);
                          AfxMessageBox(_T("X3 must be greater than X4 when X3 is greater than zero."));
                          pDX->Fail();
                      }
                  }
                  else if (!IsZero(m_HammerheadPierLayoutDlg.m_XBeamTaperHeight[pgsTypes::stRight]))
                  {
                      // if X3 is zero, then H4 must also be zero
                      pDX->PrepareCtrl(IDC_H4);
                      AfxMessageBox(_T("H4 must be zero when X3 is zero"));
                      pDX->Fail();
                  }



                  CColumnData column = m_pPier->GetColumnData(0);
                  column.SetColumnHeightMeasurementType(m_HammerheadPierLayoutDlg.m_ColumnHeightMeasurementType);
                  
                  //DDX_CBItemData(pDX, IDC_COLUMN_SHAPE, m_HammerheadPierLayoutDlg.m_ColumnShapeType);
				  column.SetColumnShape(m_HammerheadPierLayoutDlg.m_ColumnShapeType);

                  DDV_UnitValueGreaterThanZero(pDX, IDC_COLUMN_HEIGHT_EDIT, m_HammerheadPierLayoutDlg.m_ColumnHeight, pDisplayUnits->GetSpanLengthUnit());
                  column.SetColumnHeight(m_HammerheadPierLayoutDlg.m_ColumnHeight);

                  Float64 D1, D2;
                  // X5 must be >= diameter of first column
                  m_pPier->GetColumnData(0).GetColumnDimensions(&D1, &D2);
                  DDV_UnitValueGreaterThanZero(pDX, IDC_COLUMN_WIDTH, m_HammerheadPierLayoutDlg.m_D1, pDisplayUnits->GetSpanLengthUnit());
                  DDV_UnitValueGreaterThanZero(pDX, IDC_COLUMN_DEPTH, m_HammerheadPierLayoutDlg.m_D2, pDisplayUnits->GetSpanLengthUnit());
				  column.SetColumnDimensions(m_HammerheadPierLayoutDlg.m_D1, m_HammerheadPierLayoutDlg.m_D2);

                  m_pPier->SetColumnData(0, column);

                  DDV_UnitValueLimitOrMore(pDX, IDC_X5, m_HammerheadPierLayoutDlg.m_XBeamOverhang[pgsTypes::stLeft], D1 / 2, pDisplayUnits->GetSpanLengthUnit());
                  DDV_UnitValueLimitOrMore(pDX, IDC_X6, m_HammerheadPierLayoutDlg.m_XBeamOverhang[pgsTypes::stRight], D1 / 2, pDisplayUnits->GetSpanLengthUnit());



                  Float64 pierWidth = m_HammerheadPierLayoutDlg.m_XBeamOverhang[pgsTypes::stLeft] + m_HammerheadPierLayoutDlg.m_XBeamOverhang[pgsTypes::stRight];
                  Float64 sumOverhangs = m_HammerheadPierLayoutDlg.m_XBeamTaperLength[pgsTypes::stLeft] + m_HammerheadPierLayoutDlg.m_XBeamTaperLength[pgsTypes::stRight];
                  if (pierWidth < sumOverhangs)
                  {
                      pDX->PrepareCtrl(IDC_X5);
                      AfxMessageBox(_T("X1 + X3 cannot exceed the overall pier width (X5 + X6 + summation of S)"));
                      pDX->Fail();
                  }



              }
          }
          
      }

   }
}

BEGIN_MESSAGE_MAP(CPierLayoutPage, CPropertyPage)
	//{{AFX_MSG_MAP(CPierLayoutPage)
   ON_BN_CLICKED(IDC_EC_LABEL,OnUserEc)
	ON_EN_CHANGE(IDC_FC, OnChangeFc)
   ON_BN_CLICKED(IDC_MORE_PROPERTIES, OnMoreProperties)
   ON_CBN_SELCHANGE(IDC_PIER_MODEL_TYPE, OnPierModelTypeChanged)
   ON_CBN_SELCHANGE(IDC_PIER_LAYOUT_TYPE, OnPierLayoutTypeChanged)
	ON_COMMAND(ID_HELP, OnHelp)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CPierLayoutPage message handlers

BOOL CPierLayoutPage::OnInitDialog() 
{

   m_PierModelType = m_pPier->GetPierModelType();
   m_PierLayoutType = m_pPier->GetPierLayoutType();

   FillPierModelTypeComboBox();
   FillPierLayoutTypeComboBox();

   CWnd* pBox = GetDlgItem(IDC_STATIC_BOUNDS);
   pBox->ShowWindow(SW_HIDE);

   CRect boxRect;
   pBox->GetWindowRect(&boxRect);
   ScreenToClient(boxRect);

   m_CommonPierLayoutDlg.SetPierModelType(m_PierModelType);
   m_CommonPierLayoutDlg.SetPierData(*m_pPier);
   VERIFY(m_CommonPierLayoutDlg.Create(IDD_PIER_LAYOUT_COMMON, this));
   VERIFY(m_CommonPierLayoutDlg.SetWindowPos(GetDlgItem(IDC_STATIC_BOUNDS), boxRect.left, boxRect.top, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE));//|SWP_NOMOVE));

   m_HammerheadPierLayoutDlg.SetPierModelType(m_PierModelType);
   m_HammerheadPierLayoutDlg.SetPierData(*m_pPier);
   VERIFY(m_HammerheadPierLayoutDlg.Create(IDD_PIER_LAYOUT_HAMMERHEAD, this));
   VERIFY(m_HammerheadPierLayoutDlg.SetWindowPos(GetDlgItem(IDC_STATIC_BOUNDS), boxRect.left, boxRect.top, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE));//|SWP_NOMOVE));


   CPropertyPage::OnInitDialog();

   UpdateConcreteTypeLabel();
   if ( m_strUserEc == _T("") )
   {
      m_ctrlEc.GetWindowText(m_strUserEc);
   }
   OnUserEc();

   OnPierModelTypeChanged();
   OnPierLayoutTypeChanged();


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
   pcbPierModel->SetItemData(idx, (DWORD_PTR)pgsTypes::pmtIdealized);

   idx = pcbPierModel->AddString(_T("Physical"));
   pcbPierModel->SetItemData(idx, (DWORD_PTR)pgsTypes::pmtPhysical);
}

void CPierLayoutPage::FillPierLayoutTypeComboBox()
{
   CComboBox* pcbPierModel = (CComboBox*)GetDlgItem(IDC_PIER_LAYOUT_TYPE);
   pcbPierModel->ResetContent();

   int idx = pcbPierModel->AddString(_T("Common"));
   pcbPierModel->SetItemData(idx, (DWORD_PTR)pgsTypes::pltCommon);

   idx = pcbPierModel->AddString(_T("Hammerhead"));
   pcbPierModel->SetItemData(idx, (DWORD_PTR)pgsTypes::pltHammerhead);

   idx = pcbPierModel->AddString(_T("Haunched"));
   pcbPierModel->SetItemData(idx, (DWORD_PTR)pgsTypes::pltHaunched);

   idx = pcbPierModel->AddString(_T("Custom"));
   pcbPierModel->SetItemData(idx, (DWORD_PTR)pgsTypes::pltCustom);

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


void CPierLayoutPage::OnPierLayoutTypeChanged()
{
    CComboBox* pcbPierModel = (CComboBox*)GetDlgItem(IDC_PIER_LAYOUT_TYPE);
    int curSel = pcbPierModel->GetCurSel();
    m_PierLayoutType = (pgsTypes::PierLayoutType)pcbPierModel->GetItemData(curSel);

    SwapDialogs();
}

void CPierLayoutPage::SwapDialogs()
{
    if (m_PierModelType == pgsTypes::pmtPhysical)
    {
        if (m_PierLayoutType == pgsTypes::pltCommon)
        {
            m_CommonPierLayoutDlg.ShowWindow(SW_SHOW);
            m_HammerheadPierLayoutDlg.ShowWindow(SW_HIDE);
        }
        else if (m_PierLayoutType == pgsTypes::pltHammerhead)
        {
            m_CommonPierLayoutDlg.ShowWindow(SW_HIDE);
            m_HammerheadPierLayoutDlg.ShowWindow(SW_SHOW);
            //m_HammerheadPierLayoutDlg.UpdateData(TRUE);
        }
        else
        {
            m_CommonPierLayoutDlg.ShowWindow(SW_HIDE);
            m_HammerheadPierLayoutDlg.ShowWindow(SW_HIDE);
        }
    }
}

