#if !defined(AFX_SPECLIMITSPAGE_H__D150B0F9_D673_4BB0_B36F_9E6C2CD6909B__INCLUDED_)
#define AFX_SPECLIMITSPAGE_H__D150B0F9_D673_4BB0_B36F_9E6C2CD6909B__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// SpecLimitsPage.h : header file
//

#include "resource.h"

/////////////////////////////////////////////////////////////////////////////
// CSpecLimitsPage dialog

class CSpecLimitsPage : public CPropertyPage
{
	DECLARE_DYNCREATE(CSpecLimitsPage)

// Construction
public:
	CSpecLimitsPage();
	~CSpecLimitsPage();

// Dialog Data
	//{{AFX_DATA(CSpecLimitsPage)
	enum { IDD = IDD_SPEC_LIMITS };
		// NOTE - ClassWizard will add data members here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CSpecLimitsPage)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CSpecLimitsPage)
		// NOTE: the ClassWizard will add member functions here
	//}}AFX_MSG
   afx_msg LRESULT OnCommandHelp(WPARAM, LPARAM lParam);

	DECLARE_MESSAGE_MAP()

public:
   afx_msg void OnBnClickedCheckGirderSag();
   virtual BOOL OnInitDialog();
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_SPECLIMITSPAGE_H__D150B0F9_D673_4BB0_B36F_9E6C2CD6909B__INCLUDED_)
