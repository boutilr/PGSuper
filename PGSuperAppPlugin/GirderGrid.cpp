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

// GirderGrid.cpp : implementation file
//

#include "PGSuperAppPlugin\stdafx.h"
#include "GirderGrid.h"
#include "EditGirderlineDlg.h"
#include "GirderDescDlg.h"

#include <IFace\Project.h>
#include <PgsExt\BridgeDescription2.h>
#include <PgsExt\PrecastSegmentData.h>
#include <PgsExt\ClosurePourData.h>

#include "GirderSegmentDlg.h"
#include "ClosurePourDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

GRID_IMPLEMENT_REGISTER(CGirderGrid, CS_DBLCLKS, 0, 0, 0);
const ROWCOL nEditCol = 1;

class CRowType : public CGXAbstractUserAttribute
{
public:
   enum RowType { Segment, Closure };
   CRowType(RowType type,CollectionIndexType idx)
   {
      m_Index = idx;
      m_Type = type;
   }

   virtual CGXAbstractUserAttribute* Clone() const
   {
      return new CRowType(m_Type,m_Index);
   }

   RowType m_Type;
   CollectionIndexType m_Index;
};

/////////////////////////////////////////////////////////////////////////////
// CGirderGrid

CGirderGrid::CGirderGrid()
{
//   RegisterClass();
}

CGirderGrid::~CGirderGrid()
{
}

int CGirderGrid::GetColWidth(ROWCOL nCol)
{
   CRect rect = GetGridRect( );
   return rect.Width()/2;
}

BEGIN_MESSAGE_MAP(CGirderGrid, CGXGridWnd)
	//{{AFX_MSG_MAP(CGirderGrid)
		// NOTE - the ClassWizard will add and remove mapping macros here.
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

void CGirderGrid::CustomInit()
{
   // Initialize the grid. For CWnd based grids this call is // 
   // essential. For view based grids this initialization is done 
   // in OnInitialUpdate.
	this->Initialize( );

	this->GetParam( )->EnableUndo(FALSE);

   const int num_rows=0;
   const int num_cols=1;

	this->SetRowCount(num_rows);
	this->SetColCount(num_cols);

	// Turn off selecting whole columns when clicking on a column header
	this->GetParam()->EnableSelection((WORD) (GX_SELFULL & ~GX_SELCOL & ~GX_SELTABLE & ~GX_SELROW));

   SetMergeCellsMode(gxnMergeEvalOnDisplay);
   SetScrollBarMode(SB_HORZ,gxnDisabled);
   SetScrollBarMode(SB_VERT,gxnAutomatic | gxnEnabled | gxnEnhanced);

   // no row moving
	this->GetParam()->EnableMoveRows(FALSE);

   // set text along top row
	this->SetStyleRange(CGXRange(0,0), CGXStyle()
         .SetWrapText(TRUE)
         .SetHorizontalAlignment(DT_CENTER)
         .SetVerticalAlignment(DT_VCENTER)
			.SetEnabled(FALSE)          // disables usage as current cell
			.SetValue(_T("Type"))
         .SetMergeCell(GX_MERGE_HORIZONTAL | GX_MERGE_COMPVALUE)
		);

	this->SetStyleRange(CGXRange(0,1), CGXStyle()
         .SetWrapText(TRUE)
			.SetEnabled(FALSE)          // disables usage as current cell
         .SetHorizontalAlignment(DT_CENTER)
         .SetVerticalAlignment(DT_VCENTER)
			.SetValue(_T("Type"))
         .SetMergeCell(GX_MERGE_HORIZONTAL | GX_MERGE_COMPVALUE)
		);

   // don't allow users to resize grids
   this->GetParam( )->EnableTrackColWidth(0); 
   this->GetParam( )->EnableTrackRowHeight(0); 

	this->EnableIntelliMouse();
	this->SetFocus();

   CEditGirderlineDlg* pParent = (CEditGirderlineDlg*)GetParent();

   SegmentIndexType nSegments = pParent->m_Girder.GetSegmentCount();
   for ( SegmentIndexType segIdx = 0; segIdx < nSegments; segIdx++ )
   {
      CString strLabel;
      strLabel.Format(_T("Segment %d"),LABEL_SEGMENT(segIdx));
      AddRow(strLabel,CRowType(CRowType::Segment,segIdx));

      if ( segIdx < nSegments-1 )
      {
         //CClosurePourData* pClosure = pParent->m_Girder.GetClosurePour(segIdx);
         AddRow(_T("Closure Pour"),CRowType(CRowType::Closure,segIdx));
      }
   }

   this->HideRows(0,0);

	this->GetParam( )->EnableUndo(TRUE);
}

