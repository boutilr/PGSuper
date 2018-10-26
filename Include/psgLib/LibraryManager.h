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

#ifndef INCLUDED_PSGLIB_LIBRARYMANAGER_H_
#define INCLUDED_PSGLIB_LIBRARYMANAGER_H_

// SYSTEM INCLUDES
//

// PROJECT INCLUDES
//
#include <psgLib\psgLibLib.h>
#include <LibraryFw\Library.h>
#include <LibraryFw\LibraryManager.h>
#include <psgLib\ConcreteLibraryEntry.h>
#include <psgLib\ConnectionLibraryEntry.h>
#include <psgLib\GirderLibraryEntry.h>
#include <psgLib\DiaphragmLayoutEntry.h>
#include <psgLib\TrafficBarrierEntry.h>
#include <psgLib\SpecLibraryEntry.h>
#include <psgLib\LiveLoadLibraryEntry.h>

// LOCAL INCLUDES
//

// FORWARD DECLARATIONS
//

// MISCELLANEOUS
//
// the available library types

#define DECLARE_LIBRARY(name,entry_type) \
   PSGLIBTPL libLibrary<entry_type>; \
   typedef libLibrary<entry_type> name;

DECLARE_LIBRARY( ConcreteLibrary,        ConcreteLibraryEntry )
DECLARE_LIBRARY( ConnectionLibrary,      ConnectionLibraryEntry )
DECLARE_LIBRARY( GirderLibrary,          GirderLibraryEntry )
DECLARE_LIBRARY( DiaphragmLayoutLibrary, DiaphragmLayoutEntry )
DECLARE_LIBRARY( TrafficBarrierLibrary,  TrafficBarrierEntry )
DECLARE_LIBRARY( SpecLibrary,            SpecLibraryEntry )
DECLARE_LIBRARY( LiveLoadLibrary,        LiveLoadLibraryEntry )


/*****************************************************************************
CLASS 
   psgLibraryManager

   PSGuper spefic library manager


DESCRIPTION
   Loads PGSuper libraries


COPYRIGHT
   Copyright � 1997-1998
   Washington State Department Of Transportation
   All Rights Reserved

LOG
   rdp : 08.18.1998 : Created file
*****************************************************************************/

class PSGLIBCLASS psgLibraryManager : public libLibraryManager
{
public:
   // GROUP: LIFECYCLE

   //------------------------------------------------------------------------
   // Default constructor
   psgLibraryManager();

   //------------------------------------------------------------------------
   // Destructor
   virtual ~psgLibraryManager();

   // GROUP: OPERATORS
   // GROUP: OPERATIONS
   // GROUP: ACCESS

   //------------------------------------------------------------------------
   // access to the different types of libraries in the manager
   ConcreteLibrary&        GetConcreteLibrary();
   const ConcreteLibrary&        GetConcreteLibrary() const;
   ConnectionLibrary&      GetConnectionLibrary();
   const ConnectionLibrary&      GetConnectionLibrary() const;
   GirderLibrary&          GetGirderLibrary();
   const GirderLibrary&          GetGirderLibrary() const;
   DiaphragmLayoutLibrary& GetDiaphragmLayoutLibrary();
   const DiaphragmLayoutLibrary& GetDiaphragmLayoutLibrary() const;
   TrafficBarrierLibrary& GetTrafficBarrierLibrary();
   const TrafficBarrierLibrary& GetTrafficBarrierLibrary() const;
   SpecLibrary* GetSpecLibrary();
   const SpecLibrary* GetSpecLibrary() const;
   LiveLoadLibrary* GetLiveLoadLibrary();
   const LiveLoadLibrary* GetLiveLoadLibrary() const;

   virtual bool LoadMe(sysIStructuredLoad* pLoad);

   void SetMasterLibraryInfo(const char* strPublisher,const char* strLibFile);
   void GetMasterLibraryInfo(std::string& strPublisher,std::string& strLibFile) const;

   // GROUP: INQUIRY

protected:
   // GROUP: DATA MEMBERS
   // GROUP: LIFECYCLE
   // GROUP: OPERATORS
   // GROUP: OPERATIONS
   // GROUP: ACCESS
   // GROUP: INQUIRY
   void UpdateLibraryNames();

private:
   // GROUP: DATA MEMBERS
   Uint32 m_ConcLibIdx;
   Uint32 m_ConnLibIdx;
   Uint32 m_GirdLibIdx;
   Uint32 m_DiapLibIdx;
   Uint32 m_BarrLibIdx;
   Uint32 m_SpecLibIdx;
   Uint32 m_LiveLibIdx;

   std::string m_strPublisher;
   std::string m_strLibFile;

   // GROUP: LIFECYCLE

   // Prevent accidental copying and assignment
   psgLibraryManager(const psgLibraryManager&);
   psgLibraryManager& operator=(const psgLibraryManager&);

   // GROUP: OPERATORS
   // GROUP: OPERATIONS
   // GROUP: ACCESS
   // GROUP: INQUIRY

public:
   // GROUP: DEBUG
   #if defined _DEBUG
   //------------------------------------------------------------------------
   // Returns true if the object is in a valid state, otherwise returns false.
   virtual bool AssertValid() const;

   //------------------------------------------------------------------------
   // Dumps the contents of the object to the given dump context.
   virtual void Dump(dbgDumpContext& os) const;
   #endif // _DEBUG

   #if defined UNIT_TEST
   //------------------------------------------------------------------------
   // Runs a self-diagnostic test.  Returns true if the test passed,
   // otherwise false.
   static bool TestMe(dbgLog& rlog);
   #endif // UNIT_TEST
};

// INLINE METHODS
//

// EXTERNAL REFERENCES
//

#endif // INCLUDED_PSGLIB_LIBRARYMANAGER_H_
