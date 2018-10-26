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

#include "stdafx.h"
#include "resource.h"
#include "TendonStressGraphController.h"
#include <Graphing\TendonStressGraphBuilder.h>

#include <EAF\EAFUtilities.h>
#include <IFace\DocumentType.h>
#include <IFace\Bridge.h>
#include <IFace\Intervals.h>
#include <IFace\Selection.h>

#include <Hints.h>

#include <EAF\EAFGraphBuilderBase.h>
#include <EAF\EAFGraphView.h>

#include <PgsExt\BridgeDescription2.h>

IMPLEMENT_DYNCREATE(CTendonStressGraphController,CMultiIntervalGirderGraphControllerBase)

CTendonStressGraphController::CTendonStressGraphController():
CMultiIntervalGirderGraphControllerBase(false/*don't use ALL_GROUPS*/),
m_DuctIdx(INVALID_INDEX)
{
}

BEGIN_MESSAGE_MAP(CTendonStressGraphController, CMultiIntervalGirderGraphControllerBase)
	//{{AFX_MSG_MAP(CTendonStressGraphController)
   ON_CBN_SELCHANGE( IDC_DUCT, OnDuctChanged )
   ON_CONTROL_RANGE( BN_CLICKED, IDC_STRESS, IDC_FORCE, OnRadioButton)
   //}}AFX_MSG_MAP
END_MESSAGE_MAP()

BOOL CTendonStressGraphController::OnInitDialog()
{
   CMultiIntervalGirderGraphControllerBase::OnInitDialog();

   FillDuctCtrl();
   CheckRadioButton(IDC_STRESS,IDC_FORCE,IDC_STRESS);

   return TRUE;
}

void CTendonStressGraphController::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint)
{
   CMultiIntervalGirderGraphControllerBase::OnUpdate(pSender,lHint,pHint);
   if ( lHint == HINT_BRIDGECHANGED )
   {
      FillDuctCtrl();
   }
}

DuctIndexType CTendonStressGraphController::GetDuct()
{
   return m_DuctIdx;
}

bool CTendonStressGraphController::IsStressGraph()
{
   if ( GetSafeHwnd() == NULL )
   {
      return true;
   }
   else
   {
      return GetCheckedRadioButton(IDC_STRESS,IDC_FORCE) == IDC_STRESS ? true : false;
   }
}

void CTendonStressGraphController::OnRadioButton(UINT nIDC)
{
   UpdateGraph();
}

void CTendonStressGraphController::OnGroupChanged()
{
   FillDuctCtrl();
}

void CTendonStressGraphController::OnGirderChanged()
{
   FillDuctCtrl();
}

void CTendonStressGraphController::OnDuctChanged()
{
   CComboBox* pcbDuct = (CComboBox*)GetDlgItem(IDC_DUCT);
   int curSel = pcbDuct->GetCurSel();
   DuctIndexType ductIdx = (DuctIndexType)pcbDuct->GetItemData(curSel);
   if ( m_DuctIdx != ductIdx )
   {
      m_DuctIdx = ductIdx;
      FillIntervalCtrl();
      UpdateGraph();
   }
}

void CTendonStressGraphController::FillDuctCtrl()
{
   GroupIndexType grpIdx = GetGirderGroup();
   GirderIndexType gdrIdx = GetGirder();

   CComboBox* pcbDuct = (CComboBox*)GetDlgItem(IDC_DUCT);
   int curSel = pcbDuct->GetCurSel();
   pcbDuct->ResetContent();

   int idx = pcbDuct->AddString(_T("Prestressing"));
   pcbDuct->SetItemData(idx,(DWORD_PTR)INVALID_INDEX);

   GET_IFACE(ITendonGeometry,pTendonGeom);
   DuctIndexType nDucts = pTendonGeom->GetDuctCount(CGirderKey(grpIdx,gdrIdx));
   
   for ( DuctIndexType ductIdx = 0; ductIdx < nDucts; ductIdx++ )
   {
      CString strDuct;
      strDuct.Format(_T("Tendon %d"),LABEL_DUCT(ductIdx));
      idx = pcbDuct->AddString(strDuct);
      pcbDuct->SetItemData(idx,(DWORD_PTR)ductIdx);
   }

   if ( curSel == CB_ERR )
   {
      curSel = pcbDuct->SetCurSel(0);
   }
   else
   {
      curSel = pcbDuct->SetCurSel(curSel);
   }

   if ( curSel == CB_ERR )
   {
      m_DuctIdx = INVALID_INDEX;
      pcbDuct->EnableWindow(FALSE);
   }
   else
   {
      m_DuctIdx = (DuctIndexType)pcbDuct->GetItemData(curSel);
      pcbDuct->EnableWindow(TRUE);
   }
}

IntervalIndexType CTendonStressGraphController::GetFirstInterval()
{
   GET_IFACE(IIntervals,pIntervals);
   CGirderKey girderKey(GetGirderKey());
   if ( m_DuctIdx == INVALID_INDEX )
   {
      return pIntervals->GetFirstStressStrandInterval(girderKey);
   }
   else
   {
      return pIntervals->GetFirstTendonStressingInterval(girderKey);
   }
}

#ifdef _DEBUG
void CTendonStressGraphController::AssertValid() const
{
	CMultiIntervalGirderGraphControllerBase::AssertValid();
}

void CTendonStressGraphController::Dump(CDumpContext& dc) const
{
	CMultiIntervalGirderGraphControllerBase::Dump(dc);
}
#endif //_DEBUG