void CGirderGrid::AddRow(LPCTSTR lpszGirderName,CGXAbstractUserAttribute& rowType)
{
	ROWCOL nRow = 0;
   nRow = GetRowCount() + 1;

   GetParam()->EnableUndo(FALSE);
	InsertRows(nRow, 1);

   SetStyleRange(CGXRange(nRow,0), CGXStyle()
      .SetHorizontalAlignment(DT_LEFT)
      .SetInterior(::GetSysColor(COLOR_BTNFACE)) // dialog face color
      .SetTextColor(::GetSysColor(COLOR_WINDOWTEXT)) // COLOR_GRAYTEXT
      .SetReadOnly(TRUE)
      .SetEnabled(FALSE)
      .SetValue(lpszGirderName)
      );

   SetStyleRange(CGXRange(nRow,nEditCol), CGXStyle()
		.SetControl(GX_IDS_CTRL_PUSHBTN)
		.SetChoiceList(_T("Edit"))
      .SetUserAttribute(0,rowType)
      );

   GetParam()->EnableUndo(TRUE);
}

void CGirderGrid::OnClickedButtonRowCol(ROWCOL nRow,ROWCOL nCol)
{
   if ( nCol != nEditCol )
      return;

   CGXStyle style;
   GetStyleRowCol(nRow,nEditCol,style);
   if ( style.GetIncludeUserAttribute(0) )
   {
      const CRowType& rowType = dynamic_cast<const CRowType&>(style.GetUserAttribute(0));

      if ( rowType.m_Type == CRowType::Segment )
      {
         EditSegment(rowType.m_Index);
      }
      else
      {
         EditClosure(rowType.m_Index);
      }
   }
}

void CGirderGrid::EditSegment(SegmentIndexType segIdx)
{
   CEditGirderlineDlg* pParent = (CEditGirderlineDlg*)GetParent();
   CGirderSegmentDlg dlg(true,this);

   CGirderKey girderKey = pParent->m_GirderKey;
   CSegmentKey segmentKey(girderKey,segIdx);

   CComPtr<IBroker> pBroker;
   EAFGetBroker(&pBroker);
   GET_IFACE2(pBroker,IBridgeDescription,pIBridgeDesc);
   const CTimelineManager* pTimelineMgr = pIBridgeDesc->GetTimelineManager();

   dlg.m_Girder     = pParent->m_Girder;
   dlg.m_SegmentKey = segmentKey;
   dlg.m_SegmentID  = pParent->m_Girder.GetSegment(segIdx)->GetID();


#pragma Reminder("UPDATE: Clean up handling of shear data")
   // Shear data is kind of messy. It is the only data on the segment that we have to
   // set on the dialog and then get it for the transaction. Updated the dialog
   // so it works seamlessly for all cases
   dlg.m_Stirrups.m_ShearData = dlg.m_Girder.GetSegment(segIdx)->ShearData;

   SegmentIDType segID = pParent->m_GirderID;

   dlg.m_ConstructionEventIdx = pTimelineMgr->GetSegmentConstructionEventIndex();
   dlg.m_ErectionEventIdx     = pTimelineMgr->GetSegmentErectionEventIndex(segID);

   if ( dlg.DoModal() == IDOK )
   {
      dlg.m_Girder.GetSegment(segIdx)->ShearData = dlg.m_Stirrups.m_ShearData;

      if ( dlg.m_bCopyToAll )
      {
         // copy the data for the segment that was edited to all the segments in this girder
         SegmentIndexType nSegments = pParent->m_Girder.GetSegmentCount();
         for ( SegmentIndexType idx = 0; idx < nSegments; idx++ )
         {
            pParent->m_Girder.SetSegment(idx,*dlg.m_Girder.GetSegment(segIdx));
         }
      }
      else
      {
         pParent->m_Girder.SetSegment(segIdx,*dlg.m_Girder.GetSegment(segIdx));
      }

#pragma Reminder("BUG: Segment events aren't getting set even if they were changed")
      pParent->Invalidate();
      pParent->UpdateWindow();
   }
}

void CGirderGrid::EditClosure(CollectionIndexType idx)
{
   CEditGirderlineDlg* pParent = (CEditGirderlineDlg*)GetParent();

   CSegmentKey segmentKey(pParent->m_GirderKey,idx);
   CClosurePourDlg dlg(_T("Closure Pour / Splice"),segmentKey,pParent->m_Girder.GetClosurePour(idx),pParent->m_CastClosureEvent[idx],true,this);

   if ( dlg.DoModal() == IDOK )
   {
      if ( dlg.m_bCopyToAllClosurePours )
      {
         // copy to all closure pours in this girder
         IndexType nCP = pParent->m_Girder.GetClosurePourCount();
         for ( IndexType i = 0; i < nCP; i++ )
         {
            pParent->m_Girder.GetClosurePour(i)->CopyClosurePourData(&dlg.m_ClosurePour);
            pParent->m_CastClosureEvent[i] = dlg.m_EventIdx;
         }
      }
      else
      {
         pParent->m_Girder.GetClosurePour(idx)->CopyClosurePourData(&dlg.m_ClosurePour);
         pParent->m_CastClosureEvent[idx] = dlg.m_EventIdx;
      }
      pParent->Invalidate();
      pParent->UpdateWindow();
   }
}

