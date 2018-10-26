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

#include <PgsExt\PgsExtLib.h>
#include <PgsExt\ShearZoneData.h>
#include <Units\SysUnits.h>
#include <StdIo.h>
#include <StrData.cpp>
#include <comdef.h> // for _variant_t

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/****************************************************************************
CLASS
   CShearZoneData
****************************************************************************/


////////////////////////// PUBLIC     ///////////////////////////////////////

//======================== LIFECYCLE  =======================================
CShearZoneData::CShearZoneData():
ZoneNum(0),
BarSpacing(0),
ZoneLength(),
VertBarSize(matRebar::bs3),
HorzBarSize(matRebar::bs3),
nVertBars(2),
nHorzBars(2)
{

}

CShearZoneData::CShearZoneData(Uint32 zoneNum,matRebar::Size barSize,Float64 barSpacing,Float64 zoneLength,Uint32 nbrLegs):
ZoneNum(zoneNum),
VertBarSize(barSize),
HorzBarSize(barSize),
BarSpacing(barSpacing),
ZoneLength(zoneLength),
nVertBars(nbrLegs),
nHorzBars(nbrLegs)
{

}

CShearZoneData::CShearZoneData(const CShearZoneData& rOther)
{
   MakeCopy(rOther);
}

CShearZoneData::~CShearZoneData()
{
}

//======================== OPERATORS  =======================================
CShearZoneData& CShearZoneData::operator= (const CShearZoneData& rOther)
{
   if( this != &rOther )
   {
      MakeAssignment(rOther);
   }

   return *this;
}

bool CShearZoneData::operator == (const CShearZoneData& rOther) const
{
   if ( ZoneNum != rOther.ZoneNum )
      return false;

   if ( !IsEqual(BarSpacing, rOther.BarSpacing) )
      return false;

   if ( !IsEqual(ZoneLength, rOther.ZoneLength ))
      return false;

   if ( VertBarSize != rOther.VertBarSize )
      return false;

   if ( nVertBars != rOther.nVertBars )
      return false;

   if ( HorzBarSize != rOther.HorzBarSize )
      return false;

   if ( nHorzBars != rOther.nHorzBars )
      return false;

   return true;
}

bool CShearZoneData::operator != (const CShearZoneData& rOther) const
{
   return !operator==( rOther );
}

