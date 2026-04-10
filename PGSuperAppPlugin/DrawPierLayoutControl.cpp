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

    // Split the drawing area: left side for front view, right side for side view
    int total_width = rClient.Width();
    int view_split = total_width / 2;  // Split at midpoint

    // ===== LEFT SIDE: FRONT VIEW (Elevation) =====
    CRect rLeftView(rClient.left, rClient.top, rClient.left + view_split, rClient.bottom);
    rLeftView.DeflateRect(1, 1, 1, 1);
    CSize sLeftClient = rLeftView.Size();

    // Get bounding box dimensions for front view
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
    Float64 padding = max(xbeam_width, max_column_height) * 0.25;
    Float64 left = -xbeam_width / 2.0 - padding * 4;
    Float64 right = xbeam_width / 2.0 + padding;
    Float64 top = max_column_height + max_xbeam_height + padding;
    Float64 bottom = -padding;

    WBFL::Graphing::Rect box(left, bottom, right, top);
    WBFL::Graphing::Size size = box.Size();
    WBFL::Graphing::Point org = box.Center();

    WBFL::Graphing::PointMapper mapper;
    mapper.SetMappingMode(WBFL::Graphing::PointMapper::MapMode::Isotropic);
    mapper.SetWorldExt(size);
    mapper.SetWorldOrg(org);
    mapper.SetDeviceExt(sLeftClient.cx, -sLeftClient.cy);  // Negate Y to flip axis
    mapper.SetDeviceOrg(rLeftView.left + sLeftClient.cx / 2, rLeftView.top + sLeftClient.cy / 2);

    // Draw the pier geometry on the left
    DrawPierGeometry(&dc, mapper);

    // ===== RIGHT SIDE: SIDE VIEW =====
    CRect rRightView(rClient.left + view_split, rClient.top, rClient.right, rClient.bottom);
    rRightView.DeflateRect(1, 1, 1, 1);
    CSize sRightClient = rRightView.Size();

    // Create mapper for side view
    Float64 w = pPier->GetXBeamWidth();
    Float64 max_col_depth = 0.0;

    // Get maximum column depth (D2)
    for (ColumnIndexType i = 0; i < nColumns; i++)
    {
        const CColumnData* pColumn = &pPier->GetColumnData(i);
        Float64 col_d1, col_d2;
        pColumn->GetColumnDimensions(&col_d1, &col_d2);
        max_col_depth = max(max_col_depth, col_d2);
    }

    // Bounding box for side view
    Float64 side_left = -w / 2.0 - padding;
    Float64 side_right = w / 2.0 + padding;
    Float64 side_top = max_column_height + max_xbeam_height + padding;
    Float64 side_bottom = -padding;

    WBFL::Graphing::Rect side_box(side_left, side_bottom, side_right, side_top);
    WBFL::Graphing::Size side_size = side_box.Size();
    WBFL::Graphing::Point side_org = side_box.Center();

    WBFL::Graphing::PointMapper side_mapper;
    side_mapper.SetMappingMode(WBFL::Graphing::PointMapper::MapMode::Isotropic);
    side_mapper.SetWorldExt(side_size);
    side_mapper.SetWorldOrg(side_org);
    side_mapper.SetDeviceExt(sRightClient.cx, -sRightClient.cy);
    side_mapper.SetDeviceOrg(rRightView.left + sRightClient.cx / 2, rRightView.top + sRightClient.cy / 2);

    // Draw the side view on the right
    DrawSideView(&dc, side_mapper);
}

