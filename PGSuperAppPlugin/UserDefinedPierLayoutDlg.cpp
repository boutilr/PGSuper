///////////////////////////////////////////////////////////////////////
// PGSuper - Prestressed Girder SUPERstructure Design and Analysis
// Copyright © 1999-2026  Washington State Department of Transportation
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

// UserDefinedPierLayoutDlg.cpp : implementation file
//

///////////////////////////////////////////////////////////////////////////
// NOTE: Duplicate code warning
//
// This dialog along with all its property pages are basically repeated in
// the PGSuperLibrary project. I could not get a single implementation to
// work because of issues with the module resources.
//
// If changes are made here, the same changes are likely needed in
// the other location.

#include "stdafx.h"
#include "PierLayoutPage.h"
#include "UserDefinedPierLayoutDlg.h"
#include <EAF\EAFDisplayUnits.h>
#include <IFace\Project.h>
#include <IFace/Tools.h>

CUserDefinedPierLayoutDlg::CUserDefinedPierLayoutDlg(CWnd* pParent)
    :CDialog(IDD_PIER_LAYOUT_USERDEFINED, pParent)
{

    // only using the fixed option (no pinned at base of column,
    // it leads to unstable models before continuity is achieved)
    m_cbColumnFixity.SetFixityTypes(COLUMN_FIXITY_FIXED);

}

// Add to message map
BEGIN_MESSAGE_MAP(CUserDefinedPierLayoutDlg, CDialog)
    ON_MESSAGE(WM_COLUMN_GRID_CELL_CHANGED, OnColumnGridCellChanged)
    ON_MESSAGE(WM_PIERPOINT_GRID_CELL_CHANGED, OnPierPointGridCellChanged)

    ON_CBN_SELCHANGE(IDC_HEIGHT_MEASURE, OnHeightMeasureChanged)
    ON_BN_CLICKED(IDC_ADD_COLUMN, &CUserDefinedPierLayoutDlg::OnAddColumn)
    ON_BN_CLICKED(IDC_REMOVE_COLUMN, &CUserDefinedPierLayoutDlg::OnRemoveColumns)

    ON_BN_CLICKED(IDC_ADD_PIERPOINT, &CUserDefinedPierLayoutDlg::OnAddPierPoint)
    ON_BN_CLICKED(IDC_REMOVE_PIERPOINT, &CUserDefinedPierLayoutDlg::OnRemovePierPoints)

    ON_EN_CHANGE(IDC_W, &CUserDefinedPierLayoutDlg::OnPierLayoutChanged)
    ON_EN_CHANGE(IDC_H1, &CUserDefinedPierLayoutDlg::OnPierLayoutChanged)
    ON_EN_CHANGE(IDC_H2, &CUserDefinedPierLayoutDlg::OnPierLayoutChanged)
    ON_EN_CHANGE(IDC_H3, &CUserDefinedPierLayoutDlg::OnPierLayoutChanged)
    ON_EN_CHANGE(IDC_H4, &CUserDefinedPierLayoutDlg::OnPierLayoutChanged)
    ON_EN_CHANGE(IDC_X1, &CUserDefinedPierLayoutDlg::OnPierLayoutChanged)
    ON_EN_CHANGE(IDC_X2, &CUserDefinedPierLayoutDlg::OnPierLayoutChanged)
    ON_EN_CHANGE(IDC_X3, &CUserDefinedPierLayoutDlg::OnPierLayoutChanged)
    ON_EN_CHANGE(IDC_X4, &CUserDefinedPierLayoutDlg::OnPierLayoutChanged)
    ON_EN_CHANGE(IDC_X5, &CUserDefinedPierLayoutDlg::OnPierLayoutChanged)
    ON_EN_CHANGE(IDC_X6, &CUserDefinedPierLayoutDlg::OnPierLayoutChanged)

    ON_CBN_SELCHANGE(IDC_REFCOLUMN, &CUserDefinedPierLayoutDlg::OnRefColumnChanged)
    ON_EN_CHANGE(IDC_REFCOLUMN_OFFSET, &CUserDefinedPierLayoutDlg::OnRefColumnChanged)
    ON_CBN_SELCHANGE(IDC_REFCOLUMN_MEASUREMENT, &CUserDefinedPierLayoutDlg::OnRefColumnChanged)

    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONUP()
    ON_WM_MOUSEMOVE()
    ON_WM_LBUTTONDBLCLK()

END_MESSAGE_MAP()

