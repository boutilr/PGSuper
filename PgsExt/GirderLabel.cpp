///////////////////////////////////////////////////////////////////////
// PGSuper - Prestressed Girder SUPERstructure Design and Analysis
// Copyright � 1999-2020  Washington State Department of Transportation
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

#include <PgsExt\PierData2.h>
#include <PgsExt\TemporarySupportData.h>
#include <EAF\EAFDisplayUnits.h>

// Includes for ConcreteDescription
#include <PgsExt\GirderLabel.h>
#include <EAF\EAFUtilities.h>
#include <IFace\Project.h>
#include <IFace\DocumentType.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

bool pgsGirderLabel::ms_bUseAlpha = true;

std::_tstring pgsGirderLabel::GetGirderLabel(GirderIndexType gdrIdx)
{
   std::_tstring strLabel;
   if ( ms_bUseAlpha )
   {
      strLabel += ((TCHAR)((gdrIdx % 26) + _T('A')));
      gdrIdx = ((gdrIdx - (gdrIdx % 26))/26);
      if ( 0 < gdrIdx )
      {
         std::_tstring strTemp = strLabel;
         strLabel = pgsGirderLabel::GetGirderLabel(gdrIdx-1);
         strLabel += strTemp;
      }
   }
   else
   {
      std::_tostringstream os;
      os << (long)(gdrIdx+1L);
      strLabel = os.str();
   }

   return strLabel;
}

std::_tstring pgsGirderLabel::GetGirderLabel(const CGirderKey& girderKey)
{
   CComPtr<IBroker> pBroker;
   EAFGetBroker(&pBroker);
   GET_IFACE2(pBroker,IDocumentType,pDocType);
   if ( pDocType->IsPGSuperDocument() )
   {
      std::_tostringstream os;
      os << _T("Span ") << LABEL_SPAN(girderKey.groupIndex) << _T(", Girder ") << LABEL_GIRDER(girderKey.girderIndex);
      return os.str();
   }
   else
   {
      std::_tostringstream os;
      os << _T("Group ") << LABEL_GROUP(girderKey.groupIndex) << _T(", Girder ") << LABEL_GIRDER(girderKey.girderIndex);
      return os.str();
   }
}

std::_tstring pgsGirderLabel::GetSegmentLabel(const CSegmentKey& segmentKey)
{
   CComPtr<IBroker> pBroker;
   EAFGetBroker(&pBroker);
   GET_IFACE2(pBroker,IDocumentType,pDocType);
   if ( pDocType->IsPGSuperDocument() )
   {
      ATLASSERT(segmentKey.segmentIndex == 0);
      std::_tostringstream os;
      os << _T("Span ") << LABEL_SPAN(segmentKey.groupIndex) << _T(", Girder ") << LABEL_GIRDER(segmentKey.girderIndex);
      return os.str();
   }
   else
   {
      std::_tostringstream os;
      os << _T("Group ") << LABEL_GROUP(segmentKey.groupIndex) << _T(", Girder ") << LABEL_GIRDER(segmentKey.girderIndex) << _T(", Segment ") << LABEL_SEGMENT(segmentKey.segmentIndex);
      return os.str();
   }
}

std::_tstring pgsGirderLabel::GetClosureLabel(const CClosureKey& closureKey)
{
   std::_tostringstream os;
   os << _T("Group ") << LABEL_GROUP(closureKey.groupIndex) << _T(", Girder ") << LABEL_GIRDER(closureKey.girderIndex) << _T(", Closure ") << LABEL_SEGMENT(closureKey.segmentIndex);
   return os.str();
}

bool pgsGirderLabel::UseAlphaLabel()
{
   return ms_bUseAlpha;
}

bool pgsGirderLabel::UseAlphaLabel(bool bUseAlpha)
{
   bool bOldValue = ms_bUseAlpha;
   ms_bUseAlpha = bUseAlpha;
   return bOldValue;
}

pgsGirderLabel::pgsGirderLabel(void)
{
}

pgsGirderLabel::~pgsGirderLabel(void)
{
}


LPCTSTR GetEndDistanceMeasureString(ConnectionLibraryEntry::EndDistanceMeasurementType type,bool bAbutment,bool bAbbreviation)
{
   switch( type )
   {
   case ConnectionLibraryEntry::FromBearingAlongGirder:
      if (bAbbreviation)
      {
         return _T("FCLB-AG");
      }
      else
      {
         return _T("Measured From CL Bearing along Girder");
      }

   case ConnectionLibraryEntry::FromBearingNormalToPier:
      if ( bAbbreviation )
      {
         return _T("FCLB-NCLB");
      }
      else
      {
         return _T("Measured From and Normal to CL Bearing");
      }

   case ConnectionLibraryEntry::FromPierAlongGirder:
      if ( bAbbreviation )
      {
         return bAbutment ? _T("FA-AG") : _T("FP-AG");
      }
      else
      {
         return bAbutment ? _T("Measured From Abutment Line and Along Girder Centerline") : _T("Measured From Pier Line and Along Girder Centerline");
      }

   case ConnectionLibraryEntry::FromPierNormalToPier:
      if ( bAbbreviation )
      {
         return bAbutment ? _T("FA-NA") : _T("FP-NP");
      }
      else
      {
         return bAbutment ? _T("Measured From and Normal to Abutment Line") : _T("Measured From and Normal to Pier Line");
      }

   default:
      ATLASSERT(false);
      return _T("");
   }
}

