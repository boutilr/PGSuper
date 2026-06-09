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

#include "XBeamDisplayObjectFactory.h"

#include <WBFLGenericBridge.h>

#include "XBeamSectionCut.h"

#include <WBFLGeometry/GeomHelpers.h>

#include <EAF\EAFDisplayUnits.h>

#include <IFace/Tools.h>
#include <IFace\Bridge.h>
#include <IFace\Project.h>
#include <PsgLib\PierData2.h>
#include <PsgLib\BridgeDescription2.h>

#define XBEAM_LINE_COLOR               GREY50
#define XBEAM_FILL_COLOR               GREY70

#define ROADWAY_DISPLAY_LIST_ID        0
#define CROSSBEAM_DISPLAY_LIST_ID      1
#define COLUMN_DISPLAY_LIST_ID         2
#define SECTION_CUT_DISPLAY_LIST_ID    3

#define SECTION_CUT_ID                500

// The End/Section view of the pier is offset from the Elevation
// view by this amount.
const Float64 EndOffset = WBFL::Units::ConvertToSysUnits(10, WBFL::Units::Measure::Feet);

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
    ON_WM_SIZE()
END_MESSAGE_MAP()

void CDrawPierLayoutControl::OnDraw(CDC* pDC)
{
    CDisplayWnd::OnDraw(pDC);
}

void CDrawPierLayoutControl::CustomInit(IPierLayoutDataSource* pSource)
{
    CDisplayWnd::CustomInit();

    m_pSource = pSource;
    const CPierData2* pPier = m_pSource->GetPierData();

    auto doFactory = std::make_shared<CXBeamDisplayObjectFactory>();
    m_pDispMgr->AddDisplayObjectFactory(doFactory);

    m_pDispMgr->CreateDisplayList(ROADWAY_DISPLAY_LIST_ID);
    m_pDispMgr->CreateDisplayList(CROSSBEAM_DISPLAY_LIST_ID);
    m_pDispMgr->CreateDisplayList(COLUMN_DISPLAY_LIST_ID);
    m_pDispMgr->CreateDisplayList(SECTION_CUT_DISPLAY_LIST_ID);

    Float64 xMin = -pPier->GetXBeamLength() / 2.0;
    Float64 xLoc = 1.0;
    Float64 xMax = pPier->GetXBeamLength() / 2.0;
    m_pCutLoc = new CXBeamCutLocation(0,xLoc, xMax);

    SetMappingMode(WBFL::DManip::MapMode::Isotropic, false);
    CDManipClientDC dc2(this);


	UpdateDisplayObjects();
    ScaleToFit();
    
}


void CDrawPierLayoutControl::UpdateDisplayObjects()
{
    CWaitCursor wait;


    // Capture the current selection before blasting all the 
    // display objects
    auto vCurSel = m_pDispMgr->GetSelectedObjects();
    ATLASSERT(vCurSel.size() < 2);
    IDType curSel = INVALID_ID;
    IDType listID = INVALID_ID;
    if (vCurSel.size() == 1)
    {
        auto pDO = vCurSel[0];
        curSel = pDO->GetID();
        auto pDL = pDO->GetDisplayList();
        listID = pDL->GetID();
    }

    m_pDispMgr->ClearDisplayObjects();
    m_DisplayObjectID = 0;

    UpdateRoadwayDisplayObjects();
    UpdateXBeamDisplayObjects();
    UpdateColumnDisplayObjects();
    //UpdateSectionCutDisplayObjects();

    // Re-instate the current selection
    if (curSel != INVALID_ID)
    {
        auto doSel = m_pDispMgr->FindDisplayObject(curSel, listID, WBFL::DManip::AccessType::ByID);
        m_pDispMgr->SelectObject(doSel, TRUE);
    }
}



void CDrawPierLayoutControl::OnLButtonDown(UINT nFlags, CPoint point)
{
    m_bDragging = TRUE;
    m_dragStart = point;
    m_dragEnd = point;
    SetCapture();
}

