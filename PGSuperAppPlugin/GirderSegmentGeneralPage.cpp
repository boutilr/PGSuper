// GirderSegmentGeneralPage.cpp : implementation file
//

#include "PGSuperAppPlugin\stdafx.h"
#include "PGSuperAppPlugin.h"
#include "GirderSegmentGeneralPage.h"
#include "GirderSegmentDlg.h"

#include <PgsExt\BridgeDescription2.h>


#include <EAF\EAFDisplayUnits.h>
#include <IFace\Project.h>
#include <IFace\Bridge.h>
#include <IFace\BeamFactory.h>

#include "ConcreteDetailsDlg.h"

#include <System\Tokenizer.h>
#include <Material\Material.h>

#include "TimelineEventDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// CGirderSegmentGeneralPage dialog

IMPLEMENT_DYNAMIC(CGirderSegmentGeneralPage, CPropertyPage)

CGirderSegmentGeneralPage::CGirderSegmentGeneralPage()
	: CPropertyPage(CGirderSegmentGeneralPage::IDD)
{

}

CGirderSegmentGeneralPage::~CGirderSegmentGeneralPage()
{
}

void CGirderSegmentGeneralPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);

   DDX_Control(pDX, IDC_EC,      m_ctrlEc);
	DDX_Control(pDX, IDC_ECI,     m_ctrlEci);
	DDX_Control(pDX, IDC_MOD_EC,  m_ctrlEcCheck);
	DDX_Control(pDX, IDC_MOD_ECI, m_ctrlEciCheck);
   DDX_Control(pDX, IDC_GIRDER_FC, m_ctrlFc);
	DDX_Control(pDX, IDC_FCI, m_ctrlFci);

   DDX_Control(pDX, IDC_LEFT_PRISMATIC_LENGTH,  m_ctrlSectionLength[pgsTypes::sztLeftPrismatic]);
   DDX_Control(pDX, IDC_LEFT_TAPERED_LENGTH,    m_ctrlSectionLength[pgsTypes::sztLeftTapered]);
   DDX_Control(pDX, IDC_RIGHT_TAPERED_LENGTH,   m_ctrlSectionLength[pgsTypes::sztRightTapered]);
   DDX_Control(pDX, IDC_RIGHT_PRISMATIC_LENGTH, m_ctrlSectionLength[pgsTypes::sztRightPrismatic]);

   DDX_Control(pDX, IDC_LEFT_PRISMATIC_HEIGHT,  m_ctrlSectionHeight[pgsTypes::sztLeftPrismatic]);
   DDX_Control(pDX, IDC_LEFT_TAPERED_HEIGHT,    m_ctrlSectionHeight[pgsTypes::sztLeftTapered]);
   DDX_Control(pDX, IDC_RIGHT_TAPERED_HEIGHT,   m_ctrlSectionHeight[pgsTypes::sztRightTapered]);
   DDX_Control(pDX, IDC_RIGHT_PRISMATIC_HEIGHT, m_ctrlSectionHeight[pgsTypes::sztRightPrismatic]);

   DDX_Control(pDX, IDC_LEFT_PRISMATIC_FLANGE_DEPTH,  m_ctrlBottomFlangeDepth[pgsTypes::sztLeftPrismatic]);
   DDX_Control(pDX, IDC_LEFT_TAPERED_FLANGE_DEPTH,    m_ctrlBottomFlangeDepth[pgsTypes::sztLeftTapered]);
   DDX_Control(pDX, IDC_RIGHT_TAPERED_FLANGE_DEPTH,   m_ctrlBottomFlangeDepth[pgsTypes::sztRightTapered]);
   DDX_Control(pDX, IDC_RIGHT_PRISMATIC_FLANGE_DEPTH, m_ctrlBottomFlangeDepth[pgsTypes::sztRightPrismatic]);


   CComPtr<IBroker> pBroker;
   EAFGetBroker(&pBroker);
   GET_IFACE2(pBroker,IEAFDisplayUnits,pDisplayUnits);

   CGirderSegmentDlg* pParent = (CGirderSegmentDlg*)GetParent();

   CPrecastSegmentData* pSegment = pParent->m_Girder.GetSegment(pParent->m_SegmentKey.segmentIndex);

   DDX_CBItemData(pDX,IDC_CONSTRUCTION_EVENT,pParent->m_ConstructionEventIdx);
   DDX_CBItemData(pDX,IDC_ERECTION_EVENT,pParent->m_ErectionEventIdx);

   pgsTypes::SegmentVariationType variationType;
   Float64 VariationLength[4];
   Float64 VariationHeight[4];
   Float64 VariationBottomFlangeDepth[4];
   bool bBottomFlangeDepth = false;

   if ( !pDX->m_bSaveAndValidate )
   {
      variationType = pSegment->GetVariationType();
      bBottomFlangeDepth = pSegment->IsVariableBottomFlangeDepthEnabled();
      for ( int i = 0; i < 4; i++ )
      {
         pSegment->GetVariationParameters(pgsTypes::SegmentZoneType(i),false,&VariationLength[i],&VariationHeight[i],&VariationBottomFlangeDepth[i]);
      }
   }

   DDX_Check_Bool(pDX,IDC_BOTTOM_FLANGE_DEPTH,bBottomFlangeDepth);

   DDX_CBItemData(pDX,IDC_VARIATION_TYPE,variationType);

   DDX_UnitValueAndTag(pDX, IDC_LEFT_PRISMATIC_LENGTH,  IDC_LEFT_PRISMATIC_LENGTH_UNIT,  VariationLength[pgsTypes::sztLeftPrismatic],  pDisplayUnits->GetSpanLengthUnit() );
   DDX_UnitValueAndTag(pDX, IDC_LEFT_TAPERED_LENGTH,    IDC_LEFT_TAPERED_LENGTH_UNIT,    VariationLength[pgsTypes::sztLeftTapered],    pDisplayUnits->GetSpanLengthUnit() );
   DDX_UnitValueAndTag(pDX, IDC_RIGHT_TAPERED_LENGTH,   IDC_RIGHT_TAPERED_LENGTH_UNIT,   VariationLength[pgsTypes::sztRightTapered],   pDisplayUnits->GetSpanLengthUnit() );
   DDX_UnitValueAndTag(pDX, IDC_RIGHT_PRISMATIC_LENGTH, IDC_RIGHT_PRISMATIC_LENGTH_UNIT, VariationLength[pgsTypes::sztRightPrismatic], pDisplayUnits->GetSpanLengthUnit() );

   DDX_UnitValueAndTag(pDX, IDC_LEFT_PRISMATIC_HEIGHT,  IDC_LEFT_PRISMATIC_HEIGHT_UNIT,  VariationHeight[pgsTypes::sztLeftPrismatic],  pDisplayUnits->GetSpanLengthUnit() );
   DDX_UnitValueAndTag(pDX, IDC_LEFT_TAPERED_HEIGHT,    IDC_LEFT_TAPERED_HEIGHT_UNIT,    VariationHeight[pgsTypes::sztLeftTapered],    pDisplayUnits->GetSpanLengthUnit() );
   DDX_UnitValueAndTag(pDX, IDC_RIGHT_TAPERED_HEIGHT,   IDC_RIGHT_TAPERED_HEIGHT_UNIT,   VariationHeight[pgsTypes::sztRightTapered],   pDisplayUnits->GetSpanLengthUnit() );
   DDX_UnitValueAndTag(pDX, IDC_RIGHT_PRISMATIC_HEIGHT, IDC_RIGHT_PRISMATIC_HEIGHT_UNIT, VariationHeight[pgsTypes::sztRightPrismatic], pDisplayUnits->GetSpanLengthUnit() );

   DDX_UnitValueAndTag(pDX, IDC_LEFT_PRISMATIC_FLANGE_DEPTH,  IDC_LEFT_PRISMATIC_FLANGE_DEPTH_UNIT,  VariationBottomFlangeDepth[pgsTypes::sztLeftPrismatic],  pDisplayUnits->GetComponentDimUnit() );
   DDX_UnitValueAndTag(pDX, IDC_LEFT_TAPERED_FLANGE_DEPTH,    IDC_LEFT_TAPERED_FLANGE_DEPTH_UNIT,    VariationBottomFlangeDepth[pgsTypes::sztLeftTapered],    pDisplayUnits->GetComponentDimUnit() );
   DDX_UnitValueAndTag(pDX, IDC_RIGHT_TAPERED_FLANGE_DEPTH,   IDC_RIGHT_TAPERED_FLANGE_DEPTH_UNIT,   VariationBottomFlangeDepth[pgsTypes::sztRightTapered],   pDisplayUnits->GetComponentDimUnit() );
   DDX_UnitValueAndTag(pDX, IDC_RIGHT_PRISMATIC_FLANGE_DEPTH, IDC_RIGHT_PRISMATIC_FLANGE_DEPTH_UNIT, VariationBottomFlangeDepth[pgsTypes::sztRightPrismatic], pDisplayUnits->GetComponentDimUnit() );

   DDX_UnitValueAndTag(pDX, IDC_LEFT_END_BLOCK_LENGTH,     IDC_LEFT_END_BLOCK_LENGTH_UNIT,     pSegment->EndBlockLength[pgsTypes::metStart],           pDisplayUnits->GetSpanLengthUnit() );
   DDX_UnitValueAndTag(pDX, IDC_LEFT_END_BLOCK_TRANSITION, IDC_LEFT_END_BLOCK_TRANSITION_UNIT, pSegment->EndBlockTransitionLength[pgsTypes::metStart], pDisplayUnits->GetSpanLengthUnit() );
   DDX_UnitValueAndTag(pDX, IDC_LEFT_END_BLOCK_WIDTH,      IDC_LEFT_END_BLOCK_WIDTH_UNIT,      pSegment->EndBlockWidth[pgsTypes::metStart],            pDisplayUnits->GetComponentDimUnit() );

   DDX_UnitValueAndTag(pDX, IDC_RIGHT_END_BLOCK_LENGTH,     IDC_RIGHT_END_BLOCK_LENGTH_UNIT,     pSegment->EndBlockLength[pgsTypes::metEnd],           pDisplayUnits->GetSpanLengthUnit() );
   DDX_UnitValueAndTag(pDX, IDC_RIGHT_END_BLOCK_TRANSITION, IDC_RIGHT_END_BLOCK_TRANSITION_UNIT, pSegment->EndBlockTransitionLength[pgsTypes::metEnd], pDisplayUnits->GetSpanLengthUnit() );
   DDX_UnitValueAndTag(pDX, IDC_RIGHT_END_BLOCK_WIDTH,      IDC_RIGHT_END_BLOCK_WIDTH_UNIT,      pSegment->EndBlockWidth[pgsTypes::metEnd],            pDisplayUnits->GetComponentDimUnit() );

   if ( pDX->m_bSaveAndValidate )
   {
      pSegment->SetVariationType(variationType);
      pSegment->EnableVariableBottomFlangeDepth(bBottomFlangeDepth);
      for ( int i = 0; i < 4; i++ )
      {
         pSegment->SetVariationParameters(pgsTypes::SegmentZoneType(i),VariationLength[i],VariationHeight[i],VariationBottomFlangeDepth[i]);
      }
   }

   // concrete material
   ExchangeConcreteData(pDX);

   DDX_UnitValueAndTag( pDX, IDC_FCI, IDC_FCI_UNIT, pSegment->Material.Concrete.Fci, pDisplayUnits->GetStressUnit() );
   // Validation: 0 < f'ci <= f'c   
   DDV_UnitValueLimitOrLess( pDX, IDC_FCI, pSegment->Material.Concrete.Fci,  pSegment->Material.Concrete.Fc, pDisplayUnits->GetStressUnit() );

   if ( !pDX->m_bSaveAndValidate )
   {
      CString strSegmentLength;
      strSegmentLength.Format(_T("Segment Length: %s"),FormatDimension(GetSegmentLength(),pDisplayUnits->GetSpanLengthUnit(),true) );
      DDX_Text(pDX,IDC_SEGMENT_LENGTH,strSegmentLength);
   }
}


