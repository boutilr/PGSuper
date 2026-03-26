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

// CommonPierLayoutDlg.cpp : implementation file
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
#include "CommonPierLayoutDlg.h"
#include <EAF\EAFDisplayUnits.h>
#include <IFace\Project.h>
#include <IFace/Tools.h>


void DDX_ColumnGrid(CDataExchange* pDX, CColumnLayoutGrid& grid, CPierData2* pPier)
{
    if (pDX->m_bSaveAndValidate)
    {
        grid.GetColumnData(*pPier);
    }
    else
    {
        grid.SetColumnData(*pPier);
    }
}

void DDV_ColumnGrid(CDataExchange* pDX, CColumnLayoutGrid& grid)
{
    if (pDX->m_bSaveAndValidate)
    {
        if (grid.GetRowCount() == 0)
        {
            AfxMessageBox(_T("The pier must have at least one column"), MB_OK | MB_ICONEXCLAMATION);
            pDX->Fail();
        }
    }
}


CCommonPierLayoutDlg::CCommonPierLayoutDlg(CWnd* pParent)
	:CDialog(IDD_PIER_LAYOUT_COMMON, pParent)
{

     // only using the fixed option (no pinned at base of column,
     // it leads to unstable models before continuity is achieved)
     m_cbColumnFixity.SetFixityTypes(COLUMN_FIXITY_FIXED); 

}

BEGIN_MESSAGE_MAP(CCommonPierLayoutDlg, CDialog)
    ON_CBN_SELCHANGE(IDC_HEIGHT_MEASURE, OnHeightMeasureChanged)
    ON_BN_CLICKED(IDC_ADD_COLUMN, &CCommonPierLayoutDlg::OnAddColumn)
    ON_BN_CLICKED(IDC_REMOVE_COLUMN, &CCommonPierLayoutDlg::OnRemoveColumns)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CCommonPierLayoutDlg message handlers

