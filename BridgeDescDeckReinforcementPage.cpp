///////////////////////////////////////////////////////////////////////
// PGSuper - Prestressed Girder SUPERstructure Design and Analysis
// Copyright � 1999-2012  Washington State Department of Transportation
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

// BridgeDescDeckReinforcementPage.cpp : implementation file
//

#include "PGSuperAppPlugin\stdafx.h"
#include "PGSuperAppPlugin\resource.h"
#include "BridgeDescDeckReinforcementPage.h"
#include "BridgeDescDlg.h"
#include "PGSuperDoc.h"
#include <MFCTools\CustomDDX.h>

#include <EAF\EAFDisplayUnits.h>

#include "HtmlHelp\HelpTopics.hh"

#include <Lrfd\RebarPool.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CBridgeDescDeckReinforcementPage property page

IMPLEMENT_DYNCREATE(CBridgeDescDeckReinforcementPage, CPropertyPage)

CBridgeDescDeckReinforcementPage::CBridgeDescDeckReinforcementPage() : CPropertyPage(CBridgeDescDeckReinforcementPage::IDD)
{
	//{{AFX_DATA_INIT(CBridgeDescDeckReinforcementPage)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}

CBridgeDescDeckReinforcementPage::~CBridgeDescDeckReinforcementPage()
{
}

void CBridgeDescDeckReinforcementPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CBridgeDescDeckReinforcementPage)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP

   if (pDX->m_bSaveAndValidate)
   {
      int idx;
      DDX_CBIndex(pDX,IDC_MILD_STEEL_SELECTOR,idx);
      GetStirrupMaterial(idx,m_RebarData.TopRebarType,m_RebarData.TopRebarGrade);
      GetStirrupMaterial(idx,m_RebarData.BottomRebarType,m_RebarData.BottomRebarGrade);
   }
   else
   {
      int idx = GetStirrupMaterialIndex(m_RebarData.TopRebarType,m_RebarData.TopRebarGrade);
      DDX_CBIndex(pDX,IDC_MILD_STEEL_SELECTOR,idx);
   }

   CComPtr<IBroker> pBroker;
   EAFGetBroker(&pBroker);
   GET_IFACE2(pBroker,IEAFDisplayUnits,pDisplayUnits);

   DDX_UnitValueAndTag(pDX, IDC_TOP_COVER,    IDC_TOP_COVER_UNIT,    m_RebarData.TopCover,    pDisplayUnits->GetComponentDimUnit() );
   DDX_UnitValueAndTag(pDX, IDC_BOTTOM_COVER, IDC_BOTTOM_COVER_UNIT, m_RebarData.BottomCover, pDisplayUnits->GetComponentDimUnit() );

   DDX_CBItemData(pDX,IDC_TOP_MAT_BAR,    m_RebarData.TopRebarSize);
   DDX_CBItemData(pDX,IDC_BOTTOM_MAT_BAR, m_RebarData.BottomRebarSize);

   DDX_UnitValueAndTag(pDX, IDC_TOP_MAT_BAR_SPACING,    IDC_TOP_MAT_BAR_SPACING_UNIT,    m_RebarData.TopSpacing,    pDisplayUnits->GetComponentDimUnit() );
   DDX_UnitValueAndTag(pDX, IDC_BOTTOM_MAT_BAR_SPACING, IDC_BOTTOM_MAT_BAR_SPACING_UNIT, m_RebarData.BottomSpacing, pDisplayUnits->GetComponentDimUnit() );

   DDX_UnitValueAndTag(pDX, IDC_TOP_MAT_LUMP_SUM,    IDC_TOP_MAT_LUMP_SUM_UNIT,    m_RebarData.TopLumpSum,    pDisplayUnits->GetAvOverSUnit() );
   DDX_UnitValueAndTag(pDX, IDC_BOTTOM_MAT_LUMP_SUM, IDC_BOTTOM_MAT_LUMP_SUM_UNIT, m_RebarData.BottomLumpSum, pDisplayUnits->GetAvOverSUnit() );

   if ( pDX->m_bSaveAndValidate )
      m_Grid.GetRebarData(m_RebarData.NegMomentRebar);
   else
      m_Grid.FillGrid(m_RebarData.NegMomentRebar);
}


BEGIN_MESSAGE_MAP(CBridgeDescDeckReinforcementPage, CPropertyPage)
	//{{AFX_MSG_MAP(CBridgeDescDeckReinforcementPage)
	ON_COMMAND(ID_HELP, OnHelp)
	ON_BN_CLICKED(IDC_ADD, OnAdd)
	ON_BN_CLICKED(IDC_REMOVE, OnRemove)
   ON_NOTIFY_EX(TTN_NEEDTEXT,0,OnToolTipNotify)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CBridgeDescDeckReinforcementPage message handlers

void CBridgeDescDeckReinforcementPage::OnAdd() 
{
	m_Grid.AddRow();
}

void CBridgeDescDeckReinforcementPage::OnRemove() 
{
   m_Grid.RemoveSelectedRows();
}

