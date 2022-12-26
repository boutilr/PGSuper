///////////////////////////////////////////////////////////////////////
// PGSuper - Prestressed Girder SUPERstructure Design and Analysis
// Copyright © 1999-2022  Washington State Department of Transportation
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

// EditHaunchDlg.cpp : implementation file
//

#include "stdafx.h"
#include "resource.h"
#include "EditHaunchDlg.h"

#include <EAF\EAFMainFrame.h>
#include <EAF\EAFDisplayUnits.h>
#include <EAF\EAFDocument.h>

#include "PGSuperUnits.h"
#include "PGSuperDoc.h"
#include "Utilities.h"

#include <PgsExt\BridgeDescription2.h>
#include <PgsExt\TemporarySupportData.h>
#include <PgsExt\ClosureJointData.h>

#include <IFace\Project.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


IMPLEMENT_DYNAMIC(CEditHaunchDlg, CDialog)

CEditHaunchDlg::CEditHaunchDlg(const CBridgeDescription2* pBridgeDesc, CWnd* pParent /*=nullptr*/)
	: CDialog(CEditHaunchDlg::IDD, pParent),
   m_HaunchShape(pgsTypes::hsSquare)
{
   m_BridgeDesc = *pBridgeDesc;

   m_HaunchInputDepthType = m_BridgeDesc.GetHaunchInputDepthType();
}

CEditHaunchDlg::~CEditHaunchDlg()
{
}

void CEditHaunchDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);

   CComPtr<IBroker> pBroker;
   EAFGetBroker(&pBroker);
   GET_IFACE2(pBroker,IEAFDisplayUnits,pDisplayUnits);

   // fillet
   DDX_UnitValueAndTag( pDX, IDC_FILLET, IDC_FILLET_UNIT, m_Fillet, pDisplayUnits->GetComponentDimUnit() );
   DDV_UnitValueZeroOrMore( pDX, IDC_FILLET, m_Fillet, pDisplayUnits->GetComponentDimUnit() );

   DDX_Control(pDX,IDC_HAUNCH_SHAPE,m_cbHaunchShape);
   DDX_CBItemData(pDX, IDC_HAUNCH_SHAPE, m_HaunchShape);
   DDX_CBItemData(pDX,IDC_HAUNCH_DEPTH_TYPE,m_HaunchInputDepthType);

   if (pDX->m_bSaveAndValidate)
   {
      m_BridgeDesc.SetHaunchInputDepthType(m_HaunchInputDepthType);

      // Haunch shape
      m_BridgeDesc.GetDeckDescription()->HaunchShape = m_HaunchShape;
      m_BridgeDesc.SetFillet(m_Fillet);

      // PDX->Fails from lower level dialogs will not keep the dialog open. The code below will.
      if (m_HaunchInputDepthType == pgsTypes::hidACamber)
      {
         if (FALSE == m_EditHaunchACamberDlg.UpdateData(pDX->m_bSaveAndValidate))
         {
            pDX->Fail();
         }
      }
      else
      {
         if (FALSE == m_EditHaunchByHaunchDlg.UpdateData(true))
         {
            pDX->Fail();
         }
      }
   }
}

BEGIN_MESSAGE_MAP(CEditHaunchDlg, CDialog)
   ON_BN_CLICKED(ID_HELP, &CEditHaunchDlg::OnBnClickedHelp)
   ON_CBN_SELCHANGE(IDC_HAUNCH_DEPTH_TYPE,&OnHaunchDepthTypeChanged)
   ON_CBN_SELCHANGE(IDC_BY_SEG_SPAN,&OnHaunchLayoutTypeChanged)
END_MESSAGE_MAP()

// CEditHaunchDlg message handlers