std::shared_ptr<WBFL::DManip::iLineDisplayObject> CDrawPierLayoutControl::CreateLineDisplayObject(const WBFL::Geometry::Point2d& pntStart, const WBFL::Geometry::Point2d& pntEnd)
{
    auto doPntStart = WBFL::DManip::PointDisplayObject::Create();
    doPntStart->Visible(false);
    doPntStart->SetPosition(pntStart, false, false);
    auto connectable1 = std::dynamic_pointer_cast<WBFL::DManip::iConnectable>(doPntStart);
    auto socket1 = connectable1->AddSocket(0, pntStart);

    auto doPntEnd = WBFL::DManip::PointDisplayObject::Create();
    doPntEnd->Visible(false);
    doPntEnd->SetPosition(pntEnd, false, false);
    auto connectable2 = std::dynamic_pointer_cast<WBFL::DManip::iConnectable>(doPntEnd);
    auto socket2 = connectable2->AddSocket(0, pntEnd);

    auto doLine = WBFL::DManip::LineDisplayObject::Create();

    auto connector = std::dynamic_pointer_cast<WBFL::DManip::iConnector>(doLine);
    auto startPlug = connector->GetStartPlug();
    auto endPlug = connector->GetEndPlug();

    connectable1->Connect(0, WBFL::DManip::AccessType::ByID, startPlug);
    connectable2->Connect(0, WBFL::DManip::AccessType::ByID, endPlug);

    return doLine;
}

