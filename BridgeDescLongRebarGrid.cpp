///////////////////////////////////////////////////////////////////////
// PGSuper - Prestressed Girder SUPERstructure Design and Analysis
// Copyright � 1999-2016  Washington State Department of Transportation
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

// BridgeDescLongRebarGrid.cpp : implementation file
//

#include "PGSuperAppPlugin\stdafx.h"
#include "PGSuperDoc.h"
#include <EAF\EAFDisplayUnits.h>
#include "BridgeDescLongRebarGrid.h"
#include "BridgeDescLongitudinalRebar.h"
#include <system\tokenizer.h>

#include <LRFD\RebarPool.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

GRID_IMPLEMENT_REGISTER(CGirderDescLongRebarGrid, CS_DBLCLKS, 0, 0, 0);

/////////////////////////////////////////////////////////////////////////////
// CGirderDescLongRebarGrid

CGirderDescLongRebarGrid::CGirderDescLongRebarGrid()
{
//   RegisterClass();
}

CGirderDescLongRebarGrid::~CGirderDescLongRebarGrid()
{
}

BEGIN_MESSAGE_MAP(CGirderDescLongRebarGrid, CGXGridWnd)
	//{{AFX_MSG_MAP(CGirderDescLongRebarGrid)
		// NOTE - the ClassWizard will add and remove mapping macros here.
	ON_COMMAND(ID_EDIT_INSERTROW, OnEditInsertrow)
	ON_COMMAND(ID_EDIT_REMOVEROWS, OnEditRemoverows)
	ON_UPDATE_COMMAND_UI(ID_EDIT_REMOVEROWS, OnUpdateEditRemoverows)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CGirderDescLongRebarGrid message handlers

int CGirderDescLongRebarGrid::GetColWidth(ROWCOL nCol)
{
	CRect rect = GetGridRect( );

   switch (nCol)
   {
   case 0:
      return rect.Width( )/19;
   case 1:
      return rect.Width( )*3/18;
   default:
      return rect.Width( )*2/18;
   }
}

BOOL CGirderDescLongRebarGrid::OnRButtonClickedRowCol(ROWCOL nRow, ROWCOL nCol, UINT nFlags, CPoint pt)
{
	 // unreferenced parameters
	 nRow, nCol, nFlags;

	CMenu menu;
	VERIFY(menu.LoadMenu(IDR_ADD_DELETE_POPUP));

	CMenu* pPopup = menu.GetSubMenu( 0 );
	ASSERT( pPopup != NULL );

	// display the menu
	ClientToScreen(&pt);
	pPopup->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, 
	pt.x, pt.y, this);

	// we processed the message
	return TRUE;
}

BOOL CGirderDescLongRebarGrid::OnLButtonClickedRowCol(ROWCOL nRow, ROWCOL nCol, UINT nFlags, CPoint pt)
{
   CGirderDescLongitudinalRebar* pdlg = (CGirderDescLongitudinalRebar*)GetParent();
   ASSERT (pdlg);

   if (nCol==0 && nRow!=0)
      pdlg->OnEnableDelete(true);
   else
      pdlg->OnEnableDelete(false);

   return TRUE;
}

void CGirderDescLongRebarGrid::OnEditInsertrow()
{
   Insertrow();
}

void CGirderDescLongRebarGrid::Insertrow()
{
	ROWCOL nRow = 0;

	// if there are no cells selected,
	// copy the current cell's coordinates
	CGXRangeList selList;
	if (CopyRangeList(selList, TRUE))
		nRow = selList.GetHead()->top;
	else
		nRow = GetRowCount()+1;

	nRow = max(1, nRow);

	InsertRows(nRow, 1);
   SetRowStyle(nRow);
   OnLayoutTypeChanged(nRow);
   SetCurrentCell(nRow, GetLeftCol(), GX_SCROLLINVIEW|GX_DISPLAYEDITWND);
	Invalidate();
}

