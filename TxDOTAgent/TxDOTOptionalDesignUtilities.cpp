///////////////////////////////////////////////////////////////////////
// PGSuper - Prestressed Girder SUPERstructure Design and Analysis
// Copyright � 1999-2018  Washington State Department of Transportation
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

// TxDOTOptionalDesignUtilities.cpp
#include "stdafx.h"

#include "TxDOTOptionalDesignUtilities.h"

#include <PgsExt\BridgeDescription2.h>
#include <PgsExt\DebondUtil.h>
#include <PsgLib\GirderLibraryEntry.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// local function
static StrandRowUtil::StrandRowSet GetFullyPopulatedStrandRowSet(IBroker* pBroker, const pgsPointOfInterest& midPoi);


BOOL DoParseTemplateFile(const LPCTSTR lpszPathName, CString& girderEntry, 
                              CString& leftConnEntry, CString& rightConnEntry,
                              CString& projectCriteriaEntry, CString& folderName,
                              Float64& girderUnitWeight)
{
   // Read girder type, connection types, and pgsuper template file name
   std::_tifstream ifile(lpszPathName);
   if ( !ifile )
   {
      CString msg;
      msg.Format(_T("Error opening template file: %s - File not found?"),lpszPathName);
      AfxMessageBox(msg );
      ASSERT( 0 ); // this should never happen
      return FALSE;
   }

   TCHAR line[1024];
   ifile.getline(line,1024);

   // comma delimited file in format of:
   // GirderEntryName, EndConnection, StartConnection, ProjectCriteria
   sysTokenizer tokenizer(_T(","));
   tokenizer.push_back(line);

   sysTokenizer::size_type nitems = tokenizer.size();
   if (nitems<4) // Don't limit to allow new items to be added
   {
      CString msg;
      msg.Format(_T("Error reading template file: %s - Invalid Format"),lpszPathName);
      AfxMessageBox(msg );
      return FALSE;
   }

   // set our data values
   girderEntry = tokenizer[0].c_str();
   leftConnEntry = tokenizer[1].c_str();
   rightConnEntry = tokenizer[2].c_str();
   projectCriteriaEntry = tokenizer[3].c_str();

   if (nitems>4)
   {
      folderName= tokenizer[4].c_str();
   }
   else
   {
      // Folder name not spec'd in template file. Give it a default
      folderName = _T("Unnamed");
   }

   // concrete unit weight
   bool did_parse(false);
   if (nitems>5)
   {
      std::_tstring str = tokenizer[5].c_str();
      did_parse = sysTokenizer::ParseDouble(str.c_str(), &girderUnitWeight);
      // value stored in file is pcf
      girderUnitWeight = ::ConvertToSysUnits(girderUnitWeight, unitMeasure::LbfPerFeet3);
      ATLASSERT(did_parse);
   }

   if (!did_parse)
   {
      girderUnitWeight = ::ConvertToSysUnits(150.0, unitMeasure::LbfPerFeet3);
   }

   return TRUE;
}

OptionalDesignHarpedFillUtil::StrandRowSet OptionalDesignHarpedFillUtil::GetStrandRowSet(IBroker* pBroker, const pgsPointOfInterest& midPoi)
{
   GET_IFACE2(pBroker, IStrandGeometry, pStrandGeometry );
   GET_IFACE2(pBroker,IGirder,pGirder);

   // Need girder height - strands are measured from top downward
   Float64 hg = pGirder->GetHeight(midPoi);

   // Want to list filled strands in each row location. Loop over and build fill string
   StrandRowSet strandrows;

   // Harped
   CComPtr<IPoint2dCollection> hs_points;
   pStrandGeometry->GetStrandPositions(midPoi, pgsTypes::Harped, &hs_points);

   RowIndexType nrows = pStrandGeometry->GetNumRowsWithStrand(midPoi,pgsTypes::Harped);
   for (RowIndexType rowIdx=0; rowIdx!=nrows; rowIdx++)
   {
      std::vector<StrandIndexType> hstrands = pStrandGeometry->GetStrandsInRow(midPoi, rowIdx, pgsTypes::Harped);
      for (std::vector<StrandIndexType>::iterator sit=hstrands.begin(); sit!=hstrands.end(); sit++)
      {
         StrandIndexType idx = *sit;
         CComPtr<IPoint2d> point;
         hs_points->get_Item(idx,&point);
         Float64 Y;
         point->get_Y(&Y);

         Float64 elev = hg + Y; // measure from bottom

         Float64 X;
         point->get_X(&X);

         if (X>0.0)
         {
            TCHAR fill_char = GetFillString(X);

            StrandRow srow(elev);
            StrandRowIter srit = strandrows.find(srow);
            if (srit != strandrows.end())
            {
               StrandRow& row(const_cast<StrandRow&>(*srit));
               row.fillListString += fill_char;
            }
            else
            {
               srow.fillListString += fill_char;
               strandrows.insert(srow);
            }
         }
      }
   }


   // Straight
   CComPtr<IPoint2dCollection> ss_points;
   pStrandGeometry->GetStrandPositions(midPoi, pgsTypes::Straight, &ss_points);

   nrows = pStrandGeometry->GetNumRowsWithStrand(midPoi,pgsTypes::Straight);
   for (RowIndexType rowIdx=0; rowIdx!=nrows; rowIdx++)
   {
      std::vector<StrandIndexType> sstrands = pStrandGeometry->GetStrandsInRow(midPoi, rowIdx, pgsTypes::Straight);
      for (std::vector<StrandIndexType>::iterator sit=sstrands.begin(); sit!=sstrands.end(); sit++)
      {
         StrandIndexType idx = *sit;
         CComPtr<IPoint2d> point;
         ss_points->get_Item(idx,&point);
         Float64 Y;
         point->get_Y(&Y);

         Float64 elev = hg + Y; // measure from bottom

         Float64 X;
         point->get_X(&X);

         if (X>0.0)
         {
            TCHAR fill_char = GetFillString(X);

            StrandRow srow(elev);
            StrandRowIter srit = strandrows.find(srow);
            if (srit != strandrows.end())
            {
               StrandRow& row(const_cast<StrandRow&>(*srit));
               row.fillListString += fill_char;
            }
            else
            {
               srow.fillListString += fill_char;
               strandrows.insert(srow);
            }
         }
      }
   }

   return strandrows;
}

