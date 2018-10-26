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

#ifndef INCLUDED_STATUSITEM_H_
#define INCLUDED_STATUSITEM_H_

// SYSTEM INCLUDES
//
#include <PgsExt\PgsExtExp.h>
#include <PgsExt\Keys.h>
#include <PgsExt\SpanGirderRelatedStatusItem.h>
#include <PgsExt\SegmentRelatedStatusItem.h>


// status for refined analysis
class PGSEXTCLASS pgsRefinedAnalysisStatusItem : public CEAFStatusItem
{
public:
   pgsRefinedAnalysisStatusItem(StatusGroupIDType statusGroupID,StatusCallbackIDType callbackID,LPCTSTR strDescription);
   bool IsEqual(CEAFStatusItem* pOther);
};

///////////////////////////
class PGSEXTCLASS pgsRefinedAnalysisStatusCallback : public iStatusCallback
{
public:
   pgsRefinedAnalysisStatusCallback(IBroker* pBroker);
   virtual eafTypes::StatusSeverityType GetSeverity();
   virtual void Execute(CEAFStatusItem* pStatusItem);

private:
   IBroker* m_pBroker;
};

// status for install error
class PGSEXTCLASS pgsInstallationErrorStatusItem : public CEAFStatusItem
{
public:
   pgsInstallationErrorStatusItem(StatusGroupIDType statusGroupID,StatusCallbackIDType callbackID,LPCTSTR strComponent,LPCTSTR strDescription);
   bool IsEqual(CEAFStatusItem* pOther);
   std::_tstring m_Component;
};

///////////////////////////
class PGSEXTCLASS pgsInstallationErrorStatusCallback : public iStatusCallback
{
public:
   pgsInstallationErrorStatusCallback();
   virtual eafTypes::StatusSeverityType GetSeverity();
   virtual void Execute(CEAFStatusItem* pStatusItem);
};

// status for unknown error
class PGSEXTCLASS pgsUnknownErrorStatusItem : public CEAFStatusItem
{
public:
   pgsUnknownErrorStatusItem(StatusGroupIDType statusGroupID,StatusCallbackIDType callbackID,LPCTSTR file,long line,LPCTSTR strDescription);
   bool IsEqual(CEAFStatusItem* pOther);
   std::_tstring m_File;
   long m_Line;
};

///////////////////////////
class PGSEXTCLASS pgsUnknownErrorStatusCallback : public iStatusCallback
{
public:
   pgsUnknownErrorStatusCallback();
   virtual eafTypes::StatusSeverityType GetSeverity();
   virtual void Execute(CEAFStatusItem* pStatusItem);
};

// status informational message
class PGSEXTCLASS pgsInformationalStatusItem : public CEAFStatusItem
{
public:
   pgsInformationalStatusItem(StatusGroupIDType statusGroupID,StatusCallbackIDType callbackID,LPCTSTR strDescription);
   bool IsEqual(CEAFStatusItem* pOther);

};

class PGSEXTCLASS pgsInformationalStatusCallback : public iStatusCallback
{
public:
   pgsInformationalStatusCallback(eafTypes::StatusSeverityType severity,UINT helpID=0);
   virtual eafTypes::StatusSeverityType GetSeverity();
   virtual void Execute(CEAFStatusItem* pStatusItem);

private:
   eafTypes::StatusSeverityType m_Severity;
   UINT m_HelpID;
};

// status for girder input
class PGSEXTCLASS pgsGirderDescriptionStatusItem : public pgsSegmentRelatedStatusItem
{
public:
   pgsGirderDescriptionStatusItem(const CSegmentKey& segmentKey,Uint16 page,StatusGroupIDType statusGroupID,StatusCallbackIDType callbackID,LPCTSTR strDescription);
   bool IsEqual(CEAFStatusItem* pOther);

   CSegmentKey m_SegmentKey;
   Uint16 m_Page; // page of girder input wizard
};