BEGIN_MESSAGE_MAP(CGirderSegmentGeneralPage, CPropertyPage)
   ON_BN_CLICKED(IDC_MOD_ECI, OnUserEci)
   ON_BN_CLICKED(IDC_MOD_EC, OnUserEc)
	ON_EN_CHANGE(IDC_FCI, OnChangeFci)
	ON_EN_CHANGE(IDC_GIRDER_FC, OnChangeGirderFc)
	ON_EN_CHANGE(IDC_ECI, OnChangeEci)
	ON_EN_CHANGE(IDC_EC, OnChangeEc)
	ON_BN_CLICKED(IDC_MORE, OnMoreConcreteProperties)
   ON_NOTIFY_EX(TTN_NEEDTEXT,0,OnToolTipNotify)
   ON_CBN_SELCHANGE(IDC_VARIATION_TYPE, &CGirderSegmentGeneralPage::OnVariationTypeChanged)
   ON_EN_CHANGE(IDC_LEFT_PRISMATIC_LENGTH, &CGirderSegmentGeneralPage::OnSegmentChanged)
   ON_EN_CHANGE(IDC_LEFT_PRISMATIC_HEIGHT, &CGirderSegmentGeneralPage::OnSegmentChanged)
   ON_EN_CHANGE(IDC_LEFT_PRISMATIC_FLANGE_DEPTH, &CGirderSegmentGeneralPage::OnSegmentChanged)
   ON_EN_CHANGE(IDC_LEFT_TAPERED_LENGTH, &CGirderSegmentGeneralPage::OnSegmentChanged)
   ON_EN_CHANGE(IDC_LEFT_TAPERED_HEIGHT, &CGirderSegmentGeneralPage::OnSegmentChanged)
   ON_EN_CHANGE(IDC_LEFT_TAPERED_FLANGE_DEPTH, &CGirderSegmentGeneralPage::OnSegmentChanged)
   ON_EN_CHANGE(IDC_RIGHT_TAPERED_LENGTH, &CGirderSegmentGeneralPage::OnSegmentChanged)
   ON_EN_CHANGE(IDC_RIGHT_TAPERED_HEIGHT, &CGirderSegmentGeneralPage::OnSegmentChanged)
   ON_EN_CHANGE(IDC_RIGHT_TAPERED_FLANGE_DEPTH, &CGirderSegmentGeneralPage::OnSegmentChanged)
   ON_EN_CHANGE(IDC_RIGHT_PRISMATIC_LENGTH, &CGirderSegmentGeneralPage::OnSegmentChanged)
   ON_EN_CHANGE(IDC_RIGHT_PRISMATIC_HEIGHT, &CGirderSegmentGeneralPage::OnSegmentChanged)
   ON_EN_CHANGE(IDC_RIGHT_PRISMATIC_FLANGE_DEPTH, &CGirderSegmentGeneralPage::OnSegmentChanged)
   ON_CBN_SELCHANGE(IDC_CONSTRUCTION_EVENT, OnConstructionEventChanged)
   ON_CBN_DROPDOWN(IDC_CONSTRUCTION_EVENT, OnConstructionEventChanging)
   ON_CBN_SELCHANGE(IDC_ERECTION_EVENT, OnErectionEventChanged)
   ON_CBN_DROPDOWN(IDC_ERECTION_EVENT, OnErectionEventChanging)
   ON_BN_CLICKED(IDC_FC1, &CGirderSegmentGeneralPage::OnConcreteStrength)
   ON_BN_CLICKED(IDC_FC2, &CGirderSegmentGeneralPage::OnConcreteStrength)
   ON_BN_CLICKED(IDC_BOTTOM_FLANGE_DEPTH, &CGirderSegmentGeneralPage::OnBnClickedBottomFlangeDepth)
END_MESSAGE_MAP()


