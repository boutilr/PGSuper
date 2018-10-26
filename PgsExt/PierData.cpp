///////////////////////////////////////////////////////////////////////
// PGSuper - Prestressed Girder SUPERstructure Design and Analysis
// Copyright � 1999-2013  Washington State Department of Transportation
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
#include <PgsExt\PierData.h>
#include <PgsExt\SpanData.h>
#include <PgsExt\BridgeDescription.h>

#include <EAF\EAFUtilities.h>
#include <IFace\Project.h>

#include <Units\SysUnits.h>
#include <StdIo.h>
#include <StrData.cpp>
#include <WBFLCogo.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/****************************************************************************
CLASS
   CPierData
****************************************************************************/

// old data storage format
//BEGIN_STRSTORAGEMAP(CPierData,_T("PierData"),1.0)
//   PROPERTY(_T("Station"),     SDT_R8, m_Station )
//   PROPERTY(_T("Orientation"), SDT_I4, Orientation )
//   PROPERTY(_T("Angle"),       SDT_R8, Angle )
//   PROPERTY(_T("Connection"),  SDT_STDSTRING, m_Connection[pgsTypes::Back] )
//END_STRSTORAGEMAP

CPierData::CPierData()
{
   m_PierIdx = INVALID_INDEX;
   m_pPrevSpan = NULL;
   m_pNextSpan = NULL;

   m_pBridgeDesc = NULL;

   m_Station     = 0.0;
   Orientation = Normal;
   Angle       = 0.0;

   m_ConnectionType = pgsTypes::Hinged;
   
   m_strOrientation = _T("Normal");

    for ( int i = 0; i < 2; i++ )
   {
      m_GirderEndDistance[i]            = ::ConvertToSysUnits(6.0,unitMeasure::Inch);
      m_EndDistanceMeasurementType[i]   = ConnectionLibraryEntry::FromBearingNormalToPier;

      m_GirderBearingOffset[i]          = ::ConvertToSysUnits(1.0,unitMeasure::Feet);
      m_BearingOffsetMeasurementType[i] = ConnectionLibraryEntry::NormalToPier;

      m_SupportWidth[i]                 = ::ConvertToSysUnits(1.0,unitMeasure::Feet);

      m_DiaphragmHeight[i] = 0;
      m_DiaphragmWidth[i] = 0;
      m_DiaphragmLoadType[i] = ConnectionLibraryEntry::DontApply;
      m_DiaphragmLoadLocation[i] = 0;
   }

    m_DistributionFactorsFromOlderVersion = false;
}

CPierData::CPierData(const CPierData& rOther)
{
   m_pPrevSpan = NULL;
   m_pNextSpan = NULL;

   m_pBridgeDesc = NULL;

   m_Station   = 0.0;
   Orientation = Normal;
   Angle       = 0.0;
   
   m_strOrientation = _T("Normal");

   MakeCopy(rOther);
}

CPierData::~CPierData()
{
}

CPierData& CPierData::operator= (const CPierData& rOther)
{
   if( this != &rOther )
   {
      MakeAssignment(rOther);
   }

   return *this;
}

bool CPierData::operator==(const CPierData& rOther) const
{
   if ( m_PierIdx != rOther.m_PierIdx )
      return false;

   if ( m_Station != rOther.m_Station )
      return false;

   if ( m_strOrientation != rOther.m_strOrientation )
      return false;

   if ( m_ConnectionType != rOther.m_ConnectionType )
      return false;

   for ( int i = 0; i < 2; i++ )
   {
      if ( !IsEqual(m_GirderEndDistance[i], rOther.m_GirderEndDistance[i]) )
         return false;

      if ( m_EndDistanceMeasurementType[i] != rOther.m_EndDistanceMeasurementType[i] )
         return false;

      if ( !IsEqual(m_GirderBearingOffset[i], rOther.m_GirderBearingOffset[i]) )
         return false;

      if ( m_BearingOffsetMeasurementType[i] != rOther.m_BearingOffsetMeasurementType[i] )
         return false;

      if ( !IsEqual(m_SupportWidth[i], rOther.m_SupportWidth[i]) )
         return false;

      if ( !IsEqual(m_DiaphragmHeight[i], rOther.m_DiaphragmHeight[i]) )
         return false;
      
      if ( !IsEqual(m_DiaphragmWidth[i], rOther.m_DiaphragmWidth[i]) )
         return false;

      if ( m_DiaphragmLoadType[i] != rOther.m_DiaphragmLoadType[i] )
         return false;

      if ( !IsEqual(m_DiaphragmLoadLocation[i], rOther.m_DiaphragmLoadLocation[i]) )
         return false;
   }

   if ( m_pBridgeDesc->GetDistributionFactorMethod() == pgsTypes::DirectlyInput )
   {
      if (m_LLDFs != rOther.m_LLDFs)
         return false;
   }

   return true;
}

bool CPierData::operator!=(const CPierData& rOther) const
{
   return !operator==(rOther);
}

LPCTSTR CPierData::AsString(pgsTypes::PierConnectionType type)
{
   switch(type)
   { 
   case pgsTypes::Hinged:
      return _T("Hinged");

   case pgsTypes::Roller:
      return _T("Roller");

   case pgsTypes::ContinuousAfterDeck:
      return _T("Continuous after deck placement");

   case pgsTypes::ContinuousBeforeDeck:
      return _T("Continuous before deck placement");

   case pgsTypes::IntegralAfterDeck:
      return _T("Integral after deck placement");

   case pgsTypes::IntegralBeforeDeck:
      return _T("Integral before deck placement");

   case pgsTypes::IntegralAfterDeckHingeBack:
      return _T("Hinged on back side; Integral on ahead side after deck placement");

   case pgsTypes::IntegralBeforeDeckHingeBack:
      return _T("Hinged on back side; Integral on ahead side before deck placement");

   case pgsTypes::IntegralAfterDeckHingeAhead:
      return _T("Integral on back side after deck placement; Hinged on ahead side");

   case pgsTypes::IntegralBeforeDeckHingeAhead:
      return _T("Integral on back side before deck placement; Hinged on ahead side");
   
   default:
      ATLASSERT(0);

   };

   return _T("");
}