///////////////////////////
class PGSEXTCLASS pgsGirderDescriptionStatusCallback : public iStatusCallback
{
public:
   pgsGirderDescriptionStatusCallback(IBroker* pBroker,eafTypes::StatusSeverityType severity);
   virtual eafTypes::StatusSeverityType GetSeverity();
   virtual void Execute(CEAFStatusItem* pStatusItem);

private:
   IBroker* m_pBroker;
   eafTypes::StatusSeverityType m_Severity;
};

// status for structural analysis type
class PGSEXTCLASS pgsStructuralAnalysisTypeStatusItem : public CEAFStatusItem
{
public:
   pgsStructuralAnalysisTypeStatusItem(StatusGroupIDType statusGroupID,StatusCallbackIDType callbackID,LPCTSTR strDescription);
   bool IsEqual(CEAFStatusItem* pOther);
};

class PGSEXTCLASS pgsStructuralAnalysisTypeStatusCallback : public iStatusCallback
{
public:
   pgsStructuralAnalysisTypeStatusCallback();
   virtual eafTypes::StatusSeverityType GetSeverity();
   virtual void Execute(CEAFStatusItem* pStatusItem);
};

// status for general bridge description input
class PGSEXTCLASS pgsBridgeDescriptionStatusItem : public CEAFStatusItem
{
public:
   typedef enum IssueType { General, Framing, Railing, Deck, BoundaryConditions };
   pgsBridgeDescriptionStatusItem(StatusGroupIDType statusGroupID,StatusCallbackIDType callbackID,IssueType issueType,LPCTSTR strDescription);
   bool IsEqual(CEAFStatusItem* pOther);

   IssueType m_IssueType;
};

///////////////////////////
class PGSEXTCLASS pgsBridgeDescriptionStatusCallback : public iStatusCallback
{
public:
   pgsBridgeDescriptionStatusCallback(IBroker* pBroker,eafTypes::StatusSeverityType severity);
   virtual eafTypes::StatusSeverityType GetSeverity();
   virtual void Execute(CEAFStatusItem* pStatusItem);

private:
   IBroker* m_pBroker;
   eafTypes::StatusSeverityType m_Severity;
};

// status for distribution factor warnings
class PGSEXTCLASS pgsLldfWarningStatusItem : public CEAFStatusItem
{
public:
   pgsLldfWarningStatusItem(StatusGroupIDType statusGroupID,StatusCallbackIDType callbackID,LPCTSTR strDescription);
   bool IsEqual(CEAFStatusItem* pOther);
};

///////////////////////////
class PGSEXTCLASS pgsLldfWarningStatusCallback : public iStatusCallback
{
public:
   pgsLldfWarningStatusCallback(IBroker* pBroker);
   virtual eafTypes::StatusSeverityType GetSeverity();
   virtual void Execute(CEAFStatusItem* pStatusItem);

private:
   IBroker* m_pBroker;
};

// status for effective flange width warnings
class PGSEXTCLASS pgsEffectiveFlangeWidthStatusItem : public CEAFStatusItem
{
public:
   pgsEffectiveFlangeWidthStatusItem(StatusGroupIDType statusGroupID,StatusCallbackIDType callbackID,LPCTSTR strDescription);
   bool IsEqual(CEAFStatusItem* pOther);
};

///////////////////////////
class PGSEXTCLASS pgsEffectiveFlangeWidthStatusCallback : public iStatusCallback
{
public:
   pgsEffectiveFlangeWidthStatusCallback(IBroker* pBroker,eafTypes::StatusSeverityType severity);
   virtual eafTypes::StatusSeverityType GetSeverity();
   virtual void Execute(CEAFStatusItem* pStatusItem);

private:
   IBroker* m_pBroker;
   eafTypes::StatusSeverityType m_Severity;
};

#endif // INCLUDED_STATUSITEM_H_