// CGirderSegmentGeneralPage message handlers
BOOL CGirderSegmentGeneralPage::OnInitDialog() 
{
   FillVariationTypeComboBox();
   FillEventList();

   m_ctrlDrawSegment.SubclassDlgItem(IDC_DRAW_SEGMENT,this);
   m_ctrlDrawSegment.CustomInit(this);

   CPropertyPage::OnInitDialog();

   InitBottomFlangeDepthControls();
   InitEndBlockControls();

   CComPtr<IBroker> pBroker;
   EAFGetBroker(&pBroker);
   GET_IFACE2(pBroker,IBridgeDescription,pIBridgeDesc);
   const CTimelineManager* pTimelineMgr = pIBridgeDesc->GetTimelineManager();
   EventIndexType eventIdx = pTimelineMgr->GetSegmentConstructionEventIndex();
   m_AgeAtRelease = pTimelineMgr->GetEventByIndex(eventIdx)->GetConstructSegmentsActivity().GetAgeAtRelease();

   if ( m_strUserEc == _T("") )
      m_ctrlEc.GetWindowText(m_strUserEc);
	
   if ( m_strUserEci == _T("") )
      m_ctrlEci.GetWindowText(m_strUserEci);

   OnUserEci();
   OnUserEc();
   OnConcreteStrength();

   OnVariationTypeChanged();

   EnableToolTips(TRUE);

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CGirderSegmentGeneralPage::ExchangeConcreteData(CDataExchange* pDX)
{
   CComPtr<IBroker> pBroker;
   EAFGetBroker(&pBroker);
   GET_IFACE2(pBroker,IEAFDisplayUnits,pDisplayUnits);

   CGirderSegmentDlg* pParent = (CGirderSegmentDlg*)GetParent();
   CPrecastSegmentData* pSegment = pParent->m_Girder.GetSegment(pParent->m_SegmentKey.segmentIndex);

   int value;
   if ( !pDX->m_bSaveAndValidate )
      value = pSegment->Material.Concrete.bBasePropertiesOnInitialValues ? 0 : 1;
   DDX_Radio(pDX,IDC_FC1,value);
   if ( pDX->m_bSaveAndValidate )
      pSegment->Material.Concrete.bBasePropertiesOnInitialValues = (value == 0 ? true : false);

   DDX_UnitValueAndTag( pDX, IDC_FCI,  IDC_FCI_UNIT,   pSegment->Material.Concrete.Fci , pDisplayUnits->GetStressUnit() );
   DDV_UnitValueGreaterThanZero( pDX, IDC_FCI,pSegment->Material.Concrete.Fci, pDisplayUnits->GetStressUnit() );

   DDX_UnitValueAndTag( pDX, IDC_GIRDER_FC,  IDC_GIRDER_FC_UNIT,   pSegment->Material.Concrete.Fc , pDisplayUnits->GetStressUnit() );
   DDV_UnitValueGreaterThanZero( pDX, IDC_GIRDER_FC,pSegment->Material.Concrete.Fc, pDisplayUnits->GetStressUnit() );

   DDX_Check_Bool(pDX, IDC_MOD_ECI, pSegment->Material.Concrete.bUserEci);
   DDX_UnitValueAndTag( pDX, IDC_ECI,  IDC_ECI_UNIT,   pSegment->Material.Concrete.Eci , pDisplayUnits->GetModEUnit() );
   DDV_UnitValueGreaterThanZero( pDX, IDC_ECI,pSegment->Material.Concrete.Eci, pDisplayUnits->GetModEUnit() );

   DDX_Check_Bool(pDX, IDC_MOD_EC,  pSegment->Material.Concrete.bUserEc);
   DDX_UnitValueAndTag( pDX, IDC_EC,  IDC_EC_UNIT, pSegment->Material.Concrete.Ec , pDisplayUnits->GetModEUnit() );
   DDV_UnitValueGreaterThanZero( pDX, IDC_EC, pSegment->Material.Concrete.Ec, pDisplayUnits->GetModEUnit() );

   if ( pDX->m_bSaveAndValidate && m_ctrlEcCheck.GetCheck() == 1 )
   {
      m_ctrlEc.GetWindowText(m_strUserEc);
   }

   if ( pDX->m_bSaveAndValidate && m_ctrlEciCheck.GetCheck() == 1 )
   {
      m_ctrlEci.GetWindowText(m_strUserEci);
   }
}

void CGirderSegmentGeneralPage::FillVariationTypeComboBox()
{
   CComboBox* pCB = (CComboBox*)GetDlgItem(IDC_VARIATION_TYPE);

   CGirderSegmentDlg* pParent = (CGirderSegmentDlg*)GetParent();
   std::vector<pgsTypes::SegmentVariationType> segmentVariations(pParent->m_Girder.GetSupportedSegmentVariations());

   std::vector<pgsTypes::SegmentVariationType>::iterator iter(segmentVariations.begin());
   std::vector<pgsTypes::SegmentVariationType>::iterator end(segmentVariations.end());

   for ( ; iter != end; iter++ )
   {
      pgsTypes::SegmentVariationType variationType = *iter;
      int idx;
      switch(variationType)
      {
      case pgsTypes::svtNone:
         idx = pCB->AddString(_T("None"));
         break;

      case pgsTypes::svtLinear:
         idx = pCB->AddString(_T("Linear"));
         break;

      case pgsTypes::svtParabolic:
         idx = pCB->AddString(_T("Parabolic"));
         break;

      case pgsTypes::svtDoubleLinear:
         idx = pCB->AddString(_T("Double Linear"));
         break;

      case pgsTypes::svtDoubleParabolic:
         idx = pCB->AddString(_T("Double Parabolic"));
         break;

      default:
         ATLASSERT(false); // should never get here
      }

      pCB->SetItemData(idx,(DWORD_PTR)variationType);
   }
}

void CGirderSegmentGeneralPage::OnUserEci()
{
   BOOL bEnable = ((CButton*)GetDlgItem(IDC_MOD_ECI))->GetCheck();
   GetDlgItem(IDC_ECI)->EnableWindow(bEnable);
   GetDlgItem(IDC_ECI_UNIT)->EnableWindow(bEnable);

   if (bEnable==FALSE)
   {
      m_ctrlEci.GetWindowText(m_strUserEci);
      UpdateEci();
   }
   else
   {
      m_ctrlEci.SetWindowText(m_strUserEci);
   }

   UpdateEc();
}

void CGirderSegmentGeneralPage::OnUserEc()
{
   BOOL bEnable = ((CButton*)GetDlgItem(IDC_MOD_EC))->GetCheck();
   GetDlgItem(IDC_EC)->EnableWindow(bEnable);
   GetDlgItem(IDC_EC_UNIT)->EnableWindow(bEnable);

   if (bEnable==FALSE)
   {
      m_ctrlEc.GetWindowText(m_strUserEc);
      UpdateEc();
   }
   else
   {
      m_ctrlEc.SetWindowText(m_strUserEc); 
   }

   UpdateEci();
}

void CGirderSegmentGeneralPage::OnChangeFci() 
{
   UpdateEci();
   UpdateFc();
   UpdateEc();
}

void CGirderSegmentGeneralPage::OnChangeGirderFc() 
{
   UpdateEc();
   UpdateFci();
   UpdateEci();
}

void CGirderSegmentGeneralPage::OnChangeEc()
{
   if (m_ctrlEcCheck.GetCheck() == TRUE) // checked
      UpdateEci();
}

void CGirderSegmentGeneralPage::OnChangeEci()
{
   if (m_ctrlEciCheck.GetCheck() == TRUE) // checked
      UpdateEc();
}

void CGirderSegmentGeneralPage::UpdateEci()
{
   // update modulus
   int i = GetCheckedRadioButton(IDC_FC1,IDC_FC2);
   int method = 0;

   if ( i == IDC_FC2 )
   {
      // concrete model is based on f'c
      if ( m_ctrlEcCheck.GetCheck() == TRUE )
      {
         // Ec box is checked... user has input a value for Ec
         // Eci is based on user value for Ec not f'ci
         method = 0;
      }
      else
      {
         // Ec box is not checked... Ec is computed from f'c, compute Eci from f'ci
         method = 1;
      }
   }
   else
   {
      if ( m_ctrlEciCheck.GetCheck() == FALSE )// not checked
      {
         method = 1;
      }
      else
      {
         method = -1; // don't compute... it has user input
      }
   }

   if ( method == 0 )
   {
      // Eci is based on the user input value of Ec and not f'ci
      CComPtr<IBroker> pBroker;
      EAFGetBroker(&pBroker);
      GET_IFACE2(pBroker,IEAFDisplayUnits,pDisplayUnits);

      CString strEc;
      m_ctrlEc.GetWindowText(strEc);
      Float64 Ec;
      sysTokenizer::ParseDouble(strEc,&Ec);
      Ec = ::ConvertToSysUnits(Ec,pDisplayUnits->GetModEUnit().UnitOfMeasure);

      CGirderSegmentDlg* pParent = (CGirderSegmentDlg*)GetParent();
      CPrecastSegmentData* pSegment = pParent->m_Girder.GetSegment(pParent->m_SegmentKey.segmentIndex);

      matACI209Concrete concrete;
      concrete.UserEc28(true);
      concrete.SetEc28(Ec);
      concrete.SetA(pSegment->Material.Concrete.A);
      concrete.SetBeta(pSegment->Material.Concrete.B);
      concrete.SetTimeAtCasting(0);
      concrete.SetFc28(pSegment->Material.Concrete.Fc);
      concrete.SetStrengthDensity(pSegment->Material.Concrete.StrengthDensity);
      Float64 Eci = concrete.GetEc(m_AgeAtRelease);

      CString strEci;
      strEci.Format(_T("%s"),FormatDimension(Eci,pDisplayUnits->GetModEUnit(),false));
      m_ctrlEci.SetWindowText(strEci);
   }
   else if ( method == 1 )
   {
      CString strFci, strDensity, strK1, strK2;
      m_ctrlFci.GetWindowText(strFci);

      CComPtr<IBroker> pBroker;
      EAFGetBroker(&pBroker);
      GET_IFACE2(pBroker,IEAFDisplayUnits,pDisplayUnits);

      CGirderSegmentDlg* pParent = (CGirderSegmentDlg*)GetParent();
      CPrecastSegmentData* pSegment = pParent->m_Girder.GetSegment(pParent->m_SegmentKey.segmentIndex);

      strDensity.Format(_T("%s"),FormatDimension(pSegment->Material.Concrete.StrengthDensity,pDisplayUnits->GetDensityUnit(),false));
      strK1.Format(_T("%f"),pSegment->Material.Concrete.EcK1);
      strK2.Format(_T("%f"),pSegment->Material.Concrete.EcK2);

      CString strEci = CConcreteDetailsDlg::UpdateEc(strFci,strDensity,strK1,strK2);
      m_ctrlEci.SetWindowText(strEci);
   }
}

void CGirderSegmentGeneralPage::UpdateEc()
{
   // update modulus
   int i = GetCheckedRadioButton(IDC_FC1,IDC_FC2);
   int method = 0;

   if ( i == IDC_FC1 )
   {
      // concrete model is based on f'ci
      if ( m_ctrlEciCheck.GetCheck() == TRUE )
      {
         // Eci box is checked... user has input a value for Eci
         // Ec is based on the user input value of Eci and not f'c
         method = 0;
      }
      else
      {
         // Eci box is not checked... Eci is computed from f'ci, compute Ec from f'c
         method = 1;
      }
   }
   else
   {
      if (m_ctrlEcCheck.GetCheck() == FALSE) // not checked
      {
         method = 1;
      }
      else
      {
         method = -1; // don't compute.... it is user input
      }
   }

   if ( method == 0 )
   {
      // Ec is based on the user input value of Eci and not f'c
      CComPtr<IBroker> pBroker;
      EAFGetBroker(&pBroker);
      GET_IFACE2(pBroker,IEAFDisplayUnits,pDisplayUnits);

      CString strEci;
      m_ctrlEci.GetWindowText(strEci);
      Float64 Eci;
      sysTokenizer::ParseDouble(strEci,&Eci);
      Eci = ::ConvertToSysUnits(Eci,pDisplayUnits->GetModEUnit().UnitOfMeasure);

      CGirderSegmentDlg* pParent = (CGirderSegmentDlg*)GetParent();
      CPrecastSegmentData* pSegment = pParent->m_Girder.GetSegment(pParent->m_SegmentKey.segmentIndex);

      Float64 Ec = matACI209Concrete::ComputeEc28(Eci,m_AgeAtRelease,pSegment->Material.Concrete.A,pSegment->Material.Concrete.B);

      CString strEc;
      strEc.Format(_T("%s"),FormatDimension(Ec,pDisplayUnits->GetModEUnit(),false));
      m_ctrlEc.SetWindowText(strEc);
   }
   else if ( method == 1 )
   {
      // Compute Ec based on f'c
      CString strFc, strDensity, strK1, strK2;
      m_ctrlFc.GetWindowText(strFc);

      CComPtr<IBroker> pBroker;
      EAFGetBroker(&pBroker);
      GET_IFACE2(pBroker,IEAFDisplayUnits,pDisplayUnits);

      CGirderSegmentDlg* pParent = (CGirderSegmentDlg*)GetParent();
      CPrecastSegmentData* pSegment = pParent->m_Girder.GetSegment(pParent->m_SegmentKey.segmentIndex);

      strDensity.Format(_T("%s"),FormatDimension(pSegment->Material.Concrete.StrengthDensity,pDisplayUnits->GetDensityUnit(),false));
      strK1.Format(_T("%f"),pSegment->Material.Concrete.EcK1);
      strK2.Format(_T("%f"),pSegment->Material.Concrete.EcK2);

      CString strEc = CConcreteDetailsDlg::UpdateEc(strFc,strDensity,strK1,strK2);
      m_ctrlEc.SetWindowText(strEc);
   }
}

void CGirderSegmentGeneralPage::UpdateFc()
{
   int i = GetCheckedRadioButton(IDC_FC1,IDC_FC2);
   if ( i == IDC_FC1 )
   {
      // concrete model is based on f'ci... compute f'c
#pragma Reminder("UPDATE: assuming ACI209 concrete model")
      // Get f'ci from edit control
      CString strFci;
      m_ctrlFci.GetWindowText(strFci);

      CComPtr<IBroker> pBroker;
      EAFGetBroker(&pBroker);
      GET_IFACE2(pBroker,IEAFDisplayUnits,pDisplayUnits);

      Float64 fci;
      sysTokenizer::ParseDouble(strFci, &fci);
      fci = ::ConvertToSysUnits(fci,pDisplayUnits->GetStressUnit().UnitOfMeasure);

      CGirderSegmentDlg* pParent = (CGirderSegmentDlg*)GetParent();
      CPrecastSegmentData* pSegment = pParent->m_Girder.GetSegment(pParent->m_SegmentKey.segmentIndex);
      Float64 fc = matACI209Concrete::ComputeFc28(fci,m_AgeAtRelease,pSegment->Material.Concrete.A,pSegment->Material.Concrete.B);

      CString strFc;
      strFc.Format(_T("%s"),FormatDimension(fc,pDisplayUnits->GetStressUnit(),false));
      m_ctrlFc.SetWindowText(strFc);
   }
}

void CGirderSegmentGeneralPage::UpdateFci()
{
   int i = GetCheckedRadioButton(IDC_FC1,IDC_FC2);
   if ( i == IDC_FC2 )
   {
      // concrete model is based on f'ci... compute f'c
#pragma Reminder("UPDATE: assuming ACI209 concrete model")
      // Get f'c from edit control
      CString strFc;
      m_ctrlFc.GetWindowText(strFc);

      CComPtr<IBroker> pBroker;
      EAFGetBroker(&pBroker);
      GET_IFACE2(pBroker,IEAFDisplayUnits,pDisplayUnits);

      Float64 fc;
      sysTokenizer::ParseDouble(strFc, &fc);
      fc = ::ConvertToSysUnits(fc,pDisplayUnits->GetStressUnit().UnitOfMeasure);

      CGirderSegmentDlg* pParent = (CGirderSegmentDlg*)GetParent();
      CPrecastSegmentData* pSegment = pParent->m_Girder.GetSegment(pParent->m_SegmentKey.segmentIndex);

      matACI209Concrete concrete;
      concrete.SetTimeAtCasting(0);
      concrete.SetFc28(fc);
      concrete.SetA(pSegment->Material.Concrete.A);
      concrete.SetBeta(pSegment->Material.Concrete.B);
      concrete.SetStrengthDensity(pSegment->Material.Concrete.StrengthDensity);
      Float64 fci = concrete.GetFc(m_AgeAtRelease);

      CString strFci;
      strFci.Format(_T("%s"),FormatDimension(fci,pDisplayUnits->GetStressUnit(),false));
      m_ctrlFci.SetWindowText(strFci);
   }
}

void CGirderSegmentGeneralPage::OnMoreConcreteProperties() 
{
   AFX_MANAGE_STATE(AfxGetStaticModuleState());

   CConcreteDetailsDlg dlg;

   CDataExchange dx(this,TRUE);
   ExchangeConcreteData(&dx);

   CGirderSegmentDlg* pParent = (CGirderSegmentDlg*)GetParent();
   CPrecastSegmentData* pSegment = pParent->m_Girder.GetSegment(pParent->m_SegmentKey.segmentIndex);

   dlg.m_General.m_Type        = pSegment->Material.Concrete.Type;
   dlg.m_General.m_Fc          = pSegment->Material.Concrete.Fc;
   dlg.m_General.m_AggSize     = pSegment->Material.Concrete.MaxAggregateSize;
   dlg.m_General.m_bUserEc     = pSegment->Material.Concrete.bUserEc;
   dlg.m_General.m_Ds          = pSegment->Material.Concrete.StrengthDensity;
   dlg.m_General.m_Dw          = pSegment->Material.Concrete.WeightDensity;
   dlg.m_General.m_Ec          = pSegment->Material.Concrete.Ec;

   dlg.m_AASHTO.m_EccK1       = pSegment->Material.Concrete.EcK1;
   dlg.m_AASHTO.m_EccK2       = pSegment->Material.Concrete.EcK2;
   dlg.m_AASHTO.m_CreepK1     = pSegment->Material.Concrete.CreepK1;
   dlg.m_AASHTO.m_CreepK2     = pSegment->Material.Concrete.CreepK2;
   dlg.m_AASHTO.m_ShrinkageK1 = pSegment->Material.Concrete.ShrinkageK1;
   dlg.m_AASHTO.m_ShrinkageK2 = pSegment->Material.Concrete.ShrinkageK2;
   dlg.m_AASHTO.m_bHasFct     = pSegment->Material.Concrete.bHasFct;
   dlg.m_AASHTO.m_Fct         = pSegment->Material.Concrete.Fct;

   dlg.m_ACI.m_bUserParameters = pSegment->Material.Concrete.bACIUserParameters;
   dlg.m_ACI.m_A               = pSegment->Material.Concrete.A;
   dlg.m_ACI.m_B               = pSegment->Material.Concrete.B;
   dlg.m_ACI.m_CureMethod      = pSegment->Material.Concrete.CureMethod;
   dlg.m_ACI.m_CementType      = pSegment->Material.Concrete.CementType;
   dlg.m_ACI.m_TimeAtInitialStrength = ::ConvertToSysUnits(m_AgeAtRelease,unitMeasure::Day);
   dlg.m_ACI.m_fci             = pSegment->Material.Concrete.Fci;
   dlg.m_ACI.m_fc28            = pSegment->Material.Concrete.Fc;

#pragma Reminder("UPDATE: deal with CEB-FIP concrete models")

   dlg.m_General.m_strUserEc  = m_strUserEc;

   if ( dlg.DoModal() == IDOK )
   {
      pSegment->Material.Concrete.Type             = dlg.m_General.m_Type;
      pSegment->Material.Concrete.Fc               = dlg.m_General.m_Fc;
      pSegment->Material.Concrete.MaxAggregateSize = dlg.m_General.m_AggSize;
      pSegment->Material.Concrete.bUserEc          = dlg.m_General.m_bUserEc;
      pSegment->Material.Concrete.StrengthDensity  = dlg.m_General.m_Ds;
      pSegment->Material.Concrete.WeightDensity    = dlg.m_General.m_Dw;
      pSegment->Material.Concrete.Ec               = dlg.m_General.m_Ec;

      pSegment->Material.Concrete.EcK1             = dlg.m_AASHTO.m_EccK1;
      pSegment->Material.Concrete.EcK2             = dlg.m_AASHTO.m_EccK2;
      pSegment->Material.Concrete.CreepK1          = dlg.m_AASHTO.m_CreepK1;
      pSegment->Material.Concrete.CreepK2          = dlg.m_AASHTO.m_CreepK2;
      pSegment->Material.Concrete.ShrinkageK1      = dlg.m_AASHTO.m_ShrinkageK1;
      pSegment->Material.Concrete.ShrinkageK2      = dlg.m_AASHTO.m_ShrinkageK2;
      pSegment->Material.Concrete.bHasFct          = dlg.m_AASHTO.m_bHasFct;
      pSegment->Material.Concrete.Fct              = dlg.m_AASHTO.m_Fct;

      pSegment->Material.Concrete.bACIUserParameters = dlg.m_ACI.m_bUserParameters;
      pSegment->Material.Concrete.A                  = dlg.m_ACI.m_A;
      pSegment->Material.Concrete.B                  = dlg.m_ACI.m_B;
      pSegment->Material.Concrete.CureMethod         = dlg.m_ACI.m_CureMethod;
      pSegment->Material.Concrete.CementType         = dlg.m_ACI.m_CementType;
      pSegment->Material.Concrete.Fci                = dlg.m_ACI.m_fci;

#pragma Reminder("UPDATE: deal with CEB-FIP concrete models")

      m_strUserEc  = dlg.m_General.m_strUserEc;
      m_ctrlEc.SetWindowText(m_strUserEc);

      dx.m_bSaveAndValidate = FALSE;
      ExchangeConcreteData(&dx);

      UpdateFci();
      UpdateFc();
      UpdateEci();
      UpdateEc();

      UpdateConcreteControls();
   }
	
}

void CGirderSegmentGeneralPage::UpdateConcreteControls()
{
   int i = GetCheckedRadioButton(IDC_FC1,IDC_FC2);
   INT idFci[5] = {IDC_FCI,       IDC_FCI_UNIT,       IDC_MOD_ECI, IDC_ECI, IDC_ECI_UNIT};
   INT idFc[5]  = {IDC_GIRDER_FC, IDC_GIRDER_FC_UNIT, IDC_MOD_EC,  IDC_EC,  IDC_EC_UNIT };

   BOOL bEnableFci = (i == IDC_FC1);

   for ( int j = 0; j < 5; j++ )
   {
      GetDlgItem(idFci[j])->EnableWindow(  bEnableFci );
      GetDlgItem(idFc[j] )->EnableWindow( !bEnableFci );
   }

   if ( i == IDC_FC1 ) // input based on f'ci
   {
      m_ctrlEciCheck.SetCheck(m_ctrlEcCheck.GetCheck());
      m_ctrlEcCheck.SetCheck(FALSE); // can't check Ec
   }

   if ( i == IDC_FC2 ) // input is based on f'ci
   {
      m_ctrlEcCheck.SetCheck(m_ctrlEciCheck.GetCheck());
      m_ctrlEciCheck.SetCheck(FALSE); // can't check Eci
   }

   BOOL bEnable = m_ctrlEcCheck.GetCheck();
   GetDlgItem(IDC_EC)->EnableWindow(bEnable);
   GetDlgItem(IDC_EC_UNIT)->EnableWindow(bEnable);

   bEnable = m_ctrlEciCheck.GetCheck();
   GetDlgItem(IDC_ECI)->EnableWindow(bEnable);
   GetDlgItem(IDC_ECI_UNIT)->EnableWindow(bEnable);


   CGirderSegmentDlg* pParent = (CGirderSegmentDlg*)GetParent();
   CPrecastSegmentData* pSegment = pParent->m_Girder.GetSegment(pParent->m_SegmentKey.segmentIndex);
   CString strLabel( ConcreteDescription(pSegment->Material.Concrete));
   GetDlgItem(IDC_CONCRETE_TYPE_LABEL)->SetWindowText( strLabel );
}


BOOL CGirderSegmentGeneralPage::OnToolTipNotify(UINT id,NMHDR* pNMHDR, LRESULT* pResult)
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
         UpdateConcreteParametersToolTip();
         break;

      default:
         return FALSE;
      }

      ::SendMessage(pNMHDR->hwndFrom,TTM_SETDELAYTIME,TTDT_AUTOPOP,TOOLTIP_DURATION); // sets the display time to 10 seconds
      ::SendMessage(pNMHDR->hwndFrom,TTM_SETMAXTIPWIDTH,0,TOOLTIP_WIDTH); // makes it a multi-line tooltip
      pTTT->lpszText = m_strTip.GetBuffer();
      pTTT->hinst = NULL;
      return TRUE;
   }
   return FALSE;
}