void CGirderDescLongRebarGrid::Appendrow()
{
	ROWCOL nRow = 0;
   nRow = GetRowCount()+1;

	InsertRows(nRow, 1);
   SetRowStyle(nRow);
   OnLayoutTypeChanged(nRow);
   SetCurrentCell(nRow, GetLeftCol(), GX_SCROLLINVIEW|GX_DISPLAYEDITWND);
	Invalidate();
}

void CGirderDescLongRebarGrid::OnEditRemoverows()
{
   Removerows();
}

void CGirderDescLongRebarGrid::Removerows()
{
	CGXRangeList* pSelList = GetParam()->GetRangeList();
	if (pSelList->IsAnyCellFromCol(0) && pSelList->GetCount() == 1)
	{
		CGXRange range = pSelList->GetHead();
		range.ExpandRange(1, 0, GetRowCount(), 0);
		RemoveRows(range.top, range.bottom);
	}
}

void CGirderDescLongRebarGrid::OnUpdateEditRemoverows(CCmdUI* pCmdUI)
{
	if (GetParam() == NULL)
	{
		pCmdUI->Enable(FALSE);
		return;
	}

	CGXRangeList* pSelList = GetParam()->GetRangeList();
	pCmdUI->Enable(pSelList->IsAnyCellFromCol(0) && pSelList->GetCount() == 1);
}

void CGirderDescLongRebarGrid::CustomInit()
{
   CEAFApp* pApp = EAFGetApp();
   const unitmgtIndirectMeasure* pDisplayUnits = pApp->GetDisplayUnits();

// Initialize the grid. For CWnd based grids this call is // 
// essential. For view based grids this initialization is done 
// in OnInitialUpdate.
	this->Initialize( );

	this->GetParam( )->EnableUndo(FALSE);

   const int num_rows=0;
   const int num_cols=8;

	this->SetRowCount(num_rows);
	this->SetColCount(num_cols);

		// Turn off selecting whole columns when clicking on a column header
	this->GetParam()->EnableSelection((WORD) (GX_SELFULL & ~GX_SELCOL & ~GX_SELTABLE));

   // disable left side
	this->SetStyleRange(CGXRange(0,0,num_rows,0), CGXStyle()
			.SetControl(GX_IDS_CTRL_HEADER)
			.SetEnabled(FALSE)          // disables usage as current cell
		);

// set text along top row
	this->SetStyleRange(CGXRange(0,0), CGXStyle()
         .SetWrapText(TRUE)
         .SetHorizontalAlignment(DT_CENTER)
         .SetVerticalAlignment(DT_VCENTER)
			.SetEnabled(FALSE)          // disables usage as current cell
			.SetValue(_T("Row\n#"))
		);

	this->SetStyleRange(CGXRange(0,1), CGXStyle()
         .SetWrapText(TRUE)
			.SetEnabled(FALSE)          // disables usage as current cell
         .SetHorizontalAlignment(DT_CENTER)
         .SetVerticalAlignment(DT_VCENTER)
			.SetValue(_T("Measured\nFrom"))
		);

   CString cv;
   cv.Format(_T("Distance\nFrom End\n(%s)"),pDisplayUnits->SpanLength.UnitOfMeasure.UnitTag().c_str());
	this->SetStyleRange(CGXRange(0,2), CGXStyle()
         .SetWrapText(TRUE)
			.SetEnabled(FALSE)          // disables usage as current cell
         .SetHorizontalAlignment(DT_CENTER)
         .SetVerticalAlignment(DT_VCENTER)
			.SetValue(cv)
		);

   cv.Format(_T("Bar\nLength\n(%s)"),pDisplayUnits->SpanLength.UnitOfMeasure.UnitTag().c_str());
	this->SetStyleRange(CGXRange(0,3), CGXStyle()
         .SetWrapText(TRUE)
			.SetEnabled(FALSE)          // disables usage as current cell
         .SetHorizontalAlignment(DT_CENTER)
         .SetVerticalAlignment(DT_VCENTER)
			.SetValue(cv)
		);

	this->SetStyleRange(CGXRange(0,4), CGXStyle()
         .SetWrapText(TRUE)
			.SetEnabled(FALSE)          // disables usage as current cell
         .SetHorizontalAlignment(DT_CENTER)
         .SetVerticalAlignment(DT_VCENTER)
			.SetValue(_T("Girder\nFace"))
		);

   cv.Format(_T("Cover\n(%s)"),pDisplayUnits->ComponentDim.UnitOfMeasure.UnitTag().c_str());
	this->SetStyleRange(CGXRange(0,5), CGXStyle()
         .SetWrapText(TRUE)
			.SetEnabled(FALSE)          // disables usage as current cell
         .SetHorizontalAlignment(DT_CENTER)
         .SetVerticalAlignment(DT_VCENTER)
			.SetValue(cv)
		);

	this->SetStyleRange(CGXRange(0,6), CGXStyle()
         .SetWrapText(TRUE)
			.SetEnabled(FALSE)          // disables usage as current cell
         .SetHorizontalAlignment(DT_CENTER)
         .SetVerticalAlignment(DT_VCENTER)
			.SetValue(_T("Bar\nSize"))
		);

	this->SetStyleRange(CGXRange(0,7), CGXStyle()
         .SetWrapText(TRUE)
			.SetEnabled(FALSE)          // disables usage as current cell
         .SetHorizontalAlignment(DT_CENTER)
         .SetVerticalAlignment(DT_VCENTER)
			.SetValue(_T("# of\nBars"))
		);

   cv.Format(_T("Spacing\n(%s)"),pDisplayUnits->ComponentDim.UnitOfMeasure.UnitTag().c_str());
	this->SetStyleRange(CGXRange(0,8), CGXStyle()
         .SetWrapText(TRUE)
			.SetEnabled(FALSE)          // disables usage as current cell
         .SetHorizontalAlignment(DT_CENTER)
         .SetVerticalAlignment(DT_VCENTER)
			.SetValue(cv)
		);


   // make it so that text fits correctly in header row
	this->ResizeRowHeightsToFit(CGXRange(0,0,0,num_cols));

   // don't allow users to resize grids
   this->GetParam( )->EnableTrackColWidth(0); 
   this->GetParam( )->EnableTrackRowHeight(0); 

	this->EnableIntelliMouse();
	this->SetFocus();

	this->GetParam( )->EnableUndo(TRUE);
}

