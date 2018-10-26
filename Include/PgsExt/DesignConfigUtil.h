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

#pragma once

#include <PgsExt\PgsExtExp.h>
#include <PgsExt\StrandData.h>
#include <PgsExt\ShearData.h>
#include <IFace\Bridge.h>

// Various utility functions that operate on CONFIG objects in PGSuperTypes.h

//////////////////////////////////////////////////////////////////////////
///////////// Functions for Strand Design...  ////////////////////////////
//////////////////////////////////////////////////////////////////////////

// convert config fill to compacted CDirectStrandFillCollection
CDirectStrandFillCollection PGSEXTFUNC ConvertConfigToDirectStrandFill(const ConfigStrandFillVector& rconfigfil);
ConfigStrandFillVector PGSEXTFUNC ConvertDirectToConfigFill(IStrandGeometry* pStrandGeometry, pgsTypes::StrandType type, LPCTSTR strGirderName, const CDirectStrandFillCollection& coll);
ConfigStrandFillVector PGSEXTFUNC ConvertDirectToConfigFill(IStrandGeometry* pStrandGeometry, pgsTypes::StrandType type, const CSegmentKey& segmentKey, const CDirectStrandFillCollection& coll);

// mean/lean compute class for computing strand ordering information
class PGSEXTCLASS ConfigStrandFillTool
{
public:
   // Return position indices (as from IStrandGeometry::GetStrandPositions) for our fill for given grid index. 
   // One or both may be INVALID_INDEX if no strands filled at gridIndex
   void GridIndexToStrandPositionIndex(GridIndexType gridIndex, StrandIndexType* pStrand1, StrandIndexType* pStrand2) const;


   // Get grid index for a given strand position index
   // Also get the other strand position index if two strands are located at this grid position, or INVALID_INDEX if only one
   void StrandPositionIndexToGridIndex(StrandIndexType strandPosIndex, GridIndexType* pGridIndex, StrandIndexType* pOtherStrandPosition) const;

   // Only way to construct us is with a valid fill vector
   ConfigStrandFillTool(const ConfigStrandFillVector& rFillVec);
private:
   ConfigStrandFillTool();

   std::vector< std::pair<StrandIndexType, StrandIndexType> > m_CoordinateIndices;
};

//////////////////////////////////////////////////////////////////////////
///////////// Functions for Stirrup Design... ////////////////////////////
//////////////////////////////////////////////////////////////////////////
template<class IteratorType>
ZoneIndexType GetZoneIndexAtLocation(Float64 location, Float64 girderLength, Float64 startSupportLoc, Float64 endSupportLoc,
                             bool bSymmetrical, IteratorType& rItBegin, IteratorType& rItEnd, ZoneIndexType collSize)
{
   CHECK(collSize>0);

   ZoneIndexType zone = 0;

   if (bSymmetrical)
   {
      bool on_left=true;

      if (girderLength/2. < location)  // zones are symmetric about mid-span
      {
         location = girderLength-location;
         on_left=false;
      }
      ATLASSERT(0 <= location);

      Float64 end_dist;
      if (on_left)
         end_dist = startSupportLoc;
      else
         end_dist = girderLength-endSupportLoc;

      // find zone
      Float64 ezloc=0.0;
      ZoneIndexType znum=0;
      bool found=false;
      // need to give value a little 'shove' so that pois inboard from the support
      // are assigned the zone to the left, and pois outboard from the support
      // are assigned the zone to the right.
      const Float64 tol = 1.0e-6;
      for (IteratorType it = rItBegin; it != rItEnd; it++)
      {
         ezloc += it->ZoneLength;
         if (location < end_dist+tol)
         {
            if (location+tol < ezloc)
            {
               found = true;
               break;
            }
         }
         else
         {
            if (location < ezloc+tol)
            {
               found = true;
               break;
            }
         }
         znum++;
      }

      if (found)
      {
         zone = znum;
      }
      else
      {
         // not found - must be in middle
         ATLASSERT(collSize != 0); // -1 will cause an underflow 
         zone = collSize-1;
      }
   }
   else
   {
      // not symmetrical
      // find zone
      Float64 ezloc=0.0;
      ZoneIndexType znum=0;
      bool found=false;
      // need to give value a little 'shove' so that pois inboard from the support
      // are assigned the zone to the left, and pois outboard from the support
      // are assigned the zone to the right.
      const Float64 tol = 1.0e-6;
      for (IteratorType it = rItBegin; it != rItEnd; it++)
      {
         ezloc += it->ZoneLength;
         if (location < startSupportLoc + tol)
         {
            if (location+tol<ezloc)
            {
               found = true;
               break;
            }
         }
         else if (location + tol > endSupportLoc)
         {
            if (location<ezloc+tol)
            {
               found = true;
               break;
            }
         }
         else
         {
            if (location<ezloc+tol)
            {
               found = true;
               break;
            }
         }

         znum++;
      }

      if (found)
      {
         zone = znum;
      }
      else
      {
         zone = collSize-1; // extend last zone to end of girder
      }
   }

   return zone;
}


enum PrimaryStirrupType {getVerticalStirrup, getHorizShearStirrup};

Float64 PGSEXTFUNC GetPrimaryStirrupAvs(const STIRRUPCONFIG& config, PrimaryStirrupType type, Float64 location, Float64 gdrLength, Float64 leftSupportLoc,Float64 rgtSupportLoc, matRebar::Size* pSize, Float64* pSingleBarArea, Float64* pNBars, Float64* pSpacing);
Float64 PGSEXTFUNC GetAdditionalHorizInterfaceAvs(const STIRRUPCONFIG& config, Float64 location, Float64 gdrLength, Float64 leftSupportLoc,Float64 rgtSupportLoc, matRebar::Size* pSize, Float64* pSingleBarArea, Float64* pNBars, Float64* pSpacing);
void PGSEXTFUNC GetConfinementInfoFromStirrupConfig(const STIRRUPCONFIG& config, Float64 reqdStartZl, matRebar::Size* pStartRBsiz, Float64* pStartZL, Float64* pStartS, Float64 reqdEndZl,   matRebar::Size* pEndRBsiz,   Float64* pEndZL,   Float64* pEndS);
Float64 PGSEXTFUNC GetPrimaryAvLeftEnd(const STIRRUPCONFIG& config, matRebar::Type barType, matRebar::Grade barGrade,Float64 gdrLength, Float64 rangeLength);
Float64 PGSEXTFUNC GetPrimaryAvRightEnd(const STIRRUPCONFIG& config, matRebar::Type barType, matRebar::Grade barGrade,Float64 gdrLength, Float64 rangeLength);
void PGSEXTFUNC GetSplittingAvFromStirrupConfig(const STIRRUPCONFIG& config, matRebar::Type barType, matRebar::Grade barGrade, Float64 gdrLength, Float64 reqdStartZl, Float64* pStartAv,Float64 reqdEndZl,   Float64* pEndAv);
void PGSEXTFUNC WriteShearDataToStirrupConfig(const CShearData2* pShearData, STIRRUPCONFIG& rConfig);
bool PGSEXTFUNC DoAllStirrupsEngageDeck( const STIRRUPCONFIG& config);

// NOTE: The method below could be useful to a design algorithm sometime in the future.
/*
void  PGSEXTFUNC WriteLongitudinalRebarDataToConfig(const CLongitudinalRebarData& rRebarData, LONGITUDINALREBARCONFIG& rConfig)'
*/