void CDrawPierLayoutControl::DrawSideView(CDC* pDC, WBFL::Graphing::PointMapper& mapper)
{
    if (m_pSource == nullptr)
        return;

    const CPierData2* pPier = m_pSource->GetPierData();
    if (pPier == nullptr)
        return;

    ColumnIndexType nColumns = pPier->GetColumnCount();

    // Get cross beam width (W)
    Float64 W = pPier->GetXBeamWidth();

    // Get XBeam heights for reference
    Float64 h_left, h2_left, x1_left, x2_left;
    pPier->GetXBeamDimensions(pgsTypes::stLeft, &h_left, &h2_left, &x1_left, &x2_left);

    Float64 h_right, h2_right, x1_right, x2_right;
    pPier->GetXBeamDimensions(pgsTypes::stRight, &h_right, &h2_right, &x1_right, &x2_right);

    // Maximum height of xbeam in side view: max(H1+H2, H3+H4)
    Float64 max_xbeam_height = max(h_left + h2_left, h_right + h2_right);

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


    // Draw columns (from side view perspective - as simple rectangles)
    // Only draw the FIRST column
    CPen column_pen(PS_SOLID, 1, SEGMENT_BORDER_COLOR);
    CBrush column_brush;
    column_brush.CreateSolidBrush(SEGMENT_FILL_COLOR);

    pDC->SelectObject(&column_pen);
    pDC->SelectObject(&column_brush);

    if (nColumns > 0)
    {
        const CColumnData* pColumn = &pPier->GetColumnData(0);

        Float64 col_d1, col_d2;
        pColumn->GetColumnDimensions(&col_d1, &col_d2);

        Float64 col_height = 0.0;
        if (pColumn->GetColumnHeightMeasurementType() == CColumnData::chtHeight)
        {
            col_height = pColumn->GetColumnHeight();
        }

        // In side view, center the column at origin (x = 0)
        Float64 col_x = 0.0;

        WBFL::Graphing::Point col_bl(col_x - col_d1 / 2.0, 0.0);
        WBFL::Graphing::Point col_br(col_x + col_d1 / 2.0, 0.0);
        WBFL::Graphing::Point col_tr(col_x + col_d1 / 2.0, col_height);
        WBFL::Graphing::Point col_tl(col_x - col_d1 / 2.0, col_height);

        LONG dx_col_bl, dy_col_bl, dx_col_br, dy_col_br, dx_col_tr, dy_col_tr, dx_col_tl, dy_col_tl;
        mapper.WPtoDP(col_bl, &dx_col_bl, &dy_col_bl);
        mapper.WPtoDP(col_br, &dx_col_br, &dy_col_br);
        mapper.WPtoDP(col_tr, &dx_col_tr, &dy_col_tr);
        mapper.WPtoDP(col_tl, &dx_col_tl, &dy_col_tl);

        CPoint col_points[4] = { CPoint(dx_col_bl, dy_col_bl), CPoint(dx_col_br, dy_col_br),
                                  CPoint(dx_col_tr, dy_col_tr), CPoint(dx_col_tl, dy_col_tl) };
        pDC->Polygon(col_points, 4);

    }


    // Draw cross beam rectangle (side view - showing width W)
    // Positioned at top of columns
    CPen xbeam_pen(PS_SOLID, 2, SEGMENT_BORDER_COLOR);
    CBrush xbeam_brush;
    xbeam_brush.CreateSolidBrush(RGB(200, 200, 200));

    CPen* pOldPen = pDC->GetCurrentPen();
    CBrush* pOldBrush = pDC->GetCurrentBrush();

    pDC->SelectObject(&xbeam_pen);
    pDC->SelectObject(&xbeam_brush);

    // Draw cross beam as rectangle from -W/2 to W/2, at top of columns
    WBFL::Graphing::Point xbeam_bl(-W / 2.0, 0.0);
    WBFL::Graphing::Point xbeam_br(W / 2.0, 0.0);
    WBFL::Graphing::Point xbeam_tr(W / 2.0, max_xbeam_height);
    WBFL::Graphing::Point xbeam_tl(-W / 2.0, max_xbeam_height);

    LONG dx_bl, dy_bl, dx_br, dy_br, dx_tr, dy_tr, dx_tl, dy_tl;
    mapper.WPtoDP(xbeam_bl, &dx_bl, &dy_bl);
    mapper.WPtoDP(xbeam_br, &dx_br, &dy_br);
    mapper.WPtoDP(xbeam_tr, &dx_tr, &dy_tr);
    mapper.WPtoDP(xbeam_tl, &dx_tl, &dy_tl);

    CPoint xbeam_points[4] = { CPoint(dx_bl, dy_bl), CPoint(dx_br, dy_br),
                                CPoint(dx_tr, dy_tr), CPoint(dx_tl, dy_tl) };
    pDC->Polygon(xbeam_points, 4);

    CFont font;
    font.CreatePointFont(80, _T("Arial"));  // 8pt font
    CFont* pOldFont = pDC->SelectObject(&font);

    pDC->SetTextColor(RGB(0, 0, 0));
    pDC->SetBkMode(OPAQUE);
    int oldTA = pDC->SetTextAlign(TA_CENTER | TA_BOTTOM);

    // Constants for dimension line placement
    const Float64 DIM_OFFSET = 0.5;

    // Draw dimension for width (W)
    DrawHorizontalDimension(pDC, mapper, -W / 2.0, max_xbeam_height + DIM_OFFSET, W / 2.0, _T("W"));

    pDC->SetTextAlign(oldTA);
    pDC->SelectObject(pOldFont);
    pDC->SelectObject(pOldBrush);
    pDC->SelectObject(pOldPen);
    font.DeleteObject();

}