HRESULT CPierData::Load(IStructuredLoad* pStrLoad,IProgress* pProgress)
{
   USES_CONVERSION;
   HRESULT hr = S_OK;

   HRESULT hr2 = pStrLoad->BeginUnit(_T("PierDataDetails"));

   bool bUpdateFromLibrary = false;
   std::_tstring connectionLibName[2]; // temporary data for loading library connection name.... at end of load,
   // data will be extracted from library entries and filled into the data members of this class

   Float64 version;
   pStrLoad->get_Version(&version);
   if ( version == 1.0 )
   {
      bUpdateFromLibrary = true;
      //STRSTG_LOAD( hr, pStrLoad, pProgress );
      //BEGIN_STRSTORAGEMAP(CPierData,_T("PierData"),1.0)
      //   PROPERTY(_T("Station"),     SDT_R8, m_Station )
      //   PROPERTY(_T("Orientation"), SDT_I4, Orientation )
      //   PROPERTY(_T("Angle"),       SDT_R8, Angle )
      //   PROPERTY(_T("Connection"),  SDT_STDSTRING, m_Connection[pgsTypes::Back] )
      //END_STRSTORAGEMAP

      // replacement for structured storage map... map had to be replaced because
      // of the connection data member was eliminated in version 9
      CComVariant var;
      pStrLoad->BeginUnit(_T("PierData"));
      var.vt = VT_R8;
      if ( FAILED(pStrLoad->get_Property(_T("Station"),&var)) )
         return STRLOAD_E_INVALIDFORMAT;
      else
         m_Station = var.dblVal;

      var.vt = VT_I4;
      if ( FAILED(pStrLoad->get_Property(_T("Orientation"),&var)) )
         return STRLOAD_E_INVALIDFORMAT;
      else
         Orientation = (CPierData::PierOrientation)(var.iVal);

      var.vt = VT_R8;
      if ( FAILED(pStrLoad->get_Property(_T("Angle"),&var)) )
         return STRLOAD_E_INVALIDFORMAT;
      else
         Angle = var.dblVal;

      var.vt = VT_BSTR;
      if ( FAILED(pStrLoad->get_Property(_T("Connection"),&var)) )
         return STRLOAD_E_INVALIDFORMAT;
      else
         connectionLibName[pgsTypes::Back] = OLE2T(var.bstrVal);

      pStrLoad->EndUnit(); // PierData


      connectionLibName[pgsTypes::Ahead] = connectionLibName[pgsTypes::Back];

      m_ConnectionType  = pgsTypes::Hinged;

      // Convert old input into a bearing string
      CComPtr<IDirectionDisplayUnitFormatter> dirFormatter;
      CComPtr<IAngleDisplayUnitFormatter> angleFormatter;
      CComBSTR bstrOrientation;
      switch(Orientation)
      {
      case Normal:
         m_strOrientation = _T("Normal");
         break;

      case Skew:
         angleFormatter.CoCreateInstance(CLSID_AngleDisplayUnitFormatter);
         angleFormatter->put_CondensedFormat(VARIANT_TRUE);
         angleFormatter->put_Signed(VARIANT_FALSE);

         angleFormatter->Format(Angle,NULL,&bstrOrientation);
         m_strOrientation = OLE2T(bstrOrientation);
         break;
      
      case Bearing:
         dirFormatter.CoCreateInstance(CLSID_DirectionDisplayUnitFormatter);
         dirFormatter->put_CondensedFormat(VARIANT_TRUE);
         dirFormatter->put_BearingFormat(VARIANT_TRUE);

         dirFormatter->Format(Angle,NULL,&bstrOrientation);
         m_strOrientation = OLE2T(bstrOrientation);
         break;

      default:
         ATLASSERT(false);
         return STRLOAD_E_INVALIDFORMAT;
      }

      if ( FAILED(hr2) )
      {
         // Failed to read "PierDataDetails" block. This means the file was
         // created before changing out the COGO engine in bmfBridgeModeling.
         //
         // The reference for bearings has changed. Do a conversion here.
         if ( Orientation == Bearing )
         {
            Angle = PI_OVER_2 - Angle;
            if ( Angle < 0 )
               Angle += 2*M_PI;
         }
      }
   }
   else
   {
      // greater than version 1
      CComVariant var;
      var.vt = VT_R8;
      if ( FAILED(pStrLoad->get_Property(_T("Station"),&var)) )
         return STRLOAD_E_INVALIDFORMAT;
      else
         m_Station = var.dblVal;

#pragma Reminder("UPDATE: Need to validate string... make sure it is a valid pier orientation")
      var.Clear();
      var.vt = VT_BSTR;
      if (FAILED(pStrLoad->get_Property(_T("Orientation"), &var )) )
         return STRLOAD_E_INVALIDFORMAT;
      else
         m_strOrientation = OLE2T(var.bstrVal);

      if ( 4.0 <= version )
      {
         var.Clear();

         if ( 6.0 <= version && version < 8 )
         {
            var.vt = VT_R8;
            if ( FAILED(pStrLoad->get_Property(_T("AlignmentOffset"),&var)))
               return STRLOAD_E_INVALIDFORMAT;
//            else // removed in version 8
//               m_AlignmentOffset = var.dblVal;
         }

         // prior to version 9.0, a reference to the connection library entry was stored...
         if ( version < 9.0 )
         {
            bUpdateFromLibrary = true;

            // prior to version 7 we had left and right boundary conditions with an option to make both same
            bool use_same_both = false;
            pgsTypes::PierConnectionType back_conn_type, ahead_conn_type;
            if ( 5.0 <= version && version < 7.0 )
            {
               var.vt = VT_BOOL;
               if (FAILED(pStrLoad->get_Property(_T("UseSameConnectionOnBothSides"),&var)))
                  return STRLOAD_E_INVALIDFORMAT;
               else
                  use_same_both = (var.boolVal == VARIANT_TRUE);
            }

            var.vt = VT_BSTR;
            if (FAILED(pStrLoad->get_Property(_T("LeftConnection"), &var )) )
               return STRLOAD_E_INVALIDFORMAT;
            else
               connectionLibName[pgsTypes::Back] = OLE2T(var.bstrVal);

            if ( version < 7.0 )
            {
               var.Clear();
               var.vt = VT_I4;
               if ( FAILED(pStrLoad->get_Property(_T("LeftConnectionType"),&var)) )
                  return STRLOAD_E_INVALIDFORMAT;
               else
                  back_conn_type = (pgsTypes::PierConnectionType)var.lVal;
            }

            var.Clear();
            var.vt = VT_BSTR;
            if (FAILED(pStrLoad->get_Property(_T("RightConnection"), &var )) )
            {
               return STRLOAD_E_INVALIDFORMAT;
            }
            else
            {
               if (!use_same_both)
               {
                  connectionLibName[pgsTypes::Ahead] = OLE2T(var.bstrVal);
               }
               else
               {
                  connectionLibName[pgsTypes::Ahead] = connectionLibName[pgsTypes::Back]; 
               }
            }               

            if ( 7.0 <= version )
            {
               // After version 7.0, we have single definition for boundary conditions over pier
               var.Clear();
               var.vt = VT_I4;
               if ( FAILED(pStrLoad->get_Property(_T("ConnectionType"),&var)) )
                  return STRLOAD_E_INVALIDFORMAT;
               else
                  m_ConnectionType = (pgsTypes::PierConnectionType)var.lVal;
            }
            else
            {
               var.Clear();
               var.vt = VT_I4;
               if ( FAILED(pStrLoad->get_Property(_T("RightConnectionType"),&var)) )
                  return STRLOAD_E_INVALIDFORMAT;
               else
                  ahead_conn_type = (pgsTypes::PierConnectionType)var.lVal;

               // Pre-version 7.0, we must resolve separate bc's for ahead and back
               // Tricky: this can be ambiguous
               m_ConnectionType = back_conn_type;

               if (!use_same_both)
               {
                  // Only some conditions where different bc's made sense (main reason why we got rid of ahead/back bc concept)
                  if ( back_conn_type < 0 )
                  {
                     // there isn't a back side to this pier
                     m_ConnectionType = ahead_conn_type;
                  }
                  else if ( ahead_conn_type < 0 )
                  {
                     // there isn't an ahead side to this pier
                     m_ConnectionType = back_conn_type;
                  }
                  else if (back_conn_type == pgsTypes::ContinuousAfterDeck || ahead_conn_type == pgsTypes::ContinuousAfterDeck)
                  {
                     m_ConnectionType = pgsTypes::ContinuousAfterDeck;
                  }
                  else if (back_conn_type == pgsTypes::ContinuousBeforeDeck || ahead_conn_type == pgsTypes::ContinuousBeforeDeck)
                  {
                     m_ConnectionType = pgsTypes::ContinuousBeforeDeck;
                  }
                  else if (back_conn_type == pgsTypes::IntegralAfterDeck)
                  {
                     if (ahead_conn_type == pgsTypes::Hinged || ahead_conn_type == pgsTypes::Roller)
                     {
                        m_ConnectionType = pgsTypes::IntegralAfterDeckHingeAhead;
                     }
                  }
                  else if (back_conn_type == pgsTypes::IntegralBeforeDeck)
                  {
                     if (ahead_conn_type == pgsTypes::Hinged || ahead_conn_type == pgsTypes::Roller)
                     {
                        m_ConnectionType = pgsTypes::IntegralBeforeDeckHingeAhead;
                     }
                  }
                  else if (ahead_conn_type == pgsTypes::IntegralAfterDeck)
                  {
                     if (back_conn_type == pgsTypes::Hinged || back_conn_type == pgsTypes::Roller)
                     {
                        m_ConnectionType = pgsTypes::IntegralAfterDeckHingeBack;
                     }
                  }
                  else if (ahead_conn_type == pgsTypes::IntegralBeforeDeck)
                  {
                     if (back_conn_type == pgsTypes::Hinged || back_conn_type == pgsTypes::Roller)
                     {
                        m_ConnectionType = pgsTypes::IntegralBeforeDeckHingeBack;
                     }
                  }
               } // !use_same_both
            } // 7.0 <= version
         } // version < 9.0
         else
         {
            // 9.0 <= version
            // Starting with version 9, full connection dimensions for back and ahead sides are stored
            var.vt = VT_I4;
            hr = pStrLoad->get_Property(_T("ConnectionType"),&var);
            m_ConnectionType = (pgsTypes::PierConnectionType)var.lVal;

            hr = pStrLoad->BeginUnit(_T("Back"));

            var.vt = VT_R8;
            hr = pStrLoad->get_Property(_T("GirderEndDistance"),&var);
            m_GirderEndDistance[pgsTypes::Back] = var.dblVal;

            var.vt = VT_BSTR;
            hr = pStrLoad->get_Property(_T("EndDistanceMeasurementType"),&var);
            m_EndDistanceMeasurementType[pgsTypes::Back] = ConnectionLibraryEntry::EndDistanceMeasurementTypeFromString(OLE2T(var.bstrVal));

            var.vt = VT_R8;
            hr = pStrLoad->get_Property(_T("GirderBearingOffset"),&var);
            m_GirderBearingOffset[pgsTypes::Back] = var.dblVal;

            var.vt = VT_BSTR;
            hr = pStrLoad->get_Property(_T("BearingOffsetMeasurementType"),&var);
            m_BearingOffsetMeasurementType[pgsTypes::Back] = ConnectionLibraryEntry::BearingOffsetMeasurementTypeFromString(OLE2T(var.bstrVal));

            var.vt = VT_R8;
            hr = pStrLoad->get_Property(_T("SupportWidth"),&var);
            m_SupportWidth[pgsTypes::Back] = var.dblVal;

            {
               hr = pStrLoad->BeginUnit(_T("Diaphragm"));
               var.vt = VT_R8;
               hr = pStrLoad->get_Property(_T("DiaphragmWidth"),&var);
               m_DiaphragmWidth[pgsTypes::Back] = var.dblVal;

               hr = pStrLoad->get_Property(_T("DiaphragmHeight"),&var);
               m_DiaphragmHeight[pgsTypes::Back] = var.dblVal;

               var.vt = VT_BSTR;
               hr = pStrLoad->get_Property(_T("DiaphragmLoadType"),&var);

               std::_tstring tmp(OLE2T(var.bstrVal));
               if (tmp == _T("ApplyAtBearingCenterline"))
               {
                  m_DiaphragmLoadType[pgsTypes::Back] = ConnectionLibraryEntry::ApplyAtBearingCenterline;
               }
               else if (tmp == _T("ApplyAtSpecifiedLocation"))
               {
                  m_DiaphragmLoadType[pgsTypes::Back] = ConnectionLibraryEntry::ApplyAtSpecifiedLocation;

                  var.vt = VT_R8;
                  hr = pStrLoad->get_Property(_T("DiaphragmLoadLocation"),&var);
                  m_DiaphragmLoadLocation[pgsTypes::Back] = var.dblVal;
               }
               else if (tmp == _T("DontApply"))
               {
                  m_DiaphragmLoadType[pgsTypes::Back] = ConnectionLibraryEntry::DontApply;
               }
               else
               {
                  hr = STRLOAD_E_INVALIDFORMAT;
               }

               hr = pStrLoad->EndUnit(); // Diaphragm
            }

            hr = pStrLoad->EndUnit(); // Back

            hr = pStrLoad->BeginUnit(_T("Ahead"));

            var.vt = VT_R8;
            hr = pStrLoad->get_Property(_T("GirderEndDistance"),&var);
            m_GirderEndDistance[pgsTypes::Ahead] = var.dblVal;

            var.vt = VT_BSTR;
            hr = pStrLoad->get_Property(_T("EndDistanceMeasurementType"),&var);
            m_EndDistanceMeasurementType[pgsTypes::Ahead] = ConnectionLibraryEntry::EndDistanceMeasurementTypeFromString(OLE2T(var.bstrVal));

            var.vt = VT_R8;
            hr = pStrLoad->get_Property(_T("GirderBearingOffset"),&var);
            m_GirderBearingOffset[pgsTypes::Ahead] = var.dblVal;

            var.vt = VT_BSTR;
            hr = pStrLoad->get_Property(_T("BearingOffsetMeasurementType"),&var);
            m_BearingOffsetMeasurementType[pgsTypes::Ahead] = ConnectionLibraryEntry::BearingOffsetMeasurementTypeFromString(OLE2T(var.bstrVal));

            var.vt = VT_R8;
            hr = pStrLoad->get_Property(_T("SupportWidth"),&var);
            m_SupportWidth[pgsTypes::Ahead] = var.dblVal;

            {
               hr = pStrLoad->BeginUnit(_T("Diaphragm"));
               var.vt = VT_R8;
               hr = pStrLoad->get_Property(_T("DiaphragmWidth"),&var);
               m_DiaphragmWidth[pgsTypes::Ahead] = var.dblVal;

               hr = pStrLoad->get_Property(_T("DiaphragmHeight"),&var);
               m_DiaphragmHeight[pgsTypes::Ahead] = var.dblVal;

               var.vt = VT_BSTR;
               hr = pStrLoad->get_Property(_T("DiaphragmLoadType"),&var);

               std::_tstring tmp(OLE2T(var.bstrVal));
               if (tmp == _T("ApplyAtBearingCenterline"))
               {
                  m_DiaphragmLoadType[pgsTypes::Ahead] = ConnectionLibraryEntry::ApplyAtBearingCenterline;
               }
               else if (tmp == _T("ApplyAtSpecifiedLocation"))
               {
                  m_DiaphragmLoadType[pgsTypes::Ahead] = ConnectionLibraryEntry::ApplyAtSpecifiedLocation;

                  var.vt = VT_R8;
                  hr = pStrLoad->get_Property(_T("DiaphragmLoadLocation"),&var);
                  m_DiaphragmLoadLocation[pgsTypes::Ahead] = var.dblVal;
               }
               else if (tmp == _T("DontApply"))
               {
                  m_DiaphragmLoadType[pgsTypes::Ahead] = ConnectionLibraryEntry::DontApply;
               }
               else
               {
                  hr = STRLOAD_E_INVALIDFORMAT;
               }

               hr = pStrLoad->EndUnit(); // Diaphragm
            }
            hr = pStrLoad->EndUnit(); // Ahead
         }
      }
      else // 4.0 <= version
      {
         // version is less that 4.0

         bUpdateFromLibrary = true;

         var.Clear();
         var.vt = VT_BSTR;
         if (FAILED(pStrLoad->get_Property(_T("Connection"), &var )) )
            return STRLOAD_E_INVALIDFORMAT;
         else
            connectionLibName[pgsTypes::Back] = OLE2T(var.bstrVal);

         connectionLibName[pgsTypes::Ahead] = connectionLibName[pgsTypes::Back];

         if ( 3.0 <= version )
         {
            var.Clear();
            var.vt = VT_I4;
            if ( FAILED(pStrLoad->get_Property(_T("ConnectionType"),&var)) )
               return STRLOAD_E_INVALIDFORMAT;
            else
               m_ConnectionType = (pgsTypes::PierConnectionType)var.lVal;
         }
      } // 4.0 <= version

      if ( 5.0 <= version )
      {
         // LLDF stuff added in version 5
         if ( m_pBridgeDesc->GetDistributionFactorMethod() == pgsTypes::DirectlyInput )
         {
            pStrLoad->BeginUnit(_T("LLDF"));
            Float64 lldf_version;
            pStrLoad->get_Version(&lldf_version);

            if ( lldf_version < 3 )
            {
               // Prior to version 3, factors were for interior and exterior only
               Float64 gM[2][2];
               Float64 gR[2][2];

               if ( lldf_version < 2 )
               {
                  var.vt = VT_R8;
                  if ( FAILED(pStrLoad->get_Property(_T("gM_Interior"),&var)) )
                     return STRLOAD_E_INVALIDFORMAT;

                  gM[pgsTypes::Interior][0] = var.dblVal;
                  gM[pgsTypes::Interior][1] = var.dblVal;

                  var.vt = VT_R8;
                  if ( FAILED(pStrLoad->get_Property(_T("gM_Exterior"),&var)) )
                     return STRLOAD_E_INVALIDFORMAT;

                  gM[pgsTypes::Exterior][0] = var.dblVal;
                  gM[pgsTypes::Exterior][1] = var.dblVal;

                  var.vt = VT_R8;
                  if ( FAILED(pStrLoad->get_Property(_T("gR_Interior"),&var)) )
                     return STRLOAD_E_INVALIDFORMAT;

                  gR[pgsTypes::Interior][0] = var.dblVal;
                  gR[pgsTypes::Interior][1] = var.dblVal;

                  var.vt = VT_R8;
                  if ( FAILED(pStrLoad->get_Property(_T("gR_Exterior"),&var)) )
                     return STRLOAD_E_INVALIDFORMAT;

                  gR[pgsTypes::Exterior][0] = var.dblVal;
                  gR[pgsTypes::Exterior][1] = var.dblVal;
               }
               else
               {
                  var.vt = VT_R8;
                  if ( FAILED(pStrLoad->get_Property(_T("gM_Interior_Strength"),&var)) )
                     return STRLOAD_E_INVALIDFORMAT;

                  gM[pgsTypes::Interior][0] = var.dblVal;

                  var.vt = VT_R8;
                  if ( FAILED(pStrLoad->get_Property(_T("gM_Exterior_Strength"),&var)) )
                     return STRLOAD_E_INVALIDFORMAT;

                  gM[pgsTypes::Exterior][0] = var.dblVal;

                  var.vt = VT_R8;
                  if ( FAILED(pStrLoad->get_Property(_T("gR_Interior_Strength"),&var)) )
                     return STRLOAD_E_INVALIDFORMAT;

                  gR[pgsTypes::Interior][0] = var.dblVal;

                  var.vt = VT_R8;
                  if ( FAILED(pStrLoad->get_Property(_T("gR_Exterior_Strength"),&var)) )
                     return STRLOAD_E_INVALIDFORMAT;

                  gR[pgsTypes::Exterior][0] = var.dblVal;

                  var.vt = VT_R8;
                  if ( FAILED(pStrLoad->get_Property(_T("gM_Interior_Fatigue"),&var)) )
                     return STRLOAD_E_INVALIDFORMAT;

                  gM[pgsTypes::Interior][1] = var.dblVal;

                  var.vt = VT_R8;
                  if ( FAILED(pStrLoad->get_Property(_T("gM_Exterior_Fatigue"),&var)) )
                     return STRLOAD_E_INVALIDFORMAT;

                  gM[pgsTypes::Exterior][1] = var.dblVal;

                  var.vt = VT_R8;
                  if ( FAILED(pStrLoad->get_Property(_T("gR_Interior_Fatigue"),&var)) )
                     return STRLOAD_E_INVALIDFORMAT;

                  gR[pgsTypes::Interior][1] = var.dblVal;

                  var.vt = VT_R8;
                  if ( FAILED(pStrLoad->get_Property(_T("gR_Exterior_Fatigue"),&var)) )
                     return STRLOAD_E_INVALIDFORMAT;

                  gR[pgsTypes::Exterior][1] = var.dblVal;
               }

               // Move interior and exterior factors into first two slots in df vector. We will 
               // need to move them into all girder slots once this object is fully connected to the bridge
               m_DistributionFactorsFromOlderVersion = true;

               LLDF df;
               df.gM[0] = gM[pgsTypes::Exterior][0];
               df.gM[1] = gM[pgsTypes::Exterior][1];
               df.gR[0] = gR[pgsTypes::Exterior][0];
               df.gR[1] = gR[pgsTypes::Exterior][1];

               m_LLDFs.push_back(df); // First in list is exterior

               df.gM[0] = gM[pgsTypes::Interior][0];
               df.gM[1] = gM[pgsTypes::Interior][1];
               df.gR[0] = gR[pgsTypes::Interior][0];
               df.gR[1] = gR[pgsTypes::Interior][1];

               m_LLDFs.push_back(df); // Second is interior
            }
            else
            {
               // distribution factors by girder
               var.vt = VT_I4;
               hr = pStrLoad->get_Property(_T("nLLDFGirders"),&var);
               int ng = var.lVal;

               var.vt = VT_R8;

               for (int ig=0; ig<ng; ig++)
               {
                  LLDF lldf;

                  hr = pStrLoad->BeginUnit(_T("LLDF_Girder"));

                  hr = pStrLoad->get_Property(_T("gM_Strength"),&var);
                  lldf.gM[0] = var.dblVal;

                  hr = pStrLoad->get_Property(_T("gR_Strength"),&var);
                  lldf.gR[0] = var.dblVal;

                  hr = pStrLoad->get_Property(_T("gM_Fatigue"),&var);
                  lldf.gM[1] = var.dblVal;

                  hr = pStrLoad->get_Property(_T("gR_Fatigue"),&var);
                  lldf.gR[1] = var.dblVal;

                  pStrLoad->EndUnit(); // LLDF

                  m_LLDFs.push_back(lldf);
               }
            }

            pStrLoad->EndUnit();
         }

         // link stuff added in version 5
         // RAB: 10/17/2008 - got rid of the link stuff... still need to read this attribute
         var.Clear();
         var.vt = VT_BOOL;
         bool bIsLinked;
         if ( FAILED(pStrLoad->get_Property(_T("IsLinked"),&var)) )
            return STRLOAD_E_INVALIDFORMAT;
         else
            bIsLinked = (var.boolVal == VARIANT_TRUE ? true : false);

      }
   }

   // Get connection data from library entry
   if ( bUpdateFromLibrary )
   {
      CComPtr<IBroker> pBroker;
      EAFGetBroker(&pBroker);
      GET_IFACE2(pBroker,ILibrary,pLibrary);
      for ( int i = 0; i < 2; i++ )
      {
         const ConnectionLibraryEntry* pConnection = pLibrary->GetConnectionEntry(connectionLibName[i].c_str());
         if ( pConnection )
         {
            m_GirderEndDistance[i]          = pConnection->GetGirderEndDistance();
            m_EndDistanceMeasurementType[i] = pConnection->GetEndDistanceMeasurementType();;
            
            m_GirderBearingOffset[i]          = pConnection->GetGirderBearingOffset();
            m_BearingOffsetMeasurementType[i] = pConnection->GetBearingOffsetMeasurementType();

            m_SupportWidth[i] = pConnection->GetSupportWidth();

            m_DiaphragmHeight[i]       = pConnection->GetDiaphragmHeight();
            m_DiaphragmWidth[i]        = pConnection->GetDiaphragmWidth();
            m_DiaphragmLoadType[i]     = pConnection->GetDiaphragmLoadType();
            m_DiaphragmLoadLocation[i] = pConnection->GetDiaphragmLoadLocation();
         }
      }
   }

   if ( SUCCEEDED(hr2) )
      pStrLoad->EndUnit();

   return hr;
}

