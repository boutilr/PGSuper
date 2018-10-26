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

#if !defined(AFX_PIERGIRDERSPACINGPAGE_H__46D9CF84_9F04_43CC_9585_F9987EC38EBC__INCLUDED_)
#define AFX_PIERGIRDERSPACINGPAGE_H__46D9CF84_9F04_43CC_9585_F9987EC38EBC__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// PierGirderSpacingPage.h : header file
//

#include "PGSuperAppPlugin\resource.h"
#include "GirderSpacingGrid.h"
#include "SameNumberOfGirdersHyperLink.h"
#include "SameGirderSpacingHyperLink.h"
#include "SameSlabOffsetHyperLink.h"


/////////////////////////////////////////////////////////////////////////////
// CPierGirderSpacingPage dialog

class CPierGirderSpacingPage : public CPropertyPage
{
	DECLARE_DYNCREATE(CPierGirderSpacingPage)

// Construction
public:
	CPierGirderSpacingPage();
	~CPierGirderSpacingPage();

// Dialog Data
	//{{AFX_DATA(CPierGirderSpacingPage)
	enum { IDD = IDD_GIRDERSPACING };
		// NOTE - ClassWizard will add data members here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_DATA

   // array index 0 = back side of pier = end of previous span
   //             1 = ahead side of pier = start of next span
   // use pgsTypes::PierFaceType as key
	CSpinButtonCtrl	            m_NumGdrSpinner[2];
   CSameNumberOfGirdersHyperLink m_NumGirdersHyperLink[2];
   CSameGirderSpacingHyperLink   m_GirderSpacingHyperLink[2];
   CSameSlabOffsetHyperLink      m_SlabOffsetHyperLink[2];
   CComboBox                     m_cbGirderSpacingMeasurement[2];

   GirderIndexType m_nGirders[2];
   
   // global bridge girder spacing and number of girder settings
   bool m_bUseSameNumGirders;

   CGirderSpacingGrid m_GirderSpacingGrid[2];
   DWORD m_GirderSpacingMeasure[2];

   GirderIndexType m_RefGirderIdx[2];
   Float64 m_RefGirderOffset[2];
   pgsTypes::OffsetMeasurementType m_RefGirderOffsetType[2];

   pgsTypes::SlabOffsetType m_SlabOffsetType;
   pgsTypes::SlabOffsetType m_SlabOffsetTypeCache;
   Float64 m_SlabOffset[2];
   CString m_strSlabOffsetCache[2];


   void Init(const CPierData* pPier);
   bool AllowConnectionChange(pgsTypes::PierFaceType side, const CString& conectionName);

// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CPierGirderSpacingPage)
	public:
	virtual BOOL OnSetActive();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CPierGirderSpacingPage)
	virtual BOOL OnInitDialog();
	afx_msg void OnNumGirdersPrevSpanChanged(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnNumGirdersNextSpanChanged(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	afx_msg void OnCopyToAheadSide();
	afx_msg void OnCopyToBackSide();
   afx_msg void OnAheadPierSpacingDatumChanged();
   afx_msg void OnBackPierSpacingDatumChanged();
   afx_msg void OnHelp();
	//}}AFX_MSG
   afx_msg LRESULT OnChangeSameNumberOfGirders(WPARAM wParam,LPARAM lParam);
   afx_msg LRESULT OnChangeSameGirderSpacing(WPARAM wParam,LPARAM lParam);
   afx_msg LRESULT OnChangeSlabOffset(WPARAM wParam,LPARAM lParam);
	DECLARE_MESSAGE_MAP()

   const CPierData* m_pPier;
   const CSpanData* m_pPrevSpan;
   const CSpanData* m_pNextSpan;

   void ToggleGirderSpacingType();

   GirderIndexType m_MinGirderCount[2];

   GirderIndexType m_nGirdersCache[2];
   CGirderSpacingGridData m_GirderSpacingCache[2];
   long m_GirderSpacingMeasureCache[2];

   int GetMinGirderCount(const CSpanData* pSpan);

   void OnPierSpacingDatumChanged(UINT nIDC,pgsTypes::PierFaceType pierFace);

   void FillGirderSpacingMeasurementComboBox(int nIDC, ConnectionLibraryEntry::BearingOffsetMeasurementType bearingMeasure);
   void OnNumGirdersChanged(NMHDR* pNMHDR,LRESULT* pResult,pgsTypes::PierFaceType pierFace);
   void AddGirders(GirderIndexType nGirders,pgsTypes::PierFaceType pierFace);
   void RemoveGirders(GirderIndexType nGirders,pgsTypes::PierFaceType pierFace);
   void SetGirderCount(GirderIndexType nGirders,pgsTypes::PierFaceType pierFace);

   void FillRefGirderOffsetTypeComboBox(pgsTypes::PierFaceType pierFace);
   void FillRefGirderComboBox(pgsTypes::PierFaceType pierFace);

   void UpdateGirderSpacingHyperLinkText();
   void UpdateNumGirdersHyperLinkText();
   void UpdateSlabOffsetHyperLinkText();
   
   void DisableAll();
   void UpdateLinkedNote();
   void UpdateChildWindowState();
   void UpdateGirderSpacingState(pgsTypes::PierFaceType pierFace);
   void UpdateCopyButtonState(BOOL bEnable);

   void HideBackGroup(bool bHide=true);
   void HideAheadGroup(bool bHide=true);
   void HideGroup(UINT* nIDs,UINT nControls,bool bHide=true);
   void MoveAheadGroup();

   void InitSpacingBack(bool bUse);
   void InitSpacingAhead(bool bUse);

   bool IsAbutment();

   enum SpacingType {Single, // a single spacing is used for both sides
                     Back, // last pier, spacing on back side only
                     Ahead, // first pier, spacing on ahead side only
                     Both}; // spacing on both sides
   SpacingType m_SpacingType;
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_PIERGIRDERSPACINGPAGE_H__46D9CF84_9F04_43CC_9585_F9987EC38EBC__INCLUDED_)