void CGirderDescLongRebarGrid::SetRowStyle(ROWCOL nRow)
{
	GetParam()->EnableUndo(FALSE);

	this->SetStyleRange(CGXRange(nRow,1), CGXStyle()
			.SetControl(GX_IDS_CTRL_CBS_DROPDOWNLIST)
			.SetChoiceList(_T("Full-Length\nLeft End\nRight End\nMid-Girder-Length\nMid-Girder-Ends"))
			.SetValue(_T("Full-Length"))
         .SetHorizontalAlignment(DT_RIGHT)
         );

	this->SetStyleRange(CGXRange(nRow,2,nRow,3), CGXStyle()
			.SetUserAttribute(GX_IDS_UA_VALID_MIN, _T("0"))
			.SetUserAttribute(GX_IDS_UA_VALID_MAX, _T("1.0e99"))
			.SetUserAttribute(GX_IDS_UA_VALID_MSG, _T("Please enter zero or greater"))
         .SetHorizontalAlignment(DT_RIGHT)
		);

	this->SetStyleRange(CGXRange(nRow,4), CGXStyle()
			.SetControl(GX_IDS_CTRL_CBS_DROPDOWNLIST)
			.SetChoiceList(_T("Top\nBottom"))
			.SetValue(_T("Top"))
         .SetHorizontalAlignment(DT_RIGHT)
         );

	this->SetStyleRange(CGXRange(nRow,5), CGXStyle()
			.SetUserAttribute(GX_IDS_UA_VALID_MIN, _T("0"))
			.SetUserAttribute(GX_IDS_UA_VALID_MAX, _T("1.0e99"))
			.SetUserAttribute(GX_IDS_UA_VALID_MSG, _T("Cover must be zero or greater"))
         .SetHorizontalAlignment(DT_RIGHT)
         );

	this->SetStyleRange(CGXRange(nRow,6), CGXStyle()
			.SetControl(GX_IDS_CTRL_CBS_DROPDOWNLIST)
			.SetChoiceList(_T("#3\n#4\n#5\n#6\n#7\n#8\n#9\n#10\n#11\n#14\n#18"))
			.SetValue(_T("#4"))
         .SetHorizontalAlignment(DT_RIGHT)
         );

	this->SetStyleRange(CGXRange(nRow,7), CGXStyle()
			.SetUserAttribute(GX_IDS_UA_VALID_MIN, _T("1"))
			.SetUserAttribute(GX_IDS_UA_VALID_MAX, _T("1.0e99"))
			.SetUserAttribute(GX_IDS_UA_VALID_MSG, _T("You must have at least one bar in a row"))
         .SetHorizontalAlignment(DT_RIGHT)
		);

	this->SetStyleRange(CGXRange(nRow,8), CGXStyle()
			.SetUserAttribute(GX_IDS_UA_VALID_MIN, _T("0"))
			.SetUserAttribute(GX_IDS_UA_VALID_MAX, _T("1.0e99"))
			.SetUserAttribute(GX_IDS_UA_VALID_MSG, _T("Please enter zero or greater"))
         .SetHorizontalAlignment(DT_RIGHT)
		);

	GetParam()->EnableUndo(TRUE);
}

