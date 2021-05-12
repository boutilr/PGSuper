///////////////////////////////////////////////////////////////////////
// PGSuper - Prestressed Girder SUPERstructure Design and Analysis
// Copyright � 1999-2021  Washington State Department of Transportation
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
#pragma once

////////////////////////////////////////////////////
// CTxDataExporter
//
// Abstract base class for a data exporter
class CTxDataExporter
{
public:
   CTxDataExporter(void);
   virtual ~CTxDataExporter(void);

   // Write to cell using named range and row offset
   // Can have two tables: hTable==1:2
   virtual void WriteStringToCell(IndexType hTable, LPCTSTR strRangeName, IndexType rowIdx, LPCTSTR strString) = 0;
   virtual void WriteWarningToCell(IndexType hTable, LPCTSTR strRangeName, IndexType rowIdx, LPCTSTR strString) = 0;
   virtual void WriteIntToCell(IndexType hTable, LPCTSTR strRangeName, IndexType rowIdx, Int64 intVal);
   virtual void WriteFloatToCell(IndexType hTable, LPCTSTR strRangeName, IndexType rowIdx, Float64 fltVal);
   // commits the data to a file
   virtual BOOL Commit(LPCTSTR strFilename) = 0;
   // called if the data export fails.
   virtual void Fail() = 0;
};
