///////////////////////////////////////////////////////////////////////
// PGSuper - Prestressed Girder SUPERstructure Design and Analysis
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

#include "StdAfx.h"
#include "resource.h"
#include "PGSuperTemplateManager.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CPGSuperTemplateManager::CPGSuperTemplateManager(CHAR chSep) :
m_cFolderSeparator(chSep)
{
}

void CPGSuperTemplateManager::GetTemplates(const CString& strRootSourcePath,const CString& strRootDestinationPath,IProgressMonitor* pProgress)
{
   // Get files at the root level
   GetTemplateFiles(strRootSourcePath,strRootDestinationPath,pProgress);

   // Drill down into sub folders
   CFileFind* pFinder = CreateFileFinder();
   CString strObject = strRootSourcePath;
   // make sure the path ends with a slash
   if ( strObject.GetAt(strObject.GetLength()-1) != m_cFolderSeparator )
      strObject += CString(m_cFolderSeparator);

   strObject += "*.*";

   BOOL bWorkingOnFolders = pFinder->FindFile(strObject);
   std::vector<std::pair<CString,CString>> folders;
   while ( bWorkingOnFolders )
   {
      bWorkingOnFolders = pFinder->FindNextFile();
      CString strFileTitle = pFinder->GetFileTitle();
      if ( pFinder->IsDirectory() && !pFinder->IsDots() && strFileTitle != "" )
      {

         CString strSourcePath;
         strSourcePath.Format("%s%c%s",strRootSourcePath,m_cFolderSeparator,strFileTitle);

         CString strDestinationPath;
         strDestinationPath.Format("%s\\%s",strRootDestinationPath,strFileTitle);

         folders.push_back(std::make_pair(strSourcePath,strDestinationPath));
      }
   }
   pFinder->Close();
   delete pFinder;

   std::vector<std::pair<CString,CString>>::iterator iter;
   for ( iter = folders.begin(); iter != folders.end(); iter++ )
   {
      CString strSourcePath      = iter->first;
      CString strDestinationPath = iter->second;


      ::CreateDirectory(strDestinationPath,NULL);

      GetTemplates(strSourcePath,strDestinationPath,pProgress);
   }
}

void CPGSuperTemplateManager::GetTemplateFiles(const CString& strSourcePath,const CString& strDestinationPath,IProgressMonitor* pProgress)
{
   CString strTemplateSuffix;
   VERIFY(strTemplateSuffix.LoadString(IDS_TEMPLATE_FILE_SUFFIX));
   ASSERT(!strTemplateSuffix.IsEmpty());

   CFileFind* pTemplateFinder = CreateFileFinder();
   CString strTemplateFileSpec = strSourcePath + CString(m_cFolderSeparator) + CString("*.") + strTemplateSuffix;

   BOOL bWorkingOnFiles = pTemplateFinder->FindFile(strTemplateFileSpec);
   while ( bWorkingOnFiles )
   {
      bWorkingOnFiles = pTemplateFinder->FindNextFile();
      CString strRemoteFile = pTemplateFinder->GetFilePath();

      CString strLocalFile = strDestinationPath + "\\" + pTemplateFinder->GetFileName();

      CString strMessage;
      strMessage.Format("Downloading %s",pTemplateFinder->GetFileTitle());
      pProgress->put_Message(0,CComBSTR(strMessage));

      BOOL bSuccess = GetFile(strRemoteFile,strLocalFile);

      if ( !bSuccess )
      {
         strMessage.Format("Failed to download %s",pTemplateFinder->GetFileTitle());
         pProgress->put_Message(0,CComBSTR(strMessage));
      }
   } // end of while

   pTemplateFinder->Close();
   delete pTemplateFinder;


   CFileFind* pIconFinder = CreateFileFinder();
   CString strIconFileSpec = strSourcePath + CString(m_cFolderSeparator) + CString("*.ico");

   bWorkingOnFiles = pIconFinder->FindFile(strIconFileSpec);
   while ( bWorkingOnFiles )
   {
      bWorkingOnFiles = pIconFinder->FindNextFile();
      CString strRemoteFile = pIconFinder->GetFilePath();

      CString strLocalFile = strDestinationPath + "\\" + pIconFinder->GetFileName();

      CString strMessage;
      strMessage.Format("Downloading icon for %s",pIconFinder->GetFileTitle());
      pProgress->put_Message(0,CComBSTR(strMessage));

      BOOL bSuccess = GetFile(strRemoteFile,strLocalFile);

      if ( !bSuccess )
      {
         strMessage.Format("Failed to download icon for %s",pIconFinder->GetFileTitle());
         pProgress->put_Message(0,CComBSTR(strMessage));
      }
   } // end of while

   pIconFinder->Close();
   delete pIconFinder;
}

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
CFileTemplateManager::CFileTemplateManager() :
CPGSuperTemplateManager('\\')
{
}

CFileFind* CFileTemplateManager::CreateFileFinder()
{
   return new CFileFind;
}

BOOL CFileTemplateManager::GetFile(const CString& strSource,const CString& strDestination)
{
   return ::CopyFile(strSource,strDestination,FALSE);
}

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
CFTPTemplateManager::CFTPTemplateManager(CFtpConnection* pFTP) :
CPGSuperTemplateManager('/')
{
   m_pFTP = pFTP;
}

CFileFind* CFTPTemplateManager::CreateFileFinder()
{
   return new CFtpFileFind(m_pFTP);
}

BOOL CFTPTemplateManager::GetFile(const CString& strSource,const CString& strDestination)
{
   BOOL bSuccess = false;
   int count = 0;

   do
   {
      bSuccess = m_pFTP->GetFile(strSource,strDestination,FALSE,FILE_ATTRIBUTE_NORMAL,FTP_TRANSFER_TYPE_ASCII | INTERNET_FLAG_RELOAD);
   } while (!bSuccess && count++ < 3);

   return bSuccess;
}