void CGirderSegmentGeneralPage::UpdateConcreteParametersToolTip()
{
   CComPtr<IBroker> pBroker;
   EAFGetBroker(&pBroker);

   GET_IFACE2(pBroker,IEAFDisplayUnits,pDisplayUnits);

   CGirderSegmentDlg* pParent = (CGirderSegmentDlg*)GetParent();
   CPrecastSegmentData* pSegment = pParent->m_Girder.GetSegment(pParent->m_SegmentKey.segmentIndex);

   const unitmgtDensityData& density = pDisplayUnits->GetDensityUnit();
   const unitmgtLengthData&  aggsize = pDisplayUnits->GetComponentDimUnit();
   const unitmgtStressData&  stress  = pDisplayUnits->GetStressUnit();
   const unitmgtScalar&      scalar  = pDisplayUnits->GetScalarFormat();

#pragma Reminder("UPDATE: add ACI and CEB-FIP parameters to the tooltip text")

   CString strTip;
   strTip.Format(_T("%-20s %s\r\n%-20s %s\r\n%-20s %s\r\n%-20s %s"),
      _T("Type"), matConcrete::GetTypeName((matConcrete::Type)pSegment->Material.Concrete.Type,true).c_str(),
      _T("Unit Weight"),FormatDimension(pSegment->Material.Concrete.StrengthDensity,density),
      _T("Unit Weight (w/ reinforcement)"),  FormatDimension(pSegment->Material.Concrete.WeightDensity,density),
      _T("Max Aggregate Size"),  FormatDimension(pSegment->Material.Concrete.MaxAggregateSize,aggsize)
      );

   //if ( lrfdVersionMgr::ThirdEditionWith2005Interims <= lrfdVersionMgr::GetVersion() )
   //{
   //   // add K1 parameter
   //   CString strK1;
   //   strK1.Format(_T("\r\n%-20s %s"),
   //      _T("K1"),FormatScalar(pParent->m_GirderData.Material.K1,scalar));

   //   strTip += strK1;
   //}

   if ( pSegment->Material.Concrete.Type != pgsTypes::Normal && pSegment->Material.Concrete.bHasFct )
   {
      CString strLWC;
      strLWC.Format(_T("\r\n%-20s %s"),
         _T("fct"),FormatDimension(pSegment->Material.Concrete.Fct,stress));

      strTip += strLWC;
   }

   CString strPress(_T("\r\n\r\nPress button to edit"));
   strTip += strPress;

   m_strTip = strTip;
}


