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
#pragma once
#include "XBeamCutLocation.h"
#include <Graphing/PointMapper.h>
#include <PsgLib\PierData2.h>
#include <PsgLib\Keys.h>
#include <DManip/DManip.h>

interface IPierLayoutDataSource
{
public:
   virtual const CPierData2 * GetPierData() const = 0;
   virtual pgsTypes::PierModelType GetPierModelType() const = 0;
};

class CDrawPierLayoutControl : public CDisplayWnd
{
	DECLARE_DYNAMIC(CDrawPierLayoutControl)

public:
	CDrawPierLayoutControl();
	virtual ~CDrawPierLayoutControl();

	void OnDraw(CDC* pDC) override;

	void CustomInit(IPierLayoutDataSource* pSource);

	void ResetExtents();

	BOOL CreatePopout(IPierLayoutDataSource* pSource, CWnd* pOwner);

	afx_msg void OnSize(UINT nType, int cx, int cy);

protected:

	IDType m_DisplayObjectID; // used to generate display object IDs

	void UpdateDisplayObjects();
	void UpdateRoadwayDisplayObjects();
	void UpdateXBeamDisplayObjects();
	void UpdateColumnDisplayObjects();
	void UpdateSectionCutDisplayObjects();

	std::shared_ptr<WBFL::DManip::iLineDisplayObject> CreateLineDisplayObject(const WBFL::Geometry::Point2d& pntStart, const WBFL::Geometry::Point2d& pntEnd);


	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);

	DECLARE_MESSAGE_MAP()

private:
	IPierLayoutDataSource* m_pSource;

	CXBeamCutLocation* m_pCutLoc;

	// Zoom state - store initial bounds for reset
	WBFL::Graphing::Point m_initialOrgFront;
	WBFL::Graphing::Size m_initialExtFront;
	WBFL::Graphing::Point m_currentOrgFront;
	WBFL::Graphing::Size m_currentExtFront;

	WBFL::Graphing::Point m_initialOrgSide;
	WBFL::Graphing::Size m_initialExtSide;
	WBFL::Graphing::Point m_currentOrgSide;
	WBFL::Graphing::Size m_currentExtSide;

	BOOL m_bInitialized;
	BOOL m_bDragging;
	CPoint m_dragStart;
	CPoint m_dragEnd;

	void DrawPierGeometry(CDC* pDC, WBFL::Graphing::PointMapper& mapper);
	void DrawSymbolicDimensions(CDC* pDC, WBFL::Graphing::PointMapper& mapper,
		Float64 H1, Float64 H2, Float64 X1, Float64 X2,
		Float64 H3, Float64 H4, Float64 X3, Float64 X4);
	void DrawHorizontalDimension(CDC* pDC, WBFL::Graphing::PointMapper& mapper,
		Float64 x1, Float64 y, Float64 x2, LPCTSTR pszLabel);
	void DrawVerticalDimension(CDC* pDC, WBFL::Graphing::PointMapper& mapper,
		Float64 x, Float64 y1, Float64 y2, LPCTSTR pszLabel);
};
