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

#include <Graphing/PointMapper.h>
#include <PsgLib\PierData2.h>
#include <PsgLib\Keys.h>

interface IPierLayoutDataSource
{
public:
   virtual const CPierData2 * GetPierData() const = 0;
   virtual pgsTypes::PierModelType GetPierModelType() const = 0;
};

class CDrawPierLayoutControl : public CWnd
{
	DECLARE_DYNAMIC(CDrawPierLayoutControl)

public:
	CDrawPierLayoutControl();
	virtual ~CDrawPierLayoutControl();

	void CustomInit(IPierLayoutDataSource* pSource);

protected:
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);

	DECLARE_MESSAGE_MAP()

private:
	IPierLayoutDataSource* m_pSource;

	void CalculateFrontViewBoundingBox(const CPierData2* pPier,
		WBFL::Graphing::PointMapper& mapper, CSize sDeviceClient);
	void CalculateSideViewBoundingBox(const CPierData2* pPier,
		WBFL::Graphing::PointMapper& mapper, CSize sDeviceClient);
	void DrawPierGeometry(CDC* pDC, WBFL::Graphing::PointMapper& mapper);
	void DrawSideView(CDC* pDC, WBFL::Graphing::PointMapper& mapper);
	void DrawSymbolicDimensions(CDC* pDC, WBFL::Graphing::PointMapper& mapper,
		Float64 H1, Float64 H2, Float64 X1, Float64 X2,
		Float64 H3, Float64 H4, Float64 X3, Float64 X4);
	void DrawHorizontalDimension(CDC* pDC, WBFL::Graphing::PointMapper& mapper,
		Float64 x1, Float64 y, Float64 x2, LPCTSTR pszLabel);
	void DrawVerticalDimension(CDC* pDC, WBFL::Graphing::PointMapper& mapper,
		Float64 x, Float64 y1, Float64 y2, LPCTSTR pszLabel);
};