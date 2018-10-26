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

// psgLib.cpp : Defines the initialization routines for the DLL.
//

#include "stdafx.h"
#include "resource.h"
#include <psgLib\psgLib.h>
#include <psgLib\StructuredLoad.h>

#include "LibraryEntryConflict.h"

#include <initguid.h>
#include <WBFLGeometry_i.c>
#include <WBFLCore_i.c>
#include <IFace\BeamFamily.h>
#include "PGSuperLibrary_i.h"
#include "LibraryAppPlugin.h"

#include <BridgeLinkCatCom.h>

#include <PGSuperCatCom.h>
#include "PGSuperLibraryMgrCATID.h"

#include "dllmain.h"

#include <EAF\EAFApp.h>
#include <EAF\EAFUtilities.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


//
//	Note!
//
//		If this DLL is dynamically linked against the MFC
//		DLLs, any functions exported from this DLL which
//		call into MFC must have the AFX_MANAGE_STATE macro
//		added at the very beginning of the function.
//
//		For example:
//
//		extern "C" BOOL PASCAL EXPORT ExportedFunction()
//		{
//			AFX_MANAGE_STATE(AfxGetStaticModuleState());
//			// normal function body here
//		}
//
//		It is very important that this macro appear in each
//		function, prior to any calls into MFC.  This means that
//		it must appear as the first statement within the 
//		function, even before any object variable declarations
//		as their constructors may generate calls into the MFC
//		DLL.
//
//		Please see MFC Technical Notes 33 and 58 for additional
//		details.
//

/////////////////////////////////////////////////////////////////////////////
// CPsgLibApp
// See psgLib.cpp for the implementation of this class
//

class CPsgLibApp : public CWinApp
{
public:
	CPsgLibApp();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CPsgLibApp)
	public:
	virtual BOOL InitInstance();
	virtual int ExitInstance();
	//}}AFX_VIRTUAL

	//{{AFX_MSG(CPsgLibApp)
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

   public:
      bool SetHelpFilePath(const std::_tstring& path);
};

/////////////////////////////////////////////////////////////////////////////
// CPsgLibApp

BEGIN_MESSAGE_MAP(CPsgLibApp, CWinApp)
	//{{AFX_MSG_MAP(CPsgLibApp)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code!
	//}}AFX_MSG_MAP
	ON_COMMAND(ID_HELP_FINDER, OnHelpFinder)
	ON_COMMAND(ID_HELP, OnHelp)
	ON_COMMAND(ID_CONTEXT_HELP, OnContextHelp)
	ON_COMMAND(ID_DEFAULT_HELP, OnHelpFinder)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CPsgLibApp construction

CPsgLibApp::CPsgLibApp()
{
	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
}

/////////////////////////////////////////////////////////////////////////////
// The one and only CPsgLibApp object

CPsgLibApp theApp;

/////////////////////////////////////////////////////////////////////////////
// CPsgLibApp initialization

BOOL CPsgLibApp::InitInstance()
{
   // enable use of activeX controls
   AfxEnableControlContainer();

   // This call will initialize the grid library
	GXInit( );

   // Use the same help file as the main application
   if ( EAFGetApp() )
   {
      free((void*)m_pszHelpFilePath);
      m_pszHelpFilePath = _tcsdup(EAFGetApp()->m_pszHelpFilePath);
   }

   sysComCatMgr::CreateCategory(L"PGSuper Library Editor Components",CATID_PGSuperLibraryManagerPlugin);

	return TRUE;
}

bool CPsgLibApp::SetHelpFilePath(const std::_tstring& rpath)
{
   if (m_pszHelpFilePath!=NULL) 	free((void*)m_pszHelpFilePath);
   m_pszHelpFilePath = _tcsdup(rpath.c_str());
   return true;
}

/////////////////////////////////////////////////////////////////////////////
// Special entry points required for inproc servers