LPCTSTR GetBearingOffsetMeasureString(ConnectionLibraryEntry::BearingOffsetMeasurementType type,bool bAbutment,bool bAbbreviation)
{
   switch( type )
   {
   case ConnectionLibraryEntry::AlongGirder:
      if ( bAbbreviation )
      {
         return bAbutment ? _T("FA-AG") : _T("FP-AG");
      }
      else
      {
         return bAbutment ? _T("Measured From Abutment Line and Along Girder Centerline") :  _T("Measured From Pier Line and Along Girder Centerline");
      }

   case ConnectionLibraryEntry::NormalToPier:
      if ( bAbbreviation )
      {
         return bAbutment ? _T("FA-NA") : _T("FP-NP");
      }
      else
      {
         return bAbutment ? _T("Measured From and Normal to Abutment Line") : _T("Measured From and Normal to Pier Line");
      }

   default:
      ATLASSERT(false);
      return _T("");
   }
}

CString GetLabel(const CPierData2* pPier,IEAFDisplayUnits* pDisplayUnits)
{
   CString strLabel;
   strLabel.Format(_T("%s %d, %s"),(pPier->IsAbutment() ? _T("Abutment") : _T("Pier")),
                                   LABEL_PIER(pPier->GetIndex()),
                                   FormatStation(pDisplayUnits->GetStationFormat(),pPier->GetStation()));

   return strLabel;
}

CString GetLabel(const CTemporarySupportData* pTS,IEAFDisplayUnits* pDisplayUnits)
{
   CString strLabel;
   strLabel.Format(_T("TS %d, %s, %s"),LABEL_TEMPORARY_SUPPORT(pTS->GetIndex()),
                                       pTS->GetSupportType() == pgsTypes::ErectionTower ? _T("Erection Tower") : _T("Strong Back"),
                                       FormatStation(pDisplayUnits->GetStationFormat(),pTS->GetStation()));

   return strLabel;
}

CString ConcreteDescription(const CConcreteMaterial& concrete)
{
   CComPtr<IBroker> pBroker;
   EAFGetBroker(&pBroker);

   GET_IFACE2(pBroker,ILossParameters,pLossParameters);
   pgsTypes::LossMethod loss_method = pLossParameters->GetLossMethod();

   CString strLabel;
   if ( pLossParameters->GetLossMethod() == pgsTypes::TIME_STEP )
   {
      if ( pLossParameters->GetTimeDependentModel() == pgsTypes::tdmAASHTO || 
           pLossParameters->GetTimeDependentModel() == pgsTypes::tdmACI209 )
      {
         if ( concrete.bACIUserParameters )
         {
            GET_IFACE2(pBroker,IEAFDisplayUnits,pDisplayUnits);

            strLabel.Format(_T("%s, ACI 209R-92, %s cured, a = %s, Beta = %4.2f"),
               lrfdConcreteUtil::GetTypeName((matConcrete::Type)concrete.Type,true).c_str(),
               concrete.CureMethod == pgsTypes::Steam ? _T("Steam") : _T("Moist"),
               ::FormatDimension(concrete.A,pDisplayUnits->GetFractionalDaysUnit()),
               concrete.B);
         }
         else
         {
            strLabel.Format(_T("%s, ACI 209R-92, %s cured, %s cement"),
               lrfdConcreteUtil::GetTypeName((matConcrete::Type)concrete.Type,true).c_str(),
               concrete.CureMethod == pgsTypes::Steam ? _T("Steam") : _T("Moist"),
               concrete.ACI209CementType == pgsTypes::TypeI ? _T("Type I") : _T("Type III"));
         }
      }
      else
      {
         if ( concrete.bCEBFIPUserParameters )
         {
            strLabel.Format(_T("%s, CEB-FIP, s = %.6f, Beta SC = %.6f"),
                  lrfdConcreteUtil::GetTypeName((matConcrete::Type)concrete.Type,true).c_str(),
                  concrete.S,
                  concrete.BetaSc);
         }
         else
         {
            strLabel.Format(_T("%s, CEB-FIP, Type %s cement"),
                  lrfdConcreteUtil::GetTypeName((matConcrete::Type)concrete.Type,true).c_str(),
                  matCEBFIPConcrete::GetCementType((matCEBFIPConcrete::CementType)concrete.CEBFIPCementType));
         }
      }
   }
   else
   {
      strLabel.Format(_T("%s"),lrfdConcreteUtil::GetTypeName((matConcrete::Type)concrete.Type,true).c_str());
   }

   return strLabel;
}