HRESULT CPierData::Save(IStructuredSave* pStrSave,IProgress* pProgress)
{
   HRESULT hr = S_OK;

   pStrSave->BeginUnit(_T("PierDataDetails"),9.0);

   pStrSave->put_Property(_T("Station"),         CComVariant(m_Station) );
   pStrSave->put_Property(_T("Orientation"),     CComVariant( CComBSTR(m_strOrientation.c_str()) ) );
   //pStrSave->put_Property(_T("AlignmentOffset"), CComVariant( m_AlignmentOffset) ); // added in version 6, removed in version 8
   //pStrSave->put_Property(_T("LeftConnection"),  CComVariant( CComBSTR(m_Connection[pgsTypes::Back].c_str()) ) ); // removed in version 9
   //pStrSave->put_Property(_T("RightConnection"), CComVariant( CComBSTR(m_Connection[pgsTypes::Ahead].c_str()) ) ); // removed in version 9
   pStrSave->put_Property(_T("ConnectionType"),  CComVariant( m_ConnectionType ) ); // changed from left and right to a single value in version 7


   // Back unit and everything in it was added in version 9
   pStrSave->BeginUnit(_T("Back"),1.0);
   pStrSave->put_Property(_T("GirderEndDistance"),CComVariant( m_GirderEndDistance[pgsTypes::Back] ) );
   pStrSave->put_Property(_T("EndDistanceMeasurementType"), CComVariant(ConnectionLibraryEntry::StringForEndDistanceMeasurementType(m_EndDistanceMeasurementType[pgsTypes::Back]).c_str()) );
   pStrSave->put_Property(_T("GirderBearingOffset"),CComVariant(m_GirderBearingOffset[pgsTypes::Back]));
   pStrSave->put_Property(_T("BearingOffsetMeasurementType"),CComVariant(ConnectionLibraryEntry::StringForBearingOffsetMeasurementType(m_BearingOffsetMeasurementType[pgsTypes::Back]).c_str()) );
   pStrSave->put_Property(_T("SupportWidth"),CComVariant(m_SupportWidth[pgsTypes::Back]));

   {
      pStrSave->BeginUnit(_T("Diaphragm"),1.0);
      pStrSave->put_Property(_T("DiaphragmWidth"),  CComVariant(m_DiaphragmWidth[pgsTypes::Back]));
      pStrSave->put_Property(_T("DiaphragmHeight"), CComVariant(m_DiaphragmHeight[pgsTypes::Back]));

      if (m_DiaphragmLoadType[pgsTypes::Back] == ConnectionLibraryEntry::ApplyAtBearingCenterline)
      {
         pStrSave->put_Property(_T("DiaphragmLoadType"),CComVariant(_T("ApplyAtBearingCenterline")));
      }
      else if (m_DiaphragmLoadType[pgsTypes::Back] == ConnectionLibraryEntry::ApplyAtSpecifiedLocation)
      {
         pStrSave->put_Property(_T("DiaphragmLoadType"),CComVariant(_T("ApplyAtSpecifiedLocation")));
         pStrSave->put_Property(_T("DiaphragmLoadLocation"),CComVariant(m_DiaphragmLoadLocation[pgsTypes::Back]));
      }
      else if (m_DiaphragmLoadType[pgsTypes::Back] == ConnectionLibraryEntry::DontApply)
      {
         pStrSave->put_Property(_T("DiaphragmLoadType"),CComVariant(_T("DontApply")));
      }
      else
      {
         ATLASSERT(false); // is there a new load type?
      }
      pStrSave->EndUnit(); // Diaphragm
   }
   pStrSave->EndUnit(); // Back

   // Ahead unit and everything in it was added in version 9
   pStrSave->BeginUnit(_T("Ahead"),1.0);
   pStrSave->put_Property(_T("GirderEndDistance"),CComVariant( m_GirderEndDistance[pgsTypes::Ahead] ) );
   pStrSave->put_Property(_T("EndDistanceMeasurementType"), CComVariant(ConnectionLibraryEntry::StringForEndDistanceMeasurementType(m_EndDistanceMeasurementType[pgsTypes::Ahead]).c_str()) );
   pStrSave->put_Property(_T("GirderBearingOffset"),CComVariant(m_GirderBearingOffset[pgsTypes::Ahead]));
   pStrSave->put_Property(_T("BearingOffsetMeasurementType"),CComVariant(ConnectionLibraryEntry::StringForBearingOffsetMeasurementType(m_BearingOffsetMeasurementType[pgsTypes::Ahead]).c_str()) );
   pStrSave->put_Property(_T("SupportWidth"),CComVariant(m_SupportWidth[pgsTypes::Ahead]));

   {
      pStrSave->BeginUnit(_T("Diaphragm"),1.0);
      pStrSave->put_Property(_T("DiaphragmWidth"),  CComVariant(m_DiaphragmWidth[pgsTypes::Ahead]));
      pStrSave->put_Property(_T("DiaphragmHeight"), CComVariant(m_DiaphragmHeight[pgsTypes::Ahead]));

      if (m_DiaphragmLoadType[pgsTypes::Ahead] == ConnectionLibraryEntry::ApplyAtBearingCenterline)
      {
         pStrSave->put_Property(_T("DiaphragmLoadType"),CComVariant(_T("ApplyAtBearingCenterline")));
      }
      else if (m_DiaphragmLoadType[pgsTypes::Ahead] == ConnectionLibraryEntry::ApplyAtSpecifiedLocation)
      {
         pStrSave->put_Property(_T("DiaphragmLoadType"),CComVariant(_T("ApplyAtSpecifiedLocation")));
         pStrSave->put_Property(_T("DiaphragmLoadLocation"),CComVariant(m_DiaphragmLoadLocation[pgsTypes::Ahead]));
      }
      else if (m_DiaphragmLoadType[pgsTypes::Ahead] == ConnectionLibraryEntry::DontApply)
      {
         pStrSave->put_Property(_T("DiaphragmLoadType"),CComVariant(_T("DontApply")));
      }
      else
      {
         ATLASSERT(false); // is there a new load type?
      }
      pStrSave->EndUnit(); // Diaphragm
   }
   pStrSave->EndUnit(); // Ahead




   // added in version 5
   if ( m_pBridgeDesc->GetDistributionFactorMethod() == pgsTypes::DirectlyInput )
   {
      pStrSave->BeginUnit(_T("LLDF"),3.0); // Version 3 went from interior/exterior to girder by girder

      GirderIndexType ngs = GetLldfGirderCount();
      pStrSave->put_Property(_T("nLLDFGirders"),CComVariant(ngs));

      for (GirderIndexType igs=0; igs<ngs; igs++)
      {
         pStrSave->BeginUnit(_T("LLDF_Girder"),1.0);
         LLDF& lldf = GetLLDF(igs);

         pStrSave->put_Property(_T("gM_Strength"), CComVariant(lldf.gM[0]));
         pStrSave->put_Property(_T("gR_Strength"), CComVariant(lldf.gR[0]));
         pStrSave->put_Property(_T("gM_Fatigue"),  CComVariant(lldf.gM[1]));
         pStrSave->put_Property(_T("gR_Fatigue"),  CComVariant(lldf.gR[1]));
         pStrSave->EndUnit(); // LLDF_Girder
      }

      pStrSave->EndUnit(); // LLDF
   }

   // added in version 5 - RAB: 10/17/2008 - not linking any more
   pStrSave->put_Property(_T("IsLinked"),CComVariant(VARIANT_FALSE));

   pStrSave->EndUnit();

   return hr;
}