int CPsgLibApp::ExitInstance() 
{
// This call performs cleanup etc
	GXTerminate( );
	
   // RAB : 7.26.99
   // This is taken care of in ~CWinApp()
   // See line 342 of AppCore.cpp
   //
   // This line of code is causing regsvr32.exe to crash when the DLL
   // is registered.
   //free((void*)m_pszHelpFilePath);

	return CWinApp::ExitInstance();
}

bool PSGLIBFUNC WINAPI psglibSetHelpFileLocation(const std::_tstring& rpath)
{
   return theApp.SetHelpFilePath(rpath);
}



std::_tstring PSGLIBFUNC WINAPI psglibGetFirstEntryName(const libILibrary& rlib)
{
   libKeyListType key_list;
   rlib.KeyList(key_list);
   CHECK(key_list.size()>0);
   return key_list[0];
}

template <class EntryType, class LibType>
bool do_deal_with_library_conflicts(ConflictList* pList, LibType* pMasterLib, const LibType& projectLib, const std::_tstring& libName, const EntryType& dummy, bool isImported,bool bForceUpdate)
{
   // loop over entries in project library and check to see if names are the same
   libKeyListType project_keys;
   projectLib.KeyList(project_keys);
   // create a key list with all names in it for the sole purpose of dealing with 
   // name conflicts with newly added entries
   libKeyListType master_keys;
   pMasterLib->KeyList(master_keys);
   master_keys.insert(master_keys.end(),project_keys.begin(),project_keys.end());

   for (libKeyListIterator ik=project_keys.begin(); ik!=project_keys.end(); ik++)
   {
      const std::_tstring& name= *ik;
      const EntryType* pproject = 0;
      const EntryType* pmaster = pMasterLib->LookupEntry(name.c_str());
      if (pmaster!=0)
      {
         // name is the same, now compare contents
         pproject = projectLib.LookupEntry(name.c_str());
         PRECONDITION(pproject!=0);

         if (!pmaster->IsEqual(*pproject))
         {
            // we have a conflict - ask user what he wants to do about it.
            LibConflictOutcome res;
            std::_tstring new_name;

            if ( bForceUpdate )
               res = OverWrite;
            else
               res = psglibResolveLibraryEntryConflict(name,libName,master_keys,isImported,&new_name);

            if (res==Rename)
            {
               // user wants to rename entry - add renamed entry to list
               if (!pMasterLib->AddEntry(*pproject, new_name.c_str(),false))
                  return false;

               master_keys.push_back(new_name);

               // now we have a conflict - need to save names so that we can update references
               pList->AddConflict(*pMasterLib, name, new_name);
            }
            else if (res==OverWrite)
            {
               // user wants to replace project entry with master - don't need to do anything
               pList->AddConflict(*pMasterLib, name, name);
            }
            else
               CHECK(0);
         }
         pmaster->Release();
         pproject->Release();
      }
      else
      {
         // Project entry is totally unique. copy it into master library
         const EntryType* pproject = projectLib.LookupEntry(name.c_str());
         PRECONDITION(pproject!=0);
         if (!pMasterLib->AddEntry(*pproject, pproject->GetName().c_str(),false))
            return false;

         pproject->Release();
      }
   }

   return true;
}

