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

// GirderSegmentDlg.cpp : implementation file
//

#include "PGSuperAppPlugin\stdafx.h"
#include "PGSuperAppPlugin.h"
#include "GirderSegmentDlg.h"

#include <IFace\Project.h>
#include <IFace\GirderHandlingSpecCriteria.h>
#include <IFace\Bridge.h>

#include <LRFD\RebarPool.h>

#define IDC_CHECKBOX 100

// CGirderSegmentDlg

IMPLEMENT_DYNAMIC(CGirderSegmentDlg, CPropertySheet)

CGirderSegmentDlg::CGirderSegmentDlg(bool bEditingInGirder,CWnd* pParentWnd, UINT iSelectPage)
	:CPropertySheet(_T(""), pParentWnd, iSelectPage),m_bEditingInGirder(bEditingInGirder)
{
   Init();
}

CGirderSegmentDlg::~CGirderSegmentDlg()
{
}


BEGIN_MESSAGE_MAP(CGirderSegmentDlg, CPropertySheet)
      WBFL_ON_PROPSHEET_OK
END_MESSAGE_MAP()


// CGirderSegmentDlg message handlers
void CGirderSegmentDlg::Init()
{
   m_bCopyToAll = false;

   m_psh.dwFlags |= PSH_HASHELP | PSH_NOAPPLYNOW;

   m_General.m_psp.dwFlags        |= PSP_HASHELP;
   m_Strands.m_psp.dwFlags        |= PSP_HASHELP;
   m_Rebar.m_psp.dwFlags          |= PSP_HASHELP;
   m_Stirrups.m_psp.dwFlags       |= PSP_HASHELP;
   m_Lifting.m_psp.dwFlags        |= PSP_HASHELP;

   AddPage(&m_General);
   AddPage(&m_Strands);
   AddPage(&m_Rebar);
   AddPage(&m_Stirrups);


   CComPtr<IBroker> pBroker;
   EAFGetBroker(&pBroker);
   GET_IFACE2(pBroker,IGirderLiftingSpecCriteria,pGirderLiftingSpecCriteria);
   GET_IFACE2(pBroker,IGirderHaulingSpecCriteria,pGirderHaulingSpecCriteria);

   // don't add page if both hauling and lifting checks are disabled
   if (pGirderLiftingSpecCriteria->IsLiftingAnalysisEnabled() || pGirderHaulingSpecCriteria->IsHaulingAnalysisEnabled())
   {
      AddPage( &m_Lifting );
   }
}

ConfigStrandFillVector CGirderSegmentDlg::ComputeStrandFillVector(pgsTypes::StrandType type)
{
#pragma Reminder("UPDATE: this method is a duplicate of CGirderDescDlg")
   // find a way to make it one function

   CComPtr<IBroker> pBroker;
   EAFGetBroker(&pBroker);
   GET_IFACE2(pBroker,IStrandGeometry,pStrandGeometry);

   CPrecastSegmentData* pSegment = m_Girder.GetSegment(m_SegmentKey.segmentIndex);
   if (pSegment->Strands.NumPermStrandsType == CStrandData::npsDirectSelection)
   {
      // first get in girderdata format
      const DirectStrandFillCollection* pDirectFillData(NULL);
      if (type==pgsTypes::Straight)
      {
         pDirectFillData = pSegment->Strands.GetDirectStrandFillStraight();
      }
      else if (type==pgsTypes::Harped)
      {
         pDirectFillData = pSegment->Strands.GetDirectStrandFillHarped();
      }
      if (type==pgsTypes::Temporary)
      {
         pDirectFillData = pSegment->Strands.GetDirectStrandFillTemporary();
      }

      // Convert girderdata to config
      // Start with unfilled grid 
      ConfigStrandFillVector vec(pStrandGeometry->ComputeStrandFill(m_Girder.GetGirderName(), type, 0));
      StrandIndexType gridsize = vec.size();

      if(pDirectFillData!=NULL)
      {
         DirectStrandFillCollection::const_iterator it = pDirectFillData->begin();
         DirectStrandFillCollection::const_iterator itend = pDirectFillData->end();
         while(it != itend)
         {
            StrandIndexType idx = it->permStrandGridIdx;
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
      StrandIndexType Ns = pSegment->Strands.GetNstrands(type);

      return pStrandGeometry->ComputeStrandFill(m_SegmentKey, type, Ns);
   }
}

BOOL CGirderSegmentDlg::OnInitDialog()
{
   BOOL bResult = CPropertySheet::OnInitDialog();

   CString strTitle;
   strTitle.Format(_T("Group %d Girder %s Segment %d"),LABEL_GROUP(m_SegmentKey.groupIndex),LABEL_GIRDER(m_SegmentKey.girderIndex),LABEL_SEGMENT(m_SegmentKey.segmentIndex));
   SetWindowText(strTitle);

   // Build the OK button
   CWnd* pOK = GetDlgItem(IDOK);
   CRect rOK;
   pOK->GetWindowRect(&rOK);

   CRect wndPage;
   GetActivePage()->GetWindowRect(&wndPage);

   CRect rect;
   rect.left = wndPage.left;
   rect.top = rOK.top;
   rect.bottom = rOK.bottom;
   rect.right = rOK.left - 7;
   ScreenToClient(&rect);
   CString strTxt;
   if ( m_bEditingInGirder )
      strTxt = _T("Copy to all segments in this girder");
   else
      strTxt.Format(_T("Copy to Segment %d of all girders in Group %d"),LABEL_SEGMENT(m_SegmentKey.segmentIndex),LABEL_GROUP(m_SegmentKey.groupIndex));

   m_CheckBox.Create(strTxt,WS_CHILD | WS_VISIBLE | BS_LEFTTEXT | BS_RIGHT | BS_AUTOCHECKBOX,rect,this,IDC_CHECKBOX);
   m_CheckBox.SetFont(GetFont());

   UpdateData(FALSE); // calls DoDataExchange

   return bResult;
}

void CGirderSegmentDlg::DoDataExchange(CDataExchange* pDX)
{
   CPropertySheet::DoDataExchange(pDX);
   DDX_Check_Bool(pDX,IDC_CHECKBOX,m_bCopyToAll);
}

BOOL CGirderSegmentDlg::OnOK()
{
   UpdateData(TRUE);
   return FALSE; // MUST RETURN FALSE
}