CString CGirderDescLongRebarGrid::GetCellValue(ROWCOL nRow, ROWCOL nCol)
{
    if (IsCurrentCell(nRow, nCol) && IsActiveCurrentCell())
    {
        CString s;
        CGXControl* pControl = GetControl(nRow, nCol);
        pControl->GetValue(s);
        return s;
  }
    else
        return GetValueRowCol(nRow, nCol);
}

void CGirderDescLongRebarGrid::OnModifyCell(ROWCOL nRow,ROWCOL nCol)
{
	if (nCol==1)
	{
      OnLayoutTypeChanged(nRow);
   }
}

matRebar::Size CGirderDescLongRebarGrid::GetBarSize(ROWCOL row)
{
   CString s = GetCellValue(row, 6);
   s.TrimLeft();
   int l = s.GetLength();
   CString s2 = s.Right(l-1);
   int i = _tstoi(s2);
   if (s.IsEmpty() || (i==0))
      return matRebar::bsNone;

   switch(i)
   {
   case 3:  return matRebar::bs3;
   case 4:  return matRebar::bs4;
   case 5:  return matRebar::bs5;
   case 6:  return matRebar::bs6;
   case 7:  return matRebar::bs7;
   case 8:  return matRebar::bs8;
   case 9:  return matRebar::bs9;
   case 10: return matRebar::bs10;
   case 11: return matRebar::bs11;
   case 14: return matRebar::bs14;
   case 18: return matRebar::bs18;
   default: ATLASSERT(false);
   }

   return matRebar::bsNone;
}

