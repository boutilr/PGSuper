///////////////////////////////////////////////////////////////////////
// PGSuper - Prestressed Girder SUPERstructure Design and Analysis
// Copyright (C) 1999  Washington State Department of Transportation
//                     Bridge and Structures Office
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
#include <psgLib\LibraryManager.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/****************************************************************************
CLASS
   psgLibraryManager
****************************************************************************/



////////////////////////// PUBLIC     ///////////////////////////////////////

//======================== LIFECYCLE  =======================================
psgLibraryManager::psgLibraryManager() :
libLibraryManager()
{
   std::auto_ptr<ConcreteLibrary>        conc_lib(new ConcreteLibrary("CONCRETE_LIBRARY", "Concrete Types"));
   std::auto_ptr<ConnectionLibrary>      conn_lib(new ConnectionLibrary("CONNECTION_LIBRARY", "Connections"));
   std::auto_ptr<GirderLibrary>          gird_lib(new GirderLibrary("GIRDER_LIBRARY", "Girders"));
   std::auto_ptr<DiaphragmLayoutLibrary> diap_lib(new DiaphragmLayoutLibrary("DIAPHRAGM_LAYOUT_LIBRARY", "Diaphragm Layouts",true)); 
   std::auto_ptr<TrafficBarrierLibrary>  barr_lib(new TrafficBarrierLibrary("TRAFFIC_BARRIER_LIBRARY", "Traffic Barriers"));
   std::auto_ptr<SpecLibrary>            spec_lib(new SpecLibrary("SPECIFICATION_LIBRARY", "Project Criteria"));
   std::auto_ptr<LiveLoadLibrary>        live_lib(new LiveLoadLibrary("USER_LIVE_LOAD_LIBRARY", "User-Defined Live Loads"));

   live_lib->AddReservedName("HL-93");
   live_lib->AddReservedName("Fatigue");
   live_lib->AddReservedName("Pedestrian on Sidewalk");
   live_lib->AddReservedName("No Live Load Defined"); // this is for the dummy live load, when one isn't defined for analysis

   m_ConcLibIdx = this->AddLibrary(conc_lib.release()); 
   m_ConnLibIdx = this->AddLibrary(conn_lib.release());
   m_GirdLibIdx = this->AddLibrary(gird_lib.release());
   m_DiapLibIdx = this->AddLibrary(diap_lib.release());
   m_BarrLibIdx = this->AddLibrary(barr_lib.release());
   m_SpecLibIdx = this->AddLibrary(spec_lib.release());
   m_LiveLibIdx = this->AddLibrary(live_lib.release());

   m_strPublisher = "Unknown";
   m_strLibFile   = "Unknown";

   UpdateLibraryNames();
}

psgLibraryManager::~psgLibraryManager()
{
   // Release all the class factories before the DLL unloads
   GirderLibraryEntry::ms_ClassFactories.clear();
}

//======================== OPERATORS  =======================================
//======================== OPERATIONS =======================================
//======================== ACCESS     =======================================

ConcreteLibrary&        psgLibraryManager::GetConcreteLibrary()
{
   libILibrary* pl = GetLibrary(m_ConcLibIdx);
   ConcreteLibrary* pc = dynamic_cast<ConcreteLibrary*>(pl);
   ASSERT(pc);
   return *pc;
}

const ConcreteLibrary&        psgLibraryManager::GetConcreteLibrary() const
{
   const libILibrary* pl = GetLibrary(m_ConcLibIdx);
   const ConcreteLibrary* pc = dynamic_cast<const ConcreteLibrary*>(pl);
   ASSERT(pc);
   return *pc;
}

ConnectionLibrary&      psgLibraryManager::GetConnectionLibrary()
{
   libILibrary* pl = GetLibrary(m_ConnLibIdx);
   ConnectionLibrary* pc = dynamic_cast<ConnectionLibrary*>(pl);
   ASSERT(pc);
   return *pc;
}

