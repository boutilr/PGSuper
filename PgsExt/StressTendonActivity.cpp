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

#include <PgsExt\PgsExtLib.h>
#include <PgsExt\StressTendonActivity.h>

///////////////////////////////////////////////////////////////////////

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CStressTendonActivity::CStressTendonActivity()
{
   m_bEnabled = false;
}

CStressTendonActivity::CStressTendonActivity(const CStressTendonActivity& rOther)
{
   MakeCopy(rOther);
}

CStressTendonActivity::~CStressTendonActivity()
{
}

CStressTendonActivity& CStressTendonActivity::operator= (const CStressTendonActivity& rOther)
{
   if( this != &rOther )
   {
      MakeAssignment(rOther);
   }

   return *this;
}

bool CStressTendonActivity::operator==(const CStressTendonActivity& rOther) const
{
   if ( m_bEnabled != rOther.m_bEnabled )
      return false;

   if ( m_Tendons != rOther.m_Tendons )
      return false;

   return true;
}

bool CStressTendonActivity::operator!=(const CStressTendonActivity& rOther) const
{
   return !operator==(rOther);
}

void CStressTendonActivity::Enable(bool bEnable)
{
   m_bEnabled = bEnable;
}

bool CStressTendonActivity::IsEnabled() const
{
   return m_bEnabled;
}

void CStressTendonActivity::Clear()
{
   m_Tendons.clear();
}

void CStressTendonActivity::AddTendon(const CGirderKey& girderKey,DuctIndexType ductIdx)
{
   m_Tendons.insert(CTendonKey(girderKey,ductIdx));
}

void CStressTendonActivity::AddTendon(const CTendonKey& tendonKey)
{
   m_Tendons.insert(tendonKey);
}

void CStressTendonActivity::AddTendons(const std::set<CTendonKey>& tendons)
{
   m_Tendons.insert(tendons.begin(),tendons.end());
}

void CStressTendonActivity::RemoveTendon(const CGirderKey& girderKey,DuctIndexType ductIdx)
{
   CTendonKey key(girderKey,ductIdx);
   std::set<CTendonKey>::iterator found(m_Tendons.find(key));
   if ( found != m_Tendons.end() )
   {
      m_Tendons.erase(found);
   }
   else
   {
      ATLASSERT(false); // not found???
   }
}

bool CStressTendonActivity::IsTendonStressed(const CGirderKey& girderKey,DuctIndexType ductIdx) const
{
   CTendonKey key(girderKey,ductIdx);
   std::set<CTendonKey>::const_iterator found(m_Tendons.find(key));
   if ( found == m_Tendons.end() )
   {
      return false;
   }
   else
   {
      return true;
   }
}

bool CStressTendonActivity::IsTendonStressed() const
{
   return (m_Tendons.size() != 0 ? true : false);
}

const std::set<CTendonKey>& CStressTendonActivity::GetTendons() const
{
   return m_Tendons;
}

IndexType CStressTendonActivity::GetTendonCount() const
{
   return m_Tendons.size();
}

HRESULT CStressTendonActivity::Load(IStructuredLoad* pStrLoad,IProgress* pProgress)
{
   CHRException hr;

   try
   {
      hr = pStrLoad->BeginUnit(_T("StressTendons"));

      CComVariant var;
      var.vt = VT_BOOL;
      hr = pStrLoad->get_Property(_T("Enabled"),&var);
      m_bEnabled = (var.boolVal == VARIANT_TRUE ? true : false);

      if ( m_bEnabled )
      {
         CollectionIndexType nTendons;
         var.vt = VT_INDEX;
         hr = pStrLoad->get_Property(_T("Count"),&var);
         nTendons = VARIANT2INDEX(var);
         m_Tendons.clear();

         for ( CollectionIndexType i = 0; i < nTendons; i++ )
         {
            pStrLoad->BeginUnit(_T("Tendon"));
            CGirderKey girderKey;
            girderKey.Load(pStrLoad,pProgress);

            var.vt = VT_INDEX;
            pStrLoad->get_Property(_T("DuctIndex"),&var);
            DuctIndexType ductIdx = VARIANT2INDEX(var);

            m_Tendons.insert(CTendonKey(girderKey,ductIdx));

            pStrLoad->EndUnit(); // Tendon
         }
      }

      pStrLoad->EndUnit();
   }
   catch (HRESULT)
   {
      ATLASSERT(false);
      THROW_LOAD(InvalidFileFormat,pStrLoad);
   };

   return S_OK;
}

HRESULT CStressTendonActivity::Save(IStructuredSave* pStrSave,IProgress* pProgress)
{
   pStrSave->BeginUnit(_T("StressTendons"),1.0);
   pStrSave->put_Property(_T("Enabled"),CComVariant(m_bEnabled));

   if ( m_bEnabled )
   {
      pStrSave->put_Property(_T("Count"),CComVariant(m_Tendons.size()));

      std::set<CTendonKey>::iterator iter(m_Tendons.begin());
      std::set<CTendonKey>::iterator iterEnd(m_Tendons.end());
      for ( ; iter != iterEnd; iter++ )
      {
         CTendonKey& key = *iter;
         pStrSave->BeginUnit(_T("Tendon"),1.0);
         key.girderKey.Save(pStrSave,pProgress);
         pStrSave->put_Property(_T("DuctIndex"),CComVariant(key.ductIdx));
         pStrSave->EndUnit(); // Tendon
      }
   }

   pStrSave->EndUnit(); // Stress Tendons

   return S_OK;
}

void CStressTendonActivity::MakeCopy(const CStressTendonActivity& rOther)
{
   m_bEnabled  = rOther.m_bEnabled;
   m_Tendons   = rOther.m_Tendons;
}

void CStressTendonActivity::MakeAssignment(const CStressTendonActivity& rOther)
{
   MakeCopy(rOther);
}