void CPierData::MakeCopy(const CPierData& rOther)
{
   m_PierIdx             = rOther.m_PierIdx;

   m_Station               = rOther.m_Station;
   Orientation             = rOther.Orientation;
   Angle                   = rOther.Angle;
   
   m_ConnectionType  = rOther.m_ConnectionType;

   m_strOrientation  = rOther.m_strOrientation;

   for ( int i = 0; i < 2; i++ )
   {
      m_GirderEndDistance[i]            = rOther.m_GirderEndDistance[i];
      m_EndDistanceMeasurementType[i]   = rOther.m_EndDistanceMeasurementType[i];
      m_GirderBearingOffset[i]          = rOther.m_GirderBearingOffset[i];
      m_BearingOffsetMeasurementType[i] = rOther.m_BearingOffsetMeasurementType[i];
      m_SupportWidth[i]                 = rOther.m_SupportWidth[i];

      m_DiaphragmHeight[i]       = rOther.m_DiaphragmHeight[i];       
      m_DiaphragmWidth[i]        = rOther.m_DiaphragmWidth[i];
      m_DiaphragmLoadType[i]     = rOther.m_DiaphragmLoadType[i];
      m_DiaphragmLoadLocation[i] = rOther.m_DiaphragmLoadLocation[i];
   }

   m_LLDFs = rOther.m_LLDFs;

   m_DistributionFactorsFromOlderVersion = rOther.m_DistributionFactorsFromOlderVersion;
}

