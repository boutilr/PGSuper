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

// TxDOTOptionalDesignChildFrm.cpp : implementation of the CTxDOTOptionalDesignChildFrame class
//

#include "stdafx.h"
#include "resource.h"
#include "HtmlHelp\TogaHelp.hh"

#include "TxDOTOptionalDesignChildFrame.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CTxDOTOptionalDesignChildFrame

IMPLEMENT_DYNCREATE(CTxDOTOptionalDesignChildFrame, CMDIChildWnd)

BEGIN_MESSAGE_MAP(CTxDOTOptionalDesignChildFrame, CMDIChildWnd)
	//{{AFX_MSG_MAP(CTxDOTOptionalDesignChildFrame)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG_MAP
   ON_WM_CREATE()
   ON_COMMAND(ID_LICENSE_AGREEMENT, &CTxDOTOptionalDesignChildFrame::OnLicenseAgreement)
   ON_WM_HELPINFO()
   ON_COMMAND(ID_HELP_FINDER, &CTxDOTOptionalDesignChildFrame::OnHelpFinder)
   ON_COMMAND(ID_HELP, &CTxDOTOptionalDesignChildFrame::OnHelpFinder)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CTxDOTOptionalDesignChildFrame construction/destruction

CTxDOTOptionalDesignChildFrame::CTxDOTOptionalDesignChildFrame()
{
}

CTxDOTOptionalDesignChildFrame::~CTxDOTOptionalDesignChildFrame()
{
}

BOOL CTxDOTOptionalDesignChildFrame::PreCreateWindow(CREATESTRUCT& cs)
{
   // get rid of system menu and resizable frame
   cs.style = WS_CHILD;

	if( !CMDIChildWnd::PreCreateWindow(cs) )
		return FALSE;

	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// CTxDOTOptionalDesignChildFrame diagnostics

#ifdef _DEBUG
void CTxDOTOptionalDesignChildFrame::AssertValid() const
{
	CMDIChildWnd::AssertValid();
}

void CTxDOTOptionalDesignChildFrame::Dump(CDumpContext& dc) const
{
	CMDIChildWnd::Dump(dc);
}

#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CTxDOTOptionalDesignChildFrame message handlers


void CTxDOTOptionalDesignChildFrame::ActivateFrame(int nCmdShow)
{
   if (nCmdShow == -1)
      nCmdShow = SW_SHOWMAXIMIZED;

   CMDIChildWnd::ActivateFrame(nCmdShow);
}

void CTxDOTOptionalDesignChildFrame::OnUpdateFrameTitle(BOOL bAddToTitle)
{
	if (bAddToTitle)
   {
      CString msg(_T("Toga Plugin"));

      // set our title
		AfxSetWindowText(m_hWnd, msg);
   }
}

void CTxDOTOptionalDesignChildFrame::OnLicenseAgreement()
{
   AFX_MANAGE_STATE(AfxGetStaticModuleState());

   CWinApp* papp = AfxGetApp();
   ::HtmlHelp( *this, papp->m_pszHelpFilePath, HH_HELP_CONTEXT, IDH_LICENSE );
}

BOOL CTxDOTOptionalDesignChildFrame::OnHelpInfo(HELPINFO* pHelpInfo)
{
   AFX_MANAGE_STATE(AfxGetStaticModuleState());
   CWinApp* pApp = AfxGetApp();
   ::HtmlHelp( *this, pApp->m_pszHelpFilePath, HH_HELP_CONTEXT, IDH_GIRDER_INPUT );

   return TRUE;
}

void CTxDOTOptionalDesignChildFrame::OnHelpFinder()
{
   AFX_MANAGE_STATE(AfxGetStaticModuleState());
   CWinApp* pApp = AfxGetApp();
   ::HtmlHelp( *this, pApp->m_pszHelpFilePath, HH_HELP_CONTEXT, IDH_WELCOME );
}