bool CGirderDescLongRebarGrid::GetRowData(ROWCOL nRow, CLongitudinalRebarData::RebarRow* plsi)
{
   Float64 d;
   int i;

   plsi->BarLayout = GetLayout(nRow); // bar layout type

   if(plsi->BarLayout == pgsTypes::blFullLength)
   {
      // Set irrelevant data to zero
      plsi->DistFromEnd = 0.0;
      plsi->BarLength = 0.0;
   }
   else
   {
      CString s;
      if(plsi->BarLayout == pgsTypes::blMidGirderEnds)
      {
         plsi->BarLength = 0.0;
      }
      else
      {
         s = GetCellValue(nRow, 3);
         d = _tstof(s);
         if (s.IsEmpty())
            d=0;
         else if (d==0.0 && s[0]!=_T('0'))
            return false;

         if (d<=0.0)
         {
            CString msg;
            msg.Format(_T("Bar Length must be greater than zero in Row %d"), nRow);
            ::AfxMessageBox(msg, MB_OK | MB_ICONWARNING);
            return false;
         }

         plsi->BarLength = d;
      }

      if(plsi->BarLayout == pgsTypes::blMidGirderLength)
      {
         plsi->DistFromEnd = 0.0;
      }
      else
      {
         s = GetCellValue(nRow, 2);
         d = _tstof(s);
         if (s.IsEmpty())
            d=0;
         else if (d==0.0 && s[0]!=_T('0'))
            return false;
         plsi->DistFromEnd = d;
      }
   }

   CString s = GetCellValue(nRow, 4);
   if (s==_T("Top"))
      plsi->Face = pgsTypes::GirderTop;
   else
      plsi->Face = pgsTypes::GirderBottom;

   s = GetCellValue(nRow, 5);
   d = _tstof(s);
   if (s.IsEmpty())
      d=0;
   else if (d==0.0 && s[0]!=_T('0'))
      return false;

   if (d<0.0)
   {
      CString msg;
      msg.Format(_T("Cover must be zero or greater in Row %d"), nRow);
      ::AfxMessageBox(msg, MB_OK | MB_ICONWARNING);
      return false;
   }

   plsi->Cover = d;

   plsi->BarSize = GetBarSize(nRow);

   s = GetCellValue(nRow, 7);
   i = _tstoi(s);
   if (s.IsEmpty())
      i=0;
   else if (i==0 && s[0]!=_T('0'))
      return false;

   if (i<=0)
   {
      CString msg;
      msg.Format(_T("Number of bars per row must be zero or greater in Row %d"), nRow);
      ::AfxMessageBox(msg, MB_OK | MB_ICONWARNING);
      return false;
   }

   plsi->NumberOfBars = i;

   s = GetCellValue(nRow, 8);
   d = _tstof(s);
   if (s.IsEmpty())
      d=0;
   else if (d==0.0 && s[0]!=_T('0'))
      return false;

   if (d<0.0)
   {
      CString msg;
      msg.Format(_T("Bar spacing must be zero or greater in Row %d"), nRow);
      ::AfxMessageBox(msg, MB_OK | MB_ICONWARNING);
      return false;
   }

   plsi->BarSpacing = d;

   return true;
}

void CGirderDescLongRebarGrid::FillGrid(const CLongitudinalRebarData& rebarData)
{
   GetParam()->SetLockReadOnly(FALSE);

   ROWCOL rows = GetRowCount();
   if (rows>=1)
	   RemoveRows(1, rows);

   CollectionIndexType size = rebarData.RebarRows.size();
   if (size>0)
   {
      // size grid
      for (CollectionIndexType i=0; i<size; i++)
	      Insertrow();

      // fill grid
      ROWCOL nRow=1;
      for (std::vector<CLongitudinalRebarData::RebarRow>::const_iterator it = rebarData.RebarRows.begin(); it!=rebarData.RebarRows.end(); it++)
      {
         CString tmp;
         pgsTypes::RebarLayoutType layout = (*it).BarLayout;
         if (layout == pgsTypes::blFullLength)
            tmp = _T("Full-Length");
         else if (layout == pgsTypes::blFromLeft)
            tmp = _T("Left End");
         else if (layout == pgsTypes::blFromRight)
            tmp = _T("Right End");
         else if (layout == pgsTypes::blMidGirderLength)
            tmp = _T("Mid-Girder-Length");
         else if (layout == pgsTypes::blMidGirderEnds)
            tmp = _T("Mid-Girder-Ends");
         else
         {
            ATLASSERT(0);
            tmp = _T("Full-Length");
         }

         VERIFY(SetValueRange(CGXRange(nRow, 1), tmp));

         VERIFY(SetValueRange(CGXRange(nRow, 2), (*it).DistFromEnd));
         VERIFY(SetValueRange(CGXRange(nRow, 3), (*it).BarLength));

         pgsTypes::GirderFace face = (*it).Face;
         if (face==pgsTypes::GirderBottom)
            tmp = _T("Bottom");
         else
            tmp = _T("Top");
            
         VERIFY(SetValueRange(CGXRange(nRow, 4), tmp));

         VERIFY(SetValueRange(CGXRange(nRow, 5), (*it).Cover));

         tmp.Format(_T("%s"), lrfdRebarPool::GetBarSize((*it).BarSize).c_str());
         VERIFY(SetValueRange(CGXRange(nRow, 6), tmp));

         VERIFY(SetValueRange(CGXRange(nRow, 7), (LONG)(*it).NumberOfBars));
         VERIFY(SetValueRange(CGXRange(nRow, 8), (*it).BarSpacing));

         OnLayoutTypeChanged(nRow);

         nRow++;
      }
   }

   GetParam()->SetLockReadOnly(TRUE);
}

