///////////////////////////////////////////////////////////////////////
// PGSuper - Prestressed Girder SUPERstructure Design and Analysis
// Copyright (C) 1999  Washington State Department of Transportation
//                     Bridge and Structures Office
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

// BridgeDescDeckDetailsPagePage.cpp : implementation file
//

#include "stdafx.h"
#include "pgsuper.h"
#include "PGSuperDoc.h"
#include "BridgeDescDeckDetailsPage.h"
#include "BridgeDescDlg.h"
#include "ConcreteDetailsDlg.h"
#include "UIHintsDlg.h"
#include "Hints.h"

#include <System\Flags.h>

#include "HtmlHelp\HelpTopics.hh"

#include <WBFLGenericBridge.h>

#include <MfcTools\CustomDDX.h>

#include <IFace\Bridge.h>
#include <IFace\DisplayUnits.h>
#include "PGSuperUnits.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CBridgeDescDeckDetailsPage property page

IMPLEMENT_DYNCREATE(CBridgeDescDeckDetailsPage, CPropertyPage)

CBridgeDescDeckDetailsPage::CBridgeDescDeckDetailsPage() : CPropertyPage(CBridgeDescDeckDetailsPage::IDD)
{
	//{{AFX_DATA_INIT(CBridgeDescDeckDetailsPage)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}

CBridgeDescDeckDetailsPage::~CBridgeDescDeckDetailsPage()
{
}

void CBridgeDescDeckDetailsPage::DoDataExchange(CDataExchange* pDX)
{
   CComPtr<IBroker> pBroker;
   AfxGetBroker(&pBroker);
   GET_IFACE2(pBroker,IDisplayUnits,pDispUnits);
	
   CBridgeDescDlg* pParent = (CBridgeDescDlg*)GetParent();
   ASSERT( pParent->IsKindOf(RUNTIME_CLASS(CBridgeDescDlg)) );

   pgsTypes::SupportedDeckType deckType = pParent->m_BridgeDesc.GetDeckDescription()->DeckType;

   CPropertyPage::DoDataExchange(pDX);

	//{{AFX_DATA_MAP(CBridgeDescDeckDetailsPage)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	DDX_Control(pDX, IDC_EC,      m_ctrlEc);
	DDX_Control(pDX, IDC_MOD_E,   m_ctrlEcCheck);
	DDX_Control(pDX, IDC_SLAB_FC, m_ctrlFc);
	//}}AFX_DATA_MAP

   // make sure unit tags are always displayed (even if disabled)
   DDX_Tag( pDX, IDC_GROSS_DEPTH_UNIT,    pDispUnits->GetComponentDimUnit() );
   DDX_Tag( pDX, IDC_FILLET_UNIT,         pDispUnits->GetComponentDimUnit() );
   DDX_Tag( pDX, IDC_ADIM_UNIT,           pDispUnits->GetComponentDimUnit() );
   DDX_Tag( pDX, IDC_OVERHANG_DEPTH_UNIT, pDispUnits->GetComponentDimUnit() );
   DDX_Tag( pDX, IDC_PANEL_DEPTH_UNIT,    pDispUnits->GetComponentDimUnit() );
   DDX_Tag( pDX, IDC_PANEL_SUPPORT_UNIT,  pDispUnits->GetComponentDimUnit() );
   DDX_Tag( pDX, IDC_SACDEPTH_UNIT,       pDispUnits->GetComponentDimUnit() );
   DDX_Tag( pDX, IDC_SLAB_FC_UNIT,        pDispUnits->GetStressUnit() );
   DDX_Tag( pDX, IDC_EC_UNIT,             pDispUnits->GetModEUnit() );

   if ( deckType != pgsTypes::sdtNone )
   {
      // gross or cast depth
      DDX_UnitValueAndTag( pDX, IDC_GROSS_DEPTH,   IDC_GROSS_DEPTH_UNIT,  pParent->m_BridgeDesc.GetDeckDescription()->GrossDepth, pDispUnits->GetComponentDimUnit() );
      DDV_UnitValueGreaterThanZero( pDX, pParent->m_BridgeDesc.GetDeckDescription()->GrossDepth, pDispUnits->GetComponentDimUnit() );

      // fillet
      DDX_UnitValueAndTag( pDX, IDC_FILLET, IDC_FILLET_UNIT, pParent->m_BridgeDesc.GetDeckDescription()->Fillet, pDispUnits->GetComponentDimUnit() );
      DDV_UnitValueZeroOrMore( pDX, pParent->m_BridgeDesc.GetDeckDescription()->Fillet, pDispUnits->GetComponentDimUnit() );

      // slab offset
      DDX_Check_Bool(pDX,IDC_SAMESLABOFFSET,m_bSlabOffsetWholeBridge);
      if ( m_bSlabOffsetWholeBridge )
         DDX_UnitValueAndTag( pDX, IDC_ADIM, IDC_ADIM_UNIT, m_SlabOffset, pDispUnits->GetComponentDimUnit() );

      // overhang
      DDX_UnitValueAndTag( pDX, IDC_OVERHANG_DEPTH, IDC_OVERHANG_DEPTH_UNIT, pParent->m_BridgeDesc.GetDeckDescription()->OverhangEdgeDepth, pDispUnits->GetComponentDimUnit() );
      DDX_CBItemData( pDX, IDC_OVERHANG_TAPER, pParent->m_BridgeDesc.GetDeckDescription()->OverhangTaper );

      // deck panel
      DDX_UnitValueAndTag( pDX, IDC_PANEL_DEPTH, IDC_PANEL_DEPTH_UNIT,  pParent->m_BridgeDesc.GetDeckDescription()->PanelDepth, pDispUnits->GetComponentDimUnit() );
      if ( pParent->m_BridgeDesc.GetDeckDescription()->DeckType == pgsTypes::sdtCompositeSIP ) // SIP
         DDV_UnitValueGreaterThanZero( pDX, pParent->m_BridgeDesc.GetDeckDescription()->PanelDepth, pDispUnits->GetXSectionDimUnit() );

      DDX_UnitValueAndTag( pDX, IDC_PANEL_SUPPORT, IDC_PANEL_SUPPORT_UNIT,  pParent->m_BridgeDesc.GetDeckDescription()->PanelSupport, pDispUnits->GetComponentDimUnit() );

      // slab material
      ExchangeConcreteData(pDX);

      DDX_UnitValueAndTag( pDX, IDC_SACDEPTH,      IDC_SACDEPTH_UNIT,     pParent->m_BridgeDesc.GetDeckDescription()->SacrificialDepth, pDispUnits->GetComponentDimUnit() );
   }

   // overlay
   DDX_CBItemData( pDX, IDC_WEARINGSURFACETYPE, pParent->m_BridgeDesc.GetDeckDescription()->WearingSurface );

   int bInputAsDepthAndDensity;
   if ( !pDX->m_bSaveAndValidate )
   {
      bInputAsDepthAndDensity = (pParent->m_BridgeDesc.GetDeckDescription()->bInputAsDepthAndDensity ? 1 : 0);
   }
   
   DDX_Radio(pDX, IDC_OLAY_WEIGHT_LABEL, bInputAsDepthAndDensity );

   if ( pDX->m_bSaveAndValidate )
   {
      pParent->m_BridgeDesc.GetDeckDescription()->bInputAsDepthAndDensity = (bInputAsDepthAndDensity == 1 ? true : false);
   }
   
   DDX_UnitValueAndTag( pDX, IDC_OLAY_WEIGHT,   IDC_OLAY_WEIGHT_UNIT,  pParent->m_BridgeDesc.GetDeckDescription()->OverlayWeight, pDispUnits->GetOverlayWeightUnit() );

   DDX_UnitValueAndTag( pDX, IDC_OLAY_DEPTH,    IDC_OLAY_DEPTH_UNIT,   pParent->m_BridgeDesc.GetDeckDescription()->OverlayDepth, pDispUnits->GetComponentDimUnit() );
   DDX_UnitValueAndTag( pDX, IDC_OLAY_DENSITY,  IDC_OLAY_DENSITY_UNIT, pParent->m_BridgeDesc.GetDeckDescription()->OverlayDensity, pDispUnits->GetDensityUnit() );

   if ( pDX->m_bSaveAndValidate )
   {
      if ( pParent->m_BridgeDesc.GetDeckDescription()->WearingSurface == pgsTypes::wstSacrificialDepth )
      {
         pDX->PrepareEditCtrl(IDC_SACDEPTH);
         DDV_UnitValueZeroOrMore( pDX, pParent->m_BridgeDesc.GetDeckDescription()->SacrificialDepth, pDispUnits->GetComponentDimUnit() );
         DDV_UnitValueLessThanLimit( pDX, pParent->m_BridgeDesc.GetDeckDescription()->SacrificialDepth, pParent->m_BridgeDesc.GetDeckDescription()->GrossDepth, pDispUnits->GetComponentDimUnit() );
      }
      else
      {
         if ( pParent->m_BridgeDesc.GetDeckDescription()->bInputAsDepthAndDensity )
         {
            pDX->PrepareEditCtrl(IDC_OLAY_DEPTH);
            DDV_UnitValueZeroOrMore( pDX, pParent->m_BridgeDesc.GetDeckDescription()->OverlayDepth, pDispUnits->GetComponentDimUnit() );

            pDX->PrepareEditCtrl(IDC_OLAY_DENSITY);
            DDV_UnitValueZeroOrMore( pDX, pParent->m_BridgeDesc.GetDeckDescription()->OverlayDensity, pDispUnits->GetDensityUnit());

            Float64 g = unitSysUnitsMgr::GetGravitationalAcceleration();
            pParent->m_BridgeDesc.GetDeckDescription()->OverlayWeight = pParent->m_BridgeDesc.GetDeckDescription()->OverlayDepth * pParent->m_BridgeDesc.GetDeckDescription()->OverlayDensity * g;
         }
         else
         {
            pDX->PrepareEditCtrl(IDC_OLAY_WEIGHT);
            DDV_UnitValueZeroOrMore( pDX, pParent->m_BridgeDesc.GetDeckDescription()->OverlayWeight, pDispUnits->GetOverlayWeightUnit() );
         }
      }
   }
      
   if ( pDX->m_bSaveAndValidate && deckType != pgsTypes::sdtNone )
   {
      // Validate slab depth
      if ( m_bSlabOffsetWholeBridge )
      {
         // Slab offset applies to the entire bridge... have users adjust Slab offset if it doesn't
         // fit with the slab depth

         pDX->PrepareEditCtrl(IDC_ADIM);

         DDV_UnitValueLimitOrMore( pDX, m_SlabOffset, pParent->m_BridgeDesc.GetDeckDescription()->GrossDepth + pParent->m_BridgeDesc.GetDeckDescription()->Fillet, pDispUnits->GetComponentDimUnit() );
         pParent->m_BridgeDesc.SetSlabOffset(m_SlabOffset);
         pParent->m_BridgeDesc.SetSlabOffsetType(pgsTypes::sotBridge);

         Float64 grossDepth;
         bool bCheckDepth = false;
         CString strMsg1;
         CString strMsg2;

         if ( pParent->m_BridgeDesc.GetDeckDescription()->DeckType == pgsTypes::sdtCompositeCIP ||
              pParent->m_BridgeDesc.GetDeckDescription()->DeckType == pgsTypes::sdtCompositeOverlay ) // CIP deck
         {
            grossDepth = pParent->m_BridgeDesc.GetDeckDescription()->GrossDepth;
            strMsg1 = "Slab Offset must be larger than the gross slab depth";
            strMsg2 = "Overhang edge depth must be less than the gross slab depth";
            bCheckDepth = true;
         }
         else if ( pParent->m_BridgeDesc.GetDeckDescription()->DeckType == pgsTypes::sdtCompositeSIP ) // SIP
         {
            grossDepth = pParent->m_BridgeDesc.GetDeckDescription()->GrossDepth + pParent->m_BridgeDesc.GetDeckDescription()->PanelDepth;
            strMsg1 = "Slab Offset must be larger than the cast depth + panel depth";
            strMsg2 = "Overhang edge depth must be less than the cast depth + panel depth";
            bCheckDepth = true;
         }
         else
         {
            ATLASSERT(0);
            // should not get here
         }

         if ( bCheckDepth && m_SlabOffset < grossDepth )
         {
            AfxMessageBox(strMsg1);
            pDX->PrepareEditCtrl(IDC_ADIM);
            pDX->Fail();
         }

         //if ( bCheckDepth && 
         //     grossDepth < pParent->m_BridgeDesc.GetDeckDescription()->OverhangEdgeDepth &&
         //     pParent->m_BridgeDesc.GetDeckDescription()->DeckType != pgsTypes::sdtCompositeOverlay  )
         //{
         //   AfxMessageBox(strMsg2);
         //   pDX->PrepareEditCtrl(IDC_OVERHANG_DEPTH);
         //   pDX->Fail();
         //}
      }
      else
      {
         // Slab offset is girder by girder. Have user adjust the slab depth if it doesn't
         // fit with the current values for slab offset.

         pParent->m_BridgeDesc.SetSlabOffsetType(pgsTypes::sotGirder);

         Float64 maxSlabOffset = pParent->m_BridgeDesc.GetMaxSlabOffset();

         if ( pParent->m_BridgeDesc.GetDeckDescription()->DeckType == pgsTypes::sdtCompositeCIP || 
              pParent->m_BridgeDesc.GetDeckDescription()->DeckType == pgsTypes::sdtCompositeOverlay )
         {
            pDX->PrepareEditCtrl(IDC_GROSS_DEPTH);
            if ( maxSlabOffset - pParent->m_BridgeDesc.GetDeckDescription()->Fillet < pParent->m_BridgeDesc.GetDeckDescription()->GrossDepth )
            {
               CString msg;
               msg.Format("Gross slab depth must less than %s to accomodate the %s fillet and maximum slab offset of %s",
                            FormatDimension(maxSlabOffset - pParent->m_BridgeDesc.GetDeckDescription()->Fillet,pDispUnits->GetComponentDimUnit()),
                            FormatDimension(pParent->m_BridgeDesc.GetDeckDescription()->Fillet,pDispUnits->GetComponentDimUnit()),
                            FormatDimension(maxSlabOffset,pDispUnits->GetComponentDimUnit()));
               AfxMessageBox(msg,MB_ICONEXCLAMATION);
               pDX->Fail();
            }
         }
         else if ( pParent->m_BridgeDesc.GetDeckDescription()->DeckType == pgsTypes::sdtCompositeSIP )
         {
            pDX->PrepareEditCtrl(IDC_PANEL_DEPTH);
            if ( maxSlabOffset -  pParent->m_BridgeDesc.GetDeckDescription()->PanelDepth - pParent->m_BridgeDesc.GetDeckDescription()->Fillet < pParent->m_BridgeDesc.GetDeckDescription()->GrossDepth )
            {
               CString msg;
               msg.Format("Cast slab depth must less than %s to accomodate the %s panel, %s fillet and maximum slab offset of %s",
                            FormatDimension(maxSlabOffset - pParent->m_BridgeDesc.GetDeckDescription()->PanelDepth - pParent->m_BridgeDesc.GetDeckDescription()->Fillet,pDispUnits->GetComponentDimUnit()),
                            FormatDimension(pParent->m_BridgeDesc.GetDeckDescription()->PanelDepth,pDispUnits->GetComponentDimUnit()),
                            FormatDimension(pParent->m_BridgeDesc.GetDeckDescription()->Fillet,pDispUnits->GetComponentDimUnit()),
                            FormatDimension(maxSlabOffset,pDispUnits->GetComponentDimUnit()));
               AfxMessageBox(msg,MB_ICONEXCLAMATION);
               pDX->Fail();
            }
         }
         else
         {
            ATLASSERT(0);
            // should not get here
         }
      }
   }

   if ( pDX->m_bSaveAndValidate )
   {
      pParent->m_BridgeDesc.GetDeckDescription()->DeckEdgePoints = m_Grid.GetEdgePoints();
   }
   else
   {
      m_Grid.FillGrid(pParent->m_BridgeDesc.GetDeckDescription());
   }
}


void CBridgeDescDeckDetailsPage::ExchangeConcreteData(CDataExchange* pDX)
{
   CComPtr<IBroker> pBroker;
   AfxGetBroker(&pBroker);
   GET_IFACE2(pBroker,IDisplayUnits,pDispUnits);

   CBridgeDescDlg* pParent = (CBridgeDescDlg*)GetParent();
   ASSERT( pParent->IsKindOf(RUNTIME_CLASS(CBridgeDescDlg)) );

   DDX_UnitValueAndTag( pDX, IDC_SLAB_FC, IDC_SLAB_FC_UNIT, pParent->m_BridgeDesc.GetDeckDescription()->SlabFc, pDispUnits->GetStressUnit() );

   DDX_Check_Bool(pDX,IDC_MOD_E, pParent->m_BridgeDesc.GetDeckDescription()->SlabUserEc);
   DDX_UnitValueAndTag( pDX, IDC_EC,  IDC_EC_UNIT, pParent->m_BridgeDesc.GetDeckDescription()->SlabEc, pDispUnits->GetModEUnit() );


   if ( pParent->m_BridgeDesc.GetDeckDescription()->DeckType != pgsTypes::sdtNone && pDX->m_bSaveAndValidate )
   {
      pDX->PrepareCtrl(IDC_SLAB_FC);
      DDV_UnitValueGreaterThanZero( pDX, pParent->m_BridgeDesc.GetDeckDescription()->SlabFc, pDispUnits->GetStressUnit() );

      pDX->PrepareCtrl(IDC_EC);
      DDV_UnitValueGreaterThanZero( pDX, pParent->m_BridgeDesc.GetDeckDescription()->SlabEc, pDispUnits->GetModEUnit() );
   }

   if ( pDX->m_bSaveAndValidate && m_ctrlEcCheck.GetCheck() == 1 )
   {
      m_ctrlEc.GetWindowText(m_strUserEc);
   }
}

BEGIN_MESSAGE_MAP(CBridgeDescDeckDetailsPage, CPropertyPage)
	//{{AFX_MSG_MAP(CBridgeDescDeckDetailsPage)
	ON_COMMAND(ID_HELP, OnHelp)
	ON_BN_CLICKED(IDC_MOD_E, OnUserEc)
	ON_EN_CHANGE(IDC_SLAB_FC, OnChangeSlabFc)
   ON_CBN_SELCHANGE(IDC_WEARINGSURFACETYPE,OnWearingSurfaceTypeChanged)
	ON_BN_CLICKED(IDC_MORE, OnMoreConcreteProperties)
   ON_NOTIFY_EX(TTN_NEEDTEXT,0,OnToolTipNotify)
   ON_BN_CLICKED(IDC_ADD,OnAddDeckEdgePoint)
   ON_BN_CLICKED(IDC_REMOVE,OnRemoveDeckEdgePoint)
	//}}AFX_MSG_MAP
   ON_BN_CLICKED(IDC_OLAY_WEIGHT_LABEL, &CBridgeDescDeckDetailsPage::OnBnClickedOlayWeightLabel)
   ON_BN_CLICKED(IDC_OLAY_DEPTH_LABEL, &CBridgeDescDeckDetailsPage::OnBnClickedOlayDepthLabel)
   ON_BN_CLICKED(IDC_SAMESLABOFFSET, &CBridgeDescDeckDetailsPage::OnBnClickedSameslaboffset)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CBridgeDescDeckDetailsPage message handlers

BOOL CBridgeDescDeckDetailsPage::OnInitDialog() 
{
   m_Grid.SubclassDlgItem(IDC_GRID, this);
   m_Grid.CustomInit();

   CBridgeDescDlg* pParent = (CBridgeDescDlg*)GetParent();
   ASSERT( pParent->IsKindOf(RUNTIME_CLASS(CBridgeDescDlg)) );

   CComPtr<IBroker> pBroker;
   AfxGetBroker(&pBroker);
   GET_IFACE2(pBroker,IDisplayUnits,pDispUnits);

   // set density/weight labels
   CStatic* pStatic = (CStatic*)GetDlgItem( IDC_OLAY_DENSITY_LABEL );
   if ( pDispUnits->GetUnitDisplayMode() == pgsTypes::umSI )
      pStatic->SetWindowText( "Overlay Density" );
   else
      pStatic->SetWindowText( "Overlay Weight" );

   // fill up slab overhang taper options
   CComboBox* pCB = (CComboBox*)GetDlgItem(IDC_OVERHANG_TAPER);
   int idx = pCB->AddString("Taper overhang to top of top flange");
   pCB->SetItemData(idx,(DWORD)pgsTypes::TopTopFlange);

   idx = pCB->AddString("Taper overhang to bottom of top flange");
   pCB->SetItemData(idx,(DWORD)pgsTypes::BottomTopFlange);

   idx = pCB->AddString("Don't taper overhang");
   pCB->SetItemData(idx,(DWORD)pgsTypes::None);

   // wearing surface types
   pCB = (CComboBox*)GetDlgItem(IDC_WEARINGSURFACETYPE);

   if ( pParent->m_BridgeDesc.GetDeckDescription()->DeckType != pgsTypes::sdtNone )
   {
      idx = pCB->AddString("Sacrificial Depth of Concrete Deck");
      pCB->SetItemData(idx,(DWORD)pgsTypes::wstSacrificialDepth);
   }

   idx = pCB->AddString("Overlay");
   pCB->SetItemData(idx,(DWORD)pgsTypes::wstOverlay);

   idx = pCB->AddString("Future Overlay");
   pCB->SetItemData(idx,(DWORD)pgsTypes::wstFutureOverlay);

   m_SlabOffset = pParent->m_BridgeDesc.GetSlabOffset();
   m_bSlabOffsetWholeBridge = pParent->m_BridgeDesc.GetSlabOffsetType() == pgsTypes::sotBridge ? true : false;
   m_strSlabOffsetCache.Format("%s",FormatDimension(m_SlabOffset,pDispUnits->GetComponentDimUnit(), false));

   CPropertyPage::OnInitDialog();

   if ( m_strUserEc == "" )
      m_ctrlEc.GetWindowText(m_strUserEc);
	
   OnWearingSurfaceTypeChanged();
   UpdateSlabOffsetControls();

   EnableToolTips(TRUE);

   EnableRemove(FALSE);
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CBridgeDescDeckDetailsPage::OnHelp() 
{
   ::HtmlHelp( *this, AfxGetApp()->m_pszHelpFilePath, HH_HELP_CONTEXT, IDH_BRIDGEDESC_DECKDETAILS );
}

BOOL CBridgeDescDeckDetailsPage::OnSetActive() 
{
   CBridgeDescDlg* pParent = (CBridgeDescDlg*)GetParent();
   ASSERT( pParent->IsKindOf(RUNTIME_CLASS(CBridgeDescDlg)) );

   pgsTypes::SupportedDeckType deckType = pParent->m_BridgeDesc.GetDeckDescription()->DeckType;

   CString strDeckType;
   switch ( deckType )
   {
      case pgsTypes::sdtCompositeCIP:
         strDeckType = "Composite Cast-In-Place Deck";
         break;

      case pgsTypes::sdtCompositeSIP:
         strDeckType = "Composite Stay-In-Place Deck Panels";
         break;

      case pgsTypes::sdtCompositeOverlay:
         strDeckType = "Composite Cast-In-Place Overlay";
         break;

      case pgsTypes::sdtNone:
         strDeckType = "No deck";
   }
   GetDlgItem(IDC_DECK_TYPE)->SetWindowText(strDeckType);


   CWnd* pWnd = GetDlgItem(IDC_DEPTH_LABEL);
   pWnd->SetWindowText(deckType == pgsTypes::sdtCompositeCIP || deckType == pgsTypes::sdtCompositeOverlay ? "Gross Depth" : "Cast Depth");

   GetDlgItem(IDC_GROSS_DEPTH_LABEL)->EnableWindow( deckType != pgsTypes::sdtNone);
   GetDlgItem(IDC_GROSS_DEPTH)->EnableWindow(       deckType != pgsTypes::sdtNone);
   GetDlgItem(IDC_GROSS_DEPTH_UNIT)->EnableWindow(  deckType != pgsTypes::sdtNone);

   GetDlgItem(IDC_OVERHANG_DEPTH_LABEL)->EnableWindow( deckType == pgsTypes::sdtCompositeCIP || deckType == pgsTypes::sdtCompositeSIP);
   GetDlgItem(IDC_OVERHANG_DEPTH)->EnableWindow(       deckType == pgsTypes::sdtCompositeCIP || deckType == pgsTypes::sdtCompositeSIP);
   GetDlgItem(IDC_OVERHANG_DEPTH_UNIT)->EnableWindow(  deckType == pgsTypes::sdtCompositeCIP || deckType == pgsTypes::sdtCompositeSIP);
   GetDlgItem(IDC_OVERHANG_TAPER)->EnableWindow(       deckType == pgsTypes::sdtCompositeCIP || deckType == pgsTypes::sdtCompositeSIP);

   GetDlgItem(IDC_FILLET_LABEL)->EnableWindow( deckType == pgsTypes::sdtCompositeCIP || deckType == pgsTypes::sdtCompositeSIP);
   GetDlgItem(IDC_FILLET)->EnableWindow(       deckType == pgsTypes::sdtCompositeCIP || deckType == pgsTypes::sdtCompositeSIP);
   GetDlgItem(IDC_FILLET_UNIT)->EnableWindow(  deckType == pgsTypes::sdtCompositeCIP || deckType == pgsTypes::sdtCompositeSIP);

   if ( deckType == pgsTypes::sdtCompositeOverlay )
      GetDlgItem(IDC_FILLET)->SetWindowText("0.00");

   UpdateSlabOffsetControls();

   GetDlgItem(IDC_SAMESLABOFFSET)->EnableWindow( deckType != pgsTypes::sdtNone );

   GetDlgItem(IDC_SLAB_FC_LABEL)->EnableWindow( deckType != pgsTypes::sdtNone);
   GetDlgItem(IDC_SLAB_FC)->EnableWindow(       deckType != pgsTypes::sdtNone);
   GetDlgItem(IDC_SLAB_FC_UNIT)->EnableWindow(  deckType != pgsTypes::sdtNone);
   GetDlgItem(IDC_MORE)->EnableWindow(  deckType != pgsTypes::sdtNone);
   
   GetDlgItem(IDC_ADD)->EnableWindow( deckType == pgsTypes::sdtCompositeCIP || deckType == pgsTypes::sdtCompositeSIP );
   GetDlgItem(IDC_REMOVE)->EnableWindow( deckType == pgsTypes::sdtCompositeCIP || deckType == pgsTypes::sdtCompositeSIP );
   m_Grid.Enable( deckType == pgsTypes::sdtCompositeCIP || deckType == pgsTypes::sdtCompositeSIP );

   GetDlgItem(IDC_SACDEPTH_LABEL)->EnableWindow( deckType != pgsTypes::sdtNone );
   GetDlgItem(IDC_SACDEPTH)->EnableWindow( deckType != pgsTypes::sdtNone );
   GetDlgItem(IDC_SACDEPTH_UNIT)->EnableWindow( deckType != pgsTypes::sdtNone );

   GetDlgItem(IDC_SIP_LABEL)->EnableWindow( deckType == pgsTypes::sdtCompositeSIP);

   GetDlgItem(IDC_PANEL_DEPTH_LABEL)->EnableWindow( deckType == pgsTypes::sdtCompositeSIP);
   GetDlgItem(IDC_PANEL_DEPTH)->EnableWindow(       deckType == pgsTypes::sdtCompositeSIP);
   GetDlgItem(IDC_PANEL_DEPTH_UNIT)->EnableWindow(  deckType == pgsTypes::sdtCompositeSIP);

   GetDlgItem(IDC_PANEL_SUPPORT_LABEL)->EnableWindow( deckType == pgsTypes::sdtCompositeSIP);
   GetDlgItem(IDC_PANEL_SUPPORT)->EnableWindow(       deckType == pgsTypes::sdtCompositeSIP);
   GetDlgItem(IDC_PANEL_SUPPORT_UNIT)->EnableWindow(  deckType == pgsTypes::sdtCompositeSIP);
	
   OnUserEc();
   OnWearingSurfaceTypeChanged();

   // blank out deck related inputs
   if ( deckType == pgsTypes::sdtNone )
   {
      GetDlgItem(IDC_GROSS_DEPTH)->SetWindowText("");
      GetDlgItem(IDC_OVERHANG_DEPTH)->SetWindowText("");
      GetDlgItem(IDC_FILLET)->SetWindowText("");
      GetDlgItem(IDC_ADIM)->SetWindowText("");
      GetDlgItem(IDC_SACDEPTH)->SetWindowText("");
      GetDlgItem(IDC_PANEL_DEPTH)->SetWindowText("");
      GetDlgItem(IDC_PANEL_SUPPORT)->SetWindowText("");
      GetDlgItem(IDC_SLAB_FC)->SetWindowText("");
      GetDlgItem(IDC_EC)->SetWindowText("");
      GetDlgItem(IDC_OVERHANG_TAPER)->SetWindowText(""); // this is a combobox
   }

   return CPropertyPage::OnSetActive();
}

void CBridgeDescDeckDetailsPage::OnUserEc()
{
   CBridgeDescDlg* pParent = (CBridgeDescDlg*)GetParent();
   ASSERT( pParent->IsKindOf(RUNTIME_CLASS(CBridgeDescDlg)) );

   BOOL bEnable = m_ctrlEcCheck.GetCheck();

   // can't be enabled if there is no deck
   pgsTypes::SupportedDeckType deckType = pParent->m_BridgeDesc.GetDeckDescription()->DeckType;

   if ( deckType == pgsTypes::sdtNone )
   {
      bEnable = FALSE;
      GetDlgItem(IDC_MOD_E)->EnableWindow(FALSE);
   }
   else
   {
      GetDlgItem(IDC_MOD_E)->EnableWindow(TRUE);

      if (bEnable==FALSE)
      {
         m_ctrlEc.GetWindowText(m_strUserEc);
         UpdateEc();
      }
      else
      {
         m_ctrlEc.SetWindowText(m_strUserEc);
      }
   }

   GetDlgItem(IDC_EC)->EnableWindow(bEnable);
   GetDlgItem(IDC_EC_UNIT)->EnableWindow(bEnable);
}

void CBridgeDescDeckDetailsPage::OnChangeSlabFc() 
{
   UpdateEc();
}

void CBridgeDescDeckDetailsPage::UpdateEc()
{
   CBridgeDescDlg* pParent = (CBridgeDescDlg*)GetParent();
   ASSERT( pParent->IsKindOf(RUNTIME_CLASS(CBridgeDescDlg)) );

   // update modulus
   if (m_ctrlEcCheck.GetCheck() == 0)
   {
      // blank out ec
      CString strEc;
      m_ctrlEc.SetWindowText(strEc);

      // need to manually parse strength and density values
      CString strFc, strDensity, strK1;
      m_ctrlFc.GetWindowText(strFc);

      CComPtr<IBroker> pBroker;
      AfxGetBroker(&pBroker);
      GET_IFACE2(pBroker,IDisplayUnits,pDispUnits);

      strDensity.Format("%s",FormatDimension(pParent->m_BridgeDesc.GetDeckDescription()->SlabStrengthDensity,pDispUnits->GetDensityUnit(),false));
      strK1.Format("%f",pParent->m_BridgeDesc.GetDeckDescription()->SlabK1);

      strEc = CConcreteDetailsDlg::UpdateEc(strFc,strDensity,strK1);
      m_ctrlEc.SetWindowText(strEc);
   }
}

void CBridgeDescDeckDetailsPage::OnWearingSurfaceTypeChanged()
{
   CComboBox* pCB = (CComboBox*)GetDlgItem(IDC_WEARINGSURFACETYPE);
   int idx = pCB->GetCurSel();

   BOOL bSacDepth;
   BOOL bOverlayLabel;
   BOOL bOverlayWeight;
   BOOL bOverlayDepthAndDensity;

   int iOption = GetCheckedRadioButton(IDC_OLAY_WEIGHT_LABEL,IDC_OLAY_DEPTH_LABEL);

   CBridgeDescDlg* pParent = (CBridgeDescDlg*)GetParent();
   ASSERT( pParent->IsKindOf(RUNTIME_CLASS(CBridgeDescDlg)) );
   pgsTypes::SupportedDeckType deckType = pParent->m_BridgeDesc.GetDeckDescription()->DeckType;

   pgsTypes::WearingSurfaceType ws = (pgsTypes::WearingSurfaceType)(pCB->GetItemData(idx));
   if ( ws == pgsTypes::wstSacrificialDepth )
   {
      bSacDepth               = deckType == pgsTypes::sdtNone ? FALSE : TRUE;
      bOverlayLabel           = FALSE;
      bOverlayWeight          = FALSE;
      bOverlayDepthAndDensity = FALSE;
   }
   else if ( ws == pgsTypes::wstFutureOverlay )
   {
      bSacDepth               = deckType == pgsTypes::sdtNone ? FALSE : TRUE;
      bOverlayLabel           = TRUE;
      bOverlayWeight          = (iOption == IDC_OLAY_WEIGHT_LABEL ? TRUE : FALSE);
      bOverlayDepthAndDensity = (iOption == IDC_OLAY_DEPTH_LABEL  ? TRUE : FALSE);
   }
   else
   {
      bSacDepth               = FALSE;
      bOverlayLabel           = TRUE;
      bOverlayWeight          = (iOption == IDC_OLAY_WEIGHT_LABEL ? TRUE : FALSE);
      bOverlayDepthAndDensity = (iOption == IDC_OLAY_DEPTH_LABEL  ? TRUE : FALSE);
   }

   GetDlgItem(IDC_OLAY_WEIGHT_LABEL)->EnableWindow( bOverlayLabel );
   GetDlgItem(IDC_OLAY_WEIGHT)->EnableWindow( bOverlayWeight );
   GetDlgItem(IDC_OLAY_WEIGHT_UNIT)->EnableWindow( bOverlayWeight );

   GetDlgItem(IDC_OLAY_DEPTH_LABEL)->EnableWindow( bOverlayLabel );
   GetDlgItem(IDC_OLAY_DEPTH)->EnableWindow( bOverlayDepthAndDensity );
   GetDlgItem(IDC_OLAY_DEPTH_UNIT)->EnableWindow( bOverlayDepthAndDensity );

   GetDlgItem(IDC_OLAY_DENSITY_LABEL)->EnableWindow( bOverlayDepthAndDensity );
   GetDlgItem(IDC_OLAY_DENSITY)->EnableWindow( bOverlayDepthAndDensity );
   GetDlgItem(IDC_OLAY_DENSITY_UNIT)->EnableWindow( bOverlayDepthAndDensity );

   GetDlgItem(IDC_SACDEPTH_LABEL)->EnableWindow( bSacDepth );
   GetDlgItem(IDC_SACDEPTH)->EnableWindow( bSacDepth );
   GetDlgItem(IDC_SACDEPTH_UNIT)->EnableWindow( bSacDepth );
}

void CBridgeDescDeckDetailsPage::OnMoreConcreteProperties() 
{
   CBridgeDescDlg* pParent = (CBridgeDescDlg*)GetParent();
   ASSERT( pParent->IsKindOf(RUNTIME_CLASS(CBridgeDescDlg)) );

   CConcreteDetailsDlg dlg;

   CDataExchange dx(this,TRUE);
   ExchangeConcreteData(&dx);

   dlg.m_Fc      = pParent->m_BridgeDesc.GetDeckDescription()->SlabFc;
   dlg.m_AggSize = pParent->m_BridgeDesc.GetDeckDescription()->SlabMaxAggregateSize;
   dlg.m_bUserEc = pParent->m_BridgeDesc.GetDeckDescription()->SlabUserEc;
   dlg.m_Ds      = pParent->m_BridgeDesc.GetDeckDescription()->SlabStrengthDensity;
   dlg.m_Dw      = pParent->m_BridgeDesc.GetDeckDescription()->SlabWeightDensity;
   dlg.m_Ec      = pParent->m_BridgeDesc.GetDeckDescription()->SlabEc;
   dlg.m_K1      = pParent->m_BridgeDesc.GetDeckDescription()->SlabK1;

   dlg.m_strUserEc  = m_strUserEc;

   if ( dlg.DoModal() == IDOK )
   {
      pParent->m_BridgeDesc.GetDeckDescription()->SlabFc               = dlg.m_Fc;
      pParent->m_BridgeDesc.GetDeckDescription()->SlabMaxAggregateSize = dlg.m_AggSize;
      pParent->m_BridgeDesc.GetDeckDescription()->SlabUserEc           = dlg.m_bUserEc;
      pParent->m_BridgeDesc.GetDeckDescription()->SlabStrengthDensity  = dlg.m_Ds;
      pParent->m_BridgeDesc.GetDeckDescription()->SlabWeightDensity    = dlg.m_Dw;
      pParent->m_BridgeDesc.GetDeckDescription()->SlabEc               = dlg.m_Ec;
      pParent->m_BridgeDesc.GetDeckDescription()->SlabK1               = dlg.m_K1;

      m_strUserEc  = dlg.m_strUserEc;
      m_ctrlEc.SetWindowText(m_strUserEc);

      UpdateConcreteControls();
   }
}

void CBridgeDescDeckDetailsPage::UpdateConcreteControls()
{
   // the concrete details were updated in the details dialog
   // update f'c and Ec in this dialog
   CDataExchange dx(this,FALSE);
   ExchangeConcreteData(&dx);
   UpdateEc();

   BOOL bEnable = m_ctrlEcCheck.GetCheck();
   GetDlgItem(IDC_EC)->EnableWindow(bEnable);
   GetDlgItem(IDC_EC_UNIT)->EnableWindow(bEnable);
}

BOOL CBridgeDescDeckDetailsPage::OnToolTipNotify(UINT id,NMHDR* pNMHDR, LRESULT* pResult)
{
   TOOLTIPTEXT* pTTT = (TOOLTIPTEXT*)pNMHDR;
   HWND hwndTool = (HWND)pNMHDR->idFrom;
   if ( pTTT->uFlags & TTF_IDISHWND )
   {
      // idFrom is actually HWND of tool
      UINT nID = ::GetDlgCtrlID(hwndTool);
      switch(nID)
      {
      case IDC_MORE:
         ::SendMessage(pNMHDR->hwndFrom,TTM_SETMAXTIPWIDTH,0,300); // makes it a multi-line tooltip
         UpdateConcreteParametersToolTip();
         break;

      default:
         return FALSE;
      }

      pTTT->lpszText = m_strTip.GetBuffer();
      pTTT->hinst = NULL;
      return TRUE;
   }
   return FALSE;
}

void CBridgeDescDeckDetailsPage::UpdateConcreteParametersToolTip()
{
   CBridgeDescDlg* pParent = (CBridgeDescDlg*)GetParent();
   ASSERT( pParent->IsKindOf(RUNTIME_CLASS(CBridgeDescDlg)) );

   CComPtr<IBroker> pBroker;
   AfxGetBroker(&pBroker);
   GET_IFACE2(pBroker,IDisplayUnits,pDisplayUnits);

   const unitmgtDensityData& density = pDisplayUnits->GetDensityUnit();
   const unitmgtLengthData&  aggsize = pDisplayUnits->GetComponentDimUnit();
   const unitmgtScalar&      scalar  = pDisplayUnits->GetScalarFormat();


   CString strTip;
   strTip.Format("%-20s %s\r\n%-20s %s\r\n%-20s %s",
      "Density for Strength",FormatDimension(pParent->m_BridgeDesc.GetDeckDescription()->SlabStrengthDensity,density),
      "Density for Weight",  FormatDimension(pParent->m_BridgeDesc.GetDeckDescription()->SlabWeightDensity,density),
      "Max Aggregate Size",  FormatDimension(pParent->m_BridgeDesc.GetDeckDescription()->SlabMaxAggregateSize,aggsize)
      );

   if ( lrfdVersionMgr::ThirdEditionWith2005Interims <= lrfdVersionMgr::GetVersion() )
   {
      // add K1 parameter
      CString strK1;
      strK1.Format("\r\n%-20s %s",
         "K1",FormatScalar(pParent->m_BridgeDesc.GetDeckDescription()->SlabK1,scalar));

      strTip += strK1;
   }

   CString strPress("\r\n\r\nPress button to edit");
   strTip += strPress;

   m_strTip = strTip;
}

void CBridgeDescDeckDetailsPage::OnAddDeckEdgePoint()
{
   CBridgeDescDlg* pParent = (CBridgeDescDlg*)GetParent();
   ASSERT( pParent->IsKindOf(RUNTIME_CLASS(CBridgeDescDlg)) );

   CDeckDescription* pDeck = pParent->m_BridgeDesc.GetDeckDescription();

   pDeck->DeckEdgePoints = m_Grid.GetEdgePoints();

   CDeckPoint newPoint;
   if ( pDeck->DeckEdgePoints.size() == 0 )
   {
      newPoint.LeftEdge = ::ConvertToSysUnits(15.0,unitMeasure::Feet);
      newPoint.RightEdge = ::ConvertToSysUnits(15.0,unitMeasure::Feet);
      newPoint.Station = ::ConvertToSysUnits(100.0,unitMeasure::Feet);
   }
   else
   {
      newPoint = pDeck->DeckEdgePoints.back();
      newPoint.Station += ::ConvertToSysUnits(100.0,unitMeasure::Feet);
   }
   pDeck->DeckEdgePoints.push_back(newPoint);

   m_Grid.FillGrid(pDeck);
}

void CBridgeDescDeckDetailsPage::OnRemoveDeckEdgePoint()
{
   m_Grid.RemoveSelectedRows();
}

void CBridgeDescDeckDetailsPage::EnableRemove(BOOL bEnable)
{
   GetDlgItem(IDC_REMOVE)->EnableWindow(bEnable);
}

void CBridgeDescDeckDetailsPage::OnBnClickedOlayWeightLabel()
{
   OnWearingSurfaceTypeChanged();
}

void CBridgeDescDeckDetailsPage::OnBnClickedOlayDepthLabel()
{
   OnWearingSurfaceTypeChanged();
}

void CBridgeDescDeckDetailsPage::OnBnClickedSameslaboffset()
{
   BOOL bEnable = IsDlgButtonChecked(IDC_SAMESLABOFFSET);
   CString strHint;
   if ( bEnable )
   {
      strHint = "By checking this box, the same slab offset will be used for the entire bridge";
   }
   else
   {
      strHint = "By unchecking this box, the slab offset can be specified by span or by girder. Span by span slab offsets can be defined on either the Span or Pier Connections page. Girder by girder slab offsets can be defined on the Girder Spacing or Girder dialogs";
   }

   // Show the hint dialog
   UIHint(strHint,UIHINT_SINGLE_SLAB_OFFSET);

   CWnd* pWnd = GetDlgItem(IDC_ADIM);
   if ( bEnable )   
   {
      // box was checked on so put the cache value into the window
      pWnd->SetWindowText(m_strSlabOffsetCache);
   }
   else
   {
      // box was checked off so read the current value from the window and cache it
      pWnd->GetWindowText(m_strSlabOffsetCache);
      pWnd->SetWindowText("");
   }

   UpdateSlabOffsetControls();


}

void CBridgeDescDeckDetailsPage::UpdateSlabOffsetControls()
{
   BOOL bEnable = IsDlgButtonChecked(IDC_SAMESLABOFFSET) ? TRUE : FALSE;

   // enable/disable slab offset controls
   CBridgeDescDlg* pParent = (CBridgeDescDlg*)GetParent();
   ASSERT( pParent->IsKindOf(RUNTIME_CLASS(CBridgeDescDlg)) );
   pgsTypes::SupportedDeckType deckType = pParent->m_BridgeDesc.GetDeckDescription()->DeckType;

   if ( bEnable )
      bEnable = (deckType != pgsTypes::sdtNone); // if enabled, disable if deck type is none

   GetDlgItem(IDC_ADIM_LABEL)->EnableWindow(bEnable);
   GetDlgItem(IDC_ADIM)->EnableWindow(bEnable);
   GetDlgItem(IDC_ADIM_UNIT)->EnableWindow(bEnable);
}

void CBridgeDescDeckDetailsPage::UIHint(const CString& strText,UINT mask)
{
   CPGSuperApp* pApp = (CPGSuperApp*)AfxGetApp();
   Uint32 hintSettings = pApp->GetUIHintSettings();
   if ( sysFlags<Uint32>::IsClear(hintSettings,mask) )
   {
      CUIHintsDlg dlg;
      dlg.m_strTitle = "Hint";
      dlg.m_strText = strText;
      dlg.DoModal();
      if ( dlg.m_bDontShowAgain )
      {
         sysFlags<Uint32>::Set(&hintSettings,mask);
         pApp->SetUIHintSettings(hintSettings);
      }
   }
}