BOOL CCommonPierLayoutDlg::OnInitDialog()
{

    m_ColumnLayoutGrid.SubclassDlgItem(IDC_COLUMN_GRID, this);
    m_ColumnLayoutGrid.CustomInit();

    m_Pier.GetTransverseOffset(&m_RefColumnIdx,&m_TransverseOffset,&m_TransverseOffsetMeasurement);
    m_XBeamWidth = m_Pier.GetXBeamWidth();

    for ( int i = 0; i < 2; i++ )
    {
       pgsTypes::SideType side = (pgsTypes::SideType)i;
       m_Pier.GetXBeamDimensions(side,&m_XBeamHeight[side],&m_XBeamTaperHeight[side],&m_XBeamTaperLength[side],&m_XBeamEndSlopeOffset[side]);
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


void CCommonPierLayoutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);

    DDX_Control(pDX, IDC_FIXITY,       m_cbColumnFixity);

    auto pBroker = EAFGetBroker();
    GET_IFACE2(pBroker, IEAFDisplayUnits, pDisplayUnits);

    DDX_MetaFileStatic(pDX, IDC_PIER_LAYOUT, m_LayoutPicture,_T("PIERLAYOUT"), _T("Metafile") );

    DDX_UnitValueAndTag(pDX,IDC_H1,IDC_H1_UNIT,m_XBeamHeight[pgsTypes::stLeft],pDisplayUnits->GetSpanLengthUnit() );
    DDX_UnitValueAndTag(pDX,IDC_H2,IDC_H2_UNIT,m_XBeamTaperHeight[pgsTypes::stLeft],pDisplayUnits->GetSpanLengthUnit() );
    DDX_UnitValueAndTag(pDX,IDC_X1,IDC_X1_UNIT,m_XBeamTaperLength[pgsTypes::stLeft],pDisplayUnits->GetSpanLengthUnit() );
    DDX_UnitValueAndTag(pDX,IDC_X2,IDC_X2_UNIT,m_XBeamEndSlopeOffset[pgsTypes::stLeft],pDisplayUnits->GetSpanLengthUnit() );

    DDX_UnitValueAndTag(pDX,IDC_H3,IDC_H3_UNIT,m_XBeamHeight[pgsTypes::stRight],pDisplayUnits->GetSpanLengthUnit() );
    DDX_UnitValueAndTag(pDX,IDC_H4,IDC_H4_UNIT,m_XBeamTaperHeight[pgsTypes::stRight],pDisplayUnits->GetSpanLengthUnit() );
    DDX_UnitValueAndTag(pDX,IDC_X3,IDC_X3_UNIT,m_XBeamTaperLength[pgsTypes::stRight],pDisplayUnits->GetSpanLengthUnit() );
    DDX_UnitValueAndTag(pDX,IDC_X4,IDC_X4_UNIT,m_XBeamEndSlopeOffset[pgsTypes::stRight],pDisplayUnits->GetSpanLengthUnit() );

    DDX_UnitValueAndTag(pDX,IDC_W,IDC_W_UNIT,m_XBeamWidth,pDisplayUnits->GetSpanLengthUnit() );

    DDX_CBIndex(pDX,IDC_REFCOLUMN,m_RefColumnIdx);
    DDX_OffsetAndTag(pDX,IDC_REFCOLUMN_OFFSET,IDC_REFCOLUMN_OFFSET_UNIT,m_TransverseOffset, pDisplayUnits->GetSpanLengthUnit() );
    DDX_CBItemData(pDX,IDC_REFCOLUMN_MEASUREMENT,m_TransverseOffsetMeasurement);

    DDX_UnitValueAndTag(pDX,IDC_X5,IDC_X5_UNIT,m_XBeamOverhang[pgsTypes::stLeft], pDisplayUnits->GetSpanLengthUnit() );
    DDX_UnitValueAndTag(pDX,IDC_X6,IDC_X6_UNIT,m_XBeamOverhang[pgsTypes::stRight],pDisplayUnits->GetSpanLengthUnit() );

    DDX_CBItemData(pDX,IDC_FIXITY,m_ColumnFixity);

    if (m_PierModelType == pgsTypes::pmtPhysical)
    {
        DDV_ColumnGrid(pDX,m_ColumnLayoutGrid);
        DDX_ColumnGrid(pDX,m_ColumnLayoutGrid, &m_Pier);
    }

    if (pDX->m_bSaveAndValidate)
    {
        CColumnData::ColumnHeightMeasurementType measure;
        DDX_CBItemData(pDX,IDC_HEIGHT_MEASURE,measure);
        ColumnIndexType nColumns = m_Pier.GetColumnCount();
        for ( ColumnIndexType colIdx = 0; colIdx < nColumns; colIdx++ )
        {
           CColumnData column = m_Pier.GetColumnData(colIdx);
           column.SetColumnHeightMeasurementType(measure);
           m_Pier.SetColumnData(colIdx,column);
        }
    }
    else
    {
        CColumnData::ColumnHeightMeasurementType measure = m_Pier.GetColumnData(0).GetColumnHeightMeasurementType();
        DDX_CBItemData(pDX,IDC_HEIGHT_MEASURE,measure);
    }

    if (pDX->m_bSaveAndValidate)
    {
        if (m_PierModelType == pgsTypes::pmtPhysical)
        {
            m_Pier.SetTransverseOffset(m_RefColumnIdx,m_TransverseOffset,m_TransverseOffsetMeasurement);
            for ( int i = 0; i < 2; i++ )
            {
               pgsTypes::SideType side = (pgsTypes::SideType)i;
               m_Pier.SetXBeamDimensions(side,m_XBeamHeight[side],m_XBeamTaperHeight[side],m_XBeamTaperLength[side],m_XBeamEndSlopeOffset[side]);
               m_Pier.SetXBeamOverhang(side,m_XBeamOverhang[side]);
            }
            m_Pier.SetXBeamWidth(m_XBeamWidth);

            m_Pier.SetColumnFixity(m_ColumnFixity);

            // XBeam width, W, must be greater than zeo
            DDV_UnitValueGreaterThanZero(pDX, IDC_W, m_XBeamWidth, pDisplayUnits->GetSpanLengthUnit());

            // H1 and H3 must be > 0
            DDV_UnitValueGreaterThanZero(pDX, IDC_H1, m_XBeamHeight[pgsTypes::stLeft], pDisplayUnits->GetSpanLengthUnit());
            DDV_UnitValueGreaterThanZero(pDX, IDC_H3, m_XBeamHeight[pgsTypes::stRight], pDisplayUnits->GetSpanLengthUnit());

            // X2 and X4 must be >= 0
            DDV_UnitValueZeroOrMore(pDX, IDC_X1, m_XBeamTaperLength[pgsTypes::stLeft],    pDisplayUnits->GetSpanLengthUnit());
            DDV_UnitValueZeroOrMore(pDX, IDC_X2, m_XBeamEndSlopeOffset[pgsTypes::stLeft], pDisplayUnits->GetSpanLengthUnit());
            DDV_UnitValueZeroOrMore(pDX, IDC_X3, m_XBeamTaperLength[pgsTypes::stRight],   pDisplayUnits->GetSpanLengthUnit());
            DDV_UnitValueZeroOrMore(pDX, IDC_X4, m_XBeamEndSlopeOffset[pgsTypes::stRight],pDisplayUnits->GetSpanLengthUnit());

            //Left end
            if (0 < m_XBeamTaperLength[pgsTypes::stLeft])
            {
               // if H2 > 0, then X1 must be > 0
               if ( IsZero(m_XBeamTaperHeight[pgsTypes::stLeft]) )
               {
                  pDX->PrepareCtrl(IDC_H2);
                  AfxMessageBox(_T("H2 must be greater than zero when X1 is greater than zero."));
                  pDX->Fail();
               }
               else if ( m_XBeamTaperLength[pgsTypes::stLeft] < m_XBeamEndSlopeOffset[pgsTypes::stLeft] )
               {
                  pDX->PrepareCtrl(IDC_X1);
                  AfxMessageBox(_T("X1 must be greater than X2 when X1 is greater than zero."));
                  pDX->Fail();
               }
            }
            else if ( !IsZero(m_XBeamTaperHeight[pgsTypes::stLeft]) )
            {
               // if X1 is zero, then H2 must also be zero
               pDX->PrepareCtrl(IDC_H2);
               AfxMessageBox(_T("H2 must be zero when X1 is zero"));
               pDX->Fail();
            }

            // Right end
            if (0 < m_XBeamTaperLength[pgsTypes::stRight])
            {
               // if H4 > 0, then X3 must be > 0
               if ( IsZero(m_XBeamTaperHeight[pgsTypes::stRight]) )
               {
                  pDX->PrepareCtrl(IDC_H4);
                  AfxMessageBox(_T("H4 must be greater than zero when X3 is greater than zero."));
                  pDX->Fail();
               }
               else if ( m_XBeamTaperLength[pgsTypes::stRight] < m_XBeamEndSlopeOffset[pgsTypes::stRight] )
               {
                  pDX->PrepareCtrl(IDC_X3);
                  AfxMessageBox(_T("X3 must be greater than X4 when X3 is greater than zero."));
                  pDX->Fail();
               }
            }
            else if ( !IsZero(m_XBeamTaperHeight[pgsTypes::stRight]) )
            {
               // if X3 is zero, then H4 must also be zero
               pDX->PrepareCtrl(IDC_H4);
               AfxMessageBox(_T("H4 must be zero when X3 is zero"));
               pDX->Fail();
            }

            Float64 D1, D2;
            // X5 must be >= diameter of first column
            m_Pier.GetColumnData(0).GetColumnDimensions(&D1, &D2);
            DDV_UnitValueLimitOrMore(pDX, IDC_X5, m_XBeamOverhang[pgsTypes::stLeft], D1 / 2, pDisplayUnits->GetSpanLengthUnit());

            // X6 must be >= diameter of first column
            ColumnIndexType nColumns = m_Pier.GetColumnCount();
            m_Pier.GetColumnData(nColumns - 1).GetColumnDimensions(&D1, &D2);
            DDV_UnitValueLimitOrMore(pDX, IDC_X6, m_XBeamOverhang[pgsTypes::stRight], D1 / 2, pDisplayUnits->GetSpanLengthUnit());

            // X1 + X3 must be less than X5 + X6 + Sum(S)
            ATLASSERT(1 <= nColumns);
            Float64 S = 0;
            for (SpacingIndexType spaIdx = 0; spaIdx < nColumns - 1; spaIdx++)
            {
               S += m_Pier.GetColumnSpacing(spaIdx);
            }
            Float64 pierWidth = m_XBeamOverhang[pgsTypes::stLeft] + m_XBeamOverhang[pgsTypes::stRight] + S;
            Float64 sumOverhangs = m_XBeamTaperLength[pgsTypes::stLeft] + m_XBeamTaperLength[pgsTypes::stRight];
            if (pierWidth < sumOverhangs)
            {
               pDX->PrepareCtrl(IDC_X5);
               AfxMessageBox(_T("X1 + X3 cannot exceed the overall pier width (X5 + X6 + summation of S)"));
               pDX->Fail();
            }
        }
    }

}


