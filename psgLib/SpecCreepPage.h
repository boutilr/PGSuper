///////////////////////////////////////////////////////////////////////
// PGSuper - Prestressed Girder SUPERstructure Design and Analysis
// Copyright � 1999-2023  Washington State Department of Transportation
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

#if !defined(AFX_SPECCREEPPAGE_H__8D6A22E6_87A5_11D2_887F_006097C68A9C__INCLUDED_)
#define AFX_SPECCREEPPAGE_H__8D6A22E6_87A5_11D2_887F_006097C68A9C__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// SpecCreepPage.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CSpecCreepPage dialog

class CSpecCreepPage : public CPropertyPage
{
	DECLARE_DYNCREATE(CSpecCreepPage)

// Construction
public:
	CSpecCreepPage();
	~CSpecCreepPage();

// Dialog Data
	//{{AFX_DATA(CSpecCreepPage)
	enum { IDD = IDD_SPEC_CREEP };
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CSpecCreepPage)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CSpecCreepPage)
	afx_msg void OnWsdotCreepMethod();
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
   afx_msg void OnHelp();
   afx_msg void OnCbnSelchangeHaunchCompCb();
   afx_msg void OnCbnSelchangeHaunchCompPropCb();

   void OnChangeHaunch();

	DECLARE_MESSAGE_MAP()

};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_SPECCREEPPAGE_H__8D6A22E6_87A5_11D2_887F_006097C68A9C__INCLUDED_)