void CDrawPierLayoutControl::UpdateRoadwayDisplayObjects()
{
    const CPierData2* pPier = m_pSource->GetPierData();

    auto displayList = m_pDispMgr->FindDisplayList(ROADWAY_DISPLAY_LIST_ID);

    auto pBroker = EAFGetBroker();

	auto pierIdx = pPier->GetIndex();

    GET_IFACE2(pBroker, IBridge, pBridge);

    CComPtr<IAngle> angle;
	pBridge->GetPierSkew(pierIdx, &angle);
    Float64 skew;
	angle->get_Value(&skew);

    Float64 cos_skew = cos(skew);

    // Model a vertical line for the alignment
    // The alignment is at X = 0 in Pier coordinates
    Float64 X = 0;

    Float64 Xcl = pBridge->ConvertPierToCurbLineCoordinate(pierIdx, X);

    Float64 Ydeck = pBridge->GetElevation(pierIdx, Xcl); // deck elevation at alignment

    Float64 Yt = Ydeck + WBFL::Units::ConvertToSysUnits(1.0, WBFL::Units::Measure::Feet); // add a little so it projects over the roadway surface
    WBFL::Geometry::Point2d pnt1(X, Yt);

    Float64 Yb = Yt - pBridge->GetMaxColumnHeight(pierIdx);
    WBFL::Geometry::Point2d pnt2(X, Yb);

    auto doAlignment = CreateLineDisplayObject(pnt1, pnt2);
    auto drawStrategy = doAlignment->GetDrawLineStrategy();
    auto drawAlignmentStrategy = std::dynamic_pointer_cast<WBFL::DManip::SimpleDrawLineStrategy>(drawStrategy);
    drawAlignmentStrategy->SetWidth(ALIGNMENT_LINE_WEIGHT);
    drawAlignmentStrategy->SetColor(ALIGNMENT_COLOR);
    drawAlignmentStrategy->SetLineStyle(WBFL::DManip::LineStyleType::Centerline);

    // Don't add the object to the display list here... do it at the end
    // We want it to be drawn on top so it has to go into the display list last
    //displayList->AddDisplayObject(doAlignment);

    // Draw the bridge line if different then the alignment
    std::shared_ptr<WBFL::DManip::iLineDisplayObject> doBridgeLine;
    Float64 BLO = pBridge->GetAlignmentOffset();
    if (!IsZero(BLO))
    {
        // Model a vertical line for the bridge line
        // Let X = BLO be at the alignment and Y = the alignment elevation
        Float64 X = BLO / cos_skew;
        pnt1.Move(X, Yt);
        pnt2.Move(X, Yb);

        doBridgeLine = CreateLineDisplayObject(pnt1, pnt2);
        auto drawStrategy = doBridgeLine->GetDrawLineStrategy();
        auto drawBridgeLineStrategy = std::dynamic_pointer_cast<WBFL::DManip::SimpleDrawLineStrategy>(drawStrategy);
        drawBridgeLineStrategy->SetWidth(BRIDGELINE_LINE_WEIGHT);
        drawBridgeLineStrategy->SetColor(BRIDGE_COLOR);
        drawBridgeLineStrategy->SetLineStyle(WBFL::DManip::LineStyleType::Centerline);

        //displayList->AddDisplayObject(doBridgeLine); // do this at the end
    }

    // Draw Roadway Surface

    Float64 pierStation = pPier->GetStation();

    GET_IFACE2(pBroker, IShapes, pShapes);

    CComPtr<IDirection> pierDirection;
    pBridge->GetPierDirection(pierIdx, &pierDirection);

    GET_IFACE2(pBroker, IBridgeDescription, pIBridgeDesc);
    const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
    const CDeckDescription2* pDeck = pBridgeDesc->GetDeckDescription();

    pgsTypes::SupportedDeckType deckType = pDeck->GetDeckType();
    if (deckType != pgsTypes::sdtNone)
    {
        auto dispObj = WBFL::DManip::PointDisplayObject::Create(m_DisplayObjectID++);

        CComPtr<IShape> shape;
        pShapes->GetSlabShape(pierStation, pierDirection, true/*include haunch*/, &shape);

        auto strategy = WBFL::DManip::ShapeDrawStrategy::Create();

        strategy->SetShape(geomUtil::ConvertShape(shape));
        strategy->SetSolidLineColor(IsStructuralDeck(deckType) ? DECK_BORDER_COLOR : NONSTRUCTURAL_DECK_BORDER_COLOR);
        strategy->SetSolidFillColor(IsStructuralDeck(deckType) ? DECK_FILL_COLOR : NONSTRUCTURAL_DECK_FILL_COLOR);
        strategy->SetVoidLineColor(VOID_BORDER_COLOR);
        strategy->SetVoidFillColor(GetSysColor(COLOR_WINDOW));
        strategy->Fill(true);

        dispObj->SetDrawingStrategy(strategy);

        auto gravity_well = WBFL::DManip::ShapeGravityWellStrategy::Create();
        gravity_well->SetShape(geomUtil::ConvertShape(shape));

        dispObj->SetGravityWellStrategy(gravity_well);

        dispObj->SetSelectionType(g_selectionType);

        displayList->AddDisplayObject(dispObj);
        

        // Left Hand Barrier
        auto left_dispObj = WBFL::DManip::PointDisplayObject::Create();

        Float64 left_curb_offset = pBridge->GetLeftCurbOffset(pierIdx);
        Float64 right_curb_offset = pBridge->GetRightCurbOffset(pierIdx);

        CComPtr<IShape> left_shape;
        pShapes->GetLeftTrafficBarrierShape(pierStation, pierDirection, &left_shape);

        if (left_shape)
        {
            auto strategy = WBFL::DManip::ShapeDrawStrategy::Create();
            strategy->SetShape(geomUtil::ConvertShape(left_shape));
            strategy->SetSolidLineColor(BARRIER_BORDER_COLOR);
            strategy->SetSolidFillColor(BARRIER_FILL_COLOR);
            strategy->SetVoidLineColor(VOID_BORDER_COLOR);
            strategy->SetVoidFillColor(GetSysColor(COLOR_WINDOW));
            strategy->Fill(true);
            strategy->HasBoundingShape(false);

            left_dispObj->SetDrawingStrategy(strategy);

            displayList->AddDisplayObject(left_dispObj);
        }

        // Right Hand Barrier
        auto right_dispObj = WBFL::DManip::PointDisplayObject::Create();

        CComPtr<IShape> right_shape;
        pShapes->GetRightTrafficBarrierShape(pierStation, pierDirection, &right_shape);

        if (right_shape)
        {
            auto strategy = WBFL::DManip::ShapeDrawStrategy::Create();
            strategy->SetShape(geomUtil::ConvertShape(right_shape));
            strategy->SetSolidLineColor(BARRIER_BORDER_COLOR);
            strategy->SetSolidFillColor(BARRIER_FILL_COLOR);
            strategy->SetVoidLineColor(VOID_BORDER_COLOR);
            strategy->SetVoidFillColor(GetSysColor(COLOR_WINDOW));
            strategy->Fill(true);
            strategy->HasBoundingShape(false);

            right_dispObj->SetDrawingStrategy(strategy);

            displayList->AddDisplayObject(right_dispObj);
        }
    }

    displayList->AddDisplayObject(doAlignment);
    if (doBridgeLine)
    {
        displayList->AddDisplayObject(doBridgeLine);
    }
}