// Add these handler implementations at the end of the file
void CUserDefinedPierLayoutDlg::OnLButtonDown(UINT nFlags, CPoint point)
{
    CRect rcControl;
    m_ctrlDrawXBeam.GetWindowRect(&rcControl);
    ScreenToClient(&rcControl);

    if (rcControl.PtInRect(point))
    {
        m_ctrlDrawXBeam.SendMessage(WM_LBUTTONDOWN, nFlags, MAKELPARAM(point.x - rcControl.left, point.y - rcControl.top));
        return;
    }

    CDialog::OnLButtonDown(nFlags, point);
}

void CUserDefinedPierLayoutDlg::OnLButtonUp(UINT nFlags, CPoint point)
{
    CRect rcControl;
    m_ctrlDrawXBeam.GetWindowRect(&rcControl);
    ScreenToClient(&rcControl);

    if (rcControl.PtInRect(point))
    {
        m_ctrlDrawXBeam.SendMessage(WM_LBUTTONUP, nFlags, MAKELPARAM(point.x - rcControl.left, point.y - rcControl.top));
        return;
    }

    CDialog::OnLButtonUp(nFlags, point);
}

void CUserDefinedPierLayoutDlg::OnMouseMove(UINT nFlags, CPoint point)
{
    CRect rcControl;
    m_ctrlDrawXBeam.GetWindowRect(&rcControl);
    ScreenToClient(&rcControl);

    if (rcControl.PtInRect(point))
    {
        m_ctrlDrawXBeam.SendMessage(WM_MOUSEMOVE, nFlags, MAKELPARAM(point.x - rcControl.left, point.y - rcControl.top));
        return;
    }

    CDialog::OnMouseMove(nFlags, point);
}

void CUserDefinedPierLayoutDlg::OnLButtonDblClk(UINT nFlags, CPoint point)
{
    CRect rcControl;
    m_ctrlDrawXBeam.GetWindowRect(&rcControl);
    ScreenToClient(&rcControl);

    if (rcControl.PtInRect(point))
    {
        m_ctrlDrawXBeam.SendMessage(WM_LBUTTONDBLCLK, nFlags, MAKELPARAM(point.x - rcControl.left, point.y - rcControl.top));
        return;
    }

    CDialog::OnLButtonDblClk(nFlags, point);
}

/////////////////////////////////////////////////////////////////////////////
// CUserDefinedPierLayoutDlg message handlers

BOOL CUserDefinedPierLayoutDlg::OnInitDialog()
{
    m_ctrlDrawXBeam.SubclassDlgItem(IDC_PIER_LAYOUT, this);
    m_ctrlDrawXBeam.CustomInit(this);

    m_ColumnLayoutGrid.SubclassDlgItem(IDC_COLUMN_GRID, this);
    m_ColumnLayoutGrid.CustomInit();

    m_PierPointGrid.SubclassDlgItem(IDC_PIER_POINT_GRID, this);
    m_PierPointGrid.CustomInit();

    m_Pier.GetTransverseOffset(&m_RefColumnIdx, &m_TransverseOffset, &m_TransverseOffsetMeasurement);
    m_XBeamWidth = m_Pier.GetXBeamWidth();

    for (int i = 0; i < 2; i++)
    {
        pgsTypes::SideType side = (pgsTypes::SideType)i;
        m_Pier.GetXBeamDimensions(side, &m_XBeamHeight[side], &m_XBeamTaperHeight[side], &m_XBeamTaperLength[side], &m_XBeamEndSlopeOffset[side]);
        m_XBeamOverhang[side] = m_Pier.GetXBeamOverhang(side);
    }

    m_ColumnFixity = m_Pier.GetColumnFixity();

    FillTransverseLocationComboBox();
    FillRefColumnComboBox(m_Pier.GetColumnCount());
    FillHeightMeasureComboBox();

    CDialog::OnInitDialog();

    OnHeightMeasureChanged();


    return TRUE;
}

void CUserDefinedPierLayoutDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);

    DDX_Control(pDX, IDC_FIXITY, m_cbColumnFixity);

    auto pBroker = EAFGetBroker();
    GET_IFACE2(pBroker, IEAFDisplayUnits, pDisplayUnits);

    DDX_MetaFileStatic(pDX, IDC_PIER_LAYOUT_GUIDE, m_LayoutPicture,_T("PIERLAYOUT"), _T("Metafile") );

    DDX_UnitValueAndTag(pDX, IDC_H1, IDC_H1_UNIT, m_XBeamHeight[pgsTypes::stLeft], pDisplayUnits->GetSpanLengthUnit());
    DDX_UnitValueAndTag(pDX, IDC_X1, IDC_X1_UNIT, m_XBeamEndSlopeOffset[pgsTypes::stLeft], pDisplayUnits->GetSpanLengthUnit());
    DDX_UnitValueAndTag(pDX, IDC_H2, IDC_H2_UNIT, m_XBeamHeight[pgsTypes::stRight], pDisplayUnits->GetSpanLengthUnit());
    DDX_UnitValueAndTag(pDX, IDC_X2, IDC_X2_UNIT, m_XBeamEndSlopeOffset[pgsTypes::stRight], pDisplayUnits->GetSpanLengthUnit());

    DDX_UnitValueAndTag(pDX, IDC_W, IDC_W_UNIT, m_XBeamWidth, pDisplayUnits->GetSpanLengthUnit());

    DDX_CBIndex(pDX, IDC_REFCOLUMN, m_RefColumnIdx);
    DDX_OffsetAndTag(pDX, IDC_REFCOLUMN_OFFSET, IDC_REFCOLUMN_OFFSET_UNIT, m_TransverseOffset, pDisplayUnits->GetSpanLengthUnit());
    DDX_CBItemData(pDX, IDC_REFCOLUMN_MEASUREMENT, m_TransverseOffsetMeasurement);

    DDX_CBItemData(pDX, IDC_FIXITY, m_ColumnFixity);

    CColumnLayoutGrid::DDV_ColumnGrid(pDX, m_ColumnLayoutGrid);
    CColumnLayoutGrid::DDX_ColumnGrid(pDX, m_ColumnLayoutGrid, &m_Pier);

    CPierPointGrid::DDV_PierPointGrid(pDX, m_PierPointGrid);
    CPierPointGrid::DDX_PierPointGrid(pDX, m_PierPointGrid, &m_Pier);

    m_ColumnHeightMeasurementType = m_Pier.GetColumnData(0).GetColumnHeightMeasurementType();
    DDX_CBItemData(pDX, IDC_HEIGHT_MEASURE, m_ColumnHeightMeasurementType);

    if (pDX->m_bSaveAndValidate)
    {
        // XBeam width, W, must be greater than zero
        DDV_UnitValueGreaterThanZero(pDX, IDC_W, m_XBeamWidth, pDisplayUnits->GetSpanLengthUnit());

        // H1 and H3 must be > 0
        DDV_UnitValueGreaterThanZero(pDX, IDC_H1, m_XBeamHeight[pgsTypes::stLeft], pDisplayUnits->GetSpanLengthUnit());
        DDV_UnitValueGreaterThanZero(pDX, IDC_H2, m_XBeamHeight[pgsTypes::stRight], pDisplayUnits->GetSpanLengthUnit());

        // X1..X4 must be >= 0
        DDV_UnitValueZeroOrMore(pDX, IDC_X1, m_XBeamEndSlopeOffset[pgsTypes::stLeft], pDisplayUnits->GetSpanLengthUnit());
        DDV_UnitValueZeroOrMore(pDX, IDC_X2, m_XBeamEndSlopeOffset[pgsTypes::stRight], pDisplayUnits->GetSpanLengthUnit());
    }
}

void CUserDefinedPierLayoutDlg::RefreshDisplay()
{
    CPierLayoutPage* pPage =
    DYNAMIC_DOWNCAST(CPierLayoutPage, GetParent());
    if (pPage->m_pPierLayoutPopout != nullptr)
    {
        // Invalidate and update the drawing control to reflect the changes
        m_ctrlDrawXBeam.UpdateDisplayObjects();
        m_ctrlDrawXBeam.Invalidate();
        m_ctrlDrawXBeam.UpdateWindow();

        if (GetParent())
            GetParent()->SendMessage(WM_PIER_LAYOUT_CHANGED);
    }
}


void CUserDefinedPierLayoutDlg::FillTransverseLocationComboBox()
{
    CComboBox* pcbMeasure = (CComboBox*)GetDlgItem(IDC_REFCOLUMN_MEASUREMENT);
    pcbMeasure->ResetContent();
    int idx = pcbMeasure->AddString(_T("from the Alignment"));
    pcbMeasure->SetItemData(idx, (DWORD_PTR)pgsTypes::omtAlignment);
    idx = pcbMeasure->AddString(_T("from the Bridgeline"));
    pcbMeasure->SetItemData(idx, (DWORD_PTR)pgsTypes::omtBridge);
}