// class to help sort strands
class StrandLoc
{
public:
   StrandLoc(Float64 x, Float64 y, StrandIndexType idx):
   X(x), Y(y), Idx(idx)
   {;}

   bool operator < (const StrandLoc& rOther) const
   {
      if (Y < rOther.Y)
         return true;
      else if (Y > rOther.Y)
         return false;
      else if (X < rOther.X)
         return true;
      else if (X > rOther.X)
         return false;
      else if (Idx < rOther.Idx)
         return true;
      else
         return false;
   }

   Float64 X, Y;
   StrandIndexType Idx;
};

void MakeHarpedCloneStraight(const GirderLibraryEntry* pGdrEntry, GirderLibraryEntry* pGdrClone)
{
   pGdrClone->ClearAllStrands();

   // Cycle through all permanent strands. Change to straight in same global order
   StrandIndexType np = pGdrEntry->GetPermanentStrandGridSize();
   for (StrandIndexType ip=0; ip<np; ip++)
   {
      GirderLibraryEntry::psStrandType sType;
      GridIndexType gridIdx;

      pGdrEntry->GetGridPositionFromPermStrandGrid(ip, &sType, &gridIdx);

      StrandIndexType istr;
      if (sType==pgsTypes::Straight)
      {
         // Copy straight strands and make all debondable
         Float64 Xstart, Ystart, Xend, Yend;
         bool canDebond;
         pGdrEntry->GetStraightStrandCoordinates(gridIdx, &Xstart, &Ystart, &Xend, &Yend, &canDebond);

         istr = pGdrClone->AddStraightStrandCoordinates(Xstart, Ystart, Xstart, Ystart, true);
      }
      else if (sType==pgsTypes::Harped)
      {
         Float64 Xstart, Ystart, Xhp, Yhp, Xend, Yend;
         pGdrEntry->GetHarpedStrandCoordinates(gridIdx, &Xstart, &Ystart, &Xhp, &Yhp, &Xend, &Yend);

         istr = pGdrClone->AddStraightStrandCoordinates(Xstart, Ystart, Xstart, Ystart, false);
      }
      else
      {
         ATLASSERT(false); // TxDOT girders should not have temp strands
         break;
      }

      // make permanent strand order same as original
      pGdrClone->AddStrandToPermStrandGrid(GirderLibraryEntry::stStraight, istr-1);
   }
}