BOOL CBridgeDescDeckReinforcementPage::OnInitDialog() 
{
   m_Grid.SubclassDlgItem(IDC_GRID, this);
   m_Grid.CustomInit();

   CComboBox* pcbTopRebar    = (CComboBox*)GetDlgItem(IDC_TOP_MAT_BAR);
   CComboBox* pcbBottomRebar = (CComboBox*)GetDlgItem(IDC_BOTTOM_MAT_BAR);
   FillRebarComboBox(pcbTopRebar);
   FillRebarComboBox(pcbBottomRebar);

   CComboBox* pcbMaterial = (CComboBox*)GetDlgItem(IDC_MILD_STEEL_SELECTOR);
   FillMaterialComboBox(pcbMaterial);

   CPropertyPage::OnInitDialog();
   
   EnableToolTips(TRUE);
   
   EnableRemoveBtn(false); // start off with the button disabled... enabled once a selection is made
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}
   
void CBridgeDescDeckReinforcementPage::FillRebarComboBox(CComboBox* pcbRebar)
{
   // Item data is the rebar key for the lrfdRebarPool object
   int idx = pcbRebar->AddString(_T("None"));
   pcbRebar->SetItemData(idx,(DWORD_PTR)matRebar::bsNone);

#pragma Reminder("BUG: get the available bar sizes from the rebar pool")
   // using hard coded list... use a rebar pool iter
   // also, the name can come from the rebar pool as well
   idx = pcbRebar->AddString(_T("#4"));
   pcbRebar->SetItemData(idx,(DWORD_PTR)matRebar::bs4);

   idx = pcbRebar->AddString(_T("#5"));
   pcbRebar->SetItemData(idx,(DWORD_PTR)matRebar::bs5);

   idx = pcbRebar->AddString(_T("#6"));
   pcbRebar->SetItemData(idx,(DWORD_PTR)matRebar::bs6);

   idx = pcbRebar->AddString(_T("#7"));
   pcbRebar->SetItemData(idx,(DWORD_PTR)matRebar::bs7);

   idx = pcbRebar->AddString(_T("#8"));
   pcbRebar->SetItemData(idx,(DWORD_PTR)matRebar::bs8);

   idx = pcbRebar->AddString(_T("#9"));
   pcbRebar->SetItemData(idx,(DWORD_PTR)matRebar::bs9);
}

void CBridgeDescDeckReinforcementPage::EnableRemoveBtn(bool bEnable)
{
   GetDlgItem(IDC_REMOVE)->EnableWindow(bEnable);
}

BOOL CBridgeDescDeckReinforcementPage::OnSetActive() 
{
   // If the deck type is sdtNone then there isn't a deck and isn't any need for rebar
   // If the deck type is sdtCompositeSIP then there is only a top mat of rebar (deck panels for the bottom mat)
   BOOL bEnableTop    = TRUE;
   BOOL bEnableBottom = TRUE;
   
   CBridgeDescDlg* pParent = (CBridgeDescDlg*)GetParent();

   if ( pParent->m_BridgeDesc.GetDeckDescription()->DeckType == pgsTypes::sdtNone )
   {
      bEnableTop    = FALSE;
      bEnableBottom = FALSE;
      GetDlgItem(IDC_DECK)->SetWindowText(_T("Deck reinforcement cannot be described because deck type is None."));
   }
   else if ( pParent->m_BridgeDesc.GetDeckDescription()->DeckType == pgsTypes::sdtCompositeSIP )
   {
      bEnableTop    = TRUE;
      bEnableBottom = FALSE;
      GetDlgItem(IDC_DECK)->SetWindowText(_T("Bottom mat reinforcement cannot be described because deck type is Stay in Place deck panels."));
   }
   else
   {
      bEnableTop    = TRUE;
      bEnableBottom = TRUE;
      GetDlgItem(IDC_DECK)->SetWindowText(_T("Describe top and bottom mat of deck reinforcement."));
   }

   GetDlgItem(IDC_TOP_COVER)->EnableWindow(bEnableTop);
   GetDlgItem(IDC_TOP_COVER_UNIT)->EnableWindow(bEnableTop);

   GetDlgItem(IDC_TOP_MAT_BAR)->EnableWindow(bEnableTop);

   GetDlgItem(IDC_TOP_MAT_BAR_SPACING)->EnableWindow(bEnableTop);
   GetDlgItem(IDC_TOP_MAT_BAR_SPACING_UNIT)->EnableWindow(bEnableTop);

   GetDlgItem(IDC_TOP_MAT_LUMP_SUM)->EnableWindow(bEnableTop);
   GetDlgItem(IDC_TOP_MAT_LUMP_SUM_UNIT)->EnableWindow(bEnableTop);
	

   GetDlgItem(IDC_BOTTOM_COVER)->EnableWindow(bEnableBottom);
   GetDlgItem(IDC_BOTTOM_COVER_UNIT)->EnableWindow(bEnableBottom);

   GetDlgItem(IDC_BOTTOM_MAT_BAR)->EnableWindow(bEnableBottom);

   GetDlgItem(IDC_BOTTOM_MAT_BAR_SPACING)->EnableWindow(bEnableBottom);
   GetDlgItem(IDC_BOTTOM_MAT_BAR_SPACING_UNIT)->EnableWindow(bEnableBottom);

   GetDlgItem(IDC_BOTTOM_MAT_LUMP_SUM)->EnableWindow(bEnableBottom);
   GetDlgItem(IDC_BOTTOM_MAT_LUMP_SUM_UNIT)->EnableWindow(bEnableBottom);

   if ( !bEnableTop && !bEnableBottom )
      m_Grid.EnableWindow(FALSE);

   m_Grid.EnableMats(bEnableTop,bEnableBottom);
   GetDlgItem(IDC_ADD)->EnableWindow(bEnableTop || bEnableBottom);
   GetDlgItem(IDC_REMOVE)->EnableWindow(bEnableTop || bEnableBottom);
	
   return CPropertyPage::OnSetActive();
}