void CGirderSegmentGeneralPage::OnVariationTypeChanged()
{
   CComboBox* pCB = (CComboBox*)GetDlgItem(IDC_VARIATION_TYPE);
   int cursel = pCB->GetCurSel();
   pgsTypes::SegmentVariationType variationType = (pgsTypes::SegmentVariationType)pCB->GetItemData(cursel);

   CGirderSegmentDlg* pParent = (CGirderSegmentDlg*)GetParent();
   CPrecastSegmentData* pSegment = pParent->m_Girder.GetSegment(pParent->m_SegmentKey.segmentIndex);

   // if the variation type is changing to None, make sure the "default value" for the edit
   // control is the basic height of the segment in display units
   if ( variationType == pgsTypes::svtNone )
   {
      CComPtr<IBroker> pBroker;
      EAFGetBroker(&pBroker);
      GET_IFACE2(pBroker,IEAFDisplayUnits,pDisplayUnits);
      Float64 height = pSegment->GetBasicSegmentHeight();
      height = ::ConvertFromSysUnits(height,pDisplayUnits->GetSpanLengthUnit().UnitOfMeasure);
      m_ctrlSectionHeight[pgsTypes::sztLeftPrismatic].SetDefaultValue(height);
      m_ctrlSectionHeight[pgsTypes::sztRightPrismatic].SetDefaultValue(height);
   }

   UpdateSegmentVariationParameters(variationType);

   pSegment->SetVariationType(variationType);

   m_ctrlDrawSegment.Invalidate();
   m_ctrlDrawSegment.UpdateWindow();
}

void CGirderSegmentGeneralPage::GetSectionVariationControlState(BOOL* pbEnable)
{
   CComboBox* pCB = (CComboBox*)GetDlgItem(IDC_VARIATION_TYPE);
   int cursel = pCB->GetCurSel();
   pgsTypes::SegmentVariationType variationType = (pgsTypes::SegmentVariationType)pCB->GetItemData(cursel);
   GetSectionVariationControlState(variationType,pbEnable);
}

void CGirderSegmentGeneralPage::GetSectionVariationControlState(pgsTypes::SegmentVariationType variationType,BOOL* pbEnable)
{
   BOOL bEnable[4];
   if ( variationType == pgsTypes::svtNone )
   {
      bEnable[pgsTypes::sztLeftPrismatic]  = FALSE;
      bEnable[pgsTypes::sztLeftTapered]    = FALSE;
      bEnable[pgsTypes::sztRightTapered]   = FALSE;
      bEnable[pgsTypes::sztRightPrismatic] = FALSE;
   }
   else if (variationType == pgsTypes::svtLinear || variationType == pgsTypes::svtParabolic )
   {
      bEnable[pgsTypes::sztLeftPrismatic]  = TRUE;
      bEnable[pgsTypes::sztLeftTapered]    = FALSE;
      bEnable[pgsTypes::sztRightTapered]   = FALSE;
      bEnable[pgsTypes::sztRightPrismatic] = TRUE;
   }
   else if (variationType == pgsTypes::svtDoubleLinear || variationType == pgsTypes::svtDoubleParabolic )
   {
      bEnable[pgsTypes::sztLeftPrismatic]  = TRUE;
      bEnable[pgsTypes::sztLeftTapered]    = TRUE;
      bEnable[pgsTypes::sztRightTapered]   = TRUE;
      bEnable[pgsTypes::sztRightPrismatic] = TRUE;
   }
   //else if ( variationType == CPrecastSegmentData::General )
   //{
   //   ATLASSERT(false); // not implemented yet
   //   bEnable[pgsTypes::sztLeftPrismatic]  = FALSE;
   //   bEnable[pgsTypes::sztLeftTapered]    = FALSE;
   //   bEnable[pgsTypes::sztRightTapered]   = FALSE;
   //   bEnable[pgsTypes::sztRightPrismatic] = FALSE;
   //}
   else
   {
      ATLASSERT(false); // is there a new type
      bEnable[pgsTypes::sztLeftPrismatic]  = FALSE;
      bEnable[pgsTypes::sztLeftTapered]    = FALSE;
      bEnable[pgsTypes::sztRightTapered]   = FALSE;
      bEnable[pgsTypes::sztRightPrismatic] = FALSE;
   }

   pbEnable[0] = bEnable[0];
   pbEnable[1] = bEnable[1];
   pbEnable[2] = bEnable[2];
   pbEnable[3] = bEnable[3];
}

