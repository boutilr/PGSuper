
#if !defined(AFX_SPECHAULINGERECTIONPAGE_H__22F9FD45_4F00_11D2_9D5F_00609710E6CE__INCLUDED_)
#define AFX_SPECHAULINGERECTIONPAGE_H__22F9FD45_4F00_11D2_9D5F_00609710E6CE__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// SpecHaulingErectionPage.h : header file
//
#include "resource.h"
#include "WsdotHaulingDlg.h"
#include "KdotHaulingDlg.h"

/////////////////////////////////////////////////////////////////////////////
// CSpecHaulingErectionPage dialog

class CSpecHaulingErectionPage : public CPropertyPage
{
	DECLARE_DYNCREATE(CSpecHaulingErectionPage)

// Construction
public:
	CSpecHaulingErectionPage();
	~CSpecHaulingErectionPage();

// Dialog Data
	//{{AFX_DATA(CSpecHaulingErectionPage)
	enum { IDD = IDD_SPEC_HAULING_ERECTIOND };
		// NOTE - ClassWizard will add data members here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CSpecHaulingErectionPage)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
public:
   void HideControls(bool hide);

public:
   bool m_IsHaulingEnabled;
   pgsTypes::HaulingAnalysisMethod m_HaulingAnalysisMethod;

private:
   // Embedded dialogs
   CWsdotHaulingDlg m_WsdotHaulingDlg;
   CKdotHaulingDlg  m_KdotHaulingDlg;

   bool m_BeforeInit; // true 'til after oninitdialog

protected:
	// Generated message map functions
	//{{AFX_MSG(CSpecHaulingErectionPage)
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	//}}AFX_MSG
   afx_msg LRESULT OnCommandHelp(WPARAM, LPARAM lParam);
	DECLARE_MESSAGE_MAP()

   virtual BOOL OnInitDialog();
   virtual BOOL OnSetActive();
   virtual BOOL OnKillActive();

   static BOOL CALLBACK EnableWindows(HWND hwnd,LPARAM lParam);
public:
   afx_msg void OnCbnSelchangeHaulingMethod();

   void SwapDialogs();
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_SPECHAULINGERECTIONPAGE_H__22F9FD45_4F00_11D2_9D5F_00609710E6CE__INCLUDED_)
