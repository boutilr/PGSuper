///////////////////////////////////////////////////////////////////////
// PGSuper - Prestressed Girder SUPERstructure Design and Analysis
// Copyright � 1999-2014  Washington State Department of Transportation
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

// GirderDescDlg.cpp : implementation file
//

#include "PGSuperAppPlugin\stdafx.h"
#include "PGSuperAppPlugin\Resource.h"
#include "PGSuperDoc.h"
#include "GirderDescDlg.h"
#include <IFace\Bridge.h>
#include <IFace\Project.h>
#include <IFace\GirderHandling.h>
#include <IFace\GirderHandlingSpecCriteria.h>
#include <PgsExt\BridgeDescription.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CGirderDescDlg

IMPLEMENT_DYNAMIC(CGirderDescDlg, CPropertySheet)

CGirderDescDlg::CGirderDescDlg(SpanIndexType spanIdx,GirderIndexType gdrIdx,LPCTSTR strGirderName,CWnd* pParentWnd, UINT iSelectPage)
	:CPropertySheet(_T(""), pParentWnd, iSelectPage)
{
   m_CurrentSpanIdx   = spanIdx;
   m_CurrentGirderIdx = gdrIdx;
   m_strGirderName = strGirderName;

   CString strTitle;
   strTitle.Format(_T("Girder Details for Span %d, Girder %s"),LABEL_SPAN(m_CurrentSpanIdx),LABEL_GIRDER(m_CurrentGirderIdx));
   SetTitle(strTitle);

   Init();
}

CGirderDescDlg::~CGirderDescDlg()
{
   DestroyExtensionPages();
}

INT_PTR CGirderDescDlg::DoModal()
{
   INT_PTR result = CPropertySheet::DoModal();
   if ( result == IDOK )
   {
      NotifyExtensionPages();
   }

   return result;
}

void CGirderDescDlg::Init()
{
   m_psh.dwFlags |= PSH_HASHELP | PSH_NOAPPLYNOW;

   m_General.m_psp.dwFlags   |= PSP_HASHELP;
   m_Prestress.m_psp.dwFlags |= PSP_HASHELP;
   m_Shear.m_psp.dwFlags     |= PSP_HASHELP;
   m_LongRebar.m_psp.dwFlags |= PSP_HASHELP;
   m_Lifting.m_psp.dwFlags   |= PSP_HASHELP;
   m_Debond.m_psp.dwFlags    |= PSP_HASHELP;
   m_Rating.m_psp.dwFlags    |= PSP_HASHELP;


   AddPage( &m_General );
   AddPage( &m_Prestress );

   CComPtr<IBroker> pBroker;
   EAFGetBroker(&pBroker);

   GET_IFACE2(pBroker,IStrandGeometry,pStrandGeom);
   GET_IFACE2(pBroker,ISpecification,pSpec);
   GET_IFACE2(pBroker,ILibrary,pLib);
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry(pSpec->GetSpecification().c_str());
   this->AddAdditionalPropertyPages( pSpecEntry->AllowStraightStrandExtensions(), pStrandGeom->CanDebondStrands(m_CurrentSpanIdx,m_CurrentGirderIdx,pgsTypes::Straight) );

   CreateExtensionPages();
}

void CGirderDescDlg::CreateExtensionPages()
{
   CEAFDocument* pEAFDoc = EAFGetDocument();
   CPGSuperDoc* pDoc = (CPGSuperDoc*)pEAFDoc;

   std::map<IDType,IEditGirderCallback*> callbacks = pDoc->GetEditGirderCallbacks();
   std::map<IDType,IEditGirderCallback*>::iterator callbackIter(callbacks.begin());
   std::map<IDType,IEditGirderCallback*>::iterator callbackIterEnd(callbacks.end());
   for ( ; callbackIter != callbackIterEnd; callbackIter++ )
   {
      IEditGirderCallback* pCallback = callbackIter->second;
      CPropertyPage* pPage = pCallback->CreatePropertyPage(this);
      if ( pPage )
      {
         m_ExtensionPages.push_back( std::make_pair(pCallback,pPage) );
         AddPage(pPage);
      }
   }
}

void CGirderDescDlg::DestroyExtensionPages()
{
   std::vector<std::pair<IEditGirderCallback*,CPropertyPage*>>::iterator pageIter(m_ExtensionPages.begin());
   std::vector<std::pair<IEditGirderCallback*,CPropertyPage*>>::iterator pageIterEnd(m_ExtensionPages.end());
   for ( ; pageIter != pageIterEnd; pageIter++ )
   {
      CPropertyPage* pPage = pageIter->second;
      delete pPage;
   }
   m_ExtensionPages.clear();
}