LPCTSTR GetLimitStateString(pgsTypes::LimitState limitState)
{
   // these are the names that are displayed to the user in the UI and reports
   // this must be in the same order as the LimitState enum
   static LPCTSTR strNames[] =
   {
      _T("Service I"),
      _T("Service IA"),
      _T("Service III"),
      _T("Strength I"),
      _T("Strength II"),
      _T("Fatigue I"),
      _T("Strength I (Inventory)"),
      _T("Strength I (Operating)"),
      _T("Service III (Inventory)"),
      _T("Service III (Operating)"),
      _T("Strength I (Legal - Routine)"),
      _T("Strength I (Legal - Special)"),
      _T("Service III (Legal - Routine)"),
      _T("Service III (Legal - Special)"),
      _T("Strength II (Routine Permit Rating)"),
      _T("Service I (Routine Permit Rating)"),
      _T("Service III (Routine Permit Rating)"),
      _T("Strength II (Special Permit Rating)"),
      _T("Service I (Special Permit Rating)"),
      _T("Service III (Special Permit Rating)"),
      _T("Strength I (Legal - Emergency)"),
      _T("Service III (Legal - Emergency)"),
   };

   // the direct lookup in the array is faster, however if the enum changes (number of values or order of values)
   // it isn't easily detectable... the switch/case below is slower but it can detect errors that result
   // from changing the enum
#if defined _DEBUG
   std::_tstring strName;
   switch(limitState)
   {
      case pgsTypes::ServiceI:
         strName = _T("Service I");
         break;

      case pgsTypes::ServiceIA:
         strName = _T("Service IA");
         break;

      case pgsTypes::ServiceIII:
         strName = _T("Service III");
         break;

      case pgsTypes::StrengthI:
         strName = _T("Strength I");
         break;

      case pgsTypes::StrengthII:
         strName = _T("Strength II");
         break;

      case pgsTypes::FatigueI:
         strName = _T("Fatigue I");
         break;

      case pgsTypes::StrengthI_Inventory:
         strName = _T("Strength I (Inventory)");
         break;

      case pgsTypes::StrengthI_Operating:
         strName = _T("Strength I (Operating)");
         break;

      case pgsTypes::ServiceIII_Inventory:
         strName = _T("Service III (Inventory)");
         break;

      case pgsTypes::ServiceIII_Operating:
         strName = _T("Service III (Operating)");
         break;

      case pgsTypes::StrengthI_LegalRoutine:
         strName = _T("Strength I (Legal - Routine)");
         break;

      case pgsTypes::StrengthI_LegalSpecial:
         strName = _T("Strength I (Legal - Special)");
         break;

      case pgsTypes::StrengthI_LegalEmergency:
         strName = _T("Strength I (Legal - Emergency)");
         break;

      case pgsTypes::ServiceIII_LegalRoutine:
         strName = _T("Service III (Legal - Routine)");
         break;

      case pgsTypes::ServiceIII_LegalSpecial:
         strName = _T("Service III (Legal - Special)");
         break;

      case pgsTypes::ServiceIII_LegalEmergency:
         strName = _T("Service III (Legal - Emergency)");
         break;

      case pgsTypes::StrengthII_PermitRoutine:
         strName = _T("Strength II (Routine Permit Rating)");
         break;

      case pgsTypes::ServiceI_PermitRoutine:
         strName = _T("Service I (Routine Permit Rating)");
         break;

      case pgsTypes::ServiceIII_PermitRoutine:
         strName = _T("Service III (Routine Permit Rating)");
         break;

      case pgsTypes::StrengthII_PermitSpecial:
         strName = _T("Strength II (Special Permit Rating)");
         break;

      case pgsTypes::ServiceI_PermitSpecial:
         strName = _T("Service I (Special Permit Rating)");
         break;

      case pgsTypes::ServiceIII_PermitSpecial:
         strName = _T("Service III (Special Permit Rating)");
         break;

      default:
         ATLASSERT(false); // SHOULD NEVER GET HERE
   }

   ATLASSERT(strName == std::_tstring(strNames[limitState]));
#endif

   return strNames[limitState];
}

LPCTSTR GetStressLocationString(pgsTypes::StressLocation loc)
{
   switch(loc)
   {
   case pgsTypes::BottomGirder:
      return _T("Bottom of Girder");
      break;
   case pgsTypes::TopGirder:
      return _T("Top of Girder");
      break;
   case pgsTypes::TopDeck:
      return _T("Top of Slab");
      break;
   default:
      ATLASSERT(false);
      return _T("Error in StressLocation");
   }
}

LPCTSTR GetStressTypeString(pgsTypes::StressType type)
{
   switch(type)
   {
   case pgsTypes::Tension:
      return _T("Tension");
      break;
   case pgsTypes::Compression:
      return _T("Compression");
      break;
   default:
      ATLASSERT(false);
      return _T("Error in StressType");
   }
}
