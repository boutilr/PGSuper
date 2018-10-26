///////////////////////////////////////////////////////////////////////
// PGSuper - Prestressed Girder SUPERstructure Design and Analysis
// Copyright � 1999-2010  Washington State Department of Transportation
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

// BridgeDescGeneralPage.cpp : implementation file
//

#include "stdafx.h"
#include "pgsuper.h"
#include "PGSuperUnits.h"
#include "BridgeDescGeneralPage.h"
#include "BridgeDescDlg.h"
#include "UIHintsDlg.h"
#include "Hints.h"
#include <MfcTools\CustomDDX.h>

#include "HtmlHelp\HelpTopics.hh"

#include <IFace\Project.h>
#include <IFace\Bridge.h>
#include <IFace\DisplayUnits.h>
#include <IFace\DistFactorEngineer.h>

#include <algorithm>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CBridgeDescGeneralPage property page

IMPLEMENT_DYNCREATE(CBridgeDescGeneralPage, CPropertyPage)

CBridgeDescGeneralPage::CBridgeDescGeneralPage() : CPropertyPage(CBridgeDescGeneralPage::IDD)
{
	//{{AFX_DATA_INIT(CBridgeDescGeneralPage)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT

   m_MinGirderCount = 2;
   m_CacheGirderConnectivityIdx = CB_ERR;
   m_CacheDeckTypeIdx = CB_ERR;

   m_bSetActive = false;
}

CBridgeDescGeneralPage::~CBridgeDescGeneralPage()
{
}

void CBridgeDescGeneralPage::DoDataExchange(CDataExchange* pDX)
{
   CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CBridgeDescGeneralPage)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	DDX_Control(pDX, IDC_NUMGDR_SPIN, m_NumGdrSpinner);
	//}}AFX_DATA_MAP

   ////////////////////////////////////////////////
   // Girders
   ////////////////////////////////////////////////
   if (!pDX->m_bSaveAndValidate)
   {
      FillGirderSpacingMeasurementComboBox();
   }


   DDX_Check_Bool(pDX, IDC_SAME_NUM_GIRDERLINES, m_bSameNumberOfGirders);
   DDX_Check_Bool(pDX, IDC_SAME_GIRDERNAME,      m_bSameGirderName);

   DDX_CBStringExactCase(pDX, IDC_BEAM_FAMILIES, m_GirderFamilyName );
   DDX_CBStringExactCase(pDX, IDC_GDR_TYPE, m_GirderName );
   DDX_CBItemData(pDX, IDC_GIRDER_ORIENTATION,  m_GirderOrientation);
   DDX_CBItemData(pDX, IDC_GIRDER_SPACING_TYPE, m_GirderSpacingType);

   
   if ( m_bSameNumberOfGirders )
   {
      DDX_Text(pDX, IDC_NUMGDR, m_nGirders );
      DDV_MinMaxLong(pDX, m_nGirders, m_MinGirderCount, MAX_GIRDERS_PER_SPAN );
   }

   CComPtr<IBroker> pBroker;
   AfxGetBroker(&pBroker);
   GET_IFACE2(pBroker,IDisplayUnits,pDisplayUnits);

   if ( IsGirderSpacing(m_GirderSpacingType) )
   {
      // girder spacing
      DDX_Tag(pDX,IDC_SPACING_UNIT,pDisplayUnits->GetXSectionDimUnit());
      if ( !pDX->m_bSaveAndValidate || (pDX->m_bSaveAndValidate && (m_GirderSpacingType == pgsTypes::sbsUniform || m_GirderSpacingType == pgsTypes::sbsConstantAdjacent)) )
      {
         DDX_UnitValueAndTag(pDX,IDC_SPACING,IDC_SPACING_UNIT,m_GirderSpacing,pDisplayUnits->GetXSectionDimUnit());
      }
   }
   else
   {
      // joint spacing
      DDX_Tag(pDX,IDC_SPACING_UNIT,pDisplayUnits->GetComponentDimUnit());
      if ( !pDX->m_bSaveAndValidate || (pDX->m_bSaveAndValidate && m_GirderSpacingType == pgsTypes::sbsUniformAdjacent) )
         DDX_UnitValueAndTag(pDX,IDC_SPACING,IDC_SPACING_UNIT,m_GirderSpacing,pDisplayUnits->GetComponentDimUnit());
   }

   if ( m_GirderSpacingType == pgsTypes::sbsUniform || m_GirderSpacingType == pgsTypes::sbsConstantAdjacent )
   {
      // check girder spacing
      if ( IsEqual(m_GirderSpacing,m_MinGirderSpacing) )
         m_GirderSpacing = m_MinGirderSpacing;

      if ( IsEqual(m_GirderSpacing,m_MaxGirderSpacing) )
         m_GirderSpacing = m_MaxGirderSpacing;

      DDV_UnitValueLimitOrMore(pDX, m_GirderSpacing, m_MinGirderSpacing, pDisplayUnits->GetXSectionDimUnit() );
      DDV_UnitValueLimitOrLess(pDX, m_GirderSpacing, m_MaxGirderSpacing, pDisplayUnits->GetXSectionDimUnit() );
   }
   else if ( m_GirderSpacingType == pgsTypes::sbsUniformAdjacent )
   {
      // check joint spacing
      DDV_UnitValueLimitOrLess(pDX, m_GirderSpacing, m_MaxGirderSpacing-m_MinGirderSpacing, pDisplayUnits->GetComponentDimUnit() );
   }

   if ( pDX->m_bSaveAndValidate )
   {
      DWORD dw;
      DDX_CBItemData(pDX, IDC_GIRDER_SPACING_MEASURE, dw);
      UnhashGirderSpacing(dw,&m_GirderSpacingMeasurementLocation,&m_GirderSpacingMeasurementType);
   }
   else
   {
      DWORD dw = HashGirderSpacing(m_GirderSpacingMeasurementLocation,m_GirderSpacingMeasurementType);
      DDX_CBItemData(pDX, IDC_GIRDER_SPACING_MEASURE, dw);
   }

   DDX_CBItemData(pDX,IDC_REF_GIRDER,m_RefGirderIdx);
   DDX_OffsetAndTag(pDX,IDC_REF_GIRDER_OFFSET,IDC_REF_GIRDER_OFFSET_UNIT,m_RefGirderOffset,pDisplayUnits->GetXSectionDimUnit() );
   DDX_CBItemData(pDX,IDC_REF_GIRDER_OFFSET_TYPE,m_RefGirderOffsetType);

   ////////////////////////////////////////////////
   // Deck and Railing System
   ////////////////////////////////////////////////
	DDX_CBItemData(pDX, IDC_DECK_TYPE, m_Deck.DeckType);
 	DDX_CBItemData(pDX, IDC_GIRDER_CONNECTIVITY, m_Deck.TransverseConnectivity);

   if ( pDX->m_bSaveAndValidate )
   {
      UpdateBridgeDescription();
   }
}


BEGIN_MESSAGE_MAP(CBridgeDescGeneralPage, CPropertyPage)
	//{{AFX_MSG_MAP(CBridgeDescGeneralPage)
	ON_BN_CLICKED(IDC_SAME_NUM_GIRDERLINES, OnSameNumGirders)
	ON_BN_CLICKED(IDC_SAME_GIRDERNAME, OnSameGirderName)
	ON_NOTIFY(UDN_DELTAPOS, IDC_NUMGDR_SPIN, OnNumGirdersChanged)
	ON_CBN_SELCHANGE(IDC_BEAM_FAMILIES, OnGirderFamilyChanged)
	ON_CBN_SELCHANGE(IDC_GDR_TYPE, OnGirderNameChanged)
	ON_CBN_SELCHANGE(IDC_DECK_TYPE, OnDeckTypeChanged)
	ON_CBN_SELCHANGE(IDC_GIRDER_CONNECTIVITY, OnGirderConnectivityChanged)
   ON_CBN_SELCHANGE(IDC_GIRDER_SPACING_MEASURE,OnSpacingDatumChanged)
   ON_CBN_SELCHANGE(IDC_GIRDER_SPACING_TYPE,OnGirderSpacingTypeChanged)
   ON_NOTIFY_EX(TTN_NEEDTEXT,0,OnToolTipNotify)
	ON_COMMAND(ID_HELP, OnHelp)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CBridgeDescGeneralPage message handlers

