///////////////////////////////////////////////////////////////////////
// PGSuper - Prestressed Girder SUPERstructure Design and Analysis
// Copyright � 1999-2011  Washington State Department of Transportation
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

/////////////////////////////////////////////////////////////////////////////
// CPierDetailsDlg

class CPierDetailsDlg : public CPropertySheet
{
	DECLARE_DYNAMIC(CPierDetailsDlg)

// Construction
public:
	CPierDetailsDlg(const CPierData* pPierData = NULL,CWnd* pParentWnd = NULL, UINT iSelectPage = 0);

   void SetPierData(const CPierData* pPier);
   txnEditPierData GetEditPierData();

   pgsTypes::MovePierOption GetMovePierOption();
   double GetStation();
   LPCTSTR GetOrientation();

   LPCTSTR GetConnection(pgsTypes::PierFaceType pierFace);
   pgsTypes::PierConnectionType GetConnectionType();
   
   GirderIndexType GetNumGirders(pgsTypes::PierFaceType pierFace);
   CGirderSpacing GetGirderSpacing(pgsTypes::PierFaceType pierFace);
   pgsTypes::SupportedBeamSpacing GetGirderSpacingType();
   bool UseSameNumberOfGirdersInAllSpans();
   pgsTypes::MeasurementLocation GetMeasurementLocation(pgsTypes::PierFaceType pierFace);
   pgsTypes::MeasurementType GetMeasurementType(pgsTypes::PierFaceType pierFace);
   
   pgsTypes::MeasurementLocation GetMeasurementLocation(); // for the entire bridge

   pgsTypes::SlabOffsetType GetSlabOffsetType();
   Float64 GetSlabOffset(pgsTypes::PierFaceType pierFace);

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

   const CBridgeDescription* m_pBridge;
   const CSpanData* m_pPrevSpan;
   const CPierData* m_pPierData;
   const CSpanData* m_pNextSpan;

   pgsTypes::PierConnectionType m_ConnectionType;
   CString m_ConnectionName[2];

private:
   friend CPierLayoutPage;
   friend CPierConnectionsPage;
   friend CPierGirderSpacingPage;

   CPierLayoutPage m_PierLayoutPage;
   CPierConnectionsPage m_PierConnectionsPage;
   CPierGirderSpacingPage m_PierGirderSpacingPage;
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_PIERDETAILSDLG_H__43875824_6EA0_4E3A_BF7A_B8D20B90BE96__INCLUDED_)