void CUserDefinedPierLayoutDlg::FillRefColumnComboBox(ColumnIndexType nColumns)
{
    CComboBox* pcbRefColumn = (CComboBox*)GetDlgItem(IDC_REFCOLUMN);
    int curSel = pcbRefColumn->GetCurSel();
    pcbRefColumn->ResetContent();
    if (nColumns == INVALID_INDEX)
    {
        nColumns = (ColumnIndexType)m_ColumnLayoutGrid.GetRowCount();
    }

    for (ColumnIndexType colIdx = 0; colIdx < nColumns; colIdx++)
    {
        CString strLabel;
        strLabel.Format(_T("Column %d"), LABEL_COLUMN(colIdx));
        pcbRefColumn->AddString(strLabel);
    }

    if (pcbRefColumn->SetCurSel(curSel) == CB_ERR)
    {
        pcbRefColumn->SetCurSel(m_RefColumnIdx);
        OnRefColumnChanged();
    }
}

void CUserDefinedPierLayoutDlg::FillHeightMeasureComboBox()
{
    CComboBox* pcbHeightMeasure = (CComboBox*)GetDlgItem(IDC_HEIGHT_MEASURE);
    pcbHeightMeasure->ResetContent();
    int idx = pcbHeightMeasure->AddString(_T("Column Height (H)"));
    pcbHeightMeasure->SetItemData(idx, (DWORD_PTR)CColumnData::chtHeight);
    idx = pcbHeightMeasure->AddString(_T("Bottom Elevation"));
    pcbHeightMeasure->SetItemData(idx, (DWORD_PTR)CColumnData::chtBottomElevation);
}

void CUserDefinedPierLayoutDlg::OnHeightMeasureChanged()
{
    CComboBox* pcbHeightMeasure = (CComboBox*)GetDlgItem(IDC_HEIGHT_MEASURE);
    int curSel = pcbHeightMeasure->GetCurSel();
    CColumnData::ColumnHeightMeasurementType measure = (CColumnData::ColumnHeightMeasurementType)(pcbHeightMeasure->GetItemData(curSel));
    m_ColumnLayoutGrid.SetHeightMeasurementType(measure);
}

LRESULT CUserDefinedPierLayoutDlg::OnColumnGridCellChanged(WPARAM wParam, LPARAM lParam)
{

    FillRefColumnComboBox();

    // Update pier data with current column data
    m_ColumnLayoutGrid.GetColumnData(m_Pier);

    RefreshDisplay();

    return 0;
}

LRESULT CUserDefinedPierLayoutDlg::OnPierPointGridCellChanged(WPARAM wParam, LPARAM lParam)
{
    // Update pier data with current pier point data
    m_PierPointGrid.GetPierPointData(m_Pier);

    RefreshDisplay();

    return 0;
}

void CUserDefinedPierLayoutDlg::OnAddColumn()
{
    m_ColumnLayoutGrid.AddColumn();
    FillRefColumnComboBox();

    // Update pier data with current column data
    m_ColumnLayoutGrid.GetColumnData(m_Pier);

    RefreshDisplay();

}

void CUserDefinedPierLayoutDlg::OnRemoveColumns()
{
    m_ColumnLayoutGrid.RemoveSelectedColumns();

    FillRefColumnComboBox();

    // Update pier data with current column data
	const auto nCols = m_Pier.GetColumnCount();
	if (nCols > 1)
    {
        m_ColumnLayoutGrid.GetColumnData(m_Pier);
    }

    RefreshDisplay();
}

void CUserDefinedPierLayoutDlg::OnAddPierPoint()
{
    m_PierPointGrid.AddPierPoint();

    // Update pier data with current pier point data
    m_PierPointGrid.GetPierPointData(m_Pier);

    RefreshDisplay();

}

void CUserDefinedPierLayoutDlg::OnRemovePierPoints()
{
    m_PierPointGrid.RemoveSelectedPierPoints();

    // Update pier data with current column data
    m_PierPointGrid.GetPierPointData(m_Pier);
    
    RefreshDisplay();
}

void CUserDefinedPierLayoutDlg::SetPierModelType(const pgsTypes::PierModelType& pierModelType)
{
    m_PierModelType = pierModelType;
}

void CUserDefinedPierLayoutDlg::SetPierData(const CPierData2& pierData)
{
    m_Pier = pierData;
    m_Pier.SetPierLayoutType(pgsTypes::pltUserDefined);
}

const CPierData2* CUserDefinedPierLayoutDlg::GetPierData() const
{
    return &m_Pier;
}