void CPierData::MakeAssignment(const CPierData& rOther)
{
   MakeCopy( rOther );
}

void CPierData::SetPierIndex(PierIndexType pierIdx)
{
   m_PierIdx = pierIdx;
}

PierIndexType CPierData::GetPierIndex() const
{
   return m_PierIdx;
}

void CPierData::SetBridgeDescription(const CBridgeDescription* pBridge)
{
   m_pBridgeDesc = pBridge;
}

const CBridgeDescription* CPierData::GetBridgeDescription() const
{
   return m_pBridgeDesc;
}

void CPierData::SetSpans(CSpanData* pPrevSpan,CSpanData* pNextSpan)
{
   m_pPrevSpan = pPrevSpan;
   m_pNextSpan = pNextSpan;
}

CSpanData* CPierData::GetPrevSpan()
{
   return m_pPrevSpan;
}

CSpanData* CPierData::GetNextSpan()
{
   return m_pNextSpan;
}

CSpanData* CPierData::GetSpan(pgsTypes::PierFaceType face)
{
   return (face == pgsTypes::Ahead ? m_pNextSpan : m_pPrevSpan);
}

const CSpanData* CPierData::GetPrevSpan() const
{
   return m_pPrevSpan;
}

const CSpanData* CPierData::GetNextSpan() const
{
   return m_pNextSpan;
}

