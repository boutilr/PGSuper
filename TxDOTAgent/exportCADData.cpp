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
//
// exportCADData.cpp : implementation file
//

#include "stdafx.h"

#include <ATLBase.h>

#include "exportCADData.h"

#include <IFace\Bridge.h>
//#include "HtmlHelp\HelpTopics.hh"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// exportCADData dialog

/*--------------------------------------------------------------------*/
exportCADData::exportCADData(IBroker* pBroker,CWnd* pParent /*=NULL*/)
	: CDialog(exportCADData::IDD, pParent)
{
   m_pBroker  = pBroker;
	//{{AFX_DATA_INIT(exportCADData)
	m_IsExtended = FALSE;
	//}}AFX_DATA_INIT

   m_pGrid = new CMultiGirderSelectGrid();
}

/*--------------------------------------------------------------------*/
exportCADData::~exportCADData()
{
   delete m_pGrid;
}

/*--------------------------------------------------------------------*/
void exportCADData::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(exportCADData)
	DDX_Check(pDX, IDC_EXTENDED, m_IsExtended);
	//}}AFX_DATA_MAP

   if ( pDX->m_bSaveAndValidate )
   {
      // Girders
      m_SelGdrs = m_pGrid->GetData();
      if ( m_SelGdrs.empty() )
      {
         pDX->PrepareCtrl(IDC_SELECT_GRID);
         AfxMessageBox(IDS_E_NOGIRDERS);
         pDX->Fail();
      }
   }
}

/*--------------------------------------------------------------------*/
BEGIN_MESSAGE_MAP(exportCADData, CDialog)
	//{{AFX_MSG_MAP(exportCADData)
	ON_COMMAND(ID_HELP, OnHelp)
	ON_CBN_SELCHANGE(IDC_SPAN, OnSelchangeSpan)
	//}}AFX_MSG_MAP
   ON_BN_CLICKED(IDC_SELECT_ALL, &exportCADData::OnBnClickedSelectAll)
   ON_BN_CLICKED(IDC_CLEAR_ALL, &exportCADData::OnBnClickedClearAll)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// exportCADData message handlers


/*--------------------------------------------------------------------*/
BOOL exportCADData::OnInitDialog() 
{
 	m_pGrid->SubclassDlgItem(IDC_SELECT_GRID, this);

	/* Perform the root dialog initialization */
	CDialog::OnInitDialog();

	/* Get interface pointer to Bridge Agent */
	GET_IFACE( IBridge, pBridge ); 

   SpanGirderOnCollection coll;
   SpanIndexType ns = pBridge->GetSpanCount();
   for (SpanIndexType is=0; is<ns; is++)
   {
      GirderIndexType ng = pBridge->GetGirderCount(is);
      std::vector<bool> gdrson;
      gdrson.assign(ng, false); // set all to false

      coll.push_back(gdrson);
   }

   // set selected girders
   for(std::vector<SpanGirderHashType>::iterator it = m_SelGdrs.begin(); it != m_SelGdrs.end(); it++)
   {
      SpanIndexType span;
      GirderIndexType gdr;
      UnhashSpanGirder(*it, &span, &gdr);

      if (span<ns)
      {
         std::vector<bool>& rgdrson = coll[span];
         if (gdr < (GirderIndexType)rgdrson.size())
         {
            rgdrson[gdr] = true;
         }
         else
         {
            ATLASSERT(0); // might be a problem?
         }
      }
      else
      {
         ATLASSERT(0); // might be a problem?
      }
   }

   m_pGrid->CustomInit(coll,pgsGirderLabel::GetGirderLabel);

   return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}


/*--------------------------------------------------------------------*/
void exportCADData::OnHelp() 
{
//   ::HtmlHelp( *this, AfxGetApp()->m_pszHelpFilePath, HH_HELP_CONTEXT, IDH_DIALOG_EXPORTTXDOTCADDATA);
}

void exportCADData::OnSelchangeSpan() 
{
	GET_IFACE( IBridge, pBridge ); 
	CComboBox* pSpanBox = (CComboBox*)GetDlgItem( IDC_SPAN );
   CComboBox* pGdrBox  = (CComboBox*)GetDlgItem( IDC_GIRDER );

   SpanIndexType span = pSpanBox->GetCurSel();
   if ( span == CB_ERR )
      span = 0;

   GirderIndexType girder = pGdrBox->GetCurSel();
   if (girder == CB_ERR )
      girder = 0;

   pGdrBox->ResetContent();

	/* Get count of girders (same number of girders in all spans) */
   GirderIndexType cGirder = pBridge->GetGirderCount(span);
	for ( GirderIndexType j = 0; j < cGirder; j++ )
	{
		/* Add current girder string to girder list */
		CString strGdr;
		strGdr.Format( _T("Girder %s"), LABEL_GIRDER(j));
		pGdrBox->AddString( strGdr );
	}

   girder = min(girder, cGirder-1); // don't allow out of bounds if ng decreases between spans

   pGdrBox->SetCurSel(girder);
	
}

void exportCADData::OnBnClickedSelectAll()
{
   m_pGrid->SetAllValues(true);
}

void exportCADData::OnBnClickedClearAll()
{
   m_pGrid->SetAllValues(false);
}