void CDrawPierLayoutControl::UpdateXBeamDisplayObjects()
{

    const CPierData2* pPier = m_pSource->GetPierData();
    PierIndexType pierIdx = pPier->GetIndex();

    auto displayList = m_pDispMgr->FindDisplayList(CROSSBEAM_DISPLAY_LIST_ID);

    auto pBroker = EAFGetBroker();

    GET_IFACE2(pBroker, IBridge, pBridge);

    // Model Upper Cross Beam (Elevation)
    WBFL::Geometry::Point2d point(0, 0);

    if (pBridge->GetPierType(pierIdx) != ptExpansion)
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

    // Section Cut
    Float64 Lxb = pPier->GetXBeamLength();
    Lxb = pBridge->ConvertCrossBeamToPierCoordinate(pierIdx, Lxb);

    Float64 XxbCut = pBridge->ConvertPierToCrossBeamCoordinate(pierIdx, m_pCutLoc->GetCurrentCutLocation());

    auto doXBeamSection = WBFL::DManip::PointDisplayObject::Create(m_DisplayObjectID++);
    doXBeamSection->SetPosition(point, false, false);
    doXBeamSection->SetSelectionType(g_selectionType);

    CComPtr<IShape> xbeamShape;
    pBridge->GetXBeamShape(pierIdx, pgsTypes::Stage2, XxbCut, &xbeamShape);
    CComQIPtr<IXYPosition> position(xbeamShape);
    position->Offset(EndOffset + Lxb, 0);

    auto xbeamDrawStrategy = WBFL::DManip::ShapeDrawStrategy::Create();
    xbeamDrawStrategy->SetShape(geomUtil::ConvertShape(xbeamShape));
    xbeamDrawStrategy->SetSolidLineColor(XBEAM_LINE_COLOR);
    xbeamDrawStrategy->SetSolidFillColor(XBEAM_FILL_COLOR);
    xbeamDrawStrategy->Fill(true);

    doXBeamSection->SetDrawingStrategy(xbeamDrawStrategy);

    auto xbeam_section_gravity_well = WBFL::DManip::ShapeGravityWellStrategy::Create();
    xbeam_section_gravity_well->SetShape(geomUtil::ConvertShape(xbeamShape));
    doXBeamSection->SetGravityWellStrategy(xbeam_section_gravity_well);

    displayList->AddDisplayObject(doXBeamSection);

    GET_IFACE2(pBroker, IEAFDisplayUnits, pDisplayUnits);
    CString strSectionCutLabel;
    strSectionCutLabel.Format(_T("Section @ %s"), ::FormatDimension(XxbCut, pDisplayUnits->GetSpanLengthUnit()));

    CComPtr<IPoint2d> pntBC;
    position->get_LocatorPoint(lpBottomCenter, &pntBC);
    pntBC->Offset(0, -WBFL::Units::ConvertToSysUnits(3.0, WBFL::Units::Measure::Feet));

    auto doLabel = WBFL::DManip::TextBlock::Create();
    doLabel->SetText(strSectionCutLabel);
    doLabel->SetBkMode(TRANSPARENT);
    doLabel->SetTextAlign(TA_TOP | TA_CENTER);
    doLabel->SetPosition(geomUtil::GetPoint(pntBC));
    //displayList->AddDisplayObject(doLabel);
}

