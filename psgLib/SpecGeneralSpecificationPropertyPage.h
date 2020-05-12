#pragma once


// CSpecGeneralSpecificationPropertyPage

class CSpecGeneralSpecificationPropertyPage : public CMFCPropertyPage
{
	DECLARE_DYNAMIC(CSpecGeneralSpecificationPropertyPage)

public:
   CSpecGeneralSpecificationPropertyPage();
	virtual ~CSpecGeneralSpecificationPropertyPage();

protected:
   virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
   virtual BOOL OnInitDialog();

   lrfdVersionMgr::Version GetSpecVersion();

   afx_msg void OnSpecificationChanged();
   afx_msg void OnHelp();
   DECLARE_MESSAGE_MAP()
};