txnTransaction* CGirderDescDlg::GetExtensionPageTransaction()
{
   if ( 0 < m_Macro.GetTxnCount() )
      return m_Macro.CreateClone();
   else
      return NULL;
}

void CGirderDescDlg::NotifyExtensionPages()
{
   std::vector<std::pair<IEditGirderCallback*,CPropertyPage*>>::iterator pageIter(m_ExtensionPages.begin());
   std::vector<std::pair<IEditGirderCallback*,CPropertyPage*>>::iterator pageIterEnd(m_ExtensionPages.end());
   for ( ; pageIter != pageIterEnd; pageIter++ )
   {
      IEditGirderCallback* pCallback = pageIter->first;
      CPropertyPage* pPage = pageIter->second;
      txnTransaction* pTxn = pCallback->OnOK(pPage,this);
      if ( pTxn )
      {
         m_Macro.AddTransaction(pTxn);
      }
   }
}

BEGIN_MESSAGE_MAP(CGirderDescDlg, CPropertySheet)
	//{{AFX_MSG_MAP(CGirderDescDlg)
		// NOTE - the ClassWizard will add and remove mapping macros here.
	//}}AFX_MSG_MAP
	ON_MESSAGE(WM_KICKIDLE,OnKickIdle)
END_MESSAGE_MAP()

LRESULT CGirderDescDlg::OnKickIdle(WPARAM wp, LPARAM lp)
{
   // The CPropertySheet::OnKickIdle method calls GetActivePage()
   // which doesn't work with extension pages. Since GetActivePage
   // is not virtual, we have to replace the implementation of
   // OnKickIdle.
   // The same problem exists with OnCommandHelp

	ASSERT_VALID(this);

	CPropertyPage* pPage = GetPage(GetActiveIndex());

	/* Forward the message on to the active page of the property sheet */
	if( pPage != NULL )
	{
		//ASSERT_VALID(pPage);
		return pPage->SendMessage( WM_KICKIDLE, wp, lp );
	}
	else
		return 0;
}

/////////////////////////////////////////////////////////////////////////////
// CGirderDescDlg message handlers


void CGirderDescDlg::DoUpdate()
{
   CComPtr<IBroker> pBroker;
   EAFGetBroker(&pBroker);

   GET_IFACE2(pBroker,IGirderData,pGirderData);
   GET_IFACE2(pBroker,IShear,pShear);
   GET_IFACE2(pBroker,ILongitudinalRebar,pLongitudinaRebar);
   GET_IFACE2(pBroker,IGirderLifting,pGirderLifting);
   GET_IFACE2(pBroker,IGirderHauling,pGirderHauling);
   GET_IFACE2(pBroker,IBridge,pBridge);
   GET_IFACE2(pBroker,IBridgeDescription,pIBridgeDesc);

   const CBridgeDescription* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();

   // Setup girder data for our pages
   m_General.m_bUseSameGirderType = pBridgeDesc->UseSameGirderForEntireBridge();

   m_GirderData = *pGirderData->GetGirderData(m_CurrentSpanIdx,m_CurrentGirderIdx);

   // shear page
   m_Shear.m_CurGrdName = pBridgeDesc->GetSpan(m_CurrentSpanIdx)->GetGirderTypes()->GetGirderName(m_CurrentGirderIdx);
   m_Shear.m_ShearData = pShear->GetShearData(m_CurrentSpanIdx,m_CurrentGirderIdx);

   // longitudinal rebar page
   m_LongRebar.m_CurGrdName = m_Shear.m_CurGrdName;
   m_LongRebar.m_RebarData  = pLongitudinaRebar->GetLongitudinalRebarData(m_CurrentSpanIdx,m_CurrentGirderIdx);

   // Guts of program can now handle unequal overhangs
   Float64 lifting_loc = pGirderLifting->GetLeftLiftingLoopLocation(m_CurrentSpanIdx,m_CurrentGirderIdx);
   m_Lifting.m_LiftingLocation = lifting_loc;

   Float64 trailingOverhang = pGirderHauling->GetTrailingOverhang(m_CurrentSpanIdx,m_CurrentGirderIdx);
   m_Lifting.m_TrailingOverhang = trailingOverhang;

   Float64 leadingOverhang = pGirderHauling->GetLeadingOverhang(m_CurrentSpanIdx,m_CurrentGirderIdx);
   m_Lifting.m_LeadingOverhang = leadingOverhang;
}

BOOL CGirderDescDlg::OnInitDialog() 
{
	DoUpdate();

	BOOL bResult = CPropertySheet::OnInitDialog();
		
   AFX_MANAGE_STATE(AfxGetStaticModuleState());
   HICON hIcon = (HICON)LoadImage(AfxGetResourceHandle(),MAKEINTRESOURCE(IDI_EDIT_GIRDER),IMAGE_ICON,0,0,LR_DEFAULTSIZE);
   SetIcon(hIcon,FALSE);

   SetDebondTabName();

   return bResult;
}