void CCommonPierLayoutDlg::FillTransverseLocationComboBox()
{
   CComboBox* pcbMeasure = (CComboBox*)GetDlgItem(IDC_REFCOLUMN_MEASUREMENT);
   pcbMeasure->ResetContent();
   int idx = pcbMeasure->AddString(_T("from the Alignment"));
   pcbMeasure->SetItemData(idx,(DWORD_PTR)pgsTypes::omtAlignment);
   idx = pcbMeasure->AddString(_T("from the Bridgeline"));
   pcbMeasure->SetItemData(idx,(DWORD_PTR)pgsTypes::omtBridge);
}

void CCommonPierLayoutDlg::FillRefColumnComboBox(ColumnIndexType nColumns)
{
   CComboBox* pcbRefColumn = (CComboBox*)GetDlgItem(IDC_REFCOLUMN);
   int curSel = pcbRefColumn->GetCurSel();
   pcbRefColumn->ResetContent();
   if (nColumns == INVALID_INDEX)
   {
      nColumns = (ColumnIndexType)m_ColumnLayoutGrid.GetRowCount();
   }

   for ( ColumnIndexType colIdx = 0; colIdx < nColumns; colIdx++ )
   {
      CString strLabel;
      strLabel.Format(_T("Column %d"),LABEL_COLUMN(colIdx));
      pcbRefColumn->AddString(strLabel);
   }

   if ( pcbRefColumn->SetCurSel(curSel) == CB_ERR )
   {
      pcbRefColumn->SetCurSel(0);
   }
}