void CDrawPierLayoutControl::DrawPierGeometry(CDC* pDC, WBFL::Graphing::PointMapper& mapper)
{
    if (m_pSource == nullptr)
        return;

    const CPierData2* pPier = m_pSource->GetPierData();
    if (pPier == nullptr)
        return;

    // Get XBeam and overhang dimensions
    Float64 H1, H2, H3, H4;
    Float64 X1, X2, X3, X4, W;
    pPier->GetXBeamDimensions(pgsTypes::stLeft, &H1, &H2, &X1, &X2);
    pPier->GetXBeamDimensions(pgsTypes::stRight, &H3, &H4, &X3, &X4);
    W = pPier->GetXBeamWidth();

    Float64 X5 = pPier->GetXBeamOverhang(pgsTypes::stLeft);
    Float64 X6 = pPier->GetXBeamOverhang(pgsTypes::stRight);

    ColumnIndexType nColumns = pPier->GetColumnCount();

    // Calculate total column spacing
    Float64 S = 0.0;
    for (SpacingIndexType spaIdx = 0; spaIdx < nColumns - 1; spaIdx++)
    {
        S += pPier->GetColumnSpacing(spaIdx);
    }

    // Total width at base: X5 + S + X6
    Float64 pierWidth = X5 + S + X6;

    // Draw columns
    CPen column_pen(PS_SOLID, 1, SEGMENT_BORDER_COLOR);
    CBrush column_brush;
    column_brush.CreateSolidBrush(SEGMENT_FILL_COLOR);

    CPen* pOldPen = pDC->GetCurrentPen();
    CBrush* pOldBrush = pDC->GetCurrentBrush();

    pDC->SelectObject(&column_pen);
    pDC->SelectObject(&column_brush);

    // Calculate column positions starting from left edge
    Float64 col_x = -pierWidth / 2.0 + X5; // Start at first column position

    // Draw each column
    for (ColumnIndexType i = 0; i < nColumns; i++)
    {
        const CColumnData* pColumn = &pPier->GetColumnData(i);

        // Get column height
        Float64 col_height = 0.0;
        if (pColumn->GetColumnHeightMeasurementType() == CColumnData::chtHeight)
        {
            col_height = pColumn->GetColumnHeight();
        }

        // Get column dimensions
        Float64 col_d1, col_d2;
        pColumn->GetColumnDimensions(&col_d1, &col_d2);

        // Use average for width/depth visualization (simplified)
        Float64 col_width = col_d1;
        Float64 col_depth = col_d2;

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

        // Move to next column position
        if (i < nColumns - 1)
        {
            col_x += pPier->GetColumnSpacing((SpacingIndexType)i);
        }
    }

    // Draw XBeam (trapezoid with 7 points)
    CPen xbeam_pen(PS_SOLID, 2, SEGMENT_BORDER_COLOR);
    CBrush xbeam_brush;
    xbeam_brush.CreateSolidBrush(RGB(200, 200, 200));

    pDC->SelectObject(&xbeam_pen);
    pDC->SelectObject(&xbeam_brush);

    WBFL::Graphing::Point xbeam_points[7];
    xbeam_points[0] = WBFL::Graphing::Point(-pierWidth / 2.0, 0.0);
    xbeam_points[1] = WBFL::Graphing::Point(-pierWidth / 2.0 + X2, H1);
    xbeam_points[2] = WBFL::Graphing::Point(-pierWidth / 2.0 + X2 + X1, H1 + H2);
    xbeam_points[3] = WBFL::Graphing::Point(pierWidth / 2.0 - X2 - X3, H3 + H4);
    xbeam_points[4] = WBFL::Graphing::Point(pierWidth / 2.0 - X2, H3);
    xbeam_points[5] = WBFL::Graphing::Point(pierWidth / 2.0, 0.0);

    CPoint dev_points[7];
    IndexType nPoints = 6;  // We have 6 points for the xbeam polygon

    for (IndexType i = 0; i < nPoints; i++)
    {
        LONG dx, dy;
        mapper.WPtoDP(xbeam_points[i], &dx, &dy);
        dev_points[i] = CPoint(dx, dy);
    }

    pDC->Polygon(dev_points, (int)nPoints);

    pDC->SelectObject(pOldPen);
    pDC->SelectObject(pOldBrush);

    // Draw symbolic dimensions
    DrawSymbolicDimensions(pDC, mapper, H1, H2, X1, X2, H3, H4, X3, X4);
}

