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
#include "PGSuperAppPlugin\ClosurePourGeometryPage.h"
#include "PGSuperAppPlugin\GirderSegmentSpacingPage.h"
#include <PgsExt\BridgeDescription2.h>
#include "EditPier.h"

/////////////////////////////////////////////////////////////////////////////
// CPierDetailsDlg

class CPierDetailsDlg : public CPropertySheet, public IPierConnectionsParent
{
	DECLARE_DYNAMIC(CPierDetailsDlg)

// Construction
public:
	CPierDetailsDlg(const CPierData2* pPierData = NULL,CWnd* pParentWnd = NULL, UINT iSelectPage = 0);

   void SetPierData(const CPierData2* pPier);
   txnEditPierData GetEditPierData();

//interface IPierConnectionsParent
   virtual pgsTypes::PierConnectionType GetPierConnectionType(PierIndexType pierIdx);
   virtual void SetPierConnectionType(PierIndexType pierIdx,pgsTypes::PierConnectionType type);
   virtual pgsTypes::PierSegmentConnectionType GetSegmentConnectionType(PierIndexType pierIdx);
   virtual void SetSegmentConnectionType(PierIndexType pierIdx,pgsTypes::PierSegmentConnectionType type);
   virtual const CSpanData2* GetPrevSpan(PierIndexType pierIdx);
   virtual const CSpanData2* GetNextSpan(PierIndexType pierIdx);
   virtual const CBridgeDescription2* GetBridgeDescription();

   pgsTypes::MovePierOption GetMovePierOption();
   Float64 GetStation();
   LPCTSTR GetOrientation();
   EventIndexType GetErectionEventIndex();

   bool IsInteriorPier();
   bool IsBoundaryPier();
   bool HasSpacing();

   pgsTypes::PierConnectionType GetPierConnectionType();
   pgsTypes::PierSegmentConnectionType GetSegmentConnectionType();
   Float64 GetBearingOffset(pgsTypes::PierFaceType face);
   ConnectionLibraryEntry::BearingOffsetMeasurementType GetBearingOffsetMeasurementType(pgsTypes::PierFaceType face);
   Float64 GetEndDistance(pgsTypes::PierFaceType face);
   ConnectionLibraryEntry::EndDistanceMeasurementType GetEndDistanceMeasurementType(pgsTypes::PierFaceType face);
   Float64 GetSupportWidth(pgsTypes::PierFaceType face);

   GirderIndexType GetGirderCount(pgsTypes::PierFaceType pierFace);
   CGirderSpacing2 GetSpacing(pgsTypes::PierFaceType pierFace);
   pgsTypes::SupportedBeamSpacing GetSpacingType();
   bool UseSameNumberOfGirdersInAllGroups();
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

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CPierDetailsDlg)
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CPierDetailsDlg();

	// Generated message map functions
protected:
	//{{AFX_MSG(CPierDetailsDlg)
		// NOTE - the ClassWizard will add and remove member functions here.
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

   void Init();

   const CBridgeDescription2* m_pBridge;
   const CSpanData2* m_pPrevSpan;
   const CPierData2* m_pPierData;
   const CSpanData2* m_pNextSpan;

private:
   friend CPierLayoutPage;
   friend CPierGirderSpacingPage;

   // General layout page
   CPierLayoutPage            m_PierLayoutPage;

   // These two pages are used when the pier is at a boundary between girder groups
   CPierConnectionsPage       m_PierConnectionsPage;
   CPierGirderSpacingPage     m_PierGirderSpacingPage;

   // These two pages are used when the pier is interior to a girder group
   CClosurePourGeometryPage   m_ClosurePourGeometryPage;
   CGirderSegmentSpacingPage  m_GirderSegmentSpacingPage;
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_PIERDETAILSDLG_H__43875824_6EA0_4E3A_BF7A_B8D20B90BE96__INCLUDED_)