void CGirderSegmentGeneralPage::UpdateSegmentVariationParameters(pgsTypes::SegmentVariationType variationType)
{
   BOOL bEnable[4];
   GetSectionVariationControlState(variationType,&bEnable[0]);

   // if the input is enabled because of the section variation type,
   // show the default value, otherwise the input isn't applicable to
   // this input type and the edit control should be blank when disabled.
   for ( int i = 0; i < 4; i++ )
   {
      m_ctrlSectionLength[i].ShowDefaultWhenDisabled(bEnable[i]);
      m_ctrlSectionHeight[i].ShowDefaultWhenDisabled(bEnable[i]);
      m_ctrlBottomFlangeDepth[i].ShowDefaultWhenDisabled(bEnable[i]);
   }

   // if the variation type is None, then only the prismatic input elements should show the default value
   if ( variationType == pgsTypes::svtNone )
   {
      m_ctrlSectionHeight[pgsTypes::sztLeftPrismatic].ShowDefaultWhenDisabled(TRUE);
      m_ctrlBottomFlangeDepth[pgsTypes::sztLeftPrismatic].ShowDefaultWhenDisabled(TRUE);

      m_ctrlSectionHeight[pgsTypes::sztRightPrismatic].ShowDefaultWhenDisabled(TRUE);
      m_ctrlBottomFlangeDepth[pgsTypes::sztRightPrismatic].ShowDefaultWhenDisabled(TRUE);
   }

   BOOL bBottomFlange = IsDlgButtonChecked(IDC_BOTTOM_FLANGE_DEPTH);

   CGirderSegmentDlg* pParent = (CGirderSegmentDlg*)GetParent();
   CPrecastSegmentData* pSegment = pParent->m_Girder.GetSegment(pParent->m_SegmentKey.segmentIndex);
   BOOL bStartDepthFixed = FALSE;
   if ( pSegment->GetPrevSegment() && pSegment->GetPrevSegment()->GetVariationType() == pgsTypes::svtNone )
      bStartDepthFixed = TRUE;

   BOOL bEndDepthFixed = FALSE;
   if ( pSegment->GetNextSegment() && pSegment->GetNextSegment()->GetVariationType() == pgsTypes::svtNone )
      bEndDepthFixed = TRUE;

   GetDlgItem(IDC_BOTTOM_FLANGE_DEPTH)->EnableWindow(variationType == pgsTypes::svtNone ? FALSE : TRUE);

   GetDlgItem(IDC_LEFT_PRISMATIC_LABEL)->EnableWindow(bEnable[pgsTypes::sztLeftPrismatic]);
   m_ctrlSectionLength[pgsTypes::sztLeftPrismatic].EnableWindow(bEnable[pgsTypes::sztLeftPrismatic]);
   GetDlgItem(IDC_LEFT_PRISMATIC_LENGTH_UNIT)->EnableWindow(bEnable[pgsTypes::sztLeftPrismatic]);
   m_ctrlSectionHeight[pgsTypes::sztLeftPrismatic].EnableWindow(bStartDepthFixed ? FALSE : bEnable[pgsTypes::sztLeftPrismatic]);
   GetDlgItem(IDC_LEFT_PRISMATIC_HEIGHT_UNIT)->EnableWindow(bStartDepthFixed ? FALSE : bEnable[pgsTypes::sztLeftPrismatic]);
   m_ctrlBottomFlangeDepth[pgsTypes::sztLeftPrismatic].EnableWindow(bBottomFlange ? bEnable[pgsTypes::sztLeftPrismatic] : FALSE);
   GetDlgItem(IDC_LEFT_PRISMATIC_FLANGE_DEPTH_UNIT)->EnableWindow(bBottomFlange ? bEnable[pgsTypes::sztLeftPrismatic] : FALSE);

   GetDlgItem(IDC_LEFT_TAPERED_LABEL)->EnableWindow(bEnable[pgsTypes::sztLeftTapered]);
   m_ctrlSectionLength[pgsTypes::sztLeftTapered].EnableWindow(bEnable[pgsTypes::sztLeftTapered]);
   GetDlgItem(IDC_LEFT_TAPERED_LENGTH_UNIT)->EnableWindow(bEnable[pgsTypes::sztLeftTapered]);
   m_ctrlSectionHeight[pgsTypes::sztLeftTapered].EnableWindow(bEnable[pgsTypes::sztLeftTapered]);
   GetDlgItem(IDC_LEFT_TAPERED_HEIGHT_UNIT)->EnableWindow(bEnable[pgsTypes::sztLeftTapered]);
   m_ctrlBottomFlangeDepth[pgsTypes::sztLeftTapered].EnableWindow(bBottomFlange ? bEnable[pgsTypes::sztLeftTapered] : FALSE);
   GetDlgItem(IDC_LEFT_TAPERED_FLANGE_DEPTH_UNIT)->EnableWindow(bBottomFlange ? bEnable[pgsTypes::sztLeftTapered] : FALSE);

   GetDlgItem(IDC_RIGHT_TAPERED_LABEL)->EnableWindow(bEnable[pgsTypes::sztRightTapered]);
   m_ctrlSectionLength[pgsTypes::sztRightTapered].EnableWindow(bEnable[pgsTypes::sztRightTapered]);
   GetDlgItem(IDC_RIGHT_TAPERED_LENGTH_UNIT)->EnableWindow(bEnable[pgsTypes::sztRightTapered]);
   m_ctrlSectionHeight[pgsTypes::sztRightTapered].EnableWindow(bEnable[pgsTypes::sztRightTapered]);
   GetDlgItem(IDC_RIGHT_TAPERED_HEIGHT_UNIT)->EnableWindow(bEnable[pgsTypes::sztRightTapered]);
   m_ctrlBottomFlangeDepth[pgsTypes::sztRightTapered].EnableWindow(bBottomFlange ? bEnable[pgsTypes::sztRightTapered] : FALSE);
   GetDlgItem(IDC_RIGHT_TAPERED_FLANGE_DEPTH_UNIT)->EnableWindow(bBottomFlange ? bEnable[pgsTypes::sztRightTapered] : FALSE);

   GetDlgItem(IDC_RIGHT_PRISMATIC_LABEL)->EnableWindow(bEnable[pgsTypes::sztRightPrismatic]);
   m_ctrlSectionLength[pgsTypes::sztRightPrismatic].EnableWindow(bEnable[pgsTypes::sztRightPrismatic]);
   GetDlgItem(IDC_RIGHT_PRISMATIC_LENGTH_UNIT)->EnableWindow(bEnable[pgsTypes::sztRightPrismatic]);
   m_ctrlSectionHeight[pgsTypes::sztRightPrismatic].EnableWindow(bEndDepthFixed ? FALSE : bEnable[pgsTypes::sztRightPrismatic]);
   GetDlgItem(IDC_RIGHT_PRISMATIC_HEIGHT_UNIT)->EnableWindow(bEndDepthFixed ? FALSE : bEnable[pgsTypes::sztRightPrismatic]);
   m_ctrlBottomFlangeDepth[pgsTypes::sztRightPrismatic].EnableWindow(bBottomFlange ? bEnable[pgsTypes::sztRightPrismatic] : FALSE);
   GetDlgItem(IDC_RIGHT_PRISMATIC_FLANGE_DEPTH_UNIT)->EnableWindow(bBottomFlange ? bEnable[pgsTypes::sztRightPrismatic] : FALSE);
}

Float64 CGirderSegmentGeneralPage::GetBottomFlangeDepth(pgsTypes::SegmentZoneType segZone)
{
   CComPtr<IBroker> pBroker;
   EAFGetBroker(&pBroker);
   GET_IFACE2(pBroker,IEAFDisplayUnits,pDisplayUnits);

   pgsTypes::SegmentVariationType variationType = GetSegmentVariation();

   Float64 depth = 0;
   switch( segZone )
   {
   case pgsTypes::sztLeftPrismatic:
      depth = GetValue(IDC_LEFT_PRISMATIC_FLANGE_DEPTH,  pDisplayUnits->GetComponentDimUnit() );
      break;

   case pgsTypes::sztLeftTapered:
      depth = GetValue(IDC_LEFT_TAPERED_FLANGE_DEPTH, pDisplayUnits->GetComponentDimUnit() );
      break;

   case pgsTypes::sztRightTapered:
      depth = GetValue(IDC_RIGHT_TAPERED_FLANGE_DEPTH,  pDisplayUnits->GetComponentDimUnit() );
      break;

   case pgsTypes::sztRightPrismatic:
      depth = GetValue(IDC_RIGHT_PRISMATIC_FLANGE_DEPTH,  pDisplayUnits->GetComponentDimUnit() );
      break;

   default:
      ATLASSERT(false);
      // What is the bottom flange depth if variation is none?
      break;
   }

   return depth;
}

Float64 CGirderSegmentGeneralPage::GetHeight(pgsTypes::SegmentZoneType segZone)
{
   CComPtr<IBroker> pBroker;
   EAFGetBroker(&pBroker);
   GET_IFACE2(pBroker,IEAFDisplayUnits,pDisplayUnits);

   Float64 height = 0;
   switch( segZone )
   {
   case pgsTypes::sztLeftPrismatic:
      height = GetValue(IDC_LEFT_PRISMATIC_HEIGHT,  pDisplayUnits->GetSpanLengthUnit() );
      break;

   case pgsTypes::sztLeftTapered:
      height = GetValue(IDC_LEFT_TAPERED_HEIGHT, pDisplayUnits->GetSpanLengthUnit() );
      break;

   case pgsTypes::sztRightTapered:
      height = GetValue(IDC_RIGHT_TAPERED_HEIGHT,  pDisplayUnits->GetSpanLengthUnit() );
      break;

   case pgsTypes::sztRightPrismatic:
      height = GetValue(IDC_RIGHT_PRISMATIC_HEIGHT,  pDisplayUnits->GetSpanLengthUnit() );
      break;

   default:
      ATLASSERT(false);
      break;
   }

   return height;
}

Float64 CGirderSegmentGeneralPage::GetLength(pgsTypes::SegmentZoneType segZone)
{
   CComPtr<IBroker> pBroker;
   EAFGetBroker(&pBroker);
   GET_IFACE2(pBroker,IEAFDisplayUnits,pDisplayUnits);

   Float64 length = 0;
   switch( segZone )
   {
   case pgsTypes::sztLeftPrismatic:
      length = GetValue(IDC_LEFT_PRISMATIC_LENGTH,  pDisplayUnits->GetSpanLengthUnit() );
      break;

   case pgsTypes::sztLeftTapered:
      length = GetValue(IDC_LEFT_TAPERED_LENGTH, pDisplayUnits->GetSpanLengthUnit() );
      break;

   case pgsTypes::sztRightTapered:
      length = GetValue(IDC_RIGHT_TAPERED_LENGTH,  pDisplayUnits->GetSpanLengthUnit() );
      break;

   case pgsTypes::sztRightPrismatic:
      length = GetValue(IDC_RIGHT_PRISMATIC_LENGTH,  pDisplayUnits->GetSpanLengthUnit() );
      break;

   default:
      ATLASSERT(false);
      break;
   }

   return length;
}