void CDrawPierLayoutControl::DrawSymbolicDimensions(CDC* pDC, WBFL::Graphing::PointMapper& mapper,
    Float64 H1, Float64 H2, Float64 X1, Float64 X2,
    Float64 H3, Float64 H4, Float64 X3, Float64 X4)
{
    if (m_pSource == nullptr)
        return;

    const CPierData2* pPier = m_pSource->GetPierData();
    if (pPier == nullptr)
        return;

    CPen dim_pen(PS_SOLID, 1, RGB(0, 0, 0));
    CPen* pOldPen = pDC->SelectObject(&dim_pen);

    CFont font;
    font.CreatePointFont(80, _T("Arial"));  // 8pt font
    CFont* pOldFont = pDC->SelectObject(&font);

    pDC->SetTextColor(RGB(0, 0, 0));
    pDC->SetBkMode(OPAQUE);
    int oldTA = pDC->SetTextAlign(TA_CENTER | TA_BOTTOM);

    // Constants for dimension line placement
    const Float64 DIM_OFFSET = 0.5;

    //Float64 xbeam_width = pPier->GetXBeamWidth();
    Float64 X5 = pPier->GetXBeamOverhang(pgsTypes::stLeft);
    Float64 X6 = pPier->GetXBeamOverhang(pgsTypes::stRight);

    ColumnIndexType nColumns = pPier->GetColumnCount();
    Float64 S = 0.0;
    for (SpacingIndexType spaIdx = 0; spaIdx < nColumns - 1; spaIdx++)
    {
        S += pPier->GetColumnSpacing(spaIdx);
    }
    Float64 pierWidth = X5 + S + X6;

    // H1 dimension (left xbeam height)
    if (::IsGT(0.0, H1))
    {
        DrawVerticalDimension(pDC, mapper, -pierWidth / 2.0 - DIM_OFFSET * 2, 0.0, H1, _T("H1"));
    }

    // H3 dimension (right xbeam height)
    if (::IsGT(0.0, H3))
    {
        DrawVerticalDimension(pDC, mapper, pierWidth / 2.0 + DIM_OFFSET * 3, 0.0, H3, _T("H3"));
    }

    // X5 dimension (left overhang)
    Float64 left_edge = -pierWidth / 2.0;
	if (::IsGT(0.0, X5))
    {
        DrawHorizontalDimension(pDC, mapper, left_edge, H1 + H2 + DIM_OFFSET * 4, left_edge + X5, _T("X5"));
    }

    // X6 dimension (right overhang)
    Float64 right_edge = pierWidth / 2.0;
	if (::IsGT(0.0, X6))
    {
        DrawHorizontalDimension(pDC, mapper, right_edge - X6, H3 + H4 + DIM_OFFSET * 4, right_edge, _T("X6"));
    }

    // H2 dimension (left taper height) - if non-zero
    if (::IsGT(0.0, H2))
    {
        DrawVerticalDimension(pDC, mapper, -pierWidth / 2.0 - DIM_OFFSET * 2, H1, H1 + H2, _T("H2"));
    }

    // H4 dimension (right taper height) - if non-zero
    if (::IsGT(0.0, H4))
    {
        DrawVerticalDimension(pDC, mapper, pierWidth / 2.0 + DIM_OFFSET * 3, H3, H3 + H4, _T("H4"));
    }

    // X1 dimension (left taper length) - if non-zero
    if (::IsGT(0.0, X1))
    {
        DrawHorizontalDimension(pDC, mapper, -pierWidth / 2.0 + X2, H1 + H2 + DIM_OFFSET,
            -pierWidth / 2.0 + X2 + X1, _T("X1"));
    }

    // X2 dimension - if non-zero
    if (::IsGT(0.0, X2))
    {
        DrawHorizontalDimension(pDC, mapper, -pierWidth / 2.0, H1 + H2 + DIM_OFFSET,
            -pierWidth / 2.0 + X2, _T("X2"));
    }

    // X3 dimension (right taper length) - if non-zero
    if (::IsGT(0.0, X3))
    {
        DrawHorizontalDimension(pDC, mapper, pierWidth / 2.0 - X4 - X3, H3 + H4 + DIM_OFFSET,
            pierWidth / 2.0 - X4, _T("X3"));
    }

    // X4 dimension (right taper length) - if non-zero
    if (::IsGT(0.0, X4))
    {
        DrawHorizontalDimension(pDC, mapper, pierWidth / 2.0 - X4, H3 + H4 + DIM_OFFSET,
            pierWidth / 2.0, _T("X4"));
    }

    pDC->SetTextAlign(oldTA);
    pDC->SelectObject(pOldFont);
    pDC->SelectObject(pOldPen);
    font.DeleteObject();
}