bool PSGLIBFUNC WINAPI psglibDealWithLibraryConflicts(ConflictList* pList, psgLibraryManager* pMasterMgr, const psgLibraryManager& projectMgr, bool isImported, bool bForceUpdate)
{
   // cycle through project library and see if entry names match master library. If the names
   // match and the entries are the same, no problem. If the entry names match and the entries are
   // different, we have a conflict

     if (!do_deal_with_library_conflicts( pList, &(pMasterMgr->GetConcreteLibrary()), 
                                    projectMgr.GetConcreteLibrary(), _T("Concrete Library"), ConcreteLibraryEntry(),isImported,bForceUpdate))
        return false;

     if (!do_deal_with_library_conflicts( pList, &(pMasterMgr->GetConnectionLibrary()), 
                                    projectMgr.GetConnectionLibrary(), _T("Connection Library"), ConnectionLibraryEntry(),isImported,bForceUpdate))
        return false;

     if (!do_deal_with_library_conflicts( pList, &(pMasterMgr->GetGirderLibrary()), 
                                    projectMgr.GetGirderLibrary(), _T("Girder Library"), GirderLibraryEntry(),isImported,bForceUpdate))
        return false;

     if (!do_deal_with_library_conflicts( pList, &(pMasterMgr->GetDiaphragmLayoutLibrary()), 
                                    projectMgr.GetDiaphragmLayoutLibrary(), _T("Diaphragm Library"), DiaphragmLayoutEntry(),isImported,bForceUpdate))
        return false;

     if (!do_deal_with_library_conflicts( pList, &(pMasterMgr->GetTrafficBarrierLibrary()), 
                                    projectMgr.GetTrafficBarrierLibrary(), _T("Traffic Barrier Library"), TrafficBarrierEntry(),isImported,bForceUpdate))
        return false;

     if (!do_deal_with_library_conflicts( pList, pMasterMgr->GetSpecLibrary(), 
                                    *(projectMgr.GetSpecLibrary()), _T("Project Criteria Library"), SpecLibraryEntry(),isImported,bForceUpdate))
        return false;

     if (!do_deal_with_library_conflicts( pList, pMasterMgr->GetLiveLoadLibrary(), 
                                    *(projectMgr.GetLiveLoadLibrary()), _T("User Defined Live Load Library"), LiveLoadLibraryEntry(),isImported,bForceUpdate))
        return false;

     if (!do_deal_with_library_conflicts( pList, pMasterMgr->GetRatingLibrary(), 
                                    *(projectMgr.GetRatingLibrary()), _T("Rating Criteria Library"), RatingLibraryEntry(),isImported,bForceUpdate))
        return false;

   return true;
}

bool do_make_saveable_copy(const libILibrary& lib, libILibrary* ptempLib)
{
   libKeyListType key_list;
   lib.KeyList(key_list);
   for (libKeyListIterator i = key_list.begin(); i!=key_list.end(); i++)
   {
      LPCTSTR key = i->c_str();
      // only copy entries to temp library if they are not read only, or if 
      // they are referenced
      if (lib.IsEditingEnabled(key) || lib.GetEntryRefCount(key)>0)
      {
         std::auto_ptr<libLibraryEntry> pent(lib.CreateEntryClone(key));
         if (!ptempLib->AddEntry(*pent, key))
            return false;
      }
   }
   return true;
}


bool PSGLIBFUNC WINAPI psglibMakeSaveableCopy(const psgLibraryManager& libMgr, psgLibraryManager* ptempManager)
{
   // concrete entries
   if (!do_make_saveable_copy(libMgr.GetConcreteLibrary(), &(ptempManager->GetConcreteLibrary())))
      return false;

   // connections
   if (!do_make_saveable_copy(libMgr.GetConnectionLibrary(), &(ptempManager->GetConnectionLibrary())))
      return false;

   // girders
   if (!do_make_saveable_copy(libMgr.GetGirderLibrary(), &(ptempManager->GetGirderLibrary())))
      return false;

   // diaphragms
   if (!do_make_saveable_copy(libMgr.GetDiaphragmLayoutLibrary(), &(ptempManager->GetDiaphragmLayoutLibrary())))
      return false;

   // traffic barriers
   if (!do_make_saveable_copy(libMgr.GetTrafficBarrierLibrary(), &(ptempManager->GetTrafficBarrierLibrary())))
      return false;

   // project criteria
   if (!do_make_saveable_copy( *(libMgr.GetSpecLibrary()), ptempManager->GetSpecLibrary()))
      return false;

   // user defined live load
   if (!do_make_saveable_copy( *(libMgr.GetLiveLoadLibrary()), ptempManager->GetLiveLoadLibrary()))
      return false;

   // rating specification
   if (!do_make_saveable_copy( *(libMgr.GetRatingLibrary()), ptempManager->GetRatingLibrary()))
      return false;

   return true;
}

