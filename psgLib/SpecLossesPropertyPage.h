///////////////////////////////////////////////////////////////////////
// PGSuper - Prestressed Girder SUPERstructure Design and Analysis
// Copyright � 1999-2020  Washington State Department of Transportation
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

// SpecLossesPropertyPage.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CSpecLossesPropertyPage dialog

class CSpecLossesPropertyPage : public CMFCPropertyPage
{
	DECLARE_DYNCREATE(CSpecLossesPropertyPage)

// Construction
public:
   CSpecLossesPropertyPage();
	~CSpecLossesPropertyPage();


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CSpecLossesPropertyPage)
	public:
	virtual BOOL OnSetActive();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CSpecLossesPropertyPage)
	virtual BOOL OnInitDialog();
   afx_msg void OnHelp();
   afx_msg void OnLossMethodChanged();
	afx_msg void OnShippingLossMethodChanged();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

   lrfdVersionMgr::Version m_SpecVersion;
   bool m_IsShippingEnabled;
   void EnableShippingLosses(BOOL bEnable);
   void EnableRefinedShippingTime(BOOL bEnable);
   void EnableApproximateShippingTime(BOOL bEnable);
   void EnableRelaxation(BOOL bEnable);
   void EnableElasticGains(BOOL bEnable, BOOL bEnableDeckShrinkage);
   void EnableTimeDependentModel(BOOL bEnable);
   void EnableTxDOT2013(BOOL bEnable);
   BOOL IsFractionalShippingLoss();
};
