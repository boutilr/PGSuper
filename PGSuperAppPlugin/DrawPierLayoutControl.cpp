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
    m_DisplayObjectID = 0;
}

CDrawPierLayoutControl::~CDrawPierLayoutControl()
{
}

BEGIN_MESSAGE_MAP(CDrawPierLayoutControl, CDisplayWnd)
    ON_WM_PAINT()
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
        pBridge->GetXBeamProfile(*pPier, pgsTypes::Stage::Stage2, &upperXBeamShape);

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
    pBridge->GetXBeamProfile(*pPier, pgsTypes::Stage::Stage1, &pLowerXBeamShape);

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
    Lxb = pBridge->ConvertCrossBeamToPierCoordinate(*pPier, Lxb);

    Float64 XxbCut = pBridge->ConvertPierToCrossBeamCoordinate(*pPier, m_pCutLoc->GetCurrentCutLocation());

    auto doXBeamSection = WBFL::DManip::PointDisplayObject::Create(m_DisplayObjectID++);
    doXBeamSection->SetPosition(point, false, false);
    doXBeamSection->SetSelectionType(g_selectionType);

    CComPtr<IShape> xbeamShape;
    pBridge->GetXBeamShape(*pPier, pgsTypes::Stage2, XxbCut, &xbeamShape);
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

    pBridge->GetPierBottomSurface(*pPier, &points); 

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
        const CColumnData& columnData = pPier->GetColumnData(colIdx);
		Float64 XxbCol = pBridge->GetColumnLocation(*pPier, colIdx);
        Float64 XpCol = pBridge->ConvertCrossBeamToPierCoordinate(*pPier, XxbCol);
        Float64 Ytop = pBridge->GetTopColumnElevation(*pPier, colIdx);
        Float64 Ybot = Ytop - columnData.GetColumnHeight();

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

void CDrawPierLayoutControl::OnPaint()
{

	CDisplayWnd::OnPaint();

}