std::list<ColumnIndexType> ComputeTableCols(const std::vector<CGirderKey>& girderKeys)
{
   // Idea here is to break tables at spans. 
   // First build list of sizes of contiguous blocks of spans
   std::list<ColumnIndexType> contiguous_blocks1;
   SpanIndexType curr_span(INVALID_INDEX);
   bool first=false;
   std::vector<CGirderKey>::const_iterator it(girderKeys.begin());
   std::vector<CGirderKey>::const_iterator itEnd(girderKeys.end());
   for( ; it != itEnd; it++)
   {
      const CGirderKey& girderKey = *it;
      SpanIndexType new_span = girderKey.groupIndex;
      GirderIndexType new_gdr = girderKey.girderIndex;

      if (first || curr_span!=new_span)
      {
         first = false;
         curr_span = new_span;
         contiguous_blocks1.push_back(1);
      }
      else
      {
         contiguous_blocks1.back()++;
      }
   }

   // Next break blocks into list of table-sized chunks 
   std::list<ColumnIndexType> contiguous_blocks2;
   for(std::list<ColumnIndexType>::const_iterator it=contiguous_blocks1.begin(); it!=contiguous_blocks1.end(); it++)
   {
      ColumnIndexType ncols = *it;
      if (ncols > MAX_TBL_COLS)
      {
         ColumnIndexType num_big_chunks = ncols / MAX_TBL_COLS;
         ColumnIndexType rmdr = ncols % MAX_TBL_COLS;

         for (ColumnIndexType ich=0; ich<num_big_chunks; ich++)
         {
            contiguous_blocks2.push_back(MAX_TBL_COLS);
         }

         if(rmdr != 0)
         {
            contiguous_blocks2.push_back(rmdr);
         }
      }
      else
      {
         contiguous_blocks2.push_back(ncols);
      }
   }

   // Now we have a "right-sized" columns, but we could have a list of one-column tables, which
   // would be ugly. If all num colums are LE than min, combine into a wider, but not pretty table
   bool is_ugly = true;
   for(std::list<ColumnIndexType>::const_iterator it=contiguous_blocks2.begin(); it!=contiguous_blocks2.end(); it++)
   {
      ColumnIndexType ncols = *it;
      if (ncols > MIN_TBL_COLS)
      {
         is_ugly = false; // we have at least one table of minimum width - we're not ugly.
         break;
      }
   }

   std::list<ColumnIndexType> final_blocks;
   if (!is_ugly)
   {
      final_blocks = contiguous_blocks2;
   }
   else
   {
      // work to combine blocks
      std::list<ColumnIndexType>::const_iterator it=contiguous_blocks2.begin();
      while(it!=contiguous_blocks2.end())
      {
         ColumnIndexType ncols = *it;
         while (ncols<=MAX_TBL_COLS)
         {
            it++;
            if (it==contiguous_blocks2.end() || ncols+(*it) > MAX_TBL_COLS)
            {
               final_blocks.push_back(ncols);
               break;
            }
            else
            {
               ncols+= (*it);
            }
         }
      }
   }

   return final_blocks;
}

bool IsTxDOTStandardStrands(bool isHarpedDesign, CStrandData::StrandDefinitionType sdtType, const CSegmentKey& segmentKey, IBroker* pBroker)
{
   GET_IFACE2(pBroker, IStrandGeometry, pStrandGeometry );

   StrandIndexType ns = pStrandGeometry->GetStrandCount(segmentKey,pgsTypes::Straight);
   StrandIndexType nh = pStrandGeometry->GetStrandCount(segmentKey,pgsTypes::Harped);
   StrandIndexType nperm = ns + nh;

   if (nperm == 0)
   {
      return false; // no strands, not standard
   }
   else if (sdtType == CStrandData::sdtTotal)
   {
      // strands filled using library order. always standard
      return true;
   }
   else if (sdtType == CStrandData::sdtStraightHarped)
   {
      // check if strands entered straight/harped match library order. then standard
      StrandIndexType tns, tnh;
      if (pStrandGeometry->ComputeNumPermanentStrands(nperm, segmentKey, &tns, &tnh))
      {
         if (tns == ns && tnh == nh)
         {
            return true;
         }
      }
   }

   if (isHarpedDesign)
   {
      // standard harped designs must be filled using library order
      return false;
   }
   else if (sdtType == CStrandData::sdtDirectSelection)
   {
      // This is the hard one. We have a straight design, possibly with raised strands. In order to be standard;
      // the bottom half of the girder must be filled filling each row completely from the bottom up.
   	GET_IFACE2(pBroker, IPointOfInterest, pPointOfInterest );
      PoiList vPoi;
      pPointOfInterest->GetPointsOfInterest(segmentKey, POI_5L | POI_SPAN, &vPoi);
	   ATLASSERT(vPoi.size() == 1);
      const pgsPointOfInterest& pmid(vPoi.back());

      // Get list of strand rows filled by current project
      StrandRowUtil::StrandRowSet strandrows = StrandRowUtil::GetStrandRowSet(pBroker, pmid);

      // Get list of strand rows for case with all possible permanent strands filled
      StrandRowUtil::StrandRowSet popstrandrows = GetFullyPopulatedStrandRowSet(pBroker, pmid);

      // Only consider strands in the bottom half of the girder (raised strands are standard)
      GET_IFACE2(pBroker,IGirder,pGirder);
      Float64 hg2 = pGirder->GetHeight(pmid) / 2.0;

      bool isStandard(true);
      bool oneBelow(false); // at least one row must be below mid-height
      RowIndexType numPartiallyFilledRows = 0; // we can have one partially filled row only
      StrandRowUtil::StrandRowIter itstr = strandrows.begin();
      StrandRowUtil::StrandRowIter itpopstr = popstrandrows.begin();
      while (itstr != strandrows.end())
      {
         if (itstr->Elevation >= hg2)
         {
            // row is above half height. we are done and stardard if at least one row is below mid-height
            isStandard = oneBelow;
            break;
         }
         else if (!IsEqual(itstr->Elevation, itpopstr->Elevation))
         {
            // can't skip possible rows when we are below half height - rows must be continuously filled from bottom
            isStandard = false;
            break;
         }
         else if (itstr->Count != itpopstr->Count)
         {
            // a partially filled row
            if (++numPartiallyFilledRows > 1)
            {
               // can only have one partially filled row below half height
               isStandard = false;
               break;
            }
         }
         else // (itstr->Count == itpopstr->Count) // by default
         {
            // a fully filled row
            if (numPartiallyFilledRows > 0)
            {
               // cannot have a fully filled row above a partially filled row
               isStandard = false;
               break;
            }
         }

         oneBelow = true;
         itstr++;
         itpopstr++;
      }

      return isStandard;
   }
   else
   {
      return false; // myriad of other cases fall through the cracks
   }
}