void CCommonPierLayoutDlg::FillHeightMeasureComboBox()
{
   CComboBox* pcbHeightMeasure = (CComboBox*)GetDlgItem(IDC_HEIGHT_MEASURE);
   pcbHeightMeasure->ResetContent();
   int idx = pcbHeightMeasure->AddString(_T("Column Height (H)"));
   pcbHeightMeasure->SetItemData(idx,(DWORD_PTR)CColumnData::chtHeight);
   idx = pcbHeightMeasure->AddString(_T("Bottom Elevation"));
   pcbHeightMeasure->SetItemData(idx,(DWORD_PTR)CColumnData::chtBottomElevation);
}

void CCommonPierLayoutDlg::OnHeightMeasureChanged()
{
   CComboBox* pcbHeightMeasure = (CComboBox*)GetDlgItem(IDC_HEIGHT_MEASURE);
   int curSel = pcbHeightMeasure->GetCurSel();
   CColumnData::ColumnHeightMeasurementType measure = (CColumnData::ColumnHeightMeasurementType)(pcbHeightMeasure->GetItemData(curSel));
   m_ColumnLayoutGrid.SetHeightMeasurementType(measure);
}


void CCommonPierLayoutDlg::OnAddColumn()
{
   m_ColumnLayoutGrid.AddColumn();
   FillRefColumnComboBox();
}

void CCommonPierLayoutDlg::OnRemoveColumns()
{
   m_ColumnLayoutGrid.RemoveSelectedColumns();
   FillRefColumnComboBox();
}

void CCommonPierLayoutDlg::SetPierModelType(const pgsTypes::PierModelType& pierModelType)
{
    m_PierModelType = pierModelType;
}

void CCommonPierLayoutDlg::SetPierData(const CPierData2& pierData)
{
    m_Pier = pierData;
}

