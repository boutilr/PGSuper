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

#include "stdafx.h"
#include "resource.h"
#include "FinishedElevationGraphController.h"
#include <Graphs/FinishedElevationGraphBuilder.h>
#include <Graphs/ExportGraphXYTool.h>

#include <EAF\EAFUtilities.h>
#include <IFace\DocumentType.h>
#include <IFace\Bridge.h>
#include <IFace\Intervals.h>
#include <IFace\Selection.h>

#include <Hints.h>

#include <EAF\EAFGraphBuilderBase.h>
#include <EAF\EAFGraphView.h>
#include <EAF\EAFDocument.h>

#include <PgsExt\BridgeDescription2.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

IMPLEMENT_DYNCREATE(CFinishedElevationGraphController,CMultiIntervalGirderGraphControllerBase)

CFinishedElevationGraphController::CFinishedElevationGraphController():
CMultiIntervalGirderGraphControllerBase(false/*don't use ALL_GROUPS*/)
{
}

BEGIN_MESSAGE_MAP(CFinishedElevationGraphController, CMultiIntervalGirderGraphControllerBase)
   //{{AFX_MSG_MAP(CFinishedElevationGraphController)
   ON_BN_CLICKED(IDC_EXPORT_GRAPH_BTN,OnGraphExportClicked)
   ON_UPDATE_COMMAND_UI(IDC_EXPORT_GRAPH_BTN,OnCommandUIGraphExport)
   ON_BN_CLICKED(IDC_PGL,OnShowPGL)
   ON_BN_CLICKED(IDC_FINISHED_DECK,OnShowFinishedDeck)
   ON_BN_CLICKED(IDC_FINISHED_DECK_BOTTOM,OnShowFinishedDeckBottom)
   ON_BN_CLICKED(IDC_FINISHED_GIRDER_TOP,OnShowFinishedGirderTop)
   ON_BN_CLICKED(IDC_FINISHED_GIRDER_BOTTOM,OnShowFinishedGirderBottom)
   ON_BN_CLICKED(IDC_GIRDER_CHORD,OnShowGirderChord)
   ON_CBN_SELCHANGE(IDC_GRAPH_TYPE,OnGraphTypeChanged)
   ON_CBN_SELCHANGE(IDC_PLOT_AT,OnGraphSideChanged)
   //}}AFX_MSG_MAP
END_MESSAGE_MAP()

BOOL CFinishedElevationGraphController::OnInitDialog()
{
   CMultiIntervalGirderGraphControllerBase::OnInitDialog();

   GET_IFACE(IBridge,pBridge);
   pgsTypes::SupportedDeckType deckType = pBridge->GetDeckType();
   pgsTypes::HaunchInputDepthType haunchInputDepthType = pBridge->GetHaunchInputDepthType();

   CButton* pBut = (CButton*)GetDlgItem(IDC_PGL);
   BOOL show = ((CFinishedElevationGraphBuilder*)GetGraphBuilder())->ShowPGL();
   pBut->SetCheck(show == FALSE ? BST_UNCHECKED : BST_CHECKED);

   pBut = (CButton*)GetDlgItem(IDC_FINISHED_DECK);
   show = ((CFinishedElevationGraphBuilder*)GetGraphBuilder())->ShowFinishedDeck();
   pBut->SetCheck(show==FALSE ? BST_UNCHECKED: BST_CHECKED);

   pBut = (CButton*)GetDlgItem(IDC_FINISHED_DECK_BOTTOM);
   if (pgsTypes::sdtNone == deckType)
   {
      pBut->SetCheck(BST_UNCHECKED);
      pBut->EnableWindow(FALSE);
   }
   else
   {
      show = ((CFinishedElevationGraphBuilder*)GetGraphBuilder())->ShowFinishedDeckBottom();
      pBut->SetCheck(show == FALSE ? BST_UNCHECKED : BST_CHECKED);
   }

   pBut = (CButton*)GetDlgItem(IDC_FINISHED_GIRDER_BOTTOM);
   if (pgsTypes::sdtNone == deckType)
   {
      pBut->SetCheck(BST_UNCHECKED);
      pBut->EnableWindow(FALSE);
   }
   else
   {
      show = ((CFinishedElevationGraphBuilder*)GetGraphBuilder())->ShowFinishedGirderBottom();
      pBut->SetCheck(show == FALSE ? BST_UNCHECKED : BST_CHECKED);
   }

   pBut = (CButton*)GetDlgItem(IDC_FINISHED_GIRDER_TOP);
   if (pgsTypes::sdtNone == deckType)
   {
      pBut->SetCheck(BST_UNCHECKED);
      pBut->EnableWindow(FALSE);
   }
   else
   {
      show = ((CFinishedElevationGraphBuilder*)GetGraphBuilder())->ShowFinishedGirderTop();
      pBut->SetCheck(show == FALSE ? BST_UNCHECKED : BST_CHECKED);
   }

   pBut = (CButton*)GetDlgItem(IDC_GIRDER_CHORD);
   show = ((CFinishedElevationGraphBuilder*)GetGraphBuilder())->ShowGirderChord();
   pBut->SetCheck(show == FALSE ? BST_UNCHECKED : BST_CHECKED);

   int st;
   CComboBox* pBox = (CComboBox*)GetDlgItem(IDC_GRAPH_TYPE);
   pBox->ResetContent();
   st = pBox->AddString(_T("Elevations"));
   st = pBox->AddString(_T("Elevation differential from PGL"));
   CFinishedElevationGraphBuilder::GraphType gt = ((CFinishedElevationGraphBuilder*)GetGraphBuilder())->GetGraphType();
   st = pBox->SetCurSel((int)gt);

   pBox = (CComboBox*)GetDlgItem(IDC_PLOT_AT);
   pBox->ResetContent();
   st = pBox->AddString(_T("Left Edge of Top Flange"));
   st = pBox->AddString(_T("Centerline of Girder"));
   st = pBox->AddString(_T("Right Edge of Top Flange"));
   CFinishedElevationGraphBuilder::GraphSide gs = ((CFinishedElevationGraphBuilder*)GetGraphBuilder())->GetGraphSide();
   st = pBox->SetCurSel((int)gs);

   return TRUE;
}