const CSpanData* CPierData::GetSpan(pgsTypes::PierFaceType face) const
{
   return (face == pgsTypes::Ahead ? m_pNextSpan : m_pPrevSpan);
}

Float64 CPierData::GetStation() const
{
   return m_Station;
}

void CPierData::SetStation(Float64 station)
{
   m_Station = station;
}

LPCTSTR CPierData::GetOrientation() const
{
   return m_strOrientation.c_str();
}

void CPierData::SetOrientation(LPCTSTR strOrientation)
{
   m_strOrientation = strOrientation;
}

pgsTypes::PierConnectionType CPierData::GetConnectionType() const
{
   return m_ConnectionType;
}

void CPierData::SetConnectionType(pgsTypes::PierConnectionType type)
{
   m_ConnectionType = type;
}

Float64 CPierData::GetLLDFNegMoment(GirderIndexType gdrIdx, pgsTypes::LimitState ls) const
{
   LLDF& rlldf = GetLLDF(gdrIdx);

   return rlldf.gM[ls == pgsTypes::FatigueI ? 1 : 0];
}

void CPierData::SetLLDFNegMoment(GirderIndexType gdrIdx, pgsTypes::LimitState ls, Float64 gM)
{
   LLDF& rlldf = GetLLDF(gdrIdx);

   rlldf.gM[ls == pgsTypes::FatigueI ? 1 : 0] = gM;
}

