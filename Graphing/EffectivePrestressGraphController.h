///////////////////////////////////////////////////////////////////////
// PGSuper - Prestressed Girder SUPERstructure Design and Analysis
// Copyright � 1999-2018  Washington State Department of Transportation
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

#include <Graphing\EffectivePrestressGraphBuilder.h>
#include "GirderGraphControllerBase.h"

class CEffectivePrestressGraphController : public CMultiIntervalGirderGraphControllerBase
{
public:
   CEffectivePrestressGraphController();
   DECLARE_DYNCREATE(CEffectivePrestressGraphController);

   enum ViewMode { Stress, Force };
   enum StrandType { Permanent, Temporary };

   void SetViewMode(ViewMode mode);
   ViewMode GetViewMode() const;

   void SetStrandType(StrandType strandType);
   StrandType GetStrandType() const;

   void SetDuct(DuctIndexType ductIdx);
   DuctIndexType GetDuct() const;

   bool IsStressGraph();
   bool IsPermanentStrands();


   // called by the framework when the view's OnUpdate method is called
   virtual void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint) override;

   virtual IntervalIndexType GetFirstInterval() override;

protected:

   virtual BOOL OnInitDialog() override;

   virtual void OnGroupChanged() override;
   virtual void OnGirderChanged() override;

	//{{AFX_MSG(CEffectivePrestressGraphController)
   afx_msg void OnDuctChanged();
   afx_msg void OnRadioButton(UINT nIDC);
   //}}AFX_MSG

	DECLARE_MESSAGE_MAP()

   void FillDuctCtrl();

   // control variables
   DuctIndexType          m_DuctIdx;

#ifdef _DEBUG
public:
   void AssertValid() const;
   void Dump(CDumpContext& dc) const;
#endif //_DEBUG
};