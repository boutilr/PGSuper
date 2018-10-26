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

#include "PGSuperAppPlugin\stdafx.h"
#include "InsertDeleteLoad.h"
#include "PGSuperDoc.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////
//////////////////////////////////////////
//////////////////////////////////////////

txnInsertPointLoad::txnInsertPointLoad(const CPointLoadData& loadData)
{
   m_LoadIdx = Uint32_Max;
   m_LoadData = loadData;
}

std::_tstring txnInsertPointLoad::Name() const
{
   return _T("Insert Point Load");
}

txnTransaction* txnInsertPointLoad::CreateClone() const
{
   return new txnInsertPointLoad(m_LoadData);
}

bool txnInsertPointLoad::IsUndoable()
{
   return true;
}

bool txnInsertPointLoad::IsRepeatable()
{
   return false;
}

bool txnInsertPointLoad::Execute()
{
   CComPtr<IBroker> pBroker;
   EAFGetBroker(&pBroker);
   GET_IFACE2(pBroker,IEvents,pEvents);
   pEvents->HoldEvents();

   GET_IFACE2(pBroker,IUserDefinedLoadData, pUdl);
   m_LoadIdx = pUdl->AddPointLoad(m_LoadData);

   pEvents->FirePendingEvents();

   return true;
}

void txnInsertPointLoad::Undo()
{
   CComPtr<IBroker> pBroker;
   EAFGetBroker(&pBroker);

   GET_IFACE2(pBroker,IEvents,pEvents);
   pEvents->HoldEvents();

   GET_IFACE2(pBroker,IUserDefinedLoadData, pUdl);
   pUdl->DeletePointLoad(m_LoadIdx);

   pEvents->FirePendingEvents();
}

///////////////////////////////////////////////

txnDeletePointLoad::txnDeletePointLoad(Uint32 loadIdx)
{
   m_LoadIdx = loadIdx;
}

std::_tstring txnDeletePointLoad::Name() const
{
   return _T("Delete Point Load");
}

txnTransaction* txnDeletePointLoad::CreateClone() const
{
   return new txnDeletePointLoad(m_LoadIdx);
}

bool txnDeletePointLoad::IsUndoable()
{
   return true;
}

bool txnDeletePointLoad::IsRepeatable()
{
   return false;
}

bool txnDeletePointLoad::Execute()
{
   CComPtr<IBroker> pBroker;
   EAFGetBroker(&pBroker);
   GET_IFACE2(pBroker,IEvents,pEvents);
   pEvents->HoldEvents();

   GET_IFACE2(pBroker,IUserDefinedLoadData, pUdl);
   m_LoadData = pUdl->GetPointLoad(m_LoadIdx);
   pUdl->DeletePointLoad(m_LoadIdx);

   pEvents->FirePendingEvents();

   return true;
}

void txnDeletePointLoad::Undo()
{
   CComPtr<IBroker> pBroker;
   EAFGetBroker(&pBroker);

   GET_IFACE2(pBroker,IEvents,pEvents);
   pEvents->HoldEvents();

   GET_IFACE2(pBroker,IUserDefinedLoadData, pUdl);
   m_LoadIdx = pUdl->AddPointLoad(m_LoadData);

   pEvents->FirePendingEvents();
}

///////////////////////////////////////////////

txnEditPointLoad::txnEditPointLoad(Uint32 loadIdx,const CPointLoadData& oldLoadData,const CPointLoadData& newLoadData)
{
   m_LoadIdx = loadIdx;
   m_LoadData[0] = oldLoadData;
   m_LoadData[1] = newLoadData;
}

std::_tstring txnEditPointLoad::Name() const
{
   return _T("Edit Point Load");
}

txnTransaction* txnEditPointLoad::CreateClone() const
{
   return new txnEditPointLoad(m_LoadIdx,m_LoadData[0],m_LoadData[1]);
}

bool txnEditPointLoad::Execute()
{
   DoExecute(1);
   return true;
}

void txnEditPointLoad::Undo()
{
   DoExecute(0);
}

bool txnEditPointLoad::IsUndoable()
{
   return true;
}

bool txnEditPointLoad::IsRepeatable()
{
   return true;
}

void txnEditPointLoad::DoExecute(int i)
{
   CComPtr<IBroker> pBroker;
   EAFGetBroker(&pBroker);
   GET_IFACE2(pBroker,IEvents,pEvents);
   pEvents->HoldEvents();

   GET_IFACE2(pBroker,IUserDefinedLoadData, pUdl);
   pUdl->UpdatePointLoad(m_LoadIdx,m_LoadData[i]);

   pEvents->FirePendingEvents();
}