void CFinishedElevationGraphController::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint)
{
   CMultiIntervalGirderGraphControllerBase::OnUpdate(pSender,lHint,pHint);
   if ( lHint == HINT_BRIDGECHANGED )
   {
      FillIntervalCtrl();
   }
}

void CFinishedElevationGraphController::OnShowFinishedDeck()
{
   CButton* pBox = (CButton*)GetDlgItem(IDC_FINISHED_DECK);
   BOOL show = pBox->GetCheck() == 0 ? FALSE : TRUE;
   ((CFinishedElevationGraphBuilder*)GetGraphBuilder())->ShowFinishedDeck(show);
}

void CFinishedElevationGraphController::OnShowPGL()
{
   CButton* pBox = (CButton*)GetDlgItem(IDC_PGL);
   BOOL show = pBox->GetCheck() == 0 ? FALSE : TRUE;
   ((CFinishedElevationGraphBuilder*)GetGraphBuilder())->ShowPGL(show);
}

void CFinishedElevationGraphController::OnShowFinishedDeckBottom()
{
   CButton* pBox = (CButton*)GetDlgItem(IDC_FINISHED_DECK_BOTTOM);
   BOOL show = pBox->GetCheck() == 0 ? FALSE : TRUE;
   ((CFinishedElevationGraphBuilder*)GetGraphBuilder())->ShowFinishedDeckBottom(show);
}

void CFinishedElevationGraphController::OnShowFinishedGirderTop()
{
   CButton* pBox = (CButton*)GetDlgItem(IDC_FINISHED_GIRDER_TOP);
   BOOL show = pBox->GetCheck() == 0 ? FALSE : TRUE;
   ((CFinishedElevationGraphBuilder*)GetGraphBuilder())->ShowFinishedGirderTop(show);
}

void CFinishedElevationGraphController::OnShowFinishedGirderBottom()
{
   CButton* pBox = (CButton*)GetDlgItem(IDC_FINISHED_GIRDER_BOTTOM);
   BOOL show = pBox->GetCheck() == 0 ? FALSE : TRUE;
   ((CFinishedElevationGraphBuilder*)GetGraphBuilder())->ShowFinishedGirderBottom(show);
}

void CFinishedElevationGraphController::OnShowGirderChord()
{
   CButton* pBox = (CButton*)GetDlgItem(IDC_GIRDER_CHORD);
   BOOL show = pBox->GetCheck() == 0 ? FALSE : TRUE;
   ((CFinishedElevationGraphBuilder*)GetGraphBuilder())->ShowGirderChord(show);
}

