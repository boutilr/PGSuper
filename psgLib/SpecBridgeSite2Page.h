///////////////////////////////////////////////////////////////////////
// PGSuper - Prestressed Girder SUPERstructure Design and Analysis
// Copyright � 1999-2015  Washington State Department of Transportation
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

#if !defined(AFX_SPECBRIDGESITE2PAGE_H__EC05479B_1AE6_11D4_AEA0_00105A9AF985__INCLUDED_)
#define AFX_SPECBRIDGESITE2PAGE_H__EC05479B_1AE6_11D4_AEA0_00105A9AF985__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// SpecBridgeSite2Page.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CSpecBridgeSite2Page dialog

class CSpecBridgeSite2Page : public CPropertyPage
{
	DECLARE_DYNCREATE(CSpecBridgeSite2Page)

// Construction
public:
	CSpecBridgeSite2Page();
	~CSpecBridgeSite2Page();

// Dialog Data
	//{{AFX_DATA(CSpecBridgeSite2Page)
	enum { IDD = IDD_SPEC_BRIDGE_SITE2 };
	CSpinButtonCtrl	m_TrafficSpin;

	//}}AFX_DATA

   afx_msg LRESULT OnCommandHelp(WPARAM, LPARAM lParam);

// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CSpecBridgeSite2Page)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CSpecBridgeSite2Page)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

public:
   afx_msg void OnBnClickedCheckTension();
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_SPECBRIDGESITE2PAGE_H__EC05479B_1AE6_11D4_AEA0_00105A9AF985__INCLUDED_)