//////////////////////////////////////////
//////////////////////////////////////////
//////////////////////////////////////////

txnInsertDistributedLoad::txnInsertDistributedLoad(const CDistributedLoadData& loadData)
{
   m_LoadIdx = Uint32_Max;
   m_LoadData = loadData;
}

std::_tstring txnInsertDistributedLoad::Name() const
{
   return _T("Insert Distributed Load");
}

txnTransaction* txnInsertDistributedLoad::CreateClone() const
{
   return new txnInsertDistributedLoad(m_LoadData);
}

bool txnInsertDistributedLoad::IsUndoable()
{
   return true;
}

bool txnInsertDistributedLoad::IsRepeatable()
{
   return false;
}

bool txnInsertDistributedLoad::Execute()
{
   CComPtr<IBroker> pBroker;
   EAFGetBroker(&pBroker);
   GET_IFACE2(pBroker,IEvents,pEvents);
   pEvents->HoldEvents();

   GET_IFACE2(pBroker,IUserDefinedLoadData, pUdl);
   m_LoadIdx = pUdl->AddDistributedLoad(m_LoadData);

   pEvents->FirePendingEvents();

   return true;
}

void txnInsertDistributedLoad::Undo()
{
   CComPtr<IBroker> pBroker;
   EAFGetBroker(&pBroker);

   GET_IFACE2(pBroker,IEvents,pEvents);
   pEvents->HoldEvents();

   GET_IFACE2(pBroker,IUserDefinedLoadData, pUdl);
   pUdl->DeleteDistributedLoad(m_LoadIdx);

   pEvents->FirePendingEvents();
}

///////////////////////////////////////////////

txnDeleteDistributedLoad::txnDeleteDistributedLoad(Uint32 loadIdx)
{
   m_LoadIdx = loadIdx;
}

std::_tstring txnDeleteDistributedLoad::Name() const
{
   return _T("Delete Distributed Load");
}

txnTransaction* txnDeleteDistributedLoad::CreateClone() const
{
   return new txnDeleteDistributedLoad(m_LoadIdx);
}

bool txnDeleteDistributedLoad::IsUndoable()
{
   return true;
}

bool txnDeleteDistributedLoad::IsRepeatable()
{
   return false;
}

bool txnDeleteDistributedLoad::Execute()
{
   CComPtr<IBroker> pBroker;
   EAFGetBroker(&pBroker);

   GET_IFACE2(pBroker,IEvents,pEvents);
   pEvents->HoldEvents();

   GET_IFACE2(pBroker,IUserDefinedLoadData, pUdl);
   m_LoadData = pUdl->GetDistributedLoad(m_LoadIdx);
   pUdl->DeleteDistributedLoad(m_LoadIdx);

   pEvents->FirePendingEvents();

   return true;
}

void txnDeleteDistributedLoad::Undo()
{
   CComPtr<IBroker> pBroker;
   EAFGetBroker(&pBroker);

   GET_IFACE2(pBroker,IEvents,pEvents);
   pEvents->HoldEvents();

   GET_IFACE2(pBroker,IUserDefinedLoadData, pUdl);
   m_LoadIdx = pUdl->AddDistributedLoad(m_LoadData);

   pEvents->FirePendingEvents();
}

///////////////////////////////////////////////

txnEditDistributedLoad::txnEditDistributedLoad(Uint32 loadIdx,const CDistributedLoadData& oldLoadData,const CDistributedLoadData& newLoadData)
{
   m_LoadIdx = loadIdx;
   m_LoadData[0] = oldLoadData;
   m_LoadData[1] = newLoadData;
}

std::_tstring txnEditDistributedLoad::Name() const
{
   return _T("Edit Distributed Load");
}

txnTransaction* txnEditDistributedLoad::CreateClone() const
{
   return new txnEditDistributedLoad(m_LoadIdx,m_LoadData[0],m_LoadData[1]);
}

bool txnEditDistributedLoad::Execute()
{
   DoExecute(1);
   return true;
}

void txnEditDistributedLoad::Undo()
{
   DoExecute(0);
}

bool txnEditDistributedLoad::IsUndoable()
{
   return true;
}

bool txnEditDistributedLoad::IsRepeatable()
{
   return true;
}

void txnEditDistributedLoad::DoExecute(int i)
{
   CComPtr<IBroker> pBroker;
   EAFGetBroker(&pBroker);
   GET_IFACE2(pBroker,IEvents,pEvents);
   pEvents->HoldEvents();

   GET_IFACE2(pBroker,IUserDefinedLoadData, pUdl);
   pUdl->UpdateDistributedLoad(m_LoadIdx,m_LoadData[i]);

   pEvents->FirePendingEvents();
}