BOOL CBridgeDescGeneralPage::OnInitDialog() 
{
   EnableToolTips(TRUE);

   Init();

   FillGirderFamilyComboBox();

   FillGirderNameComboBox();

   FillGirderOrientationComboBox();

   FillGirderSpacingTypeComboBox();

   FillDeckTypeComboBox();

   FillGirderConnectivityComboBox();

   FillRefGirderOffsetTypeComboBox();
   
   FillRefGirderComboBox();

   UpdateGirderSpacingLimits();

   if ( IsGirderSpacing(m_GirderSpacingType) )
      EnableGirderSpacing(FALSE,FALSE);

	CPropertyPage::OnInitDialog();
   
   m_NumGdrSpinner.SetRange32(m_MinGirderCount,MAX_GIRDERS_PER_SPAN);
   UDACCEL accel[2];
   accel[0].nInc = 1;
   accel[0].nSec = 5;
   accel[1].nInc = 5;
   accel[1].nSec = 5;
   m_NumGdrSpinner.SetAccel(2,accel);

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CBridgeDescGeneralPage::Init()
{
   // initialize the page data members with values from the bridge model
   CBridgeDescDlg* pParent = (CBridgeDescDlg*)GetParent();

   CComPtr<IBroker> pBroker;
   AfxGetBroker(&pBroker);
   GET_IFACE2(pBroker, IDisplayUnits, pDisplayUnits);

   m_bSameNumberOfGirders = pParent->m_BridgeDesc.UseSameNumberOfGirdersInAllSpans();
   m_bSameGirderName      = pParent->m_BridgeDesc.UseSameGirderForEntireBridge();
   m_nGirders = pParent->m_BridgeDesc.GetSpan(0)->GetGirderCount();

   m_strCacheNumGirders.Format("%d",m_nGirders);

   m_GirderFamilyName = pParent->m_BridgeDesc.GetGirderFamilyName();

   if ( m_bSameGirderName )
   {
      m_GirderName = pParent->m_BridgeDesc.GetGirderName();
   }
   else
   {
      ASSERT(1 <= pParent->m_BridgeDesc.GetSpan(0)->GetGirderTypes()->GetGirderGroupCount());
      m_GirderName = pParent->m_BridgeDesc.GetSpan(0)->GetGirderTypes()->GetGirderName(0);
   }
   UpdateGirderFactory();

   m_GirderOrientation = pParent->m_BridgeDesc.GetGirderOrientation();
   m_GirderSpacingType = pParent->m_BridgeDesc.GetGirderSpacingType();

   if ( IsBridgeSpacing(m_GirderSpacingType) )
   {
      m_GirderSpacing                    = pParent->m_BridgeDesc.GetGirderSpacing();
      m_GirderSpacingMeasurementLocation = pParent->m_BridgeDesc.GetMeasurementLocation();
      m_GirderSpacingMeasurementType     = pParent->m_BridgeDesc.GetMeasurementType();

      m_RefGirderIdx        = pParent->m_BridgeDesc.GetRefGirder();
      m_RefGirderOffset     = pParent->m_BridgeDesc.GetRefGirderOffset();
      m_RefGirderOffsetType = pParent->m_BridgeDesc.GetRefGirderOffsetType();

   }
   else
   {
      const CSpanData* pSpan = pParent->m_BridgeDesc.GetSpan(0);
      const CGirderSpacing* pSpacing = pSpan->GetGirderSpacing(pgsTypes::metStart);

      m_GirderSpacing                    = pSpacing->GetGirderSpacing(0);
      m_GirderSpacingMeasurementLocation = pSpacing->GetMeasurementLocation();
      m_GirderSpacingMeasurementType     = pSpacing->GetMeasurementType();

      m_RefGirderIdx        = pSpacing->GetRefGirder();
      m_RefGirderOffset     = pSpacing->GetRefGirderOffset();
      m_RefGirderOffsetType = pSpacing->GetRefGirderOffsetType();
   }

   if ( IsGirderSpacing(m_GirderSpacingType) )
   {
      m_strCacheGirderSpacing.Format("%s",FormatDimension(m_GirderSpacing,pDisplayUnits->GetXSectionDimUnit(), false));
      m_strCacheJointSpacing.Format( "%s",FormatDimension(0,              pDisplayUnits->GetComponentDimUnit(),false));
   }
   else
   {
      m_strCacheGirderSpacing.Format("%s",FormatDimension(m_MinGirderSpacing,pDisplayUnits->GetXSectionDimUnit(), false));
      m_strCacheJointSpacing.Format( "%s",FormatDimension(m_GirderSpacing,   pDisplayUnits->GetComponentDimUnit(),false));
   }

   int sign = ::Sign(m_RefGirderOffset);
   char* strOffset = (sign == 0 ? "" : sign < 0 ? "L" : "R");
   m_strCacheRefGirderOffset.Format("%s %s",FormatDimension(fabs(m_RefGirderOffset),pDisplayUnits->GetXSectionDimUnit(),false),strOffset);

   m_Deck = *pParent->m_BridgeDesc.GetDeckDescription();
   m_CacheDeckEdgePoints = m_Deck.DeckEdgePoints;

   UpdateMinimumGirderCount();
}

void CBridgeDescGeneralPage::UpdateBridgeDescription()
{
   // put the page data values into the bridge model
   CBridgeDescDlg* pParent = (CBridgeDescDlg*)GetParent();

   pParent->m_BridgeDesc.UseSameNumberOfGirdersInAllSpans( m_bSameNumberOfGirders );
   pParent->m_BridgeDesc.UseSameGirderForEntireBridge( m_bSameGirderName );
   pParent->m_BridgeDesc.SetGirderCount(m_nGirders);


   bool bNewGirderFamily = (pParent->m_BridgeDesc.GetGirderFamilyName() != m_GirderFamilyName);

   pParent->m_BridgeDesc.SetGirderFamilyName(m_GirderFamilyName);
   pParent->m_BridgeDesc.SetGirderOrientation(m_GirderOrientation);

   if ( m_bSameGirderName || bNewGirderFamily )
   {
      pParent->m_BridgeDesc.SetGirderName(m_GirderName);

      CComPtr<IBroker> pBroker;
      AfxGetBroker(&pBroker);
      GET_IFACE2( pBroker, ILibrary, pLib );
   
      const GirderLibraryEntry* pGdrEntry = pLib->GetGirderEntry(m_GirderName);
      pParent->m_BridgeDesc.SetGirderLibraryEntry(pGdrEntry);
   }

   pParent->m_BridgeDesc.SetGirderSpacingType(m_GirderSpacingType);
   pParent->m_BridgeDesc.SetGirderSpacing(m_GirderSpacing);

   pParent->m_BridgeDesc.SetMeasurementLocation(m_GirderSpacingMeasurementLocation);
   pParent->m_BridgeDesc.SetMeasurementType(m_GirderSpacingMeasurementType);

   pParent->m_BridgeDesc.SetRefGirder(m_RefGirderIdx);
   pParent->m_BridgeDesc.SetRefGirderOffset(m_RefGirderOffset);
   pParent->m_BridgeDesc.SetRefGirderOffsetType(m_RefGirderOffsetType);

   *pParent->m_BridgeDesc.GetDeckDescription() = m_Deck;

   if ( bNewGirderFamily )
      pParent->m_BridgeDesc.CopyDown(false,true,false,false);
}

void CBridgeDescGeneralPage::OnSameNumGirders() 
{
	CButton* pBtn = (CButton*)GetDlgItem(IDC_SAME_NUM_GIRDERLINES);

   // grab the new state of the check box
   m_bSameNumberOfGirders = (pBtn->GetCheck() == BST_CHECKED ? true : false);

   CEdit* pEdit = (CEdit*)GetDlgItem(IDC_NUMGDR);
   CString strText; // hint dialog text
   if ( m_bSameNumberOfGirders )
   {
      // box was just checked so restore the number of girders from the cache
      pEdit->SetWindowText(m_strCacheNumGirders);
      strText = CString("By checking this box, all spans will have the same number of girders.");
   }
   else
   {
      // box was just unchecked so cache the number of girders
      pEdit->GetWindowText(m_strCacheNumGirders);
      strText = CString("By unchecking this box, each span can have a different number of girders. To define the number of girders in a span, edit the Span Details for each span.\n\nSpan Details can be edited by selecting the Framing tab and then pressing the edit button for a span.");
   }

   // enable/disable the associated controls
   EnableNumGirderLines(m_bSameNumberOfGirders);

   UpdateBridgeDescription();

   // Show the hint dialog
   UIHint(strText,UIHINT_SAME_NUMBER_OF_GIRDERS);
}

void CBridgeDescGeneralPage::EnableNumGirderLines(BOOL bEnable)
{
   CEdit* pEdit = (CEdit*)GetDlgItem(IDC_NUMGDR);
   pEdit->EnableWindow(bEnable);

   if ( !bEnable )
   {
      pEdit->SetSel(0,-1);
      pEdit->Clear();
   }
   
   GetDlgItem(IDC_NUMGDR_SPIN)->EnableWindow(bEnable);
}

void CBridgeDescGeneralPage::OnSameGirderName() 
{
	CButton* pBtn = (CButton*)GetDlgItem(IDC_SAME_GIRDERNAME);

   m_bSameGirderName = (pBtn->GetCheck() == BST_CHECKED ? true : false);

   CComboBox* pcbGirderName = (CComboBox*)GetDlgItem(IDC_GDR_TYPE);
   CString strText;
   if ( m_bSameGirderName )
   {
      // button was just checked
      pcbGirderName->SetCurSel(m_CacheGirderNameIdx);
      strText = CString("By checking this box, the same girder type will be used throughout the bridge.");
   }
   else
   {
      // was was just unchecked
      m_CacheGirderNameIdx = pcbGirderName->GetCurSel();
      strText = CString("By unchecking this box, a different girder can be assigned to each girder line. To do this, edit the Span Details for each span.\n\nSpan Details can be edited by selecting the Framing tab and then pressing the edit button for a span.");
   }

   EnableGirderName(m_bSameGirderName);
   UpdateGirderSpacingLimits();

   UpdateBridgeDescription();

   // Show the hint dialog
   UIHint(strText,UIHINT_SAME_GIRDER_NAME);
}

void CBridgeDescGeneralPage::EnableGirderName(BOOL bEnable)
{
   CComboBox* pcbGirderName        = (CComboBox*)GetDlgItem(IDC_GDR_TYPE);

   pcbGirderName->EnableWindow(bEnable);

   if ( !bEnable )
   {
      pcbGirderName->SetCurSel(-1);
   }
}

void CBridgeDescGeneralPage::EnableGirderSpacing(BOOL bEnable,BOOL bClearControls)
{
   CEdit* pEdit = (CEdit*)GetDlgItem(IDC_SPACING);
   pEdit->EnableWindow(bEnable);

   CWnd* pTag = GetDlgItem(IDC_SPACING_UNIT);
   pTag->EnableWindow(bEnable);
   pTag->ShowWindow(bClearControls ? SW_HIDE : SW_SHOW);

   CWnd* pAllowable = GetDlgItem(IDC_ALLOWABLE_SPACING);
   pAllowable->EnableWindow(bEnable);
   pAllowable->ShowWindow(bClearControls ? SW_HIDE : SW_SHOW);

   CComboBox* pcbGirderMeasure = (CComboBox*)GetDlgItem(IDC_GIRDER_SPACING_MEASURE);
   pcbGirderMeasure->EnableWindow(bEnable);
   pcbGirderMeasure->ShowWindow(bClearControls ? SW_HIDE : SW_SHOW);

   CComboBox* pcbRefGirder = (CComboBox*)GetDlgItem(IDC_REF_GIRDER);
   pcbRefGirder->EnableWindow(bEnable);
   pcbRefGirder->ShowWindow(bClearControls ? SW_HIDE : SW_SHOW);

   CEdit* pRefGirderOffset = (CEdit*)GetDlgItem(IDC_REF_GIRDER_OFFSET);
   pRefGirderOffset->EnableWindow(bEnable);
   pRefGirderOffset->ShowWindow(bClearControls ? SW_HIDE : SW_SHOW);

   CWnd* pRefGirderOffsetTag = GetDlgItem(IDC_REF_GIRDER_OFFSET_UNIT);
   pRefGirderOffsetTag->EnableWindow(bEnable);
   pRefGirderOffsetTag->ShowWindow(bClearControls ? SW_HIDE : SW_SHOW);

   CComboBox* pcbRefGirderOffsetType = (CComboBox*)GetDlgItem(IDC_REF_GIRDER_OFFSET_TYPE);
   pcbRefGirderOffsetType->EnableWindow(bEnable);
   pcbRefGirderOffsetType->ShowWindow(bClearControls ? SW_HIDE : SW_SHOW);

   if ( !bEnable && bClearControls )
   {
      pEdit->SetSel(0,-1);
      pEdit->Clear();

      pRefGirderOffset->SetSel(0,-1);
      pRefGirderOffset->Clear();

      pcbGirderMeasure->SetCurSel(-1);
      pcbRefGirder->SetCurSel(-1);
      pcbRefGirderOffsetType->SetCurSel(-1);
   }
}

void CBridgeDescGeneralPage::OnNumGirdersChanged(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_UPDOWN* pNMUpDown = (NM_UPDOWN*)pNMHDR;

   // this is what the count will be
   int new_count = pNMUpDown->iPos + pNMUpDown->iDelta;

   // if it goes over the top
   if ( MAX_GIRDERS_PER_SPAN < new_count )
      new_count = m_MinGirderCount; // make it the min value

   // if it goes under the bottom
   if ( new_count < int(m_MinGirderCount) )
      new_count = MAX_GIRDERS_PER_SPAN; // make it the max value

   m_nGirders = new_count;

   // disable girder spacing if girder count is 1
   // this could happen for U-beams, Voided Slabs, Triple-Tees, or other multi-web beams
   if ( IsBridgeSpacing(m_GirderSpacingType) )
      EnableGirderSpacing(new_count != 1,FALSE);

   *pResult = 0;

   FillRefGirderComboBox();
   UpdateBridgeDescription();
}

void CBridgeDescGeneralPage::FillGirderFamilyComboBox()
{
   CComPtr<IBroker> pBroker;
   AfxGetBroker(&pBroker);
   GET_IFACE2( pBroker, ILibraryNames, pLibNames );
   std::vector<std::string> names;
   std::vector<std::string>::iterator iter;

   CComboBox* pcbGirderFamilies = (CComboBox*)GetDlgItem(IDC_BEAM_FAMILIES);
   std::vector<std::string> girderFamilyNames;
   pLibNames->EnumGirderFamilyNames(&girderFamilyNames);

   for ( iter = girderFamilyNames.begin(); iter != girderFamilyNames.end(); iter++ )
   {
      std::string strGirderFamilyName = *iter;
      pLibNames->EnumGirderNames(strGirderFamilyName.c_str(), &names );

      // only add a girder family name into the combo box if the library
      // has girders of this type
      if ( names.size() != 0 )
      {
         pcbGirderFamilies->AddString(strGirderFamilyName.c_str());
      }
   }

   int curSel = pcbGirderFamilies->FindStringExact(-1,m_GirderFamilyName);
   pcbGirderFamilies->SetCurSel(curSel);
}

void CBridgeDescGeneralPage::FillGirderNameComboBox()
{
   CComPtr<IBroker> pBroker;
   AfxGetBroker(&pBroker);

   GET_IFACE2( pBroker, ILibraryNames, pLibNames );
   std::vector<std::string> names;
   std::vector<std::string>::iterator iter;
   
   CComboBox* pcbGirders = (CComboBox*)GetDlgItem( IDC_GDR_TYPE );
   pcbGirders->ResetContent();

   int curSel = CB_ERR;
   pLibNames->EnumGirderNames(m_GirderFamilyName, &names );
   for ( iter = names.begin(); iter < names.end(); iter++ )
   {
      std::string& name = *iter;

      int idx = pcbGirders->AddString( name.c_str() );
      if ( CString(name.c_str()) == m_GirderName )
         curSel = idx;
   }

   m_CacheGirderNameIdx = (curSel == CB_ERR ? 0 : curSel);
   pcbGirders->SetCurSel(m_CacheGirderNameIdx);
}

void CBridgeDescGeneralPage::FillGirderOrientationComboBox()
{
   // Girder Orientation
   CComboBox* pOrientation = (CComboBox*)GetDlgItem(IDC_GIRDER_ORIENTATION);
   int idx = pOrientation->AddString("Plumb"); 
   pOrientation->SetItemData(idx,pgsTypes::Plumb);

   idx = pOrientation->AddString("Normal to roadway at Start of Bridge");
   pOrientation->SetItemData(idx,pgsTypes::StartNormal);

   idx = pOrientation->AddString("Normal to roadway at Mid-span of Bridge");
   pOrientation->SetItemData(idx,pgsTypes::MidspanNormal);

   idx = pOrientation->AddString("Normal to roadway at End of Bridge");
   pOrientation->SetItemData(idx,pgsTypes::EndNormal);

   int curSel;
   switch( m_GirderOrientation )
   {
   case pgsTypes::Plumb:
      curSel = 0;
      break;

   case pgsTypes::StartNormal:
      curSel = 1;
      break;

   case pgsTypes::MidspanNormal:
      curSel = 2;
      break;

   case pgsTypes::EndNormal:
      curSel = 3;
      break;

   default:
      ASSERT(FALSE); // shouldn't get here
   }

   pOrientation->SetCurSel(curSel);
}

void CBridgeDescGeneralPage::FillGirderSpacingTypeComboBox()
{
   CComboBox* pSpacingType = (CComboBox*)GetDlgItem(IDC_GIRDER_SPACING_TYPE);
   pSpacingType->ResetContent();

   pgsTypes::SupportedBeamSpacings sbs = m_Factory->GetSupportedBeamSpacings();
   pgsTypes::SupportedBeamSpacings::iterator iter;
   int idx;
   int curSel = CB_ERR;
   for ( iter = sbs.begin(); iter != sbs.end(); iter++ )
   {
      pgsTypes::SupportedBeamSpacing spacingType = *iter;

      switch( spacingType )
      {
      case pgsTypes::sbsUniform:
         idx = pSpacingType->AddString("Spread girders with same spacing in all spans");
         pSpacingType->SetItemData(idx,(DWORD)spacingType);
         break;

      case pgsTypes::sbsGeneral:
         idx = pSpacingType->AddString("Spread girders with unique spacing for each span");
         pSpacingType->SetItemData(idx,(DWORD)spacingType);
         break;

      case pgsTypes::sbsUniformAdjacent:
         idx = pSpacingType->AddString("Adjacent girders with same joint spacing in all spans");
         pSpacingType->SetItemData(idx,(DWORD)spacingType);
         break;

      case pgsTypes::sbsGeneralAdjacent:
         idx = pSpacingType->AddString("Adjacent girders with unique joint spacing for each span");
         pSpacingType->SetItemData(idx,(DWORD)spacingType);
         break;

      case pgsTypes::sbsConstantAdjacent:
         idx = pSpacingType->AddString("Adjacent girders with the same width for all girders");
         pSpacingType->SetItemData(idx,(DWORD)spacingType);
         break;

      default:
         ATLASSERT(false); // is there a new spacing type???
      }

      if ( m_GirderSpacingType == spacingType )
         curSel = idx;
   }

   if ( curSel == CB_ERR )
   {
      pSpacingType->SetCurSel(0);
      m_GirderSpacingType = sbs.front();
   }
   else
   {
      pSpacingType->SetCurSel(curSel);
      m_GirderSpacingType = sbs[curSel];
   }

   // update the unit tag that goes with the spacing input box
   CComPtr<IBroker> pBroker;
   AfxGetBroker(&pBroker);
   GET_IFACE2(pBroker,IDisplayUnits,pDisplayUnits);
   CDataExchange dx(this,FALSE);
   if ( IsGirderSpacing(m_GirderSpacingType) )
   {
      DDX_Tag(&dx,IDC_SPACING_UNIT,pDisplayUnits->GetXSectionDimUnit());
   }
   else
   {
      ASSERT(IsJointSpacing(m_GirderSpacingType));
      DDX_Tag(&dx,IDC_SPACING_UNIT,pDisplayUnits->GetComponentDimUnit());
   }
}

void CBridgeDescGeneralPage::FillGirderSpacingMeasurementComboBox()
{
   m_CacheGirderSpacingMeasureIdx = 0;

   CComboBox* pSpacingType = (CComboBox*)GetDlgItem(IDC_GIRDER_SPACING_MEASURE);
   pSpacingType->ResetContent();

   DWORD current_value = HashGirderSpacing(m_GirderSpacingMeasurementLocation,m_GirderSpacingMeasurementType);

   int idx = pSpacingType->AddString("Measured at and along the CL pier");
   DWORD item_data = HashGirderSpacing(pgsTypes::AtCenterlinePier,pgsTypes::AlongItem);
   pSpacingType->SetItemData(idx,item_data);
   m_CacheGirderSpacingMeasureIdx = (item_data == current_value ? idx : m_CacheGirderSpacingMeasureIdx );
   
   idx = pSpacingType->AddString("Measured normal to alignment at CL pier");
   item_data = HashGirderSpacing(pgsTypes::AtCenterlinePier,pgsTypes::NormalToItem);
   pSpacingType->SetItemData(idx,item_data);
   m_CacheGirderSpacingMeasureIdx = (item_data == current_value ? idx : m_CacheGirderSpacingMeasureIdx );

   // Cannot measure along bearing if any bearings are measured along girder
   if (!AreAnyBearingsMeasuredAlongGirder())
   {
      idx = pSpacingType->AddString("Measured at and along the CL bearing");
      item_data = HashGirderSpacing(pgsTypes::AtCenterlineBearing,pgsTypes::AlongItem);
      pSpacingType->SetItemData(idx,item_data);
      m_CacheGirderSpacingMeasureIdx = (item_data == current_value ? idx : m_CacheGirderSpacingMeasureIdx );

      idx = pSpacingType->AddString("Measured normal to alignment at CL bearing");
      item_data = HashGirderSpacing(pgsTypes::AtCenterlineBearing,pgsTypes::NormalToItem);
      pSpacingType->SetItemData(idx,item_data);
      m_CacheGirderSpacingMeasureIdx = (item_data == current_value ? idx : m_CacheGirderSpacingMeasureIdx );
   }
}

bool CBridgeDescGeneralPage::AreAnyBearingsMeasuredAlongGirder()
{
   CComPtr<IBroker> pBroker;
   AfxGetBroker(&pBroker);
   GET_IFACE2( pBroker, ILibrary, pLib );
   
   bool test=false;
   CBridgeDescDlg* pParent = (CBridgeDescDlg*)GetParent();

   SpanIndexType nSpans   = pParent->m_BridgeDesc.GetSpanCount();
   for ( SpanIndexType spanIdx = 0; spanIdx < nSpans; spanIdx++ )
   {
      CSpanData* pSpan = pParent->m_BridgeDesc.GetSpan(spanIdx);
      const CPierData* pPrevPier = pSpan->GetPrevPier();
      const CPierData* pNextPier = pSpan->GetNextPier();

      std::string start_connection = pPrevPier->GetConnection(pgsTypes::Ahead);
      std::string end_connection   = pNextPier->GetConnection(pgsTypes::Back);

      const ConnectionLibraryEntry* start_connection_entry = pLib->GetConnectionEntry(start_connection.c_str());
      const ConnectionLibraryEntry*   end_connection_entry = pLib->GetConnectionEntry(  end_connection.c_str());

      ConnectionLibraryEntry::BearingOffsetMeasurementType mbs = start_connection_entry->GetBearingOffsetMeasurementType();
      ConnectionLibraryEntry::BearingOffsetMeasurementType mbe =   end_connection_entry->GetBearingOffsetMeasurementType();
      if (mbs==ConnectionLibraryEntry::AlongGirder || mbe==ConnectionLibraryEntry::AlongGirder )
      {
         test = true;
         break;
      }
   }

   return test;
}

void CBridgeDescGeneralPage::FillDeckTypeComboBox()
{
   pgsTypes::SupportedDeckTypes deckTypes = m_Factory->GetSupportedDeckTypes(m_GirderSpacingType);

   CComboBox* pcbDeck = (CComboBox*)GetDlgItem(IDC_DECK_TYPE);
   pcbDeck->ResetContent();

   int cursel = CB_ERR;
   pgsTypes::SupportedDeckTypes::iterator iter;
   int selidx = 0;
   for ( iter = deckTypes.begin(); iter != deckTypes.end(); iter++ )
   {
      CString typestr = GetDeckString(*iter);

      selidx = pcbDeck->AddString(typestr);

      pcbDeck->SetItemData(selidx,(DWORD)*iter);

      if ( *iter == m_Deck.DeckType )
         cursel = selidx;
   }

   if ( cursel != CB_ERR )
   {
      pcbDeck->SetCurSel(cursel);
   }
   else
   {
      pcbDeck->SetCurSel(0);
      m_Deck.DeckType = deckTypes.front();
      OnDeckTypeChanged();
   }
}

void CBridgeDescGeneralPage::FillRefGirderOffsetTypeComboBox()
{
   CComboBox* pCB = (CComboBox*)GetDlgItem(IDC_REF_GIRDER_OFFSET_TYPE);
   int idx = pCB->AddString("Alignment");
   pCB->SetItemData(idx,(DWORD)pgsTypes::omtAlignment);

   idx = pCB->AddString("Bridge Line");
   pCB->SetItemData(idx,(DWORD)pgsTypes::omtBridge);
}

void CBridgeDescGeneralPage::FillRefGirderComboBox()
{
   CComboBox* pCB = (CComboBox*)GetDlgItem(IDC_REF_GIRDER);
   int curSel = pCB->GetCurSel();
   pCB->ResetContent();

   int idx = pCB->AddString("Center of Girders");
   pCB->SetItemData(idx,(DWORD)-1);

   for ( GirderIndexType i = 0; i < m_nGirders; i++ )
   {
      CString str;
      str.Format("Girder %s",LABEL_GIRDER(i));
      idx = pCB->AddString(str);
      pCB->SetItemData(idx,(DWORD)i);
   }

   pCB->SetCurSel(curSel == CB_ERR ? 0 : curSel);
}

void CBridgeDescGeneralPage::FillGirderConnectivityComboBox()
{
   CComboBox* pcbConnectivity = (CComboBox*)GetDlgItem(IDC_GIRDER_CONNECTIVITY);
   int cursel = pcbConnectivity->GetCurSel();
   pcbConnectivity->ResetContent();

   if ( IsConstantWidthDeck(m_Deck.DeckType) )
   {
      int idx = pcbConnectivity->AddString("Sufficient to make girders act as a unit");
      pcbConnectivity->SetItemData(idx,pgsTypes::atcConnectedAsUnit);

      idx =     pcbConnectivity->AddString("Prevents relative vertical displacement");
      pcbConnectivity->SetItemData(idx,pgsTypes::atcConnectedRelativeDisplacement);

      if ( cursel != CB_ERR ) // if there was a previous selection
      {
         pcbConnectivity->SetCurSel(cursel);
      }
      else
      {
         pcbConnectivity->SetCurSel(0); /// just select index 0
      }
   }
}

void CBridgeDescGeneralPage::UpdateGirderFactory()
{
   CComPtr<IBroker> pBroker;
   AfxGetBroker(&pBroker);

   GET_IFACE2( pBroker, ILibraryNames, pLibNames );
   m_Factory.Release();
   pLibNames->GetBeamFactory(std::string(m_GirderFamilyName),std::string(m_GirderName),&m_Factory);
}

CString CBridgeDescGeneralPage::GetDeckString(pgsTypes::SupportedDeckType deckType)
{
   switch ( deckType )
   {
      case pgsTypes::sdtCompositeCIP:
         return CString("Composite Cast-In-Place Deck");

      case pgsTypes::sdtCompositeSIP:
         return CString("Composite Stay-In-Place Deck Panels");

      case pgsTypes::sdtCompositeOverlay:
         return CString("Composite Cast-In-Place Overlay");

      case pgsTypes::sdtNone:
         return CString("No deck");
   }

   ATLASSERT(0);
   return CString("Unknown Deck Type");
}

void CBridgeDescGeneralPage::OnGirderFamilyChanged() 
{
   CComboBox* pBeamFamilies = (CComboBox*)GetDlgItem(IDC_BEAM_FAMILIES);
   int curSel = pBeamFamilies->GetCurSel();

   if ( curSel == CB_ERR )
   {
      ASSERT(false); // this shouldn't happen
      return; // just return if we are in a release build
   }

   pBeamFamilies->GetLBText(curSel,m_GirderFamilyName);

   InitGirderName();          // sets the current girder name to the first girder of the family
   UpdateGirderFactory();     // gets the new factory for this girder family
   FillGirderNameComboBox();  // fills the girder name combo box with girders from this family
   FillGirderSpacingTypeComboBox(); // get new spacing options for this girder family
   FillDeckTypeComboBox();          // set deck type options to match this girder family
   UpdateGirderConnectivity();      // fills the combo box and enables/disables

   UpdateBridgeDescription();

   if ( !UpdateGirderSpacingLimits() || m_NumGdrSpinner.GetPos() == 1)
      EnableGirderSpacing(FALSE,FALSE);
   else
      EnableGirderSpacing(TRUE,FALSE);


   UpdateSuperstructureDescription();
}

void CBridgeDescGeneralPage::UpdateMinimumGirderCount()
{
   // Determine minimum number of girders
   CComPtr<IBroker> pBroker;
   AfxGetBroker(&pBroker);
   GET_IFACE2( pBroker, ILibrary, pLib );
   const GirderLibraryEntry* pGdrEntry = pLib->GetGirderEntry(m_GirderName);

   CComPtr<IGirderSection> section;
   m_Factory->CreateGirderSection(pBroker,0,INVALID_INDEX,INVALID_INDEX,pGdrEntry->GetDimensions(),&section);

   WebIndexType nWebs;
   section->get_WebCount(&nWebs);

   m_MinGirderCount = (1 < nWebs ? 1 : 2);

   if ( m_NumGdrSpinner.GetSafeHwnd() != NULL )
      m_NumGdrSpinner.SetRange(short(m_MinGirderCount),MAX_GIRDERS_PER_SPAN);
}

void CBridgeDescGeneralPage::OnGirderNameChanged() 
{
   CComboBox* pCBGirders = (CComboBox*)GetDlgItem(IDC_GDR_TYPE);
   int sel = pCBGirders->GetCurSel();
   pCBGirders->GetLBText(sel,m_GirderName);

   UpdateGirderFactory();

   UpdateMinimumGirderCount();


   UpdateBridgeDescription();

   if ( !UpdateGirderSpacingLimits() || m_NumGdrSpinner.GetPos() == 1)
      EnableGirderSpacing(FALSE,FALSE);
   else
      EnableGirderSpacing(TRUE,FALSE);


   UpdateSuperstructureDescription();
}

void CBridgeDescGeneralPage::OnGirderConnectivityChanged() 
{
   UpdateSuperstructureDescription();

   UpdateBridgeDescription();

   UpdateSuperstructureDescription();
}

void CBridgeDescGeneralPage::OnSpacingDatumChanged()
{
   UpdateGirderSpacingLimits();

   UpdateBridgeDescription();

   UpdateSuperstructureDescription();
}

void CBridgeDescGeneralPage::OnGirderSpacingTypeChanged() 
{
   CBridgeDescDlg* pParent = (CBridgeDescDlg*)GetParent();

   CComboBox* pcbSpacingType = (CComboBox*)GetDlgItem(IDC_GIRDER_SPACING_TYPE);
   int curSel = pcbSpacingType->GetCurSel();
   m_GirderSpacingType = (pgsTypes::SupportedBeamSpacing)(pcbSpacingType->GetItemData(curSel));

   FillDeckTypeComboBox(); // update the available deck types for this spacing type
                           // this must be done before the girder spacing limits are determined

   FillGirderConnectivityComboBox();

   BOOL specify_spacing = UpdateGirderSpacingLimits();
   // spacing only needs to be specified if it can take on multiple values

   CComboBox* pcbMeasure = (CComboBox*)GetDlgItem(IDC_GIRDER_SPACING_MEASURE);

   CString strWndTxt;
   BOOL bEnable = FALSE;
   switch ( m_GirderSpacingType )
   {
      case pgsTypes::sbsUniform:
      case pgsTypes::sbsUniformAdjacent:
      case pgsTypes::sbsConstantAdjacent:

         // girder spacing is uniform for the entire bridge
         // enable the controls, unless there is only one girder then spacing doesn't apply
         bEnable = (m_NumGdrSpinner.GetPos() == 1 ? FALSE : specify_spacing);

         // restore the cached values for girder spacing and measurement type
         if ( IsGirderSpacing(m_GirderSpacingType) )
         {
            GetDlgItem(IDC_SPACING)->SetWindowText(m_strCacheGirderSpacing);
         }
         else
         {
            GetDlgItem(IDC_SPACING)->SetWindowText(m_strCacheJointSpacing);
         }
         pcbMeasure->SetCurSel(m_CacheGirderSpacingMeasureIdx);

         GetDlgItem(IDC_REF_GIRDER_OFFSET)->SetWindowText(m_strCacheRefGirderOffset);

      break;

      case pgsTypes::sbsGeneral:
      case pgsTypes::sbsGeneralAdjacent:

         // girder spacing is general (aka, defined span by span)
         // disable the controls
         bEnable = FALSE;

         // cache the current value of girder spacing and measurement type
         if ( IsGirderSpacing(m_GirderSpacingType) )
         {
            GetDlgItem(IDC_SPACING)->GetWindowText(strWndTxt);
            GetDlgItem(IDC_SPACING)->SetWindowText("");

            if ( strWndTxt != "" )
               m_strCacheGirderSpacing = strWndTxt;

         }
         else
         {
            GetDlgItem(IDC_SPACING)->GetWindowText(strWndTxt);
            GetDlgItem(IDC_SPACING)->SetWindowText("");

            if ( strWndTxt != "" )
               m_strCacheJointSpacing = strWndTxt;
         }

         GetDlgItem(IDC_REF_GIRDER_OFFSET)->GetWindowText(strWndTxt);
         GetDlgItem(IDC_REF_GIRDER_OFFSET)->SetWindowText("");
         if ( strWndTxt != "" )
            m_strCacheRefGirderOffset = strWndTxt;

         curSel = pcbMeasure->GetCurSel();
         pcbMeasure->SetCurSel(-1);

         if ( curSel != CB_ERR )
            m_CacheGirderSpacingMeasureIdx = curSel;

         
         pParent->m_BridgeDesc.SetGirderSpacing(m_GirderSpacing);

      break;

      default:
         ATLASSERT(false); // is there a new spacing type????
   }

   GetDlgItem(IDC_SPACING)->EnableWindow(bEnable);
   GetDlgItem(IDC_SPACING_UNIT)->EnableWindow(bEnable);
   GetDlgItem(IDC_ALLOWABLE_SPACING)->EnableWindow(bEnable);
   GetDlgItem(IDC_GIRDER_SPACING_MEASURE)->EnableWindow(bEnable);

   GetDlgItem(IDC_REF_GIRDER)->EnableWindow(bEnable);
   GetDlgItem(IDC_REF_GIRDER_OFFSET)->EnableWindow(bEnable);
   GetDlgItem(IDC_REF_GIRDER_OFFSET_UNIT)->EnableWindow(bEnable);
   GetDlgItem(IDC_REF_GIRDER_OFFSET_TYPE)->EnableWindow(bEnable);

   // update the the unit of measure
   CComPtr<IBroker> pBroker;
   AfxGetBroker(&pBroker);
   GET_IFACE2(pBroker, IDisplayUnits, pDisplayUnits);

   CWnd* pWnd = GetDlgItem(IDC_SPACING_UNIT);
   if ( IsGirderSpacing(m_GirderSpacingType) )
   {
      pWnd->SetWindowText( pDisplayUnits->GetXSectionDimUnit().UnitOfMeasure.UnitTag().c_str());
   }
   else
   {
      pWnd->SetWindowText( pDisplayUnits->GetComponentDimUnit().UnitOfMeasure.UnitTag().c_str());
   }

   if ( !m_bSetActive && IsSpanSpacing(m_GirderSpacingType) )
   {
      CString strText("By selecting this option, a different spacing can be used between each girder. To do this, edit the Span Details for each span.\n\nSpan Details can be edited by selecting the Framing tab and then pressing the edit button for a span.");
      UIHint(strText,UIHINT_SAME_GIRDER_SPACING);
   }

   OnDeckTypeChanged();

   // UpdateBridgeDescription(); // called by OnDeckTypeChanged()
}

void CBridgeDescGeneralPage::UpdateGirderConnectivity()
{
   // connectivity is only applicable for adjacent spacing
   bool bConnectivity = (m_GirderSpacingType == pgsTypes::sbsUniformAdjacent || m_GirderSpacingType == pgsTypes::sbsGeneralAdjacent || m_GirderSpacingType == pgsTypes::sbsConstantAdjacent);
   GetDlgItem(IDC_GIRDER_CONNECTIVITY)->EnableWindow(bConnectivity);
   GetDlgItem(IDC_GIRDER_CONNECTIVITY_S)->EnableWindow(bConnectivity);
   FillGirderConnectivityComboBox();
}

void CBridgeDescGeneralPage::OnDeckTypeChanged() 
{
   UpdateGirderConnectivity();

   // make sure deck dimensions are consistent with deck type
   CComboBox* pcbDeckType = (CComboBox*)GetDlgItem(IDC_DECK_TYPE);
   int curSel = pcbDeckType->GetCurSel();
   pgsTypes::SupportedDeckType newDeckType = (pgsTypes::SupportedDeckType)pcbDeckType->GetItemData(curSel);

   if (IsAdjustableWidthDeck(newDeckType))
   {
      if ( m_CacheDeckEdgePoints.size() == 0 )
      {
         // moving to an adjustable width deck, but there aren't
         // any deck points
         //
         // Create a default deck point
         CBridgeDescDlg* pParent = (CBridgeDescDlg*)GetParent();
         const CSpanData* pSpan = pParent->m_BridgeDesc.GetSpan(0);
         const CGirderTypes* pGirderTypes = pSpan->GetGirderTypes();

         GirderIndexType nGirders = pSpan->GetGirderCount();
         double w = 0;
         for ( GirderIndexType gdrIdx = 0; gdrIdx < nGirders; gdrIdx++ )
         {
            const GirderLibraryEntry* pEntry = pGirderTypes->GetGirderLibraryEntry(gdrIdx);
            w += max(pEntry->GetBeamWidth(pgsTypes::metStart),pEntry->GetBeamWidth(pgsTypes::metEnd));
         }

         CDeckPoint deckPoint;
         deckPoint.Station   = 0;
         deckPoint.LeftEdge  = w/2;
         deckPoint.RightEdge = w/2;

         m_CacheDeckEdgePoints.push_back(deckPoint);
      }

      m_Deck.DeckEdgePoints = m_CacheDeckEdgePoints;
   }
   else
   {
      m_CacheDeckEdgePoints = m_Deck.DeckEdgePoints;
      m_Deck.DeckEdgePoints.clear();
   }
   
   m_Deck.DeckType = newDeckType;

   CBridgeDescDlg* pParent = (CBridgeDescDlg*)GetParent();
   if ( m_Deck.DeckType == pgsTypes::sdtCompositeCIP || m_Deck.DeckType == pgsTypes::sdtCompositeOverlay )
   {
      Float64 maxSlabOffset = pParent->m_BridgeDesc.GetMaxSlabOffset();
      if ( maxSlabOffset < m_Deck.GrossDepth )
         m_Deck.GrossDepth = maxSlabOffset;
   }
   else if ( m_Deck.DeckType == pgsTypes::sdtCompositeSIP )
   {
      Float64 maxSlabOffset = pParent->m_BridgeDesc.GetMaxSlabOffset();
      if ( maxSlabOffset < m_Deck.GrossDepth + m_Deck.PanelDepth )
         m_Deck.GrossDepth = maxSlabOffset - m_Deck.PanelDepth; // decrease the cast depth
   }

   UpdateBridgeDescription();

   UpdateSuperstructureDescription();
}

BOOL CBridgeDescGeneralPage::UpdateGirderSpacingLimits()
{
   CBridgeDescDlg* pParent = (CBridgeDescDlg*)GetParent();

   if ( IsSpanSpacing(m_GirderSpacingType) )
   {
      GetDlgItem(IDC_ALLOWABLE_SPACING)->SetWindowText("");
      return FALSE; // girder spacing isn't input in this dialog
   }

   CComPtr<IBroker> pBroker;
   AfxGetBroker(&pBroker);
   GET_IFACE2(pBroker, IBridge,       pBridge);
   GET_IFACE2(pBroker, IDisplayUnits, pDisplayUnits);

   // need a spacing range that works for every girder in every span
   m_MinGirderSpacing = -MAX_GIRDER_SPACING;
   m_MaxGirderSpacing =  MAX_GIRDER_SPACING;
   const CSpanData* pSpan = pParent->m_BridgeDesc.GetSpan(0);
   do
   {
      // get skew information so spacing ranges can be skew adjusted
      const CPierData* pPrevPier = pSpan->GetPrevPier();
      double prevSkewAngle;
      pBridge->GetSkewAngle(pPrevPier->GetStation(),pPrevPier->GetOrientation(),&prevSkewAngle);

      const CPierData* pNextPier = pSpan->GetNextPier();
      double nextSkewAngle;
      pBridge->GetSkewAngle(pNextPier->GetStation(),pNextPier->GetOrientation(),&nextSkewAngle);

      double startSkewCorrection, endSkewCorrection;
      if ( m_GirderSpacingMeasurementType == pgsTypes::NormalToItem )
      {
         startSkewCorrection = 1;
         endSkewCorrection = 1;
      }
      else
      {
         startSkewCorrection = fabs(1.0/cos(prevSkewAngle));
         endSkewCorrection   = fabs(1.0/cos(nextSkewAngle));
      }

      // check spacing limits for each girder group... grab the controlling values
      const CGirderTypes* pGirderTypes = pSpan->GetGirderTypes();
      GroupIndexType nGirderGroups = pGirderTypes->GetGirderGroupCount();
      for ( GroupIndexType grpIdx = 0; grpIdx < nGirderGroups; grpIdx++ )
      {
         GirderIndexType firstGdrIdx, lastGdrIdx;
         std::string strGdrName;

         pGirderTypes->GetGirderGroup(grpIdx,&firstGdrIdx,&lastGdrIdx,strGdrName);

         const GirderLibraryEntry* pGdrEntry = pGirderTypes->GetGirderLibraryEntry(firstGdrIdx);
         const IBeamFactory::Dimensions& dimensions = pGdrEntry->GetDimensions();

         double min, max;
         m_Factory->GetAllowableSpacingRange(dimensions,m_Deck.DeckType,m_GirderSpacingType,&min,&max);

         double min1 = min*startSkewCorrection;
         double max1 = max*startSkewCorrection;
         double min2 = min*endSkewCorrection;
         double max2 = max*endSkewCorrection;

         if ( IsGirderSpacing(m_GirderSpacingType) )
         {
            // girder spacing
            m_MinGirderSpacing = _cpp_max(_cpp_max(min1,min2),m_MinGirderSpacing);
            m_MaxGirderSpacing = _cpp_min(_cpp_min(max1,max2),m_MaxGirderSpacing);
         }
         else
         {
            // joint spacing
            m_MinGirderSpacing = 0;
            m_MaxGirderSpacing = _cpp_min(_cpp_min(max1-min1,max2-min2),m_MaxGirderSpacing);
         }
      }

      pSpan = pSpan->GetNextPier()->GetNextSpan();
   } while ( pSpan );

   BOOL specify_spacing = !IsEqual(m_MinGirderSpacing,m_MaxGirderSpacing) ? TRUE : FALSE;

   ATLASSERT( m_MinGirderSpacing <= m_MaxGirderSpacing );

   // label for beam spacing
   if ( IsGirderSpacing(m_GirderSpacingType) )
   {
      GetDlgItem(IDC_GIRDER_SPACING_LABEL)->SetWindowText("Girder Spacing");
   }
   else
   {
      GetDlgItem(IDC_GIRDER_SPACING_LABEL)->SetWindowText("Joint Spacing");
   }

   CString label;
   if (specify_spacing)
   {
      if (m_MaxGirderSpacing < MAX_GIRDER_SPACING)
      {
         if ( IsGirderSpacing(m_GirderSpacingType) )
         {
            label.Format("( %s to %s )", 
               FormatDimension(m_MinGirderSpacing,pDisplayUnits->GetXSectionDimUnit()),
               FormatDimension(m_MaxGirderSpacing,pDisplayUnits->GetXSectionDimUnit()));
         }
         else
         {
            // this is actually joint spacing
            label.Format("( 0 to %s )", 
               FormatDimension(m_MaxGirderSpacing - m_MinGirderSpacing,pDisplayUnits->GetComponentDimUnit()));
         }
      }
      else
      {
         label.Format("( %s or more )", 
            FormatDimension(m_MinGirderSpacing,pDisplayUnits->GetXSectionDimUnit()));
      }
   }

   GetDlgItem(IDC_ALLOWABLE_SPACING)->SetWindowText(label);

   // if spacing is out of range fix it
   if (m_GirderSpacing < m_MinGirderSpacing || m_MaxGirderSpacing < m_GirderSpacing )
   {
      m_GirderSpacing = m_MinGirderSpacing;

      CDataExchange dx(this,FALSE);
      DDX_UnitValueAndTag( &dx, IDC_SPACING,  IDC_SPACING_UNIT, m_GirderSpacing, pDisplayUnits->GetXSectionDimUnit() );

      if ( IsGirderSpacing(m_GirderSpacingType) )
      {
         m_strCacheGirderSpacing.Format("%s",FormatDimension(m_GirderSpacing,pDisplayUnits->GetXSectionDimUnit(), false));
         m_strCacheJointSpacing.Format( "%s",FormatDimension(0,              pDisplayUnits->GetComponentDimUnit(),false));
      }
      else
      {
         m_strCacheGirderSpacing.Format("%s",FormatDimension(m_MinGirderSpacing,pDisplayUnits->GetXSectionDimUnit(), false));
         m_strCacheJointSpacing.Format( "%s",FormatDimension(m_GirderSpacing,   pDisplayUnits->GetComponentDimUnit(),false));
      }
   }

   return specify_spacing;
}

void CBridgeDescGeneralPage::UpdateSuperstructureDescription()
{
   CString description;

   // girder name
   CBridgeDescDlg* pParent = (CBridgeDescDlg*)GetParent();
   ASSERT( pParent->IsKindOf(RUNTIME_CLASS(CBridgeDescDlg)) );

   CComPtr<IBroker> pBroker;
   AfxGetBroker(&pBroker);
   GET_IFACE2( pBroker, ILibrary, pLib );

   CComboBox* pCBGirders = (CComboBox*)GetDlgItem(IDC_GDR_TYPE);
   int sel = pCBGirders->GetCurSel();

   const GirderLibraryEntry* pGdrEntry = NULL;
   if ( sel == CB_ERR )
   {
      pGdrEntry = pLib->GetGirderEntry(m_GirderName);
   }
   else
   {
      CString strGirder;
      pCBGirders->GetLBText(sel,strGirder);
      pGdrEntry = pLib->GetGirderEntry(strGirder);
   }


   std::string name = pGdrEntry->GetSectionName();
   description = name.c_str();

   // deck type
   CComboBox* box = (CComboBox*)GetDlgItem(IDC_DECK_TYPE);
   int cursel = box->GetCurSel();
   pgsTypes::SupportedDeckType deckType = (pgsTypes::SupportedDeckType)box->GetItemData(cursel);

   description += ", " + GetDeckString(deckType);

   pgsTypes::AdjacentTransverseConnectivity connect = pgsTypes::atcConnectedAsUnit;

   // connectivity if adjacent
   if (deckType == pgsTypes::sdtCompositeOverlay || deckType == pgsTypes::sdtNone)
   {

      CComboBox* box = (CComboBox*)GetDlgItem(IDC_GIRDER_CONNECTIVITY);
      // good a place as any to cache connectivity
      m_CacheGirderConnectivityIdx = box->GetCurSel();
      ATLASSERT(m_CacheGirderConnectivityIdx!=-1);

      connect = (pgsTypes::AdjacentTransverseConnectivity)m_CacheGirderConnectivityIdx;

      description += ". Girders are spaced adjacently, and are connected transversely ";

      if (connect == pgsTypes::atcConnectedAsUnit)
      {
         description +="sufficiently to act as a unit.";
      }
      else
      {
         description += "only enough to prevent relative vertical displacement at interface.";
      }
   }
   else
   {
      description += ", Girders at spread spacing.";
   }


   GET_IFACE2( pBroker, IBridgeDescription, pIBridgeDesc );
   const CBridgeDescription* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   if ( pBridgeDesc->GetDistributionFactorMethod() == pgsTypes::DirectlyInput )
   {
      description += "\r\n\r\nDistribution factors from Direct User Input";
   }
   else if ( pBridgeDesc->GetDistributionFactorMethod() == pgsTypes::LeverRule )
   {
      description += "\r\n\r\nDistribution factors computed using lever rule. Specification was overridden";
   }
   else
   {
#pragma Reminder("Assuming same section used for all spans/girders")
      std::string entry_name = pGdrEntry->GetName();


      CComPtr<IDistFactorEngineer> dfEngineer;
      m_Factory->CreateDistFactorEngineer(pBroker, -1, &deckType, &connect, &dfEngineer);
      std::string dfmethod = dfEngineer->GetComputationDescription(0, 0, entry_name, deckType, connect );

      description += "\r\n\r\nDistribution factors computed using ";
      description += dfmethod.c_str();
   }

   CEdit* pedit = (CEdit*)GetDlgItem(IDC_SUPERSTRUCTURE_DESCRIPTION);
   pedit->SetWindowText(description);
}



BOOL CBridgeDescGeneralPage::OnToolTipNotify(UINT id,NMHDR* pNMHDR, LRESULT* pResult)
{
   TOOLTIPTEXT* pTTT = (TOOLTIPTEXT*)pNMHDR;
   HWND hwndTool = (HWND)pNMHDR->idFrom;
   if ( pTTT->uFlags & TTF_IDISHWND )
   {
      // idFrom is actually HWND of tool
      UINT nID = ::GetDlgCtrlID(hwndTool);
      BOOL bShowTip = false;
      switch(nID)
      {
      case IDC_SAME_NUM_GIRDERLINES:
         if ( IsDlgButtonChecked(IDC_SAME_NUM_GIRDERLINES) )
            m_strToolTipText = "Uncheck this option to define a different number of girders in each span.";
         else
            m_strToolTipText = "Check this option to define the number of girders in all spans. Otherwise, open the Framing tab and click on the Edit Span Details button to define the number of girders in a particular span.";

         bShowTip = TRUE;
         break;

      case IDC_SAME_GIRDERNAME:
         if ( IsDlgButtonChecked(IDC_SAME_GIRDERNAME) )
            m_strToolTipText = "Uncheck this option to use a different girder for each girder line.";
         else
            m_strToolTipText = "Check this option to use the same girder in all spans. Otherwise, open the Framing tab and click on the Edit Span Details button to assign a different girder to each girder line in a particular span.";

         bShowTip = TRUE;
         break;

      case IDC_REF_GIRDER_OFFSET:
         m_strToolTipText.LoadString(IDS_ALIGNMENTOFFSET_FMT);
         bShowTip = TRUE;
         break;

      default:
         return FALSE;
      }

      if ( bShowTip )
      {
         ::SendMessage(pNMHDR->hwndFrom,TTM_SETMAXTIPWIDTH,0,300); // makes it a multi-line tooltip
         pTTT->lpszText = m_strToolTipText.LockBuffer();
         pTTT->hinst = NULL;
         return TRUE;
      }
      else
      {
         return FALSE;
      }

   }
   return FALSE;
}

void CBridgeDescGeneralPage::UIHint(const CString& strText,UINT mask)
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

BOOL CBridgeDescGeneralPage::OnSetActive() 
{
   m_bSetActive = true;

   if ( !m_bFirstSetActive )
      Init();

	BOOL bResult = CPropertyPage::OnSetActive(); // calls DoDataExchange if not the first time page is active

   //OnGirderNameChanged(); // Available slab types are a function of girder type
   OnDeckTypeChanged();

   EnableGirderName(m_bSameGirderName);
   EnableNumGirderLines(m_bSameNumberOfGirders);

   OnGirderSpacingTypeChanged(); // enables/disables the girder spacing input based on current value

   UpdateSuperstructureDescription();
	
   m_bSetActive = false;

   return bResult;
}


void CBridgeDescGeneralPage::OnHelp() 
{
   ::HtmlHelp( *this, AfxGetApp()->m_pszHelpFilePath, HH_HELP_CONTEXT, IDH_BRIDGEDESC_GENERAL );
}

void CBridgeDescGeneralPage::InitGirderName()
{
   // Gets the first girder name for the current girder family
   CComPtr<IBroker> pBroker;
   AfxGetBroker(&pBroker);
   GET_IFACE2( pBroker, ILibraryNames, pLibNames );
   std::vector<std::string> names;
   pLibNames->EnumGirderNames(m_GirderFamilyName, &names );
   m_GirderName = names.front().c_str();
}