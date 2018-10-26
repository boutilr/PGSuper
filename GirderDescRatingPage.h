///////////////////////////////////////////////////////////////////////
// PGSuper - Prestressed Girder SUPERstructure Design and Analysis
// Copyright � 1999-2012  Washington State Department of Transportation
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

#include "PGSuperAppPlugin\resource.h"

// CGirderDescRatingPage dialog

class CGirderDescRatingPage : public CPropertyPage
{
	DECLARE_DYNAMIC(CGirderDescRatingPage)

public:
	CGirderDescRatingPage();
	virtual ~CGirderDescRatingPage();

   pgsTypes::ConditionFactorType m_ConditionFactorType;
   Float64 m_ConditionFactor;

// Dialog Data
	enum { IDD = IDD_GIRDERDESC_RATING };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
   afx_msg void OnConditionFactorTypeChanged();
   afx_msg void OnHelp();
   virtual BOOL OnInitDialog();
};
