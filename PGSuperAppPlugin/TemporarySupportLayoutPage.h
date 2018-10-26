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


// CTemporarySupportLayoutPage dialog

class CTemporarySupportLayoutPage : public CPropertyPage
{
	DECLARE_DYNAMIC(CTemporarySupportLayoutPage)

public:
	CTemporarySupportLayoutPage();
	virtual ~CTemporarySupportLayoutPage();

   void Init(const CTemporarySupportData* pTS);

// Dialog Data
	enum { IDD = IDD_TS_GENERAL };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
   void FillEventList();
   EventIndexType CreateEvent();

   Float64 m_Station;
   std::_tstring m_strOrientation;
   pgsTypes::TemporarySupportType m_Type;
   EventIndexType m_ErectionEventIndex;
   EventIndexType m_RemovalEventIndex;
   Float64 m_ElevAdjustment;

   int m_PrevErectionEventIdx;
   int m_PrevRemovalEventIdx;

	DECLARE_MESSAGE_MAP()
public:
   virtual BOOL OnInitDialog();
   afx_msg void OnSupportTypeChanged();
   afx_msg void OnErectionEventChanged();
   afx_msg void OnErectionEventChanging();
   afx_msg void OnRemovalEventChanged();
   afx_msg void OnRemovalEventChanging();
};