StrandRowUtil::StrandRowSet GetFullyPopulatedStrandRowSet(IBroker* pBroker, const pgsPointOfInterest& midPoi)
{
   GET_IFACE2(pBroker, IStrandGeometry, pStrandGeometry );
   GET_IFACE2(pBroker,IGirder,pGirder);
   GET_IFACE2(pBroker,IBridge,pBridge);
   // Need girder height - strands are measured from top downward
   Float64 hg = pGirder->GetHeight(midPoi);

   const CSegmentKey&  rsegmentKey = midPoi.GetSegmentKey();

   // Set up a strand fill configuration that fills all possible straight and harped strands
   GDRCONFIG gdrconfig = pBridge->GetSegmentConfiguration(rsegmentKey);
   PRESTRESSCONFIG&  rpsconfig = gdrconfig.PrestressConfig;

   StrandIndexType maxss = pStrandGeometry->GetMaxStrands(rsegmentKey, pgsTypes::Straight);
   ConfigStrandFillVector ssfillvec = pStrandGeometry->ComputeStrandFill(rsegmentKey, pgsTypes::Straight, maxss);

   rpsconfig.SetStrandFill(pgsTypes::Straight, ssfillvec);

   StrandIndexType maxhs = pStrandGeometry->GetMaxStrands(rsegmentKey, pgsTypes::Harped);
   ConfigStrandFillVector hsfillvec = pStrandGeometry->ComputeStrandFill(rsegmentKey, pgsTypes::Harped, maxhs);

   rpsconfig.SetStrandFill(pgsTypes::Harped, hsfillvec);

   // Want number of strands in each row location. Count number of straight and harped per row
   StrandRowUtil::StrandRowSet strandrows;

   // Straight
   CComPtr<IPoint2dCollection> ss_points;
   pStrandGeometry->GetStrandPositionsEx(midPoi, rpsconfig, pgsTypes::Straight, &ss_points);

   StrandIndexType nss;
   ss_points->get_Count(&nss);

   for (StrandIndexType iss=0; iss<nss; iss++)
   {
      CComPtr<IPoint2d> point;
      ss_points->get_Item(iss,&point);
      Float64 Y;
      point->get_Y(&Y);

      StrandRowUtil::StrandRow srow(Y + hg); // from bottom of girder
      StrandRowUtil::StrandRowIter srit = strandrows.find(srow);
      if (srit != strandrows.end())
      {
         StrandRowUtil::StrandRow& strandRow(const_cast<StrandRowUtil::StrandRow&>(*srit));
         strandRow.Count++;
      }
      else
      {
         strandrows.insert(srow);
      }
   }

   // Harped
   CComPtr<IPoint2dCollection> hs_points;
   pStrandGeometry->GetStrandPositionsEx(midPoi, rpsconfig, pgsTypes::Harped, &hs_points);

   StrandIndexType nhs;
   hs_points->get_Count(&nhs);

   for (StrandIndexType ihs=0; ihs<nhs; ihs++)
   {
      CComPtr<IPoint2d> point;
      hs_points->get_Item(ihs,&point);
      Float64 Y;
      point->get_Y(&Y);

      StrandRowUtil::StrandRow srow(Y + hg); // from bottom of girder
      StrandRowUtil::StrandRowIter srit = strandrows.find(srow);
      if (srit != strandrows.end())
      {
         StrandRowUtil::StrandRow& strandRow(const_cast<StrandRowUtil::StrandRow&>(*srit));
         strandRow.Count++;
      }
      else
      {
         strandrows.insert(srow);
      }
   }

   return strandrows;
}