void CBridgeDescDeckReinforcementPage::OnHelp() 
{
   ::HtmlHelp( *this, AfxGetApp()->m_pszHelpFilePath, HH_HELP_CONTEXT, IDH_BRIDGEDESC_DECKREBAR );
}

BOOL CBridgeDescDeckReinforcementPage::OnToolTipNotify(UINT id,NMHDR* pNMHDR, LRESULT* pResult)
{
   TOOLTIPTEXT* pTTT = (TOOLTIPTEXT*)pNMHDR;
   HWND hwndTool = (HWND)pNMHDR->idFrom;
   if ( pTTT->uFlags & TTF_IDISHWND )
   {
      // idFrom is actually HWND of tool
      UINT nID = ::GetDlgCtrlID(hwndTool);
      switch(nID)
      {
      case IDC_TOP_COVER:
         m_strTip = _T("Measured from top of deck to center of top mat of reinforcing");
         break;

      case IDC_BOTTOM_COVER:
         m_strTip = _T("Measured from bottom of deck to center of bottom mat of reinforcing");
         break;

      default:
         return FALSE;
      }

      ::SendMessage(pNMHDR->hwndFrom,TTM_SETDELAYTIME,TTDT_AUTOPOP,TOOLTIP_DURATION); // sets the display time to 10 seconds
      ::SendMessage(pNMHDR->hwndFrom,TTM_SETMAXTIPWIDTH,0,TOOLTIP_WIDTH); // makes it a multi-line tooltip
      pTTT->lpszText = m_strTip.LockBuffer();
      pTTT->hinst = NULL;
      return TRUE;
   }
   return FALSE;
}

void CBridgeDescDeckReinforcementPage::FillMaterialComboBox(CComboBox* pCB)
{
   pCB->AddString( lrfdRebarPool::GetMaterialName(matRebar::A615,matRebar::Grade40).c_str() );
   pCB->AddString( lrfdRebarPool::GetMaterialName(matRebar::A615,matRebar::Grade60).c_str() );
   pCB->AddString( lrfdRebarPool::GetMaterialName(matRebar::A615,matRebar::Grade75).c_str() );
   pCB->AddString( lrfdRebarPool::GetMaterialName(matRebar::A615,matRebar::Grade80).c_str() );
   pCB->AddString( lrfdRebarPool::GetMaterialName(matRebar::A706,matRebar::Grade60).c_str() );
   pCB->AddString( lrfdRebarPool::GetMaterialName(matRebar::A706,matRebar::Grade80).c_str() );
}

void CBridgeDescDeckReinforcementPage::GetStirrupMaterial(int idx,matRebar::Type& type,matRebar::Grade& grade)
{
   switch(idx)
   {
   case 0:  type = matRebar::A615; grade = matRebar::Grade40; break;
   case 1:  type = matRebar::A615; grade = matRebar::Grade60; break;
   case 2:  type = matRebar::A615; grade = matRebar::Grade75; break;
   case 3:  type = matRebar::A615; grade = matRebar::Grade80; break;
   case 4:  type = matRebar::A706; grade = matRebar::Grade60; break;
   case 5:  type = matRebar::A706; grade = matRebar::Grade80; break;
   default:
      ATLASSERT(false); // should never get here
   }
}

int CBridgeDescDeckReinforcementPage::GetStirrupMaterialIndex(matRebar::Type type,matRebar::Grade grade)
{
   if ( type == matRebar::A615 )
   {
      if ( grade == matRebar::Grade40 )
         return 0;
      else if ( grade == matRebar::Grade60 )
         return 1;
      else if ( grade == matRebar::Grade75 )
         return 2;
      else if ( grade == matRebar::Grade80 )
         return 3;
   }
   else
   {
      if ( grade == matRebar::Grade60 )
         return 4;
      else if ( grade == matRebar::Grade80 )
         return 5;
   }

   ATLASSERT(false); // should never get here
   return -1;
}