void CFinishedElevationGraphController::OnGraphTypeChanged()
{
   CComboBox* pBox = (CComboBox*)GetDlgItem(IDC_GRAPH_TYPE);
   CFinishedElevationGraphBuilder::GraphType gt = (CFinishedElevationGraphBuilder::GraphType)pBox->GetCurSel();

   CButton* pButPgl = (CButton*)GetDlgItem(IDC_PGL);
   CComboBox* pBoxLoc = (CComboBox*)GetDlgItem(IDC_PLOT_AT);
   if (CFinishedElevationGraphBuilder::gtElevationDifferential == gt)
   {
      // graph of PGL is not an option for this case
      pButPgl->SetCheck(FALSE);
      pButPgl->ShowWindow(SW_HIDE);
      ((CFinishedElevationGraphBuilder*)GetGraphBuilder())->ShowPGL(FALSE);

      // Differentials are only at CL Girder
      pBoxLoc->SetCurSel((int)CFinishedElevationGraphBuilder::gsCenterLine);
      pBoxLoc->EnableWindow(FALSE);
      ((CFinishedElevationGraphBuilder*)GetGraphBuilder())->SetGraphSide(CFinishedElevationGraphBuilder::gsCenterLine);
   }
   else
   {
      pButPgl->ShowWindow(SW_SHOW);
      pButPgl->SetCheck(TRUE);
      pBoxLoc->EnableWindow(TRUE);
      OnShowPGL();
   }

   ((CFinishedElevationGraphBuilder*)GetGraphBuilder())->SetGraphType(gt);
}

void CFinishedElevationGraphController::OnGraphSideChanged()
{
   CComboBox* pBox = (CComboBox*)GetDlgItem(IDC_PLOT_AT);
   CFinishedElevationGraphBuilder::GraphSide gs = (CFinishedElevationGraphBuilder::GraphSide)pBox->GetCurSel();
   ((CFinishedElevationGraphBuilder*)GetGraphBuilder())->SetGraphSide(gs);

   CButton* pBotBut = (CButton*)GetDlgItem(IDC_FINISHED_GIRDER_BOTTOM);
   CButton* pChordBut = (CButton*)GetDlgItem(IDC_GIRDER_CHORD);
   if (CFinishedElevationGraphBuilder::gsCenterLine != gs)
   {
      pBotBut->EnableWindow(FALSE);
      pBotBut->SetCheck(0);
      ((CFinishedElevationGraphBuilder*)GetGraphBuilder())->ShowFinishedGirderBottom(FALSE);
      pChordBut->EnableWindow(FALSE);
      pChordBut->SetCheck(0);
      ((CFinishedElevationGraphBuilder*)GetGraphBuilder())->ShowGirderChord(FALSE);
   }
   else
   {
      pBotBut->EnableWindow(TRUE);
      pChordBut->EnableWindow(TRUE);
   }
}

void CFinishedElevationGraphController::OnGroupChanged()
{
   FillIntervalCtrl();
}

void CFinishedElevationGraphController::OnGirderChanged()
{
   FillIntervalCtrl();
}

void CFinishedElevationGraphController::OnGraphExportClicked()
{
   // Build default file name
   CString strProjectFileNameNoPath = CExportGraphXYTool::GetTruncatedFileName();

   const CGirderKey& girderKey = GetGirderKey();
   CString girderName = GIRDER_LABEL(girderKey);

   CString actionName = _T("FinishedElevations");

   CString strDefaultFileName = strProjectFileNameNoPath + _T("_") + girderName + _T("_") + actionName;
   strDefaultFileName.Replace(' ','_'); // prefer not to have spaces or ,'s in file names
   strDefaultFileName.Replace(',','_');

   ((CFinishedElevationGraphBuilder*)GetGraphBuilder())->ExportGraphData(strDefaultFileName);
}

// this has to be implemented otherwise button will not be enabled.
void CFinishedElevationGraphController::OnCommandUIGraphExport(CCmdUI* pCmdUI)
{
   pCmdUI->Enable(TRUE);
}

IntervalIndexType CFinishedElevationGraphController::GetFirstInterval()
{
   GET_IFACE(IBridge,pBridge);
   GET_IFACE(IIntervals,pIntervals);
   pgsTypes::SupportedDeckType deckType = pBridge->GetDeckType();
   pgsTypes::HaunchInputDepthType haunchInputDepthType = pBridge->GetHaunchInputDepthType();

   if (pgsTypes::hidACamber == haunchInputDepthType || pgsTypes::sdtNone == deckType)
   {
      return pIntervals->GetLoadRatingInterval();
   }
   else
   {
      return pIntervals->GetLastCastDeckInterval();
   }
}

IntervalIndexType CFinishedElevationGraphController::GetLastInterval()
{
   GET_IFACE(IIntervals,pIntervals);
   return pIntervals->GetLoadRatingInterval();
}


#ifdef _DEBUG
void CFinishedElevationGraphController::AssertValid() const
{
	CMultiIntervalGirderGraphControllerBase::AssertValid();
}

void CFinishedElevationGraphController::Dump(CDumpContext& dc) const
{
	CMultiIntervalGirderGraphControllerBase::Dump(dc);
}
#endif //_DEBUG
