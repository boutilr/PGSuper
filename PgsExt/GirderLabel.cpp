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

#include <PgsExt\PgsExtLib.h>
#include <PgsExt\GirderLabel.h>

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