const ConnectionLibrary&      psgLibraryManager::GetConnectionLibrary() const
{
   const libILibrary* pl = GetLibrary(m_ConnLibIdx);
   const ConnectionLibrary* pc = dynamic_cast<const ConnectionLibrary*>(pl);
   ASSERT(pc);
   return *pc;
}

GirderLibrary&          psgLibraryManager::GetGirderLibrary()
{
   libILibrary* pl = GetLibrary(m_GirdLibIdx);
   GirderLibrary* pc = dynamic_cast<GirderLibrary*>(pl);
   ASSERT(pc);
   return *pc;
}

const GirderLibrary&          psgLibraryManager::GetGirderLibrary() const
{
   const libILibrary* pl = GetLibrary(m_GirdLibIdx);
   const GirderLibrary* pc = dynamic_cast<const GirderLibrary*>(pl);
   ASSERT(pc);
   return *pc;
}

DiaphragmLayoutLibrary& psgLibraryManager::GetDiaphragmLayoutLibrary()
{
   libILibrary* pl = GetLibrary(m_DiapLibIdx);
   DiaphragmLayoutLibrary* pc = dynamic_cast<DiaphragmLayoutLibrary*>(pl);
   ASSERT(pc);
   return *pc;
}

const DiaphragmLayoutLibrary& psgLibraryManager::GetDiaphragmLayoutLibrary() const
{
   const libILibrary* pl = GetLibrary(m_DiapLibIdx);
   const DiaphragmLayoutLibrary* pc = dynamic_cast<const DiaphragmLayoutLibrary*>(pl);
   ASSERT(pc);
   return *pc;
}

TrafficBarrierLibrary& psgLibraryManager::GetTrafficBarrierLibrary()
{
   libILibrary* pl = GetLibrary(m_BarrLibIdx);
   TrafficBarrierLibrary* pc = dynamic_cast<TrafficBarrierLibrary*>(pl);
   ASSERT(pc);
   return *pc;
}

const TrafficBarrierLibrary& psgLibraryManager::GetTrafficBarrierLibrary() const
{
   const libILibrary* pl = GetLibrary(m_BarrLibIdx);
   const TrafficBarrierLibrary* pc = dynamic_cast<const TrafficBarrierLibrary*>(pl);
   ASSERT(pc);
   return *pc;
}

SpecLibrary* psgLibraryManager::GetSpecLibrary()
{
   libILibrary* pl = GetLibrary(m_SpecLibIdx);
   SpecLibrary* pc = dynamic_cast<SpecLibrary*>(pl);
   ASSERT(pc);
   return pc;
}

const SpecLibrary* psgLibraryManager::GetSpecLibrary() const
{
   const libILibrary* pl = GetLibrary(m_SpecLibIdx);
   const SpecLibrary* pc = dynamic_cast<const SpecLibrary*>(pl);
   ASSERT(pc);
   return pc;
}

LiveLoadLibrary* psgLibraryManager::GetLiveLoadLibrary()
{
   libILibrary* pl = GetLibrary(m_LiveLibIdx);
   LiveLoadLibrary* pc = dynamic_cast<LiveLoadLibrary*>(pl);
   ASSERT(pc);
   return pc;
}

const LiveLoadLibrary* psgLibraryManager::GetLiveLoadLibrary() const
{
   const libILibrary* pl = GetLibrary(m_LiveLibIdx);
   const LiveLoadLibrary* pc = dynamic_cast<const LiveLoadLibrary*>(pl);
   ASSERT(pc);
   return pc; 
}

bool psgLibraryManager::LoadMe(sysIStructuredLoad* pLoad)
{
   bool bResult = libLibraryManager::LoadMe(pLoad);
   return bResult;
}

void psgLibraryManager::UpdateLibraryNames()
{
   // Some libraries have been renamed. So as not to mess up the file format
   // simply change the display name
   SpecLibrary* pLib = GetSpecLibrary();
   pLib->SetDisplayName("Project Criteria");

   ConcreteLibrary& ConcreteLib = GetConcreteLibrary();
   ConcreteLib.SetDisplayName("Concrete");

   LiveLoadLibrary* LiveLoadLib = GetLiveLoadLibrary();
   LiveLoadLib->SetDisplayName("Vehicular Live Load");
}