Float64 CGirderSegmentGeneralPage::GetValue(UINT nIDC,const unitmgtLengthData& lengthUnit)
{
   CWnd* pWnd = GetDlgItem(nIDC);
   const int TEXT_BUFFER_SIZE = 400;
   TCHAR szBuffer[TEXT_BUFFER_SIZE];
   pWnd->GetWindowText(szBuffer,TEXT_BUFFER_SIZE);
   Float64 d;
   if ( _sntscanf_s(szBuffer,_countof(szBuffer),_T("%lf"), &d) != 1 )
      return 0;
   
   d = ::ConvertToSysUnits(d,lengthUnit.UnitOfMeasure );
   return d;
}

Float64 CGirderSegmentGeneralPage::GetSegmentLength()
{
   CComPtr<IBroker> pBroker;
   EAFGetBroker(&pBroker);
   GET_IFACE2(pBroker,IBridge,pBridge);

   CGirderSegmentDlg* pParent = (CGirderSegmentDlg*)GetParent();

   CSegmentKey segmentKey(pParent->m_SegmentKey.groupIndex,pParent->m_Girder.GetIndex(),pParent->m_SegmentKey.segmentIndex);
   return pBridge->GetSegmentLayoutLength(segmentKey);
}

pgsTypes::SegmentVariationType CGirderSegmentGeneralPage::GetSegmentVariation()
{
   CComboBox* pCB = (CComboBox*)GetDlgItem(IDC_VARIATION_TYPE);
   int cursel = pCB->GetCurSel();

   pgsTypes::SegmentVariationType variationType;
   variationType = (pgsTypes::SegmentVariationType)pCB->GetItemData(cursel);

   return variationType;
}

void CGirderSegmentGeneralPage::OnSegmentChanged()
{
   CGirderSegmentDlg* pParent = (CGirderSegmentDlg*)GetParent();
   CPrecastSegmentData* pSegment = pParent->m_Girder.GetSegment(pParent->m_SegmentKey.segmentIndex);

   for ( int i = 0; i < 4; i++ )
   {
      pSegment->SetVariationParameters(pgsTypes::SegmentZoneType(i),
                                       GetLength((pgsTypes::SegmentZoneType)i),
                                       GetHeight((pgsTypes::SegmentZoneType)i),
                                       GetBottomFlangeDepth((pgsTypes::SegmentZoneType)i) );
   }

   m_ctrlDrawSegment.Invalidate();
   m_ctrlDrawSegment.UpdateWindow();
}

const CSplicedGirderData* CGirderSegmentGeneralPage::GetGirder() const
{
   CGirderSegmentDlg* pParent = (CGirderSegmentDlg*)GetParent();
   return &pParent->m_Girder;
}

const CSegmentKey& CGirderSegmentGeneralPage::GetSegmentKey() const
{
   CGirderSegmentDlg* pParent = (CGirderSegmentDlg*)GetParent();
   return pParent->m_SegmentKey;
}

SegmentIDType CGirderSegmentGeneralPage::GetSegmentID() const
{
   CGirderSegmentDlg* pParent = (CGirderSegmentDlg*)GetParent();
   return pParent->m_SegmentID;
}

void CGirderSegmentGeneralPage::FillEventList()
{
   CComboBox* pcbConstruct = (CComboBox*)GetDlgItem(IDC_CONSTRUCTION_EVENT);
   CComboBox* pcbErect = (CComboBox*)GetDlgItem(IDC_ERECTION_EVENT);

   int constructIdx = pcbConstruct->GetCurSel();
   int erectIdx = pcbErect->GetCurSel();

   pcbConstruct->ResetContent();
   pcbErect->ResetContent();

   CComPtr<IBroker> pBroker;
   EAFGetBroker(&pBroker);
   GET_IFACE2(pBroker,IBridgeDescription,pIBridgeDesc);
   const CTimelineManager* pTimelineMgr = pIBridgeDesc->GetTimelineManager();

   EventIndexType nEvents = pTimelineMgr->GetEventCount();
   for ( EventIndexType eventIdx = 0; eventIdx < nEvents; eventIdx++ )
   {
      const CTimelineEvent* pTimelineEvent = pTimelineMgr->GetEventByIndex(eventIdx);

      CString label;
      label.Format(_T("Event %d: %s"),LABEL_EVENT(eventIdx),pTimelineEvent->GetDescription());

      pcbConstruct->SetItemData(pcbConstruct->AddString(label),eventIdx);
      pcbErect->SetItemData(pcbErect->AddString(label),eventIdx);
   }

   CString strNewEvent((LPCSTR)IDS_CREATE_NEW_EVENT);
   pcbConstruct->SetItemData(pcbConstruct->AddString(strNewEvent),CREATE_TIMELINE_EVENT);
   pcbErect->SetItemData(pcbErect->AddString(strNewEvent),CREATE_TIMELINE_EVENT);

   if ( constructIdx != CB_ERR )
      pcbConstruct->SetCurSel(constructIdx);

   if ( erectIdx != CB_ERR )
      pcbErect->SetCurSel(erectIdx);
}

void CGirderSegmentGeneralPage::OnConstructionEventChanging()
{
   CComboBox* pCB = (CComboBox*)GetDlgItem(IDC_CONSTRUCTION_EVENT);
   m_PrevConstructionEventIdx = pCB->GetCurSel();
}

void CGirderSegmentGeneralPage::OnConstructionEventChanged()
{
   CComboBox* pCB = (CComboBox*)GetDlgItem(IDC_CONSTRUCTION_EVENT);
   int curSel = pCB->GetCurSel();
   EventIndexType idx = (IndexType)pCB->GetItemData(curSel);
   if ( idx == CREATE_TIMELINE_EVENT )
   {
      EventIndexType eventIdx = CreateEvent();

      if ( eventIdx != INVALID_INDEX )
      {
         CComPtr<IBroker> pBroker;
         EAFGetBroker(&pBroker);
         GET_IFACE2(pBroker,IBridgeDescription,pIBridgeDesc);
         const CTimelineManager* pTimelineMgr = pIBridgeDesc->GetTimelineManager();

#pragma Reminder("UPDATE")
         // need to disable segment construction in prev event
         // and then enable it in the event that has been selected
         //CTimelineEvent* pTimelineEvent
         CTimelineEvent timelineEvent = *pTimelineMgr->GetEventByIndex(eventIdx);
         timelineEvent.GetConstructSegmentsActivity().Enable(true);
         // also need to updated m_AgeAtRelease and the concrete model
         // for the new construct segment activity parameters

         FillEventList();

         pCB->SetCurSel((int)eventIdx);
      }
      else
      {
         pCB->SetCurSel(m_PrevConstructionEventIdx);
      }
   }
}
   
void CGirderSegmentGeneralPage::OnErectionEventChanging()
{
   CComboBox* pCB = (CComboBox*)GetDlgItem(IDC_ERECTION_EVENT);
   m_PrevErectionEventIdx = pCB->GetCurSel();
}

void CGirderSegmentGeneralPage::OnErectionEventChanged()
{
   CComboBox* pCB = (CComboBox*)GetDlgItem(IDC_ERECTION_EVENT);
   int curSel = pCB->GetCurSel();
   EventIndexType idx = (IndexType)pCB->GetItemData(curSel);
   if ( idx == CREATE_TIMELINE_EVENT )
   {
      EventIndexType eventIdx = CreateEvent();

      if (eventIdx != INVALID_INDEX )
      {
         CComPtr<IBroker> pBroker;
         EAFGetBroker(&pBroker);
         GET_IFACE2(pBroker,IBridgeDescription,pIBridgeDesc);
         const CTimelineManager* pTimelineMgr = pIBridgeDesc->GetTimelineManager();
         CTimelineEvent timelineEvent = *pTimelineMgr->GetEventByIndex(eventIdx);
         timelineEvent.GetErectSegmentsActivity().Enable(true);

         timelineEvent.GetErectSegmentsActivity().AddSegment( GetSegmentID() );
         pIBridgeDesc->SetEventByIndex(eventIdx,timelineEvent);

         FillEventList();

         pCB->SetCurSel((int)eventIdx);
      }
      else
      {
         pCB->SetCurSel(m_PrevErectionEventIdx);
      }
   }
}

EventIndexType CGirderSegmentGeneralPage::CreateEvent()
{
   CComPtr<IBroker> pBroker;
   EAFGetBroker(&pBroker);
   GET_IFACE2(pBroker,IBridgeDescription,pIBridgeDesc);
   const CTimelineManager* pTimelineMgr = pIBridgeDesc->GetTimelineManager();

   CTimelineEventDlg dlg(pTimelineMgr,FALSE);
   if ( dlg.DoModal() == IDOK )
   {
      return pIBridgeDesc->AddTimelineEvent(dlg.m_TimelineEvent);
  }

   return INVALID_INDEX;
}

void CGirderSegmentGeneralPage::OnConcreteStrength()
{
   UpdateConcreteControls();
}