void PSGLIBFUNC WINAPI psglibCreateLibNameEnum( std::vector<std::_tstring>* pNames, const libILibrary& prjLib)
{
   pNames->clear();

   // Get all the entires from the project library
   prjLib.KeyList( *pNames );
}

LibConflictOutcome PSGLIBFUNC WINAPI psglibResolveLibraryEntryConflict(const std::_tstring& entryName, const std::_tstring& libName, const std::vector<std::_tstring>& keylists, bool isImported,std::_tstring* pNewName)
{
   AFX_MANAGE_STATE(AfxGetStaticModuleState());

   CLibraryEntryConflict dlg(entryName,libName, keylists, isImported);
   dlg.DoModal();

   CLibraryEntryConflict::OutCome outcom = dlg.m_OutCome;
   LibConflictOutcome result;

   if (outcom==CLibraryEntryConflict::Rename)
   {
      *pNewName = std::_tstring(dlg.m_NewName);
      result = Rename;
   }
   else if (outcom==CLibraryEntryConflict::OverWrite)
      result = OverWrite;
   else
      ASSERT(0);

   return result;
}

bool PSGLIBFUNC WINAPI psglibImportEntries(IStructuredLoad* pStrLoad,psgLibraryManager* pLibMgr)
{
   // Load the library data into a temporary library. Then deal with entry
   // conflict resolution.
   psgLibraryManager temp_manager;
   try
   {
      CStructuredLoad load( pStrLoad );
      temp_manager.LoadMe( &load );
   }
   catch(...)
   {
      return false;
   }

   // merge project library into master library and deal with conflicts
   ConflictList the_conflict_list;
   if (!psglibDealWithLibraryConflicts(&the_conflict_list, pLibMgr, temp_manager,true,false))
      return false;

   return true;
}

HRESULT pgslibPGSuperDocHeader(IStructuredLoad* pStrLoad)
{
   HRESULT hr = pStrLoad->BeginUnit(_T("PGSuper"));
   if ( FAILED(hr) )
      return hr;

   double ver;
   pStrLoad->get_Version(&ver);

   if ( 1.0 < ver )
   {
      CComVariant var;
      var.vt = VT_BSTR;
      hr = pStrLoad->get_Property(_T("Version"),&var);
      if ( FAILED(hr) )
         return hr;

      hr = pStrLoad->BeginUnit(_T("Broker"));
      if ( FAILED(hr) )
         return hr;

      hr = pStrLoad->BeginUnit(_T("Agent"));
      if ( FAILED(hr) )
         return hr;

      hr = pStrLoad->get_Property(_T("CLSID"),&var);
      if ( FAILED(hr) )
         return hr;
   }

   return S_OK;
}

HRESULT pgslibReadLibraryDocHeader(IStructuredLoad* pStrLoad,eafTypes::UnitMode* pUnitsMode)
{
   USES_CONVERSION;

   AFX_MANAGE_STATE(AfxGetStaticModuleState());
   CString strAppUnit = AfxGetApp()->m_pszAppName;
   strAppUnit.Trim();
   strAppUnit.Replace(_T(" "),_T(""));
   pStrLoad->BeginUnit(strAppUnit); // it is ok if this fails as not all documents have this unit

   HRESULT hr = pStrLoad->BeginUnit(_T("LIBRARY_EDITOR"));
   if ( FAILED(hr) )
      return hr;

   double ver;
   pStrLoad->get_Version(&ver);
   if (ver!=1.0)
      return E_FAIL; // bad version

   // editor units
   CComVariant var;
   var.vt = VT_BSTR;
   hr = pStrLoad->get_Property(_T("EDIT_UNITS"),&var);
   if ( FAILED(hr) )
      return hr;

   std::_tstring str(OLE2T(var.bstrVal));

   if (str==_T("US"))
      *pUnitsMode = eafTypes::umUS;
   else
      *pUnitsMode = eafTypes::umSI;

   return S_OK;
}