//======================== OPERATIONS =======================================
HRESULT CShearZoneData::Load(IStructuredLoad* pStrLoad,IProgress* pProgress)
{
   HRESULT hr = S_OK;

   if ( SUCCEEDED(pStrLoad->BeginUnit(_T("ShearZoneData"))) )
   {
      Float64 version;
      pStrLoad->get_Version(&version);
      if ( 3.0 < version )
         return STRLOAD_E_BADVERSION;

      _variant_t var;
      if ( version < 2 )
      {
         var.vt = VT_I4;
         if ( FAILED(pStrLoad->get_Property(_T("ZoneNum"),&var)) )
            return STRLOAD_E_INVALIDFORMAT;
         else
            ZoneNum = var.lVal;

         var.vt = VT_I2;
         BarSizeType key;
         if ( FAILED(pStrLoad->get_Property(_T("BarSize"),&var)) )
            return STRLOAD_E_INVALIDFORMAT;
         else
            key = var.iVal;

         matRebar::Grade grade;
         matRebar::Type type;
         lrfdRebarPool::MapOldRebarKey(key,grade,type,VertBarSize);

         var.vt = VT_R8;
         if ( FAILED(pStrLoad->get_Property(_T("BarSpacing"),&var)) )
            return STRLOAD_E_INVALIDFORMAT;
         else
            BarSpacing = var.dblVal;

         var.vt = VT_R8;
         if ( FAILED(pStrLoad->get_Property(_T("ZoneLength"),&var)) )
            return STRLOAD_E_INVALIDFORMAT;
         else
            ZoneLength = var.dblVal;

         if ( 1.1 <= version )
         {
            var.vt = VT_I4;
            if ( FAILED(pStrLoad->get_Property(_T("NbrLegs"),&var)) )
               return STRLOAD_E_INVALIDFORMAT;
            else
               nVertBars = var.lVal;
         }
         else
         {
            nVertBars = 2;
         }

         HorzBarSize = matRebar::bsNone;
         nHorzBars   = 2;
      }
      else
      {
         var.vt = VT_I4;
         if ( FAILED(pStrLoad->get_Property(_T("ZoneNum"),&var)) )
            return STRLOAD_E_INVALIDFORMAT;
         else
            ZoneNum = var.lVal;

         var.vt = VT_R8;
         if ( FAILED(pStrLoad->get_Property(_T("ZoneLength"),&var)) )
            return STRLOAD_E_INVALIDFORMAT;
         else
            ZoneLength = var.dblVal;

         var.vt = VT_R8;
         if ( FAILED(pStrLoad->get_Property(_T("BarSpacing"),&var)) )
            return STRLOAD_E_INVALIDFORMAT;
         else
            BarSpacing = var.dblVal;

         if ( version < 3 )
         {
            var.vt = VT_I2;
            BarSizeType key;
            if ( FAILED(pStrLoad->get_Property(_T("VertBarSize"),&var)) )
               return STRLOAD_E_INVALIDFORMAT;
            else
               key = var.iVal;
         
            matRebar::Grade grade;
            matRebar::Type type;
            lrfdRebarPool::MapOldRebarKey(key,grade,type,VertBarSize);
         }
         else
         {
            var.vt = VT_I4;
            if ( FAILED(pStrLoad->get_Property(_T("VertBarSize"),&var)) )
               return STRLOAD_E_INVALIDFORMAT;
            else
               VertBarSize = matRebar::Size(var.lVal);
         }

         var.vt = VT_I4;
         if ( FAILED(pStrLoad->get_Property(_T("VertBars"),&var)) )
            return STRLOAD_E_INVALIDFORMAT;
         else
            nVertBars = var.lVal;

         if ( version < 3 )
         {
            var.vt = VT_I2;
            Int32 key;
            if ( FAILED(pStrLoad->get_Property(_T("HorzBarSize"),&var)) )
               return STRLOAD_E_INVALIDFORMAT;
            else
               key = var.iVal;
         
            matRebar::Grade grade;
            matRebar::Type type;
            lrfdRebarPool::MapOldRebarKey(key,grade,type,HorzBarSize);
         }
         else
         {
            if ( FAILED(pStrLoad->get_Property(_T("HorzBarSize"),&var)) )
               return STRLOAD_E_INVALIDFORMAT;
            else
               HorzBarSize = matRebar::Size(var.lVal);
         }

         var.vt = VT_I4;
         if ( FAILED(pStrLoad->get_Property(_T("HorzBars"),&var)) )
            return STRLOAD_E_INVALIDFORMAT;
         else
            nHorzBars = var.lVal;
      }

      if ( FAILED(pStrLoad->EndUnit()) )
         return STRLOAD_E_INVALIDFORMAT;
   }
   else
   {
      return E_FAIL;
   }


   return hr;
}

HRESULT CShearZoneData::Save(IStructuredSave* pStrSave,IProgress* pProgress)
{
   HRESULT hr = S_OK;

   pStrSave->BeginUnit(_T("ShearZoneData"),3.0);
   pStrSave->put_Property(_T("ZoneNum"),    _variant_t(ZoneNum) );
   pStrSave->put_Property(_T("ZoneLength"), _variant_t(ZoneLength) );
   pStrSave->put_Property(_T("BarSpacing"), _variant_t(BarSpacing) );
   pStrSave->put_Property(_T("VertBarSize"),_variant_t((long)VertBarSize) );
   pStrSave->put_Property(_T("VertBars"),   _variant_t(nVertBars) );
   pStrSave->put_Property(_T("HorzBarSize"),_variant_t((long)HorzBarSize) );
   pStrSave->put_Property(_T("HorzBars"),   _variant_t(nHorzBars) );
   pStrSave->EndUnit();

   return hr;
}
//======================== ACCESS     =======================================
//======================== INQUIRY    =======================================

////////////////////////// PROTECTED  ///////////////////////////////////////

//======================== LIFECYCLE  =======================================
//======================== OPERATORS  =======================================
//======================== OPERATIONS =======================================
void CShearZoneData::MakeCopy(const CShearZoneData& rOther)
{
   ZoneNum       = rOther.ZoneNum;
   BarSpacing    = rOther.BarSpacing;
   ZoneLength    = rOther.ZoneLength;
   VertBarSize   = rOther.VertBarSize;
   HorzBarSize   = rOther.HorzBarSize;
   nVertBars     = rOther.nVertBars;
   nHorzBars     = rOther.nHorzBars;
}

void CShearZoneData::MakeAssignment(const CShearZoneData& rOther)
{
   MakeCopy( rOther );
}

//======================== ACCESS     =======================================
//======================== INQUIRY    =======================================

////////////////////////// PRIVATE    ///////////////////////////////////////

//======================== LIFECYCLE  =======================================
//======================== OPERATORS  =======================================
//======================== OPERATIONS =======================================
//======================== ACCESS     =======================================
//======================== INQUERY    =======================================

//======================== DEBUG      =======================================