void CGirderSegmentGeneralPage::InitBottomFlangeDepthControls()
{
   CGirderSegmentDlg* pParent = (CGirderSegmentDlg*)GetParent();
   const GirderLibraryEntry* pLibEntry = pParent->m_Girder.GetGirderLibraryEntry();
   CComPtr<IBeamFactory> factory;
   pLibEntry->GetBeamFactory(&factory);
   CComQIPtr<ISplicedBeamFactory,&IID_ISplicedBeamFactory> splicedBeamFactory(factory);
   ATLASSERT(splicedBeamFactory != NULL); // spliced girders must support the ISplicedBeamFactory interface
   if ( !splicedBeamFactory || !splicedBeamFactory->CanBottomFlangeDepthVary() )
   {
      // bottom flange depth is not variable... hide the controls
      GetDlgItem( IDC_BOTTOM_FLANGE_DEPTH               )->ShowWindow(SW_HIDE);
      GetDlgItem( IDC_LEFT_PRISMATIC_FLANGE_DEPTH       )->ShowWindow(SW_HIDE);
      GetDlgItem( IDC_LEFT_PRISMATIC_FLANGE_DEPTH_UNIT  )->ShowWindow(SW_HIDE);
      GetDlgItem( IDC_LEFT_TAPERED_FLANGE_DEPTH         )->ShowWindow(SW_HIDE);
      GetDlgItem( IDC_LEFT_TAPERED_FLANGE_DEPTH_UNIT    )->ShowWindow(SW_HIDE);
      GetDlgItem( IDC_RIGHT_TAPERED_FLANGE_DEPTH        )->ShowWindow(SW_HIDE);
      GetDlgItem( IDC_RIGHT_TAPERED_FLANGE_DEPTH_UNIT   )->ShowWindow(SW_HIDE);
      GetDlgItem( IDC_RIGHT_PRISMATIC_FLANGE_DEPTH      )->ShowWindow(SW_HIDE);
      GetDlgItem( IDC_RIGHT_PRISMATIC_FLANGE_DEPTH_UNIT )->ShowWindow(SW_HIDE);
   }
}

void CGirderSegmentGeneralPage::InitEndBlockControls()
{
   CGirderSegmentDlg* pParent = (CGirderSegmentDlg*)GetParent();
   CPrecastSegmentData* pSegment = pParent->m_Girder.GetSegment(pParent->m_SegmentKey.segmentIndex);

   const GirderLibraryEntry* pLibEntry = pParent->m_Girder.GetGirderLibraryEntry();
   CComPtr<IBeamFactory> factory;
   pLibEntry->GetBeamFactory(&factory);
   CComQIPtr<ISplicedBeamFactory,&IID_ISplicedBeamFactory> splicedBeamFactory(factory);

   bool bSupportsEndBlocks = splicedBeamFactory->SupportsEndBlocks();

   if ( bSupportsEndBlocks )
   {
      BOOL bEnableLeft  = ( pSegment->GetPrevSegment() == NULL ? TRUE : FALSE);
      BOOL bEnableRight = ( pSegment->GetNextSegment() == NULL ? TRUE : FALSE);

      GetDlgItem(IDC_LEFT_END_BLOCK_LABEL)->EnableWindow(bEnableLeft);
      GetDlgItem(IDC_LEFT_END_BLOCK_LENGTH)->EnableWindow(bEnableLeft);
      GetDlgItem(IDC_LEFT_END_BLOCK_LENGTH_UNIT)->EnableWindow(bEnableLeft);
      GetDlgItem(IDC_LEFT_END_BLOCK_TRANSITION)->EnableWindow(bEnableLeft);
      GetDlgItem(IDC_LEFT_END_BLOCK_TRANSITION_UNIT)->EnableWindow(bEnableLeft);
      GetDlgItem(IDC_LEFT_END_BLOCK_WIDTH)->EnableWindow(bEnableLeft);
      GetDlgItem(IDC_LEFT_END_BLOCK_WIDTH_UNIT)->EnableWindow(bEnableLeft);

      GetDlgItem(IDC_RIGHT_END_BLOCK_LABEL)->EnableWindow(bEnableRight);
      GetDlgItem(IDC_RIGHT_END_BLOCK_LENGTH)->EnableWindow(bEnableRight);
      GetDlgItem(IDC_RIGHT_END_BLOCK_LENGTH_UNIT)->EnableWindow(bEnableRight);
      GetDlgItem(IDC_RIGHT_END_BLOCK_TRANSITION)->EnableWindow(bEnableRight);
      GetDlgItem(IDC_RIGHT_END_BLOCK_TRANSITION_UNIT)->EnableWindow(bEnableRight);
      GetDlgItem(IDC_RIGHT_END_BLOCK_WIDTH)->EnableWindow(bEnableRight);
      GetDlgItem(IDC_RIGHT_END_BLOCK_WIDTH_UNIT)->EnableWindow(bEnableRight);
   }
   else
   {
      GetDlgItem(IDC_END_BLOCK_LENGTH_LABEL)->ShowWindow(SW_HIDE);
      GetDlgItem(IDC_END_BLOCK_TRANSITION_LABEL)->ShowWindow(SW_HIDE);
      GetDlgItem(IDC_END_BLOCK_WIDTH_LABEL)->ShowWindow(SW_HIDE);

      GetDlgItem(IDC_LEFT_END_BLOCK_LABEL)->ShowWindow(SW_HIDE);
      GetDlgItem(IDC_LEFT_END_BLOCK_LENGTH)->ShowWindow(SW_HIDE);
      GetDlgItem(IDC_LEFT_END_BLOCK_LENGTH_UNIT)->ShowWindow(SW_HIDE);
      GetDlgItem(IDC_LEFT_END_BLOCK_TRANSITION)->ShowWindow(SW_HIDE);
      GetDlgItem(IDC_LEFT_END_BLOCK_TRANSITION_UNIT)->ShowWindow(SW_HIDE);
      GetDlgItem(IDC_LEFT_END_BLOCK_WIDTH)->ShowWindow(SW_HIDE);
      GetDlgItem(IDC_LEFT_END_BLOCK_WIDTH_UNIT)->ShowWindow(SW_HIDE);

      GetDlgItem(IDC_RIGHT_END_BLOCK_LABEL)->ShowWindow(SW_HIDE);
      GetDlgItem(IDC_RIGHT_END_BLOCK_LENGTH)->ShowWindow(SW_HIDE);
      GetDlgItem(IDC_RIGHT_END_BLOCK_LENGTH_UNIT)->ShowWindow(SW_HIDE);
      GetDlgItem(IDC_RIGHT_END_BLOCK_TRANSITION)->ShowWindow(SW_HIDE);
      GetDlgItem(IDC_RIGHT_END_BLOCK_TRANSITION_UNIT)->ShowWindow(SW_HIDE);
      GetDlgItem(IDC_RIGHT_END_BLOCK_WIDTH)->ShowWindow(SW_HIDE);
      GetDlgItem(IDC_RIGHT_END_BLOCK_WIDTH_UNIT)->ShowWindow(SW_HIDE);
   }
}

void CGirderSegmentGeneralPage::OnBnClickedBottomFlangeDepth()
{
   BOOL bEnable[4];
   GetSectionVariationControlState(&bEnable[0]);

   // if the input is enabled because of the section variation type,
   // show the default value, otherwise the input isn't applicable to
   // this input type and the edit control should be blank when disabled.
   for ( int i = 0; i < 4; i++ )
   {
      m_ctrlBottomFlangeDepth[i].ShowDefaultWhenDisabled(bEnable[i]);
   }

   BOOL bChecked = IsDlgButtonChecked(IDC_BOTTOM_FLANGE_DEPTH);
   if ( !bChecked )
   {
      bEnable[0] = FALSE;
      bEnable[1] = FALSE;
      bEnable[2] = FALSE;
      bEnable[3] = FALSE;
   }

   CGirderSegmentDlg* pParent = (CGirderSegmentDlg*)GetParent();
   CPrecastSegmentData* pSegment = pParent->m_Girder.GetSegment(pParent->m_SegmentKey.segmentIndex);
   BOOL bStartDepthFixed = FALSE;
   if ( pSegment->GetPrevSegment() && pSegment->GetPrevSegment()->GetVariationType() == pgsTypes::svtNone )
      bStartDepthFixed = TRUE;

   BOOL bEndDepthFixed = FALSE;
   if ( pSegment->GetNextSegment() && pSegment->GetNextSegment()->GetVariationType() == pgsTypes::svtNone )
      bEndDepthFixed = TRUE;

   m_ctrlBottomFlangeDepth[pgsTypes::sztLeftPrismatic].EnableWindow(bStartDepthFixed ? FALSE : bEnable[pgsTypes::sztLeftPrismatic]);
   GetDlgItem( IDC_LEFT_PRISMATIC_FLANGE_DEPTH_UNIT  )->EnableWindow(bEndDepthFixed ? FALSE : bEnable[pgsTypes::sztLeftPrismatic]);
   
   m_ctrlBottomFlangeDepth[pgsTypes::sztLeftTapered].EnableWindow(bEnable[pgsTypes::sztLeftTapered]);
   GetDlgItem( IDC_LEFT_TAPERED_FLANGE_DEPTH_UNIT    )->EnableWindow(bEnable[pgsTypes::sztLeftTapered]);
   
   m_ctrlBottomFlangeDepth[pgsTypes::sztRightTapered].EnableWindow(bEnable[pgsTypes::sztRightTapered]);
   GetDlgItem( IDC_RIGHT_TAPERED_FLANGE_DEPTH_UNIT   )->EnableWindow(bEnable[pgsTypes::sztRightTapered]);
   
   m_ctrlBottomFlangeDepth[pgsTypes::sztRightPrismatic].EnableWindow(bEndDepthFixed ? FALSE : bEnable[pgsTypes::sztRightPrismatic]);
   GetDlgItem( IDC_RIGHT_PRISMATIC_FLANGE_DEPTH_UNIT )->EnableWindow(bEndDepthFixed ? FALSE : bEnable[pgsTypes::sztRightPrismatic]);

   pSegment->EnableVariableBottomFlangeDepth(bChecked == TRUE ? true : false);
}
