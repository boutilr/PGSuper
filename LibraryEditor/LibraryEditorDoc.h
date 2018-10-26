///////////////////////////////////////////////////////////////////////
// Library Editor - Editor for WBFL Library Services
// Copyright � 1999-2010  Washington State Department of Transportation
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

// LibraryEditorDoc.h : interface of the CLibraryEditorDoc class
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_LIBRARYEDITORDOC_H__340EC2FE_20E1_11D2_9D35_00609710E6CE__INCLUDED_)
#define AFX_LIBRARYEDITORDOC_H__340EC2FE_20E1_11D2_9D35_00609710E6CE__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include <psglib\ISupportLibraryManager.h>
#include <psgLib\LibraryManager.h>

#if !defined INCLUDED_LIBRARYFW_UNITSMODE_H_
#include <LibraryFw\UnitsMode.h>
#endif

class CLibraryEditorDoc  : public CDocument , public libISupportLibraryManager
{
protected: // create from serialization only
	CLibraryEditorDoc();
	DECLARE_DYNCREATE(CLibraryEditorDoc)

// Attributes
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CLibraryEditorDoc)
	public:
	virtual BOOL OnNewDocument();
	virtual void Serialize(CArchive& ar);
	virtual BOOL OnOpenDocument(LPCTSTR lpszPathName);
	virtual BOOL OnSaveDocument(LPCTSTR lpszPathName);
	//}}AFX_VIRTUAL
   void OnImport();

// Implementation
public:
	virtual ~CLibraryEditorDoc();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

   int GetNumberOfLibraryManagers() const {return 1;}
   libLibraryManager* GetLibraryManager(int n){ASSERT(n!=1); return &m_LibraryManager;}
   libLibraryManager* GetTargetLibraryManager(){return &m_LibraryManager;}

public:
   // set and get units mode
   void SetUnitsMode(libUnitsMode::Mode mode) {m_UnitsMode = mode;}
   libUnitsMode::Mode GetUnitsMode() const {return m_UnitsMode;}

private:
   libUnitsMode::Mode m_UnitsMode;

   void HandleOpenDocumentError( HRESULT hr, LPCTSTR lpszPathName );
   void HandleSaveDocumentError( HRESULT hr, LPCTSTR lpszPathName );

protected:
   psgLibraryManager m_LibraryManager;
// Generated message map functions
protected:
	//{{AFX_MSG(CLibraryEditorDoc)
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_LIBRARYEDITORDOC_H__340EC2FE_20E1_11D2_9D35_00609710E6CE__INCLUDED_)
