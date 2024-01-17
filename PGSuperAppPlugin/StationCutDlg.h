///////////////////////////////////////////////////////////////////////
// PGSuper - Prestressed Girder SUPERstructure Design and Analysis
// Copyright � 1999-2024  Washington State Department of Transportation
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

#if !defined(AFX_STATIONCUTDLG_H__03282213_735B_11D2_9D8B_00609710E6CE__INCLUDED_)
#define AFX_STATIONCUTDLG_H__03282213_735B_11D2_9D8B_00609710E6CE__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// StationCutDlg.h : header file
//
#include "resource.h"

/////////////////////////////////////////////////////////////////////////////
// CStationCutDlg dialog

class CStationCutDlg : public CDialog
{
// Construction
public:
   CStationCutDlg(CWnd* pParent=nullptr);   // standard constructor

	CStationCutDlg(Float64 value, Float64 lowerBound, Float64 upperBound, bool bSIUnits, CWnd* pParent = nullptr);

   void SetValue(Float64 value);
   Float64 GetValue()const;
   void SetBounds(Float64 lowerBound, Float64 upperBound);
   void GetBounds(Float64* plowerBound, Float64* pupperBound);
   void IsSIUnits(bool bSI) {m_bSIUnits = bSI;}
   bool IsSIUnits() const {return m_bSIUnits;}

// Dialog Data
	//{{AFX_DATA(CStationCutDlg)
	enum { IDD = IDD_STATION_CUT_DIALOG };
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CStationCutDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX) override;    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CStationCutDlg)
	virtual BOOL OnInitDialog() override;
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()

private:
   bool      m_bSIUnits;
   Float64	 m_Value;
   Float64   m_LowerBound;
   Float64   m_UpperBound;
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STATIONCUTDLG_H__03282213_735B_11D2_9D8B_00609710E6CE__INCLUDED_)
