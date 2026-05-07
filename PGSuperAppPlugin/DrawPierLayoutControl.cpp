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

#include "DisplayObjectFactory.h"

#include <WBFLGenericBridge.h>

#include <WBFLGeometry/GeomHelpers.h>

#include <EAF\EAFDisplayUnits.h>

#include <IFace/Tools.h>
#include <IFace\Bridge.h>
#include <PsgLib\PierData2.h>

#define XBEAM_LINE_COLOR               GREY50
#define XBEAM_FILL_COLOR               GREY70

#define CROSSBEAM_DISPLAY_LIST_ID        0

const WBFL::DManip::SelectionType g_selectionType = WBFL::DManip::SelectionType::None; // nothing is selectable, except for the section cut object

IMPLEMENT_DYNAMIC(CDrawPierLayoutControl, CDisplayWnd) //I guess I manually create the object then?

CDrawPierLayoutControl::CDrawPierLayoutControl()
{
    m_pSource = nullptr;
    m_bInitialized = FALSE;
    m_bDragging = FALSE;
    m_initialExtFront = WBFL::Graphing::Size(0, 0);
    m_initialExtSide = WBFL::Graphing::Size(0, 0);
    m_currentExtFront = WBFL::Graphing::Size(0, 0);
    m_currentExtSide = WBFL::Graphing::Size(0, 0);
    m_DisplayObjectID = 0;
}

CDrawPierLayoutControl::~CDrawPierLayoutControl()
{
}

