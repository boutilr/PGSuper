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
// DrawPierLayoutControl.cpp : implementation file
//

#include "stdafx.h"
#include "PGSuperPluginApp.h"
#include "PGSuperColors.h"
#include "DrawPierLayoutControl.h"

#include <IFace/Tools.h>
#include <IFace\Bridge.h>
#include <PsgLib\PierData2.h>

IMPLEMENT_DYNAMIC(CDrawPierLayoutControl, CWnd)

CDrawPierLayoutControl::CDrawPierLayoutControl()
{
    m_pSource = nullptr;
}

CDrawPierLayoutControl::~CDrawPierLayoutControl()
{
}

BEGIN_MESSAGE_MAP(CDrawPierLayoutControl, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

void CDrawPierLayoutControl::CustomInit(IPierLayoutDataSource* pSource)
{
    m_pSource = pSource;
}

void CDrawPierLayoutControl::OnPaint()
{
    CPaintDC dc(this); // device context for painting

    // set up the clipping region so we don't draw outside of the client rect
    CRect rClient;
    GetClientRect(&rClient);
    CRgn rgn;
    rgn.CreateRectRgnIndirect(&rClient);
    dc.SelectClipRgn(&rgn);

    if (m_pSource == nullptr)
        return;

    const CPierData2* pPier = m_pSource->GetPierData();
    if (pPier == nullptr)
        return;

    // Deflate rect and create coordinate mapper
    rClient.DeflateRect(1, 1, 1, 1);
    CSize sClient = rClient.Size();

    // Get bounding box dimensions
    Float64 xbeam_width = pPier->GetXBeamWidth();
    ColumnIndexType nColumns = pPier->GetColumnCount();

    // Get maximum column height
    Float64 max_column_height = 0.0;
    for (ColumnIndexType i = 0; i < nColumns; i++)
    {
        const CColumnData* pColumn = &pPier->GetColumnData(i);
        if (pColumn->GetColumnHeightMeasurementType() == CColumnData::chtHeight)
        {
            max_column_height = max(max_column_height, pColumn->GetColumnHeight());
        }
    }

    // Get XBeam dimensions to determine max height
    Float64 h_left, h2_left, x1_left, x2_left;
    pPier->GetXBeamDimensions(pgsTypes::stLeft, &h_left, &h2_left, &x1_left, &x2_left);

    Float64 h_right, h2_right, x1_right, x2_right;
    pPier->GetXBeamDimensions(pgsTypes::stRight, &h_right, &h2_right, &x1_right, &x2_right);

    Float64 max_xbeam_height = max(h_left, h_right);

    // Calculate bounding box with padding
    Float64 padding = max(xbeam_width, max_column_height) * 0.1; // 10% padding
    Float64 left = -xbeam_width / 2.0 - padding;
    Float64 right = xbeam_width / 2.0 + padding;
    Float64 top = max_column_height + max_xbeam_height + padding;
    Float64 bottom = -padding;

    WBFL::Graphing::Rect box(left, -bottom, right, -top);
    WBFL::Graphing::Size size = box.Size();
    WBFL::Graphing::Point org = box.Center();

    WBFL::Graphing::PointMapper mapper;
    mapper.SetMappingMode(WBFL::Graphing::PointMapper::MapMode::Isotropic);
    mapper.SetWorldExt(size);
    mapper.SetWorldOrg(org);
    mapper.SetDeviceExt(sClient.cx, sClient.cy);
    mapper.SetDeviceOrg(sClient.cx / 2, sClient.cy / 2);

    // Draw the pier geometry
    DrawPierGeometry(&dc, mapper);
}

void CDrawPierLayoutControl::DrawPierGeometry(CDC* pDC, WBFL::Graphing::PointMapper& mapper)
{
    if (m_pSource == nullptr)
        return;

    const CPierData2* pPier = m_pSource->GetPierData();
    if (pPier == nullptr)
        return;

    // Draw columns
    ColumnIndexType nColumns = pPier->GetColumnCount();
    Float64 xbeam_width = pPier->GetXBeamWidth();
    Float64 col_spacing = xbeam_width / (nColumns > 0 ? (Float64)(nColumns - 1) : 1.0);

    // Column dimensions (simplified)
    Float64 col_width = 2.0; // placeholder
    Float64 col_depth = 2.0; // placeholder

    CPen column_pen(PS_SOLID, 1, SEGMENT_BORDER_COLOR);
    CBrush column_brush;
    column_brush.CreateSolidBrush(SEGMENT_FILL_COLOR);

    CPen* pOldPen = pDC->GetCurrentPen();
    CBrush* pOldBrush = pDC->GetCurrentBrush();

    pDC->SelectObject(&column_pen);
    pDC->SelectObject(&column_brush);

    // Draw each column
    for (ColumnIndexType i = 0; i < nColumns; i++)
    {
        const CColumnData* pColumn = &pPier->GetColumnData(i);

        // Calculate column position (centered)
        Float64 col_x = -xbeam_width / 2.0 + i * col_spacing;

        // Get column height
        Float64 col_height = 0.0;
        if (pColumn->GetColumnHeightMeasurementType() == CColumnData::chtHeight)
        {
            col_height = pColumn->GetColumnHeight();
        }

        // Define column rectangle in world coordinates (from bottom to top)
        WBFL::Graphing::Point pt_bottom_left(col_x - col_width / 2.0, 0.0);
        WBFL::Graphing::Point pt_bottom_right(col_x + col_width / 2.0, 0.0);
        WBFL::Graphing::Point pt_top_left(col_x - col_width / 2.0, col_height);
        WBFL::Graphing::Point pt_top_right(col_x + col_width / 2.0, col_height);

        // Convert to device coordinates
        LONG dx_bl, dy_bl, dx_br, dy_br, dx_tl, dy_tl, dx_tr, dy_tr;
        mapper.WPtoDP(pt_bottom_left, &dx_bl, &dy_bl);
        mapper.WPtoDP(pt_bottom_right, &dx_br, &dy_br);
        mapper.WPtoDP(pt_top_left, &dx_tl, &dy_tl);
        mapper.WPtoDP(pt_top_right, &dx_tr, &dy_tr);

        CPoint points[4] = { CPoint(dx_bl, dy_bl), CPoint(dx_br, dy_br),
                              CPoint(dx_tr, dy_tr), CPoint(dx_tl, dy_tl) };
        pDC->Polygon(points, 4);
    }

    // Draw XBeam (trapezoid)
    CPen xbeam_pen(PS_SOLID, 2, SEGMENT_BORDER_COLOR);
    CBrush xbeam_brush;
    xbeam_brush.CreateSolidBrush(RGB(200, 200, 200));

    pDC->SelectObject(&xbeam_pen);
    pDC->SelectObject(&xbeam_brush);

    // Get XBeam dimensions
    Float64 h_left, h2_left, x1_left, x2_left;
    pPier->GetXBeamDimensions(pgsTypes::stLeft, &h_left, &h2_left, &x1_left, &x2_left);

    Float64 h_right, h2_right, x1_right, x2_right;
    pPier->GetXBeamDimensions(pgsTypes::stRight, &h_right, &h2_right, &x1_right, &x2_right);

    Float64 overhang_left = pPier->GetXBeamOverhang(pgsTypes::stLeft);
    Float64 overhang_right = pPier->GetXBeamOverhang(pgsTypes::stRight);

    WBFL::Graphing::Point xbeam_points[4];
    xbeam_points[0] = WBFL::Graphing::Point(-xbeam_width / 2.0 - overhang_left, 0.0);
    xbeam_points[1] = WBFL::Graphing::Point(xbeam_width / 2.0 + overhang_right, 0.0);
    xbeam_points[2] = WBFL::Graphing::Point(xbeam_width / 2.0, h_right);
    xbeam_points[3] = WBFL::Graphing::Point(-xbeam_width / 2.0, h_left);

    CPoint dev_points[4];
    for (int i = 0; i < 4; i++)
    {
        LONG dx, dy;
        mapper.WPtoDP(xbeam_points[i], &dx, &dy);
        dev_points[i] = CPoint(dx, dy);
    }

    pDC->Polygon(dev_points, 4);

    pDC->SelectObject(pOldPen);
    pDC->SelectObject(pOldBrush);
}

BOOL CDrawPierLayoutControl::OnEraseBkgnd(CDC* pDC)
{
    CBrush brush(::GetSysColor(COLOR_WINDOW));
    brush.UnrealizeObject();

    CPen pen(PS_SOLID, 1, ::GetSysColor(COLOR_WINDOW));
    pen.UnrealizeObject();

    CBrush* pOldBrush = pDC->SelectObject(&brush);
    CPen* pOldPen = pDC->SelectObject(&pen);

    CRect rect;
    GetClientRect(&rect);
    pDC->Rectangle(rect);

    pDC->SelectObject(pOldPen);
    pDC->SelectObject(pOldBrush);

    return TRUE;
}