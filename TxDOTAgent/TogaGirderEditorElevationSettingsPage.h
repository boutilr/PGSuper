///////////////////////////////////////////////////////////////////////
// PGSuper - Prestressed Girder SUPERstructure Design and Analysis
// Copyright � 1999-2013  Washington State Department of Transportation
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

#if !defined(AFX_TogaGirderEditorElevationSettingsPage_H__02627324_7328_11D2_9D8B_00609710E6CE__INCLUDED_)
#define AFX_TogaGirderEditorElevationSettingsPage_H__02627324_7328_11D2_9D8B_00609710E6CE__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// TogaGirderEditorElevationSettingsPage.h : header file
//
#include "resource.h"

/////////////////////////////////////////////////////////////////////////////
// CTogaGirderEditorElevationSettingsPage dialog

class CTogaGirderEditorElevationSettingsPage : public CPropertyPage
{
	DECLARE_DYNCREATE(CTogaGirderEditorElevationSettingsPage)

// Construction
public:
	CTogaGirderEditorElevationSettingsPage();
	~CTogaGirderEditorElevationSettingsPage();

// Dialog Data
	//{{AFX_DATA(CTogaGirderEditorElevationSettingsPage)
	enum { IDD = IDD_GIRDER_EDITOR_ELEVATION_PAGE };
	BOOL	m_ShowDimensions;
	BOOL	m_ShowPsCg;
	BOOL	m_ShowStirrups;
	BOOL	m_ShowStrands;
	BOOL	m_ShowSchematic;
	BOOL	m_ShowLongReinf;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CTogaGirderEditorElevationSettingsPage)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CTogaGirderEditorElevationSettingsPage)
	afx_msg void OnHelp();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_TogaGirderEditorElevationSettingsPage_H__02627324_7328_11D2_9D8B_00609710E6CE__INCLUDED_)