HRESULT pgslibLoadLibrary(LPCTSTR strFileName,psgLibraryManager* pLibMgr,eafTypes::UnitMode* pUnitMode)
{
   CComPtr<IStructuredLoad> pStrLoad;
   pStrLoad.CoCreateInstance(CLSID_StructuredLoad);
   HRESULT hr = pStrLoad->Open(strFileName);
   if ( FAILED(hr) )
      return hr;

   hr =  pgslibLoadLibrary(pStrLoad,pLibMgr,pUnitMode);
   if ( FAILED(hr) )
      return hr;

   hr = pStrLoad->Close();
   if ( FAILED(hr) )
      return hr;

   return S_OK;
}

HRESULT pgslibLoadLibrary(IStructuredLoad* pStrLoad,psgLibraryManager* pLibMgr,eafTypes::UnitMode* pUnitMode)
{
   try
   {
      CStructuredLoad load(pStrLoad);

      // clear out library and load up new
      pLibMgr->ClearAllEntries();

      if ( FAILED(pgslibReadLibraryDocHeader(pStrLoad,pUnitMode)) )
         THROW_LOAD(InvalidFileFormat,(&load));

      // load the library 
      pLibMgr->LoadMe(&load);

      pStrLoad->EndUnit(); // _T("LIBRARY_EDITOR")
   }
   catch (const sysXStructuredLoad& rLoad)
   {
      sysXStructuredLoad::Reason reason = rLoad.GetExplicitReason();
      std::_tstring msg;
      CString cmsg;
      rLoad.GetErrorMessage(&msg);
      if (reason==sysXStructuredLoad::InvalidFileFormat)
         cmsg = _T("Invalid file data format. The file may have been corrupted. Extended error information is as follows: ");
      else if (reason==sysXStructuredLoad::BadVersion)
         cmsg = _T("Data file was written by a newer program version. Please upgrade this software. Extended error information is as follows: ");
      else if ( reason == sysXStructuredLoad::UserDefined )
         cmsg = _T("Error reading file. Extended error information is as follows:");
      else
         cmsg = _T("Undetermined error reading data file.  Extended error information is as follows: ");

      cmsg += msg.c_str();
      AfxMessageBox(cmsg,MB_OK | MB_ICONEXCLAMATION);
      return E_FAIL;
   }

   return S_OK;
}

// Used to determine whether the DLL can be unloaded by OLE
STDAPI DllCanUnloadNow(void)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    return (AfxDllCanUnloadNow()==S_OK && _AtlModule.GetLockCount()==0) ? S_OK : S_FALSE;
}

// Returns a class factory to create an object of the requested type
STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv)
{
    return _AtlModule.DllGetClassObject(rclsid, riid, ppv);
}

HRESULT RegisterComponents(bool bRegister)
{
   HRESULT hr = S_OK;

   // Need to register the library application plugin with the PGSuperAppPlugin category
   hr = sysComCatMgr::RegWithCategory(CLSID_LibraryAppPlugin,CATID_BridgeLinkAppPlugin,bRegister);
   if ( FAILED(hr) )
      return hr;

   return S_OK;
}


// DllRegisterServer - Adds entries to the system registry
STDAPI DllRegisterServer(void)
{
    // registers object, typelib and all interfaces in typelib
    HRESULT hr = _AtlModule.DllRegisterServer();

    hr = RegisterComponents(true);


   return S_OK;
}


// DllUnregisterServer - Removes entries from the system registry
STDAPI DllUnregisterServer(void)
{
   HRESULT hr = _AtlModule.DllUnregisterServer();
   hr = RegisterComponents(false);

   return hr;
}
