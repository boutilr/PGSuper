///////////////////////////////////////////////////////////////////////
// PGSuper - Prestressed Girder SUPERstructure Design and Analysis
// Copyright � 1999-2014  Washington State Department of Transportation
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

#if !defined(AFX_PIERDETAILSDLG_H__43875824_6EA0_4E3A_BF7A_B8D20B90BE96__INCLUDED_)
#define AFX_PIERDETAILSDLG_H__43875824_6EA0_4E3A_BF7A_B8D20B90BE96__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// PierDetailsDlg.h : header file
//

#include "PierLayoutPage.h"
#include "PierConnectionsPage.h"
#include "PierGirderSpacingPage.h"
#include <PgsExt\BridgeDescription.h>
#include "EditPier.h"
#include <IFace\ExtendUI.h>

/////////////////////////////////////////////////////////////////////////////
// CPierDetailsDlg

class CPierDetailsDlg : public CPropertySheet, public IPierConnectionsParent, public IEditPierData
{
	DECLARE_DYNAMIC(CPierDetailsDlg)

// Construction
public:
	CPierDetailsDlg(const CPierData* pPier,CWnd* pParentWnd = NULL, UINT iSelectPage = 0);
	CPierDetailsDlg(const CPierData* pPier,const std::set<EditBridgeExtension>& editBridgeExtensions,CWnd* pParentWnd = NULL, UINT iSelectPage = 0);

   void SetPierData(const CPierData* pPier);
   txnEditPierData GetEditPierData();

//interface IPierConnectionsParent
   virtual pgsTypes::PierConnectionType GetConnectionType(PierIndexType pierIdx);
   virtual void SetConnectionType(PierIndexType pierIdx,pgsTypes::PierConnectionType type);
   virtual const CSpanData* GetPrevSpan(PierIndexType pierIdx);
   virtual const CSpanData* GetNextSpan(PierIndexType pierIdx);
   virtual const CBridgeDescription* GetBridgeDescription();

// interface IEditPierData
   virtual PierIndexType GetPierCount() { return m_pBridge->GetPierCount(); }
   virtual PierIndexType GetPier() { return m_pPierData->GetPierIndex(); }
   virtual pgsTypes::PierConnectionType GetConnectionType();
   virtual GirderIndexType GetGirderCount(pgsTypes::PierFaceType face);

   pgsTypes::MovePierOption GetMovePierOption();
   Float64 GetStation();
   LPCTSTR GetOrientation();

   //pgsTypes::PierConnectionType GetConnectionType();
   Float64 GetBearingOffset(pgsTypes::PierFaceType face);
   ConnectionLibraryEntry::BearingOffsetMeasurementType GetBearingOffsetMeasurementType(pgsTypes::PierFaceType face);
   Float64 GetEndDistance(pgsTypes::PierFaceType face);
   ConnectionLibraryEntry::EndDistanceMeasurementType GetEndDistanceMeasurementType(pgsTypes::PierFaceType face);
   Float64 GetSupportWidth(pgsTypes::PierFaceType face);

   //GirderIndexType GetGirderCount(pgsTypes::PierFaceType pierFace);
   CGirderSpacing GetSpacing(pgsTypes::PierFaceType pierFace);
   pgsTypes::SupportedBeamSpacing GetSpacingType();
   bool UseSameNumberOfGirdersInAllSpans();
   pgsTypes::MeasurementLocation GetMeasurementLocation(pgsTypes::PierFaceType pierFace);
   pgsTypes::MeasurementType GetMeasurementType(pgsTypes::PierFaceType pierFace);
   
   pgsTypes::MeasurementLocation GetMeasurementLocation(); // for the entire bridge

   pgsTypes::SlabOffsetType GetSlabOffsetType();
   Float64 GetSlabOffset(pgsTypes::PierFaceType pierFace);

   Float64 GetDiaphragmHeight(pgsTypes::PierFaceType pierFace);
   Float64 GetDiaphragmWidth(pgsTypes::PierFaceType pierFace);
   ConnectionLibraryEntry::DiaphragmLoadType GetDiaphragmLoadType(pgsTypes::PierFaceType pierFace);
   Float64 GetDiaphragmLoadLocation(pgsTypes::PierFaceType pierFace);


   bool AllowConnectionChange(pgsTypes::PierFaceType side, const CString& conectionName);

// Attributes
public:

// Operations
public:
   // Returns a macro transaction object that contains editing transactions
   // for all the extension pages. The caller is responsble for deleting this object
   txnTransaction* GetExtensionPageTransaction();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CPierDetailsDlg)
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CPierDetailsDlg();

   virtual INT_PTR DoModal();

	// Generated message map functions
protected:
	//{{AFX_MSG(CPierDetailsDlg)
		// NOTE - the ClassWizard will add and remove member functions here.
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
	afx_msg LRESULT OnKickIdle(WPARAM, LPARAM);

   void Init();
   void Init(const std::set<EditBridgeExtension>& editBridgeExtensions);
   void CommonInit();
   void CreateExtensionPages();
   void CreateExtensionPages(const std::set<EditBridgeExtension>& editBridgeExtensions);
   void DestroyExtensionPages();

   const CBridgeDescription* m_pBridge;
   const CSpanData* m_pPrevSpan;
   const CPierData* m_pPierData;
   const CSpanData* m_pNextSpan;

   pgsTypes::PierConnectionType m_ConnectionType;
   pgsTypes::SupportedBeamSpacing m_SpacingType;
   pgsTypes::MeasurementLocation m_GirderSpacingMeasurementLocation;

private:
   friend CPierLayoutPage;
   friend CPierGirderSpacingPage;

   CPierLayoutPage            m_PierLayoutPage;
   CPierConnectionsPage       m_PierConnectionsPage;
   CPierGirderSpacingPage     m_PierGirderSpacingPage;

   txnMacroTxn m_Macro;
   std::vector<std::pair<IEditPierCallback*,CPropertyPage*>> m_ExtensionPages;
   std::set<EditBridgeExtension> m_BridgeExtensionPages;
   void NotifyExtensionPages();
   void NotifyBridgeExtensionPages();
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_PIERDETAILSDLG_H__43875824_6EA0_4E3A_BF7A_B8D20B90BE96__INCLUDED_)