void CGirderDescDlg::SetDebondTabName()
{
   int index = GetPageIndex(&m_Debond);
   if ( index < 0 )
      return; // not using the debond tab

   CComPtr<IBroker> pBroker;
   EAFGetBroker(&pBroker);

   CTabCtrl* pTab = GetTabControl();
   TC_ITEM ti;
   ti.mask = TCIF_TEXT;

   GET_IFACE2(pBroker,IStrandGeometry,pStrandGeometry);
   bool bCanDebond = pStrandGeometry->CanDebondStrands(m_strGirderName.c_str(),pgsTypes::Straight);
   if ( bCanDebond )
   {
      ti.pszText = _T("Debonding");
   }
   else
   {
      ti.pszText = _T("Strand Extensions");
   }
   
   pTab->SetItem(index,&ti);
}

StrandIndexType CGirderDescDlg::GetStraightStrandCount()
{
   return  m_GirderData.PrestressData.GetNstrands(pgsTypes::Straight);
}

StrandIndexType CGirderDescDlg::GetHarpedStrandCount()
{
   return  m_GirderData.PrestressData.GetNstrands(pgsTypes::Harped);
}

ConfigStrandFillVector CGirderDescDlg::ComputeStrandFillVector(pgsTypes::StrandType type)
{
   CComPtr<IBroker> pBroker;
   EAFGetBroker(&pBroker);
   GET_IFACE2(pBroker,IStrandGeometry,pStrandGeometry);

   int filltype = m_GirderData.PrestressData.GetNumPermStrandsType();

   if (filltype == NPS_DIRECT_SELECTION)
   {
      // first get in girderdata format
      const DirectStrandFillCollection* pDirectFillData(NULL);
      if (type==pgsTypes::Straight)
      {
         pDirectFillData = m_GirderData.PrestressData.GetDirectStrandFillStraight();
      }
      else if (type==pgsTypes::Harped)
      {
         pDirectFillData = m_GirderData.PrestressData.GetDirectStrandFillHarped();
      }
      if (type==pgsTypes::Temporary)
      {
         pDirectFillData = m_GirderData.PrestressData.GetDirectStrandFillTemporary();
      }

      // Convert girderdata to config
      // Start with unfilled grid 
      ConfigStrandFillVector vec(pStrandGeometry->ComputeStrandFill(m_strGirderName.c_str(), type, 0));
      GridIndexType gridsize = vec.size();

      if(pDirectFillData!=NULL)
      {
         DirectStrandFillCollection::const_iterator it = pDirectFillData->begin();
         DirectStrandFillCollection::const_iterator itend = pDirectFillData->end();
         while(it != itend)
         {
            GridIndexType idx = it->permStrandGridIdx;
            if (idx<gridsize)
            {
               vec[idx] = it->numFilled;
            }
            else
               ATLASSERT(0); 

            it++;
         }
      }

      return vec;
   }
   else
   {
      // Continuous fill
      StrandIndexType Ns = m_GirderData.PrestressData.GetNstrands(type);

      return pStrandGeometry->ComputeStrandFill(m_CurrentSpanIdx, m_CurrentGirderIdx, type, Ns);
   }
}

void CGirderDescDlg::AddAdditionalPropertyPages(bool bAllowExtendedStrands,bool bIsDebonding)
{
   if ( bAllowExtendedStrands || bIsDebonding )
   {
      AddPage( &m_Debond );
   }

   AddPage(&m_LongRebar);
   AddPage( &m_Shear );

   CComPtr<IBroker> pBroker;
   EAFGetBroker(&pBroker);
   GET_IFACE2(pBroker,IGirderLiftingSpecCriteria,pGirderLiftingSpecCriteria);
   GET_IFACE2(pBroker,IGirderHaulingSpecCriteria,pGirderHaulingSpecCriteria);

   // don't add page if both hauling and lifting checks are disabled
   if (pGirderLiftingSpecCriteria->IsLiftingCheckEnabled() || pGirderHaulingSpecCriteria->IsHaulingCheckEnabled())
   {
      AddPage( &m_Lifting );
   }

   AddPage( &m_Rating );
}

void CGirderDescDlg::OnGirderTypeChanged(bool bAllowExtendedStrands,bool bIsDebonding)
{
   // Remove all but the first two pages
   int nps = GetPageCount();
   for (int ip=nps-1; ip>1; ip--)
   {
      RemovePage(ip);
   }

   AddAdditionalPropertyPages(bAllowExtendedStrands,bIsDebonding);
   SetDebondTabName();
}