// validate input
BOOL CGirderDescLongRebarGrid::OnValidateCell(ROWCOL nRow, ROWCOL nCol)
{
	CString s;
	CGXControl* pControl = GetControl(nRow, nCol);
	pControl->GetCurrentText(s);

   if (nCol==7  && !s.IsEmpty( ))
	{
      long l;
      if (!sysTokenizer::ParseLong(s, &l))
		{
			SetWarningText (_T("Value must be an integer"));
			return FALSE;
		}
		return TRUE;
	}
	else if ((nCol==2 || nCol==3 || nCol==5 || nCol==8)  && !s.IsEmpty( ))
	{
      Float64 d;
      if (!sysTokenizer::ParseDouble(s, &d))
		{
			SetWarningText (_T("Value must be a number"));
			return FALSE;
		}
		return TRUE;
	}

	return CGXGridWnd::OnValidateCell(nRow, nCol);
}

pgsTypes::RebarLayoutType CGirderDescLongRebarGrid::GetLayout(ROWCOL nRow)
{
   pgsTypes::RebarLayoutType type;

   CString s = GetCellValue(nRow, 1);
   if (s==_T("Full-Length"))
      type = pgsTypes::blFullLength;
   else if (s==_T("Left End"))
      type = pgsTypes::blFromLeft;
   else if (s==_T("Right End"))
      type = pgsTypes::blFromRight;
   else if (s==_T("Mid-Girder-Length"))
      type = pgsTypes::blMidGirderLength;
   else if (s==_T("Mid-Girder-Ends"))
      type = pgsTypes::blMidGirderEnds;
   else
   {
      ATLASSERT(0); // should not happen
      type = pgsTypes::blFullLength;
   }

   return type;
}

void CGirderDescLongRebarGrid::EnableCell(ROWCOL nRow, ROWCOL nCol, BOOL bEnable)
{
   if (bEnable)
   {
      SetStyleRange(CGXRange(nRow,nCol), CGXStyle()
         .SetEnabled(TRUE)
         .SetFormat(GX_FMT_GEN)
         .SetReadOnly(FALSE)
         .SetInterior(::GetSysColor(COLOR_WINDOW))
         );
   }
   else
   {
      SetStyleRange(CGXRange(nRow,nCol), CGXStyle()
         .SetEnabled(FALSE)
         .SetFormat(GX_FMT_HIDDEN)
         .SetInterior(::GetSysColor(COLOR_BTNFACE))
         );
   }
}

void CGirderDescLongRebarGrid::OnLayoutTypeChanged(ROWCOL nRow)
{
   pgsTypes::RebarLayoutType layout = GetLayout(nRow);

   // Disable and enable dist from end and bar length depending on layout
   BOOL bDistEndCol, bBarLengthCol;
   if (layout==pgsTypes::blFullLength)
   {
      bDistEndCol = FALSE;
      bBarLengthCol = FALSE;
   }
   else if (layout==pgsTypes::blMidGirderLength)
   {
      bDistEndCol = FALSE;
      bBarLengthCol = TRUE;
   }
   else if (layout==pgsTypes::blMidGirderEnds)
   {
      bDistEndCol = TRUE;
      bBarLengthCol = FALSE;
   }
   else
   {
      bDistEndCol = TRUE;
      bBarLengthCol = TRUE;
   }

   EnableCell(nRow, 2, bDistEndCol);
   EnableCell(nRow, 3, bBarLengthCol);
}