void CDrawPierLayoutControl::DrawHorizontalDimension(CDC* pDC, WBFL::Graphing::PointMapper& mapper,
    Float64 x1, Float64 y, Float64 x2, LPCTSTR pszLabel)
{
    WBFL::Graphing::Point pt_start(x1, y);
    WBFL::Graphing::Point pt_end(x2, y);

    LONG dx_start, dy_start, dx_end, dy_end;
    mapper.WPtoDP(pt_start, &dx_start, &dy_start);
    mapper.WPtoDP(pt_end, &dx_end, &dy_end);

    // Draw dimension line
    pDC->MoveTo(dx_start, dy_start);
    pDC->LineTo(dx_end, dy_end);

    // Draw tick marks at start and end
    const int TICK_SIZE = 3;
    pDC->MoveTo(dx_start, dy_start - TICK_SIZE);
    pDC->LineTo(dx_start, dy_start + TICK_SIZE);
    pDC->MoveTo(dx_end, dy_end - TICK_SIZE);
    pDC->LineTo(dx_end, dy_end + TICK_SIZE);

    // Draw label at midpoint - let TA_CENTER do the centering
    int mid_x = (dx_start + dx_end) / 2;
    int mid_y = (dy_start + dy_end) / 2 - 10;  // Only offset for vertical spacing
    pDC->TextOutW(mid_x, mid_y, pszLabel);
}

void CDrawPierLayoutControl::DrawVerticalDimension(CDC* pDC, WBFL::Graphing::PointMapper& mapper,
    Float64 x, Float64 y1, Float64 y2, LPCTSTR pszLabel)
{
    WBFL::Graphing::Point pt_start(x, y1);
    WBFL::Graphing::Point pt_end(x, y2);

    LONG dx_start, dy_start, dx_end, dy_end;
    mapper.WPtoDP(pt_start, &dx_start, &dy_start);
    mapper.WPtoDP(pt_end, &dx_end, &dy_end);

    // Draw dimension line
    pDC->MoveTo(dx_start, dy_start);
    pDC->LineTo(dx_end, dy_end);

    // Draw tick marks at start and end
    const int TICK_SIZE = 3;
    pDC->MoveTo(dx_start - TICK_SIZE, dy_start);
    pDC->LineTo(dx_start + TICK_SIZE, dy_start);
    pDC->MoveTo(dx_end - TICK_SIZE, dy_end);
    pDC->LineTo(dx_end + TICK_SIZE, dy_end);

    // Draw label at midpoint - let TA_CENTER do the centering
    int mid_x = (dx_start + dx_end) / 2 - 15;  // Only offset for horizontal spacing
    int mid_y = (dy_start + dy_end) / 2;
    pDC->TextOutW(mid_x, mid_y, pszLabel);
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