//////////////////////////////////////////
//////////////////////////////////////////
//////////////////////////////////////////

txnInsertMomentLoad::txnInsertMomentLoad(const CMomentLoadData& loadData)
{
   m_LoadIdx = Uint32_Max;
   m_LoadData = loadData;
}

std::_tstring txnInsertMomentLoad::Name() const
{
   return _T("Insert Moment Load");
}

txnTransaction* txnInsertMomentLoad::CreateClone() const
{
   return new txnInsertMomentLoad(m_LoadData);
}

bool txnInsertMomentLoad::IsUndoable()
{
   return true;
}

bool txnInsertMomentLoad::IsRepeatable()
{
   return false;
}

bool txnInsertMomentLoad::Execute()
{
   CComPtr<IBroker> pBroker;
   EAFGetBroker(&pBroker);
   GET_IFACE2(pBroker,IEvents,pEvents);
   pEvents->HoldEvents();

   GET_IFACE2(pBroker,IUserDefinedLoadData, pUdl);
   m_LoadIdx = pUdl->AddMomentLoad(m_LoadData);

   pEvents->FirePendingEvents();

   return true;
}

void txnInsertMomentLoad::Undo()
{
   CComPtr<IBroker> pBroker;
   EAFGetBroker(&pBroker);

   GET_IFACE2(pBroker,IEvents,pEvents);
   pEvents->HoldEvents();

   GET_IFACE2(pBroker,IUserDefinedLoadData, pUdl);
   pUdl->DeleteMomentLoad(m_LoadIdx);

   pEvents->FirePendingEvents();
}

///////////////////////////////////////////////

txnDeleteMomentLoad::txnDeleteMomentLoad(Uint32 loadIdx)
{
   m_LoadIdx = loadIdx;
}

std::_tstring txnDeleteMomentLoad::Name() const
{
   return _T("Delete Moment Load");
}

txnTransaction* txnDeleteMomentLoad::CreateClone() const
{
   return new txnDeleteMomentLoad(m_LoadIdx);
}

bool txnDeleteMomentLoad::IsUndoable()
{
   return true;
}

bool txnDeleteMomentLoad::IsRepeatable()
{
   return false;
}

bool txnDeleteMomentLoad::Execute()
{
   CComPtr<IBroker> pBroker;
   EAFGetBroker(&pBroker);
   GET_IFACE2(pBroker,IEvents,pEvents);
   pEvents->HoldEvents();

   GET_IFACE2(pBroker,IUserDefinedLoadData, pUdl);
   m_LoadData = pUdl->GetMomentLoad(m_LoadIdx);
   pUdl->DeleteMomentLoad(m_LoadIdx);

   pEvents->FirePendingEvents();

   return true;
}

void txnDeleteMomentLoad::Undo()
{
   CComPtr<IBroker> pBroker;
   EAFGetBroker(&pBroker);

   GET_IFACE2(pBroker,IEvents,pEvents);
   pEvents->HoldEvents();

   GET_IFACE2(pBroker,IUserDefinedLoadData, pUdl);
   m_LoadIdx = pUdl->AddMomentLoad(m_LoadData);

   pEvents->FirePendingEvents();
}

///////////////////////////////////////////////

txnEditMomentLoad::txnEditMomentLoad(Uint32 loadIdx,const CMomentLoadData& oldLoadData,const CMomentLoadData& newLoadData)
{
   m_LoadIdx = loadIdx;
   m_LoadData[0] = oldLoadData;
   m_LoadData[1] = newLoadData;
}

std::_tstring txnEditMomentLoad::Name() const
{
   return _T("Edit Moment Load");
}

txnTransaction* txnEditMomentLoad::CreateClone() const
{
   return new txnEditMomentLoad(m_LoadIdx,m_LoadData[0],m_LoadData[1]);
}

bool txnEditMomentLoad::Execute()
{
   DoExecute(1);
   return true;
}

void txnEditMomentLoad::Undo()
{
   DoExecute(0);
}

bool txnEditMomentLoad::IsUndoable()
{
   return true;
}

bool txnEditMomentLoad::IsRepeatable()
{
   return true;
}

void txnEditMomentLoad::DoExecute(int i)
{
   CComPtr<IBroker> pBroker;
   EAFGetBroker(&pBroker);
   GET_IFACE2(pBroker,IEvents,pEvents);
   pEvents->HoldEvents();

   GET_IFACE2(pBroker,IUserDefinedLoadData, pUdl);
   pUdl->UpdateMomentLoad(m_LoadIdx,m_LoadData[i]);

   pEvents->FirePendingEvents();
}