void psgLibraryManager::SetMasterLibraryInfo(const char* strPublisher,const char* strLibFile)
{
   m_strPublisher = strPublisher;
   m_strLibFile   = strLibFile;
}

void psgLibraryManager::GetMasterLibraryInfo(std::string& strPublisher,std::string& strLibFile) const
{
   strPublisher = m_strPublisher;
   strLibFile = m_strLibFile;
}

//======================== INQUIRY    =======================================

////////////////////////// PROTECTED  ///////////////////////////////////////

//======================== LIFECYCLE  =======================================
//======================== OPERATORS  =======================================
//======================== OPERATIONS =======================================
//======================== ACCESS     =======================================
//======================== INQUIRY    =======================================

////////////////////////// PRIVATE    ///////////////////////////////////////

//======================== LIFECYCLE  =======================================
//======================== OPERATORS  =======================================
//======================== OPERATIONS =======================================
//======================== ACCESS     =======================================
//======================== INQUERY    =======================================

//======================== DEBUG      =======================================
#if defined _DEBUG
bool psgLibraryManager::AssertValid() const
{
   return libLibraryManager::AssertValid();
}

void psgLibraryManager::Dump(dbgDumpContext& os) const
{
   os << "Dump for psgLibraryManager"<<endl;

   os << " m_ConcLibIdx = "<< m_ConcLibIdx << endl;
   os << " m_ConnLibIdx = "<< m_ConnLibIdx << endl;
   os << " m_GirdLibIdx = "<< m_GirdLibIdx << endl;
   os << " m_DiapLibIdx = "<< m_DiapLibIdx << endl;
   os << " m_BarrLibIdx = "<< m_BarrLibIdx << endl;
   os << " m_SpecLibIdx = "<< m_SpecLibIdx << endl;

   libLibraryManager::Dump( os );
}
#endif // _DEBUG

#if defined UNIT_TEST
bool psgLibraryManager::TestMe(dbgLog& rlog)
{
   TESTME_PROLOGUE("psgLibraryManager");

   // Create a library manager to be used through the testing procedure
   psgLibraryManager libmgr;

   // Test the library specific accessor methods
   ConcreteLibrary& conc_lib = libmgr.GetConcreteLibrary();
   const type_info& conc_lib_ti = typeid( conc_lib );
   TRY_TESTME( std::string( conc_lib_ti.name() ) == std::string("libLibrary<ConcreteLibraryEntry>") );

   ConnectionLibrary& conn_lib = libmgr.GetConnectionLibrary();
   const type_info& conn_lib_ti = typeid( conn_lib );
   TRY_TESTME( std::string( conc_lib_ti.name() ) == std::string("libLibrary<ConnectionLibraryEntry>") );

   GirderLibrary& gdr_lib = libmgr.GetGirderLibrary();
   const type_info& gdr_lib_ti = typeid( gdr_lib );
   TRY_TESTME( std::string( gdr_lib_ti.name() ) == std::string("libLibrary<GirderLibraryEntry>") );

   DiaphragmLayoutLibrary& diaph_lib = libmgr.GetDiaphragmLayoutLibrary();
   const type_info& diaph_lib_ti = typeid( diaph_lib );
   TRY_TESTME( std::string( diaph_lib_ti.name() ) == std::string("libLibrary<DiaphragmLayoutEntry>") );

   SpecLibrary* spec_lib = libmgr.GetSpecLibrary();
   const type_info& spec_lib_ti = typeid( *spec_lib );
   TRY_TESTME( std::string( spec_lib_ti.name() ) == std::string("libLibrary<SpecLibraryEntry>") );

   TESTME_EPILOG("LibraryManager");
}
#endif // UNIT_TEST