void CDrawPierLayoutControl::UpdateColumnDisplayObjects()
{
    auto displayList = m_pDispMgr->FindDisplayList(COLUMN_DISPLAY_LIST_ID);

    auto pBroker = EAFGetBroker();

   // Create a function that represents the bottom of the cross beam
   // We will use it to make the top of the column match the bottom of the
   // cross beam.
    WBFL::Math::PiecewiseFunction fn;
    CComPtr<IPoint2dCollection> points;
    GET_IFACE2(pBroker, IBridge, pBridge);
    const CPierData2* pPier = m_pSource->GetPierData();
    PierIndexType pierIdx = pPier->GetIndex();
    pBridge->GetBottomSurface(pierIdx, pgsTypes::Stage1, &points); // This is a problem I will have to fix for hammerhead piers since the ref column is not at center of pier. 

    CComPtr<IEnumPoint2d> enumPoints;
    points->get__Enum(&enumPoints);
    CComPtr<IPoint2d> pnt;
    while (enumPoints->Next(1, &pnt, nullptr) != S_FALSE)
    {
        Float64 x, y;
        pnt->Location(&x, &y);
        fn.AddPoint(x, y);
        pnt.Release();
    }

    IndexType nColumns = pPier->GetColumnCount();
    for (IndexType colIdx = 0; colIdx < nColumns; colIdx++)
    {
        const CColumnData& columnData = pPier->GetColumnData(colIdx); /////below should be in column data:
        Float64 XxbCol = pBridge->GetColumnLocation(pierIdx, colIdx);
        Float64 XpCol = pBridge->ConvertCrossBeamToPierCoordinate(pierIdx, XxbCol);
        Float64 Ytop = pBridge->GetTopColumnElevation(pierIdx, colIdx);  // same thing as column height
        Float64 Ybot = pBridge->GetBottomColumnElevation(pierIdx, colIdx);
        CColumnData::ColumnShapeType colShapeType = columnData.GetColumnShape();
        Float64 d1, d2;
        columnData.GetColumnDimensions(&d1, &d2);
        CColumnData::ColumnHeightMeasurementType columnHeightType = columnData.GetColumnHeightMeasurementType();
        Float64 H = columnData.GetColumnHeight();

        WBFL::Geometry::Point2d pntTop(XpCol, Ytop);
        WBFL::Geometry::Point2d pntBot(XpCol, Ybot);

        auto doTop = WBFL::DManip::PointDisplayObject::Create();
        doTop->Visible(false);
        doTop->SetPosition(pntTop, false, false);
        auto connectable1 = std::dynamic_pointer_cast<WBFL::DManip::iConnectable>(doTop);
        auto socket1 = connectable1->AddSocket(0, pntTop);

        auto doBot = WBFL::DManip::PointDisplayObject::Create();
        doBot->Visible(false);
        doBot->SetPosition(pntBot, false, false);
        auto connectable2 = std::dynamic_pointer_cast<WBFL::DManip::iConnectable>(doBot);
        auto socket2 = connectable2->AddSocket(0, pntBot);

        // Create the shape of the column
        auto columnShape = std::make_shared<WBFL::Geometry::Polygon>();
        Float64 X1, X2, X3;
        X2 = pntTop.X();
        X1 = X2 - d1 / 2;
        X3 = X2 + d1 / 2;
        Float64 Y1 = fn.Evaluate(X1);
        Float64 Y2 = fn.Evaluate(X2);
        Float64 Y3 = fn.Evaluate(X3);

        columnShape->AddPoint(X1, Y1);
        columnShape->AddPoint(X2, Y2);
        columnShape->AddPoint(X3, Y3);
        columnShape->AddPoint(X3, Ybot);
        columnShape->AddPoint(X1, Ybot);

        auto doColumn = WBFL::DManip::PointDisplayObject::Create(m_DisplayObjectID++);
        doColumn->SetPosition(pntTop, false, false);
        doColumn->SetSelectionType(g_selectionType);

        auto drawColumnStrategy = WBFL::DManip::ShapeDrawStrategy::Create();
        doColumn->SetDrawingStrategy(drawColumnStrategy);

        drawColumnStrategy->SetShape(columnShape);
        drawColumnStrategy->SetSolidLineColor(XBEAM_LINE_COLOR);
        drawColumnStrategy->SetSolidFillColor(XBEAM_LINE_COLOR);
        drawColumnStrategy->Fill(true);

        displayList->AddDisplayObject(doColumn);
    }
}