void CPierData::SetLLDFNegMoment(pgsTypes::GirderLocation gdrloc, pgsTypes::LimitState ls, Float64 gM)
{
   GirderIndexType ngdrs = GetLldfGirderCount();
   if (ngdrs>2 && gdrloc==pgsTypes::Interior)
   {
      for (GirderIndexType ig=1; ig<ngdrs-1; ig++)
      {
         SetLLDFNegMoment(ig,ls,gM);
      }
   }
   else if (gdrloc==pgsTypes::Exterior)
   {
      SetLLDFNegMoment(0,ls,gM);
      SetLLDFNegMoment(ngdrs-1,ls,gM);
   }
}

Float64 CPierData::GetLLDFReaction(GirderIndexType gdrIdx, pgsTypes::LimitState ls) const
{
   LLDF& rlldf = GetLLDF(gdrIdx);

   return rlldf.gR[ls == pgsTypes::FatigueI ? 1 : 0];
}

void CPierData::SetLLDFReaction(GirderIndexType gdrIdx, pgsTypes::LimitState ls, Float64 gR)
{
   LLDF& rlldf = GetLLDF(gdrIdx);

   rlldf.gR[ls == pgsTypes::FatigueI ? 1 : 0] = gR;
}

void CPierData::SetLLDFReaction(pgsTypes::GirderLocation gdrloc, pgsTypes::LimitState ls, Float64 gM)
{
   GirderIndexType ngdrs = GetLldfGirderCount();
   if (ngdrs>2 && gdrloc==pgsTypes::Interior)
   {
      for (GirderIndexType ig=1; ig<ngdrs-1; ig++)
      {
         SetLLDFReaction(ig,ls,gM);
      }
   }
   else if (gdrloc==pgsTypes::Exterior)
   {
      SetLLDFReaction(0,ls,gM);
      SetLLDFReaction(ngdrs-1,ls,gM);
   }
}


bool CPierData::IsContinuous() const
{
   return m_ConnectionType == pgsTypes::ContinuousBeforeDeck || m_ConnectionType == pgsTypes::ContinuousAfterDeck;
}

void CPierData::IsIntegral(bool* pbLeft,bool* pbRight) const
{
   if (m_ConnectionType == pgsTypes::IntegralBeforeDeck || m_ConnectionType == pgsTypes::IntegralAfterDeck)
   {
      *pbLeft  = true;
      *pbRight = true;
   }
   else
   {
      *pbLeft  = m_ConnectionType == pgsTypes::IntegralAfterDeckHingeAhead || m_ConnectionType == pgsTypes::IntegralBeforeDeckHingeAhead;

      *pbRight = m_ConnectionType == pgsTypes::IntegralAfterDeckHingeBack  || m_ConnectionType == pgsTypes::IntegralBeforeDeckHingeBack;
   }
}

