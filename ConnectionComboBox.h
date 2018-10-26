///////////////////////////////////////////////////////////////////////
// PGSuper - Prestressed Girder SUPERstructure Design and Analysis
// Copyright � 1999-2012  Washington State Department of Transportation
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
#ifndef INCLUDED_CONNECTIONCOMBOBOX_H_
#define INCLUDED_CONNECTIONCOMBOBOX_H_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#define PIERTYPE_START 0
#define PIERTYPE_INTERMEDIATE 1
#define PIERTYPE_END 2

class CConnectionComboBox : public CComboBox
{
public:
   CConnectionComboBox();

   void SetPierType(int pierType);

   virtual void DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct);

private:
   int m_PierType;
};

#endif // INCLUDED_CONNECTIONCOMBOBOX_H_