void CDrawPierLayoutControl::UpdateSectionCutDisplayObjects()
{
    auto pBroker = EAFGetBroker();

    auto display_list = m_pDispMgr->FindDisplayList(SECTION_CUT_DISPLAY_LIST_ID);

    auto factory = m_pDispMgr->GetDisplayObjectFactory(0);

    auto disp_obj = factory->Create(CXBeamSectionCutDisplayImpl::ms_Format, nullptr);

    auto sink = disp_obj->GetEventSink();

    disp_obj->SetSelectionType(WBFL::DManip::SelectionType::All);

    auto point_disp = std::dynamic_pointer_cast<WBFL::DManip::iPointDisplayObject>(disp_obj);
    point_disp->SetMaxTipWidth(TOOLTIP_WIDTH);
    point_disp->SetToolTipText(_T("Drag me to move section cut.\r\nDouble click to enter the cut location\r\nPress CTRL + -> to move ahead\r\nPress CTRL + <- to move back"));
    point_disp->SetTipDisplayTime(TOOLTIP_DURATION);

    auto section_cut_strategy = std::dynamic_pointer_cast<iXBeamSectionCutDrawStrategy>(sink);
    const CPierData2* pPier = m_pSource->GetPierData();
    PierIndexType pierIdx = pPier->GetIndex();
    section_cut_strategy->Init(pierIdx, point_disp, m_pCutLoc);
    section_cut_strategy->SetColor(CUT_COLOR);

    point_disp->SetID(SECTION_CUT_ID);

    display_list->Clear();
    display_list->AddDisplayObject(disp_obj);
}

BOOL CDrawPierLayoutControl::CreatePopout(IPierLayoutDataSource* pSource, CWnd* pOwner)
{
    CString className = AfxRegisterWndClass(
        CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS,
        ::LoadCursor(nullptr, IDC_ARROW),
        (HBRUSH)(COLOR_WINDOW + 1),
        nullptr);

    // Use WS_EX_APPWINDOW so this is treated as a normal app window (not a tool window).
    DWORD dwExStyle = WS_EX_APPWINDOW;
    // Use WS_OVERLAPPEDWINDOW (or WS_POPUPWINDOW|WS_CAPTION|WS_SYSMENU) for a regular top-level window
    DWORD dwStyle = WS_OVERLAPPEDWINDOW/* | WS_VISIBLE*/;

    HWND hOwner = (pOwner != nullptr) ? pOwner->GetSafeHwnd() : nullptr;

    BOOL ok = CreateEx(
        dwExStyle,
        className,
        _T("Pier Layout"),
        dwStyle,
        CRect(100, 100, 1000, 800),
        CWnd::FromHandle(hOwner), // supply the owner window if provided
        0);

    if (!ok)
        return FALSE;

    // Initialize control after creation
    CustomInit(pSource);

    // Optionally keep the popout above the owner without making it topmost for all apps:
    // if (hOwner) SetWindowPos(hOwner, 0,0,0,0, SWP_NOMOVE|SWP_NOSIZE|SWP_SHOWWINDOW);

    ShowWindow(SW_SHOW);
    UpdateWindow();

    return TRUE;
}


// Then implement it in the .cpp
void CDrawPierLayoutControl::OnSize(UINT nType, int cx, int cy)
{
    CDisplayWnd::OnSize(nType, cx, cy);

    CRect rect;
    GetClientRect(&rect);
    rect.DeflateRect(1, 1, 1, 1);  // Small margin

    // This sets up the logical viewport coordinate system
    SetLogicalViewRect(MM_TEXT, rect);

    ScaleToFit(false);  // Scale to fit display objects
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

	CDisplayWnd::OnPaint();

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

    auto pBroker = EAFGetBroker();
    GET_IFACE2(pBroker, IBridge, pBridge);

    Float64 Hdiaph, Wdiaph;
    pBridge->GetUpperXBeamDimensions(pPier->GetIndex(), &Hdiaph, &Wdiaph);


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