void CUserDefinedPierLayoutDlg::OnPierLayoutChanged()
{
    // Get the current values from the edit controls into member variables
    CDataExchange dx(this, TRUE);

    auto pBroker = EAFGetBroker();
    GET_IFACE2(pBroker, IEAFDisplayUnits, pDisplayUnits);

    CPierLayoutPage* pPage =
        DYNAMIC_DOWNCAST(CPierLayoutPage, GetParent());

    // Exchange XBeam dimensions
    DDX_UnitValueAndTag(&dx, IDC_H1, IDC_H1_UNIT, m_XBeamHeight[pgsTypes::stLeft], pDisplayUnits->GetSpanLengthUnit());
    DDX_UnitValueAndTag(&dx, IDC_X1, IDC_X1_UNIT, m_XBeamEndSlopeOffset[pgsTypes::stLeft], pDisplayUnits->GetSpanLengthUnit());

    DDX_UnitValueAndTag(&dx, IDC_H2, IDC_H2_UNIT, m_XBeamHeight[pgsTypes::stRight], pDisplayUnits->GetSpanLengthUnit());
    DDX_UnitValueAndTag(&dx, IDC_X2, IDC_X2_UNIT, m_XBeamEndSlopeOffset[pgsTypes::stRight], pDisplayUnits->GetSpanLengthUnit());

    DDX_UnitValueAndTag(&dx, IDC_W, IDC_W_UNIT, m_XBeamWidth, pDisplayUnits->GetSpanLengthUnit());

    // Update the pier data with the current values
    m_Pier.SetXBeamDimensions(pgsTypes::stLeft, m_XBeamHeight[pgsTypes::stLeft], m_XBeamTaperHeight[pgsTypes::stLeft],
        m_XBeamTaperLength[pgsTypes::stLeft], m_XBeamEndSlopeOffset[pgsTypes::stLeft]);

    m_Pier.SetXBeamDimensions(pgsTypes::stRight, m_XBeamHeight[pgsTypes::stRight], m_XBeamTaperHeight[pgsTypes::stRight],
        m_XBeamTaperLength[pgsTypes::stRight], m_XBeamEndSlopeOffset[pgsTypes::stRight]);

    m_Pier.SetXBeamWidth(m_XBeamWidth);

    m_Pier.SetXBeamOverhang(pgsTypes::stLeft, m_XBeamOverhang[pgsTypes::stLeft]);
    m_Pier.SetXBeamOverhang(pgsTypes::stRight, m_XBeamOverhang[pgsTypes::stRight]);

    if (pPage->m_pPierLayoutPopout != nullptr)
    {
        // XBeam width, W, must be greater than zero
        DDV_UnitValueGreaterThanZero(&dx, IDC_W, m_XBeamWidth, pDisplayUnits->GetSpanLengthUnit());

        // H1 and H3 must be > 0
        DDV_UnitValueGreaterThanZero(&dx, IDC_H1, m_XBeamHeight[pgsTypes::stLeft], pDisplayUnits->GetSpanLengthUnit());
        DDV_UnitValueGreaterThanZero(&dx, IDC_H2, m_XBeamHeight[pgsTypes::stRight], pDisplayUnits->GetSpanLengthUnit());

        // X1..X4 must be >= 0
        DDV_UnitValueZeroOrMore(&dx, IDC_X1, m_XBeamEndSlopeOffset[pgsTypes::stLeft], pDisplayUnits->GetSpanLengthUnit());
        DDV_UnitValueZeroOrMore(&dx, IDC_X2, m_XBeamEndSlopeOffset[pgsTypes::stRight], pDisplayUnits->GetSpanLengthUnit());
    }

    RefreshDisplay();
}

void CUserDefinedPierLayoutDlg::OnRefColumnChanged()
{
    // Get the current values from the edit controls into member variables
    CDataExchange dx(this, TRUE);

    auto pBroker = EAFGetBroker();
    GET_IFACE2(pBroker, IEAFDisplayUnits, pDisplayUnits);

    DDX_CBIndex(&dx, IDC_REFCOLUMN, m_RefColumnIdx);
    DDX_OffsetAndTag(&dx, IDC_REFCOLUMN_OFFSET, IDC_REFCOLUMN_OFFSET_UNIT, m_TransverseOffset, pDisplayUnits->GetSpanLengthUnit());
    DDX_CBItemData(&dx, IDC_REFCOLUMN_MEASUREMENT, m_TransverseOffsetMeasurement);

	m_Pier.SetTransverseOffset(m_RefColumnIdx, m_TransverseOffset, m_TransverseOffsetMeasurement);

    RefreshDisplay();
}