BEGIN_MESSAGE_MAP(CDrawPierLayoutControl, CDisplayWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONUP()
    ON_WM_MOUSEMOVE()
    ON_WM_LBUTTONDBLCLK()
END_MESSAGE_MAP()

void CDrawPierLayoutControl::OnDraw(CDC* pDC)
{
    CDisplayWnd::OnDraw(pDC);
}

void CDrawPierLayoutControl::CustomInit(IPierLayoutDataSource* pSource)
{

    CDisplayWnd::CustomInit();
    
    m_pSource = pSource;

    if (m_pSource == nullptr)
        return;

    const CPierData2* pPier = m_pSource->GetPierData();
    if (pPier == nullptr)
        return;

    //auto doFactory = std::make_shared<CDisplayObjectFactory>(); // Do I need this? If so I will need another constructor method like in XBeamRate.
    //m_pDispMgr->AddDisplayObjectFactory(doFactory);

    m_pDispMgr->CreateDisplayList(CROSSBEAM_DISPLAY_LIST_ID);

    //CDManipClientDC dc2(this); // do I need this?

    auto displayList = m_pDispMgr->FindDisplayList(CROSSBEAM_DISPLAY_LIST_ID);

    auto pBroker = EAFGetBroker();

    GET_IFACE2(pBroker, IBridge, pBridge);
    PierIndexType pierIdx = pPier->GetIndex();

    // Model Upper Cross Beam (Elevation)
    WBFL::Geometry::Point2d point(0, 0);

    //if (pProject->GetPierType(pierID) != xbrTypes::pctExpansion)
    {
        auto doUpperXBeam = WBFL::DManip::PointDisplayObject::Create(m_DisplayObjectID++);
        doUpperXBeam->SetPosition(point, false, false);
        doUpperXBeam->SetSelectionType(g_selectionType);

        CComPtr<IShape> upperXBeamShape;
        pBridge->GetUpperXBeamProfile(pierIdx, &upperXBeamShape);

        auto upperXBeamDrawStrategy = WBFL::DManip::ShapeDrawStrategy::Create();
        upperXBeamDrawStrategy->SetShape(geomUtil::ConvertShape(upperXBeamShape));
        upperXBeamDrawStrategy->SetSolidLineColor(XBEAM_LINE_COLOR);
        upperXBeamDrawStrategy->SetSolidFillColor(XBEAM_FILL_COLOR);
        upperXBeamDrawStrategy->Fill(true);

        doUpperXBeam->SetDrawingStrategy(upperXBeamDrawStrategy);

        auto upper_xbeam_gravity_well = WBFL::DManip::ShapeGravityWellStrategy::Create();
        upper_xbeam_gravity_well->SetShape(geomUtil::ConvertShape(upperXBeamShape));
        doUpperXBeam->SetGravityWellStrategy(upper_xbeam_gravity_well);

        displayList->AddDisplayObject(doUpperXBeam);
    }

    // Model Lower Cross Beam (Elevation)
    auto doLowerXBeam = WBFL::DManip::PointDisplayObject::Create(m_DisplayObjectID++);
    doLowerXBeam->SetPosition(point, false, false);
    doLowerXBeam->SetSelectionType(g_selectionType);

    CComPtr<IShape> pLowerXBeamShape;
	pBridge->GetLowerXBeamProfile(pierIdx, &pLowerXBeamShape);


    auto lowerXBeamDrawStrategy = WBFL::DManip::ShapeDrawStrategy::Create();
    lowerXBeamDrawStrategy->SetShape(geomUtil::ConvertShape(pLowerXBeamShape));
    lowerXBeamDrawStrategy->SetSolidLineColor(XBEAM_LINE_COLOR);
    lowerXBeamDrawStrategy->SetSolidFillColor(XBEAM_FILL_COLOR);
    lowerXBeamDrawStrategy->Fill(true);

    doLowerXBeam->SetDrawingStrategy(lowerXBeamDrawStrategy);

    auto lower_xbeam_gravity_well = WBFL::DManip::ShapeGravityWellStrategy::Create();
    lower_xbeam_gravity_well->SetShape(geomUtil::ConvertShape(pLowerXBeamShape));
    doLowerXBeam->SetGravityWellStrategy(lower_xbeam_gravity_well);

    displayList->AddDisplayObject(doLowerXBeam);
}

void CDrawPierLayoutControl::OnLButtonDown(UINT nFlags, CPoint point)
{
    m_bDragging = TRUE;
    m_dragStart = point;
    m_dragEnd = point;
    SetCapture();
}

void CDrawPierLayoutControl::OnMouseMove(UINT nFlags, CPoint point)
{
    if (m_bDragging)
    {
        // Use XOR to erase the old box and draw the new one
        CClientDC dc(this);
        CPen pen(PS_DASH, 1, RGB(0, 0, 255));
        CPen* pOldPen = dc.SelectObject(&pen);
        int oldRop = dc.SetROP2(R2_XORPEN);

        // Erase old box by drawing it again with XOR
        dc.MoveTo(m_dragStart);
        dc.LineTo(m_dragEnd.x, m_dragStart.y);
        dc.LineTo(m_dragEnd);
        dc.LineTo(m_dragStart.x, m_dragEnd.y);
        dc.LineTo(m_dragStart);

        // Update to new position
        m_dragEnd = point;

        // Draw new box with XOR
        dc.MoveTo(m_dragStart);
        dc.LineTo(m_dragEnd.x, m_dragStart.y);
        dc.LineTo(m_dragEnd);
        dc.LineTo(m_dragStart.x, m_dragEnd.y);
        dc.LineTo(m_dragStart);

        dc.SetROP2(oldRop);
        dc.SelectObject(pOldPen);

    }
}

void CDrawPierLayoutControl::OnLButtonUp(UINT nFlags, CPoint point)
{
    ReleaseCapture();
    m_bDragging = FALSE;

    if (m_dragStart == m_dragEnd)
        return;  // No meaningful drag

    CRect rClient;
    GetClientRect(&rClient);
    int view_split = (rClient.Width() * 3) / 4;

    // Determine which view was dragged in
    CRect rLeftView(rClient.left, rClient.top, rClient.left + view_split, rClient.bottom);
    CRect rRightView(rClient.left + view_split, rClient.top, rClient.right, rClient.bottom);

    if (rLeftView.PtInRect(m_dragStart))
    {
        // Zoom front view - drag box becomes the new view
        int minX = min(m_dragStart.x, m_dragEnd.x) - rLeftView.left;
        int maxX = max(m_dragStart.x, m_dragEnd.x) - rLeftView.left;
        int minY = min(m_dragStart.y, m_dragEnd.y) - rLeftView.top;
        int maxY = max(m_dragStart.y, m_dragEnd.y) - rLeftView.top;

        CSize dragSize(abs(maxX - minX), abs(maxY - minY));
        if (dragSize.cx > 20 && dragSize.cy > 20)
        {
            // Calculate the world region covered by the drag box
            Float64 worldWidthOfDrag = dragSize.cx * m_currentExtFront.Dx() / rLeftView.Width();
            Float64 worldHeightOfDrag = dragSize.cy * m_currentExtFront.Dy() / rLeftView.Height();

            // Center of drag box in device coords relative to view center
            int dragCenterDeviceX = (minX + maxX) / 2 - rLeftView.Width() / 2;
            int dragCenterDeviceY = (minY + maxY) / 2 - rLeftView.Height() / 2;

            // Convert drag center to world coordinates
            Float64 dragCenterWorldX = (dragCenterDeviceX * m_currentExtFront.Dx()) / rLeftView.Width();
            Float64 dragCenterWorldY = (dragCenterDeviceY * m_currentExtFront.Dy()) / rLeftView.Height();

            // Update origin to center on the drag box
            m_currentOrgFront = WBFL::Graphing::Point(
                m_currentOrgFront.X() + dragCenterWorldX,
                m_currentOrgFront.Y() + dragCenterWorldY
            );

            // Update extent to match the drag box size
            m_currentExtFront = WBFL::Graphing::Size(worldWidthOfDrag, worldHeightOfDrag);
        }
    }
    else if (rRightView.PtInRect(m_dragStart))
    {
        // Zoom side view (same logic)
        int minX = min(m_dragStart.x, m_dragEnd.x) - rRightView.left;
        int maxX = max(m_dragStart.x, m_dragEnd.x) - rRightView.left;
        int minY = min(m_dragStart.y, m_dragEnd.y) - rRightView.top;
        int maxY = max(m_dragStart.y, m_dragEnd.y) - rRightView.top;

        CSize dragSize(abs(maxX - minX), abs(maxY - minY));
        if (dragSize.cx > 20 && dragSize.cy > 20)
        {
            Float64 worldWidthOfDrag = dragSize.cx * m_currentExtSide.Dx() / rRightView.Width();
            Float64 worldHeightOfDrag = dragSize.cy * m_currentExtSide.Dy() / rRightView.Height();

            int dragCenterDeviceX = (minX + maxX) / 2 - rRightView.Width() / 2;
            int dragCenterDeviceY = (minY + maxY) / 2 - rRightView.Height() / 2;

            Float64 dragCenterWorldX = (dragCenterDeviceX * m_currentExtSide.Dx()) / rRightView.Width();
            Float64 dragCenterWorldY = (dragCenterDeviceY * m_currentExtSide.Dy()) / rRightView.Height();

            m_currentOrgSide = WBFL::Graphing::Point(
                m_currentOrgSide.X() + dragCenterWorldX,
                m_currentOrgSide.Y() + dragCenterWorldY
            );

            m_currentExtSide = WBFL::Graphing::Size(worldWidthOfDrag, worldHeightOfDrag);
        }
    }

    InvalidateRect(NULL, TRUE);  // TRUE = erase background and force complete repaint
}

void CDrawPierLayoutControl::ResetExtents()
{
	m_bInitialized = FALSE;
}

void CDrawPierLayoutControl::OnPaint()
{
    CPaintDC dc(this);

    CRect rClient;
    GetClientRect(&rClient);


    int total_width = rClient.Width();
    int view_split = (total_width * 3) / 4;

    // LEFT SIDE: FRONT VIEW
    CRect rLeftView(rClient.left, rClient.top, rClient.left + view_split, rClient.bottom);
    rLeftView.DeflateRect(1, 1, 1, 1);
    CSize sLeftClient = rLeftView.Size();

    // Set clipping region for left view
    CRgn rgnLeft;
    rgnLeft.CreateRectRgnIndirect(&rLeftView);
    dc.SelectClipRgn(&rgnLeft);

    if (m_pSource == nullptr)
        return;

    const CPierData2* pPier = m_pSource->GetPierData();
    if (pPier == nullptr)
        return;

    WBFL::Graphing::PointMapper mapper;
    CalculateFrontViewBoundingBox(pPier, mapper, sLeftClient);

    // On first paint, store FRONT VIEW initial extents
    if (!m_bInitialized)
    {
        m_initialExtFront = mapper.GetWorldExt();
        m_initialOrgFront = mapper.GetWorldOrg();
        m_currentExtFront = m_initialExtFront;
        m_currentOrgFront = m_initialOrgFront;
        m_bInitialized = TRUE;  // Set this early
    }

    // Apply current zoom level
    mapper.SetWorldExt(m_currentExtFront);
    mapper.SetWorldOrg(m_currentOrgFront);
    mapper.SetDeviceOrg(rLeftView.left + sLeftClient.cx / 2, rLeftView.top + sLeftClient.cy / 2);

    DrawPierGeometry(&dc, mapper);

    //// RIGHT SIDE: SIDE VIEW
    //CRect rRightView(rClient.left + view_split, rClient.top, rClient.right, rClient.bottom);
    //rRightView.DeflateRect(1, 1, 1, 1);
    //CSize sRightClient = rRightView.Size();

    //// Set clipping region for right view
    //CRgn rgnRight;
    //rgnRight.CreateRectRgnIndirect(&rRightView);
    //dc.SelectClipRgn(&rgnRight);

    //// Erase background for right view
    //CBrush brushBkgnd(::GetSysColor(COLOR_WINDOW));
    //dc.FillRect(&rRightView, &brushBkgnd);

    //WBFL::Graphing::PointMapper side_mapper;
    //CalculateSideViewBoundingBox(pPier, side_mapper, sRightClient);

    //// Store SIDE VIEW initial extents only if not already set
    //if (m_initialExtSide.Dx() == 0 || m_initialExtSide.Dy() == 0)
    //{
    //    m_initialExtSide = side_mapper.GetWorldExt();
    //    m_initialOrgSide = side_mapper.GetWorldOrg();
    //    m_currentExtSide = m_initialExtSide;
    //    m_currentOrgSide = m_initialOrgSide;
    //}

    //side_mapper.SetWorldExt(m_currentExtSide);
    //side_mapper.SetWorldOrg(m_currentOrgSide);
    //side_mapper.SetDeviceOrg(rRightView.left + sRightClient.cx / 2, rRightView.top + sRightClient.cy / 2);

    //DrawSideView(&dc, side_mapper);

    //// Clear clipping region
    //dc.SelectClipRgn(NULL);
    OnDraw(&dc);

}

void CDrawPierLayoutControl::OnLButtonDblClk(UINT nFlags, CPoint point)
{
    // Reset to initial zoom
    m_currentExtFront = m_initialExtFront;
    m_currentOrgFront = m_initialOrgFront;
    m_currentExtSide = m_initialExtSide;
    m_currentOrgSide = m_initialOrgSide;

    InvalidateRect(NULL, TRUE);  // TRUE = erase background and force complete repaint
}

void CDrawPierLayoutControl::GetUpperXBeamDimensions(const CPierData2* pPier, Float64* pH, Float64* pW)
{
    auto pBroker = EAFGetBroker();
    GET_IFACE2(pBroker, IBridge, pBridge);
    auto pierIdx = pPier->GetIndex();

    // Draw Upper Cross Beam Diaphragm. Basically, this is vertical distance from top of lower cross beam to bottom of slab
    // Take max of diaphragm depth and max girder bearing deducts
    // (don't use the pPier object here... use the pBridge interface... it resolves
    // diaphragm dimensions that are computed based on bridge component geometry)
    Float64 Wback, Hback;
    pBridge->GetPierDiaphragmSize(pierIdx, pgsTypes::Back, &Wback, &Hback);
    Float64 Wahead, Hahead;
    pBridge->GetPierDiaphragmSize(pierIdx, pgsTypes::Ahead, &Wahead, &Hahead);
    *pW = Wback + Wahead;
    Float64 Hdiap = Max(Hback, Hahead);

    Float64 Hbd = 0;

    // Compute elevation ignoring effects on non-recoverable deformations. We don't need to perform a full structural analysis to get the values we want
    std::vector<BearingElevationDetails> vBackElevDetails = pBridge->GetBearingElevationDetails(pierIdx, pgsTypes::Back, ALL_GIRDERS, true);
    for (const auto& elevdet : vBackElevDetails)
    {
        Hbd = max(Hbd, elevdet.BrgHeight + elevdet.Hg + elevdet.SlabOffset - elevdet.GrossSlabDepth);
    }

    std::vector<BearingElevationDetails> vAheadElevDetails = pBridge->GetBearingElevationDetails(pierIdx, pgsTypes::Ahead, ALL_GIRDERS, true);
    for (const auto& elevdet : vAheadElevDetails)
    {
        Hbd = max(Hbd, elevdet.BrgHeight + elevdet.Hg + elevdet.SlabOffset - elevdet.GrossSlabDepth);
    }

    *pH = max(Hdiap, Hbd);
}

void CDrawPierLayoutControl::CalculateFrontViewBoundingBox(const CPierData2* pPier,
    WBFL::Graphing::PointMapper& mapper, CSize sDeviceClient)
{
    // Get pier dimensions
    ColumnIndexType nColumns = pPier->GetColumnCount();

    // Calculate column spacing sum
    Float64 S = 0.0;
    for (SpacingIndexType spaIdx = 0; spaIdx < nColumns - 1; spaIdx++)
    {
        S += pPier->GetColumnSpacing(spaIdx);
    }

    Float64 X5 = pPier->GetXBeamOverhang(pgsTypes::stLeft);
    Float64 X6 = pPier->GetXBeamOverhang(pgsTypes::stRight);
    Float64 pierWidth = X5 + S + X6;

    // Get lower XBeam dimensions
    Float64 H1, H2, X1, X2;
    Float64 H3, H4, X3, X4;
    pPier->GetXBeamDimensions(pgsTypes::stLeft, &H1, &H2, &X1, &X2);
    pPier->GetXBeamDimensions(pgsTypes::stRight, &H3, &H4, &X3, &X4);

    // Get upper XBeam dimensions
    Float64 Hdiaph, Wdiaph;
    GetUpperXBeamDimensions(pPier, &Hdiaph, &Wdiaph);

    Float64 max_xbeam_height = max(H1 + H2, H3 + H4);

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

    // Calculate world bounding box with 10% margin
    Float64 world_width = pierWidth;
    Float64 world_height = max_column_height;

    Float64 margin_h = world_width * 0.10;
    Float64 margin_v = world_height * 0.10;

    Float64 left = -pierWidth / 2.0 - margin_h;
    Float64 right = pierWidth / 2.0 + margin_h;
    Float64 bottom = -Hdiaph - margin_v;
    Float64 top = world_height + margin_v;

    WBFL::Graphing::Rect box(left, bottom, right, top);
    WBFL::Graphing::Size size = box.Size();
    WBFL::Graphing::Point org = box.Center();

    mapper.SetMappingMode(WBFL::Graphing::PointMapper::MapMode::Isotropic);
    mapper.SetWorldExt(size);
    mapper.SetWorldOrg(org);
    mapper.SetDeviceExt(sDeviceClient.cx, -sDeviceClient.cy);  // Negate Y to flip axis
}

void CDrawPierLayoutControl::CalculateSideViewBoundingBox(const CPierData2* pPier,
    WBFL::Graphing::PointMapper& mapper, CSize sDeviceClient)
{
    // Get pier dimensions
    ColumnIndexType nColumns = pPier->GetColumnCount();

    Float64 W = pPier->GetXBeamWidth();

    // Get XBeam dimensions
    Float64 H1, H2, X1, X2;
    Float64 H3, H4, X3, X4;
    pPier->GetXBeamDimensions(pgsTypes::stLeft, &H1, &H2, &X1, &X2);
    pPier->GetXBeamDimensions(pgsTypes::stRight, &H3, &H4, &X3, &X4);

    Float64 max_xbeam_height = max(H1 + H2, H3 + H4);

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

    // Get upper XBeam dimensions
    Float64 Hdiaph, Wdiaph;
    GetUpperXBeamDimensions(pPier, &Hdiaph, &Wdiaph);

    // Calculate world bounding box with 10% margin
    Float64 world_width = W;
    Float64 world_height = max_column_height;

    Float64 margin_h = world_width * 0.10;
    Float64 margin_v = world_height * 0.10;

    Float64 left = -W / 2.0 - margin_h;
    Float64 right = W / 2.0 + margin_h;
    Float64 bottom = -Hdiaph - margin_v;
    Float64 top = world_height + margin_v;

    WBFL::Graphing::Rect box(left, bottom, right, top);
    WBFL::Graphing::Size size = box.Size();
    WBFL::Graphing::Point org = box.Center();

    mapper.SetMappingMode(WBFL::Graphing::PointMapper::MapMode::Isotropic);
    mapper.SetWorldExt(size);
    mapper.SetWorldOrg(org);
    mapper.SetDeviceExt(sDeviceClient.cx, -sDeviceClient.cy);
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
		const auto& colShape = pColumn->GetColumnShape();

        Float64 col_d1, col_d2;
        pColumn->GetColumnDimensions(&col_d1, &col_d2);

        Float64 col_height = 0.0;
        if (pColumn->GetColumnHeightMeasurementType() == CColumnData::chtHeight)
        {
            col_height = pColumn->GetColumnHeight();
        }

        // In side view, center the column at origin (x = 0)
        Float64 col_x = 0.0;

        Float64 side_width = (colShape == CColumnData::ColumnShapeType::cstCircle? side_width = col_d1 : col_d2);
        WBFL::Graphing::Point col_bl(col_x - side_width / 2.0, 0.0);
        WBFL::Graphing::Point col_br(col_x + side_width / 2.0, 0.0);
        WBFL::Graphing::Point col_tr(col_x + side_width / 2.0, col_height);
        WBFL::Graphing::Point col_tl(col_x - side_width / 2.0, col_height);

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

    // Draw upper cross beam
    CPen upper_xbeam_pen(PS_DOT, 1, SEGMENT_BORDER_COLOR);
    CBrush upper_xbeam_brush(HS_BDIAGONAL, BLACK);

    pDC->SelectObject(&upper_xbeam_pen);
    pDC->SelectObject(&upper_xbeam_brush);

    Float64 Hdiaph, Wdiaph;
    GetUpperXBeamDimensions(pPier, &Hdiaph, &Wdiaph);

    WBFL::Graphing::Point upper_xbeam_bl(-W / 2.0, -Hdiaph);
    WBFL::Graphing::Point upper_xbeam_br(W / 2.0, -Hdiaph);
    WBFL::Graphing::Point upper_xbeam_tr(W / 2.0, 0.0);
    WBFL::Graphing::Point upper_xbeam_tl(-W / 2.0, 0.0);

    mapper.WPtoDP(upper_xbeam_bl, &dx_bl, &dy_bl);
    mapper.WPtoDP(upper_xbeam_br, &dx_br, &dy_br);
    mapper.WPtoDP(upper_xbeam_tr, &dx_tr, &dy_tr);
    mapper.WPtoDP(upper_xbeam_tl, &dx_tl, &dy_tl);

    CPoint upper_xbeam_points[4] = { CPoint(dx_bl, dy_bl), CPoint(dx_br, dy_br),
                                CPoint(dx_tr, dy_tr), CPoint(dx_tl, dy_tl) };
    pDC->Polygon(upper_xbeam_points, 4);

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

    ColumnIndexType refColIdx;
    Float64 refColOffset;
    pgsTypes::OffsetMeasurementType refColOffsetMeasure;
    pPier->GetTransverseOffset(&refColIdx, &refColOffset, &refColOffsetMeasure);

    Float64 Sref = 0.0;
    if (refColIdx > 0 && refColIdx != INVALID_INDEX)
    {
        for (SpacingIndexType spaIdx = 0; spaIdx < refColIdx; spaIdx++)
        {
            Sref += pPier->GetColumnSpacing(spaIdx);
        }
    }

    Float64 refCol_x = -pierWidth / 2.0 + Sref + X5;

    WBFL::Graphing::Point xbeam_points[8];
    if (abs(refCol_x - refColOffset) <= pierWidth / 2.0)
    {
        xbeam_points[0] = WBFL::Graphing::Point(refCol_x - refColOffset, 0.02 * (refCol_x - refColOffset - pierWidth / 2.0));
    }
    else
    {
        xbeam_points[0] = WBFL::Graphing::Point(0.0, -0.02 * pierWidth / 2.0);
    }
    xbeam_points[1] = WBFL::Graphing::Point(-pierWidth / 2.0, 0.0);
    xbeam_points[2] = WBFL::Graphing::Point(-pierWidth / 2.0 + X2, H1);
    xbeam_points[3] = WBFL::Graphing::Point(-pierWidth / 2.0 + X2 + X1, H1 + H2);
    xbeam_points[4] = WBFL::Graphing::Point(pierWidth / 2.0 - X2 - X3, H3 + H4);
    xbeam_points[5] = WBFL::Graphing::Point(pierWidth / 2.0 - X2, H3);
    xbeam_points[6] = WBFL::Graphing::Point(pierWidth / 2.0, 0.0);

    CPoint dev_points[8];
    IndexType nPoints = 7;

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

    // Draw upper cross beam
    CPen upper_xbeam_pen(PS_DOT, 1, SEGMENT_BORDER_COLOR);
    CBrush upper_xbeam_brush(HS_BDIAGONAL, BLACK);

    pDC->SelectObject(&upper_xbeam_pen);
    pDC->SelectObject(&upper_xbeam_brush);

    Float64 Hdiaph, Wdiaph;
    GetUpperXBeamDimensions(pPier, &Hdiaph, &Wdiaph);

    auto pBroker = EAFGetBroker();
    GET_IFACE2(pBroker, IBridge, pBridge);



	if (refColIdx != INVALID_INDEX)
    {
        //Draw reference column offset line
        CPen ref_column_pen(PS_DASHDOT, 2, ALIGNMENT_COLOR);
        pDC->SelectObject(&ref_column_pen);

        const CColumnData* pRefColumn = &pPier->GetColumnData(refColIdx);

        WBFL::Graphing::Point pt_start(refCol_x - refColOffset, -0.5);
        WBFL::Graphing::Point pt_end(refCol_x - refColOffset, pRefColumn->GetColumnHeight() - 0.5);

        LONG dx_start, dy_start, dx_end, dy_end;
        mapper.WPtoDP(pt_start, &dx_start, &dy_start);
        mapper.WPtoDP(pt_end, &dx_end, &dy_end);

        pDC->MoveTo(dx_start, dy_start);
        pDC->LineTo(dx_end, dy_end);

        CPen dim_pen(PS_SOLID, 1, RGB(0, 0, 0));
        pOldPen = pDC->SelectObject(&dim_pen);

        CFont font;
        font.CreatePointFont(80, _T("Arial"));  // 8pt font
        CFont* pOldFont = pDC->SelectObject(&font);

        pDC->SetTextColor(RGB(0, 0, 0));
        pDC->SetBkMode(OPAQUE);
        int oldTA = pDC->SetTextAlign(TA_CENTER | TA_BOTTOM);

        CString str;
        str.Format((refColOffset == 0.0 ? _T("%s") : _T("%s offset")), (refColOffsetMeasure == pgsTypes::omtAlignment ? _T("alignment") : _T("bridgeline")));
        DrawHorizontalDimension(pDC, mapper, refCol_x - refColOffset, (H1 + H3) / 2.0 + pRefColumn->GetColumnHeight() / 2.0, refCol_x, str);
    }


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

    const CColumnData* pFirstColumn = &pPier->GetColumnData(0);

    // Get column height
    Float64 first_col_height = 0.0;
    if (pFirstColumn->GetColumnHeightMeasurementType() == CColumnData::chtHeight)
    {
        first_col_height = pFirstColumn->GetColumnHeight();
    }

    const CColumnData* pLastColumn = &pPier->GetColumnData(pPier->GetColumnCount()-1);

    // Get column height
    Float64 last_col_height = 0.0;
    if (pLastColumn->GetColumnHeightMeasurementType() == CColumnData::chtHeight)
    {
        last_col_height = pLastColumn->GetColumnHeight();
    }

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
        DrawHorizontalDimension(pDC, mapper, left_edge, first_col_height + DIM_OFFSET, left_edge + X5, _T("X5"));
    }

    // X6 dimension (right overhang)
    Float64 right_edge = pierWidth / 2.0;
	if (::IsGT(0.0, X6))
    {
        DrawHorizontalDimension(pDC, mapper, right_edge - X6, last_col_height + DIM_OFFSET, right_edge, _T("X6"));
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