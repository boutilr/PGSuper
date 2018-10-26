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

#pragma once

#include <Graphing\DeflectionHistoryGraphBuilder.h>
#include <EAF\EAFGraphControlWindow.h>

class CDeflectionHistoryGraphController : public CEAFGraphControlWindow
{
public:
   CDeflectionHistoryGraphController();
   DECLARE_DYNCREATE(CDeflectionHistoryGraphController);

   CGirderKey GetGirderKey();
   pgsPointOfInterest GetLocation();
   int GetXAxisType();

protected:
   virtual BOOL OnInitDialog();

	//{{AFX_MSG(CStressHistoryGraphController)
   afx_msg void OnGroupChanged();
   afx_msg void OnGirderChanged();
   afx_msg void OnLocationChanged();
   afx_msg void OnXAxis();
   //}}AFX_MSG

	DECLARE_MESSAGE_MAP()

   void FillGroupCtrl();
   void FillGirderCtrl();
   void FillLocationCtrl();
   void UpdateGraph();

   CComPtr<IBroker> m_pBroker;

   CGirderKey m_GirderKey;
   pgsPointOfInterest m_Poi;

#ifdef _DEBUG
public:
   void AssertValid() const;
   void Dump(CDumpContext& dc) const;
#endif //_DEBUG
};