BOOL CEditHaunchDlg::OnInitDialog()
{
   // Initialize our data structure for current data
   InitializeData();

   CEAFDocument* pDoc = EAFGetDocument();
   BOOL bIsPGSuper = pDoc->IsKindOf(RUNTIME_CLASS(CPGSuperDoc));

   // Initilize combo boxes
   CComboBox* pBox = (CComboBox*)GetDlgItem(IDC_HAUNCH_DEPTH_TYPE);
   int sqidx = pBox->AddString(CString(_T("Haunch Depth")));
   pBox->SetItemData(sqidx,(DWORD_PTR)pgsTypes::hidHaunchDirectly);
   sqidx = pBox->AddString(CString(_T("Haunch+Deck Depth")));
   pBox->SetItemData(sqidx,(DWORD_PTR)pgsTypes::hidHaunchPlusSlabDirectly);

   if (bIsPGSuper)
   {
      sqidx = pBox->AddString(CString(_T("\"A\" and Assumed Excess Camber")));
      pBox->SetItemData(sqidx,(DWORD_PTR)pgsTypes::hidACamber);
   }

   pBox = (CComboBox*)GetDlgItem(IDC_BY_SEG_SPAN);
   sqidx = pBox->AddString(CString(_T("along Spans")));
   pBox->SetItemData(sqidx,(DWORD_PTR)pgsTypes::hltAlongSpans);
   sqidx = pBox->AddString(CString(_T("along Segments")));
   pBox->SetItemData(sqidx,(DWORD_PTR)pgsTypes::hltAlongSegments);

   pBox->SetCurSel( m_BridgeDesc.GetHaunchLayoutType()== pgsTypes::hltAlongSpans? 0:1);
   pBox->EnableWindow(bIsPGSuper? FALSE: TRUE);

   // Embed dialogs for into current. A discription may be found at
   // http://www.codeproject.com/KB/dialog/embedded_dialog.aspx

   // Set up embedded dialogs
   {
      CWnd* pBox = GetDlgItem(IDC_PLACE_HOLDER);
      pBox->ShowWindow(SW_HIDE);

      CRect boxRect;
      pBox->GetWindowRect(&boxRect);
      ScreenToClient(boxRect);

      VERIFY(m_EditHaunchACamberDlg.Create(CEditHaunchACamberDlg::IDD, this));
      VERIFY(m_EditHaunchACamberDlg.SetWindowPos(GetDlgItem(IDC_PLACE_HOLDER), boxRect.left, boxRect.top, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE));//|SWP_NOMOVE));

      VERIFY(m_EditHaunchByHaunchDlg.Create(CEditHaunchByHaunchDlg::IDD,this));
      VERIFY(m_EditHaunchByHaunchDlg.SetWindowPos(GetDlgItem(IDC_PLACE_HOLDER),boxRect.left,boxRect.top,0,0,SWP_SHOWWINDOW | SWP_NOSIZE));//|SWP_NOMOVE));
   }

   CComPtr<IBroker> pBroker;
   EAFGetBroker(&pBroker);
   GET_IFACE2(pBroker,IEAFDisplayUnits,pDisplayUnits);

   const CDeckDescription2* pDeck = m_BridgeDesc.GetDeckDescription();
   Float64 Tdeck;
   if (pDeck->GetDeckType() == pgsTypes::sdtCompositeSIP)
   {
      Tdeck = pDeck->GrossDepth + pDeck->PanelDepth;
   }
   else
   {
      Tdeck = pDeck->GrossDepth;
   }

   Tdeck = WBFL::Units::ConvertFromSysUnits(Tdeck, pDisplayUnits->GetComponentDimUnit().UnitOfMeasure);

   CString deckNote;
   deckNote.Format(_T("Depth of Deck: %.3f %s"),Tdeck,pDisplayUnits->GetComponentDimUnit().UnitOfMeasure.UnitTag().c_str());

   CWnd* pWnd = GetDlgItem(IDC_DECK_DEPTH);
   pWnd->SetWindowText(deckNote);
   

   CDialog::OnInitDialog();

   m_cbHaunchShape.Initialize(m_HaunchShape);
   CDataExchange dx(this,FALSE);
   DDX_CBItemData(&dx,IDC_HAUNCH_SHAPE,m_HaunchShape);

   if ( IsAdjacentSpacing(m_BridgeDesc.GetGirderSpacingType()) )
   {
      m_cbHaunchShape.EnableWindow(FALSE); // cannot change haunch shape for adjacent spacing
   }

   OnHaunchDepthTypeChanged();

   return TRUE;  // return TRUE unless you set the focus to a control
   // EXCEPTION: OCX Property Pages should return FALSE
}

void CEditHaunchDlg::InitializeData()
{
   const CDeckDescription2* pDeck = m_BridgeDesc.GetDeckDescription();
   ATLASSERT(pDeck->GetDeckType()!= pgsTypes::sdtNone); // should not be able to edit haunch if no deck

   // If girder spacing is adjacent, force haunch shape to square
   m_HaunchShape = IsAdjacentSpacing(m_BridgeDesc.GetGirderSpacingType()) ? pgsTypes::hsSquare : pDeck->HaunchShape;
   m_Fillet = m_BridgeDesc.GetFillet();
}

pgsTypes::HaunchInputDepthType CEditHaunchDlg::GetHaunchInputDepthType()
{
   return m_HaunchInputDepthType;
}

pgsTypes::HaunchLayoutType CEditHaunchDlg::GetHaunchLayoutType()
{
   CComboBox* pBox = (CComboBox*)GetDlgItem(IDC_BY_SEG_SPAN);

   int curSel = pBox->GetCurSel();
   return (pgsTypes::HaunchLayoutType)pBox->GetItemData(curSel);
}

void CEditHaunchDlg::OnBnClickedHelp()
{
   EAFHelp( EAFGetDocument()->GetDocumentationSetName(), IDH_EDIT_HAUNCH);
}

void CEditHaunchDlg::OnHaunchDepthTypeChanged()
{
   CComboBox* pBox = (CComboBox*)GetDlgItem(IDC_HAUNCH_DEPTH_TYPE);
   int curSel = pBox->GetCurSel();
   m_HaunchInputDepthType = (pgsTypes::HaunchInputDepthType)pBox->GetItemData(curSel);

   switch (GetHaunchInputDepthType())
   {
   case pgsTypes::hidACamber:
      m_EditHaunchACamberDlg.ShowWindow(SW_SHOW);
      m_EditHaunchByHaunchDlg.ShowWindow(SW_HIDE);
      break;
   case  pgsTypes::hidHaunchDirectly:
   case  pgsTypes::hidHaunchPlusSlabDirectly:
      m_EditHaunchACamberDlg.ShowWindow(SW_HIDE);
      m_EditHaunchByHaunchDlg.ShowWindow(SW_SHOW);
      m_EditHaunchByHaunchDlg.OnHaunchDepthTypeChanged();
      break;
   default:
      ATLASSERT(0);
      break;
   };
}

void CEditHaunchDlg::OnHaunchLayoutTypeChanged()
{
   m_EditHaunchByHaunchDlg.OnHaunchLayoutTypeChanged();
}