bool CPierData::IsAbutment() const
{
   return (m_pPrevSpan == NULL || m_pNextSpan == NULL ? true : false);
}

void CPierData::SetGirderEndDistance(pgsTypes::PierFaceType face,Float64 endDist,ConnectionLibraryEntry::EndDistanceMeasurementType measure)
{
   m_GirderEndDistance[face] = endDist;
   m_EndDistanceMeasurementType[face] = measure;
}

void CPierData::GetGirderEndDistance(pgsTypes::PierFaceType face,Float64* pEndDist,ConnectionLibraryEntry::EndDistanceMeasurementType* pMeasure) const
{
   *pEndDist = m_GirderEndDistance[face];
   *pMeasure = m_EndDistanceMeasurementType[face];
}

void CPierData::SetBearingOffset(pgsTypes::PierFaceType face,Float64 offset,ConnectionLibraryEntry::BearingOffsetMeasurementType measure)
{
   m_GirderBearingOffset[face] = offset;
   m_BearingOffsetMeasurementType[face] = measure;
}

void CPierData::GetBearingOffset(pgsTypes::PierFaceType face,Float64* pOffset,ConnectionLibraryEntry::BearingOffsetMeasurementType* pMeasure) const
{
   *pOffset = m_GirderBearingOffset[face];
   *pMeasure = m_BearingOffsetMeasurementType[face];
}

void CPierData::SetSupportWidth(pgsTypes::PierFaceType face,Float64 w)
{
   m_SupportWidth[face] = w;
}

Float64 CPierData::GetSupportWidth(pgsTypes::PierFaceType face) const
{
   return m_SupportWidth[face];
}

void CPierData::SetDiaphragmHeight(pgsTypes::PierFaceType pierFace,Float64 d)
{
   m_DiaphragmHeight[pierFace] = d;
}

Float64 CPierData::GetDiaphragmHeight(pgsTypes::PierFaceType pierFace) const
{
   return m_DiaphragmHeight[pierFace];
}

void CPierData::SetDiaphragmWidth(pgsTypes::PierFaceType pierFace,Float64 w)
{
   m_DiaphragmWidth[pierFace] = w;
}

Float64 CPierData::GetDiaphragmWidth(pgsTypes::PierFaceType pierFace)const
{
   return m_DiaphragmWidth[pierFace];
}

ConnectionLibraryEntry::DiaphragmLoadType CPierData::GetDiaphragmLoadType(pgsTypes::PierFaceType pierFace) const
{
   return m_DiaphragmLoadType[pierFace];
}

void CPierData::SetDiaphragmLoadType(pgsTypes::PierFaceType pierFace,ConnectionLibraryEntry::DiaphragmLoadType type)
{
   m_DiaphragmLoadType[pierFace] = type;
   m_DiaphragmLoadLocation[pierFace] = 0.0;
}

Float64 CPierData::GetDiaphragmLoadLocation(pgsTypes::PierFaceType pierFace) const
{
   return m_DiaphragmLoadLocation[pierFace];
}

void CPierData::SetDiaphragmLoadLocation(pgsTypes::PierFaceType pierFace,Float64 loc)
{
   m_DiaphragmLoadLocation[pierFace] = loc;
}

CPierData::LLDF& CPierData::GetLLDF(GirderIndexType igs) const
{
   // First: Compare size of our collection with current number of girders and resize if they don't match
   GirderIndexType ngdrs = GetLldfGirderCount();
   ATLASSERT(ngdrs>0);

   IndexType ndfs = m_LLDFs.size();

   if (m_DistributionFactorsFromOlderVersion)
   {
      // data loaded from older versions should be loaded into first two entries
      if(ndfs==2)
      {
         LLDF exterior = m_LLDFs[0];
         LLDF interior = m_LLDFs[1];
         for (GirderIndexType ig=2; ig<ngdrs; ig++)
         {
            if (ig!=ngdrs-1)
            {
               m_LLDFs.push_back(interior);
            }
            else
            {
               m_LLDFs.push_back(exterior);
            }
         }

         m_DistributionFactorsFromOlderVersion = false;
         ndfs = ngdrs;
      }
      else
      {
         ATLASSERT(0); // something went wrong on load
      }
   }

   if (ndfs==0)
   {
      for (GirderIndexType i=0; i<ngdrs; i++)
      {
         m_LLDFs.push_back(LLDF());
      }
   }
   else if (ndfs<ngdrs)
   {
      // More girders than factors - move exterior to last girder and use last interior for new interiors
      LLDF exterior = m_LLDFs.back();
      GirderIndexType inter_idx = ndfs-2>0 ? ndfs-2 : 0; // one-girder bridges could otherwise give us trouble
      LLDF interior = m_LLDFs[inter_idx];

      m_LLDFs[ndfs-1] = interior;
      for (GirderIndexType i=ndfs; i<ngdrs; i++)
      {
         if (i != ngdrs-1)
         {
            m_LLDFs.push_back(interior);
         }
         else
         {
            m_LLDFs.push_back(exterior);
         }
      }
    }
   else if (ndfs>ngdrs)
   {
      // more factors than girders - truncate, then move last exterior to end
      LLDF exterior = m_LLDFs.back();
      m_LLDFs.resize(ngdrs);
      m_LLDFs.back() = exterior;
   }

   // Next: let's deal with retrieval
   if (igs<0)
   {
      ATLASSERT(0); // problemo in calling routine - let's not crash
      return m_LLDFs[0];
   }
   else if (igs>=ngdrs)
   {
      ATLASSERT(0); // problemo in calling routine - let's not crash
      return m_LLDFs.back();
   }
   else
   {
      return m_LLDFs[igs];
   }
}

GirderIndexType CPierData::GetLldfGirderCount() const
{
   GirderIndexType ahead(0), back(0);

   const CSpanData* pAhead = GetSpan(pgsTypes::Ahead);
   if (pAhead!=NULL)
      ahead = pAhead->GetGirderCount();

   const CSpanData* pBack = GetSpan(pgsTypes::Back);
   if (pBack!=NULL)
      back = pBack->GetGirderCount();

   if (pBack==NULL && pAhead==NULL)
   {
      ATLASSERT(0); // function called before bridge tied together - no good
      return 0;
   }
   else
   {
      return max(ahead, back);
   }
}
