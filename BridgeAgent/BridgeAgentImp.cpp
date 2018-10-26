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

// BridgeAgentImp.cpp : Implementation of CBridgeAgentImp
#include "stdafx.h"
#include "BridgeAgent.h"
#include "BridgeAgent_i.h"
#include "BridgeAgentImp.h"
#include "BridgeHelpers.h"
#include "BridgeGeometryModelBuilder.h"
#include "..\PGSuperException.h"
#include "DeckEdgeBuilder.h"

#include <PsgLib\LibraryManager.h>
#include <PsgLib\GirderLibraryEntry.h>
#include <PsgLib\ConnectionLibraryEntry.h>
#include <PsgLib\UnitServer.h>

#include <PgsExt\BridgeDescription2.h>
#include <PgsExt\DeckDescription2.h>
#include <PgsExt\PierData2.h>
#include <PgsExt\SpanData2.h>
#include <PgsExt\PrecastSegmentData.h>
#include <PgsExt\TemporarySupportData.h>
#include <PgsExt\ClosurePourData.h>

#include <PgsExt\GirderPointOfInterest.h>
#include <PsgLib\ShearData.h>
#include <PgsExt\LongitudinalRebarData.h>
#include <PgsExt\PointLoadData.h>
#include <PgsExt\DistributedLoadData.h>
#include <PgsExt\MomentLoadData.h>
#include <PgsExt\StatusItem.h>
#include <PgsExt\GirderLabel.h>

#include <MathEx.h>
#include <Math\Polynomial2d.h>
#include <Math\CompositeFunction2d.h>
#include <Math\LinFunc2d.h>
#include <System\Flags.h>
#include <Material\Material.h>
#include <GeomModel\ShapeUtils.h>

#include <IFace\DrawBridgeSettings.h>
#include <EAF\EAFDisplayUnits.h>
#include <IFace\ShearCapacity.h>
#include <IFace\GirderHandling.h>
#include <IFace\GirderHandlingSpecCriteria.h>
#include <IFace\StatusCenter.h>
#include <IFace\DocumentType.h>
#include <IFace\BeamFactory.h>

#include <BridgeModeling\DrawSettings.h>

#include <BridgeModeling\LrLayout.h>
#include <BridgeModeling\LrFlexiZone.h>
#include <BridgeModeling\LrRowPattern.h>

#include <DesignConfigUtil.h>

#include <algorithm>
#include <cctype>

#include <afxext.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


DECLARE_LOGFILE;


#define CLEAR_ALL       0
#define COGO_MODEL      1
#define BRIDGE          2
#define GIRDER          3

// Capacity problem validation states
#define CAPPROB_NC        0x0001
#define CAPPROB_CMP_INT   0x0002
#define CAPPROB_CMP_EXT   0x0004


#define VALIDATE(x) {if ( !Validate((x)) ) THROW_SHUTDOWN(_T("Fatal Error in Bridge Agent"),XREASON_AGENTVALIDATIONFAILURE,true);}
#define INVALIDATE(x) Invalidate((x))

#define STARTCONNECTION _T("Left Connection")  // must be this value for the PrecastGirder model
#define ENDCONNECTION   _T("Right Connection") // must be this value for the PrecastGirder model

#define LEFT_DECK_EDGE_LAYOUT_LINE_ID  -500
#define RIGHT_DECK_EDGE_LAYOUT_LINE_ID -501

//function to translate project loads to bridge loads

IUserDefinedLoads::UserDefinedLoadCase Project2BridgeLoads(UserLoads::LoadCase plc)
{
   if (plc==UserLoads::DC)
      return IUserDefinedLoads::userDC;
   else if (plc==UserLoads::DW)
      return IUserDefinedLoads::userDW;
   else if (plc==UserLoads::LL_IM)
      return IUserDefinedLoads::userLL_IM;
   else
   {
      ATLASSERT(0);
      return  IUserDefinedLoads::userDC;
   }
}

// function for computing debond factor from development length
inline Float64 GetDevLengthAdjustment(Float64 bonded_length, Float64 dev_length, Float64 xfer_length, Float64 fps, Float64 fpe)
{
   if (bonded_length <= 0.0)
   {
      // strand is unbonded at location, no more to do
      return 0.0;
   }
   else
   {
      Float64 adjust = -999; // dummy value, helps with debugging

      if ( bonded_length <= xfer_length)
      {
         adjust = (fpe < fps ? (bonded_length*fpe) / (xfer_length*fps) : 1.0);
      }
      else if ( bonded_length < dev_length )
      {
         adjust = (fpe + (bonded_length - xfer_length)*(fps-fpe)/(dev_length - xfer_length))/fps;
      }
      else
      {
         adjust = 1.0;
      }

      adjust = IsZero(adjust) ? 0 : adjust;
      adjust = ::ForceIntoRange(0.0,adjust,1.0);
      return adjust;
   }
}



// an arbitrary large length
static const Float64 BIG_LENGTH = 1.0e12;
// floating tolerance
static const Float64 TOL=1.0e-04;

// Function to choose confinement bars from primary and additional
static void ChooseConfinementBars(Float64 requiredZoneLength, 
                                  Float64 primSpc, Float64 primZonL, matRebar::Size primSize,
                                  Float64 addlSpc, Float64 addlZonL, matRebar::Size addlSize,
                                  matRebar::Size* pSize, Float64* pProvidedZoneLength, Float64* pSpacing)
{
   // Start assuming the worst
   *pSize = matRebar::bsNone;
   *pProvidedZoneLength = 0.0;
   *pSpacing = 0.0;

   // methods must have bars and non-zero spacing just to be in the running
   bool is_prim = primSize!=matRebar::bsNone && primSpc>0.0;
   bool is_addl = addlSize!=matRebar::bsNone && addlSpc>0.0;

   // Both must meet required zone length to qualify for a run-off
   if (is_prim && is_addl && primZonL+TOL > requiredZoneLength && addlZonL+TOL > requiredZoneLength)
   {
      // both meet zone length req. choose smallest spacing
      if (primSpc < addlSpc)
      {
         *pSize = primSize;
         *pProvidedZoneLength = primZonL;
         *pSpacing = primSpc;
      }
      else
      {
         *pSize = addlSize;
         *pProvidedZoneLength = addlZonL;
         *pSpacing = addlSpc;
      }
   }
   else if (is_prim && primZonL+TOL > requiredZoneLength)
   {
      *pSize = primSize;
      *pProvidedZoneLength = primZonL;
      *pSpacing = primSpc;
   }
   else if (is_addl && addlZonL+TOL > requiredZoneLength)
   {
      *pSize = addlSize;
      *pProvidedZoneLength = addlZonL;
      *pSpacing = addlSpc;
   }
   else if (is_addl)
   {
      *pSize = addlSize;
      *pProvidedZoneLength = addlZonL;
      *pSpacing = addlSpc;
   }
   else if (is_prim)
   {
      *pSize = primSize;
      *pProvidedZoneLength = primZonL;
      *pSpacing = primSpc;
   }
}

/////////////////////////////////////////////////////////
// Utilities for Dealing with strand array conversions
inline void IndexArray2ConfigStrandFillVec(IIndexArray* pArray, ConfigStrandFillVector& rVec)
{
   rVec.clear();
   if (pArray!=NULL)
   {
      CollectionIndexType cnt;
      pArray->get_Count(&cnt);
      rVec.reserve(cnt);

      CComPtr<IEnumIndexArray> enum_array;
      pArray->get__EnumElements(&enum_array);
      StrandIndexType value;
      while ( enum_array->Next(1,&value,NULL) != S_FALSE )
      {
         rVec.push_back(value);
      }
   }
}

bool AreStrandsInConfigFillVec(const ConfigStrandFillVector& rHarpedFillArray)
{
   ConfigStrandFillConstIterator it =    rHarpedFillArray.begin();
   ConfigStrandFillConstIterator itend = rHarpedFillArray.end();
   while(it!=itend)
   {
      if(*it>0)
         return true;

      it++;
   }

   return false;
}

StrandIndexType CountStrandsInConfigFillVec(const ConfigStrandFillVector& rHarpedFillArray)
{
   StrandIndexType cnt(0);

   ConfigStrandFillConstIterator it =    rHarpedFillArray.begin();
   ConfigStrandFillConstIterator itend = rHarpedFillArray.end();
   while(it!=itend)
   {
      cnt += *it;
      it++;
   }

   return cnt;
}


// Wrapper class to turn a ConfigStrandFillVector into a IIndexArray
// THIS IS A VERY MINIMAL WRAPPER USED TO PASS STRAND DATA INTO THE WBFL
//   -Read only
//   -Does not reference count
//   -Does not clone
//   -Does not support many functions of IIndexArray
class CIndexArrayWrapper : public IIndexArray
{
   // we are wrapping this container
   const ConfigStrandFillVector& m_Values;

   CIndexArrayWrapper(); // no default constuct
public:
   CIndexArrayWrapper(const ConfigStrandFillVector& vec):
      m_Values(vec)
   {;}

   // IUnknown
   virtual HRESULT STDMETHODCALLTYPE QueryInterface( REFIID riid, __RPC__deref_out void __RPC_FAR *__RPC_FAR *ppvObject)
   { ATLASSERT(0); return E_FAIL;}
   virtual ULONG STDMETHODCALLTYPE AddRef( void)
   { return 1;}
   virtual ULONG STDMETHODCALLTYPE Release( void)
   {return 1;}

   // IIndexArray
	STDMETHOD(Find)(/*[in]*/CollectionIndexType value, /*[out,retval]*/CollectionIndexType* fndIndex)
   {
      ATLASSERT(0);
      return E_FAIL;
   }
	STDMETHOD(ReDim)(/*[in]*/CollectionIndexType size)
   {
      ATLASSERT(0);
      return E_FAIL;
   }

	STDMETHOD(Clone)(/*[out,retval]*/IIndexArray* *clone)
   {
      ATLASSERT(0);
      return E_FAIL;
   }
	STDMETHOD(get_Count)(/*[out, retval]*/ CollectionIndexType *pVal)
   {
	   *pVal = m_Values.size();
	   return S_OK;
   }
	STDMETHOD(Clear)()
   {
      ATLASSERT(0);
      return E_FAIL;
   }
	STDMETHOD(Reserve)(/*[in]*/CollectionIndexType count)
   {
      ATLASSERT(0);
      return E_FAIL;
   }
	STDMETHOD(Insert)(/*[in]*/CollectionIndexType relPosition, /*[in]*/CollectionIndexType item)
   {
      ATLASSERT(0);
      return E_FAIL;
   }
	STDMETHOD(Remove)(/*[in]*/CollectionIndexType relPosition)
   {
      ATLASSERT(0);
      return E_FAIL;
   }
	STDMETHOD(Add)(/*[in]*/CollectionIndexType item)
   {
      ATLASSERT(0);
      return E_FAIL;
   }
	STDMETHOD(get_Item)(/*[in]*/CollectionIndexType relPosition, /*[out, retval]*/ CollectionIndexType *pVal)
   {
      try
      {
         *pVal = m_Values[relPosition];
      }
      catch(...)
      {
         ATLASSERT(0);
         return E_INVALIDARG;
      }
	   return S_OK;
   }
	STDMETHOD(put_Item)(/*[in]*/CollectionIndexType relPosition, /*[in]*/ CollectionIndexType newVal)
   {
      ATLASSERT(0);
      return E_FAIL;
   }
	STDMETHOD(get__NewEnum)(struct IUnknown ** )
   {
      ATLASSERT(0);
      return E_FAIL;
   }
	STDMETHOD(get__EnumElements)(struct IEnumIndexArray ** )
   {
      ATLASSERT(0);
      return E_FAIL;
   }
   STDMETHOD(Assign)(/*[in]*/CollectionIndexType numElements, /*[in]*/CollectionIndexType value)
   {
      ATLASSERT(0);
      return E_FAIL;
   }
};

///////////////////////////////////////////////////////////
// Exception-safe class for blocking infinite recursion
class SimpleMutex
{
public:

   SimpleMutex(Uint16& flag, Uint16 value):
   m_MutexValue(flag)
   {
      flag = value;
   }

   ~SimpleMutex()
   {
      m_MutexValue = m_Default;
   }

   static const Uint16 m_Default; // mutex is not blocking if set to Default

private:
   Uint16& m_MutexValue;
};

const Uint16 SimpleMutex::m_Default=666; // evil number to search for


static Uint16 st_MutexValue = SimpleMutex::m_Default; // store m_level

/////////////////////////////////////////////////////////////////////////////
// CBridgeAgentImp

HRESULT CBridgeAgentImp::FinalConstruct()
{
   HRESULT hr = m_BridgeGeometryTool.CoCreateInstance(CLSID_BridgeGeometryTool);
   if ( FAILED(hr) )
      return hr;

   hr = m_CogoEngine.CoCreateInstance(CLSID_CogoEngine);
   if ( FAILED(hr) )
      return hr;


   return S_OK;
}

void CBridgeAgentImp::FinalRelease()
{
}

void CBridgeAgentImp::Invalidate( Uint16 level )
{
//   LOG(_T("Invalidating"));

   if ( level <= BRIDGE )
   {
//      LOG(_T("Invalidating Bridge Model"));

      m_CogoModel.Release();
      m_Bridge.Release();
      m_HorzCurveKeys.clear();
      m_VertCurveKeys.clear();


      // Must be valided at least past COGO_MODEL
      if ( COGO_MODEL < level )
         level = COGO_MODEL;

      m_ConcreteManager.Reset();

      // If bridge is invalid, so are points of interest
      m_ValidatedPoi.clear();
      m_PoiMgr.RemoveAll();
      m_LiftingPoiMgr.RemoveAll();
      m_HaulingPoiMgr.RemoveAll();

      m_CriticalSectionState[0].clear();
      m_CriticalSectionState[1].clear();

      m_SectProps[SPT_GROSS].clear();
      m_SectProps[SPT_TRANSFORMED].clear();
      m_SectProps[SPT_NET_GIRDER].clear();
      m_SectProps[SPT_NET_DECK].clear();
      InvalidateUserLoads();


      // clear the cached shapes
      m_DeckShapes.clear();
      m_LeftBarrierShapes.clear();
      m_RightBarrierShapes.clear();

      m_LeftSlabEdgeOffset.clear();
      m_RightSlabEdgeOffset.clear();

      // cached sheardata
      InvalidateStirrupData();

      // remove our items from the status center
      GET_IFACE(IEAFStatusCenter,pStatusCenter);
      pStatusCenter->RemoveByStatusGroupID(m_StatusGroupID);
   }

   m_Level = level;
   if ( m_Level != 0 )
      m_Level -= 1;

//   LOG(_T("Invalidate complete - BridgeAgent at level ") << m_Level );
}

Uint16 CBridgeAgentImp::Validate( Uint16 level )
{
//   LOG(_T("Validating"));

   if (st_MutexValue != SimpleMutex::m_Default)
   {
      // mutex is blocking recursion
//      LOG(_T("Mutex is blocking recursion at level")<<st_MutexValue);
      ATLASSERT(level<=st_MutexValue); // this is bad. A call down the stack is requesting a higher level Validation
   }
   else
   {
      // use SimpleMutex class to block recursion
      SimpleMutex mutex(st_MutexValue, level);

      if ( level > m_Level )
      {
         VALIDATE_TO_LEVEL( COGO_MODEL,   BuildCogoModel );
         VALIDATE_TO_LEVEL( BRIDGE,       BuildBridgeModel );
         VALIDATE_AND_CHECK_TO_LEVEL( GIRDER,       BuildGirder,    ValidateGirder );
      }

  
      if (level>=BRIDGE)
         ValidateUserLoads();


//      LOG(_T("Validation complete - BridgeAgent at level ") << m_Level );
   }

      return m_Level;
}

//void CBridgeAgentImp::ValidateGirderPointsOfInterest(GirderIDType gdrID)
//{
//   ATLASSERT(false); // what do to here??? create poi for spliced girders
//}

void CBridgeAgentImp::ValidatePointsOfInterest(const CGirderKey& girderKey)
{
   // Bridge model, up to and including girders, must be valid before we can layout the poi
   VALIDATE(GIRDER);

   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();

   // Create a collection of girder keys for the girders who's POI need to be validated
   std::vector<CGirderKey> girderKeyList;

   GroupIndexType firstGrpIdx;
   GroupIndexType lastGrpIdx;
   if ( girderKey.groupIndex == ALL_GROUPS )
   {
      // validate all groups
      firstGrpIdx = 0;
      lastGrpIdx  = pBridgeDesc->GetGirderGroupCount()-1;
   }
   else
   {
      firstGrpIdx = girderKey.groupIndex;
      lastGrpIdx  = firstGrpIdx;
   }

   for ( GroupIndexType grpIdx = firstGrpIdx; grpIdx <= lastGrpIdx; grpIdx++ )
   {
      const CGirderGroupData* pGroup = pBridgeDesc->GetGirderGroup(grpIdx);

      GirderIndexType firstGdrIdx;
      GirderIndexType lastGdrIdx;
      if ( girderKey.girderIndex == ALL_GIRDERS )
      {
         firstGdrIdx = 0;
         lastGdrIdx = pGroup->GetGirderCount() - 1;
      }
      else
      {
         firstGdrIdx = girderKey.girderIndex;
         lastGdrIdx  = firstGdrIdx;
      }

      for ( GirderIndexType gdrIdx = firstGdrIdx; gdrIdx <= lastGdrIdx; gdrIdx++ )
      {
         CGirderKey key(grpIdx,gdrIdx);
         girderKeyList.push_back(key);
      } // girder loop
   } // group loop

   
   // Validate the POI
   std::vector<CGirderKey>::iterator keyIter(girderKeyList.begin());
   std::vector<CGirderKey>::iterator keyIterEnd(girderKeyList.end());
   for ( ; keyIter != keyIterEnd; keyIter++ )
   {
      CGirderKey& key = *keyIter;
      std::set<CGirderKey>::iterator found( m_ValidatedPoi.find( key ) );
      if ( found == m_ValidatedPoi.end() )
      {
         LayoutPointsOfInterest( key );
         m_ValidatedPoi.insert(key);
      }
   }
}

void CBridgeAgentImp::InvalidateUserLoads()
{
   m_bUserLoadsValidated = false;

   // remove our items from the status center
   GET_IFACE(IEAFStatusCenter,pStatusCenter);
   pStatusCenter->RemoveByStatusGroupID(m_LoadStatusGroupID);
}

void CBridgeAgentImp::ValidateUserLoads()
{
   // Bridge model must be valid first
   VALIDATE(BRIDGE);

   if (!m_bUserLoadsValidated)
   {
      // first make sure our data is wiped
      m_PointLoads.clear();
      m_DistributedLoads.clear();
      m_MomentLoads.clear();

      ValidatePointLoads();
      ValidateDistributedLoads();
      ValidateMomentLoads();

      m_bUserLoadsValidated = true;
   }
}

void CBridgeAgentImp::ValidatePointLoads()
{
   IntervalIndexType nIntervals = GetIntervalCount();
   m_PointLoads.resize(nIntervals);

   SpanIndexType nSpans = GetSpanCount();

   GET_IFACE(IEAFStatusCenter, pStatusCenter);
   GET_IFACE(IUserDefinedLoadData, pLoadData );
   GET_IFACE(IBridgeDescription, pIBridgeDesc);

   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();

   CollectionIndexType nLoads = pLoadData->GetPointLoadCount();
   for(CollectionIndexType loadIdx = 0; loadIdx < nLoads; loadIdx++)
   {
      const CPointLoadData& pointLoad = pLoadData->GetPointLoad(loadIdx);

      // need to loop over all spans if that is what is selected - use a vector to store span numbers
      std::vector<SpanIndexType> spans;
      if (pointLoad.m_SpanGirderKey.spanIndex == ALL_SPANS)
      {
         for (SpanIndexType spanIdx = 0; spanIdx < nSpans; spanIdx++)
            spans.push_back(spanIdx);
      }
      else
      {
         if(nSpans < pointLoad.m_SpanGirderKey.spanIndex+1)
         {
            CString strMsg;
            strMsg.Format(_T("Span %d for point load is out of range. Max span number is %d. This load will be ignored."), LABEL_SPAN(pointLoad.m_SpanGirderKey.spanIndex),nSpans);
            pgsPointLoadStatusItem* pStatusItem = new pgsPointLoadStatusItem(loadIdx,m_LoadStatusGroupID,m_scidPointLoadWarning,strMsg,pointLoad.m_SpanGirderKey);
            pStatusCenter->Add(pStatusItem);
            continue; // break out of this cycle
         }
         else
         {
            spans.push_back(pointLoad.m_SpanGirderKey.spanIndex);
         }
      }

      std::vector<SpanIndexType>::iterator spanIter(spans.begin());
      std::vector<SpanIndexType>::iterator spanIterEnd(spans.end());
      for ( ; spanIter != spanIterEnd; spanIter++ )
      {
         SpanIndexType span = *spanIter;

         GirderIndexType nGirders = GetGirderCountBySpan(span);

         std::vector<GirderIndexType> girders;
         if (pointLoad.m_SpanGirderKey.girderIndex == ALL_GIRDERS)
         {
            for (GirderIndexType i = 0; i < nGirders; i++)
               girders.push_back(i);
         }
         else
         {
            if(nGirders < pointLoad.m_SpanGirderKey.girderIndex+1)
            {
               CString strMsg;
               strMsg.Format(_T("Girder %s for point load is out of range. Max girder number is %s. This load will be ignored."), LABEL_GIRDER(pointLoad.m_SpanGirderKey.girderIndex), LABEL_GIRDER(nGirders-1));
               pgsPointLoadStatusItem* pStatusItem = new pgsPointLoadStatusItem(loadIdx,m_LoadStatusGroupID,m_scidPointLoadWarning,strMsg,pointLoad.m_SpanGirderKey);
               pStatusCenter->Add(pStatusItem);
               continue;
            }
            else
            {
               girders.push_back(pointLoad.m_SpanGirderKey.girderIndex);
            }
         }

         std::vector<GirderIndexType>::iterator gdrIter(girders.begin());
         std::vector<GirderIndexType>::iterator gdrIterEnd(girders.end());
         for ( ; gdrIter != gdrIterEnd; gdrIter++ )
         {
            GirderIndexType girder = *gdrIter;

            CSpanGirderKey thisSpanGirderKey(span,girder);

            UserPointLoad upl;
            upl.m_LoadCase = Project2BridgeLoads(pointLoad.m_LoadCase);
            upl.m_Description = pointLoad.m_Description;

            // only a light warning for zero loads - don't bail out
            if (IsZero(pointLoad.m_Magnitude))
            {
               CString strMsg;
               strMsg.Format(_T("Magnitude of point load is zero"));
               pgsPointLoadStatusItem* pStatusItem = new pgsPointLoadStatusItem(loadIdx,m_LoadStatusGroupID,m_scidPointLoadWarning,strMsg,thisSpanGirderKey);
               pStatusCenter->Add(pStatusItem);
            }

            upl.m_Magnitude = pointLoad.m_Magnitude;

            Float64 span_length = GetSpanLength(span,girder); // span length measured along CL girder

            if (pointLoad.m_Fractional)
            {
               if(0.0 <= pointLoad.m_Location && pointLoad.m_Location <= 1.0)
               {
                  upl.m_Location = pointLoad.m_Location * span_length;
               }
               else
               {
                  CString strMsg;
                  strMsg.Format(_T("Fractional location value for point load is out of range. Value must range from 0.0 to 1.0. This load will be ignored."));
                  pgsPointLoadStatusItem* pStatusItem = new pgsPointLoadStatusItem(loadIdx,m_LoadStatusGroupID,m_scidPointLoadWarning,strMsg,thisSpanGirderKey);
                  pStatusCenter->Add(pStatusItem);
                  continue;
               }
            }
            else
            {
               if(0.0 <= pointLoad.m_Location && pointLoad.m_Location <= span_length)
               {
                  upl.m_Location = pointLoad.m_Location;
               }
               else
               {
                  CString strMsg;
                  strMsg.Format(_T("Location value for point load is out of range. Value must range from 0.0 to span length. This load will be ignored."));
                  pgsPointLoadStatusItem* pStatusItem = new pgsPointLoadStatusItem(loadIdx,m_LoadStatusGroupID,m_scidPointLoadWarning,strMsg,thisSpanGirderKey);
                  pStatusCenter->Add(pStatusItem);
                  continue;
               }
            }

            // add a point of interest at this load location in every interval
            CSegmentKey segmentKey;
            Float64 segLocation;
            GetSegmentLocation(span,girder,pointLoad.m_Location,&segmentKey,&segLocation);

            Float64 segLength = GetSegmentLength(segmentKey);
            segLocation = ::ForceIntoRange(0.0,segLocation,segLength);

            pgsPointOfInterest poi(segmentKey,segLocation,POI_CONCLOAD);
            m_PoiMgr.AddPointOfInterest( poi );

            // put load into our collection
            IntervalIndexType intervalIdx = m_IntervalManager.GetInterval(pointLoad.m_EventIdx);
            ATLASSERT(intervalIdx != INVALID_INDEX);
            ATLASSERT(intervalIdx < m_PointLoads.size());
            PointLoadCollection& pointLoads = m_PointLoads[intervalIdx];
            PointLoadCollection::iterator itp( pointLoads.find(thisSpanGirderKey) );
            if ( itp == pointLoads.end() )
            {
               // not found, must insert
               std::pair<PointLoadCollection::iterator, bool> itbp( pointLoads.insert(PointLoadCollection::value_type(thisSpanGirderKey, std::vector<UserPointLoad>() )) );
               ATLASSERT(itbp.second);
               itbp.first->second.push_back(upl);
            }
            else
            {
               itp->second.push_back(upl);
            }
         }
      }
   }
}

void CBridgeAgentImp::ValidateDistributedLoads()
{
   IntervalIndexType nIntervals = GetIntervalCount();
   m_DistributedLoads.resize(nIntervals);

   SpanIndexType nSpans = GetSpanCount();

   GET_IFACE(IEAFStatusCenter,pStatusCenter);
   GET_IFACE( IUserDefinedLoadData, pUdl );

   CollectionIndexType nLoads = pUdl->GetDistributedLoadCount();
   for(CollectionIndexType loadIdx = 0; loadIdx < nLoads; loadIdx++)
   {
      const CDistributedLoadData& distLoad = pUdl->GetDistributedLoad(loadIdx);

      // need to loop over all spans if that is what is selected - user a vector to store span numbers
      std::vector<SpanIndexType> spans;
      if (distLoad.m_SpanGirderKey.spanIndex == ALL_SPANS)
      {
         for (SpanIndexType i = 0; i < nSpans; i++)
            spans.push_back(i);
      }
      else
      {
         if(nSpans < distLoad.m_SpanGirderKey.spanIndex+1)
         {
            CString strMsg;
            strMsg.Format(_T("Span %d for Distributed load is out of range. Max span number is %d. This load will be ignored."), LABEL_SPAN(distLoad.m_SpanGirderKey.spanIndex),nSpans);
            pgsDistributedLoadStatusItem* pStatusItem = new pgsDistributedLoadStatusItem(loadIdx,m_LoadStatusGroupID,m_scidDistributedLoadWarning,strMsg,distLoad.m_SpanGirderKey);
            pStatusCenter->Add(pStatusItem);
            continue; // break out of this cycle
         }
         else
         {
            spans.push_back(distLoad.m_SpanGirderKey.spanIndex);
         }
      }

      std::vector<SpanIndexType>::iterator spanIter(spans.begin());
      std::vector<SpanIndexType>::iterator spanIterEnd(spans.end());
      for (; spanIter != spanIterEnd; spanIter++)
      {
         SpanIndexType span = *spanIter;

         GirderIndexType nGirders = GetGirderCountBySpan(span);

         std::vector<GirderIndexType> girders;
         if (distLoad.m_SpanGirderKey.girderIndex == ALL_GIRDERS)
         {
            for (GirderIndexType i = 0; i < nGirders; i++)
               girders.push_back(i);
         }
         else
         {
            if(nGirders < distLoad.m_SpanGirderKey.girderIndex+1)
            {
               CString strMsg;
               strMsg.Format(_T("Girder %s for Distributed load is out of range. Max girder number is %s. This load will be ignored."), LABEL_GIRDER(distLoad.m_SpanGirderKey.girderIndex), LABEL_GIRDER(nGirders-1));
               pgsDistributedLoadStatusItem* pStatusItem = new pgsDistributedLoadStatusItem(loadIdx,m_LoadStatusGroupID,m_scidDistributedLoadWarning,strMsg,distLoad.m_SpanGirderKey);
               pStatusCenter->Add(pStatusItem);
               continue;
            }
            else
            {
               girders.push_back(distLoad.m_SpanGirderKey.girderIndex);
            }
         }

         std::vector<GirderIndexType>::iterator girderIter(girders.begin());
         std::vector<GirderIndexType>::iterator girderIterEnd(girders.end());
         for ( ; girderIter != girderIterEnd; girderIter++)
         {
            GirderIndexType girder = *girderIter;

            CSpanGirderKey thisSpanGirderKey(span,girder);

            Float64 span_length = GetSpanLength(span,girder); // span length measured along CL girder

            UserDistributedLoad upl;
            upl.m_LoadCase = Project2BridgeLoads(distLoad.m_LoadCase);
            upl.m_Description = distLoad.m_Description;

            if (distLoad.m_Type == UserLoads::Uniform)
            {
               if (IsZero(distLoad.m_WStart) && IsZero(distLoad.m_WEnd))
               {
                  CString strMsg;
                  strMsg.Format(_T("Magnitude of Distributed load is zero"));
                  pgsDistributedLoadStatusItem* pStatusItem = new pgsDistributedLoadStatusItem(loadIdx,m_LoadStatusGroupID,m_scidDistributedLoadWarning,strMsg,thisSpanGirderKey);
                  pStatusCenter->Add(pStatusItem);
               }

               // uniform load
               upl.m_WStart = distLoad.m_WStart;
               upl.m_WEnd   = distLoad.m_WStart;

               upl.m_StartLocation = 0.0;
               upl.m_EndLocation   = span_length;
            }
            else
            {
               // only a light warning for zero loads - don't bail out
               if (IsZero(distLoad.m_WStart) && IsZero(distLoad.m_WEnd))
               {
                  CString strMsg;
                  strMsg.Format(_T("Magnitude of Distributed load is zero"));
                  pgsDistributedLoadStatusItem* pStatusItem = new pgsDistributedLoadStatusItem(loadIdx,m_LoadStatusGroupID,m_scidDistributedLoadWarning,strMsg,thisSpanGirderKey);
                  pStatusCenter->Add(pStatusItem);
               }

               upl.m_WStart = distLoad.m_WStart;
               upl.m_WEnd   = distLoad.m_WEnd;

               // location
               if( distLoad.m_EndLocation <= distLoad.m_StartLocation )
               {
                  CString strMsg;
                  strMsg.Format(_T("Start locaton of distributed load is greater than end location. This load will be ignored."));
                  pgsDistributedLoadStatusItem* pStatusItem = new pgsDistributedLoadStatusItem(loadIdx,m_LoadStatusGroupID,103,strMsg,thisSpanGirderKey);
                  pStatusCenter->Add(pStatusItem);
                  continue;
               }

               if (distLoad.m_Fractional)
               {
                  if( (0.0 <= distLoad.m_StartLocation && distLoad.m_StartLocation <= 1.0) &&
                      (0.0 <= distLoad.m_EndLocation   && distLoad.m_EndLocation   <= 1.0) )
                  {
                     upl.m_StartLocation = distLoad.m_StartLocation * span_length;
                     upl.m_EndLocation   = distLoad.m_EndLocation * span_length;
                  }
                  else
                  {
                     CString strMsg;
                     strMsg.Format(_T("Fractional location value for Distributed load is out of range. Value must range from 0.0 to 1.0. This load will be ignored."));
                     pgsDistributedLoadStatusItem* pStatusItem = new pgsDistributedLoadStatusItem(loadIdx,m_LoadStatusGroupID,m_scidDistributedLoadWarning,strMsg,thisSpanGirderKey);
                     pStatusCenter->Add(pStatusItem);
                     continue;
                  }
               }
               else
               {
                  if( 0.0 <= distLoad.m_StartLocation && distLoad.m_StartLocation <= span_length &&
                      0.0 <= distLoad.m_EndLocation   && distLoad.m_EndLocation   <= span_length+TOL)
                  {
                     upl.m_StartLocation = distLoad.m_StartLocation;

                     // fudge a bit if user entered a slightly high value
                     if (distLoad.m_EndLocation < span_length)
                        upl.m_EndLocation   = distLoad.m_EndLocation;
                     else
                        upl.m_EndLocation   = span_length;

                  }
                  else
                  {
                     CString strMsg;
                     strMsg.Format(_T("Location value for Distributed load is out of range. Value must range from 0.0 to span length. This load will be ignored."));
                     pgsDistributedLoadStatusItem* pStatusItem = new pgsDistributedLoadStatusItem(loadIdx,m_LoadStatusGroupID,m_scidDistributedLoadWarning,strMsg,thisSpanGirderKey);
                     pStatusCenter->Add(pStatusItem);
                     continue;
                  }
               }
            }

            // add point of interests at start and end of this load
            CSegmentKey segmentKey;
            Float64 segLocation;
            GetSegmentLocation(span,girder,upl.m_StartLocation,&segmentKey,&segLocation);

            Float64 segLength = GetSegmentLength(segmentKey);
            segLocation = ::ForceIntoRange(0.0,segLocation,segLength);

            pgsPointOfInterest poiStart(segmentKey,segLocation);
            m_PoiMgr.AddPointOfInterest( poiStart );

            GetSegmentLocation(span,girder,upl.m_EndLocation,&segmentKey,&segLocation);

            segLength = GetSegmentLength(segmentKey);
            segLocation = ::ForceIntoRange(0.0,segLocation,segLength);

            pgsPointOfInterest poiEnd(segmentKey,segLocation);
            m_PoiMgr.AddPointOfInterest( poiEnd );

            IntervalIndexType intervalIdx = m_IntervalManager.GetInterval(distLoad.m_EventIdx);
            ATLASSERT(intervalIdx != INVALID_INDEX);
            ATLASSERT(intervalIdx < m_DistributedLoads.size());
            DistributedLoadCollection& distLoads = m_DistributedLoads[intervalIdx];

            DistributedLoadCollection::iterator itp( distLoads.find(thisSpanGirderKey) );
            if ( itp == distLoads.end() )
            {
               // not found, must insert
               std::pair<DistributedLoadCollection::iterator, bool> itbp( distLoads.insert(DistributedLoadCollection::value_type(thisSpanGirderKey, std::vector<UserDistributedLoad>() )) );
               ATLASSERT(itbp.second);
               itbp.first->second.push_back(upl);
            }
            else
            {
               itp->second.push_back(upl);
            }
         }
      }
   }
}

void CBridgeAgentImp::ValidateMomentLoads()
{
   IntervalIndexType nIntervals = GetIntervalCount();
   m_MomentLoads.resize(nIntervals);

   SpanIndexType nSpans = GetSpanCount();

   GET_IFACE(IEAFStatusCenter, pStatusCenter);
   GET_IFACE(IUserDefinedLoadData, pLoadData );
   GET_IFACE(IBridgeDescription, pIBridgeDesc);

   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();

   CollectionIndexType nLoads = pLoadData->GetMomentLoadCount();
   for(CollectionIndexType loadIdx = 0; loadIdx < nLoads; loadIdx++)
   {
      const CMomentLoadData& momentLoad = pLoadData->GetMomentLoad(loadIdx);

      // need to loop over all spans if that is what is selected - user a vector to store span numbers
      std::vector<SpanIndexType> spans;
      if (momentLoad.m_SpanGirderKey.spanIndex == ALL_SPANS)
      {
         for (SpanIndexType spanIdx = 0; spanIdx < nSpans; spanIdx++)
            spans.push_back(spanIdx);
      }
      else
      {
         if(nSpans < momentLoad.m_SpanGirderKey.spanIndex+1)
         {
            CString strMsg;
            strMsg.Format(_T("Span %d for moment load is out of range. Max span number is %d. This load will be ignored."), LABEL_SPAN(momentLoad.m_SpanGirderKey.spanIndex),nSpans);
            pgsMomentLoadStatusItem* pStatusItem = new pgsMomentLoadStatusItem(loadIdx,m_LoadStatusGroupID,m_scidMomentLoadWarning,strMsg,momentLoad.m_SpanGirderKey);
            pStatusCenter->Add(pStatusItem);
            continue; // break out of this cycle
         }
         else
         {
            spans.push_back(momentLoad.m_SpanGirderKey.spanIndex);
         }
      }

      std::vector<SpanIndexType>::iterator spanIter(spans.begin());
      std::vector<SpanIndexType>::iterator spanIterEnd(spans.end());
      for ( ; spanIter != spanIterEnd; spanIter++ )
      {
         SpanIndexType span = *spanIter;

         GirderIndexType nGirders = GetGirderCountBySpan(span);

         std::vector<GirderIndexType> girders;
         if (momentLoad.m_SpanGirderKey.girderIndex == ALL_GIRDERS)
         {
            for (GirderIndexType i = 0; i < nGirders; i++)
               girders.push_back(i);
         }
         else
         {
            if(nGirders < momentLoad.m_SpanGirderKey.girderIndex+1)
            {
               CString strMsg;
               strMsg.Format(_T("Girder %s for moment load is out of range. Max girder number is %s. This load will be ignored."), LABEL_GIRDER(momentLoad.m_SpanGirderKey.girderIndex), LABEL_GIRDER(nGirders-1));
               pgsMomentLoadStatusItem* pStatusItem = new pgsMomentLoadStatusItem(loadIdx,m_LoadStatusGroupID,m_scidMomentLoadWarning,strMsg,momentLoad.m_SpanGirderKey);
               pStatusCenter->Add(pStatusItem);
               continue;
            }
            else
            {
               girders.push_back(momentLoad.m_SpanGirderKey.girderIndex);
            }
         }

         std::vector<GirderIndexType>::iterator gdrIter(girders.begin());
         std::vector<GirderIndexType>::iterator gdrIterEnd(girders.end());
         for ( ; gdrIter != gdrIterEnd; gdrIter++ )
         {
            GirderIndexType girder = *gdrIter;

            CSpanGirderKey thisSpanGirderKey(span,girder);

            UserPointLoad upl;
            upl.m_LoadCase = Project2BridgeLoads(momentLoad.m_LoadCase);
            upl.m_Description = momentLoad.m_Description;

            // only a light warning for zero loads - don't bail out
            if (IsZero(momentLoad.m_Magnitude))
            {
               CString strMsg;
               strMsg.Format(_T("Magnitude of moment load is zero"));
               pgsMomentLoadStatusItem* pStatusItem = new pgsMomentLoadStatusItem(loadIdx,m_LoadStatusGroupID,m_scidMomentLoadWarning,strMsg,thisSpanGirderKey);
               pStatusCenter->Add(pStatusItem);
            }

            upl.m_Magnitude = momentLoad.m_Magnitude;

            Float64 span_length = GetSpanLength(span,girder); // span length measured along CL girder

            if (momentLoad.m_Fractional)
            {
               if(0.0 <= momentLoad.m_Location && momentLoad.m_Location <= 1.0)
               {
                  upl.m_Location = momentLoad.m_Location * span_length;
               }
               else
               {
                  CString strMsg;
                  strMsg.Format(_T("Fractional location value for moment load is out of range. Value must range from 0.0 to 1.0. This load will be ignored."));
                  pgsMomentLoadStatusItem* pStatusItem = new pgsMomentLoadStatusItem(loadIdx,m_LoadStatusGroupID,m_scidMomentLoadWarning,strMsg,thisSpanGirderKey);
                  pStatusCenter->Add(pStatusItem);
                  continue;
               }
            }
            else
            {
               if(0.0 <= momentLoad.m_Location && momentLoad.m_Location <= span_length)
               {
                  upl.m_Location = momentLoad.m_Location;
               }
               else
               {
                  CString strMsg;
                  strMsg.Format(_T("Location value for moment load is out of range. Value must range from 0.0 to span length. This load will be ignored."));
                  pgsMomentLoadStatusItem* pStatusItem = new pgsMomentLoadStatusItem(loadIdx,m_LoadStatusGroupID,m_scidMomentLoadWarning,strMsg,thisSpanGirderKey);
                  pStatusCenter->Add(pStatusItem);
                  continue;
               }
            }

            // add a point of interest at this load location in every interval
            CSegmentKey segmentKey;
            Float64 segLocation;
            GetSegmentLocation(thisSpanGirderKey.spanIndex,thisSpanGirderKey.girderIndex,momentLoad.m_Location,&segmentKey,&segLocation);

            Float64 segLength = GetSegmentLength(segmentKey);
            segLocation = ::ForceIntoRange(0.0,segLocation,segLength);

            pgsPointOfInterest poi(segmentKey,segLocation,POI_CONCLOAD);
            m_PoiMgr.AddPointOfInterest( poi );

            // put load into our collection
            IntervalIndexType intervalIdx = m_IntervalManager.GetInterval(momentLoad.m_EventIdx);
            ATLASSERT(intervalIdx != INVALID_INDEX);
            ATLASSERT(intervalIdx < m_MomentLoads.size());
            MomentLoadCollection& momentLoads = m_MomentLoads[intervalIdx];
            MomentLoadCollection::iterator itp( momentLoads.find(thisSpanGirderKey) );
            if ( itp == momentLoads.end() )
            {
               // not found, must insert
               std::pair<MomentLoadCollection::iterator, bool> itbp( momentLoads.insert(MomentLoadCollection::value_type(thisSpanGirderKey, std::vector<UserMomentLoad>() )) );
               ATLASSERT(itbp.second);
               itbp.first->second.push_back(upl);
            }
            else
            {
               itp->second.push_back(upl);
            }
         }
      }
   }
}

void CBridgeAgentImp::ValidateSegmentOrientation(const CSegmentKey& segmentKey)
{
   ValidatePointsOfInterest(segmentKey); // calls VALIDATE(BRIDGE);

   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   pgsTypes::GirderOrientationType orientationType = pBridgeDesc->GetGirderOrientation();

   Float64 orientation;
   if ( orientationType == pgsTypes::Plumb )
   {
      orientation = 0.0;
   }
   else
   {
      std::vector<pgsPointOfInterest> vPoi;
      pgsPointOfInterest poi;
      Float64 station, offset;
      switch ( orientationType )
      {
      case pgsTypes::StartNormal:
         vPoi = GetPointsOfInterest(segmentKey);
         poi = vPoi.front();
         break;

      case pgsTypes::MidspanNormal:
#pragma Reminder("UPDATE: I think the reference type is needed to get the mid-span poi")
         vPoi = GetPointsOfInterest(segmentKey,POI_MIDSPAN);
         poi = vPoi.front();
         break;

      case pgsTypes::EndNormal:
         vPoi = GetPointsOfInterest(segmentKey);
         poi = vPoi.back();
         break;

      case pgsTypes::Plumb:
      default:
         ATLASSERT(false); // should never get here
      };

      GetStationAndOffset(poi,&station,&offset);

      MatingSurfaceIndexType nMatingSurfaces = GetNumberOfMatingSurfaces(segmentKey);
      ATLASSERT(nMatingSurfaces > 0); // going to subtract 1 below, and this is an unsigned value

      Float64 left_mating_surface_offset  = GetMatingSurfaceLocation(poi,0);
      Float64 right_mating_surface_offset = GetMatingSurfaceLocation(poi,nMatingSurfaces-1);

      Float64 left_mating_surface_width  = GetMatingSurfaceWidth(poi,0);
      Float64 right_mating_surface_width = GetMatingSurfaceWidth(poi,nMatingSurfaces-1);

      Float64 left_edge_offset  = left_mating_surface_offset  - left_mating_surface_width/2;
      Float64 right_edge_offset = right_mating_surface_offset + right_mating_surface_width/2;

      Float64 distance = right_edge_offset - left_edge_offset;

      if ( IsZero(distance) )
      {
         orientation = GetCrownSlope(station,offset);
      }
      else
      {   
         Float64 ya_left  = GetElevation(station,offset+left_edge_offset);
         Float64 ya_right = GetElevation(station,offset+right_edge_offset);

         orientation = (ya_left - ya_right)/distance;
      }
   }

   CComPtr<ISuperstructureMember> ssmbr;
   GetSuperstructureMember(segmentKey,&ssmbr);

   CComPtr<ISegment> segment;
   ssmbr->get_Segment(segmentKey.segmentIndex,&segment);

   segment->put_Orientation(orientation);
}

bool CBridgeAgentImp::BuildCogoModel()
{
   ATLASSERT(m_CogoModel == NULL);
   m_CogoModel.Release();
   m_CogoModel.CoCreateInstance(CLSID_CogoModel);

   GET_IFACE( IRoadwayData, pIAlignment );
   if ( pIAlignment == NULL )
      return false;

   AlignmentData2 alignment_data   = pIAlignment->GetAlignmentData2();
   ProfileData2   profile_data     = pIAlignment->GetProfileData2();
   RoadwaySectionData section_data = pIAlignment->GetRoadwaySectionData();

   CComPtr<IPathCollection> alignments;
   m_CogoModel->get_Alignments(&alignments);

   CComPtr<IPath> path;
#pragma Reminder("UPDATE: need to be consistent with alignment ID") // see bridge geometry model builder
   alignments->Add(999,&path); // create the main alignment object
   CComQIPtr<IAlignment> alignment(path);
   ATLASSERT(alignment);

   CComPtr<IProfile> profile;
   alignment->get_Profile(&profile);
   profile->Clear();

   CComPtr<IPointCollection> points;
   m_CogoModel->get_Points(&points);

   // Setup the alignment
   if ( alignment_data.HorzCurves.size() == 0 )
   {
      // straight alignment
      CogoObjectID id1 = 20000;
      CogoObjectID id2 = 20001;
      points->Add(id1,0,0,NULL); // start at point 0,0

      CComQIPtr<ILocate> locate(m_CogoModel);
      locate->ByDistDir(id2,id1,100.00,CComVariant(alignment_data.Direction),0.00);

      CComPtr<IPoint2d> pnt1, pnt2;
      points->get_Item(id1,&pnt1);
      points->get_Item(id2,&pnt2);

      alignment->put_RefStation(CComVariant(0.00));
      alignment->AddEx(pnt1);
      alignment->AddEx(pnt2);
   }
   else
   {
      // there are horizontal curves
      CComQIPtr<ILocate2> locate(m_CogoEngine);
      CComQIPtr<IIntersect2> intersect(m_CogoEngine);

      // start the alignment at coordinate (0,0)... 
      CComPtr<IPoint2d> pbt; // point on back tangent
      pbt.CoCreateInstance(CLSID_Point2d);
      pbt->Move(0,0);

      CComPtr<IHorzCurveCollection> curves;
      m_CogoModel->get_HorzCurves(&curves);

      CComPtr<IPointCollection> points;
      m_CogoModel->get_Points(&points);

      Float64 back_tangent = alignment_data.Direction;

      Float64 prev_curve_ST_station; // station of the Spiral-to-Tangent point of the previous curve
                                    // station of the last point on the curve and will be
                                    // compared with the station at the start of the next curve
                                    // to confirm the next curve doesn't start before the previous one ends


      // The station at this first point is the PI station of the first curve less 2 radii
      // and 2 entry spiral lengths. This is completely arbitrary but will ensure that 
      // we are starting on the back tangent
      HorzCurveData& first_curve_data = *(alignment_data.HorzCurves.begin());
      prev_curve_ST_station = first_curve_data.PIStation - 2*(first_curve_data.Radius + first_curve_data.EntrySpiral);

      CogoObjectID curveID = 1;
      CollectionIndexType curveIdx = 0;

      std::vector<HorzCurveData>::iterator iter;
      for ( iter = alignment_data.HorzCurves.begin(); iter != alignment_data.HorzCurves.end(); iter++, curveID++, curveIdx++ )
      {
         HorzCurveData& curve_data = *iter;

         Float64 pi_station = curve_data.PIStation;

         Float64 T = 0;
         if ( IsZero(curve_data.Radius) )
         {
            // this is just a PI point (no curve)
            // create a line
            if ( iter == alignment_data.HorzCurves.begin() )
            {
               // if first curve, add a point on the back tangent
               points->AddEx(curveID++,pbt);
               alignment->AddEx(pbt);
            }

            // locate the PI
            CComPtr<IPoint2d> pi;
            locate->ByDistDir(pbt, pi_station, CComVariant( back_tangent ),0.00,&pi);

            // add the PI
            points->AddEx(curveID,pi);
            alignment->AddEx(pi);

            Float64 fwd_tangent = curve_data.FwdTangent;
            if ( !curve_data.bFwdTangent )
            {
               // FwdTangent data member is the curve delta
               // compute the forward tangent direction by adding delta to the back tangent
               fwd_tangent += back_tangent;
            }

            if ( iter == alignment_data.HorzCurves.end()-1 )
            {
               // this is the last point so add one more to model the last line segment
               CComQIPtr<ILocate2> locate(m_CogoEngine);
               CComPtr<IPoint2d> pnt;
               locate->ByDistDir(pi,100.00,CComVariant(fwd_tangent),0.00,&pnt);
               points->AddEx(++curveID,pnt); // pre-increment is important
               alignment->AddEx(pnt);
            }

            back_tangent = fwd_tangent;
            prev_curve_ST_station = pi_station;
         }
         else
         {
            // a real curve

            // locate the PI
            CComPtr<IPoint2d> pi;
            locate->ByDistDir(pbt, pi_station - prev_curve_ST_station, CComVariant( back_tangent ),0.00,&pi);

            Float64 fwd_tangent = curve_data.FwdTangent;
            if ( !curve_data.bFwdTangent )
            {
               // FwdTangent data member is the curve delta
               // compute the forward tangent direction by adding delta to the back tangent
               fwd_tangent += back_tangent;
            }

            // locate a point on the foward tangent.... at which distance? it doesn't matter... use
            // the curve radius for simplicity
            CComPtr<IPoint2d> pft; // point on forward tangent
            locate->ByDistDir(pi, curve_data.Radius, CComVariant( fwd_tangent ), 0.00, &pft );

            if ( IsZero(fabs(back_tangent - fwd_tangent)) || IsEqual(fabs(back_tangent - fwd_tangent),M_PI) )
            {
               std::_tostringstream os;
               os << _T("The central angle of horizontal curve ") << curveID << _T(" is 0 or 180 degrees. Horizontal curve was modeled as a single point at the PI location.");
               std::_tstring strMsg = os.str();
               pgsAlignmentDescriptionStatusItem* p_status_item = new pgsAlignmentDescriptionStatusItem(m_StatusGroupID,m_scidAlignmentError,0,strMsg.c_str());
               GET_IFACE(IEAFStatusCenter,pStatusCenter);
               pStatusCenter->Add(p_status_item);
               
               alignment->AddEx(pi);

               back_tangent = fwd_tangent;
               prev_curve_ST_station = pi_station;

               if ( curveIdx == 0 )
               {
                  // this is the first curve so set the reference station at the PI
                  alignment->put_RefStation( CComVariant(pi_station) );
               }

               continue; // GO TO NEXT HORIZONTAL CURVE

               //strMsg += std::_tstring(_T("\nSee Status Center for Details"));
               //THROW_UNWIND(strMsg.c_str(),-1);
            }

            CComPtr<IHorzCurve> hc;
            HRESULT hr = curves->Add(curveID,pbt,pi,pft,curve_data.Radius,curve_data.EntrySpiral,curve_data.ExitSpiral,&hc);
            ATLASSERT( SUCCEEDED(hr) );

            m_HorzCurveKeys.insert(std::make_pair(curveIdx,curveID));

            // Make sure this curve and the previous curve don't overlap
            hc->get_BkTangentLength(&T);
            if ( 0 < curveIdx )
            {
               // tangent to spiral station
               Float64 TS_station = pi_station - T;

               if ( TS_station < prev_curve_ST_station )
               {
                  // this curve starts before the previous curve ends
                  if ( IsEqual(TS_station,prev_curve_ST_station, ::ConvertToSysUnits(0.01,unitMeasure::Feet) ) )
                  {
                     // these 2 stations are within a 0.01 ft of each other... tweak this curve so it
                     // starts where the previous curve ends
                     CComPtr<IHorzCurve> prev_curve, this_curve;
                     curves->get_Item(curveID-1,&prev_curve);
                     CComPtr<IPoint2d> pntST;
                     prev_curve->get_ST(&pntST);

                     CComPtr<IPoint2d> pntTS;
                     hc->get_TS(&pntTS);

                     Float64 stX,stY,tsX,tsY;
                     pntST->Location(&stX,&stY);
                     pntTS->Location(&tsX,&tsY);

                     hc->Offset(stX-tsX,stY-tsY);
         
#if defined _DEBUG
                     pntTS.Release();
                     hc->get_TS(&pntTS);
                     pntTS->Location(&tsX,&tsY);
                     ATLASSERT(IsEqual(tsX,stX) && IsEqual(tsY,stY));
#endif
                     std::_tostringstream os;
                     os << _T("Horizontal curve ") << (curveIdx+1) << _T(" begins before curve ") << (curveIdx) << _T(" ends. Curve ") << (curveIdx+1) << _T(" has been adjusted.");
                     std::_tstring strMsg = os.str();
                     
                     pgsAlignmentDescriptionStatusItem* p_status_item = new pgsAlignmentDescriptionStatusItem(m_StatusGroupID,m_scidAlignmentWarning,0,strMsg.c_str());
                     GET_IFACE(IEAFStatusCenter,pStatusCenter);
                     pStatusCenter->Add(p_status_item);

                     strMsg += std::_tstring(_T("\nSee Status Center for Details"));
                  }
                  else
                  {
                     std::_tostringstream os;
                     os << _T("Horizontal curve ") << (curveIdx+1) << _T(" begins before curve ") << (curveIdx) << _T(" ends. Correct the curve data before proceeding");
                     std::_tstring strMsg = os.str();
                     
                     pgsAlignmentDescriptionStatusItem* p_status_item = new pgsAlignmentDescriptionStatusItem(m_StatusGroupID,m_scidAlignmentError,0,strMsg.c_str());
                     GET_IFACE(IEAFStatusCenter,pStatusCenter);
                     pStatusCenter->Add(p_status_item);

                     strMsg += std::_tstring(_T("\nSee Status Center for Details"));
                  }
               }
            }

            alignment->AddEx(hc);

            // determine the station of the ST point because this point will serve
            // as the next point on back tangent
            Float64 L;
            hc->get_TotalLength(&L);

            back_tangent = fwd_tangent;
            pbt.Release();
            hc->get_ST(&pbt);
            prev_curve_ST_station = pi_station - T + L;
         }

         if ( curveIdx == 0 )
         {
            // this is the first curve so set the reference station at the TS 
            alignment->put_RefStation( CComVariant(pi_station - T) );
         }
      }
   }

   // move the alignment into the correct position

   // get coordinate at the station the user wants to use as the reference station
   CComPtr<IPoint2d> objRefPoint1;
   alignment->LocatePoint(CComVariant(alignment_data.RefStation),omtAlongDirection, 0.00,CComVariant(0.00),&objRefPoint1);

   // create a point where the reference station should pass through
   CComPtr<IPoint2d> objRefPoint2;
   objRefPoint2.CoCreateInstance(CLSID_Point2d);
   objRefPoint2->Move(alignment_data.xRefPoint,alignment_data.yRefPoint);

   // measure the distance and direction between the point on alignment and the desired ref point
   CComQIPtr<IMeasure2> measure(m_CogoEngine);
   Float64 distance;
   CComPtr<IDirection> objDirection;
   measure->Inverse(objRefPoint1,objRefPoint2,&distance,&objDirection);

   // move the alignment such that it passes through the ref point
   alignment->Move(distance,objDirection);

#if defined _DEBUG
   CComPtr<IPoint2d> objPnt;
   alignment->LocatePoint(CComVariant(alignment_data.RefStation),omtAlongDirection, 0.0,CComVariant(0.0),&objPnt);
   Float64 x1,y1,x2,y2;
   objPnt->Location(&x1,&y1);
   objRefPoint2->Location(&x2,&y2);
   ATLASSERT(IsZero(x1-x2));
   ATLASSERT(IsZero(y1-y2));

   ATLASSERT(IsZero(x1-alignment_data.xRefPoint));
   ATLASSERT(IsZero(y1-alignment_data.yRefPoint));
#endif

   // Setup the profile
   if ( profile_data.VertCurves.size() == 0 )
   {
      CComPtr<IProfilePoint> ppStart, ppEnd;
      ppStart.CoCreateInstance(CLSID_ProfilePoint);
      ppEnd.CoCreateInstance(CLSID_ProfilePoint);

      ppStart->put_Station(CComVariant(profile_data.Station));
      ppStart->put_Elevation(profile_data.Elevation);

      Float64 L = 100;
      ppEnd->put_Station(CComVariant(profile_data.Station + L));
      ppEnd->put_Elevation( profile_data.Elevation + profile_data.Grade*L );

      profile->AddEx(ppStart);
      profile->AddEx(ppEnd);
   }
   else
   {
      // there are vertical curves
      Float64 pbg_station   = profile_data.Station;
      Float64 pbg_elevation = profile_data.Elevation;
      Float64 entry_grade   = profile_data.Grade;

      Float64 prev_EVC = pbg_station;
      
      CComPtr<IVertCurveCollection> vcurves;
      m_CogoModel->get_VertCurves(&vcurves);

      CComPtr<IProfilePointCollection> profilepoints;
      m_CogoModel->get_ProfilePoints(&profilepoints);

      CogoObjectID curveID = 1;
      CollectionIndexType curveIdx = 0;

      std::vector<VertCurveData>::iterator iter( profile_data.VertCurves.begin() );
      std::vector<VertCurveData>::iterator iterEnd( profile_data.VertCurves.end() );
      for ( ; iter != iterEnd; iter++ )
      {
         VertCurveData& curve_data = *iter;

         Float64 L1 = curve_data.L1;
         Float64 L2 = curve_data.L2;

         // if L2 is zero, interpert that as a symmetrical vertical curve with L1
         // being the full curve length.
         // Cut L1 in half and assign half to L2
         if ( IsZero(L2) )
         {
            L1 /= 2;
            L2 = L1;
         }

         CComPtr<IProfilePoint> pbg; // point on back grade
         pbg.CoCreateInstance(CLSID_ProfilePoint);

         if( curveID == 1 && IsEqual(pbg_station,curve_data.PVIStation) )
         {
            // it is a common input _T("mistake") to start with the PVI of the first curve...
            // project the begining point back
            pbg_station   = pbg_station - 2*L1;
            pbg_elevation = pbg_elevation - 2*L1*entry_grade;
            pbg->put_Station(CComVariant(pbg_station));
            pbg->put_Elevation(pbg_elevation);

            prev_EVC = pbg_station;
         }
         else
         {
            pbg->put_Station(CComVariant(pbg_station));
            pbg->put_Elevation(pbg_elevation);
         }

         // locate the PVI
         Float64 pvi_station   = curve_data.PVIStation;
         Float64 pvi_elevation = pbg_elevation + entry_grade*(pvi_station - pbg_station);

         CComPtr<IProfilePoint> pvi;
         pvi.CoCreateInstance(CLSID_ProfilePoint);
         pvi->put_Station(CComVariant(pvi_station));
         pvi->put_Elevation(pvi_elevation);

         if ( IsZero(L1) && IsZero(L2) )
         {
            // zero length vertical curve.... this is ok as it creates
            // a profile point. It isn't common so warn the user
            std::_tostringstream os;
            os << _T("Vertical curve ") << curveID << _T(" is a zero length curve.");
            std::_tstring strMsg = os.str();

            pgsAlignmentDescriptionStatusItem* p_status_item = new pgsAlignmentDescriptionStatusItem(m_StatusGroupID,m_scidAlignmentWarning,1,strMsg.c_str());
            GET_IFACE(IEAFStatusCenter,pStatusCenter);
            pStatusCenter->Add(p_status_item);

            // add a profile point
            if ( iter == profile_data.VertCurves.begin() )
            {
               // this is the first item so we need a point before this to model the entry grade
               CComPtr<IProfilePoint> pbg;
               pbg.CoCreateInstance(CLSID_ProfilePoint);
               pbg->put_Station(CComVariant(pvi_station - 100.));
               pbg->put_Elevation(pvi_elevation - 100.0*entry_grade);
               profilepoints->AddEx(curveID++,pbg);
               profile->AddEx(pbg);
            }

            profilepoints->AddEx(curveID,pvi);
            profile->AddEx(pvi);

            if ( iter == profile_data.VertCurves.end()-1 )
            {
               // this is the last point ... need to add a Profile point on the exit grade
               CComPtr<IProfilePoint> pfg;
               pfg.CoCreateInstance(CLSID_ProfilePoint);
               pfg->put_Station(CComVariant(pvi_station + 100.));
               pfg->put_Elevation(pvi_elevation + 100.0*curve_data.ExitGrade);
               profilepoints->AddEx(curveID++,pfg);
               profile->AddEx(pfg);
            }

            pbg_station   = pvi_station;
            pbg_elevation = pvi_elevation;
            entry_grade   = curve_data.ExitGrade;

            prev_EVC = pvi_station;
         }
         else
         {
            // add a vertical curve
            Float64 pfg_station = pvi_station + L2;
            Float64 pfg_elevation = pvi_elevation + curve_data.ExitGrade*L2;

            CComPtr<IProfilePoint> pfg; // point on forward grade
            pfg.CoCreateInstance(CLSID_ProfilePoint);
            pfg->put_Station(CComVariant(pfg_station));
            pfg->put_Elevation(pfg_elevation);

            Float64 BVC = pvi_station - L1;
            Float64 EVC = pvi_station + L2;
            Float64 tolerance = ::ConvertToSysUnits(0.006,unitMeasure::Feet); // sometimes users enter the BVC as the start point
                                                                              // and the numbers work out such that it differs by 0.01ft
                                                                              // select a tolerance so that this isn't a problem
            if( IsLT(BVC,prev_EVC,tolerance) || IsLT(pvi_station,prev_EVC,tolerance) || IsLT(EVC,prev_EVC,tolerance) )
            {
               // some part of this curve is before the end of the previous curve
               std::_tostringstream os;

               if ( curveID == 1 )
                  os << _T("Vertical Curve ") << curveID << _T(" begins before the profile reference point.");
               else
                  os << _T("Vertical curve ") << curveID << _T(" begins before curve ") << (curveID-1) << _T(" ends.");

               std::_tstring strMsg = os.str();

               pgsAlignmentDescriptionStatusItem* p_status_item = new pgsAlignmentDescriptionStatusItem(m_StatusGroupID,m_scidAlignmentError,1,strMsg.c_str());
               GET_IFACE(IEAFStatusCenter,pStatusCenter);
               pStatusCenter->Add(p_status_item);

               strMsg += std::_tstring(_T("\nSee Status Center for Details"));
   //            THROW_UNWIND(strMsg.c_str(),-1);
            }

            CComPtr<IVertCurve> vc;
            HRESULT hr = vcurves->Add(curveID,pbg,pvi,pfg,L1,L2,&vc);
            ATLASSERT(SUCCEEDED(hr));

            m_VertCurveKeys.insert(std::make_pair(curveIdx,curveID));
           

            profile->AddEx(vc);

            Float64 g1,g2;
            vc->get_EntryGrade(&g1);
            vc->get_ExitGrade(&g2);

            if ( IsEqual(g1,g2) )
            {
               // entry and exit grades are the same
               std::_tostringstream os;
               os << _T("Entry and exit grades are the same on curve ") << curveID;
               std::_tstring strMsg = os.str();

               pgsAlignmentDescriptionStatusItem* p_status_item = new pgsAlignmentDescriptionStatusItem(m_StatusGroupID,m_scidAlignmentWarning,1,strMsg.c_str());
               GET_IFACE(IEAFStatusCenter,pStatusCenter);
               pStatusCenter->Add(p_status_item);
            }

            pbg_station   = pvi_station;
            pbg_elevation = pvi_elevation;
            entry_grade   = curve_data.ExitGrade;

            prev_EVC = EVC;
         }

         curveID++;
         curveIdx++;
      }
   }

   CComPtr<ICrossSectionCollection> sections;
   profile->get_CrossSections(&sections);

   std::vector<CrownData2>::iterator iter(section_data.Superelevations.begin());
   std::vector<CrownData2>::iterator iterEnd(section_data.Superelevations.end());
   for ( ; iter != iterEnd; iter++ )
   {
      CrownData2& super = *iter;
      CComPtr<ICrossSection> section;
      sections->Add(CComVariant(super.Station),super.CrownPointOffset,super.Left,super.Right,&section);
   }

#pragma Reminder("UPDATE: What to do with the Alignment Offset?")
   //// set the alignment offset
   //GET_IFACE(IBridgeDescription,pIBridgeDesc);
   //const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   //Float64 alignment_offset = pBridgeDesc->GetAlignmentOffset();
   //m_Bridge->put_AlignmentOffset(alignment_offset);

   return true;
}

bool CBridgeAgentImp::BuildBridgeGeometryModel()
{
   WATCH(_T("Building Bridge Geometry Model"));

   CComPtr<IBridgeGeometry> bridgeGeometry;
   m_Bridge->get_BridgeGeometry(&bridgeGeometry);

   // Get the alignment that will be used for the bridge
   CComPtr<IPathCollection> alignments;
   m_CogoModel->get_Alignments(&alignments);

   CComPtr<IPath> path;
   alignments->get_Item(999,&path);

   CComQIPtr<IAlignment> alignment(path);

   // associated alignment with the bridge geometry
   bridgeGeometry->putref_Alignment(999,alignment);
   bridgeGeometry->put_BridgeAlignmentID(999);

   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();

   CBridgeGeometryModelBuilder model_builder;
   model_builder.BuildBridgeGeometryModel(pBridgeDesc,m_CogoModel,alignment,bridgeGeometry);
   return true;
}

bool CBridgeAgentImp::BuildBridgeModel()
{
   // Need to create bridge so we can get to the cogo model
   ATLASSERT(m_Bridge == NULL);
   m_Bridge.Release(); // release just in case, but it should already be done
   HRESULT hr = m_Bridge.CoCreateInstance(CLSID_GenericBridge);
   ATLASSERT(SUCCEEDED(hr));

   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();

   // Create effective flange width tool (it is needed to validate some of the parameters)
   CComObject<CEffectiveFlangeWidthTool>* pTool;
   hr = CComObject<CEffectiveFlangeWidthTool>::CreateInstance(&pTool);
   pTool->Init(m_pBroker,m_StatusGroupID);
   m_EffFlangeWidthTool = pTool;
   if ( FAILED(hr) || m_EffFlangeWidthTool == NULL )
      THROW_UNWIND(_T("Custom Effective Flange Width Tool not created"),-1);

   m_SectCutTool->putref_EffectiveFlangeWidthTool(m_EffFlangeWidthTool);

   // Set up the interval manager
   GET_IFACE(ILossParameters,pLossParameters);
   pgsTypes::LossMethod loss_method = pLossParameters->GetLossMethod();
   bool bTimeStepMethod = (loss_method == pgsTypes::TIME_STEP ? true : false);
   m_IntervalManager.BuildIntervals(pBridgeDesc->GetTimelineManager(),bTimeStepMethod);

   BuildBridgeGeometryModel(); // creates the framing geometry for the bridge model

   // Now that the basic input has been generated, updated the bridge model
   // This will get all the geometry correct and build the pier objects.
   // This basic geometry is needed to lay out girders, deck, etc
   m_Bridge->UpdateBridgeModel();

   WATCH(_T("Validating Bridge Model"));

#pragma Reminder("UPDATE: only call one of these methods... one has to go")
   if ( !LayoutGirders(pBridgeDesc) )
      return false;
   //if ( !LayoutPrecastGirders(pBridgeDesc) )
   //   return false;

   if ( !LayoutDeck(pBridgeDesc) )
      return false;

   if ( !LayoutTrafficBarriers(pBridgeDesc) )
      return false;

#pragma Reminder("UPDATE: this is wasteful")
   // Why do we have to re-generate the piers again? maybe
   // there is a way to layout the new stuff only or
   // maybe the first UpdateBridgeGeometry isn't needed

   // Layout the bridge again to complete the geometry
   m_Bridge->UpdateBridgeModel();

#pragma Reminder("BUG: Checks suspended for spliced girder development")
   //// check bridge for errors - will throw an exception if there are errors
   //CheckBridge();

   return true;
}

#pragma Reminder("OBSOLETE: remove obsolete code")
// When PGSplice updates are complete, remove this block of code
// This is the original method for laying out the girders and it has been
// replaced with LayoutGirders
/*
bool CBridgeAgentImp::LayoutPrecastGirders(const CBridgeDescription2* pBridgeDesc)
{
   CComPtr<IBridgeGeometry> geometry;
   m_Bridge->get_BridgeGeometry(&geometry);

   CComPtr<IPierCollection> piers;
   m_Bridge->get_Piers(&piers);

   SpanIndexType nSpans = pBridgeDesc->GetSpanCount();
   for ( SpanIndexType spanIdx = 0; spanIdx < nSpans; spanIdx++ )
   {
      const CSpanData* pSpan = pBridgeDesc->GetSpan(spanIdx);
      const CGirderTypes* pGirderTypes = pSpan->GetGirderTypes();

      GirderIndexType nGirders = pSpan->GetGirderCount();
      for ( GirderIndexType gdrIdx = 0; gdrIdx < nGirders; gdrIdx++ )
      {
         CSegmentKey segmentKey(spanIdx,gdrIdx,0);

         // get the girder library entry
         const GirderLibraryEntry* pGirderEntry = GetGirderLibraryEntry(segmentKey);

         GirderLibraryEntry::Dimensions dimensions = pGirderEntry->GetDimensions();

         CComPtr<IBeamFactory> beamFactory;
         pGirderEntry->GetBeamFactory(&beamFactory);

         // create a superstructure member with one segment for each girder
         LocationType locationType = (gdrIdx == 0 ? ltLeftExteriorGirder : (gdrIdx == nGirders-1 ? ltRightExteriorGirder : ltInteriorGirder));
         

         CComPtr<ISuperstructureMember> ssmbr;
         m_Bridge->CreateSuperstructureMember(::GetSuperstructureMemberID(segmentKey.groupIndex,segmentKey.girderIndex),locationType,&ssmbr);

         // Let beam factory build segments as necessary to represent the beam
         // this includes setting the cross section
         beamFactory->LayoutGirderLine(m_pBroker,m_StatusGroupID,segmentKey,ssmbr);

         // there should only be one segment for prestressed girder bridges
         CComPtr<ISegment> segment;
         ssmbr->get_Segment(0,&segment);

         // assign the corresponding girder line geometry to segment
         LineIDType girderLineID = ::GetGirderLineID(spanIdx,gdrIdx);
         CComPtr<IGirderLine> girderLine;
         geometry->FindGirderLine(girderLineID,&girderLine);

         segment->putref_GirderLine(girderLine);

         // get harped strand adjustment limits
         pgsTypes::GirderFace endTopFace, endBottomFace;
         Float64 endTopLimit, endBottomLimit;
         pGirderEntry->GetEndAdjustmentLimits(&endTopFace, &endTopLimit, &endBottomFace, &endBottomLimit);

         IBeamFactory::BeamFace etf = endTopFace    == pgsTypes::GirderBottom ? IBeamFactory::BeamBottom : IBeamFactory::BeamTop;
         IBeamFactory::BeamFace ebf = endBottomFace == pgsTypes::GirderBottom ? IBeamFactory::BeamBottom : IBeamFactory::BeamTop;

         pgsTypes::GirderFace hpTopFace, hpBottomFace;
         Float64 hpTopLimit, hpBottomLimit;
         pGirderEntry->GetHPAdjustmentLimits(&hpTopFace, &hpTopLimit, &hpBottomFace, &hpBottomLimit);
 
         IBeamFactory::BeamFace htf = hpTopFace    == pgsTypes::GirderBottom ? IBeamFactory::BeamBottom : IBeamFactory::BeamTop;
         IBeamFactory::BeamFace hbf = hpBottomFace == pgsTypes::GirderBottom ? IBeamFactory::BeamBottom : IBeamFactory::BeamTop;

         // only allow end adjustents if increment is non-negative
         Float64 end_increment = pGirderEntry->IsVerticalAdjustmentAllowedEnd() ?  pGirderEntry->GetEndStrandIncrement() : -1.0;
         Float64 hp_increment  = pGirderEntry->IsVerticalAdjustmentAllowedHP()  ?  pGirderEntry->GetHPStrandIncrement()  : -1.0;

         // create the strand mover
         CComPtr<IStrandMover> strand_mover;
         beamFactory->CreateStrandMover(dimensions, 
                                        etf, endTopLimit, ebf, endBottomLimit, 
                                        htf, hpTopLimit,  hbf, hpBottomLimit, 
                                        end_increment, hp_increment, &strand_mover);

         ATLASSERT(strand_mover != NULL);
         
         // assign a precast girder model to the superstructure member
         CComPtr<IPrecastGirder> girder;
         HRESULT hr = girder.CoCreateInstance(CLSID_PrecastGirder);
         if ( FAILED(hr) || girder == NULL )
            THROW_UNWIND(_T("Precast girder object not created"),-1);

         girder->Initialize(segment,strand_mover);
         CComQIPtr<IItemData> item_data(segment);
         item_data->AddItemData(CComBSTR(_T("Precast Girder")),girder);

         // Fill up strand patterns
         CComPtr<IStrandGrid> strGrd[2], harpGrdEnd[2], harpGrdHP[2], tempGrd[2];
         girder->get_StraightStrandGrid(etStart,&strGrd[etStart]);
         girder->get_HarpedStrandGridEnd(etStart,&harpGrdEnd[etStart]);
         girder->get_HarpedStrandGridHP(etStart,&harpGrdHP[etStart]);
         girder->get_TemporaryStrandGrid(etStart,&tempGrd[etStart]);

         girder->get_StraightStrandGrid(etEnd,&strGrd[etEnd]);
         girder->get_HarpedStrandGridEnd(etEnd,&harpGrdEnd[etEnd]);
         girder->get_HarpedStrandGridHP(etEnd,&harpGrdHP[etEnd]);
         girder->get_TemporaryStrandGrid(etEnd,&tempGrd[etEnd]);

         pGirderEntry->ConfigureStraightStrandGrid(strGrd[etStart],strGrd[etEnd]);
         pGirderEntry->ConfigureHarpedStrandGrids(harpGrdEnd[etStart], harpGrdHP[etStart], harpGrdHP[etEnd], harpGrdEnd[etEnd]);
         pGirderEntry->ConfigureTemporaryStrandGrid(tempGrd[etStart],tempGrd[etEnd]);

         girder->put_AllowOddNumberOfHarpedStrands(pGirderEntry->OddNumberOfHarpedStrands() ? VARIANT_TRUE : VARIANT_FALSE);

#pragma Reminder("UPDATE: This is wrong for harping point measured as distance")

         // lay out the harping points
         GirderLibraryEntry::MeasurementLocation hpref = pGirderEntry->GetHarpingPointReference();
         GirderLibraryEntry::MeasurementType hpmeasure = pGirderEntry->GetHarpingPointMeasure();
         Float64 hpLoc = pGirderEntry->GetHarpingPointLocation();

         girder->put_UseMinHarpPointDistance( pGirderEntry->IsMinHarpingPointLocationUsed() ? VARIANT_TRUE : VARIANT_FALSE);
         girder->put_MinHarpPointDistance( pGirderEntry->GetMinHarpingPointLocation() );

         girder->SetHarpingPoints(hpLoc,hpLoc);
         girder->put_HarpingPointReference( HarpPointReference(hpref) );
         girder->put_HarpingPointMeasure( HarpPointMeasure(hpmeasure) );
      }
   }

#pragma Reminder("Remove obsolete code")
//   HRESULT hr = S_OK;
//
//   GET_IFACE(ILibrary,pLib);
//   
//   CComPtr<ISpanCollection> spans;
//   m_Bridge->get_Spans(&spans);
//
//   SpanIndexType nSpans;
//   spans->get_Count(&nSpans);
//
//   ATLASSERT(nSpans == pBridgeDesc->GetSpanCount() );
//
//   pgsTypes::SupportedDeckType deckType = pBridgeDesc->GetDeckDescription()->DeckType;
//   pgsTypes::SupportedBeamSpacing girderSpacingType = pBridgeDesc->GetGirderSpacingType();
//
//   Float64 gross_slab_depth = pBridgeDesc->GetDeckDescription()->GrossDepth;
//
//   // if this deck has precast panels, GrossDepth is just the cast depth... add the panel depth
//   if ( pBridgeDesc->GetDeckDescription()->DeckType == pgsTypes::sdtCompositeSIP )
//      gross_slab_depth += pBridgeDesc->GetDeckDescription()->PanelDepth;
//
//   for ( SpanIndexType spanIdx = 0; spanIdx < nSpans; spanIdx++ )
//   {
//      const CSpanData* pSpan = pBridgeDesc->GetSpan(spanIdx);
//      const CPierData2* pPrevPier = pSpan->GetPrevPier();
//      const CPierData2* pNextPier = pSpan->GetNextPier();
//
//      CComPtr<ISpan> span;
//      spans->get_Item(spanIdx,&span);
//
//      CComPtr<IPier> prevPier, nextPier;
//      span->get_PrevPier(&prevPier);
//      span->get_NextPier(&nextPier);
//
//      CComPtr<IConnection> start_connection, end_connection;
//      prevPier->get_Connection(qcbAfter, &start_connection);
//      nextPier->get_Connection(qcbBefore,&end_connection);
//
//      ConfigureConnection(pPrevPier,pgsTypes::Ahead,start_connection);
//      ConfigureConnection(pNextPier,pgsTypes::Back, end_connection);
//
//      CComPtr<IGirderSpacing> startSpacing, endSpacing;
//      span->get_GirderSpacing(etStart, &startSpacing);
//      span->get_GirderSpacing(etEnd,   &endSpacing);
//
//      const CGirderSpacing* pStartSpacing = pSpan->GetGirderSpacing(pgsTypes::metStart);
//      const CGirderSpacing* pEndSpacing   = pSpan->GetGirderSpacing(pgsTypes::metEnd);
//
//      pgsTypes::MeasurementLocation mlStart = pStartSpacing->GetMeasurementLocation();
//      pgsTypes::MeasurementType     mtStart = pStartSpacing->GetMeasurementType();
//
//      GirderIndexType startRefGirderIdx = pStartSpacing->GetRefGirder();
//      Float64 startRefGirderOffset = pStartSpacing->GetRefGirderOffset();
//      pgsTypes::OffsetMeasurementType startRefGirderOffsetType = pStartSpacing->GetRefGirderOffsetType();
//
//      pgsTypes::MeasurementLocation mlEnd = pEndSpacing->GetMeasurementLocation();
//      pgsTypes::MeasurementType     mtEnd = pEndSpacing->GetMeasurementType();
//
//      GirderIndexType endRefGirderIdx = pEndSpacing->GetRefGirder();
//      Float64 endRefGirderOffset = pEndSpacing->GetRefGirderOffset();
//      pgsTypes::OffsetMeasurementType endRefGirderOffsetType = pEndSpacing->GetRefGirderOffsetType();
//
//      startSpacing->SetMeasurement((MeasurementLocation)mlStart,(MeasurementType)mtStart);
//      startSpacing->SetRefGirder(startRefGirderIdx,startRefGirderOffset,(OffsetType)startRefGirderOffsetType);
//
//      endSpacing->SetMeasurement((MeasurementLocation)mlEnd,(MeasurementType)mtEnd);
//      endSpacing->SetRefGirder(endRefGirderIdx,endRefGirderOffset,(OffsetType)endRefGirderOffsetType);
//
//      GirderIndexType nGirders = pSpan->GetGirderCount();
//
//      span->put_GirderCount(nGirders);
//
//      const CGirderTypes* pGirderTypes = pSpan->GetGirderTypes();
//
//      // create a girder for each girder line... set the girder spacing
//
//      // store the girder spacing in an array and set it all at once
//      CComPtr<IDblArray> startSpaces, endSpaces;
//      startSpaces.CoCreateInstance(CLSID_DblArray);
//      endSpaces.CoCreateInstance(CLSID_DblArray);
//      GirderIndexType gdrIdx = 0;
//      for ( gdrIdx = 0; gdrIdx < nGirders; gdrIdx++ )
//      {
//         // get the girder library entry
//         std::_tstring strGirderName = pGirderTypes->GetGirderName(gdrIdx);
//
//         const GirderLibraryEntry* pGirderEntry = pGirderTypes->GetGirderLibraryEntry(gdrIdx);
//
//         ATLASSERT( strGirderName == pGirderEntry->GetName() );
//
//         GirderLibraryEntry::Dimensions dimensions = pGirderEntry->GetDimensions();
//
//         CComPtr<IBeamFactory> beamFactory;
//         pGirderEntry->GetBeamFactory(&beamFactory);
//
//         // get the girder spacing
//         SpacingIndexType spaceIdx = gdrIdx;
//         if ( gdrIdx < nGirders-1 )
//         {
//            // set the spacing after the girder... skip if this is the last girder
//            Float64 gdrStartSpacing = pStartSpacing->GetGirderSpacing(spaceIdx);
//            Float64 gdrEndSpacing   = pEndSpacing->GetGirderSpacing(spaceIdx);
//
//            if ( IsJointSpacing(girderSpacingType) )
//            {
//               // spacing is actually a joint width... convert to a CL-CL spacing
//               Float64 leftWidth, rightWidth;
//
//               Float64 minSpacing, maxSpacing;
//               beamFactory->GetAllowableSpacingRange(dimensions,deckType,girderSpacingType,&minSpacing,&maxSpacing);
//               leftWidth = minSpacing;
//
//               const GirderLibraryEntry* pGirderEntry2 = pSpan->GetGirderTypes()->GetGirderLibraryEntry(gdrIdx+1);
//               GirderLibraryEntry::Dimensions dimensions2 = pGirderEntry2->GetDimensions();
//
//               CComPtr<IBeamFactory> beamFactory2;
//               pGirderEntry2->GetBeamFactory(&beamFactory2);
//
//               beamFactory2->GetAllowableSpacingRange(dimensions2,deckType,girderSpacingType,&minSpacing,&maxSpacing);
//               rightWidth = minSpacing;
//
//               gdrStartSpacing += (leftWidth+rightWidth)/2;
//               gdrEndSpacing   += (leftWidth+rightWidth)/2;
//            }
//
//            startSpaces->Add(gdrStartSpacing);
//            endSpaces->Add(gdrEndSpacing);
//         }
//                                                                 
//         // get the superstructure member
//         CComPtr<ISuperstructureMember> ssmbr;
//         span->get_SuperstructureMember(gdrIdx,&ssmbr);
//
//         // assign connection data to superstructure member
//         CComQIPtr<IItemData> item_data(ssmbr);
//         item_data->AddItemData(CComBSTR(STARTCONNECTION),start_connection);
//         item_data->AddItemData(CComBSTR(ENDCONNECTION),  end_connection);
//
//         // get harped strand adjustment limits
//         GirderLibraryEntry::GirderFace endTopFace, endBottomFace;
//         Float64 endTopLimit, endBottomLimit;
//         pGirderEntry->GetEndAdjustmentLimits(&endTopFace, &endTopLimit, &endBottomFace, &endBottomLimit);
//
//         IBeamFactory::BeamFace etf = endTopFace    == GirderLibraryEntry::GirderBottom ? IBeamFactory::BeamBottom : IBeamFactory::BeamTop;
//         IBeamFactory::BeamFace ebf = endBottomFace == GirderLibraryEntry::GirderBottom ? IBeamFactory::BeamBottom : IBeamFactory::BeamTop;
//
//         GirderLibraryEntry::GirderFace hpTopFace, hpBottomFace;
//         Float64 hpTopLimit, hpBottomLimit;
//         pGirderEntry->GetHPAdjustmentLimits(&hpTopFace, &hpTopLimit, &hpBottomFace, &hpBottomLimit);
// 
//         IBeamFactory::BeamFace htf = hpTopFace    == GirderLibraryEntry::GirderBottom ? IBeamFactory::BeamBottom : IBeamFactory::BeamTop;
//         IBeamFactory::BeamFace hbf = hpBottomFace == GirderLibraryEntry::GirderBottom ? IBeamFactory::BeamBottom : IBeamFactory::BeamTop;
//
//         // only allow end adjustents if increment is non-negative
//         Float64 end_increment = pGirderEntry->IsVerticalAdjustmentAllowedEnd() ?  pGirderEntry->GetEndStrandIncrement() : -1.0;
//         Float64 hp_increment  = pGirderEntry->IsVerticalAdjustmentAllowedHP()  ?  pGirderEntry->GetHPStrandIncrement()  : -1.0;
//
//         // create the strand mover
//         CComPtr<IStrandMover> strand_mover;
//         beamFactory->CreateStrandMover(dimensions, 
//                                        etf, endTopLimit, ebf, endBottomLimit, 
//                                        htf, hpTopLimit,  hbf, hpBottomLimit, 
//                                        end_increment, hp_increment, &strand_mover);
//
//         ATLASSERT(strand_mover != NULL);
//         
//         // assign a precast girder model to the superstructure member
//         CComPtr<IPrecastGirder> girder;
//         HRESULT hr = girder.CoCreateInstance(CLSID_PrecastGirder);
//         if ( FAILED(hr) || girder == NULL )
//            THROW_UNWIND(_T("Precast girder object not created"),-1);
//
//         girder->Initialize(m_Bridge,strand_mover, spanIdx,gdrIdx);
//         item_data->AddItemData(CComBSTR(_T("Precast Girder")),girder);
//
//         // Fill up strand patterns
//         CComPtr<IStrandGrid> strGrd[2], harpGrdEnd[2], harpGrdHP[2], tempGrd[2];
//         girder->get_StraightStrandGrid(etStart,&strGrd[etStart]);
//         girder->get_HarpedStrandGridEnd(etStart,&harpGrdEnd[etStart]);
//         girder->get_HarpedStrandGridHP(etStart,&harpGrdHP[etStart]);
//         girder->get_TemporaryStrandGrid(etStart,&tempGrd[etStart]);
//
//         girder->get_StraightStrandGrid(etEnd,&strGrd[etEnd]);
//         girder->get_HarpedStrandGridEnd(etEnd,&harpGrdEnd[etEnd]);
//         girder->get_HarpedStrandGridHP(etEnd,&harpGrdHP[etEnd]);
//         girder->get_TemporaryStrandGrid(etEnd,&tempGrd[etEnd]);
//
//         pGirderEntry->ConfigureStraightStrandGrid(strGrd[etStart],strGrd[etEnd]);
//         pGirderEntry->ConfigureHarpedStrandGrids(harpGrdEnd[etStart], harpGrdHP[etStart], harpGrdHP[etEnd], harpGrdEnd[etEnd]);
//         pGirderEntry->ConfigureTemporaryStrandGrid(tempGrd[etStart],tempGrd[etEnd]);
//
//         girder->put_AllowOddNumberOfHarpedStrands(pGirderEntry->OddNumberOfHarpedStrands() ? VARIANT_TRUE : VARIANT_FALSE);
//
//         // Let beam factory build segments as necessary to represent the beam
//         beamFactory->LayoutGirderLine(m_pBroker,m_StatusGroupID,spanIdx,gdrIdx,ssmbr);
//
//#pragma Reminder("UPDATE: This is wrong for harping point measured as distance")
//
//         // lay out the harping points
//         GirderLibraryEntry::MeasurementLocation hpref = pGirderEntry->GetHarpingPointReference();
//         GirderLibraryEntry::MeasurementType hpmeasure = pGirderEntry->GetHarpingPointMeasure();
//         Float64 hpLoc = pGirderEntry->GetHarpingPointLocation();
//
//         girder->put_UseMinHarpPointDistance( pGirderEntry->IsMinHarpingPointLocationUsed() ? VARIANT_TRUE : VARIANT_FALSE);
//         girder->put_MinHarpPointDistance( pGirderEntry->GetMinHarpingPointLocation() );
//
//         girder->SetHarpingPoints(hpLoc,hpLoc);
//         girder->put_HarpingPointReference( HarpPointReference(hpref) );
//         girder->put_HarpingPointMeasure( HarpPointMeasure(hpmeasure) );
//      
//
//         Float64 startHaunch = 0;
//         Float64 endHaunch   = 0;
//         if ( deckType != pgsTypes::sdtNone )
//         {
//            startHaunch = pGirderTypes->GetSlabOffset(gdrIdx,pgsTypes::metStart) - gross_slab_depth;
//            endHaunch   = pGirderTypes->GetSlabOffset(gdrIdx,pgsTypes::metEnd)   - gross_slab_depth;
//         }
//
//         // if these fire, something is really messed up
//         ATLASSERT( 0 <= startHaunch && startHaunch < 1e9 );
//         ATLASSERT( 0 <= endHaunch   && endHaunch   < 1e9 );
//
//         startSpacing->put_GirderHaunch(gdrIdx,startHaunch);
//         endSpacing->put_GirderHaunch(gdrIdx,endHaunch);
//      }
//
//      // set the spacing for all girders in this span now
//      startSpacing->put_Spacings(startSpaces);
//      endSpacing->put_Spacings(endSpaces);
//   }
//
   return true;
}
*/
bool CBridgeAgentImp::LayoutGirders(const CBridgeDescription2* pBridgeDesc)
{
   CComPtr<IBridgeGeometry> geometry;
   m_Bridge->get_BridgeGeometry(&geometry);

   Float64 gross_slab_depth = pBridgeDesc->GetDeckDescription()->GrossDepth;
   pgsTypes::SupportedDeckType deckType = pBridgeDesc->GetDeckDescription()->DeckType;

   IntervalIndexType stressStrandIntervalIdx = m_IntervalManager.GetStressStrandInterval();

   GroupIndexType nGroups = pBridgeDesc->GetGirderGroupCount();
   for ( GroupIndexType grpIdx = 0; grpIdx < nGroups; grpIdx++ )
   {
      const CGirderGroupData* pGroup = pBridgeDesc->GetGirderGroup(grpIdx);

      GirderIndexType nGirders = pGroup->GetGirderCount();
      for ( GirderIndexType gdrIdx = 0; gdrIdx < nGirders; gdrIdx++ )
      {
         CGirderKey girderKey(grpIdx,gdrIdx);

         const CSplicedGirderData* pGirder = pGroup->GetGirder(gdrIdx);

         // get the girder library entry
         const GirderLibraryEntry* pGirderEntry = GetGirderLibraryEntry(girderKey);

         GirderLibraryEntry::Dimensions dimensions = pGirderEntry->GetDimensions();

         CComPtr<IBeamFactory> beamFactory;
         pGirderEntry->GetBeamFactory(&beamFactory);

         // create a superstructure member
         LocationType locationType = (gdrIdx == 0 ? ltLeftExteriorGirder : (gdrIdx == nGirders-1 ? ltRightExteriorGirder : ltInteriorGirder));

         CComPtr<ISuperstructureMember> ssmbr;
         IDType ssmbrID = ::GetSuperstructureMemberID(girderKey.groupIndex,girderKey.girderIndex);
         m_Bridge->CreateSuperstructureMember(ssmbrID,locationType,&ssmbr);

         SegmentIndexType nSegments = pGirder->GetSegmentCount();
         for ( SegmentIndexType segIdx = 0; segIdx < nSegments; segIdx++ )
         {
            CSegmentKey segmentKey(girderKey.groupIndex,girderKey.girderIndex,segIdx);

            // Let beam factory build segments as necessary to represent the beam
            // this includes setting the cross section
            beamFactory->LayoutGirderLine(m_pBroker,m_StatusGroupID,segmentKey,ssmbr);

            // associate girder line with segment so segment has plan view geometry
            LineIDType girderLineID = ::GetGirderSegmentLineID(grpIdx,gdrIdx,segIdx);
            CComPtr<IGirderLine> girderLine;
            geometry->FindGirderLine(girderLineID,&girderLine);

            CComPtr<ISegment> segment;
            ssmbr->get_Segment(segIdx,&segment);
            segment->putref_GirderLine(girderLine);

            const CPrecastSegmentData* pSegment = pGirder->GetSegment(segIdx);
            Float64 startHaunch = 0;
            Float64 endHaunch   = 0;
            if ( deckType != pgsTypes::sdtNone )
            {
               startHaunch = pSegment->GetSlabOffset(pgsTypes::metStart) - gross_slab_depth;
               endHaunch   = pSegment->GetSlabOffset(pgsTypes::metEnd)   - gross_slab_depth;
            }

            segment->put_HaunchDepth(etStart,startHaunch);
            segment->put_HaunchDepth(etEnd,  endHaunch);

            // if these fire, something is really messed up
            ATLASSERT( 0 <= startHaunch && startHaunch < 1e9 );
            ATLASSERT( 0 <= endHaunch   && endHaunch   < 1e9 );

            // get harped strand adjustment limits
            pgsTypes::GirderFace endTopFace, endBottomFace;
            Float64 endTopLimit, endBottomLimit;
            pGirderEntry->GetEndAdjustmentLimits(&endTopFace, &endTopLimit, &endBottomFace, &endBottomLimit);

            IBeamFactory::BeamFace etf = endTopFace    == pgsTypes::GirderBottom ? IBeamFactory::BeamBottom : IBeamFactory::BeamTop;
            IBeamFactory::BeamFace ebf = endBottomFace == pgsTypes::GirderBottom ? IBeamFactory::BeamBottom : IBeamFactory::BeamTop;

            pgsTypes::GirderFace hpTopFace, hpBottomFace;
            Float64 hpTopLimit, hpBottomLimit;
            pGirderEntry->GetHPAdjustmentLimits(&hpTopFace, &hpTopLimit, &hpBottomFace, &hpBottomLimit);
    
            IBeamFactory::BeamFace htf = hpTopFace    == pgsTypes::GirderBottom ? IBeamFactory::BeamBottom : IBeamFactory::BeamTop;
            IBeamFactory::BeamFace hbf = hpBottomFace == pgsTypes::GirderBottom ? IBeamFactory::BeamBottom : IBeamFactory::BeamTop;

            // only allow end adjustents if increment is non-negative
            Float64 end_increment = pGirderEntry->IsVerticalAdjustmentAllowedEnd() ?  pGirderEntry->GetEndStrandIncrement() : -1.0;
            Float64 hp_increment  = pGirderEntry->IsVerticalAdjustmentAllowedHP()  ?  pGirderEntry->GetHPStrandIncrement()  : -1.0;

            // create the strand mover
            CComPtr<IStrandMover> strand_mover;
            beamFactory->CreateStrandMover(dimensions, 
                                           etf, endTopLimit, ebf, endBottomLimit, 
                                           htf, hpTopLimit,  hbf, hpBottomLimit, 
                                           end_increment, hp_increment, &strand_mover);

            ATLASSERT(strand_mover != NULL);
            
            // assign a precast girder model to the segment
            CComPtr<IPrecastGirder> girder;
            HRESULT hr = girder.CoCreateInstance(CLSID_PrecastGirder);
            if ( FAILED(hr) || girder == NULL )
               THROW_UNWIND(_T("Precast girder object not created"),-1);

            girder->Initialize(segment,strand_mover);
            CComQIPtr<IItemData> item_data(segment);
            item_data->AddItemData(CComBSTR(_T("Precast Girder")),girder);

            // Create strand material
            const matPsStrand* pStrand = pSegment->Strands.StrandMaterial[pgsTypes::Straight];
            CComPtr<IPrestressingStrand> straightStrandMaterial;
            straightStrandMaterial.CoCreateInstance(CLSID_PrestressingStrand);
            straightStrandMaterial->put_Name( CComBSTR(pStrand->GetName().c_str()) );
            straightStrandMaterial->put_Grade((StrandGrade)pStrand->GetGrade());
            straightStrandMaterial->put_Type((StrandType)pStrand->GetType());
            straightStrandMaterial->put_Size((StrandSize)pStrand->GetSize());
            straightStrandMaterial->put_InstallationStage(stressStrandIntervalIdx);
            girder->putref_StraightStrandMaterial(straightStrandMaterial);

            pStrand = pSegment->Strands.StrandMaterial[pgsTypes::Harped];
            CComPtr<IPrestressingStrand> harpedStrandMaterial;
            harpedStrandMaterial.CoCreateInstance(CLSID_PrestressingStrand);
            harpedStrandMaterial->put_Name( CComBSTR(pStrand->GetName().c_str()) );
            harpedStrandMaterial->put_Grade((StrandGrade)pStrand->GetGrade());
            harpedStrandMaterial->put_Type((StrandType)pStrand->GetType());
            harpedStrandMaterial->put_Size((StrandSize)pStrand->GetSize());
            harpedStrandMaterial->put_InstallationStage(stressStrandIntervalIdx);
            girder->putref_HarpedStrandMaterial(harpedStrandMaterial);

            pStrand = pSegment->Strands.StrandMaterial[pgsTypes::Temporary];
            CComPtr<IPrestressingStrand> temporaryStrandMaterial;
            temporaryStrandMaterial.CoCreateInstance(CLSID_PrestressingStrand);
            temporaryStrandMaterial->put_Name( CComBSTR(pStrand->GetName().c_str()) );
            temporaryStrandMaterial->put_Grade((StrandGrade)pStrand->GetGrade());
            temporaryStrandMaterial->put_Type((StrandType)pStrand->GetType());
            temporaryStrandMaterial->put_Size((StrandSize)pStrand->GetSize());
            temporaryStrandMaterial->put_InstallationStage(stressStrandIntervalIdx);
            girder->putref_TemporaryStrandMaterial(temporaryStrandMaterial);


            // Fill up strand patterns
            CComPtr<IStrandGrid> strGrd[2], harpGrdEnd[2], harpGrdHP[2], tempGrd[2];
            girder->get_StraightStrandGrid(etStart,&strGrd[etStart]);
            girder->get_HarpedStrandGridEnd(etStart,&harpGrdEnd[etStart]);
            girder->get_HarpedStrandGridHP(etStart,&harpGrdHP[etStart]);
            girder->get_TemporaryStrandGrid(etStart,&tempGrd[etStart]);

            girder->get_StraightStrandGrid(etEnd,&strGrd[etEnd]);
            girder->get_HarpedStrandGridEnd(etEnd,&harpGrdEnd[etEnd]);
            girder->get_HarpedStrandGridHP(etEnd,&harpGrdHP[etEnd]);
            girder->get_TemporaryStrandGrid(etEnd,&tempGrd[etEnd]);

            pGirderEntry->ConfigureStraightStrandGrid(strGrd[etStart],strGrd[etEnd]);
            pGirderEntry->ConfigureHarpedStrandGrids(harpGrdEnd[etStart], harpGrdHP[etStart], harpGrdHP[etEnd], harpGrdEnd[etEnd]);
            pGirderEntry->ConfigureTemporaryStrandGrid(tempGrd[etStart],tempGrd[etEnd]);

            girder->put_AllowOddNumberOfHarpedStrands(pGirderEntry->OddNumberOfHarpedStrands() ? VARIANT_TRUE : VARIANT_FALSE);

   #pragma Reminder("UPDATE: This is wrong for harping point measured as distance")

            // lay out the harping points
            GirderLibraryEntry::MeasurementLocation hpref = pGirderEntry->GetHarpingPointReference();
            GirderLibraryEntry::MeasurementType hpmeasure = pGirderEntry->GetHarpingPointMeasure();
            Float64 hpLoc = pGirderEntry->GetHarpingPointLocation();

            girder->put_UseMinHarpPointDistance( pGirderEntry->IsMinHarpingPointLocationUsed() ? VARIANT_TRUE : VARIANT_FALSE);
            girder->put_MinHarpPointDistance( pGirderEntry->GetMinHarpingPointLocation() );

            girder->SetHarpingPoints(hpLoc,hpLoc);
            girder->put_HarpingPointReference( HarpPointReference(hpref) );
            girder->put_HarpingPointMeasure( HarpPointMeasure(hpmeasure) );
         } // segment loop

         // add a tendon collection to the superstructure member... spliced girders know how to use it
         CComQIPtr<IItemData> ssmbrItemData(ssmbr);
         CComPtr<ITendonCollection> tendons;
         CreateTendons(pBridgeDesc,girderKey,ssmbr,&tendons);
         ssmbrItemData->AddItemData(CComBSTR(_T("Tendons")),tendons);
      } // girder loop

      // now that all the girders are layed out, we can layout the rebar
      for ( GirderIndexType gdrIdx = 0; gdrIdx < nGirders; gdrIdx++ )
      {
         CGirderKey girderKey(grpIdx,gdrIdx);

         const CSplicedGirderData* pGirder = pGroup->GetGirder(gdrIdx);

         SegmentIndexType nSegments = pGirder->GetSegmentCount();
         for ( SegmentIndexType segIdx = 0; segIdx < nSegments; segIdx++ )
         {
            CSegmentKey segmentKey(girderKey.groupIndex,girderKey.girderIndex,segIdx);

            LayoutSegmentRebar(segmentKey);

            if ( segIdx != nSegments-1 )
               LayoutClosurePourRebar(segmentKey);
         } // segment loop
      } // girder loop
   } // group loop
   return true;
}

bool CBridgeAgentImp::LayoutDeck(const CBridgeDescription2* pBridgeDesc)
{
   const CDeckDescription2* pDeck = pBridgeDesc->GetDeckDescription();
   pgsTypes::SupportedDeckType deckType = pDeck->DeckType;

   CComPtr<IBridgeDeck> deck;

   if ( IsAdjustableWidthDeck(deckType) )
   {
      if ( pDeck->DeckEdgePoints.size() == 1 )
         LayoutSimpleDeck(pBridgeDesc,&deck);
      else
         LayoutFullDeck(pBridgeDesc,&deck);
   }
   else
   {
      LayoutNoDeck(pBridgeDesc,&deck);
   }

   if ( deck )
   {
      LayoutDeckRebar(pDeck,deck);

      CComPtr<IMaterial> deck_material;
      deck_material.CoCreateInstance(CLSID_Material);

      IntervalIndexType compositeDeckIntervalIdx = GetCompositeDeckInterval();
      IntervalIndexType nIntervals = m_IntervalManager.GetIntervalCount();
      for ( IntervalIndexType intIdx = 0; intIdx < nIntervals; intIdx++ )
      {
         Float64 E(0), D(0);
         if ( compositeDeckIntervalIdx <= intIdx )
         {
            E = GetDeckAgeAdjustedEc(intIdx);
            D = GetDeckWeightDensity(intIdx);
         }

         deck_material->put_E(intIdx,E);
         deck_material->put_Density(intIdx,D);
      }
 
      deck->putref_Material(deck_material);
      m_Bridge->putref_Deck(deck);
   }


   return true;
}

void CBridgeAgentImp::NoDeckEdgePoint(GroupIndexType grpIdx,SegmentIndexType segIdx,pgsTypes::MemberEndType end,DirectionType side,IPoint2d** ppPoint)
{
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CGirderGroupData* pGroup = pBridgeDesc->GetGirderGroup(grpIdx);

   GirderIndexType gdrIdx;
   if ( side == qcbLeft )
   {
      gdrIdx = 0;
   }
   else
   {
      gdrIdx = pGroup->GetGirderCount()-1;
   }

   CSegmentKey segmentKey(grpIdx,gdrIdx,segIdx);

   Float64 location;
   if ( end == pgsTypes::metStart )
   {
      location = 0.0;
   }
   else
   {
      location = GetSegmentLength(segmentKey);
   }

   CComPtr<IGirderSection> girder_section;
   GetGirderSection(pgsPointOfInterest(segmentKey,location),pgsTypes::scBridge,&girder_section);
   Float64 width;
   girder_section->get_TopWidth(&width);
   width /= 2;

   CComPtr<ISuperstructureMember> ssmbr;
   HRESULT hr = m_Bridge->get_SuperstructureMember(::GetSuperstructureMemberID(grpIdx,gdrIdx),&ssmbr);
   ATLASSERT(SUCCEEDED(hr));

   CComPtr<ISegment> segment;
   hr = ssmbr->get_Segment(segmentKey.segmentIndex,&segment);
   ATLASSERT(SUCCEEDED(hr));

   CComPtr<IGirderLine> girderLine;
   hr = segment->get_GirderLine(&girderLine);
   ATLASSERT(SUCCEEDED(hr));

   CComPtr<IPoint2d> point_on_girder;
   hr = girderLine->get_PierPoint(end == pgsTypes::metStart ? etStart : etEnd,&point_on_girder);
   ATLASSERT(SUCCEEDED(hr));

   if ( side == qcbRight )
      width *= -1;

   CComPtr<IDirection> girder_direction;
   girderLine->get_Direction(&girder_direction); // bearing of girder

   CComPtr<IDirection> normal;
   girder_direction->Increment(CComVariant(PI_OVER_2),&normal);// rotate 90 to make a normal to the girder

   CComQIPtr<ILocate2> locate(m_CogoEngine);
   CComPtr<IPoint2d> point_on_edge;
   hr = locate->ByDistDir(point_on_girder,width,CComVariant(normal),0.00,&point_on_edge);
   ATLASSERT(SUCCEEDED(hr));

   (*ppPoint) = point_on_edge;
   (*ppPoint)->AddRef();
}

bool CBridgeAgentImp::LayoutNoDeck(const CBridgeDescription2* pBridgeDesc,IBridgeDeck** ppDeck)
{
   // There isn't an explicit deck in this case, so layout of the composite slab must be done
   // based on the girder geometry. Update the bridge model now so that the girder geometry is correct
   m_Bridge->UpdateBridgeModel();

   CComPtr<IBridgeGeometry> bridgeGeometry;
   m_Bridge->get_BridgeGeometry(&bridgeGeometry);

   CogoObjectID alignmentID;
   bridgeGeometry->get_BridgeAlignmentID(&alignmentID);

   HRESULT hr = S_OK;
   const CDeckDescription2* pDeck = pBridgeDesc->GetDeckDescription();
   pgsTypes::SupportedDeckType deckType = pDeck->DeckType;

   ATLASSERT( deckType == pgsTypes::sdtCompositeOverlay || deckType == pgsTypes::sdtNone );

   // create path objects that represent the deck edges
   CComPtr<IPath> left_path, right_path;
   left_path.CoCreateInstance(CLSID_Path);
   right_path.CoCreateInstance(CLSID_Path);

   // Build  path along exterior girders
   GroupIndexType nGroups = pBridgeDesc->GetGirderGroupCount();
   for ( GroupIndexType grpIdx = 0; grpIdx < nGroups; grpIdx++ )
   {
      GirderIndexType nGirders = pBridgeDesc->GetGirderGroup(grpIdx)->GetGirderCount();
      SegmentIndexType nSegments = pBridgeDesc->GetGirderGroup(grpIdx)->GetGirder(0)->GetSegmentCount();
      for ( SegmentIndexType segIdx = 0; segIdx < nSegments; segIdx++ )
      {
         CComPtr<IPoint2d> point_on_edge;

         // left edge - start of segment
         NoDeckEdgePoint(grpIdx,segIdx,pgsTypes::metStart,qcbLeft,&point_on_edge);
         left_path->AddEx(point_on_edge);

         // left edge - end of segment
         point_on_edge.Release();
         NoDeckEdgePoint(grpIdx,segIdx,pgsTypes::metEnd,qcbLeft,&point_on_edge);
         left_path->AddEx(point_on_edge);

         // right edge - start of segment
         point_on_edge.Release();
         NoDeckEdgePoint(grpIdx,segIdx,pgsTypes::metStart,qcbRight,&point_on_edge);
         right_path->AddEx(point_on_edge);

         // right edge - end of segment
         point_on_edge.Release();
         NoDeckEdgePoint(grpIdx,segIdx,pgsTypes::metEnd,qcbRight,&point_on_edge);
         right_path->AddEx(point_on_edge);
      } // next segment
   } // next group

   // store the deck edge paths as layout lines in the bridge geometry model
   CComPtr<ISimpleLayoutLineFactory> layoutLineFactory;
   layoutLineFactory.CoCreateInstance(CLSID_SimpleLayoutLineFactory);
   layoutLineFactory->AddPath(LEFT_DECK_EDGE_LAYOUT_LINE_ID,left_path);
   layoutLineFactory->AddPath(RIGHT_DECK_EDGE_LAYOUT_LINE_ID,right_path);
   bridgeGeometry->CreateLayoutLines(layoutLineFactory);

   // create the deck boundary
   CComPtr<ISimpleDeckBoundaryFactory> deckBoundaryFactory;
   deckBoundaryFactory.CoCreateInstance(CLSID_SimpleDeckBoundaryFactory);
   deckBoundaryFactory->put_BackEdgeID( GetPierLineID(0) );
   deckBoundaryFactory->put_BackEdgeType(setPier);
   deckBoundaryFactory->put_ForwardEdgeID( GetPierLineID(pBridgeDesc->GetPierCount()-1) ); 
   deckBoundaryFactory->put_ForwardEdgeType(setPier);
   deckBoundaryFactory->put_LeftEdgeID(LEFT_DECK_EDGE_LAYOUT_LINE_ID); 
   deckBoundaryFactory->put_RightEdgeID(RIGHT_DECK_EDGE_LAYOUT_LINE_ID);

   bridgeGeometry->CreateDeckBoundary(deckBoundaryFactory); 

   // get the deck boundary
   CComPtr<IDeckBoundary> deckBoundary;
   bridgeGeometry->get_DeckBoundary(&deckBoundary);

   // create the specific deck type
   CComPtr<IOverlaySlab> slab;
   slab.CoCreateInstance(CLSID_OverlaySlab);

   if ( deckType == pgsTypes::sdtCompositeOverlay )
   {
      slab->put_GrossDepth(pDeck->GrossDepth);
      if ( pDeck->WearingSurface == pgsTypes::wstSacrificialDepth || pDeck->WearingSurface == pgsTypes::wstFutureOverlay)
      {
         slab->put_SacrificialDepth(pDeck->SacrificialDepth);
      }
      else
      {
         slab->put_SacrificialDepth(0);
      }
   }
   else
   {
      // use a dummy zero-depth deck because there is no deck
      slab->put_GrossDepth(0);
      slab->put_SacrificialDepth(0);
   }

   slab.QueryInterface(ppDeck);

   (*ppDeck)->put_Composite( deckType == pgsTypes::sdtCompositeOverlay ? VARIANT_TRUE : VARIANT_FALSE );
   (*ppDeck)->putref_DeckBoundary(deckBoundary);


   return true;
}

bool CBridgeAgentImp::LayoutSimpleDeck(const CBridgeDescription2* pBridgeDesc,IBridgeDeck** ppDeck)
{
   const CDeckDescription2* pDeck = pBridgeDesc->GetDeckDescription();
   pgsTypes::SupportedDeckType deckType = pDeck->DeckType;

   ATLASSERT( deckType == pgsTypes::sdtCompositeSIP || deckType == pgsTypes::sdtCompositeCIP );
   ATLASSERT( pDeck->DeckEdgePoints.size() == 1 );

   Float64 alignment_offset = pBridgeDesc->GetAlignmentOffset();

   // this is a simple deck edge that parallels the alignment
   CDeckPoint deckPoint = *(pDeck->DeckEdgePoints.begin());

   Float64 left_offset;
   Float64 right_offset;

   if ( deckPoint.MeasurementType == pgsTypes::omtAlignment )
   {
      // deck edge is measured from the alignment
      left_offset  =  deckPoint.LeftEdge;
      right_offset = -deckPoint.RightEdge;
   }
   else
   {
      // deck edge is measured from the CL bridge. compute the offsets from the alignment
      left_offset  = -alignment_offset + deckPoint.LeftEdge;
      right_offset = -alignment_offset - deckPoint.RightEdge;
   }

   //
   // Create slab model in new BridgeGeometry sub-system
   //
   CComPtr<IBridgeGeometry> bridgeGeometry;
   m_Bridge->get_BridgeGeometry(&bridgeGeometry);

   PierIndexType nPiers = pBridgeDesc->GetPierCount();

   CogoObjectID alignmentID;
   bridgeGeometry->get_BridgeAlignmentID(&alignmentID);

   // Create slab edge paths
   CComPtr<IAlignmentOffsetLayoutLineFactory> factory;
   factory.CoCreateInstance(CLSID_AlignmentOffsetLayoutLineFactory);
   factory->put_AlignmentID(alignmentID);
   factory->put_LayoutLineID(LEFT_DECK_EDGE_LAYOUT_LINE_ID); // left edge
   factory->put_Offset(left_offset);
   bridgeGeometry->CreateLayoutLines(factory);

   factory->put_LayoutLineID(RIGHT_DECK_EDGE_LAYOUT_LINE_ID); // right edge
   factory->put_Offset(right_offset);
   bridgeGeometry->CreateLayoutLines(factory);

   CComPtr<ISimpleDeckBoundaryFactory> deckBoundaryFactory;
   deckBoundaryFactory.CoCreateInstance(CLSID_SimpleDeckBoundaryFactory);
   deckBoundaryFactory->put_BackEdgeID( GetPierLineID(0) );
   deckBoundaryFactory->put_BackEdgeType(setPier);
   deckBoundaryFactory->put_ForwardEdgeID( GetPierLineID(nPiers-1) ); 
   deckBoundaryFactory->put_ForwardEdgeType(setPier);
   deckBoundaryFactory->put_LeftEdgeID(LEFT_DECK_EDGE_LAYOUT_LINE_ID); 
   deckBoundaryFactory->put_RightEdgeID(RIGHT_DECK_EDGE_LAYOUT_LINE_ID);

#pragma Reminder("REVIEW: remember that the geometry engine can handle breaks in the bridge deck")
//#pragma Reminder("Fake slab break")
//   // fake slab break
//   factory->put_Offset(left_offset-0.5);
//   factory->put_LayoutLineID(-1001); // left edge
//   bridgeGeometry->CreateLayoutLines(factory);
//   factory->put_LayoutLineID(-2001); // right edge
//   factory->put_Offset(right_offset+0.5);
//   bridgeGeometry->CreateLayoutLines(factory);
//   deckBoundaryFactory->put_BreakLeftEdge(etStart,VARIANT_TRUE);
//   deckBoundaryFactory->put_BreakLeftEdge(etEnd,  VARIANT_TRUE);
//   deckBoundaryFactory->put_BreakRightEdge(etStart,VARIANT_TRUE);
//   deckBoundaryFactory->put_BreakRightEdge(etEnd,  VARIANT_TRUE);
//   deckBoundaryFactory->put_LeftEdgeBreakID(-1001);
//   deckBoundaryFactory->put_RightEdgeBreakID(-2001);

   bridgeGeometry->CreateDeckBoundary(deckBoundaryFactory); 

   CComPtr<IDeckBoundary> deckBoundary;
   bridgeGeometry->get_DeckBoundary(&deckBoundary);

   if ( deckType == pgsTypes::sdtCompositeCIP )
      LayoutCompositeCIPDeck(pDeck,deckBoundary,ppDeck);
   else if ( deckType == pgsTypes::sdtCompositeSIP )
      LayoutCompositeSIPDeck(pDeck,deckBoundary,ppDeck);
   else
      ATLASSERT(false); // shouldn't get here

   return true;
}

bool CBridgeAgentImp::LayoutFullDeck(const CBridgeDescription2* pBridgeDesc,IBridgeDeck** ppDeck)
{
   const CDeckDescription2* pDeck = pBridgeDesc->GetDeckDescription();
   pgsTypes::SupportedDeckType deckType = pDeck->DeckType;

   ATLASSERT( deckType == pgsTypes::sdtCompositeSIP || deckType == pgsTypes::sdtCompositeCIP );
   ATLASSERT( 1 < pDeck->DeckEdgePoints.size() );

   // the deck edge is described by a series of points. the transitions
   // between the points can be parallel to alignment, linear or spline. 
   // for spline transitions a cubic spline will be used. 
   // for parallel transitations, a parallel alignment sub-path will be used.
   CComPtr<IAlignment> alignment;
   m_Bridge->get_Alignment(&alignment);

   CComPtr<IBridgeGeometry> geometry;
   m_Bridge->get_BridgeGeometry(&geometry);

   // create the path objects for the edge of deck
   CComPtr<IPath> left_path, right_path;
   CDeckEdgeBuilder deck_edge_builder;
   deck_edge_builder.BuildDeckEdges(pBridgeDesc,m_CogoEngine,alignment,&left_path,&right_path);

   // store the deck edge paths as layout lines in the bridge geometry model
   CComPtr<ISimpleLayoutLineFactory> layoutLineFactory;
   layoutLineFactory.CoCreateInstance(CLSID_SimpleLayoutLineFactory);
   layoutLineFactory->AddPath(LEFT_DECK_EDGE_LAYOUT_LINE_ID,left_path);
   layoutLineFactory->AddPath(RIGHT_DECK_EDGE_LAYOUT_LINE_ID,right_path);
   geometry->CreateLayoutLines(layoutLineFactory);

   // create the bridge deck boundary
   CComPtr<ISimpleDeckBoundaryFactory> deckBoundaryFactory;
   deckBoundaryFactory.CoCreateInstance(CLSID_SimpleDeckBoundaryFactory);
   deckBoundaryFactory->put_BackEdgeID(::GetPierLineID(0));
   deckBoundaryFactory->put_BackEdgeType(setPier);
   deckBoundaryFactory->put_ForwardEdgeID(::GetPierLineID(pBridgeDesc->GetPierCount()-1));
   deckBoundaryFactory->put_ForwardEdgeType(setPier);
   deckBoundaryFactory->put_LeftEdgeID(LEFT_DECK_EDGE_LAYOUT_LINE_ID);
   deckBoundaryFactory->put_RightEdgeID(RIGHT_DECK_EDGE_LAYOUT_LINE_ID);
   geometry->CreateDeckBoundary(deckBoundaryFactory);

   // get the deck boundary
   CComPtr<IDeckBoundary> deckBoundary;
   geometry->get_DeckBoundary(&deckBoundary);

   if ( deckType == pgsTypes::sdtCompositeCIP )
      LayoutCompositeCIPDeck(pDeck,deckBoundary,ppDeck);
   else if ( deckType == pgsTypes::sdtCompositeSIP )
      LayoutCompositeSIPDeck(pDeck,deckBoundary,ppDeck);
   else
      ATLASSERT(false); // shouldn't get here

   return true;
}

bool CBridgeAgentImp::LayoutCompositeCIPDeck(const CDeckDescription2* pDeck,IDeckBoundary* pBoundary,IBridgeDeck** ppDeck)
{
   CComPtr<ICastSlab> slab;
   slab.CoCreateInstance(CLSID_CastSlab);

   slab->put_Fillet(pDeck->Fillet);
   slab->put_GrossDepth(pDeck->GrossDepth);
   slab->put_OverhangDepth(pDeck->OverhangEdgeDepth);
   slab->put_OverhangTaper((DeckOverhangTaper)pDeck->OverhangTaper);

   if ( pDeck->WearingSurface == pgsTypes::wstSacrificialDepth || pDeck->WearingSurface == pgsTypes::wstFutureOverlay)
   {
      slab->put_SacrificialDepth(pDeck->SacrificialDepth);
   }
   else
   {
      slab->put_SacrificialDepth(0);
   }

   slab.QueryInterface(ppDeck);

   (*ppDeck)->put_Composite( VARIANT_TRUE );
   (*ppDeck)->putref_DeckBoundary(pBoundary);

   return true;
}

bool CBridgeAgentImp::LayoutCompositeSIPDeck(const CDeckDescription2* pDeck,IDeckBoundary* pBoundary,IBridgeDeck** ppDeck)
{
   CComPtr<IPrecastSlab> slab;
   slab.CoCreateInstance(CLSID_PrecastSlab);

   slab->put_Fillet(pDeck->Fillet);
   slab->put_PanelDepth(pDeck->PanelDepth);
   slab->put_CastDepth(pDeck->GrossDepth); // interpreted as cast depth
   slab->put_OverhangDepth(pDeck->OverhangEdgeDepth);
   slab->put_OverhangTaper((DeckOverhangTaper)pDeck->OverhangTaper);

   if ( pDeck->WearingSurface == pgsTypes::wstSacrificialDepth || pDeck->WearingSurface == pgsTypes::wstFutureOverlay)
   {
      slab->put_SacrificialDepth(pDeck->SacrificialDepth);
   }
   else
   {
      slab->put_SacrificialDepth(0);
   }

   slab.QueryInterface(ppDeck);

   (*ppDeck)->put_Composite( VARIANT_TRUE );
   (*ppDeck)->putref_DeckBoundary(pBoundary);

   return true;
}

bool CBridgeAgentImp::LayoutTrafficBarriers(const CBridgeDescription2* pBridgeDesc)
{
   CComPtr<ISidewalkBarrier> lb;
   const CRailingSystem* pLeftRailingSystem  = pBridgeDesc->GetLeftRailingSystem();
   LayoutTrafficBarrier(pBridgeDesc,pLeftRailingSystem,pgsTypes::tboLeft,&lb);
   m_Bridge->putref_LeftBarrier(lb);

   CComPtr<ISidewalkBarrier> rb;
   const CRailingSystem* pRightRailingSystem = pBridgeDesc->GetRightRailingSystem();
   LayoutTrafficBarrier(pBridgeDesc,pRightRailingSystem,pgsTypes::tboRight,&rb);
   m_Bridge->putref_RightBarrier(rb);

   return true;
}

void ComputeBarrierShapeToeLocations(IShape* pShape, Float64* leftToe, Float64* rightToe)
{
   // barrier toe is at X=0.0 of shape. Clip a very shallow rect at this elevation and use the width
   // of the clipped shape as the toe bounds.
   CComPtr<IRect2d> rect;
   rect.CoCreateInstance(CLSID_Rect2d);
   rect->SetBounds(-1.0e06, 1.0e06, 0.0, 1.0e-04);

   CComPtr<IShape> clip_shape;
   pShape->ClipIn(rect, &clip_shape);
   if (clip_shape)
   {
      CComPtr<IRect2d> bbox;
      clip_shape->get_BoundingBox(&bbox);
      bbox->get_Left(leftToe);
      bbox->get_Right(rightToe);
   }
   else
   {
      *leftToe = 0.0;
      *rightToe = 0.0;
   }
}

void CBridgeAgentImp::CreateBarrierObject(IBarrier** pBarrier, const TrafficBarrierEntry*  pBarrierEntry, pgsTypes::TrafficBarrierOrientation orientation)
{
   CComPtr<IPolyShape> polyshape;
   pBarrierEntry->CreatePolyShape(orientation,&polyshape);

   CComQIPtr<IShape> pShape(polyshape);

   // box containing barrier
   CComPtr<IRect2d> bbox;
   pShape->get_BoundingBox(&bbox);

   Float64 rightEdge, leftEdge;
   bbox->get_Right(&rightEdge);
   bbox->get_Left(&leftEdge);

   // Distance from the barrier curb to the exterior face of barrier for purposes of determining
   // roadway width
   Float64 curbOffset = pBarrierEntry->GetCurbOffset();

   // Toe locations of barrier
   Float64 leftToe, rightToe;
   ComputeBarrierShapeToeLocations(pShape, &leftToe, &rightToe);

   // Dimensions of barrier depend on orientation
   Float64 extToeWid, intToeWid;
   if (orientation == pgsTypes::tboLeft )
   {
      intToeWid = rightEdge - rightToe;
      extToeWid = -(leftEdge - leftToe);
   }
   else
   {
      extToeWid = rightEdge - rightToe;
      intToeWid = -(leftEdge - leftToe);
   }

   // We have all data. Create barrier and initialize
   CComPtr<IGenericBarrier> barrier;
   barrier.CoCreateInstance(CLSID_GenericBarrier);

   barrier->Init(pShape, curbOffset, intToeWid, extToeWid);

   barrier.QueryInterface(pBarrier);

   CComPtr<IMaterial> material;
   (*pBarrier)->get_Material(&material);

   IntervalIndexType nIntervals = m_IntervalManager.GetIntervalCount();
   for ( IntervalIndexType intervalIdx = 0; intervalIdx < nIntervals; intervalIdx++ )
   {
      Float64 E = GetRailingSystemEc(orientation,intervalIdx);
      Float64 D = GetRailingSystemWeightDensity(orientation,intervalIdx);
      material->put_E(intervalIdx,E);
      material->put_Density(intervalIdx,D);
   }
}

bool CBridgeAgentImp::LayoutTrafficBarrier(const CBridgeDescription2* pBridgeDesc,const CRailingSystem* pRailingSystem,pgsTypes::TrafficBarrierOrientation orientation,ISidewalkBarrier** ppBarrier)
{
   // Railing system object
   CComPtr<ISidewalkBarrier> railing_system;
   railing_system.CoCreateInstance(CLSID_SidewalkBarrier);

   GET_IFACE(ILibrary,pLib);
   const TrafficBarrierEntry*  pExtRailingEntry = pLib->GetTrafficBarrierEntry( pRailingSystem->strExteriorRailing.c_str() );

   // Exterior Barrier
   CComPtr<IBarrier> extBarrier;
   CreateBarrierObject(&extBarrier, pExtRailingEntry, orientation);

   railing_system->put_IsExteriorStructurallyContinuous(pExtRailingEntry->IsBarrierStructurallyContinuous() ? VARIANT_TRUE : VARIANT_FALSE);

   SidewalkPositionType swPosition = pRailingSystem->bBarriersOnTopOfSidewalk ? swpBeneathBarriers : swpBetweenBarriers;

   int barrierType = 1;
   Float64 h1,h2,w;
   CComPtr<IBarrier> intBarrier;
   if ( pRailingSystem->bUseSidewalk )
   {
      // get the sidewalk dimensions
      barrierType = 2;
      h1 = pRailingSystem->LeftDepth;
      h2 = pRailingSystem->RightDepth;
      w  = pRailingSystem->Width;

      railing_system->put_IsSidewalkStructurallyContinuous(pRailingSystem->bSidewalkStructurallyContinuous ? VARIANT_TRUE : VARIANT_FALSE);

      if ( pRailingSystem->bUseInteriorRailing )
      {
         // there is an interior railing as well
         barrierType = 3;
         const TrafficBarrierEntry* pIntRailingEntry = pLib->GetTrafficBarrierEntry( pRailingSystem->strInteriorRailing.c_str() );

         CreateBarrierObject(&intBarrier, pIntRailingEntry, orientation);

         railing_system->put_IsInteriorStructurallyContinuous(pIntRailingEntry->IsBarrierStructurallyContinuous() ? VARIANT_TRUE : VARIANT_FALSE);
      }
   }

   switch(barrierType)
   {
   case 1:
      railing_system->put_Barrier1(extBarrier,(TrafficBarrierOrientation)orientation);
      break;

   case 2:
      railing_system->put_Barrier2(extBarrier,h1,h2,w,(TrafficBarrierOrientation)orientation, swPosition);
      break;

   case 3:
      railing_system->put_Barrier3(extBarrier,h1,h2,w,(TrafficBarrierOrientation)orientation, swPosition, intBarrier);
      break;

   default:
      ATLASSERT(FALSE); // should never get here
   }


   (*ppBarrier) = railing_system;
   (*ppBarrier)->AddRef();

   return true;
}

bool CBridgeAgentImp::BuildGirder()
{
   UpdatePrestressing(ALL_GROUPS,ALL_GIRDERS,ALL_SEGMENTS);
   return true;
}

void CBridgeAgentImp::ValidateGirder()
{
   GET_IFACE(IEAFStatusCenter,pStatusCenter);
   GET_IFACE(IBridgeDescription,pIBridgeDesc);

   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   GroupIndexType nGroups = pBridgeDesc->GetGirderGroupCount();
   for ( GroupIndexType grpIdx = 0; grpIdx < nGroups; grpIdx++ )
   {
      const CGirderGroupData* pGroup = pBridgeDesc->GetGirderGroup(grpIdx);

      GirderIndexType nGirders = pGroup->GetGirderCount();
      for ( GirderIndexType gdrIdx = 0; gdrIdx < nGirders; gdrIdx++ )
      {
         const CSplicedGirderData* pGirder = pGroup->GetGirder(gdrIdx);
         SegmentIndexType nSegments = pGirder->GetSegmentCount();
         for ( SegmentIndexType segIdx = 0; segIdx < nSegments; segIdx++ )
         {
            CSegmentKey segmentKey(grpIdx,gdrIdx,segIdx);

            IntervalIndexType releaseIntervalIdx = GetPrestressReleaseInterval(segmentKey);

            Float64 length = GetSegmentLength(segmentKey);
            Float64 effStrands;
            Float64 end_ecc = GetHsEccentricity(releaseIntervalIdx, pgsPointOfInterest(segmentKey, 0.0),       &effStrands);
            Float64 hp_ecc  = GetHsEccentricity(releaseIntervalIdx, pgsPointOfInterest(segmentKey,length/2.0), &effStrands);

            if (hp_ecc+TOLERANCE < end_ecc)
            {
               LPCTSTR msg = _T("Harped strand eccentricity at girder ends is larger than at harping points. Drape is upside down");
               pgsGirderDescriptionStatusItem* pStatusItem = new pgsGirderDescriptionStatusItem(segmentKey,0,m_StatusGroupID,m_scidGirderDescriptionWarning,msg);
               pStatusCenter->Add(pStatusItem);
            }
         } // segment loop
      } // girder loop
   } // group loop
}

void CBridgeAgentImp::UpdatePrestressing(GroupIndexType groupIdx,GirderIndexType girderIdx,SegmentIndexType segmentIdx)
{
   GET_IFACE(IEAFStatusCenter,pStatusCenter);
   GET_IFACE(ISegmentData,pSegmentData);
   GET_IFACE(ILibrary,pLib);
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();

   GroupIndexType firstGroupIdx,lastGroupIdx;
   if ( groupIdx == ALL_GROUPS )
   {
      firstGroupIdx = 0;
      lastGroupIdx = pBridgeDesc->GetGirderGroupCount()-1;
   }
   else
   {
      firstGroupIdx = groupIdx;
      lastGroupIdx = firstGroupIdx;
   }

   for ( GroupIndexType grpIdx = firstGroupIdx; grpIdx <= lastGroupIdx; grpIdx++ )
   {
      const CGirderGroupData* pGroup = pBridgeDesc->GetGirderGroup(grpIdx);

      GirderIndexType firstGirderIdx, lastGirderIdx;
      if ( girderIdx == ALL_GIRDERS )
      {
         firstGirderIdx = 0;
         lastGirderIdx = pGroup->GetGirderCount()-1;
      }
      else
      {
         firstGirderIdx = girderIdx;
         lastGirderIdx = firstGirderIdx;
      }

      for ( GirderIndexType gdrIdx = firstGirderIdx; gdrIdx <= lastGirderIdx; gdrIdx++ )
      {
         Float64 segmentOffset = 0;

         const CSplicedGirderData* pGirder = pGroup->GetGirder(gdrIdx);
         SegmentIndexType firstSegmentIdx, lastSegmentIdx;
         if ( segmentIdx == ALL_SEGMENTS )
         {
            firstSegmentIdx = 0;
            lastSegmentIdx = pGirder->GetSegmentCount()-1;
         }
         else
         {
            firstSegmentIdx = segmentIdx;
            lastSegmentIdx = firstSegmentIdx;
         }

         for ( SegmentIndexType segIdx = firstSegmentIdx; segIdx <= lastSegmentIdx; segIdx++ )
         {
            // should be passing segment key into this method
            CSegmentKey segmentKey(grpIdx,gdrIdx,segIdx);
            const GirderLibraryEntry* pGirderEntry = GetGirderLibraryEntry(segmentKey);

            // Inititalize a strand filler for each girder
            InitializeStrandFiller(pGirderEntry, segmentKey);

            CComPtr<IPrecastGirder> girder;
            GetGirder(segmentKey,&girder);

            const CPrecastSegmentData* pSegment = pGirder->GetSegment(segIdx);

            girder->ClearStraightStrandDebonding();

            // Fill strands
            int permFillType = pSegment->Strands.NumPermStrandsType;
            if (permFillType == CStrandData::npsTotal || permFillType == CStrandData::npsStraightHarped )
            {
               // Continuous fill
               CContinuousStrandFiller* pfiller = this->GetContinuousStrandFiller(segmentKey);

               HRESULT hr;
               hr = pfiller->SetStraightContinuousFill(girder, pSegment->Strands.GetNstrands(pgsTypes::Straight));
               ATLASSERT( SUCCEEDED(hr));
               hr = pfiller->SetHarpedContinuousFill(girder, pSegment->Strands.GetNstrands(pgsTypes::Harped));
               ATLASSERT( SUCCEEDED(hr));
               hr = pfiller->SetTemporaryContinuousFill(girder, pSegment->Strands.GetNstrands(pgsTypes::Temporary));
               ATLASSERT( SUCCEEDED(hr));
            }
            else if (permFillType == CStrandData::npsDirectSelection)
            {
               // Direct fill
               CDirectStrandFiller* pfiller = this->GetDirectStrandFiller(segmentKey);

               HRESULT hr;
               hr = pfiller->SetStraightDirectStrandFill(girder, pSegment->Strands.GetDirectStrandFillStraight());
               ATLASSERT( SUCCEEDED(hr));
               hr = pfiller->SetHarpedDirectStrandFill(girder, pSegment->Strands.GetDirectStrandFillHarped());
               ATLASSERT( SUCCEEDED(hr));
               hr = pfiller->SetTemporaryDirectStrandFill(girder, pSegment->Strands.GetDirectStrandFillTemporary());
               ATLASSERT( SUCCEEDED(hr));
            }

            // Apply harped strand pattern offsets.
            // Get fill array for harped and convert to ConfigStrandFillVector
            CComPtr<IIndexArray> hFillArray;
            girder->get_HarpedStrandFill(&hFillArray);
            ConfigStrandFillVector hFillVec;
            IndexArray2ConfigStrandFillVec(hFillArray, hFillVec);

            bool force_straight = pGirderEntry->IsForceHarpedStrandsStraight();
            Float64 adjustment(0.0);
            if ( pGirderEntry->IsVerticalAdjustmentAllowedEnd() )
            {
               adjustment = this->ComputeAbsoluteHarpedOffsetEnd(segmentKey, hFillVec, pSegment->Strands.HsoEndMeasurement, pSegment->Strands.HpOffsetAtEnd);
               girder->put_HarpedStrandAdjustmentEnd(adjustment);

               // use same adjustment at harping points if harped strands are forced to straight
               if (force_straight)
               {
                  girder->put_HarpedStrandAdjustmentHP(adjustment);
               }
            }

            if ( pGirderEntry->IsVerticalAdjustmentAllowedHP() && !force_straight )
            {
               adjustment = this->ComputeAbsoluteHarpedOffsetHp(segmentKey, hFillVec, 
                                                                pSegment->Strands.HsoHpMeasurement, pSegment->Strands.HpOffsetAtHp);
               girder->put_HarpedStrandAdjustmentHP(adjustment);

               ATLASSERT(!pGirderEntry->IsForceHarpedStrandsStraight()); // should not happen
            }

            // Apply debonding
            std::vector<CDebondData>::const_iterator iter;
            for ( iter = pSegment->Strands.Debond[pgsTypes::Straight].begin(); 
                  iter != pSegment->Strands.Debond[pgsTypes::Straight].end(); iter++ )
            {
               const CDebondData& debond_data = *iter;

               // debond data index is in same order as grid fill
               girder->DebondStraightStrandByGridIndex(debond_data.strandTypeGridIdx,debond_data.Length1,debond_data.Length2);
            }
         
            // lay out POIs based on this prestressing
            LayoutPrestressTransferAndDebondPoi(segmentKey,segmentOffset); 

            segmentOffset += GetSegmentLayoutLength(segmentKey);

         } // segment loop
      } // girder loop
   } // group loop
}

bool CBridgeAgentImp::AreGirderTopFlangesRoughened(const CSegmentKey& segmentKey)
{
   GET_IFACE(IShear,pShear);
	const CShearData2* pShearData = pShear->GetSegmentShearData(segmentKey);
   return pShearData->bIsRoughenedSurface;
}

void CBridgeAgentImp::LayoutPointsOfInterest(const CGirderKey& girderKey)
{
   ATLASSERT(girderKey.groupIndex  != ALL_GROUPS);
   ATLASSERT(girderKey.girderIndex != ALL_GIRDERS);

   SegmentIndexType nSegments = GetSegmentCount(girderKey);
   Float64 segment_offset = 0; // offset from start of the girder to the end of the previous segment

   for ( SegmentIndexType segIdx = 0; segIdx < nSegments; segIdx++ )
   {
      CSegmentKey segmentKey(girderKey,segIdx);

      LayoutRegularPoi(segmentKey,10,segment_offset); // 10th points in each segment.
      LayoutSpecialPoi(segmentKey,segment_offset);

      Float64 segment_layout_length = GetSegmentLayoutLength(segmentKey);
      segment_offset += segment_layout_length;
   }
}

void CBridgeAgentImp::LayoutRegularPoi(const CSegmentKey& segmentKey,Uint16 nPnts,Float64 segmentOffset)
{
   // This method creates POIs at n-th points for segment spans lengths at release and for the erected segment

   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   GET_IFACE(IClosurePour,pClosurePour);

   const Float64 toler = +1.0e-6;

   Float64 segment_length  = GetSegmentLength(segmentKey); // end to end length
   Float64 span_length     = GetSegmentSpanLength(segmentKey); // cl brg to cl brg length
   Float64 left_brg_offset = GetSegmentStartBearingOffset(segmentKey);
   Float64 left_end_size   = GetSegmentStartEndDistance(segmentKey);

   // distance from CL Support to start end of girder
   Float64 start_offset = left_brg_offset - left_end_size;

   CSegmentKey firstSegmentKey(segmentKey);
   firstSegmentKey.segmentIndex = 0;
   Float64 first_segment_start_offset = GetSegmentStartBearingOffset(firstSegmentKey) - GetSegmentStartEndDistance(firstSegmentKey);

   for ( Uint16 i = 0; i <= nPnts; i++ )
   {
      Float64 casting_yard_dist = segment_length * ((Float64)i / (Float64)nPnts); // distance from CL Brg
      Float64 bridge_site_dist = span_length * ((Float64)i / (Float64)nPnts); // distance from CL Brg

      PoiAttributeType attribute = 0;

      Uint16 tenthPoint = 0;

      // Add a special attribute flag if this poi is at the mid-span
      if ( i == (nPnts-1)/2 + 1)
         attribute |= POI_MIDSPAN;

      // Add a special attribute flag if poi is at a tenth point
      Float64 val = Float64(i)/Float64(nPnts)+toler;
      Float64 modv = fmod(val, 0.1);
      if (IsZero(modv,2*toler) || modv==0.1)
      {
         tenthPoint = Uint16(10.*Float64(i)/Float64(nPnts) + 1);
      }

      // create casting yard POI
      Float64 Xpoi = casting_yard_dist; // distance from left face of segment
      Float64 Xs   = start_offset + Xpoi; // distance from CLSupport - CLSegment intersection point
      Float64 Xgp  = segmentOffset + Xs; // distance from CLSupport - CLGirder intersection point
      Float64 Xg   = Xgp - first_segment_start_offset;  // distance from left face of first segment
      pgsPointOfInterest cyPoi(segmentKey,Xpoi,Xs,Xg,Xgp,attribute | POI_RELEASED_SEGMENT);
      cyPoi.MakeTenthPoint(POI_RELEASED_SEGMENT,tenthPoint);
      m_PoiMgr.AddPointOfInterest( cyPoi );

      Xpoi = left_end_size + bridge_site_dist;
      Xs   = start_offset + Xpoi;
      Xgp  = segmentOffset + Xs;
      Xg   = Xgp - first_segment_start_offset;
      pgsPointOfInterest bsPoi(segmentKey,Xpoi,Xs,Xg,Xgp,attribute | POI_ERECTED_SEGMENT);
      bsPoi.MakeTenthPoint(POI_ERECTED_SEGMENT,tenthPoint);
      m_PoiMgr.AddPointOfInterest( bsPoi );

      // if this is the last poi on the segment, and there is a closure pour on the right end
      // of the segment, then add a closure POI
      if ( i == nPnts )
      {
         const CSplicedGirderData*  pGirder  = pIBridgeDesc->GetGirder(segmentKey);
         const CPrecastSegmentData* pSegment = pGirder->GetSegment(segmentKey.segmentIndex);
         const CClosurePourData*    pClosure = pSegment->GetRightClosure();
         if ( pClosure != NULL )
         {
            Float64 closure_left, closure_right;
            pClosurePour->GetClosurePourSize(pClosure->GetClosureKey(),&closure_left,&closure_right);

            Float64 X = segment_length + closure_left;
            Float64 Xs = start_offset + X;
            Float64 Xgp = segmentOffset + Xs;
            Float64 Xg = Xgp - first_segment_start_offset;
            pgsPointOfInterest cpPOI(segmentKey,X,Xs,Xg,Xgp,POI_CLOSURE);
            m_PoiMgr.AddPointOfInterest( cpPOI );
         }
      }
   }
}

void CBridgeAgentImp::LayoutSpecialPoi(const CSegmentKey& segmentKey,Float64 segmentOffset)
{
   LayoutEndSizePoi(segmentKey,segmentOffset);
   LayoutHarpingPointPoi(segmentKey,segmentOffset);
   //LayoutPrestressTransferAndDebondPoi(segmentKey,segmentOffset); // this is done when the prestressing is updated
   LayoutPoiForDiaphragmLoads(segmentKey,segmentOffset);
   LayoutPoiForShear(segmentKey,segmentOffset);
   LayoutPoiForSegmentBarCutoffs(segmentKey,segmentOffset);

   LayoutPoiForSlabBarCutoffs(segmentKey);
   LayoutPoiForHandling(segmentKey);     // lifting and hauling
   LayoutPoiForSectionChanges(segmentKey);
   LayoutPoiForPiers(segmentKey);
   LayoutPoiForTemporarySupports(segmentKey);
}

void CBridgeAgentImp::ModelCantilevers(const CSegmentKey& segmentKey,bool* pbStartCantilever,bool* pbEndCantilever)
{
#pragma Reminder("UPDATE: ")
   // put this method on an interface and make it accessible everywhere... remove
   // duplicate from Analysis Agent

   // This method determines if the overhangs at the ends of a segment are modeled as cantilevers in the LBAM
   // Overhangs are modeled as cantilevers if they are longer than the height of the segment at the CL Bearing
   Float64 segment_length       = GetSegmentLength(segmentKey);
   Float64 start_offset         = GetSegmentStartEndDistance(segmentKey);
   Float64 end_offset           = GetSegmentEndEndDistance(segmentKey);
   Float64 segment_height_start = GetHeight(pgsPointOfInterest(segmentKey,start_offset));
   Float64 segment_height_end   = GetHeight(pgsPointOfInterest(segmentKey,segment_length-end_offset));

   // the cantilevers at the ends of the segment are modeled as flexural members if
   // if the cantilever length exceeds the height of the girder
   *pbStartCantilever = (start_offset < segment_height_start ? false : true);
   *pbEndCantilever   = (end_offset   < segment_height_end   ? false : true);
}

void CBridgeAgentImp::LayoutEndSizePoi(const CSegmentKey& segmentKey,Float64 segmentOffset)
{
   Float64 left_end_dist = GetSegmentStartEndDistance(segmentKey);
   Float64 left_brg_offset = GetSegmentStartBearingOffset(segmentKey);
   Float64 right_end_dist = GetSegmentEndEndDistance(segmentKey);
   Float64 segment_length = GetSegmentLength(segmentKey);
   Float64 span_length     = GetSegmentSpanLength(segmentKey); // cl brg to cl brg length

   Float64 start_offset = left_brg_offset - left_end_dist;

   CSegmentKey firstSegmentKey(segmentKey);
   firstSegmentKey.segmentIndex = 0;
   Float64 first_segment_start_offset = GetSegmentStartBearingOffset(firstSegmentKey) - GetSegmentStartEndDistance(firstSegmentKey);

   bool bStartCantilever, bEndCantilever;
   ModelCantilevers(segmentKey,&bStartCantilever,&bEndCantilever);
   if ( bStartCantilever )
   {
      // model 3 poi per cantilever, except the poi spacing can't exceed 
      // the 10th point spacing
      Float64 max_poi_spacing = span_length/10.0;
      IndexType nPoi = 3;
      Float64 poi_spacing = left_end_dist/nPoi;
      if ( max_poi_spacing < poi_spacing )
      {
         nPoi = IndexType(left_end_dist/max_poi_spacing);
         poi_spacing = left_end_dist/nPoi;
      }

      Float64 X = 0;
      for ( IndexType poiIdx = 0; poiIdx < nPoi; poiIdx++ )
      {
         Float64 Xs = X + start_offset;
         Float64 Xgp = segmentOffset + Xs;
         Float64 Xg = Xgp - first_segment_start_offset;
         pgsPointOfInterest poi(segmentKey,X,Xs,Xg,Xgp);
         m_PoiMgr.AddPointOfInterest(poi);
         X += poi_spacing;
      }
   }

   if ( bEndCantilever )
   {
      // model 3 poi per cantilever, except the poi spacing can't exceed 
      // the 10th point spacing
      Float64 max_poi_spacing = span_length/10.0;
      IndexType nPoi = 3;
      Float64 poi_spacing = right_end_dist/nPoi;
      if ( max_poi_spacing < poi_spacing )
      {
         nPoi = IndexType(right_end_dist/max_poi_spacing);
         poi_spacing = right_end_dist/nPoi;
      }

      Float64 X = segment_length - right_end_dist;
      for ( IndexType poiIdx = 0; poiIdx < nPoi; poiIdx++ )
      {
         Float64 Xs = X + start_offset;
         Float64 Xgp = segmentOffset + Xs;
         Float64 Xg = Xgp - first_segment_start_offset;
         pgsPointOfInterest poi(segmentKey, X, Xs, Xg, Xgp);
         m_PoiMgr.AddPointOfInterest( poi );
         X += poi_spacing;
      }
   }

   // Add a POI at the start and end of every segment, except at the start of the first segment in the first group
   // and the end of the last segment in the last group.
   //
   // POIs are needed at this location because of the way the Anaysis Agent builds the LBAM models. At intermediate piers
   // and closure pours, the "span length" is extended to the end of the segment rather than the CL Bearing.
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CGirderGroupData* pGroup = pBridgeDesc->GetGirderGroup(segmentKey.groupIndex);
   GroupIndexType nGroups = pBridgeDesc->GetGirderGroupCount();
   const CSplicedGirderData* pGirder = pGroup->GetGirder(segmentKey.girderIndex);
   SegmentIndexType nSegments = pGirder->GetSegmentCount();
   if ( !((segmentKey.segmentIndex == 0) && (segmentKey.groupIndex == 0)) )
   {
      Float64 X = 0;
      Float64 Xs = X + start_offset;
      Float64 Xgp = segmentOffset + Xs;
      Float64 Xg = Xgp - first_segment_start_offset;
      pgsPointOfInterest poi(segmentKey,X,Xs,Xg,Xgp);
      m_PoiMgr.AddPointOfInterest(poi);
   }

   if ( !((segmentKey.segmentIndex == nSegments-1) && (segmentKey.groupIndex == nGroups-1)) )
   {
      Float64 X = segment_length;
      Float64 Xs = X + start_offset;
      Float64 Xgp = segmentOffset + Xs;
      Float64 Xg = Xgp - first_segment_start_offset;
      pgsPointOfInterest poi(segmentKey,X,Xs,Xg,Xgp);
      m_PoiMgr.AddPointOfInterest(poi);
   }
}

void CBridgeAgentImp::LayoutHarpingPointPoi(const CSegmentKey& segmentKey,Float64 segmentOffset)
{
   IndexType maxHarped = GetNumHarpPoints(segmentKey);

   // if there can't be any harped strands, then there is no need to use the harping point attribute
   if ( maxHarped == 0 )
      return; 

   Float64 left_end_dist = GetSegmentStartEndDistance(segmentKey);
   Float64 left_brg_offset = GetSegmentStartBearingOffset(segmentKey);
   Float64 segment_length = GetSegmentLength(segmentKey);
   Float64 start_offset = left_brg_offset - left_end_dist;

   CSegmentKey firstSegmentKey(segmentKey);
   firstSegmentKey.segmentIndex = 0;
   Float64 first_segment_start_offset = GetSegmentStartBearingOffset(firstSegmentKey) - GetSegmentStartEndDistance(firstSegmentKey);

   CComPtr<IPrecastGirder> girder;
   GetGirder(segmentKey,&girder);

   Float64 hp1, hp2;
   girder->GetHarpingPointLocations( &hp1, &hp2 );

   Float64 X = hp1;
   Float64 Xs = X + start_offset;
   Float64 Xgp = segmentOffset + Xs;
   Float64 Xg = Xgp - first_segment_start_offset;
   pgsPointOfInterest poiHP1(segmentKey,X,Xs,Xg,Xgp,POI_HARPINGPOINT);

   X = hp2;
   Xs = X + start_offset;
   Xgp = segmentOffset + Xs;
   Xg = Xgp - first_segment_start_offset;
   pgsPointOfInterest poiHP2(segmentKey,X,Xs,Xg,Xgp,POI_HARPINGPOINT);


   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CTimelineManager* pTimelineMgr = pIBridgeDesc->GetTimelineManager();

   Float64 left_end, right_end, length;
   girder->get_LeftEndDistance(&left_end);
   girder->get_RightEndDistance(&right_end);
   girder->get_GirderLength(&length);
   if ( hp1 <= left_end || (length-right_end) <= hp2 )
   {
      // harp points are outside of the support location
      GET_IFACE(IEAFStatusCenter,pStatusCenter);
      GET_IFACE(IDocumentType,pDocType);
      std::_tostringstream os;
      if ( pDocType->IsPGSuperDocument() )
      {
         ATLASSERT(segmentKey.segmentIndex == 0);
         os << _T("The harping points for Girder ") << LABEL_GIRDER(segmentKey.girderIndex) << _T(" in Span ") << LABEL_SPAN(segmentKey.groupIndex) 
            << _T(" are located outside of the bearings. You can fix this by increasing the girder length, or")
            << _T(" by changing the harping point location in the girder library entry.")<<std::endl;
      }
      else
      {
         os << _T("The harping points for Group ") << LABEL_GROUP(segmentKey.groupIndex) << _T(" Girder ") << LABEL_GIRDER(segmentKey.girderIndex) << _T(" Segment ") << LABEL_SEGMENT(segmentKey.segmentIndex) 
            << _T(" are located outside of the bearings. You can fix this by increasing the segment length, or")
            << _T(" by changing the harping point location in the girder library entry.")<<std::endl;
      }

      pgsBridgeDescriptionStatusItem* pStatusItem = 
         new pgsBridgeDescriptionStatusItem(m_StatusGroupID,m_scidBridgeDescriptionError,pgsBridgeDescriptionStatusItem::General,os.str().c_str());

      pStatusCenter->Add(pStatusItem);

      os << _T("See Status Center for Details");
      THROW_UNWIND(os.str().c_str(),XREASON_NEGATIVE_GIRDER_LENGTH);
   }

#pragma Reminder("UPDATE: don't need HP POI if they don't fall between bearings")

//   std::vector<EventIndexType> events;
//   events.push_back(pgsTypes::CastingYard);
//   events.push_back(pgsTypes::Lifting);
//   events.push_back(pgsTypes::Hauling);
//
//   // Add hp poi to events that are at the bridge site if the harp points
//   // are between the support locations
//   Float64 left_end, right_end, length;
//   girder->get_LeftEndDistance(&left_end);
//   girder->get_RightEndDistance(&right_end);
//   girder->get_GirderLength(&length);
//   if ( left_end < hp1 && hp2 < (length-right_end) )
//   {
//      events.push_back(pgsTypes::GirderPlacement);
//      events.push_back(pgsTypes::TemporaryStrandRemoval);
//      events.push_back(pgsTypes::BridgeSite1);
//      events.push_back(pgsTypes::BridgeSite2);
//      events.push_back(pgsTypes::BridgeSite3);
//   }
//   else
//   {
//#pragma Reminder("UPDATE: assuming precast girder bridge")
//      SpanIndexType spanIdx = segmentKey.groupIndex;
//      GirderIndexType gdrIdx = segmentKey.girderIndex;
//      ATLASSERT(segmentKey.segmentIndex == 0);
//
//#pragma Reminder("UPDATE: Status item need labels that work for both regular and spliced girders")
//
//      // harp points are outside of the support location
//      GET_IFACE(IEAFStatusCenter,pStatusCenter);
//      std::_tostringstream os;
//      os << _T("The harping points for Girder ") << LABEL_GIRDER(gdrIdx) << _T(" in Span ") << LABEL_SPAN(spanIdx) 
//         << _T(" are located outside of the bearings. You can fix this by increasing the girder length, or")
//         << _T(" by changing the harping point location in the girder library entry.")<<std::endl;
//
//      pgsBridgeDescriptionStatusItem* pStatusItem = 
//         new pgsBridgeDescriptionStatusItem(m_StatusGroupID,m_scidBridgeDescriptionError,pgsBridgeDescriptionStatusItem::General,os.str().c_str());
//
//      pStatusCenter->Add(pStatusItem);
//
//      os << _T("See Status Center for Details");
//      THROW_UNWIND(os.str().c_str(),XREASON_NEGATIVE_GIRDER_LENGTH);
//   }

   m_PoiMgr.AddPointOfInterest( poiHP1 );
   m_PoiMgr.AddPointOfInterest( poiHP2 );


   // add POI on either side of the harping point. Because the vertical component of prestress, Vp, is zero
   // on one side of the harp point and Vp on the other side there is a jump in shear capacity. Add these
   // poi to pick up the jump.
   poiHP1.Offset( 0.0015);
   poiHP2.Offset(-0.0015);

   poiHP1.SetAttributes(0);
   poiHP2.SetAttributes(0);
   
   m_PoiMgr.AddPointOfInterest( poiHP1 );
   m_PoiMgr.AddPointOfInterest( poiHP2 );
}

void CBridgeAgentImp::LayoutPrestressTransferAndDebondPoi(const CSegmentKey& segmentKey,Float64 segmentOffset)
{
   PoiAttributeType attrib_debond = POI_DEBOND;
   PoiAttributeType attrib_xfer   = POI_PSXFER;

   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CTimelineManager* pTimelineMgr = pIBridgeDesc->GetTimelineManager();

   SegmentIDType segID = pIBridgeDesc->GetSegmentID(segmentKey);

   // remove any current ps-xfer and debond pois
   std::vector<pgsPointOfInterest> poi;
   m_PoiMgr.GetPointsOfInterest(segmentKey,POI_DEBOND | POI_PSXFER,POIMGR_OR,&poi);
   std::vector<pgsPointOfInterest>::iterator iter(poi.begin());
   std::vector<pgsPointOfInterest>::iterator iterEnd(poi.end());
   for ( ; iter != iterEnd; iter++ )
   {
      m_PoiMgr.RemovePointOfInterest(*iter);
   }

#if defined _DEBUG
   m_PoiMgr.GetPointsOfInterest(segmentKey,POI_DEBOND | POI_PSXFER,POIMGR_OR,&poi);
   ATLASSERT(poi.size() == 0);
#endif

   // Add POIs at the prestress transfer length from the ends of the girder
   GET_IFACE(IPretensionForce,pPrestress);
   Float64 xfer_length = pPrestress->GetXferLength(segmentKey,pgsTypes::Permanent);
   
   Float64 left_end_dist = GetSegmentStartEndDistance(segmentKey);
   Float64 left_brg_offset = GetSegmentStartBearingOffset(segmentKey);
   Float64 segment_length = GetSegmentLength(segmentKey);
   Float64 start_offset = left_brg_offset - left_end_dist;

   CSegmentKey firstSegmentKey(segmentKey);
   firstSegmentKey.segmentIndex = 0;
   Float64 first_segment_start_offset = GetSegmentStartBearingOffset(firstSegmentKey) - GetSegmentStartEndDistance(firstSegmentKey);

   Float64 d1 = xfer_length;
   Float64 d2 = segment_length - xfer_length;

   Float64 X = d1;
   Float64 Xs = X + start_offset;
   Float64 Xgp = segmentOffset + Xs;
   Float64 Xg = Xgp - first_segment_start_offset;
   pgsPointOfInterest poiXfer1(segmentKey,X,Xs,Xg,Xgp,attrib_xfer);
   m_PoiMgr.AddPointOfInterest( poiXfer1 );

   X = d2;
   Xs = X + start_offset;
   Xgp = segmentOffset + Xs;
   Xg = Xgp - first_segment_start_offset;
   pgsPointOfInterest poiXfer2(segmentKey,X,Xs,Xg,Xgp,attrib_xfer);
   m_PoiMgr.AddPointOfInterest( poiXfer2 );

   ////////////////////////////////////////////////////////////////
   // debonded strands
   ////////////////////////////////////////////////////////////////

   GET_IFACE(ISegmentData,pSegmentData);
   const CStrandData* pStrands = pSegmentData->GetStrandData(segmentKey);

   for ( Uint16 i = 0; i < 3; i++ )
   {
      std::vector<CDebondData>::const_iterator iter;
      for ( iter = pStrands->Debond[i].begin(); iter != pStrands->Debond[i].end(); iter++ )
      {
         const CDebondData& debond_info = *iter;

         d1 = debond_info.Length1;
         d2 = d1 + xfer_length;

         // only add POI if debond and transfer point are on the girder
         if ( d1 < segment_length && d2 < segment_length )
         {
            X = d1;
            Xs = X + start_offset;
            Xgp = segmentOffset + Xs;
            Xg = Xgp - first_segment_start_offset;
            pgsPointOfInterest poiDBD(segmentKey,X,Xs,Xg,Xgp,attrib_debond);
            m_PoiMgr.AddPointOfInterest( poiDBD );

            X = d2;
            Xs = X + start_offset;
            Xgp = segmentOffset + Xs;
            Xg = Xgp - first_segment_start_offset;
            pgsPointOfInterest poiXFR(segmentKey,X,Xs,Xg,Xgp,attrib_xfer);
            m_PoiMgr.AddPointOfInterest( poiXFR );
         }

         d1 = segment_length - debond_info.Length2;
         d2 = d1 - segment_length;
         // only add POI if debond and transfer point are on the girder
         if ( 0 < d1 && 0 < d2 )
         {
            X = d1;
            Xs = X + start_offset;
            Xgp = segmentOffset + Xs;
            Xg = Xgp - first_segment_start_offset;
            pgsPointOfInterest poiDBD(segmentKey,X,Xs,Xg,Xgp,attrib_debond);
            m_PoiMgr.AddPointOfInterest( poiDBD );

            X = d2;
            Xs = X + start_offset;
            Xgp = segmentOffset + Xs;
            Xg = Xgp - first_segment_start_offset;
            pgsPointOfInterest poiXFR(segmentKey,X,Xs,Xg,Xgp,attrib_xfer);
            m_PoiMgr.AddPointOfInterest( poiXFR );
         }
      }
   }
}

void CBridgeAgentImp::LayoutPoiForDiaphragmLoads(const CSegmentKey& segmentKey,Float64 segmentOffset)
{
   // we want to capture "jumps" due to diaphragm loads in the graphical displays
   Float64 left_end_dist = GetSegmentStartEndDistance(segmentKey);
   Float64 left_brg_offset = GetSegmentStartBearingOffset(segmentKey);
   Float64 start_offset = left_brg_offset - left_end_dist;

   CSegmentKey firstSegmentKey(segmentKey);
   firstSegmentKey.segmentIndex = 0;
   Float64 first_segment_start_offset = GetSegmentStartBearingOffset(firstSegmentKey) - GetSegmentStartEndDistance(firstSegmentKey);

   // layout for diaphragms that are built in the casting yard
   std::vector<IntermedateDiaphragm> pcDiaphragms( GetIntermediateDiaphragms(pgsTypes::dtPrecast,segmentKey) );
   std::vector<IntermedateDiaphragm>::iterator iter(pcDiaphragms.begin());
   std::vector<IntermedateDiaphragm>::iterator end(pcDiaphragms.end());
   for ( ; iter != end; iter++ )
   {
      IntermedateDiaphragm& diaphragm = *iter;

      Float64 Xpoi = diaphragm.Location; // location in POI coordinates
      Float64 Xs = start_offset  + Xpoi; // location in segment coordinates
      Float64 Xgp = segmentOffset + Xs;  // location in girder path coordinates
      Float64 Xg = Xgp - first_segment_start_offset;   // location in girder coordinates
      pgsPointOfInterest poi( segmentKey, Xpoi, Xs, Xg, Xgp, POI_CONCLOAD);
      m_PoiMgr.AddPointOfInterest( poi );
   }

   // get loads for bridge site 
   std::vector<IntermedateDiaphragm> cipDiaphragms( GetIntermediateDiaphragms(pgsTypes::dtCastInPlace,segmentKey) );
   iter = cipDiaphragms.begin();
   end  = cipDiaphragms.end();
   for ( ; iter != end; iter++ )
   {
      IntermedateDiaphragm& diaphragm = *iter;

      Float64 Xpoi = diaphragm.Location;
      Float64 Xs = start_offset + Xpoi;
      Float64 Xgp = segmentOffset + Xs;
      Float64 Xg = Xgp - first_segment_start_offset;

      pgsPointOfInterest poi( segmentKey, Xpoi, Xs, Xg, Xgp, POI_CONCLOAD );
      m_PoiMgr.AddPointOfInterest( poi );
   }
}

void CBridgeAgentImp::LayoutPoiForShear(const CSegmentKey& segmentKey,Float64 segmentOffset)
{
   // Layout POI at H, 1.5H, 2.5H, and FaceOfSupport from CL-Brg for piers that occur at the
   // ends of a segment (See LayoutPoiForPier for shear POIs that occur near
   // intermediate supports for segments that span over a pier)

   Float64 left_end_dist  = GetSegmentStartEndDistance(segmentKey);
   Float64 right_end_dist = GetSegmentEndEndDistance(segmentKey);
   Float64 left_brg_offset = GetSegmentStartBearingOffset(segmentKey);
   Float64 start_offset = left_brg_offset - left_end_dist;
   Float64 segment_length = GetSegmentLength(segmentKey);

   CSegmentKey firstSegmentKey(segmentKey);
   firstSegmentKey.segmentIndex = 0;
   Float64 first_segment_start_offset = GetSegmentStartBearingOffset(firstSegmentKey) - GetSegmentStartEndDistance(firstSegmentKey);

   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CPrecastSegmentData* pSegment = pIBridgeDesc->GetPrecastSegmentData(segmentKey);
   const CPierData2* pPier;
   const CTemporarySupportData* pTS;
   pSegment->GetStartSupport(&pPier,&pTS);

   if ( pPier )
   {
      // this is a pier at the start of this segment
      Float64 dist_from_start;
      GetPierLocation(pPier->GetIndex(),segmentKey,&dist_from_start);
      Float64 Hg = GetHeight(pgsPointOfInterest(segmentKey,dist_from_start));

#pragma Reminder("UPDATE: support width doesn't have to be symmetrical")
      // using support_width/2 below... need to worry about left and right width of support width
      Float64 support_width  = GetSegmentStartSupportWidth(segmentKey);

      // If "H" from the end of the girder is at the point of bearing
      // make sure there isn't any "noise" in the data
      if ( IsEqual(Hg,left_end_dist) )
         Hg = left_end_dist;

      Float64 X, Xs, Xg, Xgp;
      X = left_end_dist + support_width/2 + Hg;
      Xs = X + start_offset;
      Xgp = segmentOffset + Xs;
      Xg = Xgp - first_segment_start_offset;
      pgsPointOfInterest poi_h( segmentKey, X, Xs, Xg, Xgp, POI_H | POI_ERECTED_SEGMENT);
      m_PoiMgr.AddPointOfInterest(poi_h);

      X = left_end_dist + support_width/2 + 1.5*Hg;
      Xs = X + start_offset;
      Xgp = segmentOffset + Xs;
      Xg = Xgp - first_segment_start_offset;
      pgsPointOfInterest poi_15h( segmentKey, X, Xs, Xg, Xgp, POI_15H | POI_ERECTED_SEGMENT);
      m_PoiMgr.AddPointOfInterest(poi_15h);

      X = left_end_dist + support_width/2 + 2.5*Hg;
      Xs = X + start_offset;
      Xgp = segmentOffset + Xs;
      Xg = Xgp - first_segment_start_offset;
      pgsPointOfInterest poi_25h( segmentKey, X, Xs, Xg, Xgp);
      m_PoiMgr.AddPointOfInterest(poi_25h);

      X = left_end_dist + support_width/2;
      Xs = X + start_offset;
      Xgp = segmentOffset + Xs;
      Xg = Xgp - first_segment_start_offset;
      pgsPointOfInterest poi_fos( segmentKey, X, Xs, Xg, Xgp, POI_FACEOFSUPPORT);
      m_PoiMgr.AddPointOfInterest(poi_fos);

#pragma Reminder("REVIEW: remove this POI after all regression tests results match older versions of PGSuper")
      // Version 2.x of PGSuper had a POI at H from the end of the girder
      // This POI isn't necessary but it is evaluated in the regression tests.
      // The POI is created here for the sole purpose regression testing.
      Hg = GetHeight(pgsPointOfInterest(segmentKey,0.0));

      if ( IsEqual(Hg,left_end_dist) )
         Hg = left_end_dist;

      m_PoiMgr.AddPointOfInterest(pgsPointOfInterest(segmentKey,Hg,POI_H | POI_RELEASED_SEGMENT));
   }

   pSegment->GetEndSupport(&pPier,&pTS);
   if ( pPier )
   {
      // this is a pier at the end of this segment
      Float64 dist_from_start;
      GetPierLocation(pPier->GetIndex(),segmentKey,&dist_from_start);
      Float64 Hg = GetHeight(pgsPointOfInterest(segmentKey,dist_from_start));

      Float64 support_width = GetSegmentEndSupportWidth(segmentKey);
      Float64 end_size = GetSegmentEndEndDistance(segmentKey);

      // If "H" from the end of the girder is at the point of bearing
      // make sure there isn't any "noise" in the data
      if ( IsEqual(Hg,end_size) )
         Hg = end_size;

      Float64 X, Xs, Xg, Xgp;
      X = segment_length - (end_size + support_width/2 + Hg);
      Xs = X + start_offset;
      Xgp = Xs + segmentOffset;
      Xg = Xgp - first_segment_start_offset;
      pgsPointOfInterest poi_h( segmentKey, X, Xs, Xg, Xgp, POI_H | POI_ERECTED_SEGMENT);
      m_PoiMgr.AddPointOfInterest(poi_h);

      X = segment_length - (end_size + support_width/2 + 1.5*Hg);
      Xs = X + start_offset;
      Xgp = Xs + segmentOffset;
      Xg = Xgp - first_segment_start_offset;
      pgsPointOfInterest poi_15h( segmentKey, X, Xs, Xg, Xgp, POI_15H | POI_ERECTED_SEGMENT);
      m_PoiMgr.AddPointOfInterest(poi_15h);

      X = segment_length - (end_size + support_width/2 + 2.5*Hg);
      Xs = X + start_offset;
      Xgp = Xs + segmentOffset;
      Xg = Xgp - first_segment_start_offset;
      pgsPointOfInterest poi_25h( segmentKey, X, Xs, Xg, Xgp);
      m_PoiMgr.AddPointOfInterest(poi_25h);

      X = segment_length - (end_size + support_width/2);
      Xs = X + start_offset;
      Xgp = Xs + segmentOffset;
      Xg = Xgp - first_segment_start_offset;
      pgsPointOfInterest poi_fos( segmentKey, X, Xs, Xg, Xgp, POI_FACEOFSUPPORT);
      m_PoiMgr.AddPointOfInterest(poi_fos);

#pragma Reminder("REVIEW: remove this POI after all regression tests results match older versions of PGSuper")
      // Version 2.x of PGSuper had a POI at H from the end of the girder
      // This POI isn't necessary but it is evaluated in the regression tests.
      // The POI is created here for the sole purpose regression testing.
      Hg = GetHeight(pgsPointOfInterest(segmentKey,segment_length));

      if ( IsEqual(Hg,left_end_dist) )
         Hg = left_end_dist;

      m_PoiMgr.AddPointOfInterest(pgsPointOfInterest(segmentKey,segment_length-Hg,POI_H | POI_RELEASED_SEGMENT));
   }

   // POI's at stirrup zone boundaries
   Float64 right_support_loc = segment_length - right_end_dist;
   Float64 midLen = segment_length/2.0;

   ZoneIndexType nZones = GetNumPrimaryZones(segmentKey);
   for (ZoneIndexType zoneIdx = 1; zoneIdx < nZones; zoneIdx++) // note that count starts at one
   {
      Float64 zStart, zEnd;
      GetPrimaryZoneBounds(segmentKey, zoneIdx, &zStart, &zEnd);

      // Nudge poi toward mid-span as this is where smaller Av/s will typically lie
      zStart += (zStart < midLen ? 0.001 : -0.001);

      if (left_end_dist < zStart && zStart < right_support_loc)
      {
#pragma Reminder("UPDATE: add other POI locations") 
         // there is enough information for Xs, Xgp, and Xg

         pgsPointOfInterest poi(segmentKey, zStart);
         m_PoiMgr.AddPointOfInterest(poi);
      }
   }

}

void CBridgeAgentImp::LayoutPoiForSlabBarCutoffs(const CSegmentKey& segmentKey)
{
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CDeckRebarData& rebarData = pBridgeDesc->GetDeckDescription()->DeckRebarData;

   PierIndexType prev_pier_index = GetGenericBridgePierIndex(segmentKey,pgsTypes::metStart);
   PierIndexType next_pier_index = GetGenericBridgePierIndex(segmentKey,pgsTypes::metEnd);

   Float64 gdr_length = GetSegmentLength(segmentKey);

   Float64 left_end_size  = GetSegmentStartEndDistance(segmentKey);
   Float64 right_end_size = GetSegmentEndEndDistance(segmentKey);

   Float64 left_brg_offset  = GetSegmentStartBearingOffset(segmentKey);
   Float64 right_brg_offset = GetSegmentEndBearingOffset(segmentKey);

   std::vector<CDeckRebarData::NegMomentRebarData>::const_iterator iter;
   for ( iter = rebarData.NegMomentRebar.begin(); iter != rebarData.NegMomentRebar.end(); iter++ )
   {
      const CDeckRebarData::NegMomentRebarData& nmRebarData = *iter;

      if ( nmRebarData.PierIdx == prev_pier_index )
      {
         Float64 girder_start_dist = left_brg_offset - left_end_size;
         if ( girder_start_dist <= nmRebarData.RightCutoff )
         {
            Float64 dist_from_start = nmRebarData.RightCutoff - girder_start_dist;
            if ( gdr_length < dist_from_start )
               dist_from_start = gdr_length - 0.0015;


            m_PoiMgr.AddPointOfInterest( pgsPointOfInterest(segmentKey,dist_from_start,POI_DECKBARCUTOFF) );
            m_PoiMgr.AddPointOfInterest( pgsPointOfInterest(segmentKey,dist_from_start+0.0015) );
         }
      }
      else if ( nmRebarData.PierIdx == next_pier_index )
      {
         Float64 girder_end_dist = right_brg_offset - right_end_size;
         if ( girder_end_dist <= nmRebarData.LeftCutoff )
         {
            Float64 dist_from_start = gdr_length - (nmRebarData.LeftCutoff - girder_end_dist);
            if ( dist_from_start < 0 )
               dist_from_start = 0.0015;

            m_PoiMgr.AddPointOfInterest( pgsPointOfInterest(segmentKey,dist_from_start-0.0015) );
            m_PoiMgr.AddPointOfInterest( pgsPointOfInterest(segmentKey,dist_from_start,POI_DECKBARCUTOFF) );
         }
      }
   }
}

void CBridgeAgentImp::LayoutPoiForSegmentBarCutoffs(const CSegmentKey& segmentKey,Float64 segmentOffset)
{
   CComPtr<IPrecastGirder> girder;
   GetGirder(segmentKey,&girder);

   Float64 segment_length = GetSegmentLength(segmentKey);
   Float64 left_brg_loc  = GetSegmentStartEndDistance(segmentKey);
   Float64 right_brg_loc = segment_length - GetSegmentEndEndDistance(segmentKey);

   GET_IFACE(IMaterials,pMaterials);
   Float64 fc = pMaterials->GetSegmentFc28(segmentKey);
   pgsTypes::ConcreteType concType = pMaterials->GetSegmentConcreteType(segmentKey);
   bool hasFct = pMaterials->DoesSegmentConcreteHaveAggSplittingStrength(segmentKey);
   Float64 Fct = hasFct ? pMaterials->GetSegmentConcreteAggSplittingStrength(segmentKey) : 0.0;

   PoiAttributeType cut_attribute = POI_BARCUTOFF;
   PoiAttributeType dev_attribute = POI_BARDEVELOP;

   CComPtr<IRebarLayout> rebar_layout;
   girder->get_RebarLayout(&rebar_layout);

   CollectionIndexType nRebarLayoutItems;
   rebar_layout->get_Count(&nRebarLayoutItems);
   for (CollectionIndexType rebarLayoutItemIdx = 0; rebarLayoutItemIdx < nRebarLayoutItems; rebarLayoutItemIdx++)
   {
      CComPtr<IRebarLayoutItem> rebarLayoutItem;
      rebar_layout->get_Item(rebarLayoutItemIdx, &rebarLayoutItem);

      Float64 startLoc, barLength;
      rebarLayoutItem->get_Start(&startLoc);
      rebarLayoutItem->get_Length(&barLength);
      Float64 endLoc = startLoc + barLength;

      if (segment_length < endLoc)
      {
         ATLASSERT(0); // probably should never happen
         endLoc = segment_length;
      }

      // Add pois at cutoffs if they are within bearing locations
      if (left_brg_loc < startLoc && startLoc < right_brg_loc)
         m_PoiMgr.AddPointOfInterest( pgsPointOfInterest(segmentKey, startLoc, cut_attribute) );

      if (left_brg_loc < endLoc && endLoc < right_brg_loc)
         m_PoiMgr.AddPointOfInterest( pgsPointOfInterest(segmentKey, endLoc, cut_attribute) );

      // we only create one pattern per layout
      CollectionIndexType nRebarPatterns;
      rebarLayoutItem->get_Count(&nRebarPatterns);
      ATLASSERT(nRebarPatterns==1);
      if (0 < nRebarPatterns)
      {
         CComPtr<IRebarPattern> rebarPattern;
         rebarLayoutItem->get_Item(0, &rebarPattern);

         CComPtr<IRebar> rebar;
         rebarPattern->get_Rebar(&rebar);

         if (rebar)
         {
            // Get development length and add poi only if dev length is shorter than 1/2 rebar length
            REBARDEVLENGTHDETAILS devDetails = GetRebarDevelopmentLengthDetails(rebar, concType, fc, hasFct, Fct);
            Float64 ldb = devDetails.ldb;
            if (ldb < barLength/2.0)
            {
               startLoc += ldb;
               endLoc -= ldb;

               if (left_brg_loc < startLoc && startLoc < right_brg_loc)
                  m_PoiMgr.AddPointOfInterest( pgsPointOfInterest(segmentKey, startLoc, dev_attribute) );

               if (left_brg_loc < endLoc && endLoc < right_brg_loc)
                  m_PoiMgr.AddPointOfInterest( pgsPointOfInterest(segmentKey, endLoc,   dev_attribute) );
            }
         }
         else
         {
            ATLASSERT(0);
         }
      }
   }
}

void CBridgeAgentImp::LayoutPoiForHandling(const CSegmentKey& segmentKey)
{
   GET_IFACE(IGirderLiftingSpecCriteria,pGirderLiftingSpecCriteria);
   if (pGirderLiftingSpecCriteria->IsLiftingAnalysisEnabled())
   {
      LayoutLiftingPoi(segmentKey,10); // puts poi at 10th points
   }

   GET_IFACE(IGirderHaulingSpecCriteria,pGirderHaulingSpecCriteria);
   if ( pGirderHaulingSpecCriteria->IsHaulingAnalysisEnabled() )
   {
      LayoutHaulingPoi(segmentKey,10);
   }
}

void CBridgeAgentImp::LayoutPoiForSectionChanges(const CSegmentKey& segmentKey)
{
   const GirderLibraryEntry* pGirderEntry = GetGirderLibraryEntry(segmentKey);
   CComPtr<IBeamFactory> beamFactory;
   pGirderEntry->GetBeamFactory(&beamFactory);

   beamFactory->LayoutSectionChangePointsOfInterest(m_pBroker,segmentKey,&m_PoiMgr);
}

void CBridgeAgentImp::LayoutPoiForPiers(const CSegmentKey& segmentKey)
{
   // puts a POI at piers that are located between the ends of a segment
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CGirderGroupData*    pGroup      = pBridgeDesc->GetGirderGroup(segmentKey.groupIndex);
   const CSplicedGirderData*  pGirder     = pGroup->GetGirder(segmentKey.girderIndex);
   const CPrecastSegmentData* pSegment    = pGirder->GetSegment(segmentKey.segmentIndex);

   const CSpanData2* pStartSpan = pSegment->GetSpan(pgsTypes::metStart);
   const CSpanData2* pEndSpan   = pSegment->GetSpan(pgsTypes::metEnd);

   // collect all the piers that are between the ends of this segment
   std::vector<const CPierData2*> piers;
   const CPierData2* pPier = pStartSpan->GetNextPier();
   while ( pPier != pEndSpan->GetNextPier() )
   {
      piers.push_back(pPier);

      pPier = pPier->GetNextSpan()->GetNextPier();
   }

   if ( 0 < piers.size() )
   {
      std::vector<const CPierData2*>::iterator iter(piers.begin());
      std::vector<const CPierData2*>::iterator iterEnd(piers.end());
      for ( ; iter != iterEnd; iter++ )
      {
         const CPierData2* pPier = *iter;

         Float64 dist_from_start;
         GetPierLocation(pPier->GetIndex(),segmentKey,&dist_from_start);
         pgsPointOfInterest poi(segmentKey,dist_from_start,POI_INTERMEDIATE_PIER);
         m_PoiMgr.AddPointOfInterest(poi);

         // POI at h and 1.5h from cl-brg (also at 2.5h for purposes of computing critical section)
         Float64 Hg = GetHeight(poi);
         pgsPointOfInterest left_H(segmentKey,dist_from_start - Hg,POI_H | POI_ERECTED_SEGMENT);
         m_PoiMgr.AddPointOfInterest(left_H);

         pgsPointOfInterest left_15H(segmentKey,dist_from_start - 1.5*Hg,POI_15H | POI_ERECTED_SEGMENT);
         m_PoiMgr.AddPointOfInterest(left_15H);

         pgsPointOfInterest left_25H(segmentKey,dist_from_start - 2.5*Hg);
         m_PoiMgr.AddPointOfInterest(left_25H);

         pgsPointOfInterest right_H(segmentKey,dist_from_start + Hg,POI_H | POI_ERECTED_SEGMENT);
         m_PoiMgr.AddPointOfInterest(right_H);

         pgsPointOfInterest right_15H(segmentKey,dist_from_start + 1.5*Hg,POI_15H | POI_ERECTED_SEGMENT);
         m_PoiMgr.AddPointOfInterest(right_15H);

         pgsPointOfInterest right_25H(segmentKey,dist_from_start + 2.5*Hg);
         m_PoiMgr.AddPointOfInterest(right_25H);



         // Add POIs for face of support
         Float64 left_support_width  = pPier->GetSupportWidth(pgsTypes::Back);
         Float64 right_support_width = pPier->GetSupportWidth(pgsTypes::Ahead);

         pgsPointOfInterest leftFOS(segmentKey,dist_from_start - left_support_width,POI_FACEOFSUPPORT);
         m_PoiMgr.AddPointOfInterest(leftFOS);

         if ( !IsZero(left_support_width) && !IsZero(right_support_width) )
         {
            pgsPointOfInterest rightFOS(segmentKey,dist_from_start + right_support_width,POI_FACEOFSUPPORT);
            m_PoiMgr.AddPointOfInterest(rightFOS);
         }
      } // next pier
   }

   // add POI at centerline of pier between groups
   if ( pSegment->GetRightClosure() == NULL )
   {
      const CPierData2* pPier;
      const CTemporarySupportData* pTS;
      pSegment->GetEndSupport(&pPier,&pTS);
      ATLASSERT(pPier != NULL);
      ATLASSERT(pTS == NULL);

      if ( pPier->GetNextSpan() )
      {
         Float64 Xs; // pier is in segment coordinates
         GetPierLocation(pPier->GetIndex(),segmentKey,&Xs);
         Float64 Xpoi = ConvertSegmentCoordinateToPoiCoordinate(segmentKey,Xs);
         pgsPointOfInterest poi(segmentKey,Xpoi,POI_PIER);
         m_PoiMgr.AddPointOfInterest(poi);
      }
   }
}

void CBridgeAgentImp::LayoutPoiForTemporarySupports(const CSegmentKey& segmentKey)
{
   // puts a POI at temporary supports that are located between the ends of a segment
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CGirderGroupData*    pGroup      = pBridgeDesc->GetGirderGroup(segmentKey.groupIndex);
   const CSplicedGirderData*  pGirder     = pGroup->GetGirder(segmentKey.girderIndex);
   const CPrecastSegmentData* pSegment    = pGirder->GetSegment(segmentKey.segmentIndex);

   const CSpanData2* pStartSpan = pSegment->GetSpan(pgsTypes::metStart);
   const CSpanData2* pEndSpan   = pSegment->GetSpan(pgsTypes::metEnd);

   std::vector<const CTemporarySupportData*> tempSupports(pStartSpan->GetTemporarySupports());
   std::vector<const CTemporarySupportData*> endTempSupports(pEndSpan->GetTemporarySupports());
   tempSupports.insert(tempSupports.begin(),endTempSupports.begin(),endTempSupports.end());

   if ( tempSupports.size() == 0 )
      return; // no temporary supports

   Float64 segment_start_station, segment_end_station;
   pSegment->GetStations(segment_start_station,segment_end_station);
   std::vector<const CTemporarySupportData*>::iterator iter(tempSupports.begin());
   std::vector<const CTemporarySupportData*>::iterator iterEnd(tempSupports.end());
   for ( ; iter != iterEnd; iter++ )
   {
      const CTemporarySupportData* pTS = *iter;
      Float64 ts_station = pTS->GetStation();
      if ( ::IsEqual(segment_start_station,ts_station) || ::IsEqual(segment_end_station,ts_station) )
         continue; // temporary support is at end of segment... we are creating POIs for intermediate temporary supports

      if ( ::InRange(segment_start_station,ts_station,segment_end_station) )
      {
         Float64 Xs;
         VERIFY(GetTemporarySupportLocation(pTS->GetIndex(),segmentKey,&Xs));
         Float64 Xpoi = ConvertSegmentCoordinateToPoiCoordinate(segmentKey,Xs);
         pgsPointOfInterest poi(segmentKey,Xpoi,POI_TEMPSUPPORT);

         m_PoiMgr.AddPointOfInterest(poi);
      }
   } // next temporary support
}

//-----------------------------------------------------------------------------
void CBridgeAgentImp::LayoutLiftingPoi(const CSegmentKey& segmentKey,Uint16 nPnts)
{
   // lifting
   LayoutHandlingPoi(GetLiftSegmentInterval(segmentKey),segmentKey,
                     nPnts, 
                     0,
                     &m_LiftingPoiMgr); 
                     
}

//-----------------------------------------------------------------------------
void CBridgeAgentImp::LayoutHaulingPoi(const CSegmentKey& segmentKey,Uint16 nPnts)
{
   // hauling
   LayoutHandlingPoi(GetHaulSegmentInterval(segmentKey),segmentKey,
                     nPnts, 
                     0,
                     &m_HaulingPoiMgr);
}

//-----------------------------------------------------------------------------
void CBridgeAgentImp::LayoutHandlingPoi(IntervalIndexType intervalIdx,const CSegmentKey& segmentKey,
                                        Uint16 nPnts, 
                                        PoiAttributeType attrib,
                                        pgsPoiMgr* pPoiMgr)
{
   ATLASSERT(intervalIdx == GetLiftSegmentInterval(segmentKey) || intervalIdx == GetHaulSegmentInterval(segmentKey));
   PoiAttributeType supportAttribute, poiReference;
   Float64 leftOverhang, rightOverhang;
   if (intervalIdx == GetLiftSegmentInterval(segmentKey))
   {
      GET_IFACE(IGirderLifting, pGirderLifting);
      leftOverhang  = pGirderLifting->GetLeftLiftingLoopLocation(segmentKey);
      rightOverhang = pGirderLifting->GetRightLiftingLoopLocation(segmentKey);

      supportAttribute = POI_PICKPOINT;
      poiReference = POI_LIFT_SEGMENT;
   }
   else if (intervalIdx == GetHaulSegmentInterval(segmentKey))
   {
      GET_IFACE(IGirderHauling, pGirderHauling);
      leftOverhang  = pGirderHauling->GetTrailingOverhang(segmentKey);
      rightOverhang = pGirderHauling->GetLeadingOverhang(segmentKey);

      supportAttribute = POI_BUNKPOINT;
      poiReference = POI_HAUL_SEGMENT;
   }
   else
   {
      ATLASSERT(false); // you sent in the wrong interval index
   }

   LayoutHandlingPoi(segmentKey,nPnts,leftOverhang,rightOverhang,attrib,supportAttribute,poiReference,pPoiMgr);


#if defined _DEBUG
   std::vector<pgsPointOfInterest> vPoi;
   pPoiMgr->GetPointsOfInterest(segmentKey,poiReference | supportAttribute,POIMGR_AND,&vPoi);
   ATLASSERT(vPoi.size() == 2);
#endif
}

//-----------------------------------------------------------------------------
void CBridgeAgentImp::LayoutHandlingPoi(const CSegmentKey& segmentKey,
                                        Uint16 nPnts, 
                                        Float64 leftOverhang, 
                                        Float64 rightOverhang, 
                                        PoiAttributeType attrib, 
                                        PoiAttributeType supportAttribute,
                                        PoiAttributeType poiReference,
                                        pgsPoiMgr* pPoiMgr)
{
   Float64 segment_length = GetSegmentLength(segmentKey);
   Float64 span_length = segment_length - leftOverhang - rightOverhang;
   ATLASSERT(0.0 < span_length); // should be checked in input

   // add poi at support locations
   pPoiMgr->AddPointOfInterest( pgsPointOfInterest(segmentKey,leftOverhang,attrib | supportAttribute) );
   pPoiMgr->AddPointOfInterest( pgsPointOfInterest(segmentKey,segment_length - rightOverhang,attrib | supportAttribute) );

   // add poi at ends of girder
   pPoiMgr->AddPointOfInterest( pgsPointOfInterest(segmentKey,0,attrib) );
   pPoiMgr->AddPointOfInterest( pgsPointOfInterest(segmentKey,segment_length,attrib) );

   // add harping point and PSXfer POI
   std::vector<pgsPointOfInterest> vPoi;
   m_PoiMgr.GetPointsOfInterest(segmentKey,POI_HARPINGPOINT | POI_PSXFER,POIMGR_OR,&vPoi);
   std::vector<pgsPointOfInterest>::iterator iter(vPoi.begin());
   std::vector<pgsPointOfInterest>::iterator end(vPoi.end());
   for ( ; iter != end; iter++ )
   {
      pPoiMgr->AddPointOfInterest(*iter);
   }

   // nth point POI between overhang support points
   const Float64 toler = +1.0e-6;
   for ( Uint16 i = 0; i < nPnts+1; i++ )
   {
      Float64 dist = leftOverhang + span_length * ((Float64)i / (Float64)nPnts);

      PoiAttributeType attribute = attrib;

      // Add a special attribute flag if this poi is at the mid-span
      if ( i == (nPnts-1)/2 + 1)
         attribute |= poiReference | POI_MIDSPAN;

      // Add a special attribute flag if poi is at a tenth point
      Uint16 tenthPoint = 0;
      Float64 val = Float64(i)/Float64(nPnts)+toler;
      Float64 modv = fmod(val, 0.1);
      if (IsZero(modv,2*toler) || modv==0.1)
      {
         tenthPoint = Uint16(10.*Float64(i)/Float64(nPnts) + 1);
      }
      
      pgsPointOfInterest poi(segmentKey,dist,attribute);
      poi.MakeTenthPoint(poiReference,tenthPoint);
      pPoiMgr->AddPointOfInterest( poi );
   }

}

/////////////////////////////////////////////////////////////////////////
// IAgent
//
STDMETHODIMP CBridgeAgentImp::SetBroker(IBroker* pBroker)
{
   AGENT_SET_BROKER(pBroker);
   return S_OK;
}

STDMETHODIMP CBridgeAgentImp::RegInterfaces()
{
   CComQIPtr<IBrokerInitEx2,&IID_IBrokerInitEx2> pBrokerInit(m_pBroker);
#pragma Reminder("OBSOLETE: remove obsolete code")
   pBrokerInit->RegInterface( IID_IRoadway,                       this );
   pBrokerInit->RegInterface( IID_IBridge,                        this );
   pBrokerInit->RegInterface( IID_IMaterials,                     this );
   pBrokerInit->RegInterface( IID_IStrandGeometry,                this );
   pBrokerInit->RegInterface( IID_ILongRebarGeometry,             this );
   pBrokerInit->RegInterface( IID_IStirrupGeometry,               this );
   //pBrokerInit->RegInterface( IID_IGirderPointOfInterest,         this ); // obsolete???
   pBrokerInit->RegInterface( IID_IPointOfInterest,        this );
   //pBrokerInit->RegInterface( IID_IClosurePourPointOfInterest,    this ); // obsolete???
   //pBrokerInit->RegInterface( IID_ISplicedGirderPointOfInterest,  this ); // obsolete???
   pBrokerInit->RegInterface( IID_ISectionProperties,              this );
   pBrokerInit->RegInterface( IID_IShapes,                        this );
   pBrokerInit->RegInterface( IID_IBarriers,                      this );
   pBrokerInit->RegInterface( IID_IGirder,                        this );
   pBrokerInit->RegInterface( IID_IGirderLiftingPointsOfInterest, this );
   pBrokerInit->RegInterface( IID_IGirderHaulingPointsOfInterest, this );
   pBrokerInit->RegInterface( IID_IUserDefinedLoads,              this );
   pBrokerInit->RegInterface( IID_ITempSupport,                   this );
   pBrokerInit->RegInterface( IID_IGirderSegment,                 this );
   pBrokerInit->RegInterface( IID_IClosurePour,                   this );
   pBrokerInit->RegInterface( IID_ISplicedGirder,                 this );
   pBrokerInit->RegInterface( IID_ITendonGeometry,                this );
   pBrokerInit->RegInterface( IID_IIntervals,                     this );

   return S_OK;
}

STDMETHODIMP CBridgeAgentImp::Init()
{
   CREATE_LOGFILE("BridgeAgent");
   AGENT_INIT; // this macro defines pStatusCenter
   m_LoadStatusGroupID = pStatusCenter->CreateStatusGroupID();

   // Register status callbacks that we want to use
   m_scidInformationalError       = pStatusCenter->RegisterCallback(new pgsInformationalStatusCallback(eafTypes::statusError)); 
   m_scidInformationalWarning     = pStatusCenter->RegisterCallback(new pgsInformationalStatusCallback(eafTypes::statusWarning)); 
   m_scidBridgeDescriptionError   = pStatusCenter->RegisterCallback(new pgsBridgeDescriptionStatusCallback(m_pBroker,eafTypes::statusError));
   m_scidBridgeDescriptionWarning = pStatusCenter->RegisterCallback(new pgsBridgeDescriptionStatusCallback(m_pBroker,eafTypes::statusWarning));
   m_scidAlignmentWarning         = pStatusCenter->RegisterCallback(new pgsAlignmentDescriptionStatusCallback(m_pBroker,eafTypes::statusWarning));
   m_scidAlignmentError           = pStatusCenter->RegisterCallback(new pgsAlignmentDescriptionStatusCallback(m_pBroker,eafTypes::statusError));
   m_scidGirderDescriptionWarning = pStatusCenter->RegisterCallback(new pgsGirderDescriptionStatusCallback(m_pBroker,eafTypes::statusWarning));
   m_scidPointLoadWarning         = pStatusCenter->RegisterCallback(new pgsPointLoadStatusCallback(m_pBroker,eafTypes::statusWarning));
   m_scidDistributedLoadWarning   = pStatusCenter->RegisterCallback(new pgsDistributedLoadStatusCallback(m_pBroker,eafTypes::statusWarning));
   m_scidMomentLoadWarning        = pStatusCenter->RegisterCallback(new pgsMomentLoadStatusCallback(m_pBroker,eafTypes::statusWarning));

   return AGENT_S_SECONDPASSINIT;
}

STDMETHODIMP CBridgeAgentImp::Init2()
{
   // Attach to connection points
   CComQIPtr<IBrokerInitEx2,&IID_IBrokerInitEx2> pBrokerInit(m_pBroker);
   CComPtr<IConnectionPoint> pCP;
   HRESULT hr = S_OK;

   // Connection point for the bridge description
   hr = pBrokerInit->FindConnectionPoint( IID_IBridgeDescriptionEventSink, &pCP );
   CHECK( SUCCEEDED(hr) );
   hr = pCP->Advise( GetUnknown(), &m_dwBridgeDescCookie );
   CHECK( SUCCEEDED(hr) );
   pCP.Release(); // Recycle the IConnectionPoint smart pointer so we can use it again.

   // Connection point for the specifications
   hr = pBrokerInit->FindConnectionPoint( IID_ISpecificationEventSink, &pCP );
   CHECK( SUCCEEDED(hr) );
   hr = pCP->Advise( GetUnknown(), &m_dwSpecificationCookie );
   CHECK( SUCCEEDED(hr) );
   pCP.Release(); // Recycle the IConnectionPoint smart pointer so we can use it again.

   hr = m_SectCutTool.CoCreateInstance(CLSID_SectionCutTool);
   if ( FAILED(hr) || m_SectCutTool == NULL )
      THROW_UNWIND(_T("GenericBridgeTools::SectionPropertyTool not created"),-1);


   m_ConcreteManager.Init(m_pBroker,m_StatusGroupID);

   return S_OK;
}

STDMETHODIMP CBridgeAgentImp::GetClassID(CLSID* pCLSID)
{
   *pCLSID = CLSID_BridgeAgent;
   return S_OK;
}

STDMETHODIMP CBridgeAgentImp::Reset()
{
   Invalidate( CLEAR_ALL );
   m_SectCutTool.Release();

   return S_OK;
}

STDMETHODIMP CBridgeAgentImp::ShutDown()
{
   //
   // Detach to connection points
   //
   CComQIPtr<IBrokerInitEx2,&IID_IBrokerInitEx2> pBrokerInit(m_pBroker);
   CComPtr<IConnectionPoint> pCP;
   HRESULT hr = S_OK;

   hr = pBrokerInit->FindConnectionPoint(IID_IBridgeDescriptionEventSink, &pCP );
   CHECK( SUCCEEDED(hr) );
   hr = pCP->Unadvise( m_dwBridgeDescCookie );
   CHECK( SUCCEEDED(hr) );
   pCP.Release(); // Recycle the connection point

   hr = pBrokerInit->FindConnectionPoint(IID_ISpecificationEventSink, &pCP );
   CHECK( SUCCEEDED(hr) );
   hr = pCP->Unadvise( m_dwSpecificationCookie );
   CHECK( SUCCEEDED(hr) );
   pCP.Release(); // Recycle the connection point

   CLOSE_LOGFILE;

   AGENT_CLEAR_INTERFACE_CACHE;

   return S_OK;
}

/////////////////////////////////////////////////////////////////////////
// IRoadway
//
Float64 CBridgeAgentImp::GetCrownPointOffset(Float64 station)
{
   VALIDATE( COGO_MODEL );

   CComPtr<IProfile> profile;
   GetProfile(&profile);

   Float64 offset;
   profile->CrownPointOffset(CComVariant(station),&offset);

   return offset;
}

Float64 CBridgeAgentImp::GetCrownSlope(Float64 station,Float64 offset)
{
   VALIDATE( COGO_MODEL );

   CComPtr<IProfile> profile;
   GetProfile(&profile);

   Float64 slope;
   profile->CrownSlope(CComVariant(station),offset,&slope);

   return slope;
}

void CBridgeAgentImp::GetCrownSlope(Float64 station,Float64* pLeftSlope,Float64* pRightSlope)
{
   VALIDATE( COGO_MODEL );

   CComPtr<IProfile> profile;
   GetProfile(&profile);

   profile->LeftCrownSlope( CComVariant(station),pLeftSlope);
   profile->RightCrownSlope(CComVariant(station),pRightSlope);
}

Float64 CBridgeAgentImp::GetProfileGrade(Float64 station)
{
   VALIDATE( COGO_MODEL );

   CComPtr<IProfile> profile;
   GetProfile(&profile);

   Float64 grade;
   profile->Grade(CComVariant(station),&grade);
   return grade;
}

Float64 CBridgeAgentImp::GetElevation(Float64 station,Float64 offset)
{
   VALIDATE( COGO_MODEL );

   CComPtr<IProfile> profile;
   GetProfile(&profile);

   Float64 elev;
   profile->Elevation(CComVariant(station),offset,&elev);
   return elev;
}

void CBridgeAgentImp::GetBearing(Float64 station,IDirection** ppBearing)
{
   VALIDATE( COGO_MODEL );

   CComPtr<IAlignment> alignment;
   GetAlignment(&alignment);

   alignment->Bearing(CComVariant(station),ppBearing);
}

void CBridgeAgentImp::GetBearingNormal(Float64 station,IDirection** ppNormal)
{
   VALIDATE( COGO_MODEL );

   CComPtr<IAlignment> alignment;
   GetAlignment(&alignment);

   alignment->Normal(CComVariant(station),ppNormal);
}

CollectionIndexType CBridgeAgentImp::GetCurveCount()
{
   VALIDATE( COGO_MODEL );

   CComPtr<IHorzCurveCollection> curves;
   m_CogoModel->get_HorzCurves(&curves);

   CollectionIndexType nCurves;
   curves->get_Count(&nCurves);
   return nCurves;
}

void CBridgeAgentImp::GetCurve(CollectionIndexType idx,IHorzCurve** ppCurve)
{
   VALIDATE( COGO_MODEL );

   CComPtr<IHorzCurveCollection> curves;
   m_CogoModel->get_HorzCurves(&curves);

   CogoObjectID key = m_HorzCurveKeys[idx];

   HRESULT hr = curves->get_Item(key,ppCurve);
   ATLASSERT(SUCCEEDED(hr));
}

CollectionIndexType CBridgeAgentImp::GetVertCurveCount()
{
   VALIDATE( COGO_MODEL );

   CComPtr<IVertCurveCollection> curves;
   m_CogoModel->get_VertCurves(&curves);

   CollectionIndexType nCurves;
   curves->get_Count(&nCurves);
   return nCurves;
}

void CBridgeAgentImp::GetVertCurve(CollectionIndexType idx,IVertCurve** ppCurve)
{
   VALIDATE( COGO_MODEL );

   CComPtr<IVertCurveCollection> curves;
   m_CogoModel->get_VertCurves(&curves);

   CogoObjectID key = m_VertCurveKeys[idx];

   HRESULT hr = curves->get_Item(key,ppCurve);
   ATLASSERT(SUCCEEDED(hr));
}

void CBridgeAgentImp::GetCrownPoint(Float64 station,IDirection* pDirection,IPoint2d** ppPoint)
{
   VALIDATE( COGO_MODEL );

   CComPtr<IAlignment> alignment;
   GetAlignment(&alignment);

   alignment->LocateCrownPoint2D(CComVariant(station),CComVariant(pDirection),ppPoint);
}

void CBridgeAgentImp::GetCrownPoint(Float64 station,IDirection* pDirection,IPoint3d** ppPoint)
{
   VALIDATE( COGO_MODEL );

   CComPtr<IAlignment> alignment;
   GetAlignment(&alignment);

   alignment->LocateCrownPoint3D(CComVariant(station),CComVariant(pDirection),ppPoint);
}

void CBridgeAgentImp::GetPoint(Float64 station,Float64 offset,IDirection* pBearing,IPoint2d** ppPoint)

{
   VALIDATE( COGO_MODEL );

   CComPtr<IAlignment> alignment;
   GetAlignment(&alignment);

   alignment->LocatePoint(CComVariant(station),omtAlongDirection, offset,CComVariant(pBearing),ppPoint);
}

void CBridgeAgentImp::GetStationAndOffset(IPoint2d* point,Float64* pStation,Float64* pOffset)
{
   VALIDATE( COGO_MODEL );

   CComPtr<IAlignment> alignment;
   GetAlignment(&alignment);

   CComPtr<IStation> station;
   Float64 offset;
   alignment->Offset(point,&station,&offset);

   station->get_Value(pStation);
   *pOffset = offset;
}

/////////////////////////////////////////////////////////////////////////
// IBridge
//
Float64 CBridgeAgentImp::GetLength()
{
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   return pBridgeDesc->GetLength();
}

Float64 CBridgeAgentImp::GetSpanLength(SpanIndexType spanIdx)
{
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();

   PierIndexType startPierIdx = spanIdx;
   PierIndexType endPierIdx   = startPierIdx+1;

   const CPierData2* pStartPier = pBridgeDesc->GetPier(startPierIdx);
   const CPierData2* pEndPier   = pBridgeDesc->GetPier(endPierIdx);

   Float64 length = pEndPier->GetStation() - pStartPier->GetStation();

   return length;
}

Float64 CBridgeAgentImp::GetGirderLayoutLength(const CGirderKey& girderKey)
{
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CGirderGroupData* pGroup = pBridgeDesc->GetGirderGroup(girderKey.groupIndex);
   const CSplicedGirderData* pGirder = pGroup->GetGirder(girderKey.girderIndex);
   SegmentIndexType nSegments = pGirder->GetSegmentCount();

   Float64 L = 0;
   for ( SegmentIndexType segIdx = 0; segIdx < nSegments; segIdx++ )
   {
      L += GetSegmentLayoutLength(CSegmentKey(girderKey,segIdx));
   }
   return L;
}

Float64 CBridgeAgentImp::GetAlignmentOffset()
{
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   return pBridgeDesc->GetAlignmentOffset();
}

Float64 CBridgeAgentImp::GetDistanceFromStartOfBridge(Float64 station)
{
   VALIDATE( BRIDGE );
   CComPtr<IPierCollection> piers;
   m_Bridge->get_Piers(&piers);

   CComPtr<IPier> pier;
   piers->get_Item(0,&pier);

   CComPtr<IStation> objStation;
   pier->get_Station(&objStation);

   Float64 sta_value;
   objStation->get_Value(&sta_value);

   Float64 dist = station - sta_value;

   return dist;
}

SpanIndexType CBridgeAgentImp::GetSpanCount()
{
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   return pBridgeDesc->GetSpanCount();
}

PierIndexType CBridgeAgentImp::GetPierCount()
{
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   return pBridgeDesc->GetPierCount();
}

SupportIndexType CBridgeAgentImp::GetTemporarySupportCount()
{
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   return pBridgeDesc->GetTemporarySupportCount();
}

GroupIndexType CBridgeAgentImp::GetGirderGroupCount()
{
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   return pBridgeDesc->GetGirderGroupCount();
}

GirderIndexType CBridgeAgentImp::GetGirderCount(GroupIndexType grpIdx)
{
   VALIDATE( BRIDGE );

   ATLASSERT( grpIdx != ALL_GROUPS );

   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CGirderGroupData* pGroup = pBridgeDesc->GetGirderGroup(grpIdx);
   return pGroup->GetGirderCount();
}

GirderIndexType CBridgeAgentImp::GetGirderCountBySpan(SpanIndexType spanIdx)
{
   VALIDATE( BRIDGE );

   ATLASSERT( spanIdx != ALL_SPANS );

   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CSpanData2* pSpan = pBridgeDesc->GetSpan(spanIdx);
   const CGirderGroupData* pGroup = pBridgeDesc->GetGirderGroup(pSpan);
   return pGroup->GetGirderCount();
}

SegmentIndexType CBridgeAgentImp::GetSegmentCount(const CGirderKey& girderKey)
{
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CGirderGroupData* pGroup = pBridgeDesc->GetGirderGroup(girderKey.groupIndex);
   const CSplicedGirderData* pGirder = pGroup->GetGirder(girderKey.girderIndex);
   return pGirder->GetSegmentCount();
}

SegmentIndexType CBridgeAgentImp::GetSegmentCount(GroupIndexType grpIdx,GirderIndexType gdrIdx)
{
   return GetSegmentCount(CGirderKey(grpIdx,gdrIdx));
}

PierIndexType CBridgeAgentImp::GetGirderGroupStartPier(GroupIndexType grpIdx)
{
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CGirderGroupData* pGroup = pBridgeDesc->GetGirderGroup(grpIdx == ALL_GROUPS ? 0 : grpIdx);
   return pGroup->GetPierIndex(pgsTypes::metStart);
}

PierIndexType CBridgeAgentImp::GetGirderGroupEndPier(GroupIndexType grpIdx)
{
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CGirderGroupData* pGroup = pBridgeDesc->GetGirderGroup(grpIdx == ALL_GROUPS ? pBridgeDesc->GetGirderGroupCount()-1 : grpIdx);
   return pGroup->GetPierIndex(pgsTypes::metEnd);
}

void CBridgeAgentImp::GetGirderGroupPiers(GroupIndexType grpIdx,PierIndexType* pStartPierIdx,PierIndexType* pEndPierIdx)
{
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CGirderGroupData* pGroup = pBridgeDesc->GetGirderGroup(grpIdx);
   *pStartPierIdx = pGroup->GetPierIndex(pgsTypes::metStart);
   *pEndPierIdx = pGroup->GetPierIndex(pgsTypes::metEnd);
}

SpanIndexType CBridgeAgentImp::GetGirderGroupStartSpan(GroupIndexType grpIdx)
{
   return (SpanIndexType)GetGirderGroupStartPier(grpIdx);
}

SpanIndexType CBridgeAgentImp::GetGirderGroupEndSpan(GroupIndexType grpIdx)
{
   return (SpanIndexType)(GetGirderGroupEndPier(grpIdx)-1);
}

void CBridgeAgentImp::GetGirderGroupSpans(GroupIndexType grpIdx,SpanIndexType* pStartSpanIdx,SpanIndexType* pEndSpanIdx)
{
   PierIndexType startPierIdx, endPierIdx;
   GetGirderGroupPiers(grpIdx,&startPierIdx,&endPierIdx);

   *pStartSpanIdx = (SpanIndexType)startPierIdx;
   *pEndSpanIdx   = (SpanIndexType)(endPierIdx-1);
}

GroupIndexType CBridgeAgentImp::GetGirderGroupIndex(SpanIndexType spanIdx)
{
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CSpanData2* pSpan = pBridgeDesc->GetSpan(spanIdx);
   const CGirderGroupData* pGroup = pBridgeDesc->GetGirderGroup(pSpan);
   return pGroup->GetIndex();
}

void CBridgeAgentImp::GetDistanceBetweenGirders(const pgsPointOfInterest& poi,Float64 *pLeft,Float64* pRight)
{
   VALIDATE( BRIDGE );

   const CSegmentKey& segmentKey = poi.GetSegmentKey();

   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CGirderGroupData* pGroup = pBridgeDesc->GetGirderGroup(segmentKey.groupIndex);
   const CSplicedGirderData* pGirder = pGroup->GetGirder(segmentKey.girderIndex);
   const CPrecastSegmentData* pSegment = pGirder->GetSegment(segmentKey.segmentIndex);

   const CGirderSpacing2* pStartSpacing;
   const CGirderSpacing2* pEndSpacing;
   pSegment->GetSpacing(&pStartSpacing,&pEndSpacing);

   // girder spacing is on the right hand side of the girder

   // Get spacing on the left side of the girder at start and end
   Float64 left_start = 0;
   Float64 left_end   = 0;

   if ( 0 != segmentKey.girderIndex )
   {
      SpacingIndexType spaceIdx = segmentKey.girderIndex-1;
      left_start = pStartSpacing->GetGirderSpacing(spaceIdx);
      left_end   = pEndSpacing->GetGirderSpacing(spaceIdx);
   }

   // Get spacing on the right side of the girder at start and end
   GirderIndexType nGirders = pGroup->GetGirderCount();
   Float64 right_start = 0;
   Float64 right_end   = 0;
   if ( segmentKey.girderIndex < nGirders-1 )
   {
      SpacingIndexType spaceIdx = segmentKey.girderIndex;
      right_start = pStartSpacing->GetGirderSpacing(spaceIdx);
      right_end   = pEndSpacing->GetGirderSpacing(spaceIdx);
   }

   Float64 gdrLength = GetSegmentLength(segmentKey);

   Float64 left  = ::LinInterp( poi.GetDistFromStart(), left_start,  left_end,  gdrLength );
   Float64 right = ::LinInterp( poi.GetDistFromStart(), right_start, right_end, gdrLength );

   // if the spacing is a joint spacing, we have what we are after
   if ( IsJointSpacing(pBridgeDesc->GetGirderSpacingType()) )
   {
      *pLeft = left;
      *pRight = right;
      return;
   }

   // not a joint spacing so the spacing is between CL girders.... deduct the width
   // of the adjacent girders and this girder
   Float64 left_width = 0;
   Float64 width = 0;
   Float64 right_width = 0;
   if ( 0 != segmentKey.girderIndex )
   {
      pgsPointOfInterest leftPoi(poi);
      leftPoi.SetSegmentKey( CSegmentKey(segmentKey.groupIndex,segmentKey.girderIndex-1,segmentKey.segmentIndex) );
      left_width = max(GetTopWidth(leftPoi),GetBottomWidth(leftPoi));
   }

   width = max(GetTopWidth(poi),GetBottomWidth(poi));

   if ( segmentKey.girderIndex < nGirders-1 )
   {
      pgsPointOfInterest rightPoi(poi);
      rightPoi.SetSegmentKey( segmentKey );
      right_width = max(GetTopWidth(rightPoi),GetBottomWidth(rightPoi));
   }

   // clear spacing is C-C spacing minus half the width of the adjacent girder minus the width of this girder
   *pLeft = left - left_width/2 - width/2;
   *pRight = right - right_width/2 - width/2;

   if ( *pLeft < 0 )
      *pLeft = 0;

   if ( *pRight < 0 )
      *pRight = 0;
}

std::vector<Float64> CBridgeAgentImp::GetGirderSpacing(PierIndexType pierIdx,pgsTypes::PierFaceType pierFace,pgsTypes::MeasurementLocation measureLocation,pgsTypes::MeasurementType measureType)
{
#pragma Reminder("UPDATE: this method only works if the pier as at a group boundary")

   VALIDATE( BRIDGE );
   SpanIndexType spanIdx = (pierFace == pgsTypes::Back ? pierIdx-1 : pierIdx);
   ATLASSERT( 0 <= spanIdx && spanIdx < GetSpanCount() );

   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CSpanData2* pSpan = pBridgeDesc->GetSpan(spanIdx);
   const CGirderGroupData* pGroup = pBridgeDesc->GetGirderGroup(pSpan);
   GroupIndexType grpIdx = pGroup->GetIndex();

   CComPtr<IBridgeGeometry> bridgeGeometry;
   m_Bridge->get_BridgeGeometry(&bridgeGeometry);

   GirderIndexType nGirders = pGroup->GetGirderCount();
   std::vector<Float64> spaces;
   for ( GirderIndexType gdrIdx = 1; gdrIdx < nGirders; gdrIdx++ )
   {
      GirderIndexType prevGdrIdx = gdrIdx - 1;
      SegmentIndexType leftGdrSegIdx = 0;
      SegmentIndexType rightGdrSegIdx = 0;
      if ( pierFace == pgsTypes::Back )
      {
         const CSplicedGirderData* pLeftGirder = pGroup->GetGirder(prevGdrIdx);
         const CSplicedGirderData* pRightGirder = pGroup->GetGirder(gdrIdx);

         leftGdrSegIdx  = pLeftGirder->GetSegmentCount()-1;
         rightGdrSegIdx = pRightGirder->GetSegmentCount()-1;
      }

      GirderIDType leftGdrID  = ::GetGirderSegmentLineID(grpIdx,prevGdrIdx,leftGdrSegIdx);
      GirderIDType rightGdrID = ::GetGirderSegmentLineID(grpIdx,gdrIdx,rightGdrSegIdx);

      CComPtr<IGirderLine> leftGirderLine, rightGirderLine;
      bridgeGeometry->FindGirderLine(leftGdrID,&leftGirderLine);
      bridgeGeometry->FindGirderLine(rightGdrID,&rightGirderLine);

      Float64 distance = -9999;
      if ( measureType == pgsTypes::NormalToItem )
      {
         // Intersect the two girder paths with the line that is normal to the alignment at the pier location
         // The spacing is the distance between these two points

         // create a line object for the normal to the alignment at the pier
         CComPtr<IDirection> normal;
         CComPtr<IPoint2d> pntAlignment;
         CComPtr<IAlignment> alignment;
         GetAlignment(&alignment);

         Float64 station = pBridgeDesc->GetPier(pierIdx)->GetStation();
         alignment->Normal(CComVariant(station),&normal);

         if ( measureLocation == pgsTypes::AtCenterlineBearing )
         {
            // need the CL Brg station
            CGirderKey girderKey(grpIdx,prevGdrIdx);
            station = (pierFace == pgsTypes::Back ? GetBackBearingStation(pierIdx,girderKey) : GetAheadBearingStation(pierIdx,girderKey));

            Float64 offset;
            ConnectionLibraryEntry::BearingOffsetMeasurementType measureType;
            pBridgeDesc->GetPier(pierIdx)->GetBearingOffset(pierFace,&offset,&measureType);

            if (measureType == ConnectionLibraryEntry::AlongGirder )
            {
               // get the normal at the CL bearing
               normal.Release();
               alignment->Normal(CComVariant(station),&normal);
            }
         }

         // get the point where the CL intersects the alignment
         alignment->LocatePoint(CComVariant(station),omtNormal,0.0,CComVariant(normal),&pntAlignment);

         CComPtr<ILine2d> line;
         line.CoCreateInstance(CLSID_Line2d);
         CComPtr<IVector2d> v;
         CComPtr<IPoint2d> pnt;
         line->GetExplicit(&pnt,&v);

         Float64 dir;
         normal->get_Value(&dir);
         v->put_Direction(dir);
         line->SetExplicit(pntAlignment,v);

         // get the girder paths
         CComPtr<IPath> leftPath, rightPath;
         leftGirderLine->get_Path(&leftPath);
         rightGirderLine->get_Path(&rightPath);

         // intersect each path with the normal line
         CComPtr<IPoint2d> pntLeft, pntRight;
         leftPath->Intersect(line,pntAlignment,&pntLeft);
         rightPath->Intersect(line,pntAlignment,&pntRight);

         // compute the distance
         pntLeft->DistanceEx(pntRight,&distance);
      }
      else
      {
         ATLASSERT(measureType == pgsTypes::AlongItem);

         CComPtr<IPoint2d> pntLeft, pntRight;
         switch ( measureLocation )
         {
         case pgsTypes::AtPierLine:
            leftGirderLine->get_PierPoint(pierFace == pgsTypes::Back ? etEnd : etStart,&pntLeft);
            rightGirderLine->get_PierPoint(pierFace == pgsTypes::Back ? etEnd : etStart,&pntRight);
            break;

         case pgsTypes::AtCenterlineBearing:
            leftGirderLine->get_BearingPoint(pierFace == pgsTypes::Back ? etEnd : etStart,&pntLeft);
            rightGirderLine->get_BearingPoint(pierFace == pgsTypes::Back ? etEnd : etStart,&pntRight);
            break;

         default:
            ATLASSERT(false);
         }

         pntLeft->DistanceEx(pntRight,&distance); // distance along line
      }

      ATLASSERT( 0 < distance );
      spaces.push_back(distance);
   }

   return spaces;
}

std::vector<SpaceBetweenGirder> CBridgeAgentImp::GetGirderSpacing(SpanIndexType spanIdx,Float64 distFromStartOfSpan)
{
   // Get the spacing between girders at the specified location. Spacing is measured normal to the alignment.
   // Idential adjacent spaces are grouped together

#pragma Reminder("UPDATE: delegate to Generic Bridge Model or Bridge Geometry Model")

   VALIDATE( BRIDGE );
   std::vector<SpaceBetweenGirder> vSpacing;

   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CSpanData2* pSpan = pBridgeDesc->GetSpan(spanIdx);
   const CGirderGroupData* pGroup = pBridgeDesc->GetGirderGroup(pSpan);
   GroupIndexType grpIdx = pGroup->GetIndex();

   GirderIndexType nGirders = pGroup->GetGirderCount();

   if ( nGirders <= 1 )
      return vSpacing;

   const CPierData2* pPier = pSpan->GetPrevPier();
   Float64 start_pier_station = pPier->GetStation();
   Float64 station = start_pier_station + distFromStartOfSpan;

   CComPtr<IAlignment> alignment;
   GetAlignment(&alignment);

   CComPtr<IDirection> normal;
   alignment->Normal(CComVariant(station),&normal);

   CComPtr<IPoint2d> pntAlignment;
   alignment->LocatePoint(CComVariant(station),omtAlongDirection,0.00,CComVariant(normal),&pntAlignment);

   SpaceBetweenGirder gdrSpacing;
   gdrSpacing.firstGdrIdx = 0;
   gdrSpacing.lastGdrIdx  = 0;
   gdrSpacing.spacing     = -1;

   for ( GirderIndexType gdrIdx = 0; gdrIdx < nGirders-1; gdrIdx++ )
   {
#pragma Reminder("UPDATE: assuming precast girder bridge")
      // Assuming segment 0... need to use location and figure out which segment the line
      // normal to the alignment passes through.

      CComPtr<IGirderLine> girderLine1;
      GetGirderLine(CSegmentKey(grpIdx,gdrIdx,0),&girderLine1);

      CComPtr<IGirderLine> girderLine2;
      GetGirderLine(CSegmentKey(grpIdx,gdrIdx+1,0),&girderLine2);

      CComPtr<IPoint2d> pntG11, pntG12;
      girderLine1->get_PierPoint(etStart,&pntG11);
      girderLine1->get_PierPoint(etEnd,  &pntG12);

      CComPtr<IDirection> dirG1;
      girderLine1->get_Direction(&dirG1);

      CComPtr<IPoint2d> pntG21, pntG22;
      girderLine2->get_PierPoint(etStart,&pntG21);
      girderLine2->get_PierPoint(etEnd,  &pntG22);

      CComPtr<IDirection> dirG2;
      girderLine2->get_Direction(&dirG2);


      CComPtr<IBridgeGeometry> bridgeGeometry;
      m_Bridge->get_BridgeGeometry(&bridgeGeometry);

      CComPtr<ICogoModel> cogoModel;
      bridgeGeometry->get_CogoModel(&cogoModel);

      CComPtr<ICogoEngine> cogoEngine;
      cogoModel->get_Engine(&cogoEngine);

      CComQIPtr<IMeasure2> measure(cogoEngine);
      CComQIPtr<IIntersect2> intersect(cogoEngine);

      CComPtr<IPoint2d> pntIntersect1, pntIntersect2;
      intersect->Bearings(pntAlignment,CComVariant(normal),0.00,pntG11,CComVariant(dirG1),0.00,&pntIntersect1);
      intersect->Bearings(pntAlignment,CComVariant(normal),0.00,pntG21,CComVariant(dirG2),0.00,&pntIntersect2);

      Float64 space;
      measure->Distance(pntIntersect1,pntIntersect2,&space);

      gdrSpacing.lastGdrIdx++;

      if ( !IsEqual(gdrSpacing.spacing,space) )
      {
         // this is a new spacing
         gdrSpacing.spacing = space; // save it
         vSpacing.push_back(gdrSpacing);
      }
      else
      {
         // this space is the same as the previous one... change the lastGdrIdx
         vSpacing.back().lastGdrIdx = gdrSpacing.lastGdrIdx;
      }

      gdrSpacing.firstGdrIdx = gdrSpacing.lastGdrIdx;
   }

   return vSpacing;
}

void CBridgeAgentImp::GetSpacingAlongGirder(const CSegmentKey& segmentKey,Float64 distFromStartOfGirder,Float64* leftSpacing,Float64* rightSpacing)
{
   GirderIDType leftGdrID, gdrID, rightGdrID;
   ::GetAdjacentSuperstructureMemberIDs(segmentKey,&leftGdrID,&gdrID,&rightGdrID);

   m_BridgeGeometryTool->GirderSpacingBySSMbr(m_Bridge,gdrID,distFromStartOfGirder,leftGdrID, leftSpacing);
   m_BridgeGeometryTool->GirderSpacingBySSMbr(m_Bridge,gdrID,distFromStartOfGirder,rightGdrID,rightSpacing);
}

std::vector<std::pair<SegmentIndexType,Float64>> CBridgeAgentImp::GetSegmentLengths(SpanIndexType spanIdx,GirderIndexType gdrIdx)
{
   ATLASSERT(spanIdx != ALL_SPANS);
   ATLASSERT(gdrIdx  != ALL_GIRDERS);

   std::vector<std::pair<SegmentIndexType,Float64>> seg_lengths;

   // Returns the structural span length for (spanIdx,gdrIdx) measured along the centerline of the girder
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();

   // Get the group this span belongs to
   const CGirderGroupData* pGroup = pBridgeDesc->GetGirderGroup( pBridgeDesc->GetSpan(spanIdx) );
   GroupIndexType grpIdx = pGroup->GetIndex();
   GroupIndexType nGroups = pBridgeDesc->GetGirderGroupCount();

   // Get the girder in this group
   const CSplicedGirderData* pGirder = pGroup->GetGirder(gdrIdx);

   Float64 span_length = 0;

   // Determine which segments are part of this span
   SegmentIndexType nSegments = pGirder->GetSegmentCount();
   for ( SegmentIndexType segIdx = 0; segIdx < nSegments; segIdx++ )
   {
      const CPrecastSegmentData* pSegment = pGirder->GetSegment(segIdx);
      CSegmentKey segmentKey( pSegment->GetSegmentKey() );
      
      SpanIndexType startSpanIdx = pSegment->GetSpanIndex(pgsTypes::metStart);
      SpanIndexType endSpanIdx   = pSegment->GetSpanIndex(pgsTypes::metEnd);

      if ( startSpanIdx <= spanIdx && spanIdx <= endSpanIdx )
      {
         // this segment starts, ends, or is completely in the span

         if ( startSpanIdx < spanIdx && spanIdx == endSpanIdx )
         {
            // segment starts in a previous span but ends in this span
            // Only count the length of segment in this span
            // Intersect the girder line for this segment with the
            // pier line at the start of this span
            // We want the distance from the intersection point to the end of the segment
            //
            //              |<---Segment---->|
            //    |                |   spanIdx       |
            //    |--------.-----------------.-------|
            //    |                |                 |
            //
            //                     |<------->|
            //                        Length of segment in this span

            CComPtr<ISegment> segment;
            GetSegment(segmentKey,&segment);
            CComPtr<IGirderLine> girderLine;
            segment->get_GirderLine(&girderLine);
            CComPtr<IPath> path;
            girderLine->get_Path(&path);

            CComPtr<IPierLine> pierLine;
            GetPierLine(spanIdx,&pierLine);
            CComPtr<ILine2d> clPier;
            pierLine->get_Centerline(&clPier);

            CComPtr<IPoint2d> pntNearest;
            pierLine->get_AlignmentPoint(&pntNearest);

            CComPtr<IPoint2d> pntIntersect;
            path->Intersect(clPier,pntNearest,&pntIntersect);

            CComPtr<IPoint2d> pntSegmentEnd;
            girderLine->get_PierPoint(etEnd,&pntSegmentEnd);

            Float64 distance;
            pntSegmentEnd->DistanceEx(pntIntersect,&distance);

            seg_lengths.push_back(std::make_pair(segIdx,distance));
         }
         else if ( startSpanIdx == spanIdx && spanIdx < endSpanIdx )
         {
            // Segment starts in ths span but ends in a later span
            // Only count the length of segment in this span
            // Intersect the girder line for this segment with the
            // pier line at the end of this span
            // We want the distance from the start of the segment to the intersection point
            //
            //              |<---Segment---->|
            //    |  spanIdx       |                 |
            //    |--------.-----------------.-------|
            //    |                |                 |
            //
            //             |<----->|
            //                 Length of segment in this span

            CComPtr<ISegment> segment;
            GetSegment(segmentKey,&segment);
            CComPtr<IGirderLine> girderLine;
            segment->get_GirderLine(&girderLine);
            CComPtr<IPath> path;
            girderLine->get_Path(&path);

            CComPtr<IPierLine> pierLine;
            GetPierLine(spanIdx+1,&pierLine);
            CComPtr<ILine2d> clPier;
            pierLine->get_Centerline(&clPier);

            CComPtr<IPoint2d> pntNearest;
            pierLine->get_AlignmentPoint(&pntNearest);

            CComPtr<IPoint2d> pntIntersect;
            path->Intersect(clPier,pntNearest,&pntIntersect);

            CComPtr<IPoint2d> pntSegmentStart;
            girderLine->get_PierPoint(etStart,&pntSegmentStart);

            Float64 distance;
            pntSegmentStart->DistanceEx(pntIntersect,&distance);

            seg_lengths.push_back(std::make_pair(segIdx,distance));
         }
         else if ( spanIdx < startSpanIdx && endSpanIdx < spanIdx )
         {
            // Segments starts in a previous span and end in a later span
            // Intersect the girder line for this segment with the pier
            // lines at the start and end of the span
            //
            //              |<---------Segment--------------->|
            //    |                |   spanIdx       |            |
            //    |--------.----------------------------------.---|
            //    |                |                 |            |
            //
            //                     |<--------------->|
            //                          Length of segment in this span

            CComPtr<ISegment> segment;
            GetSegment(segmentKey,&segment);
            CComPtr<IGirderLine> girderLine;
            segment->get_GirderLine(&girderLine);
            CComPtr<IPath> path;
            girderLine->get_Path(&path);

            CComPtr<IPierLine> startPierLine, endPierLine;
            GetPierLine(spanIdx,&startPierLine);
            GetPierLine(spanIdx+1,&endPierLine);

            CComPtr<ILine2d> clStartPier, clEndPier;
            startPierLine->get_Centerline(&clStartPier);
            endPierLine->get_Centerline(&clEndPier);

            CComPtr<IPoint2d> pntNearest;
            startPierLine->get_AlignmentPoint(&pntNearest);

            CComPtr<IPoint2d> pntStartIntersect;
            path->Intersect(clStartPier,pntNearest,&pntStartIntersect);

            pntNearest.Release();
            endPierLine->get_AlignmentPoint(&pntNearest);

            CComPtr<IPoint2d> pntEndIntersect;
            path->Intersect(clEndPier,pntNearest,&pntEndIntersect);

            Float64 distance;
            pntStartIntersect->DistanceEx(pntEndIntersect,&distance);

            seg_lengths.push_back(std::make_pair(segIdx,distance));
         }
         else
         {
            // segment is completely within the span
            CComPtr<IGirderLine> girderLine;
            GetGirderLine(segmentKey,&girderLine);
            Float64 distance;
            girderLine->get_LayoutLength(&distance); // use the CL-Pier to CL-Pier length to account for closure pours

            if ( segIdx == 0 && grpIdx == 0 )
            {
               // first segment in the first group, measure from CL-Bearing
               Float64 brg_offset;
               girderLine->get_BearingOffset(etStart,&brg_offset);
               distance -= brg_offset;
            }
            
            if ( segIdx == nSegments-1 && grpIdx == nGroups-1)
            {
               // last segment in the last group, measure from CL-Bearing
               Float64 brg_offset;
               girderLine->get_BearingOffset(etEnd,&brg_offset);
               distance -= brg_offset;
            }

            seg_lengths.push_back(std::make_pair(segIdx,distance));
         }
      }

      // Once the segment starts after this span, break out of the loop
      if ( spanIdx < startSpanIdx )
         break;
   }

   return seg_lengths;
}

void CBridgeAgentImp::GetSegmentLocation(SpanIndexType spanIdx,GirderIndexType girderIdx,Float64 distFromStartOfSpan,CSegmentKey* pSegmentKey,Float64* pDistFromStartOfSegment)
{
   Float64 dist = 0; // distance from start of span
   std::vector<std::pair<SegmentIndexType,Float64>> segmentLengths( GetSegmentLengths(spanIdx,girderIdx) );
   std::vector<std::pair<SegmentIndexType,Float64>>::iterator iter(segmentLengths.begin());
   std::vector<std::pair<SegmentIndexType,Float64>>::iterator end(segmentLengths.end());
   for ( ; iter != end; iter++ )
   {
      SegmentIndexType segIdx = iter->first;
      Float64 segLength = iter->second;
      if (  InRange(dist,distFromStartOfSpan,dist+segLength) )
      {
         // this is the segment!
         *pDistFromStartOfSegment = distFromStartOfSpan - dist;

         GET_IFACE(IBridgeDescription,pIBridgeDesc);
         const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
         const CSpanData2* pSpan = pBridgeDesc->GetSpan(spanIdx);
         const CGirderGroupData* pGroup = pBridgeDesc->GetGirderGroup(pSpan);
         GroupIndexType grpIdx = pGroup->GetIndex();
         *pSegmentKey = CSegmentKey(grpIdx,girderIdx,segIdx);

         return;
      }

      dist += segLength;
   }
}

Float64 CBridgeAgentImp::GetSpanLength(SpanIndexType spanIdx,GirderIndexType gdrIdx)
{
   Float64 span_length = 0;
   std::vector<std::pair<SegmentIndexType,Float64>> seg_lengths(GetSegmentLengths(spanIdx,gdrIdx));
   std::vector<std::pair<SegmentIndexType,Float64>>::iterator iter(seg_lengths.begin());
   std::vector<std::pair<SegmentIndexType,Float64>>::iterator iterEnd(seg_lengths.end());
   for ( ; iter != iterEnd; iter++ )
   {
      span_length += (*iter).second;
   }

   return span_length;
}

Float64 CBridgeAgentImp::GetSegmentStartEndDistance(const CSegmentKey& segmentKey)
{
   // distance from CL Bearing to start of girder, measured along CL Girder
   CComPtr<IPrecastGirder> girder;
   GetGirder(segmentKey,&girder);
   Float64 length;
   girder->get_LeftEndDistance(&length);
   return length;
}

Float64 CBridgeAgentImp::GetSegmentEndEndDistance(const CSegmentKey& segmentKey)
{
   // distance from CL Bearing to end of girder, measured along CL Girder
   CComPtr<IPrecastGirder> girder;
   GetGirder(segmentKey,&girder);
   Float64 length;
   girder->get_RightEndDistance(&length);
   return length;
}

Float64 CBridgeAgentImp::GetSegmentOffset(const CSegmentKey& segmentKey,Float64 station)
{
   VALIDATE( BRIDGE );
   Float64 offset;
   HRESULT hr = m_BridgeGeometryTool->GirderPathOffset(m_Bridge,::GetSuperstructureMemberID(segmentKey.groupIndex,segmentKey.girderIndex),segmentKey.segmentIndex,CComVariant(station),&offset);
   ATLASSERT( SUCCEEDED(hr) );
   return offset;
}

void CBridgeAgentImp::GetPoint(const CSegmentKey& segmentKey,Float64 distFromStartOfSegment,IPoint2d** ppPoint)
{
   VALIDATE( BRIDGE );
   CComPtr<IPoint2d> pntPier1,pntEnd1,pntBrg1,pntBrg2,pntEnd2,pntPier2;
   GetSegmentEndPoints(segmentKey,&pntPier1,&pntEnd1,&pntBrg1,&pntBrg2,&pntEnd2,&pntPier2);

   CComPtr<ILocate2> locate;
   m_CogoEngine->get_Locate(&locate);
   HRESULT hr = locate->PointOnLine(pntEnd1,pntEnd2,distFromStartOfSegment,0.0,ppPoint);
   ATLASSERT( SUCCEEDED(hr) );
}

void CBridgeAgentImp::GetPoint(const pgsPointOfInterest& poi,IPoint2d** ppPoint)
{
   const CSegmentKey& segmentKey = poi.GetSegmentKey();
   GetPoint(segmentKey,poi.GetDistFromStart(),ppPoint);
}

bool CBridgeAgentImp::GetSegmentPierIntersection(const CSegmentKey& segmentKey,PierIndexType pierIdx,IPoint2d** ppPoint)
{
   VALIDATE(BRIDGE);
   CComPtr<IGirderLine> girderLine;
   GetGirderLine(segmentKey,&girderLine);

   CComPtr<IPierLine> pierLine;
   GetPierLine(pierIdx,&pierLine);

   CComPtr<IPath> path;
   girderLine->get_Path(&path);

   CComPtr<ILine2d> line;
   pierLine->get_Centerline(&line);

   CComPtr<IPoint2d> nearest;
   pierLine->get_AlignmentPoint(&nearest);

   HRESULT hr = path->IntersectEx(line,nearest,VARIANT_FALSE,VARIANT_FALSE,ppPoint);
   if ( FAILED(hr) )
   {
      ATLASSERT(hr == COGO_E_NOINTERSECTION);
      return false;
   }
   return true;
}

bool CBridgeAgentImp::GetSegmentTempSupportIntersection(const CSegmentKey& segmentKey,SupportIndexType tsIdx,IPoint2d** ppPoint)
{
   VALIDATE(BRIDGE);
   CComPtr<IGirderLine> girderLine;
   GetGirderLine(segmentKey,&girderLine);

   // recall that temporary supports are modeled with pier line objects in the bridge geometry model
   CComPtr<IPierLine> pierLine;
   GetTemporarySupportLine(tsIdx,&pierLine);

   CComPtr<IPath> path;
   girderLine->get_Path(&path);

   CComPtr<ILine2d> line;
   pierLine->get_Centerline(&line);

   CComPtr<IPoint2d> nearest;
   pierLine->get_AlignmentPoint(&nearest);

   HRESULT hr = path->IntersectEx(line,nearest,VARIANT_FALSE,VARIANT_FALSE,ppPoint);
   if ( FAILED(hr) )
   {
      ATLASSERT(hr == COGO_E_NOINTERSECTION);
      return false;
   }
   return true;
}

void CBridgeAgentImp::GetStationAndOffset(const CSegmentKey& segmentKey,Float64 distFromStartOfSegment,Float64* pStation,Float64* pOffset)
{
   VALIDATE( BRIDGE );

   CComPtr<IPoint2d> point;
   GetPoint(segmentKey,distFromStartOfSegment,&point);

   GetStationAndOffset(point,pStation,pOffset);
}

void CBridgeAgentImp::GetStationAndOffset(const pgsPointOfInterest& poi,Float64* pStation,Float64* pOffset)
{
   const CSegmentKey& segmentKey = poi.GetSegmentKey();
   GetStationAndOffset(segmentKey,poi.GetDistFromStart(),pStation,pOffset);
}

Float64 CBridgeAgentImp::GetDistanceFromStartOfBridge(const pgsPointOfInterest& poi)
{
   const CSegmentKey& segmentKey = poi.GetSegmentKey();
   return GetDistanceFromStartOfBridge(segmentKey,poi.GetDistFromStart());
}

Float64 CBridgeAgentImp::GetDistanceFromStartOfBridge(const CSegmentKey& segmentKey,Float64 distFromStartOfSegment)
{
   Float64 station,offset;
   GetStationAndOffset(segmentKey,distFromStartOfSegment,&station,&offset);

   Float64 start_station = GetPierStation(0);
   return station - start_station;
}

void CBridgeAgentImp::GetDistFromStartOfSpan(GirderIndexType gdrIdx,Float64 distFromStartOfBridge,SpanIndexType* pSpanIdx,Float64* pDistFromStartOfSpan)
{
#pragma Reminder("Is this method still needed?") 
   // does it make sense???
   // this method assumes precast girder bridge... would it even work for spliced girders???

   ATLASSERT(false); // need to update method
   *pSpanIdx = 0;
   *pDistFromStartOfSpan = distFromStartOfBridge;
   //VALIDATE(BRIDGE);

   //m_BridgeGeometryTool->GirderLinePoint(m_Bridge,distFromStartOfBridge,gdrIdx,pSpanIdx,pDistFromStartOfSpan);
}

bool CBridgeAgentImp::IsInteriorGirder(const CGirderKey& girderKey)
{
   CComPtr<ISuperstructureMember> ssMbr;
   GetSuperstructureMember(girderKey,&ssMbr);

   LocationType location;
   ssMbr->get_LocationType(&location);

   return location == ltInteriorGirder ? true : false;
}

bool CBridgeAgentImp::IsExteriorGirder(const CGirderKey& girderKey)
{
   return !IsInteriorGirder(girderKey);
}

bool CBridgeAgentImp::IsLeftExteriorGirder(const CGirderKey& girderKey)
{
   CComPtr<ISuperstructureMember> ssMbr;
   GetSuperstructureMember(girderKey,&ssMbr);

   LocationType location;
   ssMbr->get_LocationType(&location);

   return location == ltLeftExteriorGirder ? true : false;
}

bool CBridgeAgentImp::IsRightExteriorGirder(const CGirderKey& girderKey)
{
   CComPtr<ISuperstructureMember> ssMbr;
   GetSuperstructureMember(girderKey,&ssMbr);

   LocationType location;
   ssMbr->get_LocationType(&location);

   return location == ltRightExteriorGirder ? true : false;
}

void CBridgeAgentImp::GetBackSideEndDiaphragmSize(PierIndexType pierIdx,Float64* pW,Float64* pH)
{
   VALIDATE( BRIDGE );

   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CPierData2* pPierData = pBridgeDesc->GetPier(pierIdx);
   ATLASSERT( pPierData );

   *pH = pPierData->GetDiaphragmHeight(pgsTypes::Back);
   *pW = pPierData->GetDiaphragmWidth(pgsTypes::Back);
}

void CBridgeAgentImp::GetAheadSideEndDiaphragmSize(PierIndexType pierIdx,Float64* pW,Float64* pH)
{
   VALIDATE( BRIDGE );
  
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CPierData2* pPierData = pBridgeDesc->GetPier(pierIdx);
   ATLASSERT( pPierData );

   if ( pPierData->IsInteriorPier() )
   {
      // if this pier has a continuous segment connection type, there is only
      // on diaphragm at the pier... the data for that diaphragm is on the back side
      // the ahead side data isn't used
      *pH = 0;
      *pW = 0;
   }
   else
   {
      *pH = pPierData->GetDiaphragmHeight(pgsTypes::Ahead);
      *pW = pPierData->GetDiaphragmWidth(pgsTypes::Ahead);
   }
}

Float64 CBridgeAgentImp::GetSegmentStartBearingOffset(const CSegmentKey& segmentKey)
{
   // returns distance from CL pier to the bearing line,
   // measured along CL girder
   CComPtr<IPrecastGirder> girder;
   GetGirder(segmentKey,&girder);
   Float64 length;
   girder->get_LeftBearingOffset(&length);
   return length;
}

Float64 CBridgeAgentImp::GetSegmentEndBearingOffset(const CSegmentKey& segmentKey)
{
   // returns distance from CL pier to the bearing line,
   // measured along CL girder
   CComPtr<IPrecastGirder> girder;
   GetGirder(segmentKey,&girder);
   Float64 length;
   girder->get_RightBearingOffset(&length);
   return length;
}

Float64 CBridgeAgentImp::GetSegmentStartSupportWidth(const CSegmentKey& segmentKey)
{
   // returns the support width
   // measured along the CL girder
   Float64 support_width;
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CPrecastSegmentData* pSegment = pIBridgeDesc->GetPrecastSegmentData(segmentKey);
   const CClosurePourData* pClosure = pSegment->GetLeftClosure();
   if ( pClosure )
   {
      if ( pClosure->GetPier() )
         support_width = pClosure->GetPier()->GetSupportWidth(pgsTypes::Ahead);
      else
         support_width = pClosure->GetTemporarySupport()->GetSupportWidth();
   }
   else
   {
      const CSpanData2* pSpan = pSegment->GetSpan(pgsTypes::metStart);
      support_width = pSpan->GetPrevPier()->GetSupportWidth(pgsTypes::Ahead);
   }

   return support_width;
}

Float64 CBridgeAgentImp::GetSegmentEndSupportWidth(const CSegmentKey& segmentKey)
{
   // returns the support width
   // measured along the CL girder
   Float64 support_width;
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CPrecastSegmentData* pSegment = pIBridgeDesc->GetPrecastSegmentData(segmentKey);
   const CClosurePourData* pClosure = pSegment->GetRightClosure();
   if ( pClosure )
   {
      if ( pClosure->GetPier() )
         support_width = pClosure->GetPier()->GetSupportWidth(pgsTypes::Back);
      else
         support_width = pClosure->GetTemporarySupport()->GetSupportWidth();
   }
   else
   {
      const CSpanData2* pSpan = pSegment->GetSpan(pgsTypes::metEnd);
      support_width = pSpan->GetNextPier()->GetSupportWidth(pgsTypes::Back);
   }

   return support_width;
}

Float64 CBridgeAgentImp::GetCLPierToCLBearingDistance(const CSegmentKey& segmentKey,pgsTypes::MemberEndType endType,pgsTypes::MeasurementType measure)
{
   VALIDATE( BRIDGE );

   Float64 dist; // distance from CL pier to CL Bearing along the CL girder
   switch( endType )
   {
      case pgsTypes::metStart: // at start of span
         dist = GetSegmentStartBearingOffset(segmentKey);
         break;

      case pgsTypes::metEnd: // at end of span
         dist = GetSegmentEndBearingOffset(segmentKey);
         break;
   }

   if ( measure == pgsTypes::NormalToItem )
   {
      // we want dist to be measured normal to the pier
      // adjust the distance
      GET_IFACE(IBridgeDescription,pIBridgeDesc);
      const CPrecastSegmentData* pSegment = pIBridgeDesc->GetPrecastSegmentData(segmentKey);

      const CPierData2* pPier;
      const CTemporarySupportData* pTS;
      if ( endType == pgsTypes::metStart )
         pSegment->GetStartSupport(&pPier,&pTS);
      else
         pSegment->GetEndSupport(&pPier,&pTS);

      CComPtr<IDirection> supportDirection;

      if ( pPier )
      {
         GetPierDirection(pPier->GetIndex(),&supportDirection);
      }
      else
      {
         GetTemporarySupportDirection(pTS->GetIndex(),&supportDirection);
      }


      CComPtr<IDirection> dirSegment;
      GetSegmentBearing(segmentKey,&dirSegment);

      CComPtr<IAngle> angleBetween;
      supportDirection->AngleBetween(dirSegment,&angleBetween);

      Float64 angle;
      angleBetween->get_Value(&angle);

      dist *= sin(angle);
   }


   return dist;
}

Float64 CBridgeAgentImp::GetCLPierToSegmentEndDistance(const CSegmentKey& segmentKey,pgsTypes::MemberEndType endType,pgsTypes::MeasurementType measure)
{
   VALIDATE( BRIDGE );
   Float64 distCLPierToCLBearingAlongGirder = GetCLPierToCLBearingDistance(segmentKey,endType,pgsTypes::AlongItem);
   Float64 endDist;

   switch ( endType )
   {
   case pgsTypes::metStart: // at start of span
      endDist = GetSegmentStartEndDistance(segmentKey);
      break;

   case pgsTypes::metEnd: // at end of span
      endDist = GetSegmentEndEndDistance(segmentKey);
      break;
   }

   Float64 distCLPierToEndGirderAlongGirder = distCLPierToCLBearingAlongGirder - endDist;

   if ( measure == pgsTypes::NormalToItem )
   {
      // we want dist to be measured normal to the pier
      // adjust the distance
      GET_IFACE(IBridgeDescription,pIBridgeDesc);
      const CPrecastSegmentData* pSegment = pIBridgeDesc->GetPrecastSegmentData(segmentKey);

      const CPierData2* pPier;
      const CTemporarySupportData* pTS;
      if ( endType == pgsTypes::metStart )
         pSegment->GetStartSupport(&pPier,&pTS);
      else
         pSegment->GetEndSupport(&pPier,&pTS);

      CComPtr<IDirection> supportDirection;

      if ( pPier )
      {
         GetPierDirection(pPier->GetIndex(),&supportDirection);
      }
      else
      {
         GetTemporarySupportDirection(pTS->GetIndex(),&supportDirection);
      }


      CComPtr<IDirection> dirSegment;
      GetSegmentBearing(segmentKey,&dirSegment);

      CComPtr<IAngle> angleBetween;
      supportDirection->AngleBetween(dirSegment,&angleBetween);

      Float64 angle;
      angleBetween->get_Value(&angle);

      distCLPierToEndGirderAlongGirder *= sin(angle);
   }

   return distCLPierToEndGirderAlongGirder;
}

void CBridgeAgentImp::GetSegmentBearing(const CSegmentKey& segmentKey,IDirection** ppBearing)
{
   VALIDATE( BRIDGE );
   CComPtr<IGirderLine> girderLine;
   GetGirderLine(segmentKey,&girderLine);
   girderLine->get_Direction(ppBearing);
}

void CBridgeAgentImp::GetSegmentAngle(const CSegmentKey& segmentKey,pgsTypes::MemberEndType endType,IAngle** ppAngle)
{
   CComPtr<IDirection> gdrDir;
   GetSegmentBearing(segmentKey,&gdrDir);

   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CPrecastSegmentData* pSegment = pIBridgeDesc->GetPrecastSegmentData(segmentKey);

   const CPierData2* pPier;
   const CTemporarySupportData* pTS;
   if ( endType == pgsTypes::metStart )
      pSegment->GetStartSupport(&pPier,&pTS);
   else
      pSegment->GetEndSupport(&pPier,&pTS);

   CComPtr<IDirection> supportDirection;

   if ( pPier )
   {
      GetPierDirection(pPier->GetIndex(),&supportDirection);
   }
   else
   {
      GetTemporarySupportDirection(pTS->GetIndex(),&supportDirection);
   }

   supportDirection->AngleBetween(gdrDir,ppAngle);
}

CSegmentKey CBridgeAgentImp::GetSegmentAtPier(PierIndexType pierIdx,const CGirderKey& girderKey)
{
   ATLASSERT(girderKey.girderIndex != ALL_GIRDERS);

   // Gets the girder segment that touches the pier 
   // When two segments touch a pier, the segment will be in the girder group defined by the girderKey
   // If both girders are in the same group, the left segment is returned
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   GroupIndexType nGroups = pBridgeDesc->GetGirderGroupCount();
   GroupIndexType startGroupIdx = (girderKey.groupIndex == ALL_GROUPS ? 0 : girderKey.groupIndex);
   GroupIndexType endGroupIdx   = (girderKey.groupIndex == ALL_GROUPS ? nGroups-1 : startGroupIdx);
   for ( GroupIndexType grpIdx = startGroupIdx; grpIdx <= endGroupIdx; grpIdx++ )
   {
      const CGirderGroupData* pGroup = pBridgeDesc->GetGirderGroup(grpIdx);
      const CSplicedGirderData* pGirder = pGroup->GetGirder(girderKey.girderIndex);

      const CPierData2* pPierData = pBridgeDesc->GetPier(pierIdx);
      Float64 pierStation = pPierData->GetStation();

      SegmentIndexType nSegments = pGirder->GetSegmentCount();
      for ( SegmentIndexType segIdx = 0; segIdx < nSegments; segIdx++ )
      {
         const CPrecastSegmentData* pSegment = pGirder->GetSegment(segIdx);
         Float64 startStation,endStation;
         pSegment->GetStations(startStation,endStation);

         if ( ::InRange(startStation,pierStation,endStation) )
         {
            CSegmentKey segmentKey(girderKey,segIdx);
            return segmentKey;
         }
      } // next segment
   } // next group

   ATLASSERT(false); // should never get here
   return CSegmentKey();
}

void CBridgeAgentImp::GetSpansForSegment(const CSegmentKey& segmentKey,SpanIndexType* pStartSpanIdx,SpanIndexType* pEndSpanIdx)
{
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CPrecastSegmentData* pSegment = pBridgeDesc->GetGirderGroup(segmentKey.groupIndex)->GetGirder(segmentKey.girderIndex)->GetSegment(segmentKey.segmentIndex);
   const CSpanData2* pStartSpan = pSegment->GetSpan(pgsTypes::metStart);
   const CSpanData2* pEndSpan   = pSegment->GetSpan(pgsTypes::metEnd);

   *pStartSpanIdx = pStartSpan->GetIndex();
   *pEndSpanIdx   = pEndSpan->GetIndex();
}

//-----------------------------------------------------------------------------
bool CBridgeAgentImp::GetSpan(Float64 station,SpanIndexType* pSpanIdx)
{
   PierIndexType nPiers = GetPierCount();
   Float64 prev_pier = GetPierStation(0);
   for ( PierIndexType pierIdx = 1; pierIdx < nPiers; pierIdx++ )
   {
      Float64 next_pier;
      next_pier = GetPierStation(pierIdx);

      if ( IsLE(prev_pier,station) && IsLE(station,next_pier) )
      {
         *pSpanIdx = SpanIndexType(pierIdx-1);
         return true;
      }

      prev_pier = next_pier;
   }

   return false; // not found
}

//-----------------------------------------------------------------------------
GDRCONFIG CBridgeAgentImp::GetSegmentConfiguration(const CSegmentKey& segmentKey)
{
#pragma Reminder("UPDATE: assuming precast girder bridge")
   // Assuming interval for material properties (see below).
   // May want to remove Fc/Fci and Ec/Eci from config and just
   // have Fc and Ec where they are based on a specified interval
   // that would be passed into this method


   // Make sure girder is properly modeled before beginning
   VALIDATE( GIRDER );

   GDRCONFIG config;

   // Get the girder
   CComPtr<IPrecastGirder> girder;
   GetGirder(segmentKey,&girder);

   // Strand fills
   CComPtr<IIndexArray> fill[3];
   ConfigStrandFillVector fillVec[3];

   girder->get_StraightStrandFill(&fill[pgsTypes::Straight]);
   IndexArray2ConfigStrandFillVec(fill[pgsTypes::Straight], fillVec[pgsTypes::Straight]);
   config.PrestressConfig.SetStrandFill(pgsTypes::Straight, fillVec[pgsTypes::Straight]);

   girder->get_HarpedStrandFill(&fill[pgsTypes::Harped]);
   IndexArray2ConfigStrandFillVec(fill[pgsTypes::Harped], fillVec[pgsTypes::Harped]);
   config.PrestressConfig.SetStrandFill(pgsTypes::Harped, fillVec[pgsTypes::Harped]);

   girder->get_TemporaryStrandFill(&fill[pgsTypes::Temporary]);
   IndexArray2ConfigStrandFillVec(fill[pgsTypes::Temporary], fillVec[pgsTypes::Temporary]);
   config.PrestressConfig.SetStrandFill(pgsTypes::Temporary, fillVec[pgsTypes::Temporary]);

   // Get harping point offsets
   girder->get_HarpedStrandAdjustmentEnd(&config.PrestressConfig.EndOffset);
   girder->get_HarpedStrandAdjustmentHP(&config.PrestressConfig.HpOffset);

   // Get jacking force
   GET_IFACE(ISegmentData,pSegmentData);  
   const CStrandData* pStrands = pSegmentData->GetStrandData(segmentKey);
   const CGirderMaterial* pMaterial = pSegmentData->GetSegmentMaterial(segmentKey);

   for ( Uint16 i = 0; i < 3; i++ )
   {
      pgsTypes::StrandType strandType = (pgsTypes::StrandType)i;

      config.PrestressConfig.Pjack[strandType] = pStrands->Pjack[strandType];

      // Convert debond data
      // Use tool to compute strand position index from grid index used by CDebondInfo
      ConfigStrandFillTool fillTool( fillVec[strandType] );

      std::vector<CDebondData>::const_iterator iter = pStrands->Debond[strandType].begin();
      std::vector<CDebondData>::const_iterator iterend = pStrands->Debond[strandType].end();
      while(iter != iterend)
      {
         const CDebondData& debond_data = *iter;

         // convert grid index to strands index
         StrandIndexType index1, index2;
         fillTool.GridIndexToStrandPositionIndex(debond_data.strandTypeGridIdx, &index1, &index2);

         DEBONDCONFIG di;
         ATLASSERT(index1 != INVALID_INDEX);
         di.strandIdx         = index1;
         di.LeftDebondLength  = debond_data.Length1;
         di.RightDebondLength = debond_data.Length2;

         config.PrestressConfig.Debond[i].push_back(di);

         if ( index2 != INVALID_INDEX )
         {
            di.strandIdx         = index2;
            di.LeftDebondLength  = debond_data.Length1;
            di.RightDebondLength = debond_data.Length2;

            config.PrestressConfig.Debond[i].push_back(di);
         }

         iter++;
      }

      // sorts based on strand index
      std::sort(config.PrestressConfig.Debond[strandType].begin(),config.PrestressConfig.Debond[strandType].end());

      for ( Uint16 j = 0; j < 2; j++ )
      {
         pgsTypes::MemberEndType endType = pgsTypes::MemberEndType(j);
         const std::vector<GridIndexType>& gridIndicies(pStrands->GetExtendedStrands(strandType,endType));
         std::vector<StrandIndexType> strandIndicies;
         std::vector<GridIndexType>::const_iterator iter(gridIndicies.begin());
         std::vector<GridIndexType>::const_iterator end(gridIndicies.end());
         for ( ; iter != end; iter++ )
         {
            // convert grid index to strands index
            GridIndexType gridIdx = *iter;
            StrandIndexType index1, index2;
            fillTool.GridIndexToStrandPositionIndex(gridIdx, &index1, &index2);
            ATLASSERT(index1 != INVALID_INDEX);
            strandIndicies.push_back(index1);
            if ( index2 != INVALID_INDEX )
            {
               strandIndicies.push_back(index2);
            }
         }
         std::sort(strandIndicies.begin(),strandIndicies.end());
         config.PrestressConfig.SetExtendedStrands(strandType,endType,strandIndicies);
      }
   }

   config.PrestressConfig.TempStrandUsage = pStrands->TempStrandUsage;

   // Get concrete properties
   config.Fc       = GetSegmentFc(segmentKey,m_IntervalManager.GetLiveLoadInterval());
   config.Fci      = GetSegmentFc(segmentKey,m_IntervalManager.GetPrestressReleaseInterval());
   config.ConcType = GetSegmentConcreteType(segmentKey);
   config.bHasFct  = DoesSegmentConcreteHaveAggSplittingStrength(segmentKey);
   config.Fct      = GetSegmentConcreteAggSplittingStrength(segmentKey);


   config.Ec  = GetSegmentEc(segmentKey,m_IntervalManager.GetLiveLoadInterval());
   config.Eci = GetSegmentEc(segmentKey,m_IntervalManager.GetPrestressReleaseInterval());
   config.bUserEci = pMaterial->Concrete.bUserEci;
   config.bUserEc  = pMaterial->Concrete.bUserEc;

   // Slab offset
   config.SlabOffset[pgsTypes::metStart] = GetSlabOffset(segmentKey,pgsTypes::metStart);
   config.SlabOffset[pgsTypes::metEnd]   = GetSlabOffset(segmentKey,pgsTypes::metEnd);

   // Stirrup data
	const CShearData2* pShearData = GetShearData(segmentKey);

   WriteShearDataToStirrupConfig(pShearData, config.StirrupConfig);

   return config;
}

bool CBridgeAgentImp::DoesLeftSideEndDiaphragmLoadGirder(PierIndexType pierIdx)
{
   VALIDATE( BRIDGE );
  
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CPierData2* pPierData = pBridgeDesc->GetPier(pierIdx);
   ATLASSERT( pPierData );

   // if there isn't a span on the left hand side of this pier, then
   // the loads from the left side can't be applied
   if ( pPierData->GetPrevSpan() == NULL )
      return false;

   return (pPierData->GetDiaphragmLoadType(pgsTypes::Back) != ConnectionLibraryEntry::DontApply);
}

bool CBridgeAgentImp::DoesRightSideEndDiaphragmLoadGirder(PierIndexType pierIdx)
{
   VALIDATE( BRIDGE );
  
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CPierData2* pPierData = pBridgeDesc->GetPier(pierIdx);
   ATLASSERT( pPierData );

   // if there isn't a span on the right hand side of this pier, then
   // the loads from the right side can't be applied
   if ( pPierData->GetNextSpan() == NULL )
      return false;

   return (pPierData->GetDiaphragmLoadType(pgsTypes::Ahead) != ConnectionLibraryEntry::DontApply);
}

Float64 CBridgeAgentImp::GetEndDiaphragmLoadLocationAtStart(const CSegmentKey& segmentKey)
{
   VALIDATE( BRIDGE );

   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   SpanIndexType startSpanIdx,endSpanIdx;
   GetSpansForSegment(segmentKey,&startSpanIdx,&endSpanIdx);
   PierIndexType pierIdx = PierIndexType(startSpanIdx);
   const CPierData2* pPierData = pIBridgeDesc->GetPier(pierIdx);
   ATLASSERT( pPierData );

   Float64 dist;
   if (pPierData->GetDiaphragmLoadType(pgsTypes::Ahead) == ConnectionLibraryEntry::ApplyAtSpecifiedLocation)
   {
      // return distance adjusted for skew
      dist  = pPierData->GetDiaphragmLoadLocation(pgsTypes::Ahead);

      CComPtr<IAngle> angle;
      GetPierSkew(pierIdx,&angle);
      Float64 value;
      angle->get_Value(&value);

      dist /=  cos ( fabs(value) );
   }
   else if (pPierData->GetDiaphragmLoadType(pgsTypes::Ahead) == ConnectionLibraryEntry::ApplyAtBearingCenterline)
   {
      // same as bearing offset
      dist = GetSegmentStartBearingOffset(segmentKey);
   }
   else
   {
      CHECK(0); // end diaphragm load is not applied
      dist = 0.0;
   }

   return dist;
}

Float64 CBridgeAgentImp::GetEndDiaphragmLoadLocationAtEnd(const CSegmentKey& segmentKey)
{
   VALIDATE( BRIDGE );
  
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CGirderGroupData* pGroup = pBridgeDesc->GetGirderGroup(segmentKey.groupIndex);
   const CSplicedGirderData* pSplicedGirder = pGroup->GetGirder(segmentKey.girderIndex);
   const CPierData2* pPierData;
   pgsTypes::PierFaceType pierFace;
   if ( segmentKey.segmentIndex == 0 )
   {
      pPierData = pSplicedGirder->GetPier(pgsTypes::metStart);
      pierFace = pgsTypes::Ahead;
   }
   else
   {
      ATLASSERT( segmentKey.segmentIndex == pSplicedGirder->GetSegmentCount()-1 );
      pPierData = pSplicedGirder->GetPier(pgsTypes::metEnd);
      pierFace = pgsTypes::Back;
   }
   ATLASSERT( pPierData );

   Float64 dist;
   if (pPierData->GetDiaphragmLoadType(pierFace) == ConnectionLibraryEntry::ApplyAtSpecifiedLocation)
   {
      // return distance adjusted for skew
      dist  = pPierData->GetDiaphragmLoadLocation(pierFace);

      CComPtr<IAngle> angle;
      GetPierSkew(pPierData->GetIndex(),&angle);
      Float64 value;
      angle->get_Value(&value);

      dist /=  cos ( fabs(value) );
   }
   else if (pPierData->GetDiaphragmLoadType(pierFace) == ConnectionLibraryEntry::ApplyAtBearingCenterline)
   {
      // same as bearing offset
      dist = GetSegmentEndBearingOffset(segmentKey);
   }
   else
   {
      dist = 0.0;
   }

   return dist;
}

std::vector<IntermedateDiaphragm> CBridgeAgentImp::GetIntermediateDiaphragms(pgsTypes::DiaphragmType diaphragmType,const CSegmentKey& segmentKey)
{
#pragma Reminder("UPDATE: update diaphragm model") // update the diaphragm model to match the model in TxDOT BGS

   GET_IFACE(IGirder, pGirder);

   if ( segmentKey.groupIndex == INVALID_INDEX ) // not ready for spliced girder segments yet
      return std::vector<IntermedateDiaphragm>();

   const GirderLibraryEntry* pGirderEntry = GetGirderLibraryEntry(segmentKey);

   Float64 span_length      = GetSegmentSpanLength(            segmentKey );
   Float64 girder_length    = GetSegmentLength(                segmentKey );
   Float64 end_size         = GetSegmentStartEndDistance(      segmentKey );
   Float64 start_brg_offset = GetSegmentStartBearingOffset(    segmentKey );
   Float64 end_brg_offset   = GetSegmentEndBearingOffset(      segmentKey );
   bool   bIsInterior       = IsInteriorGirder(                segmentKey );

   // base the span length on the maximum span length in this span
   // we want the same number of diaphragms on every girder
   GirderIndexType nGirders = GetGirderCount(segmentKey.groupIndex);
   Float64 max_span_length = 0;
   for ( GirderIndexType i = 0; i < nGirders; i++ )
   {
      SegmentIndexType segIdx = 0;
      max_span_length = _cpp_max(max_span_length, GetSegmentSpanLength( CSegmentKey(segmentKey.groupIndex,i,segIdx) ));
   }

   // These are indicies into the generic bridge object. They could be piers or temporary support
   PierIndexType startPierIdx = GetGenericBridgePierIndex(segmentKey,pgsTypes::metStart);
   PierIndexType endPierIdx   = GetGenericBridgePierIndex(segmentKey,pgsTypes::metEnd);

   // get the actual generic bridge pier objects
   CComPtr<IPierCollection> piers;
   m_Bridge->get_Piers(&piers);
   CComPtr<IPier> objPier1, objPier2;
   piers->get_Item(startPierIdx,&objPier1);
   piers->get_Item(endPierIdx,  &objPier2);

   // get the skew angles
   CComPtr<IAngle> objSkew1, objSkew2;
   objPier1->get_SkewAngle(&objSkew1);
   objPier2->get_SkewAngle(&objSkew2);

   Float64 skew1, skew2;
   objSkew1->get_Value(&skew1);
   objSkew2->get_Value(&skew2);

   std::vector<IntermedateDiaphragm> diaphragms;

   const GirderLibraryEntry::DiaphragmLayoutRules& rules = pGirderEntry->GetDiaphragmLayoutRules();

   GirderLibraryEntry::DiaphragmLayoutRules::const_iterator iter;
   for ( iter = rules.begin(); iter != rules.end(); iter++ )
   {
      const GirderLibraryEntry::DiaphragmLayoutRule& rule = *iter;

      if ( max_span_length <= rule.MinSpan || rule.MaxSpan < max_span_length )
         continue; // this rule doesn't apply

      IntermedateDiaphragm diaphragm;
      if ( rule.Method == GirderLibraryEntry::dwmInput )
      {
         diaphragm.m_bCompute = false;
         diaphragm.P = rule.Weight;
      }
      else
      {
         diaphragm.m_bCompute = true;
         diaphragm.H = rule.Height;
         diaphragm.T = rule.Thickness;
      }

      // determine location of diaphragm load (from the reference point - whatever that is, see below)
      Float64 location1 = rule.Location;
      Float64 location2 = rule.Location;
      if ( rule.MeasureType == GirderLibraryEntry::mtFractionOfSpanLength )
      {
         location1 *= span_length;
         location2 = (1 - location2)*span_length;
      }
      else if ( rule.MeasureType == GirderLibraryEntry::mtFractionOfGirderLength )
      {
         location1 *= girder_length;
         location2 = (1 - location2)*girder_length;
      }

      // adjust location so that it is measured from the end of the girder
      if ( rule.MeasureLocation == GirderLibraryEntry::mlBearing )
      {
         // reference point is the bearing so add the end size
         location1 += end_size;
         location2 += end_size;
      }
      else if ( rule.MeasureLocation == GirderLibraryEntry::mlCenterlineOfGirder )
      {
         // reference point is the center line of the girder so go back from the centerline
         location1 = girder_length/2 - location1;
         location2 = girder_length/2 + location2; // locate the diaphragm -/+ from cl girder
      }

      if ( location1 < 0.0 || girder_length < location1 || location2 < 0.0 || girder_length < location2 )
      {
         GET_IFACE(IEAFStatusCenter,pStatusCenter);
         std::_tstring str(_T("An interior diaphragm is located outside of the girder length. The diaphragm load will not be applied. Check the diaphragm rules for this girder."));

         pgsInformationalStatusItem* pStatusItem = new pgsInformationalStatusItem(m_StatusGroupID,m_scidInformationalError,str.c_str());
         pStatusCenter->Add(pStatusItem);
         break;
      }

      // location the first diaphragm
      diaphragm.Location = location1;
      Float64 skew = (skew2-skew1)*location1/(start_brg_offset + span_length + end_brg_offset) + skew1;
      pgsPointOfInterest poi(segmentKey,location1);
      bool bDiaphragmAdded = false;

      Float64 left_spacing, right_spacing;
      GetSpacingAlongGirder(segmentKey,location1,&left_spacing,&right_spacing);

      if ( rule.Type == GirderLibraryEntry::dtExternal && diaphragmType == pgsTypes::dtCastInPlace )
      {
         // external diaphragms are only applied in the bridge site event
         // determine length (width) of the diaphragm
         Float64 W = 0;
         WebIndexType nWebs = pGirder->GetWebCount(segmentKey);

         Float64 web_location_left  = pGirder->GetWebLocation(poi,0);
         Float64 web_location_right = pGirder->GetWebLocation(poi,nWebs-1);

         Float64 tweb_left  = pGirder->GetWebThickness(poi,0)/2;
         Float64 tweb_right = pGirder->GetWebThickness(poi,nWebs-1)/2;

         if ( bIsInterior )
         {
            W = (left_spacing/2 + right_spacing/2 - fabs(web_location_left) - tweb_left - fabs(web_location_right) - tweb_right);
         }
         else
         {
            W = (segmentKey.girderIndex == 0 ? right_spacing/2 - fabs(web_location_right) - tweb_right : left_spacing/2 - fabs(web_location_left) - tweb_left );
         }

         diaphragm.W = W/cos(skew);

         if ( !diaphragm.m_bCompute )
         {
            // P is weight/length of external application
            // make P the total weight here
            diaphragm.P *= diaphragm.W;
         }

         diaphragms.push_back(diaphragm);
         bDiaphragmAdded = true;
      }
      else
      {
         // internal diaphragm
         if (
            (rule.Construction == GirderLibraryEntry::ctCastingYard && diaphragmType == pgsTypes::dtPrecast ) ||
            (rule.Construction == GirderLibraryEntry::ctBridgeSite  && diaphragmType == pgsTypes::dtCastInPlace )
            )
         {
            if ( diaphragm.m_bCompute )
            {
               // determine length (width) of the diaphragm
               Float64 W = 0;
               WebIndexType nWebs = pGirder->GetWebCount(segmentKey);
               if ( 1 < nWebs )
               {
	               SpacingIndexType nSpaces = nWebs-1;
	
	               // add up the spacing between the centerlines of webs in the girder cross section
	               for ( SpacingIndexType spaceIdx = 0; spaceIdx < nSpaces; spaceIdx++ )
	               {
	                  Float64 s = pGirder->GetWebSpacing(poi,spaceIdx);
	                  W += s;
	               }
	
	               // deduct the thickness of the webs
	               for ( WebIndexType webIdx = 0; webIdx < nWebs; webIdx++ )
	               {
	                  Float64 t = pGirder->GetWebThickness(poi,webIdx);
	
	                  if ( webIdx == 0 || webIdx == nWebs-1 )
	                     W -= t/2;
	                  else
	                     W -= t;
	
	               }
	
	               diaphragm.W = W/cos(skew);
	            }
            }

            diaphragms.push_back(diaphragm);
            bDiaphragmAdded = true;
         }
      }


      if ( !IsEqual(location1,location2) && bDiaphragmAdded )
      {
         // location the second diaphragm
         if ( rule.Method == GirderLibraryEntry::dwmInput )
         {
            diaphragm.m_bCompute = false;
            diaphragm.P = rule.Weight;
         }
         else
         {
            diaphragm.m_bCompute = true;
            diaphragm.H = rule.Height;
            diaphragm.T = rule.Thickness;
         }

         diaphragm.Location = location2;
         Float64 skew = (skew2-skew1)*location2/(start_brg_offset + span_length + end_brg_offset) + skew1;
         pgsPointOfInterest poi(segmentKey,location2);
         bool bDiaphragmAdded = false;

         GetSpacingAlongGirder(segmentKey,location2,&left_spacing,&right_spacing);

         if ( rule.Type == GirderLibraryEntry::dtExternal && diaphragmType == pgsTypes::dtCastInPlace )
         {
            // external diaphragms are only applied in the bridge site event
            // determine length (width) of the diaphragm
            Float64 W = 0;
            WebIndexType nWebs = pGirder->GetWebCount(segmentKey);

            Float64 web_location_left  = pGirder->GetWebLocation(poi,0);
            Float64 web_location_right = pGirder->GetWebLocation(poi,nWebs-1);

            Float64 tweb_left  = pGirder->GetWebThickness(poi,0)/2;
            Float64 tweb_right = pGirder->GetWebThickness(poi,nWebs-1)/2;

            if ( bIsInterior )
            {
               W = (left_spacing/2 + right_spacing/2 - fabs(web_location_left) - tweb_left - fabs(web_location_right) - tweb_right);
            }
            else
            {
               W = (segmentKey.girderIndex == 0 ? right_spacing/2 - fabs(web_location_right) - tweb_right : left_spacing/2 - fabs(web_location_left) - tweb_left );
            }

            diaphragm.W = W/cos(skew);

            if ( !diaphragm.m_bCompute )
            {
               // P is weight/length of external application
               // make P the total weight here
               diaphragm.P *= diaphragm.W;
            }

            diaphragms.push_back(diaphragm);
            bDiaphragmAdded = true;
         }
         else
         {
            // internal diaphragm
            if (
               (rule.Construction == GirderLibraryEntry::ctCastingYard && diaphragmType == pgsTypes::dtPrecast ) ||
               (rule.Construction == GirderLibraryEntry::ctBridgeSite  && diaphragmType == pgsTypes::dtCastInPlace )
               )
            {
               if ( diaphragm.m_bCompute )
               {
                  // determine length (width) of the diaphragm
                  Float64 W = 0;
                  WebIndexType nWebs = pGirder->GetWebCount(segmentKey);
                  if ( 1 < nWebs )
                  {
	                  SpacingIndexType nSpaces = nWebs-1;
	
	                  // add up the spacing between the centerlines of webs in the girder cross section
	                  for ( SpacingIndexType spaceIdx = 0; spaceIdx < nSpaces; spaceIdx++ )
	                  {
	                     Float64 s = pGirder->GetWebSpacing(poi,spaceIdx);
	                     W += s;
	                  }
	
	                  // deduct the thickness of the webs
	                  for ( WebIndexType webIdx = 0; webIdx < nWebs; webIdx++ )
	                  {
	                     Float64 t = pGirder->GetWebThickness(poi,webIdx);
	
	
	                     if ( webIdx == 0 || webIdx == nWebs-1 )
	                        W -= t/2;
	                     else
	                        W -= t;
	                  }
	
	                  diaphragm.W = W/cos(skew);
	               }
               }

               diaphragms.push_back(diaphragm);
               bDiaphragmAdded = true;
            }
         }
      }
   }

   return diaphragms;
}

pgsTypes::SupportedDeckType CBridgeAgentImp::GetDeckType()
{
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CDeckDescription2*   pDeck       = pBridgeDesc->GetDeckDescription();
   return pDeck->DeckType;
}

Float64 CBridgeAgentImp::GetSlabOffset(const CSegmentKey& segmentKey,pgsTypes::MemberEndType end)
{
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CGirderGroupData*    pGroup      = pBridgeDesc->GetGirderGroup(segmentKey.groupIndex);
   const CSplicedGirderData*  pGirder     = pGroup->GetGirder(segmentKey.girderIndex);
   const CPrecastSegmentData* pSegment    = pGirder->GetSegment(segmentKey.segmentIndex);

   return pSegment->GetSlabOffset(end);
}

Float64 CBridgeAgentImp::GetSlabOffset(const pgsPointOfInterest& poi)
{
   // compute the "A" dimension at the poi by linearly interpolating the value
   // between the start and end bearings (this assumes no camber in the girder)
   const CSegmentKey& segmentKey = poi.GetSegmentKey();

   Float64 dist_from_start_of_girder = poi.GetDistFromStart();

   Float64 Astart = GetSlabOffset(segmentKey,pgsTypes::metStart);
   Float64 Aend   = GetSlabOffset(segmentKey,pgsTypes::metEnd);
   Float64 span_length = GetSegmentLength(segmentKey);

   Float64 dist_to_start_brg = GetSegmentStartEndDistance(segmentKey);

   Float64 dist_from_brg_to_poi = dist_from_start_of_girder - dist_to_start_brg;

   Float64 slab_offset = ::LinInterp(dist_from_brg_to_poi,Astart,Aend,span_length);

   return slab_offset;
}

Float64 CBridgeAgentImp::GetSlabOffset(const pgsPointOfInterest& poi,const GDRCONFIG& config)
{
   // compute the "A" dimension at the poi by linearly interpolating the value
   // between the start and end bearings (this assumes no camber in the girder)
   const CSegmentKey& segmentKey = poi.GetSegmentKey();

   Float64 dist_from_start_of_girder = poi.GetDistFromStart();

   Float64 Astart = config.SlabOffset[pgsTypes::metStart];
   Float64 Aend   = config.SlabOffset[pgsTypes::metEnd];
   Float64 span_length = GetSegmentSpanLength(segmentKey);

   Float64 dist_to_start_brg = GetSegmentStartEndDistance(segmentKey);

   Float64 dist_from_brg_to_poi = dist_from_start_of_girder - dist_to_start_brg;

   Float64 slab_offset = ::LinInterp(dist_from_brg_to_poi,Astart,Aend,span_length);

   return slab_offset;
}

bool CBridgeAgentImp::IsCompositeDeck()
{
   pgsTypes::SupportedDeckType deckType = GetDeckType();

   return ( deckType == pgsTypes::sdtCompositeCIP ||
            deckType == pgsTypes::sdtCompositeSIP ||
            deckType == pgsTypes::sdtCompositeOverlay );
}

bool CBridgeAgentImp::HasOverlay()
{
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CDeckDescription2* pDeck = pBridgeDesc->GetDeckDescription();

   return (pDeck->WearingSurface == pgsTypes::wstFutureOverlay || pDeck->WearingSurface == pgsTypes::wstOverlay);
}

bool CBridgeAgentImp::IsFutureOverlay()
{
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CDeckDescription2* pDeck = pBridgeDesc->GetDeckDescription();

   if ( pDeck->WearingSurface == pgsTypes::wstSacrificialDepth )
      return false; // definately not a future overlay (not even an overlay)

   if (pDeck->WearingSurface == pgsTypes::wstFutureOverlay)
      return true; // explicitly an overlay

   ATLASSERT(pDeck->WearingSurface == pgsTypes::wstOverlay);
   IntervalIndexType overlayIntervalIdx  = GetOverlayInterval();
   IntervalIndexType liveLoadIntervalIdx = GetLiveLoadInterval();
   if ( liveLoadIntervalIdx < overlayIntervalIdx )
      return true; // overlay is applied after the bridge is open to traffic so it is a future overlay

   return false; // has an overlay, but it goes not before at the interval when the bridge opens to traffic
}

Float64 CBridgeAgentImp::GetOverlayWeight()
{
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CDeckDescription2* pDeck = pBridgeDesc->GetDeckDescription();

   return pDeck->OverlayWeight;
}

Float64 CBridgeAgentImp::GetOverlayDepth()
{
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CDeckDescription2* pDeck = pBridgeDesc->GetDeckDescription();

   Float64 depth = 0;
   if ( pDeck->bInputAsDepthAndDensity )
   {
      depth = pDeck->OverlayDepth;
   }
   else
   {
      // depth not explicitly input... estimate based on 140 pcf material
      Float64 g = unitSysUnitsMgr::GetGravitationalAcceleration();
      depth = pDeck->OverlayWeight / (g*::ConvertToSysUnits(140.0,unitMeasure::LbfPerFeet3));
   }

   return depth;
}

Float64 CBridgeAgentImp::GetFillet()
{
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CDeckDescription2* pDeck = pBridgeDesc->GetDeckDescription();
   if ( pDeck->DeckType == pgsTypes::sdtNone )
      return 0.0;
   else
      return pDeck->Fillet;
}

Float64 CBridgeAgentImp::GetGrossSlabDepth(const pgsPointOfInterest& poi)
{
   VALIDATE( BRIDGE );
   return GetGrossSlabDepth();
}

Float64 CBridgeAgentImp::GetStructuralSlabDepth(const pgsPointOfInterest& poi)
{
   VALIDATE( BRIDGE );

   if ( GetDeckType() == pgsTypes::sdtNone )
      return 0.00;

   CComPtr<IBridgeDeck> deck;
   m_Bridge->get_Deck(&deck);

   Float64 structural_depth;
   deck->get_StructuralDepth(&structural_depth);

   return structural_depth;
}

Float64 CBridgeAgentImp::GetCastSlabDepth(const pgsPointOfInterest& poi)
{
   VALIDATE( BRIDGE );
   Float64 cast_depth = GetCastDepth();
   return cast_depth;
}

Float64 CBridgeAgentImp::GetPanelDepth(const pgsPointOfInterest& poi)
{
   VALIDATE( BRIDGE );
   Float64 panel_depth = GetPanelDepth();
   return panel_depth;
}

Float64 CBridgeAgentImp::GetLeftSlabOverhang(Float64 distFromStartOfBridge)
{
   VALIDATE( BRIDGE );
   Float64 left, right;
   HRESULT hr = GetSlabOverhangs(distFromStartOfBridge,&left,&right);
   ATLASSERT(hr == S_OK);
   return left;
}

Float64 CBridgeAgentImp::GetRightSlabOverhang(Float64 distFromStartOfBridge)
{
   VALIDATE( BRIDGE );
   Float64 left, right;
   HRESULT hr = GetSlabOverhangs(distFromStartOfBridge,&left,&right);
   ATLASSERT(hr == S_OK);
   return right;
}

Float64 CBridgeAgentImp::GetLeftSlabOverhang(SpanIndexType spanIdx,Float64 distFromStartOfSpan)
{
   VALIDATE( BRIDGE );
   GroupIndexType grpIdx = GetGirderGroupIndex(spanIdx);
   CSegmentKey segmentKey(grpIdx,0,0);
   Float64 distFromStartOfBridge = GetDistanceFromStartOfBridge(segmentKey,distFromStartOfSpan);
   return GetLeftSlabOverhang(distFromStartOfBridge);
}

Float64 CBridgeAgentImp::GetRightSlabOverhang(SpanIndexType spanIdx,Float64 distFromStartOfSpan)
{
   VALIDATE( BRIDGE );
   GroupIndexType grpIdx = GetGirderGroupIndex(spanIdx);
   GirderIndexType nGirders = GetGirderCount(grpIdx);
   CSegmentKey segmentKey(grpIdx,nGirders-1,0);
   Float64 distFromStartOfBridge = GetDistanceFromStartOfBridge(segmentKey,distFromStartOfSpan);
   return GetRightSlabOverhang(distFromStartOfBridge);
}

#pragma Reminder("UPDATE: remove obsolete code")
// This code is the same as GetLeftSlabOverhang() and GetRightSlabOverhang()
//
//Float64 CBridgeAgentImp::GetLeftSlabGirderOverhang(SpanIndexType spanIdx,Float64 distFromStartOfSpan)
//{
//   VALIDATE( BRIDGE );
//
//   if ( GetDeckType() == pgsTypes::sdtNone )
//      return 0.0;
//
//   GET_IFACE(IBridgeDescription,pIBridgeDesc);
//   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
//   const CSpanData2* pSpan = pBridgeDesc->GetSpan(spanIdx);
//   const CGirderGroupData* pGroup = pBridgeDesc->GetGirderGroup(pSpan);
//   GroupIndexType grpIdx = pGroup->GetGroupIndex();
//
//   GirderIDType gdrID = ::GetSuperstructureMemberID(grpIdx,0);
//
//   Float64 station = pSpan->GetPrevPier()->GetStation() + distFromStartOfSpan;
//
//   // measure normal to the alignment
//   Float64 overhang;
//   HRESULT hr = m_BridgeGeometryTool->DeckOverhang(m_Bridge,station,gdrID,NULL,qcbLeft,&overhang);
//   ATLASSERT( SUCCEEDED(hr) );
//
//   return overhang;
//}
//
//Float64 CBridgeAgentImp::GetRightSlabGirderOverhang(SpanIndexType spanIdx,Float64 distFromStartOfSpan)
//{
//   if ( GetDeckType() == pgsTypes::sdtNone )
//      return 0.0;
//
//   VALIDATE( BRIDGE );
//
//   GET_IFACE(IBridgeDescription,pIBridgeDesc);
//   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
//   const CSpanData2* pSpan = pBridgeDesc->GetSpan(spanIdx);
//   const CGirderGroupData* pGroup = pBridgeDesc->GetGirderGroup(pSpan);
//   GroupIndexType grpIdx = pGroup->GetGroupIndex();
//
//   GirderIDType gdrID = ::GetSuperstructureMemberID(grpIdx,pGroup->GetGirderCount()-1);
//   
//   Float64 station = pSpan->GetPrevPier()->GetStation() + distFromStartOfSpan;
//
//   // measure normal to the alignment
//   Float64 overhang;
//   HRESULT hr = m_BridgeGeometryTool->DeckOverhang(m_Bridge,station,gdrID,NULL,qcbRight,&overhang);
//   ATLASSERT( SUCCEEDED(hr) );
//
//   return overhang;
//}

Float64 CBridgeAgentImp::GetLeftSlabOverhang(PierIndexType pierIdx)
{
   VALIDATE( BRIDGE );

   if ( GetDeckType() == pgsTypes::sdtNone )
      return 0.0;

   pgsTypes::PierFaceType pierFace = pgsTypes::Ahead;
   if ( pierIdx == 0 )
      pierFace = pgsTypes::Ahead;
   else if ( pierIdx == GetSpanCount() )
      pierFace = pgsTypes::Back;

   GroupIndexType grpIdx = GetGirderGroupAtPier(pierIdx,pierFace);
   GirderIDType gdrID = ::GetSuperstructureMemberID(grpIdx,0);

   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CGirderGroupData* pGroup = pBridgeDesc->GetGirderGroup(grpIdx);
   Float64 start_of_group_station = pGroup->GetPier(pgsTypes::metStart)->GetStation();

   Float64 pier_station = GetPierStation(pierIdx);

   Float64 distance = pier_station - start_of_group_station;

   Float64 overhang;
   HRESULT hr = m_BridgeGeometryTool->DeckOverhangBySSMbr(m_Bridge,gdrID,distance,NULL,qcbLeft,&overhang);
   ATLASSERT( SUCCEEDED(hr) );

   return overhang;
}

Float64 CBridgeAgentImp::GetRightSlabOverhang(PierIndexType pierIdx)
{
   VALIDATE( BRIDGE );

   if ( GetDeckType() == pgsTypes::sdtNone )
      return 0.0;

   pgsTypes::PierFaceType pierFace = pgsTypes::Ahead;
   if ( pierIdx == 0 )
      pierFace = pgsTypes::Ahead;
   else if ( pierIdx == GetSpanCount() )
      pierFace = pgsTypes::Back;

   GroupIndexType grpIdx = GetGirderGroupAtPier(pierIdx,pierFace);
   GirderIDType gdrID = ::GetSuperstructureMemberID(grpIdx,0);

   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CGirderGroupData* pGroup = pBridgeDesc->GetGirderGroup(grpIdx);
   Float64 start_of_group_station = pGroup->GetPier(pgsTypes::metStart)->GetStation();

   Float64 pier_station = GetPierStation(pierIdx);

   Float64 distance = pier_station - start_of_group_station;
   
   Float64 overhang;
   HRESULT hr = m_BridgeGeometryTool->DeckOverhangBySSMbr(m_Bridge,gdrID,distance,NULL,qcbRight,&overhang);
   ATLASSERT( SUCCEEDED(hr) );

   return overhang;
}

Float64 CBridgeAgentImp::GetLeftSlabEdgeOffset(PierIndexType pierIdx)
{
   VALIDATE( BRIDGE );
   Float64 start_station = GetPierStation(0);
   Float64 pier_station  = GetPierStation(pierIdx);
   Float64 dist_from_start_of_bridge = pier_station - start_station;
   return GetLeftSlabEdgeOffset(dist_from_start_of_bridge);
}

Float64 CBridgeAgentImp::GetRightSlabEdgeOffset(PierIndexType pierIdx)
{
   VALIDATE( BRIDGE );
   Float64 start_station = GetPierStation(0);
   Float64 pier_station  = GetPierStation(pierIdx);
   Float64 dist_from_start_of_bridge = pier_station - start_station;
   return GetRightSlabEdgeOffset(dist_from_start_of_bridge);
}

Float64 CBridgeAgentImp::GetLeftSlabEdgeOffset(Float64 distFromStartOfBridge)
{
   std::map<Float64,Float64>::iterator found = m_LeftSlabEdgeOffset.find(distFromStartOfBridge);
   if ( found != m_LeftSlabEdgeOffset.end() )
      return found->second;

   VALIDATE( BRIDGE );
   Float64 station = GetPierStation(0);
   station += distFromStartOfBridge;
   Float64 offset;
   m_BridgeGeometryTool->DeckOffset(m_Bridge,station,NULL,qcbLeft,&offset);

   m_LeftSlabEdgeOffset.insert(std::make_pair(distFromStartOfBridge,offset));

   return offset;
}

Float64 CBridgeAgentImp::GetRightSlabEdgeOffset(Float64 distFromStartOfBridge)
{
   std::map<Float64,Float64>::iterator found = m_RightSlabEdgeOffset.find(distFromStartOfBridge);
   if ( found != m_RightSlabEdgeOffset.end() )
      return found->second;

   VALIDATE( BRIDGE );
   Float64 station = GetPierStation(0);
   station += distFromStartOfBridge;
   Float64 offset;
   m_BridgeGeometryTool->DeckOffset(m_Bridge,station,NULL,qcbRight,&offset);

   m_RightSlabEdgeOffset.insert(std::make_pair(distFromStartOfBridge,offset));

   return offset;
}

Float64 CBridgeAgentImp::GetLeftCurbOffset(Float64 distFromStartOfBridge)
{
   VALIDATE( BRIDGE );
   Float64 station = GetPierStation(0);
   station += distFromStartOfBridge;
   Float64 offset;
   m_BridgeGeometryTool->CurbOffset(m_Bridge,station,NULL,qcbLeft,&offset);
   return offset;
}

Float64 CBridgeAgentImp::GetRightCurbOffset(Float64 distFromStartOfBridge)
{
   VALIDATE( BRIDGE );
   Float64 station = GetPierStation(0);
   station += distFromStartOfBridge;
   Float64 offset;
   m_BridgeGeometryTool->CurbOffset(m_Bridge,station,NULL,qcbRight,&offset);
   return offset;
}

Float64 CBridgeAgentImp::GetLeftCurbOffset(SpanIndexType spanIdx,Float64 distFromStartOfSpan)
{
#pragma Reminder("UPDATE: assuming precast girder bridge")
   CSegmentKey segmentKey(spanIdx,0,0);

   Float64 distFromStartOfBridge = GetDistanceFromStartOfBridge(segmentKey,distFromStartOfSpan);
   return GetLeftCurbOffset(distFromStartOfBridge);
}

Float64 CBridgeAgentImp::GetRightCurbOffset(SpanIndexType spanIdx,Float64 distFromStartOfSpan)
{
#pragma Reminder("UPDATE: assuming precast girder bridge")

   GirderIndexType nGirders = GetGirderCount(spanIdx);
   CSegmentKey segmentKey(spanIdx,nGirders-1,0);

   Float64 distFromStartOfBridge = GetDistanceFromStartOfBridge(segmentKey,distFromStartOfSpan);
   return GetRightCurbOffset(distFromStartOfBridge);
}

Float64 CBridgeAgentImp::GetLeftCurbOffset(PierIndexType pierIdx)
{
   Float64 start_station = GetPierStation(0);
   Float64 pier_station = GetPierStation(pierIdx);
   Float64 distFromStartOfBridge = pier_station - start_station;
   return GetLeftCurbOffset(distFromStartOfBridge);
}

Float64 CBridgeAgentImp::GetRightCurbOffset(PierIndexType pierIdx)
{
   Float64 start_station = GetPierStation(0);
   Float64 pier_station = GetPierStation(pierIdx);
   Float64 distFromStartOfBridge = pier_station - start_station;
   return GetRightCurbOffset(distFromStartOfBridge);
}

Float64 CBridgeAgentImp::GetCurbToCurbWidth(const pgsPointOfInterest& poi)
{
   Float64 distFromStartOfBridge = GetDistanceFromStartOfBridge(poi);
   return GetCurbToCurbWidth(distFromStartOfBridge);
}

Float64 CBridgeAgentImp::GetCurbToCurbWidth(const CSegmentKey& segmentKey,Float64 distFromStartOfSpan)
{
   Float64 distFromStartOfBridge = GetDistanceFromStartOfBridge(segmentKey,distFromStartOfSpan);
   return GetCurbToCurbWidth(distFromStartOfBridge);
}

Float64 CBridgeAgentImp::GetCurbToCurbWidth(Float64 distFromStartOfBridge)
{
   Float64 left_offset, right_offset;
   left_offset = GetLeftCurbOffset(distFromStartOfBridge);
   right_offset = GetRightCurbOffset(distFromStartOfBridge);
   return right_offset - left_offset;
}

Float64 CBridgeAgentImp::GetLeftInteriorCurbOffset(Float64 distFromStartOfBridge)
{
   VALIDATE( BRIDGE );
   Float64 station = GetPierStation(0);
   station += distFromStartOfBridge;
   Float64 offset;
   m_BridgeGeometryTool->InteriorCurbOffset(m_Bridge,station,NULL,qcbLeft,&offset);
   return offset;
}

Float64 CBridgeAgentImp::GetRightInteriorCurbOffset(Float64 distFromStartOfBridge)
{
   VALIDATE( BRIDGE );
   Float64 station = GetPierStation(0);
   station += distFromStartOfBridge;
   Float64 offset;
   m_BridgeGeometryTool->InteriorCurbOffset(m_Bridge,station,NULL,qcbRight,&offset);
   return offset;
}

Float64 CBridgeAgentImp::GetLeftOverlayToeOffset(Float64 distFromStartOfBridge)
{
   Float64 slab_edge = GetLeftSlabEdgeOffset(distFromStartOfBridge);

   CComPtr<ISidewalkBarrier> pSwBarrier;
   m_Bridge->get_LeftBarrier(&pSwBarrier);

   Float64 toe_width;
   pSwBarrier->get_OverlayToeWidth(&toe_width);

   return slab_edge + toe_width;
}

Float64 CBridgeAgentImp::GetRightOverlayToeOffset(Float64 distFromStartOfBridge)
{
   Float64 slab_edge = GetRightSlabEdgeOffset(distFromStartOfBridge);

   CComPtr<ISidewalkBarrier> pSwBarrier;
   m_Bridge->get_RightBarrier(&pSwBarrier);

   Float64 toe_width;
   pSwBarrier->get_OverlayToeWidth(&toe_width);

   return slab_edge - toe_width;
}

Float64 CBridgeAgentImp::GetLeftOverlayToeOffset(const pgsPointOfInterest& poi)
{
   Float64 distFromStartOfBridge = GetDistanceFromStartOfBridge(poi);
   return GetLeftOverlayToeOffset(distFromStartOfBridge);
}

Float64 CBridgeAgentImp::GetRightOverlayToeOffset(const pgsPointOfInterest& poi)
{
   Float64 distFromStartOfBridge = GetDistanceFromStartOfBridge(poi);
   return GetRightOverlayToeOffset(distFromStartOfBridge);
}

void CBridgeAgentImp::GetSlabPerimeter(CollectionIndexType nPoints,IPoint2dCollection** points)
{
   VALIDATE( BRIDGE );

   CComPtr<IBridgeGeometry> geometry;
   m_Bridge->get_BridgeGeometry(&geometry);

   CComPtr<IDeckBoundary> deckBoundary;
   geometry->get_DeckBoundary(&deckBoundary);

   deckBoundary->get_Perimeter(nPoints,points);
}

void CBridgeAgentImp::GetSpanPerimeter(SpanIndexType spanIdx,CollectionIndexType nPoints,IPoint2dCollection** points)
{
   VALIDATE( BRIDGE );

   PierIndexType startPierIdx = spanIdx;
   PierIndexType endPierIdx   = startPierIdx + 1;

   CComPtr<IBridgeGeometry> geometry;
   m_Bridge->get_BridgeGeometry(&geometry);

   CComPtr<IDeckBoundary> deckBoundary;
   geometry->get_DeckBoundary(&deckBoundary);

   PierIDType startPierID = ::GetPierLineID(startPierIdx);
   PierIDType endPierID   = ::GetPierLineID(endPierIdx);
   
   deckBoundary->get_PerimeterEx(nPoints,startPierID,endPierID,points);
}

void CBridgeAgentImp::GetLeftSlabEdgePoint(Float64 station, IDirection* direction,IPoint2d** point)
{
   GetSlabEdgePoint(station,direction,qcbLeft,point);
}

void CBridgeAgentImp::GetLeftSlabEdgePoint(Float64 station, IDirection* direction,IPoint3d** point)
{
   GetSlabEdgePoint(station,direction,qcbLeft,point);
}

void CBridgeAgentImp::GetRightSlabEdgePoint(Float64 station, IDirection* direction,IPoint2d** point)
{
   GetSlabEdgePoint(station,direction,qcbRight,point);
}

void CBridgeAgentImp::GetRightSlabEdgePoint(Float64 station, IDirection* direction,IPoint3d** point)
{
   GetSlabEdgePoint(station,direction,qcbRight,point);
}

Float64 CBridgeAgentImp::GetPierStation(PierIndexType pierIdx)
{
   VALIDATE( BRIDGE );

   CComPtr<IBridgeGeometry> geometry;
   m_Bridge->get_BridgeGeometry(&geometry);

   CComPtr<IPierLine> pier;
   PierIDType pierID = ::GetPierLineID(pierIdx);
   geometry->FindPierLine( pierID ,&pier);

   CComPtr<IStation> station;
   pier->get_Station(&station);

   Float64 value;
   station->get_Value(&value);

   return value;
}


void CBridgeAgentImp::GetSlabPerimeter(SpanIndexType startSpanIdx,SpanIndexType endSpanIdx,CollectionIndexType nPoints,IPoint2dCollection** points)
{
   VALIDATE( BRIDGE );

   CComPtr<IAlignment> alignment;
   GetAlignment(&alignment);

   ASSERT( 3 <= nPoints );

   CComPtr<IPoint2dCollection> thePoints;
   thePoints.CoCreateInstance(CLSID_Point2dCollection);

   PierIndexType startPierIdx = startSpanIdx;
   PierIndexType endPierIdx   = endSpanIdx+1;

   Float64 startStation = GetPierStation(startPierIdx);
   Float64 endStation   = GetPierStation(endPierIdx);

   // If at start or end of bridge, adjust for cantilevered girders
   if ( startSpanIdx == 0 )
   {
      CComPtr<IPoint2d> pntPier1, pntEnd1, pntBrg1, pntBrg2, pntEnd2, pntPier2;
      GetSegmentEndPoints(CSegmentKey(0,0,0),&pntPier1,&pntEnd1,&pntBrg1,&pntBrg2,&pntEnd2,&pntPier2);

      Float64 leftGirderStation,offset;
      GetStationAndOffset(pntEnd1,&leftGirderStation,&offset);

      pntPier1.Release();
      pntEnd1.Release();
      pntBrg1.Release();
      pntBrg2.Release();
      pntEnd2.Release();
      pntPier2.Release();

      GirderIndexType nGirders = GetGirderCountBySpan(startSpanIdx);
      GetSegmentEndPoints(CSegmentKey(0,nGirders-1,0),&pntPier1,&pntEnd1,&pntBrg1,&pntBrg2,&pntEnd2,&pntPier2);

      Float64 rightGirderStation;
      GetStationAndOffset(pntEnd1,&rightGirderStation,&offset);

      Float64 station = (leftGirderStation+rightGirderStation)/2;
      if ( station < startStation )
         startStation = station;
   }


   SpanIndexType nSpans = GetSpanCount();
   if ( endSpanIdx == nSpans-1 )
   {
      GroupIndexType grpIdx = GetGirderGroupIndex(endSpanIdx);
      SegmentIndexType segIdx = GetSegmentCount(grpIdx,0)-1;
      CComPtr<IPoint2d> pntPier1, pntEnd1, pntBrg1, pntBrg2, pntEnd2, pntPier2;
      GetSegmentEndPoints(CSegmentKey(grpIdx,0,segIdx),&pntPier1,&pntEnd1,&pntBrg1,&pntBrg2,&pntEnd2,&pntPier2);

      Float64 leftGirderStation,offset;
      GetStationAndOffset(pntEnd2,&leftGirderStation,&offset);

      pntPier1.Release();
      pntEnd1.Release();
      pntBrg1.Release();
      pntBrg2.Release();
      pntEnd2.Release();
      pntPier2.Release();

      GirderIndexType nGirders = GetGirderCountBySpan(endSpanIdx);
      GetSegmentEndPoints(CSegmentKey(grpIdx,nGirders-1,segIdx),&pntPier1,&pntEnd1,&pntBrg1,&pntBrg2,&pntEnd2,&pntPier2);

      Float64 rightGirderStation;
      GetStationAndOffset(pntEnd2,&rightGirderStation,&offset);

      Float64 station = (leftGirderStation+rightGirderStation)/2;
      if ( endStation < station )
         endStation = station;
   }

   Float64 stationInc   = (endStation - startStation)/(nPoints-1);

   CComPtr<IDirection> startDirection, endDirection;
   GetPierDirection(startPierIdx,&startDirection);
   GetPierDirection(endPierIdx,  &endDirection);
   Float64 dirStart, dirEnd;
   startDirection->get_Value(&dirStart);
   endDirection->get_Value(&dirEnd);

   Float64 station   = startStation;

   // Locate points along right side of deck
   // Get station of deck points at first and last piers, projected normal to aligment
   CComPtr<IPoint2d> objStartPointRight, objEndPointRight;
   m_BridgeGeometryTool->DeckEdgePoint(m_Bridge,startStation,startDirection,qcbRight,&objStartPointRight);
   m_BridgeGeometryTool->DeckEdgePoint(m_Bridge,endStation,  endDirection,  qcbRight,&objEndPointRight);

   Float64 start_normal_station_right, end_normal_station_right;
   Float64 offset;

   GetStationAndOffset(objStartPointRight,&start_normal_station_right,&offset);
   GetStationAndOffset(objEndPointRight,  &end_normal_station_right,  &offset);

   // If there is a skew, the first deck edge can be before the pier station, or after it. 
   // Same for the last deck edge. We must deal with this
   thePoints->Add(objStartPointRight);

   CollectionIndexType pntIdx;
   for (pntIdx = 0; pntIdx < nPoints; pntIdx++ )
   {
      if (station > start_normal_station_right && station < end_normal_station_right)
      {
         CComPtr<IDirection> objDirection;
         alignment->Normal(CComVariant(station), &objDirection);

         CComPtr<IPoint2d> point;
         HRESULT hr = m_BridgeGeometryTool->DeckEdgePoint(m_Bridge,station,objDirection,qcbRight,&point);
         ATLASSERT( SUCCEEDED(hr) );

         thePoints->Add(point);
      }

      station   += stationInc;
   }

   thePoints->Add(objEndPointRight);

   // Locate points along left side of deck (working backwards)
   CComPtr<IPoint2d> objStartPointLeft, objEndPointLeft;
   m_BridgeGeometryTool->DeckEdgePoint(m_Bridge,startStation,startDirection,qcbLeft,&objStartPointLeft);
   m_BridgeGeometryTool->DeckEdgePoint(m_Bridge,endStation,  endDirection,  qcbLeft,&objEndPointLeft);

   Float64 start_normal_station_left, end_normal_station_left;

   GetStationAndOffset(objStartPointLeft,&start_normal_station_left,&offset);
   GetStationAndOffset(objEndPointLeft,  &end_normal_station_left,  &offset);

   thePoints->Add(objEndPointLeft);

   station   = endStation;
   for ( pntIdx = 0; pntIdx < nPoints; pntIdx++ )
   {
      if (station > start_normal_station_left && station < end_normal_station_left)
      {
         CComPtr<IDirection> objDirection;
         alignment->Normal(CComVariant(station), &objDirection);

         CComPtr<IPoint2d> point;
         HRESULT hr = m_BridgeGeometryTool->DeckEdgePoint(m_Bridge,station,objDirection,qcbLeft,&point);
         ATLASSERT( SUCCEEDED(hr) );

         thePoints->Add(point);
      }

      station   -= stationInc;
   }

   thePoints->Add(objStartPointLeft);

   (*points) = thePoints;
   (*points)->AddRef();
}

Float64 CBridgeAgentImp::GetAheadBearingStation(PierIndexType pierIdx,const CGirderKey& girderKey)
{
   // returns the station, where a line projected from the intersection of the
   // centerline of bearing and the centerline of girder, intersects the alignment
   // at a right angle
   VALIDATE( BRIDGE );

   // Get the segment that intersects this pier
   CComPtr<ISegment> segment;
   GetSegmentAtPier(pierIdx,girderKey,&segment);

   // Get the girder line for that segment
   CComPtr<IGirderLine> girderLine;
   segment->get_GirderLine(&girderLine);

   // Get the pier line
   CComPtr<IPierLine> pierLine;
   GetPierLine(pierIdx,&pierLine);

   // Get intersection of alignment and pier
   CComPtr<IPoint2d> pntPierAlignment;
   pierLine->get_AlignmentPoint(&pntPierAlignment);

   // Get the intersection of the pier line and the girder line
   CComPtr<ILine2d> clPier;
   pierLine->get_Centerline(&clPier);

   CComPtr<IPath> gdrPath;
   girderLine->get_Path(&gdrPath);

   CComPtr<IPoint2d> pntPierGirder;
   gdrPath->Intersect(clPier,pntPierAlignment,&pntPierGirder);

   // Get the offset from the CL pier to the point of bearing, measured along the CL girder
   CComPtr<IDirection> girderDirection;
   girderLine->get_Direction(&girderDirection);

   Float64 brgOffset;
   pierLine->GetBearingOffset(pfAhead,girderDirection,&brgOffset);

   // Locate the intersection of the CL girder and the point of bearing
   CComPtr<ILocate2> locate;
   m_CogoEngine->get_Locate(&locate);
   CComPtr<IPoint2d> pntBrgGirder;
   locate->ByDistDir(pntPierGirder,brgOffset,CComVariant(girderDirection),0.0,&pntBrgGirder);

   // Get station and offset for this point
   CComPtr<IStation> objStation;
   Float64 station, offset;
   CComPtr<IAlignment> alignment;
   GetAlignment(&alignment);
   alignment->Offset(pntBrgGirder,&objStation,&offset);
   objStation->get_Value(&station);

   return station;
}

Float64 CBridgeAgentImp::GetBackBearingStation(PierIndexType pierIdx,const CGirderKey& girderKey)
{
   // returns the station, where a line projected from the intersection of the
   // centerline of bearing and the centerline of girder, intersects the alignment
   // at a right angle
   VALIDATE( BRIDGE );

   // Get the segment that intersects this pier
   CComPtr<ISegment> segment;
   GetSegmentAtPier(pierIdx,girderKey,&segment);

   // Get the girder line for that segment
   CComPtr<IGirderLine> girderLine;
   segment->get_GirderLine(&girderLine);

   // Get the pier line
   CComPtr<IPierLine> pierLine;
   GetPierLine(pierIdx,&pierLine);

   // Get intersection of alignment and pier
   CComPtr<IPoint2d> pntPierAlignment;
   pierLine->get_AlignmentPoint(&pntPierAlignment);

   // Get the intersection of the pier line and the girder line
   CComPtr<ILine2d> clPier;
   pierLine->get_Centerline(&clPier);

   CComPtr<IPath> gdrPath;
   girderLine->get_Path(&gdrPath);

   CComPtr<IPoint2d> pntPierGirder;
   gdrPath->Intersect(clPier,pntPierAlignment,&pntPierGirder);

   // Get the offset from the CL pier to the point of bearing, measured along the CL girder
   CComPtr<IDirection> girderDirection;
   girderLine->get_Direction(&girderDirection);

   Float64 brgOffset;
   pierLine->GetBearingOffset(pfBack,girderDirection,&brgOffset);

   // Locate the intersection of the CL girder and the point of bearing
   CComPtr<ILocate2> locate;
   m_CogoEngine->get_Locate(&locate);
   CComPtr<IPoint2d> pntBrgGirder;
   locate->ByDistDir(pntPierGirder,-brgOffset,CComVariant(girderDirection),0.0,&pntBrgGirder);

   // Get station and offset for this point
   CComPtr<IStation> objStation;
   Float64 station, offset;
   CComPtr<IAlignment> alignment;
   GetAlignment(&alignment);
   alignment->Offset(pntBrgGirder,&objStation,&offset);
   objStation->get_Value(&station);

   return station;
}

void CBridgeAgentImp::GetPierDirection(PierIndexType pierIdx,IDirection** ppDirection)
{
   VALIDATE( BRIDGE );

   CComPtr<IBridgeGeometry> bridgeGeometry;
   m_Bridge->get_BridgeGeometry(&bridgeGeometry);

   PierIDType pierLineID = ::GetPierLineID(pierIdx);

   CComPtr<IPierLine> line;
   bridgeGeometry->FindPierLine(pierLineID,&line);

   line->get_Direction(ppDirection);
}

void CBridgeAgentImp::GetPierSkew(PierIndexType pierIdx,IAngle** ppAngle)
{
   VALIDATE( BRIDGE );

   CComPtr<IBridgeGeometry> bridgeGeometry;
   m_Bridge->get_BridgeGeometry(&bridgeGeometry);

   PierIDType pierLineID = ::GetPierLineID(pierIdx);

   CComPtr<IPierLine> line;
   bridgeGeometry->FindPierLine(pierLineID,&line);

   line->get_Skew(ppAngle);
}

void CBridgeAgentImp::GetPierPoints(PierIndexType pierIdx,IPoint2d** left,IPoint2d** alignment,IPoint2d** bridge,IPoint2d** right)
{
   VALIDATE( BRIDGE );

   CComPtr<IBridgeGeometry> geometry;
   m_Bridge->get_BridgeGeometry(&geometry);

   CComPtr<IPierLine> pierLine;
   geometry->FindPierLine(::GetPierLineID(pierIdx),&pierLine);

   pierLine->get_AlignmentPoint(alignment);
   pierLine->get_BridgePoint(bridge);
   pierLine->get_LeftPoint(left);
   pierLine->get_RightPoint(right);
}

void CBridgeAgentImp::IsContinuousAtPier(PierIndexType pierIdx,bool* pbLeft,bool* pbRight)
{
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CPierData2* pPierData = pBridgeDesc->GetPier(pierIdx);
   ATLASSERT( pPierData );

   *pbLeft  = pPierData->IsContinuous();
   *pbRight = *pbLeft;

   if ( pierIdx == 0 && *pbRight == false )
   {
      // first pier and not already continuous... see if
      // there is a cantilever as that will make it continuous
      bool bCantileverStart, bCantileverEnd;
      ModelCantilevers(CSegmentKey(0,0,0),&bCantileverStart,&bCantileverEnd);
      *pbRight = bCantileverStart;
      *pbLeft  = bCantileverStart;
   }

   if ( pierIdx == pBridgeDesc->GetPierCount()-1 && *pbLeft == false )
   {
      // last pier and not already continuous... see if
      // there is a cantilever as that will make it continuous
      GroupIndexType grpIdx = pBridgeDesc->GetGirderGroupCount()-1;
      SegmentIndexType segIdx = pBridgeDesc->GetGirderGroup(grpIdx)->GetGirder(0)->GetSegmentCount()-1;
      bool bCantileverStart, bCantileverEnd;
      ModelCantilevers(CSegmentKey(grpIdx,0,segIdx),&bCantileverStart,&bCantileverEnd);
      *pbRight = bCantileverEnd;
      *pbLeft  = bCantileverEnd;
   }
}

void CBridgeAgentImp::IsIntegralAtPier(PierIndexType pierIdx,bool* pbLeft,bool* pbRight)
{
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CPierData2* pPierData = pBridgeDesc->GetPier(pierIdx);
   ATLASSERT( pPierData );

   pPierData->IsIntegral(pbLeft,pbRight);
}

void CBridgeAgentImp::GetContinuityEventIndex(PierIndexType pierIdx,EventIndexType* pLeft,EventIndexType* pRight)
{
   VALIDATE( BRIDGE );

   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CPierData2* pPierData = pBridgeDesc->GetPier(pierIdx);
   ATLASSERT( pPierData );

   const CTimelineManager* pTimelineMgr = pBridgeDesc->GetTimelineManager();
   EventIndexType castDeckEventIdx = pTimelineMgr->GetCastDeckEventIndex();

   *pLeft  = (pPierData->GetPierConnectionType() == pgsTypes::ContinuousBeforeDeck || pPierData->GetPierConnectionType() == pgsTypes::IntegralBeforeDeck)  ? castDeckEventIdx : castDeckEventIdx+1;
   *pRight = *pLeft;
}

bool CBridgeAgentImp::GetPierLocation(PierIndexType pierIdx,const CSegmentKey& segmentKey,Float64* pDistFromStartOfSegment)
{
   CComPtr<IPoint2d> pntSupport1,pntEnd1,pntBrg1,pntBrg2,pntEnd2,pntSupport2;
   GetSegmentEndPoints(segmentKey,&pntSupport1,&pntEnd1,&pntBrg1,&pntBrg2,&pntEnd2,&pntSupport2);

   CComPtr<IPoint2d> pntPier;
   if ( !GetSegmentPierIntersection(segmentKey,pierIdx,&pntPier) )
   {
      return false;
   }

   CComPtr<IMeasure2> measure;
   m_CogoEngine->get_Measure(&measure);

   // Measure offset from pntSupport1 so that offset is in the segment coordinate system
   Float64 offset;
   measure->Distance(pntSupport1,pntPier,&offset);
   *pDistFromStartOfSegment = IsZero(offset) ? 0 : offset;

#if defined _DEBUG
   // Get the direction of a line from the end of the girder to the pier point
   // and the direction of the girder. These should be in the same direction.
   CComPtr<IDirection> dirPierPoint;
   if ( IsZero(offset) )
      measure->Direction(pntPier,pntSupport2,&dirPierPoint); // pntSupport1 and pntPier are the same location... need a different line
   else
      measure->Direction(pntSupport1,pntPier,&dirPierPoint);

   CComPtr<IDirection> dirSegment;
   measure->Direction(pntSupport1,pntSupport2,&dirSegment);

   Float64 dirSegmentValue, dirPierPointValue;
   dirSegment->get_Value(&dirSegmentValue);
   dirPierPoint->get_Value(&dirPierPointValue);

   ATLASSERT( IsEqual(dirSegmentValue,dirPierPointValue) );
#endif

 
   return true;
}

Float64 CBridgeAgentImp::GetPierLocation(PierIndexType pierIdx,GirderIndexType gdrIdx)
{
   Float64 Xgp = 0;
   GroupIndexType nGroups = GetGirderGroupCount();
   for ( GroupIndexType grpIdx = 0; grpIdx < nGroups; grpIdx++ )
   {
      GirderIndexType nGirders = GetGirderCount(grpIdx);
      GirderIndexType thisGdrIdx = min(gdrIdx,nGirders-1);
      SegmentIndexType nSegments = GetSegmentCount(grpIdx,thisGdrIdx);
      for ( SegmentIndexType segIdx = 0; segIdx < nSegments; segIdx++ )
      {
         CSegmentKey segmentKey(grpIdx,thisGdrIdx,segIdx);
         Float64 Xs;
         bool bResult = GetPierLocation(pierIdx,segmentKey,&Xs);
         if ( bResult == true )
         {
            Xgp += Xs;
            return Xgp;
         }
         else
         {
            Xgp += GetSegmentLayoutLength(segmentKey);
         }
      }
   }

   ATLASSERT(false); // should never get here
   return -99999;
}

bool CBridgeAgentImp::GetSkewAngle(Float64 station,LPCTSTR strOrientation,Float64* pSkew)
{
   VALIDATE( COGO_MODEL );

   CComPtr<IAlignment> alignment;
   GetAlignment(&alignment);

   std::_tstring orientation(strOrientation);
   std::transform(orientation.begin(),orientation.end(),orientation.begin(),(int(*)(int))std::toupper);

   if ( orientation.compare(_T("N")) == 0 || orientation.compare(_T("NORMAL")) == 0 )
   {
      // normal to alignment
      *pSkew = 0.0;
   }
   else if ( orientation[0] == _T('N') || orientation[0] == _T('S') )
   {
      // defined by bearing
      CComPtr<IDirection> brg;
      brg.CoCreateInstance(CLSID_Direction);
      HRESULT hr = brg->FromString(CComBSTR(strOrientation));
      if ( FAILED(hr) )
         return false;

      CComPtr<IDirection> normal;
      alignment->Normal(CComVariant(station),&normal);

      CComPtr<IAngle> angle;
      brg->AngleBetween(normal,&angle);

      Float64 value;
      angle->get_Value(&value);

      while ( PI_OVER_2 < value )
         value -= M_PI;

      *pSkew = value;
   }
   else
   {
      // defined by skew angle
      CComPtr<IAngle> angle;
      angle.CoCreateInstance(CLSID_Angle);
      HRESULT hr = angle->FromString(CComBSTR(strOrientation));
      if ( FAILED(hr) )
         return false;

      Float64 value;
      angle->get_Value(&value);
      *pSkew = value;
   }

   return true;
}

pgsTypes::PierConnectionType CBridgeAgentImp::GetPierConnectionType(PierIndexType pierIdx)
{
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CPierData2* pPier = pIBridgeDesc->GetPier(pierIdx);
   ATLASSERT(pPier->IsBoundaryPier());
   return pPier->GetPierConnectionType();
}

pgsTypes::PierSegmentConnectionType CBridgeAgentImp::GetSegmentConnectionType(PierIndexType pierIdx)
{
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CPierData2* pPier = pIBridgeDesc->GetPier(pierIdx);
   ATLASSERT(pPier->IsInteriorPier());
   return pPier->GetSegmentConnectionType();
}

bool CBridgeAgentImp::IsAbutment(PierIndexType pierIdx)
{
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CPierData2* pPier = pIBridgeDesc->GetPier(pierIdx);
   return pPier->IsAbutment();
}

bool CBridgeAgentImp::IsPier(PierIndexType pierIdx)
{
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CPierData2* pPier = pIBridgeDesc->GetPier(pierIdx);
   return pPier->IsPier();
}

bool CBridgeAgentImp::IsInteriorPier(PierIndexType pierIdx)
{
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CPierData2* pPier = pIBridgeDesc->GetPier(pierIdx);
   return pPier->IsInteriorPier();
}

bool CBridgeAgentImp::IsBoundaryPier(PierIndexType pierIdx)
{
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CPierData2* pPier = pIBridgeDesc->GetPier(pierIdx);
   return pPier->IsBoundaryPier();
}

bool CBridgeAgentImp::ProcessNegativeMoments(SpanIndexType spanIdx)
{
   // don't need to process negative moments if this is a simple span design
   // or if there isn't any continuity
   GET_IFACE(ISpecification,pSpec);
   pgsTypes::AnalysisType analysisType = pSpec->GetAnalysisType();
   if ( analysisType == pgsTypes::Simple )
      return false;

   PierIndexType startPier = (spanIdx == ALL_SPANS ? 0 : spanIdx);
   PierIndexType endPier   = (spanIdx == ALL_SPANS ? GetSpanCount_Private() : spanIdx+1);

   for ( PierIndexType pier = startPier; pier <= endPier; pier++ )
   {
      bool bContinuousLeft,bContinuousRight;
      IsContinuousAtPier(pier,&bContinuousLeft,&bContinuousRight);

      bool bIntegralLeft,bIntegralRight;
      IsIntegralAtPier(pier,&bIntegralLeft,&bIntegralRight);

      if ( bContinuousLeft || bContinuousRight || bIntegralLeft || bIntegralRight )
         return true;
   }

   return false;
}

void CBridgeAgentImp::GetTemporarySupportLocation(SupportIndexType tsIdx,GirderIndexType gdrIdx,SpanIndexType* pSpanIdx,Float64* pDistFromStartOfSpan)
{
   // validate the bridge geometry because we are going to use it
   VALIDATE( BRIDGE );

   CComPtr<IBridgeGeometry> geometry;
   m_Bridge->get_BridgeGeometry(&geometry);


   // Figure out which span the temporary support is in
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CTemporarySupportData* pTS = pBridgeDesc->GetTemporarySupport(tsIdx);
   const CSpanData2* pSpan = pTS->GetSpan();
   *pSpanIdx = pSpan->GetIndex();

   // get the spliced girder object
   const CGirderGroupData* pGroup = pBridgeDesc->GetGirderGroup(pSpan);
   GroupIndexType grpIdx = pGroup->GetIndex();
   const CSplicedGirderData* pGirder = pGroup->GetGirder(gdrIdx);

   // get some geometric information about the temporary support
   Float64 tsStation = pTS->GetStation();

   CComPtr<IPierLine> tsLine;
   geometry->FindPierLine(::GetTempSupportLineID(tsIdx),&tsLine);

   CComPtr<ILine2d> clTS;
   tsLine->get_Centerline(&clTS);

   CComPtr<IPoint2d> pntNearest;
   tsLine->get_AlignmentPoint(&pntNearest);

   // find where the temporary support intersects the girder line.... sum up the distances
   // to find the location from the start of the span
   Float64 location = 0;
   std::vector<std::pair<SegmentIndexType,Float64>> seg_lengths(GetSegmentLengths(*pSpanIdx,gdrIdx));
   std::vector<std::pair<SegmentIndexType,Float64>>::iterator iter(seg_lengths.begin());
   std::vector<std::pair<SegmentIndexType,Float64>>::iterator iterEnd(seg_lengths.end());
   for ( ; iter != iterEnd; iter++ )
   {
      SegmentIndexType segIdx = (*iter).first;
      Float64 seg_length = (*iter).second; // length of segment in this span

      // get the segment
      const CPrecastSegmentData* pSegment = pGirder->GetSegment(segIdx);

      // get the station boundary of the segment
      Float64 startStation, endStation;
      pSegment->GetStations(startStation,endStation);

      if ( IsEqual(tsStation,startStation) )
      {
         // Temp Support is at the start of this segment
         break;
      }

      if ( IsEqual(tsStation,endStation) )
      {
         // Temp Support is at the end of this segment. Add this segment's length
         location += seg_length;
         break;
      }

      if ( startStation < tsStation && tsStation < endStation )
      {
         // Temp support is in the middle of this segment. Determine how far it is from
         // the start of this segment to the temporary support. Add this distance to the running total

         // get the girder line
         LineIDType segID = ::GetGirderSegmentLineID(grpIdx,gdrIdx,segIdx);
         CComPtr<IGirderLine> girderLine;
         geometry->FindGirderLine(segID,&girderLine);

         // get the path (the actual geometry of the girder line)
         CComPtr<IPath> gdrPath;
         girderLine->get_Path(&gdrPath);

         // intersect the girder path and the CL of the temporar support
         CComPtr<IPoint2d> pntIntersect;
         gdrPath->Intersect(clTS,pntNearest,&pntIntersect);

         // get the distance and offset of intersection point from the start of the segment
         // dist is the distance from the start of this segment... if the segment starts in the
         // previous span this distance needs to be reduced by the length of this segment in the
         // previous span
         Float64 dist,offset;
         gdrPath->Offset(pntIntersect,&dist,&offset);
         ATLASSERT(IsZero(offset));

         Float64 length_in_prev_span = 0;
         if ( pSegment->GetSpanIndex(pgsTypes::metStart) < *pSpanIdx )
         {
            // part of this segment is in the previous span...
            Float64 true_segment_length = this->GetSegmentLayoutLength(CSegmentKey(grpIdx,gdrIdx,segIdx));
            length_in_prev_span = true_segment_length - seg_length;
         }
         dist -= length_in_prev_span;

         ATLASSERT( 0 < dist && dist < seg_length );

         // add in this distance
         location += dist;
         break;
      }

      // the temporary support doesn't intersect this segment... just add up the segment length
      // and go to the next segment
      location += seg_length;
   }

   *pDistFromStartOfSpan = location;

   ATLASSERT(*pDistFromStartOfSpan <= GetSpanLength(*pSpanIdx,gdrIdx));
}

bool CBridgeAgentImp::GetTemporarySupportLocation(SupportIndexType tsIdx,const CSegmentKey& segmentKey,Float64* pDistFromStartOfSegment)
{
   CComPtr<IPoint2d> pntSupport1,pntEnd1,pntBrg1,pntBrg2,pntEnd2,pntSupport2;
   GetSegmentEndPoints(segmentKey,&pntSupport1,&pntEnd1,&pntBrg1,&pntBrg2,&pntEnd2,&pntSupport2);

   CComPtr<IPoint2d> pntTS;
   if ( !GetSegmentTempSupportIntersection(segmentKey,tsIdx,&pntTS) )
   {
      return false;
   }

   // Measure offset from pntSupport1 so that offset is in the segment coordinate system
   pntTS->DistanceEx(pntSupport1,pDistFromStartOfSegment);
 
   return true;
}

Float64 CBridgeAgentImp::GetTemporarySupportLocation(SupportIndexType tsIdx,GirderIndexType gdrIdx)
{
   Float64 Xgp = 0;
   GroupIndexType nGroups = GetGirderGroupCount();
   for ( GroupIndexType grpIdx = 0; grpIdx < nGroups; grpIdx++ )
   {
      GirderIndexType nGirders = GetGirderCount(grpIdx);
      GirderIndexType thisGdrIdx = min(gdrIdx,nGirders-1);
      SegmentIndexType nSegments = GetSegmentCount(grpIdx,thisGdrIdx);
      for ( SegmentIndexType segIdx = 0; segIdx < nSegments; segIdx++ )
      {
         CSegmentKey segmentKey(grpIdx,thisGdrIdx,segIdx);
         Float64 Xs;
         bool bResult = GetTemporarySupportLocation(tsIdx,segmentKey,&Xs);
         if ( bResult == true )
         {
            Xgp += Xs;
            return Xgp;
         }
         else
         {
            Xgp += GetSegmentLayoutLength(segmentKey);
         }
      }
   }

   ATLASSERT(false); // should never get here
   return -99999;

}

pgsTypes::TemporarySupportType CBridgeAgentImp::GetTemporarySupportType(SupportIndexType tsIdx)
{
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CTemporarySupportData* pTS = pBridgeDesc->GetTemporarySupport(tsIdx);

   return pTS->GetSupportType();
}

pgsTypes::SegmentConnectionType CBridgeAgentImp::GetSegmentConnectionTypeAtTemporarySupport(SupportIndexType tsIdx)
{
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CTemporarySupportData* pTS = pBridgeDesc->GetTemporarySupport(tsIdx);
   return pTS->GetConnectionType();
}

void CBridgeAgentImp::GetSegmentsAtTemporarySupport(GirderIndexType gdrIdx,SupportIndexType tsIdx,CSegmentKey* pLeftSegmentKey,CSegmentKey* pRightSegmentKey)
{
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CTemporarySupportData* pTS = pBridgeDesc->GetTemporarySupport(tsIdx);
   const CClosurePourData* pClosure = pTS->GetClosurePour(gdrIdx);
   const CSpanData2* pSpan = pTS->GetSpan();
   const CGirderGroupData* pGroup = pBridgeDesc->GetGirderGroup(pSpan);

   GroupIndexType grpIdx = pGroup->GetIndex();

   if ( pClosure )
   {
      const CPrecastSegmentData* pLeftSegment = pClosure->GetLeftSegment();
      const CPrecastSegmentData* pRightSegment = pClosure->GetRightSegment();

      CSegmentKey leftSegKey(grpIdx,gdrIdx,pLeftSegment->GetIndex());
      CSegmentKey rightSegKey(grpIdx,gdrIdx,pRightSegment->GetIndex());

      *pLeftSegmentKey = leftSegKey;
      *pRightSegmentKey = rightSegKey;
   }
   else
   {
      const CSplicedGirderData* pGirder = pGroup->GetGirder(gdrIdx);
      SegmentIndexType nSegments = pGirder->GetSegmentCount();
      for ( SegmentIndexType segIdx = 0; segIdx < nSegments; segIdx++ )
      {
         const CPrecastSegmentData* pSegment = pGirder->GetSegment(segIdx);
         std::vector<const CTemporarySupportData*> tempSupports(pSegment->GetTemporarySupports());
         std::vector<const CTemporarySupportData*>::iterator tsIter(tempSupports.begin());
         std::vector<const CTemporarySupportData*>::iterator tsIterEnd(tempSupports.end());
         for ( ; tsIter != tsIterEnd; tsIter++ )
         {
            const CTemporarySupportData* pTempSupport = *tsIter;
            if ( pTS->GetIndex() == pTempSupport->GetIndex() )
            {
               CSegmentKey segKey(grpIdx,gdrIdx,segIdx);

               *pLeftSegmentKey = segKey;
               *pRightSegmentKey = segKey;

               return;
            }
         }
      }
   }
}

void CBridgeAgentImp::GetTemporarySupportDirection(SupportIndexType tsIdx,IDirection** ppDirection)
{
   VALIDATE( BRIDGE );

   CComPtr<IBridgeGeometry> bridgeGeometry;
   m_Bridge->get_BridgeGeometry(&bridgeGeometry);

   PierIDType pierLineID = ::GetTempSupportLineID(tsIdx);

   CComPtr<IPierLine> line;
   bridgeGeometry->FindPierLine(pierLineID,&line);

   line->get_Direction(ppDirection);
}

/////////////////////////////////////////////////////////////////////////
// IMaterials
Float64 CBridgeAgentImp::GetSegmentFc28(const CSegmentKey& segmentKey)
{
   Float64 time_at_casting = m_ConcreteManager.GetSegmentCastingTime(segmentKey);
   Float64 age = 28.0; // days
   Float64 t = time_at_casting + age;
   return m_ConcreteManager.GetSegmentFc(segmentKey,t);
}

Float64 CBridgeAgentImp::GetClosurePourFc28(const CSegmentKey& closureKey)
{
   Float64 time_at_casting = m_ConcreteManager.GetClosurePourCastingTime(closureKey);
   Float64 age = 28.0; // days
   Float64 t = time_at_casting + age;
   return m_ConcreteManager.GetClosurePourFc(closureKey,t);
}

Float64 CBridgeAgentImp::GetDeckFc28()
{
   Float64 time_at_casting = m_ConcreteManager.GetDeckCastingTime();
   Float64 age = 28.0; // days
   Float64 t = time_at_casting + age;
   return m_ConcreteManager.GetDeckFc(t);
}

Float64 CBridgeAgentImp::GetRailingSystemFc28(pgsTypes::TrafficBarrierOrientation orientation)
{
   Float64 time_at_casting = m_ConcreteManager.GetRailingSystemCastingTime(orientation);
   Float64 age = 28.0; // days
   Float64 t = time_at_casting + age;
   return m_ConcreteManager.GetRailingSystemFc(orientation,t);
}

Float64 CBridgeAgentImp::GetSegmentEc28(const CSegmentKey& segmentKey)
{
   Float64 time_at_casting = m_ConcreteManager.GetSegmentCastingTime(segmentKey);
   Float64 age = 28.0; // days
   Float64 t = time_at_casting + age;
   return m_ConcreteManager.GetSegmentEc(segmentKey,t);
}

Float64 CBridgeAgentImp::GetClosurePourEc28(const CSegmentKey& closureKey)
{
   Float64 time_at_casting = m_ConcreteManager.GetClosurePourCastingTime(closureKey);
   Float64 age = 28.0; // days
   Float64 t = time_at_casting + age;
   return m_ConcreteManager.GetClosurePourEc(closureKey,t);
}

Float64 CBridgeAgentImp::GetDeckEc28()
{
   Float64 time_at_casting = m_ConcreteManager.GetDeckCastingTime();
   Float64 age = 28.0; // days
   Float64 t = time_at_casting + age;
   return m_ConcreteManager.GetDeckEc(t);
}

Float64 CBridgeAgentImp::GetRailingSystemEc28(pgsTypes::TrafficBarrierOrientation orientation)
{
   Float64 time_at_casting = m_ConcreteManager.GetRailingSystemCastingTime(orientation);
   Float64 age = 28.0; // days
   Float64 t = time_at_casting + age;
   return m_ConcreteManager.GetRailingSystemEc(orientation,t);
}

Float64 CBridgeAgentImp::GetSegmentWeightDensity(const CSegmentKey& segmentKey,IntervalIndexType intervalIdx)
{
   Float64 age = GetSegmentConcreteAge(segmentKey,intervalIdx);
   if ( age < 0 )
      return 0;

   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CPrecastSegmentData* pSegment = pIBridgeDesc->GetPrecastSegmentData(segmentKey);
   return pSegment->Material.Concrete.WeightDensity;
}

Float64 CBridgeAgentImp::GetClosurePourWeightDensity(const CSegmentKey& closureKey,IntervalIndexType intervalIdx)
{
   Float64 age = GetClosurePourConcreteAge(closureKey,intervalIdx);
   if ( age < 0 )
      return 0;

   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CClosurePourData* pClosure = pIBridgeDesc->GetClosurePourData(closureKey);
   return pClosure->GetConcrete().WeightDensity;
}

Float64 CBridgeAgentImp::GetDeckWeightDensity(IntervalIndexType intervalIdx)
{
   Float64 age = GetDeckConcreteAge(intervalIdx);
   if ( age < 0 )
      return 0;


   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CDeckDescription2* pDeck = pIBridgeDesc->GetDeckDescription();
   return pDeck->Concrete.WeightDensity;
}

Float64 CBridgeAgentImp::GetRailingSystemWeightDensity(pgsTypes::TrafficBarrierOrientation orientation,IntervalIndexType intervalIdx)
{
   Float64 age = GetRailingSystemAge(orientation,intervalIdx);
   if ( age < 0 )
      return 0;


   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CRailingSystem* pRailingSystem;
   if ( orientation == pgsTypes::tboLeft )
   {
      pRailingSystem = pIBridgeDesc->GetBridgeDescription()->GetLeftRailingSystem();
   }
   else
   {
      pRailingSystem = pIBridgeDesc->GetBridgeDescription()->GetRightRailingSystem();
   }

   return pRailingSystem->Concrete.WeightDensity;
}

Float64 CBridgeAgentImp::GetSegmentConcreteAge(const CSegmentKey& segmentKey,IntervalIndexType intervalIdx)
{
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CTimelineManager* pTimelineMgr = pIBridgeDesc->GetTimelineManager();

   EventIndexType constructSegmentIdx = pTimelineMgr->GetSegmentConstructionEventIndex();
   EventIndexType eventIdx = m_IntervalManager.GetStartEvent(intervalIdx);
   if ( eventIdx < constructSegmentIdx )
   {
      ATLASSERT(false); // segments should be constructed in the first event
      return 0; // segment hasn't been cast yet
   }

   // want age at middle of interval
   Float64 middle = m_IntervalManager.GetMiddle(intervalIdx);

   const CTimelineEvent* pTimelineEvent = pTimelineMgr->GetEventByIndex(constructSegmentIdx);
   Float64 casting_day = pTimelineEvent->GetDay(); // this is the day the dekc was cast

   // age is the difference in time between the middle of the interval in question and the casting day
   Float64 age = middle - casting_day;

   return age;
}

Float64 CBridgeAgentImp::GetClosurePourConcreteAge(const CSegmentKey& closureKey,IntervalIndexType intervalIdx)
{
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CTimelineManager* pTimelineMgr = pIBridgeDesc->GetTimelineManager();

   const CClosurePourData* pClosure = pIBridgeDesc->GetClosurePourData(closureKey);
   IDType closureID = pClosure->GetID();

   EventIndexType castClosureIdx = pTimelineMgr->GetCastClosurePourEventIndex(closureID);
   EventIndexType eventIdx = m_IntervalManager.GetStartEvent(intervalIdx);
   if ( eventIdx < castClosureIdx )
   {
      return 0; // closure hasn't been cast yet
   }

   // want age at middle of interval
   Float64 middle = m_IntervalManager.GetMiddle(intervalIdx);

   const CTimelineEvent* pTimelineEvent = pTimelineMgr->GetEventByIndex(castClosureIdx);
   Float64 casting_day = pTimelineEvent->GetDay(); // this is the day the dekc was cast

   // age is the difference in time between the middle of the interval in question and the casting day
   Float64 age = middle - casting_day;

   return age;
}

Float64 CBridgeAgentImp::GetDeckConcreteAge(IntervalIndexType intervalIdx)
{
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CTimelineManager* pTimelineMgr = pIBridgeDesc->GetTimelineManager();

   // want age at middle of interval
   Float64 middle = m_IntervalManager.GetMiddle(intervalIdx);

   EventIndexType castDeckEventIdx = pTimelineMgr->GetCastDeckEventIndex();
   const CTimelineEvent* pTimelineEvent = pTimelineMgr->GetEventByIndex(castDeckEventIdx);
   Float64 casting_day = pTimelineEvent->GetDay(); // this is the day the deck was cast

   // age is the difference in time between the middle of the interval in question and the casting day
   Float64 age = middle - casting_day;

   return age;
}

Float64 CBridgeAgentImp::GetRailingSystemAge(pgsTypes::TrafficBarrierOrientation orientation,IntervalIndexType intervalIdx)
{
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CTimelineManager* pTimelineMgr = pIBridgeDesc->GetTimelineManager();

   EventIndexType railingSystemEventIdx = pTimelineMgr->GetRailingSystemLoadEventIndex();
   EventIndexType eventIdx = m_IntervalManager.GetStartEvent(intervalIdx);
   if ( eventIdx < railingSystemEventIdx )
      return 0; // railing system hasn't been cast yet

   // want age at middle of interval
   Float64 middle = m_IntervalManager.GetMiddle(intervalIdx);

   const CTimelineEvent* pTimelineEvent = pTimelineMgr->GetEventByIndex(railingSystemEventIdx);
   Float64 casting_day = pTimelineEvent->GetDay(); // this is the day the railing system was constructed

   // age is the difference in time between the middle of the interval in question and the casting day
   Float64 age = middle - casting_day;

   return age;
}

Float64 CBridgeAgentImp::GetSegmentFc(const CSegmentKey& segmentKey,IntervalIndexType intervalIdx)
{
   Float64 middle = m_IntervalManager.GetMiddle(intervalIdx);
   return m_ConcreteManager.GetSegmentFc(segmentKey,middle);
}

Float64 CBridgeAgentImp::GetClosurePourFc(const CSegmentKey& closureKey,IntervalIndexType intervalIdx)
{
   Float64 middle = m_IntervalManager.GetMiddle(intervalIdx);
   return m_ConcreteManager.GetClosurePourFc(closureKey,middle);
}

Float64 CBridgeAgentImp::GetDeckFc(IntervalIndexType intervalIdx)
{
   Float64 middle = m_IntervalManager.GetMiddle(intervalIdx);
   return m_ConcreteManager.GetDeckFc(middle);
}

Float64 CBridgeAgentImp::GetRailingSystemFc(pgsTypes::TrafficBarrierOrientation orientation,IntervalIndexType intervalIdx)
{
   Float64 middle = m_IntervalManager.GetMiddle(intervalIdx);
   return m_ConcreteManager.GetRailingSystemFc(orientation,middle);
}

Float64 CBridgeAgentImp::GetSegmentEc(const CSegmentKey& segmentKey,IntervalIndexType intervalIdx)
{
   Float64 middle = m_IntervalManager.GetMiddle(intervalIdx);
   return m_ConcreteManager.GetSegmentEc(segmentKey,middle);
}

Float64 CBridgeAgentImp::GetSegmentEc(const CSegmentKey& segmentKey,IntervalIndexType intervalIdx,Float64 trialFc,bool* pbChanged)
{
#pragma Reminder("UPDATE: this doesn't work for time step material properties")
   // need to look at the material model type and use the Ec-vs-time model for ACI209 concrete
   // If a fixed Ec is given in the model, then we need to create a Ec-vs-time model
   // with the known data point of Ec at either release of 28day, which ever is given by
   // the model
   GET_IFACE(ISegmentData,pSegmentData);
   GET_IFACE(IIntervals,pIntervals);

   const CGirderMaterial* pMaterial = pSegmentData->GetSegmentMaterial(segmentKey);

   // storage interval is the last interval for release strength
   IntervalIndexType storageIntervalIdx = pIntervals->GetStorageInterval(segmentKey);

   bool bInitial = (intervalIdx <= storageIntervalIdx ? true : false);
   Float64 E;
   if ( (bInitial && pMaterial->Concrete.bUserEci) || (!bInitial && pMaterial->Concrete.bUserEc) )
   {
      E = (bInitial ? pMaterial->Concrete.Eci : pMaterial->Concrete.Ec);
      *pbChanged = false;
   }
   else
   {
      E = lrfdConcreteUtil::ModE( trialFc, pMaterial->Concrete.StrengthDensity, false /*ignore LRFD range checks*/ );

      if ( lrfdVersionMgr::ThirdEditionWith2005Interims <= lrfdVersionMgr::GetVersion() )
      {
         E *= (pMaterial->Concrete.EcK1*pMaterial->Concrete.EcK2);
      }

      Float64 Eorig = (bInitial ? pMaterial->Concrete.Eci : pMaterial->Concrete.Ec);
      *pbChanged = IsEqual(E,Eorig) ? false : true;
   }

   return E;
}

Float64 CBridgeAgentImp::GetClosurePourEc(const CClosureKey& closureKey,IntervalIndexType intervalIdx)
{
   Float64 middle = m_IntervalManager.GetMiddle(intervalIdx);
   return m_ConcreteManager.GetClosurePourEc(closureKey,middle);
}

Float64 CBridgeAgentImp::GetClosurePourEc(const CClosureKey& closureKey,IntervalIndexType intervalIdx,Float64 trialFc,bool* pbChanged)
{
#pragma Reminder("UPDATE: this doesn't work for time step material properties")
   // need to look at the material model type and use the Ec-vs-time model for ACI209 concrete
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   GET_IFACE(IIntervals,pIntervals);

   const CSplicedGirderData* pGirder = pIBridgeDesc->GetGirder(closureKey);
   const CClosurePourData* pClosure = pGirder->GetClosurePour(closureKey.segmentIndex);
   const CConcreteMaterial& concrete(pClosure->GetConcrete());

   IntervalIndexType compositeClosureIntervalIdx = pIntervals->GetCompositeClosurePourInterval(closureKey);

   bool bInitial = (intervalIdx <= compositeClosureIntervalIdx ? true : false);
   Float64 E;
   if ( (bInitial && concrete.bUserEci) || (!bInitial && concrete.bUserEc) )
   {
      E = (bInitial ? concrete.Eci : concrete.Ec);
      *pbChanged = false;
   }
   else
   {
      E = lrfdConcreteUtil::ModE( trialFc, concrete.StrengthDensity, false /*ignore LRFD range checks*/ );

      if ( lrfdVersionMgr::ThirdEditionWith2005Interims <= lrfdVersionMgr::GetVersion() )
      {
         E *= (concrete.EcK1*concrete.EcK2);
      }

      Float64 Eorig = (bInitial ? concrete.Eci : concrete.Ec);
      *pbChanged = IsEqual(E,Eorig) ? false : true;
   }

   return E;
}

Float64 CBridgeAgentImp::GetDeckEc(IntervalIndexType intervalIdx)
{
   Float64 middle = m_IntervalManager.GetMiddle(intervalIdx);
   return m_ConcreteManager.GetDeckEc(middle);
}

Float64 CBridgeAgentImp::GetRailingSystemEc(pgsTypes::TrafficBarrierOrientation orientation,IntervalIndexType intervalIdx)
{
   Float64 middle = m_IntervalManager.GetMiddle(intervalIdx);
   return m_ConcreteManager.GetRailingSystemEc(orientation,middle);
}

Float64 CBridgeAgentImp::GetSegmentFlexureFr(const CSegmentKey& segmentKey,IntervalIndexType intervalIdx)
{
   Float64 middle = m_IntervalManager.GetMiddle(intervalIdx);
   return m_ConcreteManager.GetSegmentFlexureFr(segmentKey,middle);
}

Float64 CBridgeAgentImp::GetSegmentShearFr(const CSegmentKey& segmentKey,IntervalIndexType intervalIdx)
{
   Float64 middle = m_IntervalManager.GetMiddle(intervalIdx);
   return m_ConcreteManager.GetSegmentShearFr(segmentKey,middle);
}

Float64 CBridgeAgentImp::GetClosurePourFlexureFr(const CSegmentKey& closureKey,IntervalIndexType intervalIdx)
{
   Float64 middle = m_IntervalManager.GetMiddle(intervalIdx);
   return m_ConcreteManager.GetClosurePourFlexureFr(closureKey,middle);
}

Float64 CBridgeAgentImp::GetClosurePourShearFr(const CSegmentKey& closureKey,IntervalIndexType intervalIdx)
{
   Float64 middle = m_IntervalManager.GetMiddle(intervalIdx);
   return m_ConcreteManager.GetClosurePourShearFr(closureKey,middle);
}

Float64 CBridgeAgentImp::GetDeckFlexureFr(IntervalIndexType intervalIdx)
{
   Float64 middle = m_IntervalManager.GetMiddle(intervalIdx);
   return m_ConcreteManager.GetDeckFlexureFr(middle);
}

Float64 CBridgeAgentImp::GetDeckShearFr(IntervalIndexType intervalIdx)
{
   Float64 middle = m_IntervalManager.GetMiddle(intervalIdx);
   return m_ConcreteManager.GetDeckShearFr(middle);
}

Float64 CBridgeAgentImp::GetSegmentAgingCoefficient(const CSegmentKey& segmentKey,IntervalIndexType intervalIdx)
{
   Float64 duration = m_IntervalManager.GetDuration(intervalIdx);
   return (duration == 0.0 ? 1.0 : 0.7);
}

Float64 CBridgeAgentImp::GetClosurePourAgingCoefficient(const CSegmentKey& closureKey,IntervalIndexType intervalIdx)
{
   Float64 duration = m_IntervalManager.GetDuration(intervalIdx);
   return (duration == 0.0 ? 1.0 : 0.7);
}

Float64 CBridgeAgentImp::GetDeckAgingCoefficient(IntervalIndexType intervalIdx)
{
   Float64 duration = m_IntervalManager.GetDuration(intervalIdx);
   return (duration == 0.0 ? 1.0 : 0.7);
}

Float64 CBridgeAgentImp::GetRailingSystemAgingCoefficient(pgsTypes::TrafficBarrierOrientation orientation,IntervalIndexType intervalIdx)
{
   Float64 duration = m_IntervalManager.GetDuration(intervalIdx);
   return (duration == 0.0 ? 1.0 : 0.7);
}

Float64 CBridgeAgentImp::GetSegmentAgeAdjustedEc(const CSegmentKey& segmentKey,IntervalIndexType intervalIdx)
{
   Float64 Ec = GetSegmentEc(segmentKey,intervalIdx);
   Float64 Y  = GetSegmentCreepCoefficient(segmentKey,intervalIdx,pgsTypes::Middle,intervalIdx,pgsTypes::End);
   Float64 X  = GetSegmentAgingCoefficient(segmentKey,intervalIdx);
   Float64 Ea = Ec/(1+X*Y);
   return Ea;
}

Float64 CBridgeAgentImp::GetClosurePourAgeAdjustedEc(const CSegmentKey& closureKey,IntervalIndexType intervalIdx)
{
   Float64 Ec = GetClosurePourEc(closureKey,intervalIdx);
   Float64 Y  = GetClosurePourCreepCoefficient(closureKey,intervalIdx,pgsTypes::Middle,intervalIdx,pgsTypes::End);
   Float64 X  = GetClosurePourAgingCoefficient(closureKey,intervalIdx);
   Float64 Ea = Ec/(1+X*Y);
   return Ea;
}

Float64 CBridgeAgentImp::GetDeckAgeAdjustedEc(IntervalIndexType intervalIdx)
{
   Float64 Ec = GetDeckEc(intervalIdx);
   Float64 Y  = GetDeckCreepCoefficient(intervalIdx,pgsTypes::Middle,intervalIdx,pgsTypes::End);
   Float64 X  = GetDeckAgingCoefficient(intervalIdx);
   Float64 Ea = Ec/(1+X*Y);
   return Ea;
}

Float64 CBridgeAgentImp::GetRailingSystemAgeAdjustedEc(pgsTypes::TrafficBarrierOrientation orientation,IntervalIndexType intervalIdx)
{
   Float64 Ec = GetRailingSystemEc(orientation,intervalIdx);
   Float64 Y  = GetRailingSystemCreepCoefficient(orientation,intervalIdx,pgsTypes::Middle,intervalIdx,pgsTypes::End);
   Float64 X  = GetRailingSystemAgingCoefficient(orientation,intervalIdx);
   Float64 Ea = Ec/(1+X*Y);
   return Ea;
}

Float64 CBridgeAgentImp::GetSegmentFreeShrinkageStrain(const CSegmentKey& segmentKey,IntervalIndexType intervalIdx,pgsTypes::IntervalTimeType timeType)
{
   Float64 time = m_IntervalManager.GetTime(intervalIdx,timeType);
   return m_ConcreteManager.GetSegmentFreeShrinkageStrain(segmentKey,time);
}

Float64 CBridgeAgentImp::GetClosurePourFreeShrinkageStrain(const CSegmentKey& closureKey,IntervalIndexType intervalIdx,pgsTypes::IntervalTimeType timeType)
{
   Float64 time = m_IntervalManager.GetTime(intervalIdx,timeType);
   return m_ConcreteManager.GetClosurePourFreeShrinkageStrain(closureKey,time);
}

Float64 CBridgeAgentImp::GetDeckFreeShrinkageStrain(IntervalIndexType intervalIdx,pgsTypes::IntervalTimeType timeType)
{
   Float64 time = m_IntervalManager.GetTime(intervalIdx,timeType);
   return m_ConcreteManager.GetDeckFreeShrinkageStrain(time);
}

Float64 CBridgeAgentImp::GetRailingSystemFreeShrinakgeStrain(pgsTypes::TrafficBarrierOrientation orientation,IntervalIndexType intervalIdx,pgsTypes::IntervalTimeType timeType)
{
   Float64 time = m_IntervalManager.GetTime(intervalIdx,timeType);
   return m_ConcreteManager.GetRailingSystemFreeShrinkageStrain(orientation,time);
}

Float64 CBridgeAgentImp::GetSegmentFreeShrinkageStrain(const CSegmentKey& segmentKey,IntervalIndexType intervalIdx)
{
   return GetSegmentFreeShrinkageStrain(segmentKey,intervalIdx,pgsTypes::End) - GetSegmentFreeShrinkageStrain(segmentKey,intervalIdx,pgsTypes::Start);
}

Float64 CBridgeAgentImp::GetClosurePourFreeShrinkageStrain(const CSegmentKey& closureKey,IntervalIndexType intervalIdx)
{
   return GetClosurePourFreeShrinkageStrain(closureKey,intervalIdx,pgsTypes::End) - GetClosurePourFreeShrinkageStrain(closureKey,intervalIdx,pgsTypes::Start);
}

Float64 CBridgeAgentImp::GetDeckFreeShrinkageStrain(IntervalIndexType intervalIdx)
{
   return GetDeckFreeShrinkageStrain(intervalIdx,pgsTypes::End) - GetDeckFreeShrinkageStrain(intervalIdx,pgsTypes::Start);
}

Float64 CBridgeAgentImp::GetRailingSystemFreeShrinakgeStrain(pgsTypes::TrafficBarrierOrientation orientation,IntervalIndexType intervalIdx)
{
   return GetRailingSystemFreeShrinakgeStrain(orientation,intervalIdx,pgsTypes::End) - GetRailingSystemFreeShrinakgeStrain(orientation,intervalIdx,pgsTypes::Start);
}

Float64 CBridgeAgentImp::GetSegmentCreepCoefficient(const CSegmentKey& segmentKey,IntervalIndexType loadingIntervalIdx,pgsTypes::IntervalTimeType loadingTimeType,IntervalIndexType intervalIdx,pgsTypes::IntervalTimeType timeType)
{
   Float64 loading_time = m_IntervalManager.GetTime(loadingIntervalIdx,loadingTimeType);
   Float64 time         = m_IntervalManager.GetTime(intervalIdx,timeType);
   return m_ConcreteManager.GetSegmentCreepCoefficient(segmentKey,time,loading_time);
}

Float64 CBridgeAgentImp::GetClosurePourCreepCoefficient(const CSegmentKey& closureKey,IntervalIndexType loadingIntervalIdx,pgsTypes::IntervalTimeType loadingTimeType,IntervalIndexType intervalIdx,pgsTypes::IntervalTimeType timeType)
{
   Float64 loading_time = m_IntervalManager.GetTime(loadingIntervalIdx,loadingTimeType);
   Float64 time = m_IntervalManager.GetTime(intervalIdx,timeType);
   return m_ConcreteManager.GetClosurePourCreepCoefficient(closureKey,time,loading_time);
}

Float64 CBridgeAgentImp::GetDeckCreepCoefficient(IntervalIndexType loadingIntervalIdx,pgsTypes::IntervalTimeType loadingTimeType,IntervalIndexType intervalIdx,pgsTypes::IntervalTimeType timeType)
{
   Float64 loading_time = m_IntervalManager.GetTime(loadingIntervalIdx,loadingTimeType);
   Float64 time = m_IntervalManager.GetTime(intervalIdx,timeType);
   return m_ConcreteManager.GetDeckCreepCoefficient(time,loading_time);
}

Float64 CBridgeAgentImp::GetRailingSystemCreepCoefficient(pgsTypes::TrafficBarrierOrientation orientation,IntervalIndexType intervalIdx,pgsTypes::IntervalTimeType timeType,IntervalIndexType loadingIntervalIdx,pgsTypes::IntervalTimeType loadingTimeType)
{
   Float64 loading_time = m_IntervalManager.GetTime(loadingIntervalIdx,loadingTimeType);
   Float64 time = m_IntervalManager.GetTime(intervalIdx,timeType);
   return m_ConcreteManager.GetRailingSystemCreepCoefficient(orientation,time,loading_time);
}

pgsTypes::ConcreteType CBridgeAgentImp::GetSegmentConcreteType(const CSegmentKey& segmentKey)
{
   return m_ConcreteManager.GetSegmentConcreteType(segmentKey);
}

bool CBridgeAgentImp::DoesSegmentConcreteHaveAggSplittingStrength(const CSegmentKey& segmentKey)
{
   return m_ConcreteManager.DoesSegmentConcreteHaveAggSplittingStrength(segmentKey);
}

Float64 CBridgeAgentImp::GetSegmentConcreteAggSplittingStrength(const CSegmentKey& segmentKey)
{
   return m_ConcreteManager.GetSegmentConcreteAggSplittingStrength(segmentKey);
}

Float64 CBridgeAgentImp::GetSegmentStrengthDensity(const CSegmentKey& segmentKey)
{
   return m_ConcreteManager.GetSegmentStrengthDensity(segmentKey);
}

Float64 CBridgeAgentImp::GetSegmentMaxAggrSize(const CSegmentKey& segmentKey)
{
   return m_ConcreteManager.GetSegmentMaxAggrSize(segmentKey);
}

Float64 CBridgeAgentImp::GetSegmentEccK1(const CSegmentKey& segmentKey)
{
   return m_ConcreteManager.GetSegmentEccK1(segmentKey);
}

Float64 CBridgeAgentImp::GetSegmentEccK2(const CSegmentKey& segmentKey)
{
   return m_ConcreteManager.GetSegmentEccK2(segmentKey);
}

Float64 CBridgeAgentImp::GetSegmentCreepK1(const CSegmentKey& segmentKey)
{
   return m_ConcreteManager.GetSegmentCreepK1(segmentKey);
}

Float64 CBridgeAgentImp::GetSegmentCreepK2(const CSegmentKey& segmentKey)
{
   return m_ConcreteManager.GetSegmentCreepK2(segmentKey);
}

Float64 CBridgeAgentImp::GetSegmentShrinkageK1(const CSegmentKey& segmentKey)
{
   return m_ConcreteManager.GetSegmentShrinkageK1(segmentKey);
}

Float64 CBridgeAgentImp::GetSegmentShrinkageK2(const CSegmentKey& segmentKey)
{
   return m_ConcreteManager.GetSegmentShrinkageK2(segmentKey);
}

pgsTypes::ConcreteType CBridgeAgentImp::GetClosurePourConcreteType(const CSegmentKey& closureKey)
{
   return m_ConcreteManager.GetClosurePourConcreteType(closureKey);
}

bool CBridgeAgentImp::DoesClosurePourConcreteHaveAggSplittingStrength(const CSegmentKey& closureKey)
{
   return m_ConcreteManager.DoesClosurePourConcreteHaveAggSplittingStrength(closureKey);
}

Float64 CBridgeAgentImp::GetClosurePourConcreteAggSplittingStrength(const CSegmentKey& closureKey)
{
   return m_ConcreteManager.GetClosurePourConcreteAggSplittingStrength(closureKey);
}

Float64 CBridgeAgentImp::GetClosurePourStrengthDensity(const CSegmentKey& closureKey)
{
   return m_ConcreteManager.GetClosurePourStrengthDensity(closureKey);
}

Float64 CBridgeAgentImp::GetClosurePourMaxAggrSize(const CSegmentKey& closureKey)
{
   return m_ConcreteManager.GetClosurePourMaxAggrSize(closureKey);
}

Float64 CBridgeAgentImp::GetClosurePourEccK1(const CSegmentKey& closureKey)
{
   return m_ConcreteManager.GetClosurePourEccK1(closureKey);
}

Float64 CBridgeAgentImp::GetClosurePourEccK2(const CSegmentKey& closureKey)
{
   return m_ConcreteManager.GetClosurePourEccK2(closureKey);
}

Float64 CBridgeAgentImp::GetClosurePourCreepK1(const CSegmentKey& closureKey)
{
   return m_ConcreteManager.GetClosurePourCreepK1(closureKey);
}

Float64 CBridgeAgentImp::GetClosurePourCreepK2(const CSegmentKey& closureKey)
{
   return m_ConcreteManager.GetClosurePourCreepK2(closureKey);
}

Float64 CBridgeAgentImp::GetClosurePourShrinkageK1(const CSegmentKey& closureKey)
{
   return m_ConcreteManager.GetClosurePourShrinkageK1(closureKey);
}

Float64 CBridgeAgentImp::GetClosurePourShrinkageK2(const CSegmentKey& closureKey)
{
   return m_ConcreteManager.GetClosurePourShrinkageK2(closureKey);
}

pgsTypes::ConcreteType CBridgeAgentImp::GetDeckConcreteType()
{
   return m_ConcreteManager.GetDeckConcreteType();
}

bool CBridgeAgentImp::DoesDeckConcreteHaveAggSplittingStrength()
{
   return m_ConcreteManager.DoesDeckConcreteHaveAggSplittingStrength();
}

Float64 CBridgeAgentImp::GetDeckConcreteAggSplittingStrength()
{
   return m_ConcreteManager.GetDeckConcreteAggSplittingStrength();
}

Float64 CBridgeAgentImp::GetDeckMaxAggrSize()
{
   return m_ConcreteManager.GetDeckMaxAggrSize();
}

Float64 CBridgeAgentImp::GetDeckEccK1()
{
   return m_ConcreteManager.GetDeckEccK1();
}

Float64 CBridgeAgentImp::GetDeckEccK2()
{
   return m_ConcreteManager.GetDeckEccK2();
}

Float64 CBridgeAgentImp::GetDeckCreepK1()
{
   return m_ConcreteManager.GetDeckCreepK1();
}

Float64 CBridgeAgentImp::GetDeckCreepK2()
{
   return m_ConcreteManager.GetDeckCreepK2();
}

Float64 CBridgeAgentImp::GetDeckShrinkageK1()
{
   return m_ConcreteManager.GetDeckShrinkageK1();
}

Float64 CBridgeAgentImp::GetDeckShrinkageK2()
{
   return m_ConcreteManager.GetDeckShrinkageK2();
}

const matPsStrand* CBridgeAgentImp::GetStrandMaterial(const CSegmentKey& segmentKey,pgsTypes::StrandType strandType)
{
   GET_IFACE(ISegmentData,pSegmentData);
   return pSegmentData->GetStrandMaterial(segmentKey,strandType);
}

Float64 CBridgeAgentImp::GetStrandRelaxation(const CSegmentKey& segmentKey,Float64 t1,Float64 t2,Float64 fpso,pgsTypes::StrandType strandType)
{
#pragma Reminder("UPDATE: using dummy method for strand relaxation")
   // this is the method used in Tadros 1977 paper. use the relaxation model from AASHTO

   if ( GetNumStrands(segmentKey,strandType) == 0 )
      return 0.0; // no strands, no relaxation

   const matPsStrand* pStrand = GetStrandMaterial(segmentKey,strandType);
   Float64 fpy = pStrand->GetYieldStrength();
   Float64 K = (pStrand->GetType() == matPsStrand::LowRelaxation ? 45 : 10);
   Float64 xr = 0.7; // reduced relaxation parameter
   Float64 fr = (xr/K)*fpso*(fpso/fpy - 0.55)*log( (24*t1 + 1)/(24*t2 + 1) );
   return fr;
}

const matPsStrand* CBridgeAgentImp::GetTendonMaterial(const CGirderKey& girderKey)
{
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CPTData* pPTData = pIBridgeDesc->GetPostTensioning(girderKey);
   return pPTData->pStrand;
}

Float64 CBridgeAgentImp::GetTendonRelaxation(const CGirderKey& girderKey,DuctIndexType ductIdx,Float64 t1,Float64 t2,Float64 fpso)
{
#pragma Reminder("UPDATE: using dummy method for strand relaxation")
   // this is the method used in Tadros 1977 paper. use the relaxation model from AASHTO

   if ( GetStrandCount(girderKey,ductIdx) == 0 )
      return 0.0; // no strand, no relaxation

   const matPsStrand* pStrand = GetTendonMaterial(girderKey);
   Float64 fpy = pStrand->GetYieldStrength();
   Float64 K = (pStrand->GetType() == matPsStrand::LowRelaxation ? 45 : 10);
   Float64 xr = 0.7; // reduced relaxation parameter
   Float64 fr = (xr/K)*fpso*(fpso/fpy - 0.55)*log( (24*t1 + 1)/(24*t2 + 1) );
   return fr;
}

void CBridgeAgentImp::GetSegmentLongitudinalRebarProperties(const CSegmentKey& segmentKey,Float64* pE,Float64 *pFy,Float64* pFu)
{
   GET_IFACE(ILongitudinalRebar,pLongRebar);
   const CLongitudinalRebarData* pLRD = pLongRebar->GetSegmentLongitudinalRebarData(segmentKey);

   const matRebar* pRebar = lrfdRebarPool::GetInstance()->GetRebar(pLRD->BarType,pLRD->BarGrade,matRebar::bs3);
   *pE  = pRebar->GetE();
   *pFy = pRebar->GetYieldStrength();
   *pFu = pRebar->GetUltimateStrength();
}

void CBridgeAgentImp::GetSegmentLongitudinalRebarMaterial(const CSegmentKey& segmentKey,matRebar::Type& type,matRebar::Grade& grade)
{
   GET_IFACE(ILongitudinalRebar,pLongRebar);
   const CLongitudinalRebarData* pLRD = pLongRebar->GetSegmentLongitudinalRebarData(segmentKey);
   type = pLRD->BarType;
   grade = pLRD->BarGrade;
}

std::_tstring CBridgeAgentImp::GetSegmentLongitudinalRebarName(const CSegmentKey& segmentKey)
{
   GET_IFACE(ILongitudinalRebar,pLongRebar);
   return pLongRebar->GetSegmentLongitudinalRebarMaterial(segmentKey);
}

void CBridgeAgentImp::GetClosurePourLongitudinalRebarProperties(const CClosureKey& closureKey,Float64* pE,Float64 *pFy,Float64* pFu)
{
   GET_IFACE(ILongitudinalRebar,pLongRebar);
   const CLongitudinalRebarData* pLRD = pLongRebar->GetClosurePourLongitudinalRebarData(closureKey);

   const matRebar* pRebar = lrfdRebarPool::GetInstance()->GetRebar(pLRD->BarType,pLRD->BarGrade,matRebar::bs3);
   *pE  = pRebar->GetE();
   *pFy = pRebar->GetYieldStrength();
   *pFu = pRebar->GetUltimateStrength();
}

void CBridgeAgentImp::GetClosurePourLongitudinalRebarMaterial(const CClosureKey& closureKey,matRebar::Type& type,matRebar::Grade& grade)
{
   GET_IFACE(ILongitudinalRebar,pLongRebar);
   const CLongitudinalRebarData* pLRD = pLongRebar->GetClosurePourLongitudinalRebarData(closureKey);
   type = pLRD->BarType;
   grade = pLRD->BarGrade;
}

std::_tstring CBridgeAgentImp::GetClosurePourLongitudinalRebarName(const CClosureKey& closureKey)
{
   GET_IFACE(ILongitudinalRebar,pLongRebar);
   return pLongRebar->GetClosurePourLongitudinalRebarMaterial(closureKey);
}

void CBridgeAgentImp::GetSegmentTransverseRebarProperties(const CSegmentKey& segmentKey,Float64* pE,Float64 *pFy,Float64* pFu)
{
	GET_IFACE(IShear,pShear);
	const CShearData2* pShearData = pShear->GetSegmentShearData(segmentKey);
   const matRebar* pRebar = lrfdRebarPool::GetInstance()->GetRebar(pShearData->ShearBarType,pShearData->ShearBarGrade,matRebar::bs3);
   *pE  = pRebar->GetE();
   *pFy = pRebar->GetYieldStrength();
   *pFu = pRebar->GetUltimateStrength();

}

void CBridgeAgentImp::GetSegmentTransverseRebarMaterial(const CSegmentKey& segmentKey,matRebar::Type& type,matRebar::Grade& grade)
{
	GET_IFACE(IShear,pShear);
	const CShearData2* pShearData = pShear->GetSegmentShearData(segmentKey);
   type  = pShearData->ShearBarType;
   grade = pShearData->ShearBarGrade;
}

std::_tstring CBridgeAgentImp::GetSegmentTransverseRebarName(const CSegmentKey& segmentKey)
{
   GET_IFACE(IShear,pShear);
   return pShear->GetSegmentStirrupMaterial(segmentKey);
}

void CBridgeAgentImp::GetClosurePourTransverseRebarProperties(const CClosureKey& closureKey,Float64* pE,Float64 *pFy,Float64* pFu)
{
	GET_IFACE(IShear,pShear);
	const CShearData2* pShearData = pShear->GetClosurePourShearData(closureKey);
   const matRebar* pRebar = lrfdRebarPool::GetInstance()->GetRebar(pShearData->ShearBarType,pShearData->ShearBarGrade,matRebar::bs3);
   *pE  = pRebar->GetE();
   *pFy = pRebar->GetYieldStrength();
   *pFu = pRebar->GetUltimateStrength();

}

void CBridgeAgentImp::GetClosurePourTransverseRebarMaterial(const CClosureKey& closureKey,matRebar::Type& type,matRebar::Grade& grade)
{
	GET_IFACE(IShear,pShear);
	const CShearData2* pShearData = pShear->GetClosurePourShearData(closureKey);
   type  = pShearData->ShearBarType;
   grade = pShearData->ShearBarGrade;
}

std::_tstring CBridgeAgentImp::GetClosurePourTransverseRebarName(const CClosureKey& closureKey)
{
   GET_IFACE(IShear,pShear);
   return pShear->GetClosurePourStirrupMaterial(closureKey);
}

void CBridgeAgentImp::GetDeckRebarProperties(Float64* pE,Float64 *pFy,Float64* pFu)
{
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CDeckDescription2* pDeck = pIBridgeDesc->GetDeckDescription();
   const matRebar* pRebar = lrfdRebarPool::GetInstance()->GetRebar(pDeck->DeckRebarData.TopRebarType,pDeck->DeckRebarData.TopRebarGrade,matRebar::bs3);
   *pE  = pRebar->GetE();
   *pFy = pRebar->GetYieldStrength();
   *pFu = pRebar->GetUltimateStrength();
}

std::_tstring CBridgeAgentImp::GetDeckRebarName()
{
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CDeckDescription2* pDeck = pIBridgeDesc->GetDeckDescription();
   return lrfdRebarPool::GetMaterialName(pDeck->DeckRebarData.TopRebarType,pDeck->DeckRebarData.TopRebarGrade);
}

void CBridgeAgentImp::GetDeckRebarMaterial(matRebar::Type& type,matRebar::Grade& grade)
{
   // top and bottom mat use the same material
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CDeckDescription2* pDeck = pIBridgeDesc->GetDeckDescription();
   type = pDeck->DeckRebarData.TopRebarType;
   grade = pDeck->DeckRebarData.TopRebarGrade;
}

Float64 CBridgeAgentImp::GetNWCDensityLimit()
{
   return m_ConcreteManager.GetNWCDensityLimit();
}

Float64 CBridgeAgentImp::GetLWCDensityLimit()
{
   return m_ConcreteManager.GetLWCDensityLimit();
}

Float64 CBridgeAgentImp::GetFlexureModRupture(Float64 fc,pgsTypes::ConcreteType type)
{
   return m_ConcreteManager.GetFlexureModRupture(fc,type);
}

Float64 CBridgeAgentImp::GetFlexureFrCoefficient(const CSegmentKey& segmentKey)
{
   return m_ConcreteManager.GetFlexureFrCoefficient(segmentKey);
}

Float64 CBridgeAgentImp::GetShearModRupture(Float64 fc,pgsTypes::ConcreteType type)
{
   return m_ConcreteManager.GetShearModRupture(fc,type);
}

Float64 CBridgeAgentImp::GetShearFrCoefficient(const CSegmentKey& segmentKey)
{
   return m_ConcreteManager.GetShearFrCoefficient(segmentKey);
}

Float64 CBridgeAgentImp::GetEconc(Float64 fc,Float64 density,Float64 K1,Float64 K2)
{
   return m_ConcreteManager.GetEconc(fc,density,K1,K2);
}

/////////////////////////////////////////////////////////////////////////
// ILongRebarGeometry
void CBridgeAgentImp::GetRebars(const pgsPointOfInterest& poi,IRebarSection** rebarSection)
{
   Float64 loc = poi.GetDistFromStart();

   CComPtr<IPrecastGirder> girder;
   GetGirder(poi.GetSegmentKey(),&girder);

   CComPtr<IRebarLayout> rebar_layout;
   girder->get_RebarLayout(&rebar_layout);

   rebar_layout->CreateRebarSection(loc,rebarSection);
}

Float64 CBridgeAgentImp::GetAsBottomHalf(const pgsPointOfInterest& poi,bool bDevAdjust)
{
   return GetAsTensionSideOfGirder(poi,bDevAdjust,false);
}

Float64 CBridgeAgentImp::GetAsTopHalf(const pgsPointOfInterest& poi,bool bDevAdjust)
{
   return GetAsGirderTopHalf(poi,bDevAdjust) + GetAsDeckTopHalf(poi,bDevAdjust);
}

Float64 CBridgeAgentImp::GetAsGirderTopHalf(const pgsPointOfInterest& poi,bool bDevAdjust)
{
   return GetAsTensionSideOfGirder(poi,bDevAdjust,true);
}

Float64 CBridgeAgentImp::GetAsDeckTopHalf(const pgsPointOfInterest& poi,bool bDevAdjust)
{
   Float64 As_Top    = GetAsTopMat(poi,ILongRebarGeometry::All);
   Float64 As_Bottom = GetAsBottomMat(poi,ILongRebarGeometry::All);

   return As_Top + As_Bottom;
}

Float64 CBridgeAgentImp::GetDevLengthFactor(const CSegmentKey& segmentKey,IRebarSectionItem* rebarItem)
{
   Float64 fc = GetSegmentFc28(segmentKey);
   pgsTypes::ConcreteType type = GetSegmentConcreteType(segmentKey);
   bool isFct = DoesSegmentConcreteHaveAggSplittingStrength(segmentKey);
   Float64 fct = isFct ? GetSegmentConcreteAggSplittingStrength(segmentKey) : 0.0;

   return GetDevLengthFactor(rebarItem, type, fc, isFct, fct);
}

Float64 CBridgeAgentImp::GetDevLengthFactor(IRebarSectionItem* rebarItem, pgsTypes::ConcreteType type, Float64 fc, bool isFct, Float64 fct)
{
   Float64 fra = 1.0;

   CComPtr<IRebar> rebar;
   rebarItem->get_Rebar(&rebar);

   REBARDEVLENGTHDETAILS details = GetRebarDevelopmentLengthDetails(rebar, type, fc, isFct, fct);

   // Get distances from section cut to ends of bar
   Float64 start,end;
   rebarItem->get_LeftExtension(&start);
   rebarItem->get_RightExtension(&end);

   Float64 cut_length = min(start, end);
   if (cut_length > 0.0)
   {
      Float64 fra = cut_length/details.ldb;
      fra = min(fra, 1.0);

      return fra;
   }
   else
   {
      ATLASSERT(cut_length==0.0); // sections shouldn't be cutting bars that don't exist
      return 0.0;
   }
}

Float64 CBridgeAgentImp::GetPPRTopHalf(const pgsPointOfInterest& poi)
{
   const CSegmentKey& segmentKey = poi.GetSegmentKey();

   GET_IFACE(ILibrary,pLib);
   GET_IFACE(ISpecification, pSpec);
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( pSpec->GetSpecification().c_str() );

   Float64 As = GetAsDeckTopHalf(poi,false);
   As += GetAsGirderTopHalf(poi,false);

   Float64 Aps = GetApsTopHalf(poi,dlaNone);

   const matPsStrand* pstrand = GetStrandMaterial(segmentKey,pgsTypes::Permanent);
   CHECK(pstrand!=0);
   Float64 fps = pstrand->GetYieldStrength();
   CHECK(fps>0.0);

   Float64 E,fs,fu;
   if ( poi.HasAttribute(POI_CLOSURE) )
      GetClosurePourLongitudinalRebarProperties(segmentKey,&E,&fs,&fu);
   else
      GetSegmentLongitudinalRebarProperties(segmentKey,&E,&fs,&fu);

   Float64 denom = Aps*fps + As*fs;
   Float64 ppr;
   if ( IsZero(denom) )
      ppr = 0.0;
   else
      ppr = (Aps*fps)/denom;

   return ppr;
}

Float64 CBridgeAgentImp::GetPPRTopHalf(const pgsPointOfInterest& poi,const GDRCONFIG& config)
{
   const CSegmentKey& segmentKey = poi.GetSegmentKey();

   GET_IFACE(ILibrary,pLib);
   GET_IFACE(ISpecification, pSpec);
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( pSpec->GetSpecification().c_str() );

   Float64 As = GetAsDeckTopHalf(poi,false);
   As += GetAsGirderTopHalf(poi,false);

   Float64 Aps = GetApsTopHalf(poi,config,dlaNone);

   const matPsStrand* pstrand = GetStrandMaterial(segmentKey,pgsTypes::Permanent);
   CHECK(pstrand!=0);
   Float64 fps = pstrand->GetYieldStrength();
   CHECK(fps>0.0);

   Float64 E,fs,fu;
   if ( poi.HasAttribute(POI_CLOSURE) )
      GetClosurePourLongitudinalRebarProperties(segmentKey,&E,&fs,&fu);
   else
      GetSegmentLongitudinalRebarProperties(segmentKey,&E,&fs,&fu);

   Float64 denom = Aps*fps + As*fs;
   Float64 ppr;
   if ( IsZero(denom) )
      ppr = 0.0;
   else
      ppr = (Aps*fps)/denom;

   return ppr;
}

Float64 CBridgeAgentImp::GetPPRBottomHalf(const pgsPointOfInterest& poi)
{
   const CSegmentKey& segmentKey = poi.GetSegmentKey();

   GET_IFACE(ILibrary,pLib);
   GET_IFACE(ISpecification, pSpec);
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( pSpec->GetSpecification().c_str() );

   Float64 As = GetAsBottomHalf(poi,false);

   Float64 Aps = GetApsBottomHalf(poi,dlaNone);

   const matPsStrand* pstrand = GetStrandMaterial(segmentKey,pgsTypes::Permanent);
   CHECK(pstrand!=0);
   Float64 fps = pstrand->GetYieldStrength();
   CHECK(fps>0.0);

   Float64 E,fs,fu;
   if ( poi.HasAttribute(POI_CLOSURE) )
      GetClosurePourLongitudinalRebarProperties(segmentKey,&E,&fs,&fu);
   else
      GetSegmentLongitudinalRebarProperties(segmentKey,&E,&fs,&fu);

   Float64 denom = Aps*fps + As*fs;
   Float64 ppr;
   if ( IsZero(denom) )
      ppr = 0.0;
   else
      ppr = (Aps*fps)/denom;

   return ppr;
}

Float64 CBridgeAgentImp::GetPPRBottomHalf(const pgsPointOfInterest& poi,const GDRCONFIG& config)
{
   const CSegmentKey& segmentKey = poi.GetSegmentKey();

   GET_IFACE(ILibrary,pLib);
   GET_IFACE(ISpecification, pSpec);
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( pSpec->GetSpecification().c_str() );

   Float64 As = GetAsBottomHalf(poi,false);

   Float64 Aps = GetApsBottomHalf(poi,config,dlaNone);

   const matPsStrand* pstrand = GetStrandMaterial(segmentKey,pgsTypes::Permanent);
   CHECK(pstrand!=0);
   Float64 fps = pstrand->GetYieldStrength();
   CHECK(fps>0.0);

   Float64 E,fs,fu;
   if ( poi.HasAttribute(POI_CLOSURE) )
      GetClosurePourLongitudinalRebarProperties(segmentKey,&E,&fs,&fu);
   else
      GetSegmentLongitudinalRebarProperties(segmentKey,&E,&fs,&fu);

   Float64 denom = Aps*fps + As*fs;
   Float64 ppr;
   if ( IsZero(denom) )
      ppr = 0.0;
   else
      ppr = (Aps*fps)/denom;

   return ppr;
}
/*
REBARDEVLENGTHDETAILS CBridgeAgentImp::GetRebarDevelopmentLengthDetails(IRebar* rebar,Float64 fc)
{
   REBARDEVLENGTHDETAILS details;
   rebar->get_NominalArea(&details.Ab);
   rebar->get_NominalDiameter(&details.db);
   rebar->get_YieldStrength(&details.fy);
   details.fc = fc;

   if ( lrfdVersionMgr::GetUnits() == lrfdVersionMgr::US )
   {
      Float64 Ab = ::ConvertFromSysUnits(details.Ab,unitMeasure::Inch2);
      Float64 db = ::ConvertFromSysUnits(details.db,unitMeasure::Inch);
      Float64 fc = ::ConvertFromSysUnits(details.fc,unitMeasure::KSI);
      Float64 fy = ::ConvertFromSysUnits(details.fy,unitMeasure::KSI);
   
      details.ldb1 = 1.25*Ab*fy/sqrt(fc);
      details.ldb1 = ::ConvertToSysUnits(details.ldb1,unitMeasure::Inch);
   
      details.ldb2 = 0.4*db*fy;
      details.ldb2 = ::ConvertToSysUnits(details.ldb2,unitMeasure::Inch);

      Float64 ldb_min = ::ConvertToSysUnits(12.0,unitMeasure::Inch);

      details.ldb = Max3(details.ldb1,details.ldb2,ldb_min);
   }
   else
   {
      Float64 Ab = ::ConvertFromSysUnits(details.Ab,unitMeasure::Millimeter2);
      Float64 db = ::ConvertFromSysUnits(details.db,unitMeasure::Millimeter);
      Float64 fc = ::ConvertFromSysUnits(details.fc,unitMeasure::MPa);
      Float64 fy = ::ConvertFromSysUnits(details.fy,unitMeasure::MPa);
   
      details.ldb1 = 0.02*Ab*fy/sqrt(fc);
      details.ldb1 = ::ConvertToSysUnits(details.ldb1,unitMeasure::Millimeter);
   
      details.ldb2 = 0.06*db*fy;
      details.ldb2 = ::ConvertToSysUnits(details.ldb2,unitMeasure::Millimeter);

      Float64 ldb_min = ::ConvertToSysUnits(12.0,unitMeasure::Millimeter);

      details.ldb = Max3(details.ldb1,details.ldb2,ldb_min);
   }

   return details;
}
*/

REBARDEVLENGTHDETAILS CBridgeAgentImp::GetRebarDevelopmentLengthDetails(IRebar* rebar,pgsTypes::ConcreteType type, Float64 fc, bool isFct, Float64 Fct)
{
   CComBSTR name;
   rebar->get_Name(&name);

   Float64 Ab, db, fy;
   rebar->get_NominalArea(&Ab);
   rebar->get_NominalDiameter(&db);
   rebar->get_YieldStrength(&fy);

   return GetRebarDevelopmentLengthDetails(name, Ab, db, fy, type, fc, isFct, Fct);
}

REBARDEVLENGTHDETAILS CBridgeAgentImp::GetRebarDevelopmentLengthDetails(const CComBSTR& name, Float64 Ab, Float64 db, Float64 fy, pgsTypes::ConcreteType type, Float64 fc, bool isFct, Float64 Fct)
{
   REBARDEVLENGTHDETAILS details;
   details.Ab = Ab;
   details.db = db;
   details.fy = fy;

   details.fc = fc;

   // LRFD 5.11.2.1
   if ( lrfdVersionMgr::GetUnits() == lrfdVersionMgr::US )
   {
      Float64 Ab = ::ConvertFromSysUnits(details.Ab,unitMeasure::Inch2);
      Float64 db = ::ConvertFromSysUnits(details.db,unitMeasure::Inch);
      Float64 fc = ::ConvertFromSysUnits(details.fc,unitMeasure::KSI);
      Float64 fy = ::ConvertFromSysUnits(details.fy,unitMeasure::KSI);
   
      if (name==_T("#14"))
      {
         details.ldb1 = 2.70*fy/sqrt(fc);
         details.ldb1 = ::ConvertToSysUnits(details.ldb1,unitMeasure::Inch);
      
         details.ldb2 = 0.0;
      }
      else if (name==_T("#18"))
      {
         details.ldb1 = 3.5*fy/sqrt(fc);
         details.ldb1 = ::ConvertToSysUnits(details.ldb1,unitMeasure::Inch);
      
         details.ldb2 = 0.0;
      }
      else
      {
         details.ldb1 = 1.25*Ab*fy/sqrt(fc);
         details.ldb1 = ::ConvertToSysUnits(details.ldb1,unitMeasure::Inch);

         details.ldb2 = 0.4*db*fy;
         details.ldb2 = ::ConvertToSysUnits(details.ldb2,unitMeasure::Inch);
      }

      Float64 ldb_min = ::ConvertToSysUnits(12.0,unitMeasure::Inch);

      details.ldb = Max3(details.ldb1,details.ldb2,ldb_min);
   }
   else
   {
      Float64 Ab = ::ConvertFromSysUnits(details.Ab,unitMeasure::Millimeter2);
      Float64 db = ::ConvertFromSysUnits(details.db,unitMeasure::Millimeter);
      Float64 fc = ::ConvertFromSysUnits(details.fc,unitMeasure::MPa);
      Float64 fy = ::ConvertFromSysUnits(details.fy,unitMeasure::MPa);
   
      if (name==_T("#14"))
      {
         details.ldb1 = 25*fy/sqrt(fc);
         details.ldb1 = ::ConvertToSysUnits(details.ldb1,unitMeasure::Millimeter);
      
         details.ldb2 = 0.0;
      }
      else if (name==_T("#18"))
      {
         details.ldb1 = 34*fy/sqrt(fc);
         details.ldb1 = ::ConvertToSysUnits(details.ldb1,unitMeasure::Millimeter);
      
         details.ldb2 = 0.0;
      }
      else
      {
         details.ldb1 = 0.02*Ab*fy/sqrt(fc);
         details.ldb1 = ::ConvertToSysUnits(details.ldb1,unitMeasure::Millimeter);
      
         details.ldb2 = 0.06*db*fy;
         details.ldb2 = ::ConvertToSysUnits(details.ldb2,unitMeasure::Millimeter);
      }

      Float64 ldb_min = ::ConvertToSysUnits(12.0,unitMeasure::Millimeter);

      details.ldb = Max3(details.ldb1,details.ldb2,ldb_min);
   }

   // Compute and apply factor for LWC
   if (type==pgsTypes::Normal)
   {
      details.factor = 1.0;
   }
   else
   {
      if (isFct)
      {
         // compute factor
         Float64 fck  = ::ConvertFromSysUnits(fc,unitMeasure::KSI);
         Float64 fctk = ::ConvertFromSysUnits(Fct,unitMeasure::KSI);

         details.factor = 0.22 * sqrt(fck) / fctk;
         details.factor = min(details.factor, 1.0);
      }
      else
      {
         // fct not specified
         if (type==pgsTypes::AllLightweight)
         {
            details.factor = 1.3;
         }
         else if (type==pgsTypes::SandLightweight)
         {
            details.factor = 1.2;
         }
         else
         {
            ATLASSERT(0); // new type?
            details.factor = 1.0;
         }
      }
   }

   details.ldb *= details.factor;

   return details;
}

Float64 CBridgeAgentImp::GetCoverTopMat()
{
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CDeckDescription2* pDeck = pBridgeDesc->GetDeckDescription();
   const CDeckRebarData& rebarData = pDeck->DeckRebarData;

   return rebarData.TopCover;
}

Float64 CBridgeAgentImp::GetTopMatLocation(const pgsPointOfInterest& poi,DeckRebarType drt)
{
   return GetLocationDeckMats(poi,drt,true,false);
}

Float64 CBridgeAgentImp::GetAsTopMat(const pgsPointOfInterest& poi,ILongRebarGeometry::DeckRebarType drt)
{
   return GetAsDeckMats(poi,drt,true,false);
}

Float64 CBridgeAgentImp::GetCoverBottomMat()
{
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CDeckDescription2* pDeck = pBridgeDesc->GetDeckDescription();
   const CDeckRebarData& rebarData = pDeck->DeckRebarData;

   return rebarData.BottomCover;
}

Float64 CBridgeAgentImp::GetBottomMatLocation(const pgsPointOfInterest& poi,DeckRebarType drt)
{
   return GetLocationDeckMats(poi,drt,false,true);
}

Float64 CBridgeAgentImp::GetAsBottomMat(const pgsPointOfInterest& poi,ILongRebarGeometry::DeckRebarType drt)
{
   return GetAsDeckMats(poi,drt,false,true);
}


void CBridgeAgentImp::GetRebarLayout(const CSegmentKey& segmentKey, IRebarLayout** rebarLayout)
{
   CComPtr<IPrecastGirder> girder;
   GetGirder(segmentKey,&girder);

   CComPtr<IRebarLayout> rebar_layout;
   girder->get_RebarLayout(rebarLayout);
}

std::vector<RowIndexType> CBridgeAgentImp::CheckLongRebarGeometry(const CSegmentKey& segmentKey)
{
   std::vector<RowIndexType> rowVec;

   CComPtr<IRebarLayout> rebarLayout;
   GetRebarLayout(segmentKey, &rebarLayout);

   CollectionIndexType nRebarLayoutItems; // Same as rows in input
   rebarLayout->get_Count(&nRebarLayoutItems);
   for (CollectionIndexType rebarLayoutItemIdx = 0; rebarLayoutItemIdx < nRebarLayoutItems; rebarLayoutItemIdx++)
   {
      CComPtr<IRebarLayoutItem> layoutItem;
      rebarLayout->get_Item(rebarLayoutItemIdx, &layoutItem);

      // check at section at mid-length of bar
      Float64 startLoc, barLength2;
      layoutItem->get_Start(&startLoc);
      layoutItem->get_Length(&barLength2);
      barLength2 /= 2.0;

      pgsPointOfInterest poi(segmentKey, startLoc+barLength2);

      CComPtr<IGirderSection> gdrSection;
      GetGirderSection(poi, pgsTypes::scGirder, &gdrSection);

      CComQIPtr<IShape> girderShape(gdrSection);

      CollectionIndexType nRebarPatterns;
      layoutItem->get_Count(&nRebarPatterns);
      for (CollectionIndexType patternIdx = 0; patternIdx < nRebarPatterns; patternIdx++)
      {
         CComPtr<IRebarPattern> rebarPattern;
         layoutItem->get_Item(patternIdx, &rebarPattern);

         CollectionIndexType nBars;
         rebarPattern->get_Count(&nBars);
         for (CollectionIndexType barIdx = 0; barIdx < nBars; barIdx++)
         {
            CComPtr<IPoint2d> location;
            rebarPattern->get_Location(barLength2, barIdx, &location);

            VARIANT_BOOL isIn;
            girderShape->PointInShape(location, &isIn);
            if(isIn != VARIANT_TRUE)
            {
               rowVec.push_back(barIdx);
               break; // Only need one fail for a row
            }
         }
      }
   }

   return rowVec;
}

/////////////////////////////////////////////////////////////////////////
// IStirrupGeometry
bool CBridgeAgentImp::AreStirrupZonesSymmetrical(const CSegmentKey& segmentKey)
{
   const CShearData2* pShearData = GetShearData(segmentKey);
   return pShearData->bAreZonesSymmetrical;
}

ZoneIndexType CBridgeAgentImp::GetNumPrimaryZones(const CSegmentKey& segmentKey)
{
   const CShearData2* pShearData = GetShearData(segmentKey);
   ZoneIndexType nZones = pShearData->ShearZones.size();

   if (nZones == 0)
   {
      return 0;
   }
   else
   {
      // determine the actual number of zones within the girder
	   GET_IFACE(IBridge,pBridge);
      if (pShearData->bAreZonesSymmetrical)
      {
         Float64 half_segment_length = pBridge->GetSegmentLength(segmentKey)/2.0;

         Float64 end_of_zone = 0.0;
         for (ZoneIndexType zoneIdx = 0; zoneIdx < nZones; zoneIdx++)
         {
            end_of_zone += pShearData->ShearZones[zoneIdx].ZoneLength;
            if (half_segment_length < end_of_zone)
               return 2*(zoneIdx+1)-1;
         }
         return nZones*2-1;
      }
      else
      {
         Float64 segment_length = pBridge->GetSegmentLength(segmentKey);

         Float64 end_of_zone = 0.0;
         for (ZoneIndexType zoneIdx = 0; zoneIdx < nZones; zoneIdx++)
         {
            end_of_zone += pShearData->ShearZones[zoneIdx].ZoneLength;
            if (segment_length < end_of_zone)
               return (zoneIdx+1);
         }

         return nZones;
      }
   }
}

void CBridgeAgentImp::GetPrimaryZoneBounds(const CSegmentKey& segmentKey, ZoneIndexType zone, Float64* start, Float64* end)
{
   const CShearData2* pShearData = GetShearData(segmentKey);
   Float64 segment_length = GetSegmentLength(segmentKey);

   if(pShearData->bAreZonesSymmetrical)
   {
      ZoneIndexType nz = GetNumPrimaryZones(segmentKey);
      CHECK(zone<nz);

      ZoneIndexType idx = GetPrimaryZoneIndex(segmentKey,pShearData,zone);

      // determine which side of girder zone is on
      enum side {OnLeft, OverCenter, OnRight} zside;
      if (zone == (nz-1)/2)
         zside = OverCenter;
      else if (idx==zone)
         zside = OnLeft;
      else
         zside = OnRight;

      ZoneIndexType zsiz = pShearData->ShearZones.size();
      Float64 l_end=0.0;
      Float64 l_start=0.0;
      for (ZoneIndexType i=0; i<=idx; i++)
      {
         if (i==zsiz-1)
            l_end+= BIG_LENGTH; // last zone in infinitely long
         else
            l_end+= pShearData->ShearZones[i].ZoneLength;

         if (l_end>=segment_length/2.0)
         {
            CHECK(i==idx);  // better be last one or too many zones
         }

         if (i!=idx)
            l_start = l_end;
      }

      if (zside == OnLeft)
      {
         *start = l_start;
         *end =   l_end;
      }
      else if (zside == OverCenter)
      {
         *start = l_start;
         *end =   segment_length - l_start;
      }
      else if (zside == OnRight)
      {
         *start = segment_length - l_end;
         *end =   segment_length - l_start;
      }
   }
   else
   {
      // Non-symmetrical zones
      ZoneIndexType idx = GetPrimaryZoneIndex(segmentKey,pShearData,zone);
      ZoneIndexType zsiz = pShearData->ShearZones.size();

      Float64 l_end=0.0;
      Float64 l_start=0.0;
      for (ZoneIndexType i=0; i<=idx; i++)
      {
         if (i==zsiz-1)
            l_end = segment_length; // last zone goes to end of girder
         else
            l_end+= pShearData->ShearZones[i].ZoneLength;

         if (l_end>=segment_length)
         {
            l_end = segment_length;
            break;
         }

         if (i!=idx)
            l_start = l_end;
      }

      *start = l_start;
      *end =   l_end;
   }
}

void CBridgeAgentImp::GetPrimaryVertStirrupBarInfo(const CSegmentKey& segmentKey,ZoneIndexType zone, matRebar::Size* pSize, Float64* pCount, Float64* pSpacing)
{
   const CShearData2* pShearData = GetShearData(segmentKey);
   ZoneIndexType idx = GetPrimaryZoneIndex(segmentKey,pShearData, zone);
   const CShearZoneData2& rzone = pShearData->ShearZones[idx];

   *pSize    = rzone.VertBarSize;
   *pCount   = rzone.nVertBars;
   *pSpacing = rzone.BarSpacing;
}

Float64 CBridgeAgentImp::GetPrimaryHorizInterfaceBarCount(const CSegmentKey& segmentKey,ZoneIndexType zone)
{
   const CShearData2* pShearData = GetShearData(segmentKey);
   ZoneIndexType idx = GetPrimaryZoneIndex(segmentKey,pShearData, zone);
   const CShearZoneData2& rzone = pShearData->ShearZones[idx];

   return rzone.nHorzInterfaceBars;
}

matRebar::Size CBridgeAgentImp::GetPrimaryConfinementBarSize(const CSegmentKey& segmentKey,ZoneIndexType zone)
{
   const CShearData2* pShearData = GetShearData(segmentKey);
   ZoneIndexType idx = GetPrimaryZoneIndex(segmentKey,pShearData, zone);
   const CShearZoneData2& rzone = pShearData->ShearZones[idx];

   return rzone.ConfinementBarSize;
}

ZoneIndexType CBridgeAgentImp::GetNumHorizInterfaceZones(const CSegmentKey& segmentKey)
{
   const CShearData2* pShearData = GetShearData(segmentKey);
   ZoneIndexType nZones = pShearData->HorizontalInterfaceZones.size();
   if (nZones == 0)
   {
      return 0;
   }
   else
   {
      // determine the actual number of zones within the girder
	   GET_IFACE(IBridge,pBridge);
      if (pShearData->bAreZonesSymmetrical)
      {
         Float64 half_segment_length = pBridge->GetSegmentLength(segmentKey)/2.0;

         Float64 end_of_zone = 0.0;
         for (ZoneIndexType zoneIdx = 0; zoneIdx < nZones; zoneIdx++)
         {
            end_of_zone += pShearData->HorizontalInterfaceZones[zoneIdx].ZoneLength;
            if (half_segment_length < end_of_zone)
               return 2*(zoneIdx+1)-1;
         }
         return nZones*2-1;
      }
      else
      {
         Float64 segment_length = pBridge->GetSegmentLength(segmentKey);

         Float64 end_of_zone = 0.0;
         for (ZoneIndexType zoneIdx = 0; zoneIdx < nZones; zoneIdx++)
         {
            end_of_zone += pShearData->HorizontalInterfaceZones[zoneIdx].ZoneLength;
            if (segment_length < end_of_zone)
               return (zoneIdx+1);
         }

         return nZones;
      }
   }
}

void CBridgeAgentImp::GetHorizInterfaceZoneBounds(const CSegmentKey& segmentKey, ZoneIndexType zone, Float64* start, Float64* end)
{
   const CShearData2* pShearData = GetShearData(segmentKey);
   Float64 segment_length = GetSegmentLength(segmentKey);

   if(pShearData->bAreZonesSymmetrical)
   {
      ZoneIndexType nz = GetNumHorizInterfaceZones(segmentKey);
      CHECK(nz>zone);

      ZoneIndexType idx = GetHorizInterfaceZoneIndex(segmentKey,pShearData,zone);

      // determine which side of girder zone is on
      enum side {OnLeft, OverCenter, OnRight} zside;
      if (zone == (nz-1)/2)
         zside = OverCenter;
      else if (idx==zone)
         zside = OnLeft;
      else
         zside = OnRight;

      ZoneIndexType zsiz = pShearData->HorizontalInterfaceZones.size();
      Float64 l_end=0.0;
      Float64 l_start=0.0;
      for (ZoneIndexType i=0; i<=idx; i++)
      {
         if (i==zsiz-1)
            l_end+= BIG_LENGTH; // last zone in infinitely long
         else
            l_end+= pShearData->HorizontalInterfaceZones[i].ZoneLength;

         if (l_end >= segment_length/2.0)
         {
            CHECK(i==idx);  // better be last one or too many zones
         }

         if (i!=idx)
            l_start = l_end;
      }

      if (zside == OnLeft)
      {
         *start = l_start;
         *end =   l_end;
      }
      else if (zside == OverCenter)
      {
         *start = l_start;
         *end =   segment_length - l_start;
      }
      else if (zside == OnRight)
      {
         *start = segment_length - l_end;
         *end =   segment_length - l_start;
      }
   }
   else
   {
      // Non-symmetrical zones
      ZoneIndexType idx = GetHorizInterfaceZoneIndex(segmentKey,pShearData,zone);
      ZoneIndexType zsiz = pShearData->HorizontalInterfaceZones.size();

      Float64 l_end=0.0;
      Float64 l_start=0.0;
      for (ZoneIndexType i=0; i<=idx; i++)
      {
         if (i==zsiz-1)
            l_end = segment_length; // last zone goes to end of girder
         else
            l_end+= pShearData->HorizontalInterfaceZones[i].ZoneLength;

         if (l_end >= segment_length)
         {
            l_end = segment_length;
            break;
         }

         if (i!=idx)
            l_start = l_end;
      }

      *start = l_start;
      *end =   l_end;
   }
}

void CBridgeAgentImp::GetHorizInterfaceBarInfo(const CSegmentKey& segmentKey,ZoneIndexType zone, matRebar::Size* pSize, Float64* pCount, Float64* pSpacing)
{
   const CShearData2* pShearData = GetShearData(segmentKey);
   ZoneIndexType idx = GetHorizInterfaceZoneIndex(segmentKey,pShearData,zone);

   const CHorizontalInterfaceZoneData& rdata = pShearData->HorizontalInterfaceZones[idx];
   *pSize = rdata.BarSize;
   *pCount = rdata.nBars;
   *pSpacing = rdata.BarSpacing;
}

void CBridgeAgentImp::GetAddSplittingBarInfo(const CSegmentKey& segmentKey, matRebar::Size* pSize, Float64* pZoneLength, Float64* pnBars, Float64* pSpacing)
{
   const CShearData2* pShearData = GetShearData(segmentKey);
   *pSize = pShearData->SplittingBarSize;
   if (*pSize!=matRebar::bsNone)
   {
      *pZoneLength = pShearData->SplittingZoneLength;
      *pnBars = pShearData->nSplittingBars;
      *pSpacing = pShearData->SplittingBarSpacing;
   }
   else
   {
      *pZoneLength = 0.0;
      *pnBars = 0.0;
      *pSpacing = 0.0;
   }
}

void CBridgeAgentImp::GetAddConfinementBarInfo(const CSegmentKey& segmentKey, matRebar::Size* pSize, Float64* pZoneLength, Float64* pSpacing)
{
   const CShearData2* pShearData = GetShearData(segmentKey);
   *pSize = pShearData->ConfinementBarSize;
   if (*pSize!=matRebar::bsNone)
   {
      *pZoneLength = pShearData->ConfinementZoneLength;
      *pSpacing = pShearData->ConfinementBarSpacing;
   }
   else
   {
      *pZoneLength = 0.0;
      *pSpacing = 0.0;
   }
}


Float64 CBridgeAgentImp::GetVertStirrupBarNominalDiameter(const pgsPointOfInterest& poi)
{
   const CShearData2* pShearData = GetShearData(poi.GetSegmentKey());
   
   const CShearZoneData2* pShearZoneData = GetPrimaryShearZoneDataAtPoi(poi, pShearData);

   matRebar::Size barSize = pShearZoneData->VertBarSize;
   if ( barSize!=matRebar::bsNone && !IsZero(pShearZoneData->BarSpacing) )
   {
      lrfdRebarPool* prp = lrfdRebarPool::GetInstance();
      const matRebar* pRebar = prp->GetRebar(pShearData->ShearBarType,pShearData->ShearBarGrade,barSize);

      return (pRebar ? pRebar->GetNominalDimension() : 0.0);
   }
   else
   {
      return 0.0;
   }
}

Float64 CBridgeAgentImp::GetAlpha(const pgsPointOfInterest& poi)
{
   return ::ConvertToSysUnits(90.,unitMeasure::Degree);
}

Float64 CBridgeAgentImp::GetVertStirrupAvs(const pgsPointOfInterest& poi, matRebar::Size* pSize, Float64* pSingleBarArea, Float64* pCount, Float64* pSpacing)
{
   Float64 avs(0.0);
   Float64 Abar(0.0);
   Float64 nBars(0.0);
   Float64 spacing(0.0);

   const CShearData2* pShearData;
   if ( poi.HasAttribute(POI_CLOSURE) )
   {
      GET_IFACE(IShear,pShear);
      pShearData = pShear->GetClosurePourShearData(poi.GetSegmentKey());
   }
   else
   {
      pShearData = GetShearData(poi.GetSegmentKey());
   }

   const CShearZoneData2* pShearZoneData = GetPrimaryShearZoneDataAtPoi(poi, pShearData);

   matRebar::Size barSize = pShearZoneData->VertBarSize;
   if ( barSize!=matRebar::bsNone && !IsZero(pShearZoneData->BarSpacing) )
   {
      lrfdRebarPool* prp = lrfdRebarPool::GetInstance();
      const matRebar* pBar = prp->GetRebar(pShearData->ShearBarType,pShearData->ShearBarGrade,barSize);

      Abar    = pBar->GetNominalArea();
      nBars   = pShearZoneData->nVertBars;
      spacing = pShearZoneData->BarSpacing;

      // area of stirrups per unit length for this zone
      // (assume stirrups are smeared out along zone)
      if (0.0 < spacing)
         avs = nBars * Abar / spacing;
   }

   *pSize          = barSize;
   *pSingleBarArea = Abar;
   *pCount         = nBars;
   *pSpacing       = spacing;

   return avs;
}

bool CBridgeAgentImp::DoStirrupsEngageDeck(const CSegmentKey& segmentKey)
{
   // Just check if any stirrups engage deck
   const CShearData2* pShearData = GetShearData(segmentKey);

   CShearData2::ShearZoneConstIterator szIter(pShearData->ShearZones.begin());
   CShearData2::ShearZoneConstIterator szIterEnd(pShearData->ShearZones.end());
   for ( ; szIter != szIterEnd; szIter++ )
   {
      if (szIter->VertBarSize != matRebar::bsNone && szIter->nHorzInterfaceBars > 0)
         return true;
   }

   CShearData2::HorizontalInterfaceZoneConstIterator hiIter(pShearData->HorizontalInterfaceZones.begin());
   CShearData2::HorizontalInterfaceZoneConstIterator hiIterEnd(pShearData->HorizontalInterfaceZones.end());
   for ( ; hiIter != hiIterEnd; hiIter++ )
   {
      if (hiIter->BarSize != matRebar::bsNone)
         return true;
   }

   return false;
}

bool CBridgeAgentImp::DoAllPrimaryStirrupsEngageDeck(const CSegmentKey& segmentKey)
{
   // Check if all vertical stirrups engage deck
   const CShearData2* pShearData = GetShearData(segmentKey);

   if (pShearData->ShearZones.empty())
   {
      return false;
   }
   else
   {
      for (CShearData2::ShearZoneConstIterator its = pShearData->ShearZones.begin(); its != pShearData->ShearZones.end(); its++)
      {
         // Make sure there are vertical bars, and at least as many horiz int bars
         if (its->VertBarSize==matRebar::bsNone ||
             its->nVertBars <= 0                ||
             its->nHorzInterfaceBars < its->nVertBars)
            return false;
      }
   }

   return true;
}
Float64 CBridgeAgentImp::GetPrimaryHorizInterfaceS(const pgsPointOfInterest& poi)
{
   Float64 spacing = 0.0;

   const CShearData2* pShearData = GetShearData(poi.GetSegmentKey());

   // Horizontal legs in primary zones
   const CShearZoneData2* pShearZoneData = GetPrimaryShearZoneDataAtPoi(poi, pShearData);
   if (pShearZoneData->VertBarSize != matRebar::bsNone && 0.0 < pShearZoneData->nHorzInterfaceBars)
   {
      spacing = pShearZoneData->BarSpacing;
   }

   return spacing;
}

Float64 CBridgeAgentImp::GetPrimaryHorizInterfaceBarCount(const pgsPointOfInterest& poi)
{
   Float64 cnt = 0.0;

   const CShearData2* pShearData = GetShearData(poi.GetSegmentKey());

   // Horizontal legs in primary zones
   const CShearZoneData2* pShearZoneData = GetPrimaryShearZoneDataAtPoi(poi, pShearData);
   if (pShearZoneData->VertBarSize != matRebar::bsNone)
   {
      cnt = pShearZoneData->nHorzInterfaceBars;
   }

   return cnt;
}

Float64 CBridgeAgentImp::GetAdditionalHorizInterfaceS(const pgsPointOfInterest& poi)
{
   Float64 spacing = 0.0;

   const CShearData2* pShearData = GetShearData(poi.GetSegmentKey());

   // Additional horizontal bars
   const CHorizontalInterfaceZoneData* pHIZoneData = GetHorizInterfaceShearZoneDataAtPoi( poi, pShearData );
   if ( pHIZoneData->BarSize != matRebar::bsNone )
   {
      spacing = pHIZoneData->BarSpacing;
   }

   return spacing;
}

Float64 CBridgeAgentImp::GetAdditionalHorizInterfaceBarCount(const pgsPointOfInterest& poi)
{
   Float64 cnt = 0.0;

   const CShearData2* pShearData = GetShearData(poi.GetSegmentKey());

   // Additional horizontal bars
   const CHorizontalInterfaceZoneData* pHIZoneData = GetHorizInterfaceShearZoneDataAtPoi( poi,pShearData );
   if ( pHIZoneData->BarSize != matRebar::bsNone )
   {
      cnt = pHIZoneData->nBars;
   }

   return cnt;
}

Float64 CBridgeAgentImp::GetPrimaryHorizInterfaceAvs(const pgsPointOfInterest& poi, matRebar::Size* pSize, Float64* pSingleBarArea, Float64* pCount, Float64* pSpacing)
{
   Float64 avs(0.0);
   Float64 Abar(0.0);
   Float64 nBars(0.0);
   Float64 spacing(0.0);

   const CShearData2* pShearData = GetShearData(poi.GetSegmentKey());
   
   // First get avs from primary bar zone
   const CShearZoneData2* pShearZoneData = GetPrimaryShearZoneDataAtPoi(poi,pShearData);

   matRebar::Size barSize = pShearZoneData->VertBarSize;

   if ( barSize != matRebar::bsNone && !IsZero(pShearZoneData->BarSpacing) && 0.0 < pShearZoneData->nHorzInterfaceBars )
   {
      lrfdRebarPool* prp = lrfdRebarPool::GetInstance();
      const matRebar* pbar = prp->GetRebar(pShearData->ShearBarType,pShearData->ShearBarGrade,barSize);

      Abar    = pbar->GetNominalArea();
      nBars   = pShearZoneData->nHorzInterfaceBars;
      spacing = pShearZoneData->BarSpacing;

      if (0.0 < spacing)
         avs =   nBars * Abar / spacing;
   }

   *pSize          = barSize;
   *pSingleBarArea = Abar;
   *pCount         = nBars;
   *pSpacing       = spacing;

   return avs;
}

Float64 CBridgeAgentImp::GetAdditionalHorizInterfaceAvs(const pgsPointOfInterest& poi, matRebar::Size* pSize, Float64* pSingleBarArea, Float64* pCount, Float64* pSpacing)
{
   Float64 avs(0.0);
   Float64 Abar(0.0);
   Float64 nBars(0.0);
   Float64 spacing(0.0);

   const CShearData2* pShearData = GetShearData(poi.GetSegmentKey());
   
   const CHorizontalInterfaceZoneData* pHIZoneData = GetHorizInterfaceShearZoneDataAtPoi( poi, pShearData );

   matRebar::Size barSize = pHIZoneData->BarSize;

   if ( barSize != matRebar::bsNone && !IsZero(pHIZoneData->BarSpacing) && 0.0 < pHIZoneData->nBars )
   {
      lrfdRebarPool* prp = lrfdRebarPool::GetInstance();
      const matRebar* pbar = prp->GetRebar(pShearData->ShearBarType,pShearData->ShearBarGrade,barSize);

      Abar    = pbar->GetNominalArea();
      nBars   = pHIZoneData->nBars;
      spacing = pHIZoneData->BarSpacing;

      if (0.0 < spacing)
         avs =  nBars * Abar / spacing;
   }

   *pSize          = barSize;
   *pSingleBarArea = Abar;
   *pCount         = nBars;
   *pSpacing       = spacing;

   return avs;
}

Float64 CBridgeAgentImp::GetSplittingAv(const CSegmentKey& segmentKey,Float64 start,Float64 end)
{
   ATLASSERT(end>start);

   Float64 Av = 0.0;

   const CShearData2* pShearData = GetShearData(segmentKey);

   // Get component from primary bars
   if (pShearData->bUsePrimaryForSplitting)
   {
      Av += GetPrimarySplittingAv( segmentKey, start, end, pShearData);
   }

   // Component from additional splitting bars
   if (pShearData->SplittingBarSize != matRebar::bsNone && pShearData->nSplittingBars)
   {
      Float64 spacing = pShearData->SplittingBarSpacing;
      Float64 length = 0.0;
      if (spacing <=0.0)
      {
         ATLASSERT(0); // UI should block this
      }
      else
      {
         // determine how much additional bars is in our start/end region
         Float64 zone_length = pShearData->SplittingZoneLength;
         // left end first
         if (start < zone_length)
         {
            Float64 zend = min(end, zone_length);
            length = zend-start;
         }
         else
         {
            // try right end
            Float64 segment_length = GetSegmentLength(segmentKey);
            if (segment_length <= end)
            {
               Float64 zstart = max(segment_length-zone_length, start);
               length = end-zstart;
            }
         }

         if (0.0 < length)
         {
            // We have bars in region. multiply av/s * length
            lrfdRebarPool* prp = lrfdRebarPool::GetInstance();
            const matRebar* pbar = prp->GetRebar(pShearData->ShearBarType, pShearData->ShearBarGrade, pShearData->SplittingBarSize);

            Float64 Abar = pbar->GetNominalArea();
            Float64 avs  = pShearData->nSplittingBars * Abar / spacing;

            Av += avs * length;
         }
      }
   }

   return Av;
}

Float64 CBridgeAgentImp::GetPrimarySplittingAv(const CSegmentKey& segmentKey,Float64 start,Float64 end, const CShearData2* pShearData)
{
   if (!pShearData->bUsePrimaryForSplitting)
   {
      ATLASSERT(0); // shouldn't be called for this case
      return 0.0;
   }

   // Get total amount of splitting steel between start and end
   Float64 Av = 0;

   ZoneIndexType nbrZones = GetNumPrimaryZones(segmentKey);
   for ( ZoneIndexType zone = 0; zone < nbrZones; zone++ )
   {
      Float64 zoneStart, zoneEnd;
      GetPrimaryZoneBounds(segmentKey, zone, &zoneStart, &zoneEnd);

      Float64 length; // length of zone which falls within the range

      // zoneStart                zoneEnd
      //   |-----------------------|
      //       zone is here (1) zoneStart                zoneEnd
      //                           |-----------------------|
      //                                 or here (2)    zoneStart                zoneEnd
      //                                                   |-----------------------|
      //                                                           here (3)
      //                |=============================================|
      //             start                 Range                     end
      //      zoneStart                                                   zoneEnd
      //        |-------------------------------------------------------------|
      //                       (4) zone is larger than range 

      if ( start <= zoneStart && zoneEnd <= end )
      {
         // Case 2 - entire zone is in the range
         length = zoneEnd - zoneStart;
      }
      else if ( zoneStart < start && end < zoneEnd )
      {
         // Case 4
         length = end - start;
      }
      else if ( zoneStart < start && InRange(start,zoneEnd,end) )
      {
         // Case 1
         length = zoneEnd - start;
      }
      else if ( InRange(start,zoneStart,end) && end < zoneEnd )
      {
         // Case 3
         length = end - zoneStart;
      }
      else
      {
         continue; // This zone doesn't touch the range at all... go back to the start of the loop
      }

      // We are in a zone - determine Av/S and multiply by length
      ZoneIndexType idx = GetPrimaryZoneIndex(segmentKey, pShearData, zone);

      const CShearZoneData2& shearZoneData = pShearData->ShearZones[idx];

      matRebar::Size barSize = shearZoneData.VertBarSize; // splitting is same as vert bars
      if ( barSize != matRebar::bsNone && !IsZero(shearZoneData.BarSpacing) )
      {
         lrfdRebarPool* prp = lrfdRebarPool::GetInstance();
         const matRebar* pbar = prp->GetRebar(pShearData->ShearBarType,pShearData->ShearBarGrade,barSize);

         Float64 Abar   = pbar->GetNominalArea();

         // area of stirrups per unit length for this zone
         // (assume stirrups are smeared out along zone)
         Float64 avs = shearZoneData.nVertBars * Abar / shearZoneData.BarSpacing;

         Av += avs * length;
      }
   }

   return Av;
}

void CBridgeAgentImp::GetStartConfinementBarInfo(const CSegmentKey& segmentKey, Float64 requiredZoneLength, matRebar::Size* pSize, Float64* pProvidedZoneLength, Float64* pSpacing)
{
   const CShearData2* pShearData = GetShearData(segmentKey);

   ZoneIndexType nbrZones = GetNumPrimaryZones(segmentKey);

   // First get data from primary zones - use min bar size and max spacing from zones in required region
   Float64 primSpc(-1), primZonL(-1);
   matRebar::Size primSize(matRebar::bsNone);

   Float64 ezloc;

   // walk from left to right on girder
   for ( ZoneIndexType zone = 0; zone < nbrZones; zone++ )
   {
      Float64 zoneStart;
      GetPrimaryZoneBounds(segmentKey, zone, &zoneStart, &ezloc);

      ZoneIndexType idx = GetPrimaryZoneIndex(segmentKey, pShearData, zone);

      const CShearZoneData2& shearZoneData = pShearData->ShearZones[idx];

      if (shearZoneData.ConfinementBarSize != matRebar::bsNone)
      {
         primSize = min(primSize, shearZoneData.ConfinementBarSize);
         primSpc  = max(primSpc,  shearZoneData.BarSpacing);

         primZonL = ezloc;
      }

      if (requiredZoneLength < ezloc + TOL)
      {
         break; // actual zone length exceeds required - we are done
      }
   }

   // Next get additional confinement bar info
   Float64 addlSpc, addlZonL;
   matRebar::Size addlSize;
   GetAddConfinementBarInfo(segmentKey, &addlSize, &addlZonL, &addlSpc);

   // Use either primary bars or additional bars. Choose by which has addequate zone length, smallest spacing, largest bars
   ChooseConfinementBars(requiredZoneLength, primSpc, primZonL, primSize, addlSpc, addlZonL, addlSize,
                         pSize, pProvidedZoneLength, pSpacing);
}


void CBridgeAgentImp::GetEndConfinementBarInfo( const CSegmentKey& segmentKey, Float64 requiredZoneLength, matRebar::Size* pSize, Float64* pProvidedZoneLength, Float64* pSpacing)
{
   const CShearData2* pShearData = GetShearData(segmentKey);

   Float64 segment_length = GetSegmentLength(segmentKey);

   ZoneIndexType nbrZones = GetNumPrimaryZones(segmentKey);

   // First get data from primary zones - use min bar size and max spacing from zones in required region
   Float64 primSpc(-1), primZonL(-1);
   matRebar::Size primSize(matRebar::bsNone);

   Float64 ezloc;
   // walk from right to left on girder
   for ( ZoneIndexType zone = nbrZones-1; zone>=0; zone-- )
   {
      Float64 zoneStart, zoneEnd;
      GetPrimaryZoneBounds(segmentKey, zone, &zoneStart, &zoneEnd);

      ezloc = segment_length - zoneStart;

      ZoneIndexType idx = GetPrimaryZoneIndex(segmentKey, pShearData, zone);

      const CShearZoneData2* pShearZoneData = &pShearData->ShearZones[idx];

      if (pShearZoneData->ConfinementBarSize != matRebar::bsNone)
      {
         primSize = min(primSize, pShearZoneData->ConfinementBarSize);
         primSpc  = max(primSpc, pShearZoneData->BarSpacing);

         primZonL = ezloc;
      }

      if (requiredZoneLength < ezloc + TOL)
      {
         break; // actual zone length exceeds required - we are done
      }
   }

   // Next get additional confinement bar info
   Float64 addlSpc, addlZonL;
   matRebar::Size addlSize;
   GetAddConfinementBarInfo(segmentKey, &addlSize, &addlZonL, &addlSpc);

   // Use either primary bars or additional bars. Choose by which has addequate zone length, smallest spacing, largest bars
   ChooseConfinementBars(requiredZoneLength, primSpc, primZonL, primSize, addlSpc, addlZonL, addlSize,
                         pSize, pProvidedZoneLength, pSpacing);
}

// private:

void CBridgeAgentImp::InvalidateStirrupData()
{
   m_ShearData.clear();
}

const CShearData2* CBridgeAgentImp::GetShearData(const CSegmentKey& segmentKey)
{
   ShearDataIterator found;
   found = m_ShearData.find( segmentKey );
   if ( found == m_ShearData.end() )
   {
	   GET_IFACE2(m_pBroker,IShear,pShear);
	   const CShearData2* pShearData = pShear->GetSegmentShearData(segmentKey);
      std::pair<ShearDataIterator,bool> insit = m_ShearData.insert( std::make_pair(segmentKey, *pShearData ) );
      ATLASSERT( insit.second );
      return &(*insit.first).second;
   }
   else
   {
      ATLASSERT( found != m_ShearData.end() );
      return &(*found).second;
   }
}

ZoneIndexType CBridgeAgentImp::GetPrimaryZoneIndex(const CSegmentKey& segmentKey, const CShearData2* pShearData, ZoneIndexType zone)
{
   // mapping so that we only need to store half of the zones
   ZoneIndexType nZones = GetNumPrimaryZones(segmentKey); 
   ATLASSERT(zone < nZones);
   if (pShearData->bAreZonesSymmetrical)
   {
      ZoneIndexType nz2 = (nZones+1)/2;
      if (zone < nz2)
         return zone;
      else
         return nZones-zone-1;
   }
   else
   {
      // mapping is 1:1 for non-sym
      return zone;
   }
}

ZoneIndexType CBridgeAgentImp::GetHorizInterfaceZoneIndex(const CSegmentKey& segmentKey, const CShearData2* pShearData, ZoneIndexType zone)
{
   // mapping so that we only need to store half of the zones
   ZoneIndexType nZones = GetNumHorizInterfaceZones(segmentKey); 
   ATLASSERT(zone < nZones);
   if (pShearData->bAreZonesSymmetrical)
   {
      ZoneIndexType nz2 = (nZones+1)/2;
      if (zone < nz2)
         return zone;
      else
         return nZones-zone-1;
   }
   else
   {
      // mapping is 1:1 for non-sym
      return zone;
   }
}

bool CBridgeAgentImp::IsPoiInEndRegion(const pgsPointOfInterest& poi, Float64 distFromEnds)
{
   Float64 dist = poi.GetDistFromStart();

   // look at left end first
   if (dist <= distFromEnds)
   {
      return true;
   }

   // right end
   Float64 segment_length = GetSegmentLength(poi.GetSegmentKey());

   if (segment_length-distFromEnds <= dist)
   {
      return true;
   }

   return false;
}

ZoneIndexType CBridgeAgentImp::GetPrimaryShearZoneIndexAtPoi(const pgsPointOfInterest& poi, const CShearData2* pShearData)
{
   // NOTE: The logic here is identical to GetHorizInterfaceShearZoneIndexAtPoi
   //       If you fix a bug here, you need to fix it there also

   const CSegmentKey& segmentKey = poi.GetSegmentKey();

   ZoneIndexType nz = GetNumPrimaryZones(segmentKey);
   if (nz==0)
      return INVALID_INDEX;

   Float64 location, length, left_bearing_location, right_bearing_location;
   if ( poi.HasAttribute(POI_CLOSURE) || poi.HasAttribute(POI_PIER) )
   {
      length = GetClosurePourLength(segmentKey);
      location = poi.GetDistFromStart() - GetSegmentLength(segmentKey);
      left_bearing_location  = 0;
      right_bearing_location = 0;
   }
   else
   {
      length = GetSegmentLength(segmentKey);
      location = poi.GetDistFromStart();
      left_bearing_location  = GetSegmentStartBearingOffset(segmentKey);
      right_bearing_location = length - GetSegmentEndBearingOffset(segmentKey);
   }

   // use template function to do heavy work
   ZoneIndexType zone =  GetZoneIndexAtLocation(location, length, left_bearing_location, right_bearing_location, pShearData->bAreZonesSymmetrical, 
                                                pShearData->ShearZones.begin(), pShearData->ShearZones.end(), 
                                                pShearData->ShearZones.size());

   return zone;
}

const CShearZoneData2* CBridgeAgentImp::GetPrimaryShearZoneDataAtPoi(const pgsPointOfInterest& poi, const CShearData2* pShearData)
{
   ZoneIndexType idx = GetPrimaryShearZoneIndexAtPoi(poi,pShearData);
   return &pShearData->ShearZones[idx];
}

ZoneIndexType CBridgeAgentImp::GetHorizInterfaceShearZoneIndexAtPoi(const pgsPointOfInterest& poi, const CShearData2* pShearData)
{
   // NOTE: The logic here is identical to GetPrimaryShearZoneAtPoi
   //       If you fix a bug here, you need to fix it there also

   const CSegmentKey& segmentKey = poi.GetSegmentKey();

   ZoneIndexType nz = GetNumHorizInterfaceZones(segmentKey);
   if (nz==0)
      return INVALID_INDEX;

   Float64 location, length, left_bearing_location, right_bearing_location;
   if ( poi.HasAttribute(POI_CLOSURE) || poi.HasAttribute(POI_PIER)  )
   {
      length = GetClosurePourLength(segmentKey);
      location = poi.GetDistFromStart() - GetSegmentLength(segmentKey);
      left_bearing_location  = 0;
      right_bearing_location = 0;
   }
   else
   {
      length = GetSegmentLength(segmentKey);
      location = poi.GetDistFromStart();
      left_bearing_location  = GetSegmentStartBearingOffset(segmentKey);
      right_bearing_location = length - GetSegmentEndBearingOffset(segmentKey);
   }

   // use template function to do heavy work
   ZoneIndexType zone =  GetZoneIndexAtLocation(location, length, left_bearing_location, right_bearing_location, pShearData->bAreZonesSymmetrical, 
                                                pShearData->HorizontalInterfaceZones.begin(), pShearData->HorizontalInterfaceZones.end(), 
                                                pShearData->HorizontalInterfaceZones.size());


   return zone;
}

const CHorizontalInterfaceZoneData* CBridgeAgentImp::GetHorizInterfaceShearZoneDataAtPoi(const pgsPointOfInterest& poi, const CShearData2* pShearData)
{
   ZoneIndexType idx = GetHorizInterfaceShearZoneIndexAtPoi(poi,pShearData);
   if ( idx == INVALID_INDEX )
   {
      ATLASSERT(false);
      return NULL;
   }
   return &pShearData->HorizontalInterfaceZones[idx];
}

/////////////////////////////////////////////////////////////////////////
// IStrandGeometry
//
Float64 CBridgeAgentImp::GetEccentricity(IntervalIndexType intervalIdx,const pgsPointOfInterest& poi,bool bIncTemp, Float64* nEffectiveStrands)
{
   VALIDATE( GIRDER );

   const CSegmentKey& segmentKey = poi.GetSegmentKey();

   CComPtr<IPrecastGirder> girder;
   GetGirder(poi,&girder);

   Float64 nStraight  = 0;
   Float64 nHarped    = 0;
   Float64 nTemporary = 0;

   Float64 ecc_straight  = 0;
   Float64 ecc_harped    = 0;
   Float64 ecc_temporary = 0;

   Float64 ecc = 0;

   ecc_straight = GetSsEccentricity(intervalIdx,poi,&nStraight);
   ecc_harped   = GetHsEccentricity(intervalIdx,poi,&nHarped);

   Float64 asStraight  = GetStrandMaterial(segmentKey,pgsTypes::Straight)->GetNominalArea()*nStraight;
   Float64 asHarped    = GetStrandMaterial(segmentKey,pgsTypes::Harped)->GetNominalArea()*nHarped;
   Float64 asTemporary = 0;

   if ( bIncTemp )
   {
      ecc_temporary = GetTempEccentricity(intervalIdx,poi,&nTemporary);
      asTemporary   = GetStrandMaterial(segmentKey,pgsTypes::Temporary)->GetNominalArea()*nTemporary;
   }

   *nEffectiveStrands = nStraight + nHarped + nTemporary;
   if ( *nEffectiveStrands > 0.0)
   {
      ecc = (asStraight*ecc_straight + asHarped*ecc_harped + asTemporary*ecc_temporary) / (asStraight + asHarped + asTemporary);
   }
   else
   {
      ecc = 0.0;
   }

   return ecc;
}

Float64 CBridgeAgentImp::GetEccentricity(IntervalIndexType intervalIdx,const pgsPointOfInterest& poi,pgsTypes::StrandType strandType, Float64* nEffectiveStrands)
{
   Float64 e = -9999999999;
   switch( strandType )
   {
   case pgsTypes::Straight:
      e = GetSsEccentricity(intervalIdx,poi,nEffectiveStrands);
      break;

   case pgsTypes::Harped:
      e = GetHsEccentricity(intervalIdx,poi,nEffectiveStrands);
      break;
   
   case pgsTypes::Temporary:
      e = GetTempEccentricity(intervalIdx,poi,nEffectiveStrands);
      break;
   }

   return e;
}

Float64 CBridgeAgentImp::GetHsEccentricity(IntervalIndexType intervalIdx,const pgsPointOfInterest& poi, Float64* nEffectiveStrands)
{
   pgsTypes::SectionPropertyType spType = (pgsTypes::SectionPropertyType)(GetSectionPropertiesMode());
   return GetHsEccentricity(spType,intervalIdx,poi,nEffectiveStrands);
}

Float64 CBridgeAgentImp::GetSsEccentricity(IntervalIndexType intervalIdx,const pgsPointOfInterest& poi, Float64* nEffectiveStrands)
{
   pgsTypes::SectionPropertyType spType = (pgsTypes::SectionPropertyType)(GetSectionPropertiesMode());
   return GetSsEccentricity(spType,intervalIdx,poi,nEffectiveStrands);
}

Float64 CBridgeAgentImp::GetTempEccentricity(IntervalIndexType intervalIdx,const pgsPointOfInterest& poi, Float64* nEffectiveStrands)
{
   pgsTypes::SectionPropertyType spType = (pgsTypes::SectionPropertyType)(GetSectionPropertiesMode());
   return GetTempEccentricity(spType,intervalIdx,poi,nEffectiveStrands);
}

Float64 CBridgeAgentImp::GetMaxStrandSlope(const pgsPointOfInterest& poi)
{
   // No harped strands in a closure pour
   if ( poi.HasAttribute(POI_CLOSURE) || poi.HasAttribute(POI_PIER)  )
      return 0.0;

   VALIDATE( GIRDER );
   CComPtr<IPrecastGirder> girder;
   GetGirder(poi,&girder);

   Float64 slope;
   girder->ComputeMaxHarpedStrandSlope(poi.GetDistFromStart(),&slope);

   return slope;
}

Float64 CBridgeAgentImp::GetAvgStrandSlope(const pgsPointOfInterest& poi)
{
   // No harped strands in a closure pour
   if ( poi.HasAttribute(POI_CLOSURE) || poi.HasAttribute(POI_PIER)  )
      return 0.0;

   VALIDATE( GIRDER );
   CComPtr<IPrecastGirder> girder;
   GetGirder(poi,&girder);

   Float64 slope;
   girder->ComputeAvgHarpedStrandSlope(poi.GetDistFromStart(),&slope);

   return slope;
}

Float64 CBridgeAgentImp::GetHsEccentricity(IntervalIndexType intervalIdx,const pgsPointOfInterest& poi, const GDRCONFIG& rconfig, Float64* nEffectiveStrands)
{
   if ( poi.HasAttribute(POI_CLOSURE) || poi.HasAttribute(POI_PIER)  )
      return 0.0; // strands aren't located in closure pours

   const CSegmentKey& segmentKey = poi.GetSegmentKey();

   IntervalIndexType releaseIntervalIdx = GetPrestressReleaseInterval(segmentKey);

   StrandIndexType Nh = rconfig.PrestressConfig.GetNStrands(pgsTypes::Harped);

   ATLASSERT(rconfig.PrestressConfig.Debond[pgsTypes::Harped].empty()); // we don't deal with debonding of harped strands for design. WBFL needs to be updated

   VALIDATE( GIRDER );

   if (Nh==0)
   {
      *nEffectiveStrands = 0.0;
      return 0.0;
   }
   else
   {
      CComPtr<IPrecastGirder> girder;
      GetGirder(poi,&girder);

      // must account for partial bonding if poi is near end of girder
      // NOTE: bond factor is 1.0 if at girder ends
      Float64 poi_loc = poi.GetDistFromStart();
      Float64 bond_factor = 1.0;

      GET_IFACE(IPretensionForce,pPrestress);
      Float64 xfer_length = pPrestress->GetXferLength(segmentKey,pgsTypes::Harped);

      if (0.0 < poi_loc && poi_loc < xfer_length)
      {
         bond_factor = poi_loc/xfer_length;
      }
      else
      {
         Float64 girder_length = GetSegmentLength(segmentKey);

         if ( (girder_length-xfer_length) <poi_loc && poi_loc < girder_length)
         {
            bond_factor = (girder_length-poi_loc)/xfer_length;
         }
      }

      // use continuous interface to set strands
      CComPtr<IIndexArray> strand_fill;
      m_StrandFillers[segmentKey].ComputeHarpedStrandFill(girder, Nh, &strand_fill);

      // save and restore precast girders shift values, before/after geting point locations
      Float64 t_end_shift, t_hp_shift;
      girder->get_HarpedStrandAdjustmentEnd(&t_end_shift);
      girder->get_HarpedStrandAdjustmentHP(&t_hp_shift);

      girder->put_HarpedStrandAdjustmentEnd(rconfig.PrestressConfig.EndOffset);
      girder->put_HarpedStrandAdjustmentHP(rconfig.PrestressConfig.HpOffset);

      CComPtr<IPoint2dCollection> points;
      girder->get_HarpedStrandPositionsEx(poi.GetDistFromStart(), strand_fill, &points);

      girder->put_HarpedStrandAdjustmentEnd(t_end_shift);
      girder->put_HarpedStrandAdjustmentHP(t_hp_shift);

      Float64 ecc=0.0;

      // compute cg
      CollectionIndexType num_strand_positions;
      points->get_Count(&num_strand_positions);

      *nEffectiveStrands = bond_factor*num_strand_positions;

      ATLASSERT( Nh == StrandIndexType(num_strand_positions));
      if (0 < num_strand_positions)
      {
         Float64 cg=0.0;
         for (CollectionIndexType strandPositionIdx = 0; strandPositionIdx < num_strand_positions; strandPositionIdx++)
         {
            CComPtr<IPoint2d> point;
            points->get_Item(strandPositionIdx,&point);
            Float64 y;
            point->get_Y(&y);

            cg += y;
         }

         cg = cg / (Float64)num_strand_positions;

         Float64 Yb = GetYb(releaseIntervalIdx,poi);
         ecc = Yb-cg;
      }

      return ecc;
   }
}

Float64 CBridgeAgentImp::GetSsEccentricity(IntervalIndexType intervalIdx,const pgsPointOfInterest& poi, const GDRCONFIG& rconfig, Float64* nEffectiveStrands)
{
   if ( poi.HasAttribute(POI_CLOSURE) || poi.HasAttribute(POI_PIER)  )
      return 0.0; // strands aren't located in closure pours

   const CSegmentKey& segmentKey = poi.GetSegmentKey();

   StrandIndexType Ns = rconfig.PrestressConfig.GetNStrands(pgsTypes::Straight);

   IntervalIndexType releaseIntervalIdx = GetPrestressReleaseInterval(segmentKey);

   VALIDATE( GIRDER );

   if (Ns == 0)
   {
      *nEffectiveStrands = 0;
      return 0.0;
   }
   else
   {
      CComPtr<IPrecastGirder> girder;
      GetGirder(poi,&girder);

      Float64 girder_length = GetSegmentLength(segmentKey);
      Float64 poi_loc = poi.GetDistFromStart();

      // transfer
      GET_IFACE(IPretensionForce,pPrestress);
      Float64 xfer_length = pPrestress->GetXferLength(segmentKey,pgsTypes::Straight);

      // debonding - Let's set up a mapping data structure to make dealing with debonding easier (eliminate searching)
      const DebondConfigCollection& rdebonds = rconfig.PrestressConfig.Debond[pgsTypes::Straight];

      std::vector<Int16> debond_map;
      debond_map.assign(Ns, -1); // strands not debonded have and index of -1
      Int16 dbinc=0;
      for (DebondConfigConstIterator idb=rdebonds.begin(); idb!=rdebonds.end(); idb++)
      {
         const DEBONDCONFIG& dbinfo = *idb;
         if (dbinfo.strandIdx<Ns)
         {
            debond_map[dbinfo.strandIdx] = dbinc; 
         }
         else
         {
            ATLASSERT(0); // shouldn't be debonding any strands that don't exist
         }

         dbinc++;
      }

      // get all current straight strand locations
      CComPtr<IIndexArray> strand_fill;
      m_StrandFillers[segmentKey].ComputeStraightStrandFill(girder, Ns, &strand_fill);

      CComPtr<IPoint2dCollection> points;
      girder->get_StraightStrandPositionsEx(poi.GetDistFromStart(), strand_fill, &points);

      CollectionIndexType num_strand_positions;
      points->get_Count(&num_strand_positions);
      ATLASSERT( Ns == num_strand_positions);

      // loop over all strands, compute bonded length, and weighted cg of strands
      Float64 cg = 0.0;
      Float64 num_eff_strands = 0.0;

      // special cases are where POI is at girder ends. For this, weight strands fully
      if (IsZero(poi_loc))
      {
         // poi is at left end
         for (CollectionIndexType strandPositionIdx = 0; strandPositionIdx < num_strand_positions; strandPositionIdx++)
         {
            CComPtr<IPoint2d> point;
            points->get_Item(strandPositionIdx,&point);
            Float64 y;
            point->get_Y(&y);

            Int16 dbidx = debond_map[strandPositionIdx];
            if ( dbidx != -1)
            {
               // strand is debonded
               const DEBONDCONFIG& dbinfo = rdebonds[dbidx];
 
               if (dbinfo.LeftDebondLength==0.0)
               {
                  // left end is not debonded
                  num_eff_strands += 1.0;
                  cg += y;
               }
            }
            else
            {
               // not debonded, give full weight
               num_eff_strands += 1.0;
               cg += y;
            }
         }
      }
      else if (IsEqual(poi_loc, girder_length))
      {
         // poi is at right end
         for (CollectionIndexType strandPositionIdx = 0; strandPositionIdx < num_strand_positions; strandPositionIdx++)
         {
            CComPtr<IPoint2d> point;
            points->get_Item(strandPositionIdx,&point);
            Float64 y;
            point->get_Y(&y);

            Int16 dbidx = debond_map[strandPositionIdx];
            if ( dbidx != -1)
            {
               // strand is debonded
               const DEBONDCONFIG& dbinfo = rdebonds[dbidx];
 
               if (dbinfo.RightDebondLength==0.0)
               {
                  // right end is not debonded
                  num_eff_strands += 1.0;
                  cg += y;
               }
            }
            else
            {
               // not debonded, give full weight
               num_eff_strands += 1.0;
               cg += y;
            }
         }
      }
      else
      {
         // poi is between ends
         for (CollectionIndexType strandPositionIdx = 0; strandPositionIdx < num_strand_positions; strandPositionIdx++)
         {
            CComPtr<IPoint2d> point;
            points->get_Item(strandPositionIdx,&point);
            Float64 y;
            point->get_Y(&y);

            // bonded length
            Float64 lft_bond, rgt_bond;
            Int16 dbidx = debond_map[strandPositionIdx];
            if ( dbidx != -1)
            {
               const DEBONDCONFIG& dbinfo = rdebonds[dbidx];
 
               // strand is debonded
               lft_bond = poi_loc - dbinfo.LeftDebondLength;
               rgt_bond = girder_length - dbinfo.RightDebondLength - poi_loc;
            }
            else
            {
               lft_bond = poi_loc;
               rgt_bond = girder_length - poi_loc;
            }

            Float64 bond_len = min(lft_bond, rgt_bond);
            if (bond_len <= 0.0)
            {
               // not bonded - do nothing
            }
            else if (bond_len < xfer_length)
            {
               Float64 bf = bond_len/xfer_length; // reduce strand weight for debonding
               num_eff_strands += bf;
               cg += y * bf;
            }
            else
            {
               // fully bonded
               num_eff_strands += 1.0;
               cg += y;
            }
         }
      }

      *nEffectiveStrands = num_eff_strands;

      if (0.0 < num_eff_strands)
      {
         cg = cg/num_eff_strands;

         Float64 Yb = GetYb(releaseIntervalIdx,poi);

         Float64 ecc = Yb - cg;
         return ecc;
      }
      else
      {
         // all strands debonded out
         return 0.0;
      }
   }
}

Float64 CBridgeAgentImp::GetTempEccentricity(IntervalIndexType intervalIdx,const pgsPointOfInterest& poi, const GDRCONFIG& rconfig, Float64* nEffectiveStrands)
{
   if ( poi.HasAttribute(POI_CLOSURE) || poi.HasAttribute(POI_PIER) )
      return 0.0; // strands aren't located in closure pours

   const CSegmentKey& segmentKey = poi.GetSegmentKey();

   StrandIndexType Nt = rconfig.PrestressConfig.GetNStrands(pgsTypes::Temporary);

   ATLASSERT(rconfig.PrestressConfig.Debond[pgsTypes::Temporary].empty()); // we don't deal with debonding of temp strands for design. WBFL needs to be updated

   VALIDATE( GIRDER );

   IntervalIndexType releaseIntervalIdx = GetPrestressReleaseInterval(segmentKey);

   if (Nt==0)
   {
      *nEffectiveStrands = 0.0;
      return 0.0;
   }
   else
   {
      CComPtr<IPrecastGirder> girder;
      GetGirder(poi,&girder);

      // must account for partial bonding if poi is near end of girder
      // NOTE: bond factor is 1.0 if at girder ends
      Float64 poi_loc = poi.GetDistFromStart();
      Float64 bond_factor = 1.0;

      GET_IFACE(IPretensionForce,pPrestress);
      Float64 xfer_length = pPrestress->GetXferLength(segmentKey,pgsTypes::Temporary);

      if (0.0 < poi_loc && poi_loc < xfer_length)
      {
         bond_factor = poi_loc/xfer_length;
      }
      else
      {
         Float64 girder_length = GetSegmentLength(segmentKey);

         if ( (girder_length-xfer_length) < poi_loc && poi_loc < girder_length)
         {
            bond_factor = (girder_length-poi_loc)/xfer_length;
         }
      }

      // use continuous interface to set strands
      CComPtr<IIndexArray> strand_fill;
      m_StrandFillers[segmentKey].ComputeTemporaryStrandFill(girder, Nt, &strand_fill);

      CComPtr<IPoint2dCollection> points;
      girder->get_TemporaryStrandPositionsEx(poi.GetDistFromStart(), strand_fill, &points);

      Float64 ecc=0.0;

      // compute cg
      CollectionIndexType num_strands_positions;
      points->get_Count(&num_strands_positions);

      *nEffectiveStrands = bond_factor * num_strands_positions;

      ATLASSERT(Nt == StrandIndexType(num_strands_positions));
      Float64 cg=0.0;
      for (CollectionIndexType strandPositionIdx = 0; strandPositionIdx < num_strands_positions; strandPositionIdx++)
      {
         CComPtr<IPoint2d> point;
         points->get_Item(strandPositionIdx,&point);
         Float64 y;
         point->get_Y(&y);

         cg += y;
      }

      cg = cg / (Float64)num_strands_positions;

      Float64 Yb = GetYb(releaseIntervalIdx,poi);
      ecc = Yb-cg;

      return ecc;
   }
}

Float64 CBridgeAgentImp::GetEccentricity(IntervalIndexType intervalIdx,const pgsPointOfInterest& poi, const GDRCONFIG& rconfig, bool bIncTemp, Float64* nEffectiveStrands)
{
   if ( poi.HasAttribute(POI_CLOSURE) || poi.HasAttribute(POI_PIER) )
      return 0.0; // strands aren't located in closure pours

   VALIDATE( GIRDER );
   const CSegmentKey& segmentKey = poi.GetSegmentKey();

   CComPtr<IPrecastGirder> girder;
   GetGirder(poi,&girder);

   Float64 dns = 0.0;
   Float64 dnh = 0.0;
   Float64 dnt = 0.0;

   Float64 eccs = GetSsEccentricity(intervalIdx, poi, rconfig, &dns);
   Float64 ecch = GetHsEccentricity(intervalIdx, poi, rconfig, &dnh);
   Float64 ecct = 0.0;

   Float64 aps[3];
   aps[pgsTypes::Straight]  = GetStrandMaterial(segmentKey,pgsTypes::Straight)->GetNominalArea() * dns;
   aps[pgsTypes::Harped]    = GetStrandMaterial(segmentKey,pgsTypes::Harped)->GetNominalArea()   * dnh;
   aps[pgsTypes::Temporary] = 0;

   if (bIncTemp)
   {
      ecct = GetTempEccentricity(intervalIdx, poi, rconfig, &dnt);
      aps[pgsTypes::Temporary] = GetStrandMaterial(segmentKey,pgsTypes::Temporary)->GetNominalArea()* dnt;
   }

   Float64 ecc=0.0;
   *nEffectiveStrands = dns + dnh + dnt;
   if (*nEffectiveStrands > 0)
   {
      ecc = (aps[pgsTypes::Straight]*eccs + aps[pgsTypes::Harped]*ecch + aps[pgsTypes::Temporary]*ecct)/(aps[pgsTypes::Straight] + aps[pgsTypes::Harped] + aps[pgsTypes::Temporary]);
   }

   return ecc;
}

Float64 CBridgeAgentImp::GetMaxStrandSlope(const pgsPointOfInterest& poi,StrandIndexType Nh,Float64 endShift,Float64 hpShift)
{
   if ( poi.HasAttribute(POI_CLOSURE) || poi.HasAttribute(POI_PIER) )
      return 0.0; // strands aren't located in closure pours

   VALIDATE( GIRDER );

   const CSegmentKey& segmentKey = poi.GetSegmentKey();

   CComPtr<IPrecastGirder> girder;
   GetGirder(poi,&girder);

   // use continuous interface to compute
   CComPtr<IIndexArray> fill;
   m_StrandFillers[segmentKey].ComputeHarpedStrandFill(girder, Nh, &fill);

   Float64 slope;
   girder->ComputeMaxHarpedStrandSlopeEx(poi.GetDistFromStart(),fill,endShift,hpShift,&slope);

   return slope;
}

Float64 CBridgeAgentImp::GetAvgStrandSlope(const pgsPointOfInterest& poi,StrandIndexType Nh,Float64 endShift,Float64 hpShift)
{
   if ( poi.HasAttribute(POI_CLOSURE) || poi.HasAttribute(POI_PIER) )
      return 0.0; // strands aren't located in closure pours

   VALIDATE( GIRDER );

   const CSegmentKey& segmentKey = poi.GetSegmentKey();

   CComPtr<IPrecastGirder> girder;
   GetGirder(poi,&girder);

   // use continuous interface to compute
   CComPtr<IIndexArray> fill;
   m_StrandFillers[segmentKey].ComputeHarpedStrandFill(girder, Nh, &fill);

   Float64 slope;
   girder->ComputeAvgHarpedStrandSlopeEx(poi.GetDistFromStart(),fill,endShift,hpShift,&slope);

   return slope;
}

Float64 CBridgeAgentImp::GetEccentricity(pgsTypes::SectionPropertyType spType,IntervalIndexType intervalIdx,const pgsPointOfInterest& poi,bool bIncTemp, Float64* nEffectiveStrands)
{
   if ( poi.HasAttribute(POI_CLOSURE) || poi.HasAttribute(POI_PIER) )
      return 0.0; // strands aren't located in closure pours

   ATLASSERT( spType != pgsTypes::sptNetDeck );

   VALIDATE( GIRDER );

   const CSegmentKey& segmentKey = poi.GetSegmentKey();

   CComPtr<IPrecastGirder> girder;
   GetGirder(poi,&girder);

   Float64 nStraight  = 0;
   Float64 nHarped    = 0;
   Float64 nTemporary = 0;

   Float64 ecc_straight  = 0;
   Float64 ecc_harped    = 0;
   Float64 ecc_temporary = 0;

   Float64 ecc = 0;

   ecc_straight = GetSsEccentricity(spType,intervalIdx,poi,&nStraight);
   ecc_harped   = GetHsEccentricity(spType,intervalIdx,poi,&nHarped);

   Float64 asStraight  = GetStrandMaterial(segmentKey,pgsTypes::Straight)->GetNominalArea()*nStraight;
   Float64 asHarped    = GetStrandMaterial(segmentKey,pgsTypes::Harped)->GetNominalArea()*nHarped;
   Float64 asTemporary = 0;

   if ( bIncTemp )
   {
      ecc_temporary = GetTempEccentricity(spType,intervalIdx,poi,&nTemporary);
      asTemporary   = GetStrandMaterial(segmentKey,pgsTypes::Temporary)->GetNominalArea()*nTemporary;
   }

   *nEffectiveStrands = nStraight + nHarped + nTemporary;
   if ( 0.0 < *nEffectiveStrands )
   {
      ecc = (asStraight*ecc_straight + asHarped*ecc_harped + asTemporary*ecc_temporary) / (asStraight + asHarped + asTemporary);
   }
   else
   {
      ecc = 0.0;
   }

   return ecc;
}

Float64 CBridgeAgentImp::GetEccentricity(pgsTypes::SectionPropertyType spType,IntervalIndexType intervalIdx,const pgsPointOfInterest& poi,pgsTypes::StrandType strandType, Float64* nEffectiveStrands)
{
   if ( poi.HasAttribute(POI_CLOSURE) || poi.HasAttribute(POI_PIER) )
      return 0.0; // strands aren't located in closure pours

   Float64 e = -9999999999;
   switch( strandType )
   {
   case pgsTypes::Straight:
      e = GetSsEccentricity(spType,intervalIdx,poi,nEffectiveStrands);
      break;

   case pgsTypes::Harped:
      e = GetHsEccentricity(spType,intervalIdx,poi,nEffectiveStrands);
      break;
   
   case pgsTypes::Temporary:
      e = GetTempEccentricity(spType,intervalIdx,poi,nEffectiveStrands);
      break;
   }

   return e;
}

Float64 CBridgeAgentImp::GetHsEccentricity(pgsTypes::SectionPropertyType spType,IntervalIndexType intervalIdx,const pgsPointOfInterest& poi, Float64* nEffectiveStrands)
{
   ATLASSERT( spType != pgsTypes::sptNetDeck );

   VALIDATE( GIRDER );

   const CSegmentKey& segmentKey = poi.GetSegmentKey();

   // if the strands aren't released yet, then there isn't an eccentricty with respect to the girder cross section
   if ( intervalIdx < GetPrestressReleaseInterval(segmentKey) || poi.HasAttribute(POI_CLOSURE) || poi.HasAttribute(POI_PIER) )
      return 0;

   CComPtr<IPrecastGirder> girder;
   GetGirder(poi,&girder);

   CComPtr<IPoint2dCollection> points;
   girder->get_HarpedStrandPositions(poi.GetDistFromStart(),&points);

   CollectionIndexType nStrandPoints;
   points->get_Count(&nStrandPoints);

   Float64 ecc = 0.0;

   if (nStrandPoints == 0)
   {
      *nEffectiveStrands = 0.0;
   }
   else
   {
      // must account for partial bonding if poi is near end of girder
      // NOTE: bond factor is 1.0 if at girder ends
      Float64 poi_loc = poi.GetDistFromStart();
      Float64 bond_factor = 1.0;

      GET_IFACE(IPretensionForce,pPrestress);
      Float64 xfer_length = pPrestress->GetXferLength(segmentKey,pgsTypes::Harped);

      if (0.0 < poi_loc && poi_loc < xfer_length)
      {
         bond_factor = poi_loc/xfer_length;
      }
      else
      {
         Float64 girder_length = GetSegmentLength(segmentKey);

         if ( (girder_length-xfer_length) < poi_loc && poi_loc < girder_length)
         {
            bond_factor = (girder_length-poi_loc)/xfer_length;
         }
      }

      *nEffectiveStrands = (Float64)nStrandPoints*bond_factor;

      if (0 < nStrandPoints)
      {
         Float64 cg = 0.0;
         for (CollectionIndexType strandPointIdx = 0; strandPointIdx < nStrandPoints; strandPointIdx++)
         {
            CComPtr<IPoint2d> point;
            points->get_Item(strandPointIdx,&point);
            Float64 y;
            point->get_Y(&y);

            cg += y;
         }

         cg = cg / (Float64)nStrandPoints;

         Float64 Yb = GetYb(spType,intervalIdx,poi);
         ecc = Yb - cg;
      }
   }
   
   return ecc;
}

Float64 CBridgeAgentImp::GetSsEccentricity(pgsTypes::SectionPropertyType spType,IntervalIndexType intervalIdx,const pgsPointOfInterest& poi, Float64* nEffectiveStrands)
{
   ATLASSERT(spType != pgsTypes::sptNetDeck);

   VALIDATE( GIRDER );

   const CSegmentKey& segmentKey = poi.GetSegmentKey();

   // if the strands aren't released yet, then there isn't an eccentricty with respect to the girder cross section
   if ( intervalIdx < GetPrestressReleaseInterval(segmentKey) || poi.HasAttribute(POI_CLOSURE) || poi.HasAttribute(POI_PIER) )
      return 0;

   CComPtr<IPrecastGirder> girder;
   GetGirder(poi,&girder);

   StrandIndexType num_strands;
   girder->GetStraightStrandCount(&num_strands);
   if (0 < num_strands)
   {
      StrandIndexType num_bonded_strands = 0;
      Float64 num_eff_strands = 0.0; // weighted number of effective strands

      Float64 girder_length = GetSegmentLength(segmentKey);

      Float64 dist = poi.GetDistFromStart();

      // treat comp at ends different than in interior
      Float64 cg = 0.0;
      if (IsZero(dist))
      {
         // we are at the girder left end. Treat all bonded strands as full strength
         for (StrandIndexType strandIndex = 0; strandIndex < num_strands; strandIndex++)
         {
            Float64 yloc, left_debond, right_debond;
            girder->GetStraightStrandDebondLengthByPositionIndex(strandIndex, &yloc, &left_debond,&right_debond);

            if (left_debond==0.0)
            {
               // strand is not debonded, we don't care by how much
               cg += yloc;
               num_bonded_strands++;
            }
         }

         num_eff_strands = (Float64)num_bonded_strands;
      }
      else if (IsEqual(dist,girder_length))
      {
         // we are at the girder right end. Treat all bonded strands as full strength
         for (StrandIndexType strandIndex = 0; strandIndex < num_strands; strandIndex++)
         {
            Float64 yloc, left_debond, right_debond;
            girder->GetStraightStrandDebondLengthByPositionIndex(strandIndex, &yloc, &left_debond,&right_debond);

            if (right_debond==0.0)
            {
               cg += yloc;
               num_bonded_strands++;
            }
         }

         num_eff_strands = (Float64)num_bonded_strands;
      }
      else
      {
         // at the girder interior, must deal with partial bonding
         GET_IFACE(IPretensionForce,pPrestress);
         Float64 xfer_length = pPrestress->GetXferLength(segmentKey,pgsTypes::Straight);

         for (StrandIndexType strandIndex = 0; strandIndex < num_strands; strandIndex++)
         {
            Float64 yloc, left_bond, right_bond;
            girder->GetStraightStrandBondedLengthByPositionIndex(strandIndex, dist, &yloc,&left_bond,&right_bond);

            Float64 bond_length = min(left_bond, right_bond);
            
            if (bond_length <= 0.0) 
            {
               ;// do nothing if bond length is zero
            }
            else if (bond_length<xfer_length)
            {
               Float64 factor = bond_length/xfer_length;
               cg += yloc * factor;
               num_eff_strands += factor;
            }
            else
            {
               cg += yloc;
               num_eff_strands += 1.0;
            }
         }
      }

      *nEffectiveStrands = num_eff_strands;

      if (0.0 < num_eff_strands)
      {
         cg = cg/num_eff_strands;

         Float64 Yb = GetYb(spType,intervalIdx,poi);

         Float64 ecc = Yb - cg;
         return ecc;
      }
      else
      {
         // all strands debonded out
         return 0.0;
      }
   }
   else
   {
      // no strands
      *nEffectiveStrands = 0.0;
      return 0.0;
   }
}

Float64 CBridgeAgentImp::GetTempEccentricity(pgsTypes::SectionPropertyType spType,IntervalIndexType intervalIdx,const pgsPointOfInterest& poi, Float64* nEffectiveStrands)
{
   ATLASSERT(spType != pgsTypes::sptNetDeck);

   VALIDATE( GIRDER );

   const CSegmentKey& segmentKey = poi.GetSegmentKey();

   // if the strands aren't released yet, then there isn't an eccentricty with respect to the girder cross section
   if ( intervalIdx < GetPrestressReleaseInterval(segmentKey) || poi.HasAttribute(POI_CLOSURE) || poi.HasAttribute(POI_PIER) )
      return 0;

   CComPtr<IPrecastGirder> girder;
   GetGirder(poi,&girder);

   Float64 poi_loc = poi.GetDistFromStart();

   CComPtr<IPoint2dCollection> points;
   girder->get_TemporaryStrandPositions(poi_loc, &points);

   CollectionIndexType num_strand_positions;
   points->get_Count(&num_strand_positions);

   if (0 < num_strand_positions)
   {
      // must account for partial bonding if poi is near end of girder
      // NOTE: bond factor is 1.0 if at girder ends
      Float64 bond_factor = 1.0;

      GET_IFACE(IPretensionForce,pPrestress);
      Float64 xfer_length = pPrestress->GetXferLength(segmentKey,pgsTypes::Temporary);

      if (0.0 < poi_loc && poi_loc < xfer_length)
      {
         bond_factor = poi_loc/xfer_length;
      }
      else
      {
         Float64 girder_length = GetSegmentLength(segmentKey);

         if ( (girder_length-xfer_length) < poi_loc && poi_loc < girder_length)
         {
            bond_factor = (girder_length-poi_loc)/xfer_length;
         }
      }

      *nEffectiveStrands = num_strand_positions*bond_factor;

      Float64 cg=0.0;
      for (CollectionIndexType strandPositionIdx = 0; strandPositionIdx < num_strand_positions; strandPositionIdx++)
      {
         CComPtr<IPoint2d> point;
         points->get_Item(strandPositionIdx,&point);
         Float64 y;
         point->get_Y(&y);

         cg += y;
      }

      cg = cg / (Float64)num_strand_positions;

      Float64 Yb = GetYb(spType,intervalIdx,poi);

      Float64 ecc = Yb-cg;
      return ecc;
   }
   else
   {
      *nEffectiveStrands = 0.0;
      return 0.0;
   }
}

Float64 CBridgeAgentImp::GetApsBottomHalf(const pgsPointOfInterest& poi,DevelopmentAdjustmentType devAdjust)
{
   return GetApsTensionSide(poi, devAdjust, false );
}

Float64 CBridgeAgentImp::GetApsBottomHalf(const pgsPointOfInterest& poi, const GDRCONFIG& config,DevelopmentAdjustmentType devAdjust)
{
   return GetApsTensionSide(poi, config, devAdjust, false );
}

Float64 CBridgeAgentImp::GetApsTopHalf(const pgsPointOfInterest& poi,DevelopmentAdjustmentType devAdjust)
{
   return GetApsTensionSide(poi, devAdjust, true );
}

Float64 CBridgeAgentImp::GetApsTopHalf(const pgsPointOfInterest& poi, const GDRCONFIG& config,DevelopmentAdjustmentType devAdjust)
{
   return GetApsTensionSide(poi, config, devAdjust, true );
}

ConfigStrandFillVector CBridgeAgentImp::ComputeStrandFill(const CSegmentKey& segmentKey,pgsTypes::StrandType type,StrandIndexType Ns)
{
   VALIDATE( GIRDER );
   CComPtr<IPrecastGirder> girder;
   GetGirder(segmentKey,&girder);

   CContinuousStrandFiller* pFiller = GetContinuousStrandFiller(segmentKey);

   // Get fill in COM format
   CComPtr<IIndexArray> indexarr;
   switch( type )
   {
   case pgsTypes::Straight:
      pFiller->ComputeStraightStrandFill(girder, Ns, &indexarr);
      break;

   case pgsTypes::Harped:
      pFiller->ComputeHarpedStrandFill(girder, Ns, &indexarr);
      break;

   case pgsTypes::Temporary:
      pFiller->ComputeTemporaryStrandFill(girder, Ns, &indexarr);
      break;

   default:
      ATLASSERT(false); // should never get here
   }

   // Convert to ConfigStrandFillVector
   ConfigStrandFillVector Vec;
   IndexArray2ConfigStrandFillVec(indexarr, Vec);

   return Vec;
}

ConfigStrandFillVector CBridgeAgentImp::ComputeStrandFill(LPCTSTR strGirderName,pgsTypes::StrandType type,StrandIndexType Ns)
{
   GET_IFACE(ILibrary,pLib);
   const GirderLibraryEntry* pGdrEntry = pLib->GetGirderEntry(strGirderName);

   CStrandFiller filler;
   filler.Init(pGdrEntry);

   // Get fill in COM format
   CComPtr<IIndexArray> indexarr;
   switch( type )
   {
   case pgsTypes::Straight:
      {
         CComPtr<IStrandGrid> startGrid, endGrid;
         startGrid.CoCreateInstance(CLSID_StrandGrid);
         endGrid.CoCreateInstance(CLSID_StrandGrid);
         pGdrEntry->ConfigureStraightStrandGrid(startGrid,endGrid);

         CComQIPtr<IStrandGridFiller> gridFiller(startGrid);
         filler.ComputeStraightStrandFill(gridFiller, Ns, &indexarr);
      }
      break;

   case pgsTypes::Harped:
      {
         CComPtr<IStrandGrid> startGrid, startHPGrid, endHPGrid, endGrid;
         startGrid.CoCreateInstance(CLSID_StrandGrid);
         startHPGrid.CoCreateInstance(CLSID_StrandGrid);
         endHPGrid.CoCreateInstance(CLSID_StrandGrid);
         endGrid.CoCreateInstance(CLSID_StrandGrid);
         pGdrEntry->ConfigureHarpedStrandGrids(startGrid,startHPGrid,endHPGrid,endGrid);

         CComQIPtr<IStrandGridFiller> endGridFiller(startGrid), hpGridFiller(startHPGrid);
         filler.ComputeHarpedStrandFill(pGdrEntry->OddNumberOfHarpedStrands(),endGridFiller, hpGridFiller, Ns, &indexarr);
      }
      break;

   case pgsTypes::Temporary:
      {
         CComPtr<IStrandGrid> startGrid, endGrid;
         startGrid.CoCreateInstance(CLSID_StrandGrid);
         endGrid.CoCreateInstance(CLSID_StrandGrid);
         pGdrEntry->ConfigureTemporaryStrandGrid(startGrid,endGrid);

         CComQIPtr<IStrandGridFiller> gridFiller(startGrid);
         filler.ComputeTemporaryStrandFill(gridFiller, Ns, &indexarr);
      }
      break;

   default:
      ATLASSERT(false); // should never get here
   }

   // Convert IIndexArray to ConfigStrandFillVector
   ConfigStrandFillVector Vec;
   IndexArray2ConfigStrandFillVec(indexarr, Vec);

   return Vec;
}

GridIndexType CBridgeAgentImp::SequentialFillToGridFill(LPCTSTR strGirderName,pgsTypes::StrandType type,StrandIndexType StrandNo)
{
   GET_IFACE(ILibrary,pLib);
   const GirderLibraryEntry* pGdrEntry = pLib->GetGirderEntry(strGirderName);

   // Get fill in COM format
   StrandIndexType gridIdx(INVALID_INDEX);
   switch( type )
   {
   case pgsTypes::Straight:
      {
         CComPtr<IStrandGrid> startGrid, endGrid;
         startGrid.CoCreateInstance(CLSID_StrandGrid);
         endGrid.CoCreateInstance(CLSID_StrandGrid);
         pGdrEntry->ConfigureStraightStrandGrid(startGrid,endGrid);

         CComQIPtr<IStrandGridFiller> gridFiller(startGrid);

         CComPtr<IIndexArray> maxFill;
         gridFiller->GetMaxStrandFill(&maxFill); // sequential fill fills max from start

         gridFiller->StrandIndexToGridIndexEx(maxFill, StrandNo, &gridIdx);
      }
      break;

   case pgsTypes::Harped:
      {
         CComPtr<IStrandGrid> startGrid, startHPGrid, endHPGrid, endGrid;
         startGrid.CoCreateInstance(CLSID_StrandGrid);
         startHPGrid.CoCreateInstance(CLSID_StrandGrid);
         endHPGrid.CoCreateInstance(CLSID_StrandGrid);
         endGrid.CoCreateInstance(CLSID_StrandGrid);
         pGdrEntry->ConfigureHarpedStrandGrids(startGrid,startHPGrid,endHPGrid,endGrid);

         CComQIPtr<IStrandGridFiller> endGridFiller(startGrid);

         CComPtr<IIndexArray> maxFill;
         endGridFiller->GetMaxStrandFill(&maxFill); // sequential fill fills max from start

         endGridFiller->StrandIndexToGridIndexEx(maxFill, StrandNo, &gridIdx);
      }
      break;

   case pgsTypes::Temporary:
      {
         CComPtr<IStrandGrid> startGrid, endGrid;
         startGrid.CoCreateInstance(CLSID_StrandGrid);
         endGrid.CoCreateInstance(CLSID_StrandGrid);
         pGdrEntry->ConfigureTemporaryStrandGrid(startGrid,endGrid);

         CComQIPtr<IStrandGridFiller> gridFiller(startGrid);

         CComPtr<IIndexArray> maxFill;
         gridFiller->GetMaxStrandFill(&maxFill); // sequential fill fills max from start

         gridFiller->StrandIndexToGridIndexEx(maxFill, StrandNo, &gridIdx);
      }
      break;

   default:
      ATLASSERT(false); // should never get here
   }

   return gridIdx;
}

void CBridgeAgentImp::GridFillToSequentialFill(LPCTSTR strGirderName,pgsTypes::StrandType type,GridIndexType gridIdx, StrandIndexType* pStrandNo1, StrandIndexType* pStrandNo2)
{
   GET_IFACE(ILibrary,pLib);
   const GirderLibraryEntry* pGdrEntry = pLib->GetGirderEntry(strGirderName);

   // Get fill in COM format
   HRESULT hr;
   switch( type )
   {
   case pgsTypes::Straight:
      {
         CComPtr<IStrandGrid> startGrid, endGrid;
         startGrid.CoCreateInstance(CLSID_StrandGrid);
         endGrid.CoCreateInstance(CLSID_StrandGrid);
         pGdrEntry->ConfigureStraightStrandGrid(startGrid,endGrid);

         CComQIPtr<IStrandGridFiller> gridFiller(startGrid);
         hr = gridFiller->GridIndexToStrandIndex(gridIdx, pStrandNo1, pStrandNo2);
         ATLASSERT(SUCCEEDED(hr));
      }
      break;

   case pgsTypes::Harped:
      {
         CComPtr<IStrandGrid> startGrid, startHPGrid, endHPGrid, endGrid;
         startGrid.CoCreateInstance(CLSID_StrandGrid);
         startHPGrid.CoCreateInstance(CLSID_StrandGrid);
         endHPGrid.CoCreateInstance(CLSID_StrandGrid);
         endGrid.CoCreateInstance(CLSID_StrandGrid);
         pGdrEntry->ConfigureHarpedStrandGrids(startGrid,startHPGrid,endHPGrid,endGrid);

         CComQIPtr<IStrandGridFiller> endGridFiller(startGrid);
         hr = endGridFiller->GridIndexToStrandIndex(gridIdx, pStrandNo1, pStrandNo2);
         ATLASSERT(SUCCEEDED(hr));
      }
      break;

   case pgsTypes::Temporary:
      {
         CComPtr<IStrandGrid> startGrid, endGrid;
         startGrid.CoCreateInstance(CLSID_StrandGrid);
         endGrid.CoCreateInstance(CLSID_StrandGrid);
         pGdrEntry->ConfigureTemporaryStrandGrid(startGrid,endGrid);

         CComQIPtr<IStrandGridFiller> gridFiller(startGrid);
         hr = gridFiller->GridIndexToStrandIndex(gridIdx, pStrandNo1, pStrandNo2);
         ATLASSERT(SUCCEEDED(hr));
      }
      break;

   default:
      ATLASSERT(false); // should never get here
   }
}


StrandIndexType CBridgeAgentImp::GetNumStrands(const CSegmentKey& segmentKey,pgsTypes::StrandType type)
{
   CComPtr<IPrecastGirder> girder;
   GetGirder(segmentKey,&girder);

   StrandIndexType nStrands(0);
   if ( type == pgsTypes::Straight )
   {
      girder->GetStraightStrandCount(&nStrands);
   }
   else if ( type == pgsTypes::Harped )
   {
      girder->GetHarpedStrandCount(&nStrands);
   }
   else if ( type == pgsTypes::Temporary )
   {
      girder->GetTemporaryStrandCount(&nStrands);
   }
   else if ( type == pgsTypes::Permanent )
   {
      StrandIndexType nh, ns;
      girder->GetStraightStrandCount(&ns);
      girder->GetHarpedStrandCount(&nh);
      nStrands = ns + nh;
   }
   else
   {
      ATLASSERT(0);
   }

   return nStrands;
}

StrandIndexType CBridgeAgentImp::GetMaxStrands(const CSegmentKey& segmentKey,pgsTypes::StrandType type)
{
   CComPtr<IPrecastGirder> girder;
   GetGirder(segmentKey,&girder);

   StrandIndexType Ns, Nh;
   StrandIndexType nStrands;
   switch( type )
   {
   case pgsTypes::Permanent:
      girder->get_MaxStraightStrands(&Ns);
      girder->get_MaxHarpedStrands(&Nh);
      nStrands = Ns + Nh;
      break;

   case pgsTypes::Straight:
      girder->get_MaxStraightStrands(&nStrands);
      break;

   case pgsTypes::Harped:
      girder->get_MaxHarpedStrands(&nStrands);
      break;

   case pgsTypes::Temporary:
      girder->get_MaxTemporaryStrands(&nStrands);
      break;

   default:
      ATLASSERT(false); // should never get here
   }

   return nStrands;
}

StrandIndexType CBridgeAgentImp::GetMaxStrands(LPCTSTR strGirderName,pgsTypes::StrandType type)
{
   GET_IFACE(ILibrary,pLib);
   const GirderLibraryEntry* pGirderEntry = pLib->GetGirderEntry( strGirderName );

   ATLASSERT(pGirderEntry != NULL);
   if ( pGirderEntry == NULL )
      return 0;

   StrandIndexType Ns, Nh;
   StrandIndexType nStrands;
   switch( type )
   {
   case pgsTypes::Permanent:
      Ns = pGirderEntry->GetMaxStraightStrands();
      Nh = pGirderEntry->GetMaxHarpedStrands();
      nStrands = Ns + Nh;
      break;

   case pgsTypes::Straight:
      nStrands = pGirderEntry->GetMaxStraightStrands();
      break;

   case pgsTypes::Harped:
      nStrands = pGirderEntry->GetMaxHarpedStrands();
      break;

   case pgsTypes::Temporary:
      nStrands = pGirderEntry->GetMaxTemporaryStrands();
      break;

   default:
      ATLASSERT(false); // should never get here
   }

   return nStrands;
}

Float64 CBridgeAgentImp::GetStrandArea(const CSegmentKey& segmentKey,IntervalIndexType intervalIdx,pgsTypes::StrandType type)
{
   if ( intervalIdx < GetStressStrandInterval(segmentKey) )
      return 0; // strands aren't stressed so they aren't even in play yet

   StrandIndexType Ns = GetNumStrands(segmentKey,type);
   const matPsStrand* pStrand = GetStrandMaterial(segmentKey,type);

   Float64 aps = pStrand->GetNominalArea();
   Float64 Aps = aps * Ns;
   return Aps;
}

Float64 CBridgeAgentImp::GetAreaPrestressStrands(const CSegmentKey& segmentKey,IntervalIndexType intervalIdx,bool bIncTemp)
{
   Float64 Aps = GetStrandArea(segmentKey,intervalIdx,pgsTypes::Straight) + GetStrandArea(segmentKey,intervalIdx,pgsTypes::Harped);
   if ( bIncTemp )
      Aps += GetStrandArea(segmentKey,intervalIdx,pgsTypes::Temporary);

   return Aps;
}

Float64 CBridgeAgentImp::GetPjack(const CSegmentKey& segmentKey,pgsTypes::StrandType type)
{
   GET_IFACE(IBridgeDescription,pBridgeDesc);
   const CPrecastSegmentData* pSegment = pBridgeDesc->GetPrecastSegmentData(segmentKey);
   if ( type == pgsTypes::Permanent )
      return pSegment->Strands.Pjack[pgsTypes::Straight] + pSegment->Strands.Pjack[pgsTypes::Harped];
   else
      return pSegment->Strands.Pjack[type];
}

Float64 CBridgeAgentImp::GetPjack(const CSegmentKey& segmentKey,bool bIncTemp)
{
   Float64 Pj = GetPjack(segmentKey,pgsTypes::Straight) + GetPjack(segmentKey,pgsTypes::Harped);
   if ( bIncTemp )
      Pj += GetPjack(segmentKey,pgsTypes::Temporary);

   return Pj;
}

bool CBridgeAgentImp::GetAreHarpedStrandsForcedStraight(const CSegmentKey& segmentKey)
{
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CGirderGroupData* pGroup = pBridgeDesc->GetGirderGroup(segmentKey.groupIndex);
   const CSplicedGirderData* pGirder = pGroup->GetGirder(segmentKey.girderIndex);
   const GirderLibraryEntry* pGirderEntry = pGirder->GetGirderLibraryEntry();
   return pGirderEntry->IsForceHarpedStrandsStraight();
}

bool CBridgeAgentImp::GetAreHarpedStrandsForcedStraightEx(LPCTSTR strGirderName)
{
   GET_IFACE(ILibrary,pLib);
   const GirderLibraryEntry* pGirderEntry = pLib->GetGirderEntry( strGirderName );
   return pGirderEntry->IsForceHarpedStrandsStraight();
}

Float64 CBridgeAgentImp::GetGirderTopElevation(const CSegmentKey& segmentKey)
{
   CComPtr<IPrecastGirder> girder;
   GetGirder(segmentKey,&girder);

   Float64 top;
   girder->get_TopElevation(&top);

   return top;
}

Float64 CBridgeAgentImp::GetSplittingZoneHeight(const pgsPointOfInterest& poi)
{
   CComPtr<IGirderSection> girder_section;
   GetGirderSection(poi,pgsTypes::scBridge,&girder_section);

   CComQIPtr<IPrestressedGirderSection> psg_section(girder_section);

   Float64 splitting_zone_height;
   psg_section->get_SplittingZoneDimension(&splitting_zone_height);

   return splitting_zone_height;
}

pgsTypes::SplittingDirection CBridgeAgentImp::GetSplittingDirection(const CGirderKey& girderKey)
{
   CSegmentKey segmentKey(girderKey,0);

   CComPtr<IGirderSection> girder_section;
   pgsPointOfInterest poi(segmentKey,0.00);
   GetGirderSection(poi,pgsTypes::scBridge,&girder_section);

   CComQIPtr<IPrestressedGirderSection> psg_section(girder_section);
   SplittingDirection splitDir;
   psg_section->get_SplittingDirection(&splitDir);

   return (splitDir == sdVertical ? pgsTypes::sdVertical : pgsTypes::sdHorizontal);
}

void CBridgeAgentImp::GetHarpStrandOffsets(const CSegmentKey& segmentKey,Float64* pOffsetEnd,Float64* pOffsetHp)
{
   VALIDATE( GIRDER );

   CComPtr<IPrecastGirder> girder;
   GetGirder(segmentKey,&girder);
   girder->get_HarpedStrandAdjustmentEnd(pOffsetEnd);
   girder->get_HarpedStrandAdjustmentHP(pOffsetHp);
}

void CBridgeAgentImp::GetHarpedEndOffsetBounds(const CSegmentKey& segmentKey,Float64* DownwardOffset, Float64* UpwardOffset)
{
   VALIDATE( GIRDER );

   CComPtr<IPrecastGirder> girder;
   GetGirder(segmentKey,&girder);

   HRESULT hr = girder->GetHarpedEndAdjustmentBounds(DownwardOffset, UpwardOffset);
   ATLASSERT(SUCCEEDED(hr));
}

void CBridgeAgentImp::GetHarpedEndOffsetBoundsEx(const CSegmentKey& segmentKey,StrandIndexType Nh, Float64* DownwardOffset, Float64* UpwardOffset)
{
   VALIDATE( GIRDER );

   if (Nh==0)
   {
      *DownwardOffset = 0.0;
      *UpwardOffset = 0.0;
   }
   else
   {
      CComPtr<IPrecastGirder> girder;
      GetGirder(segmentKey,&girder);

      // use continuous interface to compute
      CComPtr<IIndexArray> fill;
      m_StrandFillers[segmentKey].ComputeHarpedStrandFill(girder, Nh, &fill);

      HRESULT hr = girder->GetHarpedEndAdjustmentBoundsEx(fill,DownwardOffset, UpwardOffset);
      ATLASSERT(SUCCEEDED(hr));
   }
}
void CBridgeAgentImp::GetHarpedEndOffsetBoundsEx(LPCTSTR strGirderName, const ConfigStrandFillVector& rHarpedFillArray, Float64* DownwardOffset, Float64* UpwardOffset)
{
   if ( !AreStrandsInConfigFillVec(rHarpedFillArray) )
   {
      // no strands, just return
      *DownwardOffset = 0.0;
      *UpwardOffset = 0.0;
      return;
   }

   // Set up strand grid filler
   GET_IFACE(ILibrary,pLib);
   const GirderLibraryEntry* pGdrEntry = pLib->GetGirderEntry(strGirderName);

   CComPtr<IStrandGrid> startGrid, startHPGrid, endHPGrid, endGrid;
   startGrid.CoCreateInstance(CLSID_StrandGrid);
   startHPGrid.CoCreateInstance(CLSID_StrandGrid);
   endHPGrid.CoCreateInstance(CLSID_StrandGrid);
   endGrid.CoCreateInstance(CLSID_StrandGrid);
   pGdrEntry->ConfigureHarpedStrandGrids(startGrid,startHPGrid,endHPGrid,endGrid);

   CComQIPtr<IStrandGridFiller> startGridFiller(startGrid);

   // Use wrapper to convert strand fill to IIndexArray
   CIndexArrayWrapper fill(rHarpedFillArray);

   // get adjusted top and bottom bounds
   Float64 top, bottom;
   HRESULT hr = startGridFiller->get_FilledGridBoundsEx(&fill,&bottom,&top);
   ATLASSERT(SUCCEEDED(hr));

   Float64 adjust;
   hr = startGridFiller->get_VerticalStrandAdjustment(&adjust);
   ATLASSERT(SUCCEEDED(hr));

   if (bottom == 0.0 && top == 0.0)
   {
      // no strands exist so we cannot adjust them
      *DownwardOffset = 0.0;
      *UpwardOffset   = 0.0;
   }
   else
   {
      bottom -= adjust;
      top    -= adjust;

      // get max locations of strands
      Float64 bottom_min, top_max;
      CComPtr<IStrandMover> strandMover;
      CreateStrandMover(strGirderName,&strandMover);
      hr = strandMover->get_EndStrandElevationBoundaries(&bottom_min, &top_max);
      ATLASSERT(SUCCEEDED(hr));

      *DownwardOffset = bottom_min - bottom;
      *DownwardOffset = IsZero(*DownwardOffset) ? 0.0 : *DownwardOffset;

      *UpwardOffset   = top_max - top;
      *UpwardOffset   = IsZero(*UpwardOffset)   ? 0.0 : *UpwardOffset;

      // if these fire, strands cannot be adjusted within section bounds. this should be caught at library entry time.
      ATLASSERT(*DownwardOffset<1.0e-06);
      ATLASSERT(*UpwardOffset>-1.0e-06);
   }
}

void CBridgeAgentImp::GetHarpedHpOffsetBounds(const CSegmentKey& segmentKey,Float64* DownwardOffset, Float64* UpwardOffset)
{
   VALIDATE( GIRDER );

   CComPtr<IPrecastGirder> girder;
   GetGirder(segmentKey,&girder);

   HRESULT hr = girder->GetHarpedHpAdjustmentBounds(DownwardOffset, UpwardOffset);
   ATLASSERT(SUCCEEDED(hr));
}

void CBridgeAgentImp::GetHarpedHpOffsetBoundsEx(const CSegmentKey& segmentKey,StrandIndexType Nh, Float64* DownwardOffset, Float64* UpwardOffset)
{
   VALIDATE( GIRDER );

   if (Nh==0)
   {
      *DownwardOffset = 0.0;
      *UpwardOffset = 0.0;
   }
   else
   {
      CComPtr<IPrecastGirder> girder;
      GetGirder(segmentKey,&girder);

      // use continuous interface to compute
      CComPtr<IIndexArray> fill;
      m_StrandFillers[segmentKey].ComputeHarpedStrandFill(girder, Nh, &fill);

      HRESULT hr = girder->GetHarpedHpAdjustmentBoundsEx(fill,DownwardOffset, UpwardOffset);
      ATLASSERT(SUCCEEDED(hr));
   }
}

void CBridgeAgentImp::GetHarpedHpOffsetBoundsEx(LPCTSTR strGirderName, const ConfigStrandFillVector& rHarpedFillArray, Float64* DownwardOffset, Float64* UpwardOffset)
{
   if ( !AreStrandsInConfigFillVec(rHarpedFillArray) )
   {
      *DownwardOffset = 0.0;
      *UpwardOffset = 0.0;
      return;
   }

   GET_IFACE(ILibrary,pLib);
   const GirderLibraryEntry* pGdrEntry = pLib->GetGirderEntry(strGirderName);

   CComPtr<IStrandGrid> startGrid, startHPGrid, endHPGrid, endGrid;
   startGrid.CoCreateInstance(CLSID_StrandGrid);
   startHPGrid.CoCreateInstance(CLSID_StrandGrid);
   endHPGrid.CoCreateInstance(CLSID_StrandGrid);
   endGrid.CoCreateInstance(CLSID_StrandGrid);
   pGdrEntry->ConfigureHarpedStrandGrids(startGrid,startHPGrid,endHPGrid,endGrid);

   CComQIPtr<IStrandGridFiller> startGridFiller(startGrid);
   CComQIPtr<IStrandGridFiller> hpGridFiller(startHPGrid);

   // Use wrapper to convert strand fill to IIndexArray
   CIndexArrayWrapper fill(rHarpedFillArray);

   // get adjusted top and bottom bounds
   Float64 top, bottom;
   HRESULT hr = hpGridFiller->get_FilledGridBoundsEx(&fill,&bottom,&top);
   ATLASSERT(SUCCEEDED(hr));

   Float64 adjust;
   hr = hpGridFiller->get_VerticalStrandAdjustment(&adjust);
   ATLASSERT(SUCCEEDED(hr));

   if (bottom == 0.0 && top == 0.0)
   {
      // no strands exist so we cannot adjust them
      *DownwardOffset = 0.0;
      *UpwardOffset   = 0.0;
   }
   else
   {
      bottom -= adjust;
      top    -= adjust;

      // get max locations of strands
      Float64 bottom_min, top_max;
      CComPtr<IStrandMover> strandMover;
      CreateStrandMover(strGirderName,&strandMover);
      hr = strandMover->get_HpStrandElevationBoundaries(&bottom_min, &top_max);
      ATLASSERT(SUCCEEDED(hr));

      *DownwardOffset = bottom_min - bottom;
      *DownwardOffset = IsZero(*DownwardOffset) ? 0.0 : *DownwardOffset;

      *UpwardOffset   = top_max - top;
      *UpwardOffset   = IsZero(*UpwardOffset)   ? 0.0 : *UpwardOffset;

      // if these fire, strands cannot be adjusted within section bounds. this should be caught at library entry time.
      ATLASSERT(*DownwardOffset<1.0e-06);
      ATLASSERT(*UpwardOffset>-1.0e-06);
   }
}

Float64 CBridgeAgentImp::GetHarpedEndOffsetIncrement(const CSegmentKey& segmentKey)
{
   VALIDATE( GIRDER );

   CComPtr<IPrecastGirder> girder;
   GetGirder(segmentKey,&girder);

   Float64 increment;

   HRESULT hr = girder->get_HarpedEndAdjustmentIncrement(&increment);
   ATLASSERT(SUCCEEDED(hr));

   return increment;
}

Float64 CBridgeAgentImp::GetHarpedEndOffsetIncrement(LPCTSTR strGirderName)
{
   GET_IFACE(ILibrary,pLib);
   const GirderLibraryEntry* pGirderEntry = pLib->GetGirderEntry(strGirderName);
   Float64 end_increment = pGirderEntry->IsVerticalAdjustmentAllowedEnd() ?  pGirderEntry->GetEndStrandIncrement() : -1.0;
   Float64 hp_increment  = pGirderEntry->IsVerticalAdjustmentAllowedHP()  ?  pGirderEntry->GetHPStrandIncrement()  : -1.0;
   return end_increment;
}

Float64 CBridgeAgentImp::GetHarpedHpOffsetIncrement(const CSegmentKey& segmentKey)
{
   VALIDATE( GIRDER );

   CComPtr<IPrecastGirder> girder;
   GetGirder(segmentKey,&girder);

   Float64 increment;

   HRESULT hr = girder->get_HarpedHpAdjustmentIncrement(&increment);
   ATLASSERT(SUCCEEDED(hr));

   return increment;
}

Float64 CBridgeAgentImp::GetHarpedHpOffsetIncrement(LPCTSTR strGirderName)
{
   GET_IFACE(ILibrary,pLib);
   const GirderLibraryEntry* pGirderEntry = pLib->GetGirderEntry(strGirderName);
   Float64 end_increment = pGirderEntry->IsVerticalAdjustmentAllowedEnd() ?  pGirderEntry->GetEndStrandIncrement() : -1.0;
   Float64 hp_increment  = pGirderEntry->IsVerticalAdjustmentAllowedHP()  ?  pGirderEntry->GetHPStrandIncrement()  : -1.0;
   return hp_increment;
}

void CBridgeAgentImp::GetHarpingPointLocations(const CSegmentKey& segmentKey,Float64* lhp,Float64* rhp)
{
   VALIDATE( GIRDER );

   CComPtr<IPrecastGirder> girder;
   GetGirder(segmentKey,&girder);

   girder->GetHarpingPointLocations(lhp,rhp);
}

void CBridgeAgentImp::GetHighestHarpedStrandLocation(const CSegmentKey& segmentKey,Float64* pElevation)
{
   // determine distance from bottom of girder to highest harped strand at end of girder 
   // to compute the txdot ibns TO value
   VALIDATE( GIRDER );

   CComPtr<IPrecastGirder> girder;
   GetGirder(segmentKey,&girder);

   CComPtr<IRect2d> boxStart;
   HRESULT hr = girder->HarpedEndStrandBoundingBox(etStart,&boxStart);
   ATLASSERT(SUCCEEDED(hr));

   Float64 startTop;
   boxStart->get_Top(&startTop);

   CComPtr<IRect2d> boxEnd;
   hr = girder->HarpedEndStrandBoundingBox(etEnd,&boxEnd);
   ATLASSERT(SUCCEEDED(hr));

   Float64 endTop;
   boxEnd->get_Top(&endTop);

   *pElevation = max(startTop,endTop);
}

IndexType CBridgeAgentImp::GetNumHarpPoints(const CSegmentKey& segmentKey)
{
   CComPtr<IPrecastGirder> girder;
   GetGirder(segmentKey,&girder);

   StrandIndexType maxHarped;
   girder->get_MaxHarpedStrands(&maxHarped);

   if(maxHarped == 0)
      return 0;

   Float64 lhp, rhp;
   GetHarpingPointLocations(segmentKey, &lhp, &rhp);

   if (IsEqual(lhp,rhp))
      return 1;
   else
      return 2;
}


StrandIndexType CBridgeAgentImp::GetNextNumStrands(const CSegmentKey& segmentKey,pgsTypes::StrandType type,StrandIndexType curNum)
{
   StrandIndexType ns;
   switch(type)
   {
   case pgsTypes::Straight:
      ns = GetNextNumStraightStrands(segmentKey,curNum);
      break;

   case pgsTypes::Harped:
      ns = GetNextNumHarpedStrands(segmentKey,curNum);
      break;

   case pgsTypes::Temporary:
      ns = GetNextNumTempStrands(segmentKey,curNum);
      break;

   default:
      ATLASSERT(false); // should never get here
   }

   return ns;
}
StrandIndexType CBridgeAgentImp::GetNextNumStrands(LPCTSTR strGirderName,pgsTypes::StrandType type,StrandIndexType curNum)
{
   StrandIndexType ns;
   switch(type)
   {
   case pgsTypes::Straight:
      ns = GetNextNumStraightStrands(strGirderName,curNum);
      break;

   case pgsTypes::Harped:
      ns = GetNextNumHarpedStrands(strGirderName,curNum);
      break;

   case pgsTypes::Temporary:
      ns = GetNextNumTempStrands(strGirderName,curNum);
      break;

   default:
      ATLASSERT(false); // should never get here
   }

   return ns;
}


StrandIndexType CBridgeAgentImp::GetPrevNumStrands(const CSegmentKey& segmentKey,pgsTypes::StrandType type,StrandIndexType curNum)
{
   VALIDATE( GIRDER );

   StrandIndexType ns;
   if (curNum==0)
   {
      ns = 0;
   }
   else
   {
      switch(type)
      {
      case pgsTypes::Straight:
         ns = GetPrevNumStraightStrands(segmentKey,curNum);
         break;

      case pgsTypes::Harped:
         ns = GetPrevNumHarpedStrands(segmentKey,curNum);
         break;

      case pgsTypes::Temporary:
         ns = GetPrevNumTempStrands(segmentKey,curNum);
         break;

      default:
         ATLASSERT(false); // should never get here
      }
   }

   return ns;
}

StrandIndexType CBridgeAgentImp::GetPrevNumStrands(LPCTSTR strGirderName,pgsTypes::StrandType type,StrandIndexType curNum)
{
   if ( curNum == 0 )
      return 0;

   StrandIndexType ns;
   switch(type)
   {
   case pgsTypes::Straight:
      ns = GetPrevNumStraightStrands(strGirderName,curNum);
      break;

   case pgsTypes::Harped:
      ns = GetPrevNumHarpedStrands(strGirderName,curNum);
      break;

   case pgsTypes::Temporary:
      ns = GetPrevNumTempStrands(strGirderName,curNum);
      break;

   default:
      ATLASSERT(false); // should never get here
   }

   return ns;
}

StrandIndexType CBridgeAgentImp::GetNumExtendedStrands(const CSegmentKey& segmentKey,pgsTypes::MemberEndType endType,pgsTypes::StrandType strandType)
{
   GDRCONFIG config = GetSegmentConfiguration(segmentKey);
   return config.PrestressConfig.GetExtendedStrands(strandType,endType).size();
}

bool CBridgeAgentImp::IsExtendedStrand(const CSegmentKey& segmentKey,pgsTypes::MemberEndType end,StrandIndexType strandIdx,pgsTypes::StrandType strandType)
{
   GDRCONFIG config = GetSegmentConfiguration(segmentKey);
   return IsExtendedStrand(segmentKey,end,strandIdx,strandType,config);
}

bool CBridgeAgentImp::IsExtendedStrand(const CSegmentKey& segmentKey,pgsTypes::MemberEndType endType,StrandIndexType strandIdx,pgsTypes::StrandType strandType,const GDRCONFIG& config)
{
   if ( config.PrestressConfig.GetExtendedStrands(strandType,endType).size() == 0 )
      return false;

   const std::vector<StrandIndexType>& extStrands = config.PrestressConfig.GetExtendedStrands(strandType,endType);
   std::vector<StrandIndexType>::const_iterator iter(extStrands.begin());
   std::vector<StrandIndexType>::const_iterator endIter(extStrands.end());
   for ( ; iter != endIter; iter++ )
   {
      if (*iter == strandIdx)
         return true;
   }

   return false;
}

bool CBridgeAgentImp::IsExtendedStrand(const pgsPointOfInterest& poi,StrandIndexType strandIdx,pgsTypes::StrandType strandType)
{
   Float64 Lg = GetSegmentLength(poi.GetSegmentKey());
   pgsTypes::MemberEndType end = (poi.GetDistFromStart() < Lg/2 ? pgsTypes::metStart : pgsTypes::metEnd);
   return IsExtendedStrand(poi.GetSegmentKey(),end,strandIdx,strandType);
}

bool CBridgeAgentImp::IsExtendedStrand(const pgsPointOfInterest& poi,StrandIndexType strandIdx,pgsTypes::StrandType strandType,const GDRCONFIG& config)
{
   Float64 Lg = GetSegmentLength(poi.GetSegmentKey());
   pgsTypes::MemberEndType end = (poi.GetDistFromStart() < Lg/2 ? pgsTypes::metStart : pgsTypes::metEnd);
   return IsExtendedStrand(poi.GetSegmentKey(),end,strandIdx,strandType,config);
}

bool CBridgeAgentImp::IsStrandDebonded(const CSegmentKey& segmentKey,StrandIndexType strandIdx,pgsTypes::StrandType strandType,Float64* pStart,Float64* pEnd)
{
   GDRCONFIG config = GetSegmentConfiguration(segmentKey);
   return IsStrandDebonded(segmentKey,strandIdx,strandType,config,pStart,pEnd);
}

bool CBridgeAgentImp::IsStrandDebonded(const CSegmentKey& segmentKey,StrandIndexType strandIdx,pgsTypes::StrandType strandType,const GDRCONFIG& config,Float64* pStart,Float64* pEnd)
{
   VALIDATE( GIRDER );

   CComPtr<IPrecastGirder> girder;
   GetGirder(segmentKey,&girder);

   Float64 length = GetSegmentLength(segmentKey);

   bool bDebonded = false;
   *pStart = 0.0;
   *pEnd   = 0.0;

   DebondConfigConstIterator iter;
   for ( iter = config.PrestressConfig.Debond[strandType].begin(); iter != config.PrestressConfig.Debond[strandType].end(); iter++ )
   {
      const DEBONDCONFIG& di = *iter;
      if ( strandIdx == di.strandIdx )
      {
         *pStart = di.LeftDebondLength;
         *pEnd   = length - di.RightDebondLength;
         bDebonded = true;
         break;
      }
   }

   if ( bDebonded && (length < *pStart) )
      *pStart = length;

   if ( bDebonded && (length < *pEnd) )
      *pEnd = 0;

   // if not debonded, bond starts at 0 and ends at the other end of the girder
   if ( !bDebonded )
   {
      *pStart = 0;
      *pEnd = 0;
   }

   return bDebonded;
}

//-----------------------------------------------------------------------------
bool CBridgeAgentImp::IsStrandDebonded(const pgsPointOfInterest& poi,
                                       StrandIndexType strandIdx,
                                       pgsTypes::StrandType strandType)
{
   VALIDATE( GIRDER );

   const CSegmentKey& segmentKey = poi.GetSegmentKey();

   Float64 debond_left, debond_right;
   if ( !IsStrandDebonded(segmentKey,strandIdx,strandType,&debond_left,&debond_right) )
      return false;

   Float64 location = poi.GetDistFromStart();

   if ( location <= debond_left || debond_right <= location )
      return true;

   return false;
}

//-----------------------------------------------------------------------------
StrandIndexType CBridgeAgentImp::GetNumDebondedStrands(const CSegmentKey& segmentKey,
                                                       pgsTypes::StrandType strandType)
{
   VALIDATE( GIRDER );

   CComPtr<IPrecastGirder> girder;
   GetGirder(segmentKey,&girder);

   StrandIndexType nDebondedStrands = 0;
   HRESULT hr;
   switch( strandType )
   {
   case pgsTypes::Permanent: // drop through
   case pgsTypes::Straight:
      hr = girder->GetStraightStrandDebondCount(&nDebondedStrands);
      break;

   case pgsTypes::Harped: // drop through

   case pgsTypes::Temporary:
      hr = S_FALSE; // Assumed only bonded for the end 10'... PS force is constant through the debonded section
                    // this is different than strands debonded at the ends and bonded in the middle
                    // Treat this strand as bonded
      break;

   default:
      ATLASSERT(false); // should never get here
   }

   return nDebondedStrands;
}

//-----------------------------------------------------------------------------
RowIndexType CBridgeAgentImp::GetNumRowsWithStrand(const CSegmentKey& segmentKey,pgsTypes::StrandType strandType )
{
   StrandIndexType nStrands = GetNumStrands(segmentKey,strandType);
   return GetNumRowsWithStrand(segmentKey,nStrands,strandType);
}

//-----------------------------------------------------------------------------
StrandIndexType CBridgeAgentImp::GetNumStrandInRow(const CSegmentKey& segmentKey,
                                                   RowIndexType rowIdx,
                                                   pgsTypes::StrandType strandType )
{
   StrandIndexType nStrands = GetNumStrands(segmentKey,strandType);
   return GetNumStrandInRow(segmentKey,nStrands,rowIdx,strandType);
}

std::vector<StrandIndexType> CBridgeAgentImp::GetStrandsInRow(const CSegmentKey& segmentKey, RowIndexType rowIdx, pgsTypes::StrandType strandType )
{
   StrandIndexType nStrands = GetNumStrands(segmentKey,strandType);
   return GetStrandsInRow(segmentKey,nStrands,rowIdx,strandType);
}

//-----------------------------------------------------------------------------
StrandIndexType CBridgeAgentImp::GetNumDebondedStrandsInRow(const CSegmentKey& segmentKey,
                                                            RowIndexType rowIdx,
                                                            pgsTypes::StrandType strandType )
{
   VALIDATE( GIRDER );

   CComPtr<IPrecastGirder> girder;
   GetGirder(segmentKey,&girder);

   StrandIndexType nDebondedStrands = 0;
   HRESULT hr;
   switch( strandType )
   {
   case pgsTypes::Straight:
      hr = girder->get_StraightStrandDebondInRow(rowIdx,&nDebondedStrands);
      break;

   case pgsTypes::Harped:
   case pgsTypes::Temporary:
      hr = S_FALSE; // Assumed only bonded for the end 10'... PS force is constant through the debonded section
                    // this is different than strands debonded at the ends and bonded in the middle
                    // Treat this strand as bonded
      break;

   default:
      ATLASSERT(false); // should never get here
   }

   return nDebondedStrands;
}

//-----------------------------------------------------------------------------
bool CBridgeAgentImp::IsExteriorStrandDebondedInRow(const CSegmentKey& segmentKey,
                                                    RowIndexType rowIdx,
                                                    pgsTypes::StrandType strandType)
{
   VALIDATE( GIRDER );

   CComPtr<IPrecastGirder> girder;
   GetGirder(segmentKey,&girder);

   VARIANT_BOOL bExteriorDebonded = VARIANT_FALSE;
   HRESULT hr;
   switch( strandType )
   {
   case pgsTypes::Straight:
      hr = girder->IsExteriorStraightStrandDebondedInRow(rowIdx,&bExteriorDebonded);
      break;

   case pgsTypes::Harped:
   case pgsTypes::Temporary:
      hr = S_FALSE; // Assumed only bonded for the end 10'... PS force is constant through the debonded section
                    // this is different than strands debonded at the ends and bonded in the middle
                    // Treat this strand as bonded
      break;

   default:
      ATLASSERT(false); // should never get here
   }

   return bExteriorDebonded == VARIANT_FALSE ? false : true;
}

//-----------------------------------------------------------------------------
Float64 CBridgeAgentImp::GetDebondSection(const CSegmentKey& segmentKey,pgsTypes::MemberEndType endType,SectionIndexType sectionIdx,pgsTypes::StrandType strandType)
{
   VALIDATE( GIRDER );

   CComPtr<IPrecastGirder> girder;
   GetGirder(segmentKey,&girder);

   HRESULT hr;
   CComPtr<IDblArray> arrLeft, arrRight;
   switch( strandType )
   {
   case pgsTypes::Straight:
      hr = girder->GetStraightStrandDebondAtSections(&arrLeft,&arrRight);
      break;

   case pgsTypes::Harped:
   case pgsTypes::Temporary:
      hr = S_FALSE; // Assumed only bonded for the end 10'... PS force is constant through the debonded section
                    // this is different than strands debonded at the ends and bonded in the middle
                    // Treat this strand as bonded
      return 0;
      break;

   default:
      ATLASSERT(false); // should never get here
   }

   Float64 location;
   if ( endType == pgsTypes::metStart )
   {
      // left end
      arrLeft->get_Item(sectionIdx,&location);
   }
   else
   {
      // right
      arrRight->get_Item(sectionIdx,&location);

      // measure section location from the left end
      // so all return values are consistent
      Float64 gdr_length = GetSegmentLength(segmentKey);
      location = gdr_length - location;
   }

   return location;
}

//-----------------------------------------------------------------------------
SectionIndexType CBridgeAgentImp::GetNumDebondSections(const CSegmentKey& segmentKey,pgsTypes::MemberEndType endType,pgsTypes::StrandType strandType)
{
   VALIDATE( GIRDER );

   CComPtr<IPrecastGirder> girder;
   GetGirder(segmentKey,&girder);

   HRESULT hr;
   CComPtr<IDblArray> arrLeft, arrRight;
   switch( strandType )
   {
   case pgsTypes::Straight:
      hr = girder->GetStraightStrandDebondAtSections(&arrLeft,&arrRight);
      break;

   case pgsTypes::Harped:
   case pgsTypes::Temporary:
      hr = S_FALSE; // Assumed only bonded for the end 10'... PS force is constant through the debonded section
                    // this is different than strands debonded at the ends and bonded in the middle
                    // Treat this strand as bonded
      return 0;
      break;

   default:
      ATLASSERT(false); // should never get here
   }

   CollectionIndexType c1;
   if ( endType == pgsTypes::metStart )
   {
      arrLeft->get_Count(&c1);
   }
   else
   {
      arrRight->get_Count(&c1);
   }

   ASSERT(c1 <= Uint16_Max); // make sure there is no loss of data
   return Uint16(c1);
}

//-----------------------------------------------------------------------------
StrandIndexType CBridgeAgentImp::GetNumDebondedStrandsAtSection(const CSegmentKey& segmentKey,pgsTypes::MemberEndType endType,SectionIndexType sectionIdx,pgsTypes::StrandType strandType)
{
   VALIDATE( GIRDER );

   CComPtr<IPrecastGirder> girder;
   GetGirder(segmentKey,&girder);

   switch( strandType )
   {

   case pgsTypes::Harped:
   case pgsTypes::Temporary:
                    // Assumed only bonded for the end 10'... PS force is constant through the debonded section
                    // this is different than strands debonded at the ends and bonded in the middle
                    // Treat this strand as bonded
      return 0;
      break;

   case pgsTypes::Straight:
      break; // fall through to below


   default:
      ATLASSERT(false); // should never get here
      return 0;
   }

   HRESULT hr;
   CollectionIndexType nStrands;
   if (endType == pgsTypes::metStart )
   {
      // left
      CComPtr<IIndexArray> strands;
      hr = girder->GetStraightStrandDebondAtLeftSection(sectionIdx,&strands);
      ATLASSERT(SUCCEEDED(hr));
      strands->get_Count(&nStrands);
   }
   else
   {
      CComPtr<IIndexArray> strands;
      hr = girder->GetStraightStrandDebondAtRightSection(sectionIdx,&strands);
      ATLASSERT(SUCCEEDED(hr));
      strands->get_Count(&nStrands);
   }

   return StrandIndexType(nStrands);
}

//-----------------------------------------------------------------------------
StrandIndexType CBridgeAgentImp::GetNumBondedStrandsAtSection(const CSegmentKey& segmentKey,pgsTypes::MemberEndType endType,SectionIndexType sectionIdx,pgsTypes::StrandType strandType)
{
   VALIDATE( GIRDER );

   CComPtr<IPrecastGirder> girder;
   GetGirder(segmentKey,&girder);

   StrandIndexType nStrands = GetNumStrands(segmentKey,strandType);

   HRESULT hr;
   CComPtr<IDblArray> arrLeft, arrRight;
   switch( strandType )
   {
   case pgsTypes::Straight:
      hr = girder->GetStraightStrandDebondAtSections(&arrLeft,&arrRight);
      ATLASSERT(SUCCEEDED(hr));
      break;

   case pgsTypes::Harped:
   case pgsTypes::Temporary:
                    // Assumed only bonded for the end 10'... PS force is constant through the debonded section
                    // this is different than strands debonded at the ends and bonded in the middle
                    // Treat this strand as bonded
      return 0;
      break;

   default:
      ATLASSERT(false); // should never get here
   }

   // all strands are straight from here on
   StrandIndexType nDebondedStrands = 0;
   if (endType == pgsTypes::metStart )
   {
      CollectionIndexType nSections;
      arrLeft->get_Count(&nSections);
      ATLASSERT(sectionIdx < nSections);

      // left
      // how many strands are debonded at this section and all the ones after it?
      for ( CollectionIndexType idx = sectionIdx; idx < nSections; idx++)
      {
         CComPtr<IIndexArray> strands;
         hr = girder->GetStraightStrandDebondAtLeftSection(idx,&strands);
         ATLASSERT(SUCCEEDED(hr));
         CollectionIndexType nDebondedStrandsAtSection;
         strands->get_Count(&nDebondedStrandsAtSection);

         nDebondedStrands += nDebondedStrandsAtSection;
      }
   }
   else
   {
      // right
      CollectionIndexType nSections;
      arrRight->get_Count(&nSections);
      ATLASSERT(sectionIdx < nSections);

      for ( CollectionIndexType idx = sectionIdx; idx < nSections; idx++ )
      {
         CComPtr<IIndexArray> strands;
         hr = girder->GetStraightStrandDebondAtRightSection(idx,&strands);
         ATLASSERT(SUCCEEDED(hr));
         CollectionIndexType nDebondedStrandsAtSection;
         strands->get_Count(&nDebondedStrandsAtSection);

         nDebondedStrands += nDebondedStrandsAtSection;
      }
   }

   ATLASSERT(nDebondedStrands<=nStrands);

   return nStrands - nDebondedStrands;
}

std::vector<StrandIndexType> CBridgeAgentImp::GetDebondedStrandsAtSection(const CSegmentKey& segmentKey,pgsTypes::MemberEndType endType,SectionIndexType sectionIdx,pgsTypes::StrandType strandType)
{
   VALIDATE( GIRDER );

   std::vector<StrandIndexType> debondedStrands;

   CComPtr<IPrecastGirder> girder;
   GetGirder(segmentKey,&girder);

   switch( strandType )
   {

   case pgsTypes::Harped:
   case pgsTypes::Temporary:
                    // Assumed only bonded for the end 10'... PS force is constant through the debonded section
                    // this is different than strands debonded at the ends and bonded in the middle
                    // Treat this strand as bonded
      return debondedStrands;
      break;

   case pgsTypes::Straight:
      break; // fall through to below


   default:
      ATLASSERT(false); // should never get here
      return debondedStrands;
   }

   HRESULT hr;
   CollectionIndexType nStrands;
   CComPtr<IIndexArray> strands;
   if (endType == pgsTypes::metStart)
   {
      // left
      hr = girder->GetStraightStrandDebondAtLeftSection(sectionIdx,&strands);
   }
   else
   {
      hr = girder->GetStraightStrandDebondAtRightSection(sectionIdx,&strands);
   }
   ATLASSERT(SUCCEEDED(hr));
   strands->get_Count(&nStrands);

   for ( IndexType idx = 0; idx < nStrands; idx++ )
   {
      StrandIndexType strandIdx;
      strands->get_Item(idx,&strandIdx);
      debondedStrands.push_back(strandIdx);
   }

   return debondedStrands;
}

//-----------------------------------------------------------------------------
bool CBridgeAgentImp::CanDebondStrands(const CSegmentKey& segmentKey,pgsTypes::StrandType strandType)
{
   VALIDATE( BRIDGE );

   if ( strandType == pgsTypes::Harped || strandType == pgsTypes::Temporary )
      return false;

   const GirderLibraryEntry* pGirderEntry = GetGirderLibraryEntry(segmentKey);

   return pGirderEntry->CanDebondStraightStrands();
}

bool CBridgeAgentImp::CanDebondStrands(LPCTSTR strGirderName,pgsTypes::StrandType strandType)
{
   if ( strandType == pgsTypes::Harped || strandType == pgsTypes::Temporary )
      return false;

   GET_IFACE(ILibrary,pLib);
   const GirderLibraryEntry* pGirderEntry = pLib->GetGirderEntry( strGirderName );

   return pGirderEntry->CanDebondStraightStrands();
}

//-----------------------------------------------------------------------------
void CBridgeAgentImp::ListDebondableStrands(const CSegmentKey& segmentKey,const ConfigStrandFillVector& rFillArray,pgsTypes::StrandType strandType, IIndexArray** list)
{
   GET_IFACE(ILibrary,pLib);

   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CGirderGroupData* pGroup = pBridgeDesc->GetGirderGroup(segmentKey.groupIndex);
   const CSplicedGirderData* pGirder = pGroup->GetGirder(segmentKey.girderIndex);

   ListDebondableStrands(pGirder->GetGirderName(),rFillArray,strandType,list);
}

void CBridgeAgentImp::ListDebondableStrands(LPCTSTR strGirderName,const ConfigStrandFillVector& rFillArray,pgsTypes::StrandType strandType, IIndexArray** list)
{
   CComPtr<IIndexArray> debondableStrands;  // array index = strand index, value = {0 means can't debond, 1 means can debond}
   debondableStrands.CoCreateInstance(CLSID_IndexArray);
   ATLASSERT(debondableStrands);

   GET_IFACE(ILibrary,pLib);
   const GirderLibraryEntry* pGirderEntry = pLib->GetGirderEntry( strGirderName );

   if ( strandType == pgsTypes::Straight)
   {
      ConfigStrandFillConstIterator it = rFillArray.begin();
      ConfigStrandFillConstIterator itend = rFillArray.end();

      GridIndexType gridIdx = 0;
      while(it != itend)
      {
         StrandIndexType nfill = *it;
         if (0 < nfill)
         {
            Float64 start_x, start_y, end_x, end_y;
            bool can_debond;
            pGirderEntry->GetStraightStrandCoordinates(gridIdx, &start_x, &start_y, &end_x, &end_y, &can_debond);

            debondableStrands->Add(can_debond?1:0);

            // have two strands?
            if (nfill==2)
            {
               ATLASSERT(0 < start_x && 0 < end_x);
               debondableStrands->Add(can_debond?1:0);
            }
         }

         gridIdx++;
         it++;
      }
   }
   else
   {
      // temp and harped. Redim zeros array
      debondableStrands->ReDim( PRESTRESSCONFIG::CountStrandsInFill(rFillArray) );
   }

   debondableStrands.CopyTo(list);
}

//-----------------------------------------------------------------------------
Float64 CBridgeAgentImp::GetDefaultDebondLength(const CSegmentKey& segmentKey)
{
   const GirderLibraryEntry* pGirderEntry = GetGirderLibraryEntry(segmentKey);
   return pGirderEntry->GetDefaultDebondSectionLength();
}

//-----------------------------------------------------------------------------
bool CBridgeAgentImp::IsDebondingSymmetric(const CSegmentKey& segmentKey)
{
   StrandIndexType Ns = GetNumDebondedStrands(segmentKey,pgsTypes::Straight);
   StrandIndexType Nh = GetNumDebondedStrands(segmentKey,pgsTypes::Harped);
   StrandIndexType Nt = GetNumDebondedStrands(segmentKey,pgsTypes::Temporary);

   // if there are no debonded strands then get the heck outta here
   if ( Ns == 0 && Nh == 0 && Nt == 0)
      return true;

   Float64 length = GetSegmentLength(segmentKey);

   // check the debonding to see if it is symmetric
   for ( int i = 0; i < 3; i++ )
   {
      pgsTypes::StrandType strandType = (pgsTypes::StrandType)(i);
      StrandIndexType nDebonded = GetNumDebondedStrands(segmentKey,strandType);
      if ( nDebonded == 0 )
         continue;

      StrandIndexType n = GetNumStrands(segmentKey,strandType);
      for ( StrandIndexType strandIdx = 0; strandIdx < n; strandIdx++ )
      {
         Float64 start, end;

         bool bIsDebonded = IsStrandDebonded(segmentKey,strandIdx,strandType,&start,&end);
         if ( bIsDebonded && !IsEqual(start,length-end) )
            return false;
      }
   }

   return true;
}

RowIndexType CBridgeAgentImp::GetNumRowsWithStrand(const CSegmentKey& segmentKey,StrandIndexType nStrands,pgsTypes::StrandType strandType )
{
   VALIDATE( GIRDER );

   CComPtr<IPrecastGirder> girder;
   GetGirder(segmentKey,&girder);

   CComPtr<IIndexArray> oldFill;
   CComPtr<IIndexArray> fill;

   RowIndexType nRows = 0;
   HRESULT hr;
   switch( strandType )
   {
   case pgsTypes::Straight:
      m_StrandFillers[segmentKey].ComputeStraightStrandFill(girder, nStrands, &fill);
      girder->get_StraightStrandFill(&oldFill);
      girder->put_StraightStrandFill(fill);
      hr = girder->get_StraightStrandRowsWithStrand(&nRows);
      girder->put_StraightStrandFill(oldFill);
      break;

   case pgsTypes::Harped:
      m_StrandFillers[segmentKey].ComputeHarpedStrandFill(girder, nStrands, &fill);
      girder->get_HarpedStrandFill(&oldFill);
      girder->put_HarpedStrandFill(fill);
      hr = girder->get_HarpedStrandRowsWithStrand(&nRows);
      girder->put_HarpedStrandFill(oldFill);
      break;

   case pgsTypes::Temporary:
      hr = S_FALSE; // Assumed only bonded for the end 10'... PS force is constant through the debonded section
                    // this is different than strands debonded at the ends and bonded in the middle
                    // Treat this strand as bonded
      break;

   default:
      ATLASSERT(false); // should never get here
   }

   return nRows;
}

StrandIndexType CBridgeAgentImp::GetNumStrandInRow(const CSegmentKey& segmentKey,StrandIndexType nStrands,RowIndexType rowIdx,pgsTypes::StrandType strandType )
{
   VALIDATE( GIRDER );

   CComPtr<IPrecastGirder> girder;
   GetGirder(segmentKey,&girder);

   CComPtr<IIndexArray> oldFill;
   CComPtr<IIndexArray> fill;

   StrandIndexType cStrands = 0;
   HRESULT hr;
   switch( strandType )
   {
   case pgsTypes::Straight:
      m_StrandFillers[segmentKey].ComputeStraightStrandFill(girder, nStrands, &fill);
      girder->get_StraightStrandFill(&oldFill);
      girder->put_StraightStrandFill(fill);
      hr = girder->get_NumStraightStrandsInRow(rowIdx,&cStrands);
      girder->put_StraightStrandFill(oldFill);
      break;

   case pgsTypes::Harped:
      m_StrandFillers[segmentKey].ComputeHarpedStrandFill(girder, nStrands, &fill);
      girder->get_HarpedStrandFill(&oldFill);
      girder->put_HarpedStrandFill(fill);
      hr = girder->get_NumHarpedStrandsInRow(rowIdx,&cStrands);
      girder->put_HarpedStrandFill(oldFill);
      break;

   case pgsTypes::Temporary:
      hr = S_FALSE; // Assumed only bonded for the end 10'... PS force is constant through the debonded section
                    // this is different than strands debonded at the ends and bonded in the middle
                    // Treat this strand as bonded
      break;

   default:
      ATLASSERT(false); // should never get here
   }

   return cStrands;
}

std::vector<StrandIndexType> CBridgeAgentImp::GetStrandsInRow(const CSegmentKey& segmentKey,StrandIndexType nStrands,RowIndexType rowIdx, pgsTypes::StrandType strandType )
{
   std::vector<StrandIndexType> strandIdxs;
   if ( strandType == pgsTypes::Temporary )
   {
      ATLASSERT(false); // shouldn't get here
      return strandIdxs;
   }

   VALIDATE( GIRDER );

   CComPtr<IPrecastGirder> girder;
   GetGirder(segmentKey,&girder);

   CComPtr<IIndexArray> oldFill;
   CComPtr<IIndexArray> fill;
   CComPtr<IIndexArray> array;

   if ( strandType == pgsTypes::Straight )
   {
      m_StrandFillers[segmentKey].ComputeStraightStrandFill(girder, nStrands, &fill);
      girder->get_StraightStrandFill(&oldFill);
      girder->put_StraightStrandFill(fill);
      girder->get_StraightStrandsInRow(rowIdx,&array);
      girder->put_StraightStrandFill(oldFill);
   }
   else
   {
      m_StrandFillers[segmentKey].ComputeHarpedStrandFill(girder, nStrands, &fill);
      girder->get_HarpedStrandFill(&oldFill);
      girder->put_HarpedStrandFill(fill);
      girder->get_HarpedStrandsInRow(rowIdx,&array);
      girder->put_HarpedStrandFill(oldFill);
   }

   CollectionIndexType nItems;

   array->get_Count(&nItems);
   for ( CollectionIndexType i = 0; i < nItems; i++ )
   {
      StrandIndexType strandIdx;
      array->get_Item(i,&strandIdx);
      strandIdxs.push_back(strandIdx);
   }

   return strandIdxs;
}

//-----------------------------------------------------------------------------
void CBridgeAgentImp::GetStrandPosition(const pgsPointOfInterest& poi, StrandIndexType strandIdx,pgsTypes::StrandType type, IPoint2d** ppPoint)
{
   CComPtr<IPoint2dCollection> points;
   GetStrandPositions(poi,type,&points);
   points->get_Item(strandIdx,ppPoint);
}

void CBridgeAgentImp::GetStrandPositions(const pgsPointOfInterest& poi, pgsTypes::StrandType type, IPoint2dCollection** ppPoints)
{
   VALIDATE( GIRDER );
   CComPtr<IPrecastGirder> girder;
   GetGirder(poi,&girder);

   HRESULT hr = S_OK;
   switch( type )
   {
   case pgsTypes::Straight:
      hr = girder->get_StraightStrandPositions(poi.GetDistFromStart(),ppPoints);
      break;

   case pgsTypes::Harped:
      hr = girder->get_HarpedStrandPositions(poi.GetDistFromStart(),ppPoints);
      break;

   case pgsTypes::Temporary:
      hr = girder->get_TemporaryStrandPositions(poi.GetDistFromStart(),ppPoints);
      break;

   default:
      ATLASSERT(false); // shouldn't get here
   }

   ATLASSERT(SUCCEEDED(hr));
}

void CBridgeAgentImp::GetStrandPositionsEx(const pgsPointOfInterest& poi,const PRESTRESSCONFIG& rconfig, pgsTypes::StrandType type, IPoint2dCollection** ppPoints)
{
   VALIDATE( GIRDER );
   CComPtr<IPrecastGirder> girder;
   GetGirder(poi,&girder);

   CIndexArrayWrapper fill(rconfig.GetStrandFill(type));

   HRESULT hr = S_OK;
   switch( type )
   {
   case pgsTypes::Straight:
      hr = girder->get_StraightStrandPositionsEx(poi.GetDistFromStart(),&fill,ppPoints);
      break;

   case pgsTypes::Harped:
      // save and restore precast harped girders adjustment shift values, before/after geting point locations
      Float64 t_end_shift, t_hp_shift;
      girder->get_HarpedStrandAdjustmentEnd(&t_end_shift);
      girder->get_HarpedStrandAdjustmentHP(&t_hp_shift);

      girder->put_HarpedStrandAdjustmentEnd(rconfig.EndOffset);
      girder->put_HarpedStrandAdjustmentHP(rconfig.HpOffset);

      hr = girder->get_HarpedStrandPositionsEx(poi.GetDistFromStart(),&fill,ppPoints);

      girder->put_HarpedStrandAdjustmentEnd(t_end_shift);
      girder->put_HarpedStrandAdjustmentHP(t_hp_shift);

      break;

   case pgsTypes::Temporary:
      hr = girder->get_TemporaryStrandPositionsEx(poi.GetDistFromStart(),&fill,ppPoints);
      break;

   default:
      ATLASSERT(false); // shouldn't get here
   }

#ifdef _DEBUG
   CollectionIndexType np;
   (*ppPoints)->get_Count(&np);
   ATLASSERT(np==rconfig.GetNStrands(type));
#endif

   ATLASSERT(SUCCEEDED(hr));
}

void CBridgeAgentImp::GetStrandPositionsEx(LPCTSTR strGirderName,const PRESTRESSCONFIG& rconfig,
                                           pgsTypes::StrandType strandType,pgsTypes::MemberEndType endType,
                                           IPoint2dCollection** ppPoints)
{
   GET_IFACE(ILibrary,pLib);
   const GirderLibraryEntry* pGdrEntry = pLib->GetGirderEntry(strGirderName);

   CComPtr<IStrandGrid> startGrid, endGrid;
   startGrid.CoCreateInstance(CLSID_StrandGrid);
   endGrid.CoCreateInstance(CLSID_StrandGrid);

   CComQIPtr<IStrandGridFiller> gridFiller(startGrid);

   CIndexArrayWrapper fill(rconfig.GetStrandFill(strandType));

   HRESULT hr = S_OK;
   switch(strandType)
   {
   case pgsTypes::Straight:
      pGdrEntry->ConfigureStraightStrandGrid(startGrid,endGrid);
      gridFiller->GetStrandPositionsEx(&fill,ppPoints);
      break;

   case pgsTypes::Harped:
      {
      CComPtr<IStrandGrid> startHPGrid, endHPGrid;
      startHPGrid.CoCreateInstance(CLSID_StrandGrid);
      endHPGrid.CoCreateInstance(CLSID_StrandGrid);
      pGdrEntry->ConfigureHarpedStrandGrids(startGrid,startHPGrid,endHPGrid,endGrid);
      CComQIPtr<IStrandGridFiller> hpGridFiller(startHPGrid);
      gridFiller->GetStrandPositionsEx(&fill,ppPoints);
      }
      break;

   case pgsTypes::Temporary:
      pGdrEntry->ConfigureTemporaryStrandGrid(startGrid,endGrid);
      gridFiller->GetStrandPositionsEx(&fill,ppPoints);
      break;

   default:
      ATLASSERT(false); // is there a new strand type?
   }

   ATLASSERT(SUCCEEDED(hr));
}

Float64 CBridgeAgentImp::ComputeAbsoluteHarpedOffsetEnd(const CSegmentKey& segmentKey, const ConfigStrandFillVector& rHarpedFillArray, HarpedStrandOffsetType measurementType, Float64 offset)
{
   // returns the offset value measured from the original strand locations defined in the girder library
   // Up is positive
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CGirderGroupData* pGroup = pBridgeDesc->GetGirderGroup(segmentKey.groupIndex);
   const CSplicedGirderData* pGirder = pGroup->GetGirder(segmentKey.girderIndex);
   Float64 absOffset = ComputeAbsoluteHarpedOffsetEnd(pGirder->GetGirderName(),rHarpedFillArray,measurementType,offset);

#if defined _DEBUG

//   VALIDATE( GIRDER );
   CComPtr<IPrecastGirder> girder;
   GetGirder(segmentKey, &girder);

   Float64 increment; // if less than zero, strands cannot be adjusted
   girder->get_HarpedEndAdjustmentIncrement(&increment);

   Float64 result = 0;
   if (0.0 <= increment && AreStrandsInConfigFillVec(rHarpedFillArray))
   {
      if (measurementType==hsoLEGACY)
      {
         // legacy end offset moved top strand to highest location in strand grid and then measured down
         CComPtr<IStrandGrid> grid;
         girder->get_HarpedStrandGridEnd(etStart,&grid);

         CComPtr<IRect2d> grid_box;
         grid->GridBoundingBox(&grid_box);

         Float64 grid_bottom, grid_top;
         grid_box->get_Bottom(&grid_bottom);
         grid_box->get_Top(&grid_top);

         CIndexArrayWrapper fill(rHarpedFillArray);

         Float64 fill_top, fill_bottom;
         girder->GetHarpedEndFilledGridBoundsEx(&fill, &fill_bottom, &fill_top);

         Float64 vert_adjust;
         girder->get_HarpedStrandAdjustmentEnd(&vert_adjust);

         result = grid_top - (fill_top-vert_adjust) - offset;
      }
      else if (measurementType==hsoCGFROMTOP || measurementType==hsoCGFROMBOTTOM || measurementType==hsoECCENTRICITY)
      {
         // compute adjusted cg location
         CIndexArrayWrapper fill(rHarpedFillArray);

         CComPtr<IPoint2dCollection> points;
         girder->get_HarpedStrandPositionsEx(0.0, &fill, &points);

         Float64 cg=0.0;

         CollectionIndexType nStrands;
         points->get_Count(&nStrands);
         ATLASSERT(CountStrandsInConfigFillVec(rHarpedFillArray) == nStrands);
         for (CollectionIndexType strandIdx = 0; strandIdx < nStrands; strandIdx++)
         {
            CComPtr<IPoint2d> point;
            points->get_Item(strandIdx,&point);
            Float64 y;
            point->get_Y(&y);

            cg += y;
         }

         cg = cg / (Float64)nStrands;

         // compute unadjusted location of cg
         Float64 vert_adjust;
         girder->get_HarpedStrandAdjustmentEnd(&vert_adjust);

         cg -= vert_adjust;

         if (measurementType==hsoCGFROMTOP)
         {
            Float64 height;
            girder->get_TopElevation(&height);

            Float64 dist = height - cg;
            result  = dist - offset;

         }
         else if ( measurementType==hsoCGFROMBOTTOM)
         {
            // bottom is a Y=0.0
            result =  offset - cg;
         }
         else if (measurementType==hsoECCENTRICITY)
         {
            IntervalIndexType releaseIntervalIdx = GetPrestressReleaseInterval(segmentKey);
            Float64 Yb = GetYb(releaseIntervalIdx,pgsPointOfInterest(segmentKey,0.0));
            Float64 ecc = Yb-cg;

            result = ecc - offset;
         }
      }
      else if (measurementType==hsoTOP2TOP || measurementType==hsoTOP2BOTTOM)
      {
         // get strand grid positions that are filled by Nh strands
         CIndexArrayWrapper fill(rHarpedFillArray);

         // fill_top is the top of the strand positions that are actually filled
         // adjusted by the harped strand adjustment
         Float64 fill_top, fill_bottom;
         girder->GetHarpedEndFilledGridBoundsEx(&fill, &fill_bottom, &fill_top);

         // get the harped strand adjustment so its effect can be removed
         Float64 vert_adjust;
         girder->get_HarpedStrandAdjustmentEnd(&vert_adjust);

         // elevation of the top of the strand grid without an offset
         Float64 toploc = fill_top-vert_adjust;

         if (measurementType==hsoTOP2TOP)
         {
            Float64 height;
            girder->get_TopElevation(&height);

            // distance from the top of the girder to the top of the strands before offset
            Float64 dist = height - toploc;

            // distance from the top of the girder to the top of the strands after offset
            result  = dist - offset;
         }
         else  // measurementType==hsoTOP2BOTTOM
         {
            result =  offset - toploc;
         }
      }
      else if (measurementType==hsoBOTTOM2BOTTOM)
      {
         CIndexArrayWrapper fill(rHarpedFillArray);

         Float64 fill_top, fill_bottom;
         girder->GetHarpedEndFilledGridBoundsEx(&fill, &fill_bottom, &fill_top);

         Float64 vert_adjust;
         girder->get_HarpedStrandAdjustmentEnd(&vert_adjust);

         Float64 botloc = fill_bottom-vert_adjust;

         result =  offset - botloc;
      }
      else
      {
         ATLASSERT(0);
      }
    }

   ATLASSERT(IsEqual(result,absOffset));
#endif // _DEBUG
   return absOffset;
}

Float64 CBridgeAgentImp::ComputeAbsoluteHarpedOffsetEnd(LPCTSTR strGirderName,const ConfigStrandFillVector& rHarpedFillArray, HarpedStrandOffsetType measurementType, Float64 offset)
{
   // returns the offset value measured from the original strand locations defined in the girder library
   // Up is positive
   if ( !AreStrandsInConfigFillVec(rHarpedFillArray) )
      return 0.0;

   GET_IFACE(ILibrary,pLib);
   const GirderLibraryEntry* pGdrEntry = pLib->GetGirderEntry(strGirderName);

   if (!pGdrEntry->IsVerticalAdjustmentAllowedEnd())
   {
      return 0.0; // No offset allowed
   }

   CComPtr<IStrandMover> pStrandMover;
   CreateStrandMover(strGirderName,&pStrandMover);

   CComPtr<IStrandGrid> startGrid, startHPGrid, endHPGrid, endGrid;
   startGrid.CoCreateInstance(CLSID_StrandGrid);
   startHPGrid.CoCreateInstance(CLSID_StrandGrid);
   endHPGrid.CoCreateInstance(CLSID_StrandGrid);
   endGrid.CoCreateInstance(CLSID_StrandGrid);
   pGdrEntry->ConfigureHarpedStrandGrids(startGrid,startHPGrid,endHPGrid,endGrid);

   CIndexArrayWrapper fill(rHarpedFillArray);

   Float64 absOffset = 0;
   if (measurementType==hsoLEGACY)
   {
      // legacy end offset moved top strand to highest location in strand grid and then measured down
      CComPtr<IRect2d> grid_box;
      startGrid->GridBoundingBox(&grid_box);

      Float64 grid_bottom, grid_top;
      grid_box->get_Bottom(&grid_bottom);
      grid_box->get_Top(&grid_top);

      CComQIPtr<IStrandGridFiller> startGridFiller(startGrid);
      CComQIPtr<IStrandGridFiller> hpGridFiller(startHPGrid);

      Float64 fill_top, fill_bottom;
      startGridFiller->get_FilledGridBoundsEx(&fill, &fill_bottom, &fill_top);

      Float64 vert_adjust;
      startGridFiller->get_VerticalStrandAdjustment(&vert_adjust);

      absOffset = grid_top - (fill_top-vert_adjust) - offset;
   }
   else if (measurementType==hsoCGFROMTOP || measurementType==hsoCGFROMBOTTOM || measurementType==hsoECCENTRICITY)
   {
      // compute adjusted cg location
      CComQIPtr<IStrandGridFiller> startGridFiller(startGrid);
      CComQIPtr<IStrandGridFiller> hpGridFiller(startHPGrid);

      CComPtr<IPoint2dCollection> points;
      startGridFiller->GetStrandPositionsEx(&fill,&points);

      Float64 cg=0.0;

      CollectionIndexType nStrands;
      points->get_Count(&nStrands);
      ATLASSERT(CountStrandsInConfigFillVec(rHarpedFillArray) == nStrands);
      for (CollectionIndexType strandIdx = 0; strandIdx < nStrands; strandIdx++)
      {
         CComPtr<IPoint2d> point;
         points->get_Item(strandIdx,&point);
         Float64 y;
         point->get_Y(&y);

         cg += y;
      }

      cg = cg / (Float64)nStrands;

      // compute unadjusted location of cg
      Float64 vert_adjust;
      startGridFiller->get_VerticalStrandAdjustment(&vert_adjust);

      cg -= vert_adjust;

      if (measurementType==hsoCGFROMTOP)
      {
         Float64 height;
         pStrandMover->get_TopElevation(&height);

         Float64 dist = height - cg;
         absOffset  = dist - offset;

      }
      else if ( measurementType==hsoCGFROMBOTTOM)
      {
         // bottom is a Y=0.0
         absOffset =  offset - cg;
      }
      else if (measurementType==hsoECCENTRICITY)
      {
         CComPtr<IBeamFactory> factory;
         pGdrEntry->GetBeamFactory(&factory);

         CComPtr<IGirderSection> gdrSection;
         factory->CreateGirderSection(m_pBroker,INVALID_ID,pGdrEntry->GetDimensions(),-1,-1,&gdrSection);

         CComQIPtr<IShape> shape(gdrSection);
         CComPtr<IShapeProperties> props;
         shape->get_ShapeProperties(&props);
         Float64 Yb;
         props->get_Ybottom(&Yb);

         Float64 ecc = Yb-cg;

         absOffset = ecc - offset;
      }
   }
   else if (measurementType==hsoTOP2TOP || measurementType==hsoTOP2BOTTOM)
   {
      // get strand grid positions that are filled by Nh strands
      CComQIPtr<IStrandGridFiller> startGridFiller(startGrid);
      CComQIPtr<IStrandGridFiller> hpGridFiller(startHPGrid);

      // fill_top is the top of the strand positions that are actually filled
      // adjusted by the harped strand adjustment
      Float64 fill_top, fill_bottom;
      startGridFiller->get_FilledGridBoundsEx(&fill, &fill_bottom, &fill_top);

      // get the harped strand adjustment so its effect can be removed
      Float64 vert_adjust;
      startGridFiller->get_VerticalStrandAdjustment(&vert_adjust);

      // elevation of the top of the strand grid without an offset
      Float64 toploc = fill_top-vert_adjust;

      if (measurementType==hsoTOP2TOP)
      {
         Float64 height;
         pStrandMover->get_TopElevation(&height);

         // distance from the top of the girder to the top of the strands before offset
         Float64 dist = height - toploc;

         // distance from the top of the girder to the top of the strands after offset
         absOffset  = dist - offset;
      }
      else  // measurementType==hsoTOP2BOTTOM
      {
         absOffset =  offset - toploc;
      }
   }
   else if (measurementType==hsoBOTTOM2BOTTOM)
   {
      CComQIPtr<IStrandGridFiller> startGridFiller(startGrid);
      CComQIPtr<IStrandGridFiller> hpGridFiller(startHPGrid);

      Float64 fill_top, fill_bottom;
      startGridFiller->get_FilledGridBoundsEx(&fill,&fill_bottom,&fill_top);

      Float64 vert_adjust;
      startGridFiller->get_VerticalStrandAdjustment(&vert_adjust);

      Float64 botloc = fill_bottom-vert_adjust;

      absOffset =  offset - botloc;
   }
   else
   {
      ATLASSERT(0);
   }

   return absOffset;
}

Float64 CBridgeAgentImp::ComputeAbsoluteHarpedOffsetHp(const CSegmentKey& segmentKey, const ConfigStrandFillVector& rHarpedFillArray, HarpedStrandOffsetType measurementType, Float64 offset)
{
   // returns the offset value measured from the original strand locations defined in the girder library
   // Up is positive
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CGirderGroupData* pGroup = pBridgeDesc->GetGirderGroup(segmentKey.groupIndex);
   const CSplicedGirderData* pGirder = pGroup->GetGirder(segmentKey.girderIndex);
   Float64 absOffset = ComputeAbsoluteHarpedOffsetHp(pGirder->GetGirderName(),rHarpedFillArray,measurementType,offset);

#if defined _DEBUG

//   VALIDATE( GIRDER );
   CComPtr<IPrecastGirder> girder;
   GetGirder(segmentKey, &girder);

   Float64 increment; // if less than zero, strands cannot be adjusted
   girder->get_HarpedHpAdjustmentIncrement(&increment);

   Float64 result = 0;
   if ( increment>=0.0 && AreStrandsInConfigFillVec(rHarpedFillArray) )
   {
      if (measurementType==hsoLEGACY)
      {
         result = offset;
      }
      else if (measurementType==hsoCGFROMTOP || measurementType==hsoCGFROMBOTTOM || measurementType==hsoECCENTRICITY)
      {
         // compute adjusted cg location
         CIndexArrayWrapper fill(rHarpedFillArray);

         Float64 L = GetSegmentLength(segmentKey); 

         CComPtr<IPoint2dCollection> points;
         girder->get_HarpedStrandPositionsEx(L/2.0, &fill, &points);

         Float64 cg=0.0;

         CollectionIndexType nStrands;
         points->get_Count(&nStrands);
         ATLASSERT(CountStrandsInConfigFillVec(rHarpedFillArray) == nStrands);
         for (CollectionIndexType strandIdx = 0; strandIdx < nStrands; strandIdx++)
         {
            CComPtr<IPoint2d> point;
            points->get_Item(strandIdx,&point);
            Float64 y;
            point->get_Y(&y);

            cg += y;
         }

         cg = cg / (Float64)nStrands;

         // compute unadjusted location of cg
         Float64 vert_adjust;
         girder->get_HarpedStrandAdjustmentHP(&vert_adjust);

         cg -= vert_adjust;

         if (measurementType==hsoCGFROMTOP)
         {
            Float64 height;
            girder->get_TopElevation(&height);

            Float64 dist = height - cg;
            result  = dist - offset;

         }
         else if ( measurementType==hsoCGFROMBOTTOM)
         {
            // bottom is a Y=0.0
            result =  offset - cg;
         }
         else if (measurementType==hsoECCENTRICITY)
         {
            IntervalIndexType releaseIntervalIdx = GetPrestressReleaseInterval(segmentKey);
            Float64 Yb = GetYb(releaseIntervalIdx,pgsPointOfInterest(segmentKey,0.0));
            Float64 ecc = Yb-cg;

            result = ecc - offset;
         }
      }
      else if (measurementType==hsoTOP2TOP || measurementType==hsoTOP2BOTTOM)
      {
         // get location of highest strand at zero offset
         CIndexArrayWrapper fill(rHarpedFillArray);

         Float64 fill_top, fill_bottom;
         girder->GetHarpedHpFilledGridBoundsEx(&fill, &fill_bottom, &fill_top);

         Float64 vert_adjust;
         girder->get_HarpedStrandAdjustmentHP(&vert_adjust);

         Float64 toploc = fill_top-vert_adjust;

         if (measurementType==hsoTOP2TOP)
         {
            Float64 height;
            girder->get_TopElevation(&height);

            Float64 dist = height - toploc;
            result  = dist - offset;
         }
         else  // measurementType==hsoTOP2BOTTOM
         {
            result =  offset - toploc;
         }
      }
      else if (measurementType==hsoBOTTOM2BOTTOM)
      {
         CIndexArrayWrapper fill(rHarpedFillArray);

         Float64 fill_top, fill_bottom;
         girder->GetHarpedHpFilledGridBoundsEx(&fill, &fill_bottom, &fill_top);

         Float64 vert_adjust;
         girder->get_HarpedStrandAdjustmentHP(&vert_adjust);

         Float64 botloc = fill_bottom-vert_adjust;

         result =  offset - botloc;
      }
      else
      {
         ATLASSERT(0);
      }
   }

   result = IsZero(result) ? 0.0 : result;
   ATLASSERT(IsEqual(result,absOffset));
#endif // __DEBUG

   return absOffset;
}

Float64 CBridgeAgentImp::ComputeAbsoluteHarpedOffsetHp(LPCTSTR strGirderName,const ConfigStrandFillVector& rHarpedFillArray, HarpedStrandOffsetType measurementType, Float64 offset)
{
   // returns the offset value measured from the original strand locations defined in the girder library
   // Up is positive
   if ( !AreStrandsInConfigFillVec(rHarpedFillArray) )
      return 0;

   GET_IFACE(ILibrary,pLib);
   const GirderLibraryEntry* pGdrEntry = pLib->GetGirderEntry(strGirderName);

   if (!pGdrEntry->IsVerticalAdjustmentAllowedHP())
   {
      return 0.0; // No offset allowed
   }

   CComPtr<IStrandMover> pStrandMover;
   CreateStrandMover(strGirderName,&pStrandMover);

   CComPtr<IStrandGrid> startGrid, startHPGrid, endHPGrid, endGrid;
   startGrid.CoCreateInstance(CLSID_StrandGrid);
   startHPGrid.CoCreateInstance(CLSID_StrandGrid);
   endHPGrid.CoCreateInstance(CLSID_StrandGrid);
   endGrid.CoCreateInstance(CLSID_StrandGrid);
   pGdrEntry->ConfigureHarpedStrandGrids(startGrid,startHPGrid,endHPGrid,endGrid);

   Float64 absOffset = 0;
   if (measurementType==hsoLEGACY)
   {
      absOffset = offset;
   }
   else if (measurementType==hsoCGFROMTOP || measurementType==hsoCGFROMBOTTOM || measurementType==hsoECCENTRICITY)
   {
      // compute adjusted cg location
      CComQIPtr<IStrandGridFiller> hpGridFiller(startHPGrid);

      CIndexArrayWrapper fill(rHarpedFillArray);

      CComPtr<IPoint2dCollection> points;
      hpGridFiller->GetStrandPositionsEx(&fill,&points);

      Float64 cg=0.0;

      CollectionIndexType nStrands;
      points->get_Count(&nStrands);
      ATLASSERT(CountStrandsInConfigFillVec(rHarpedFillArray) == nStrands);
      for (CollectionIndexType strandIdx = 0; strandIdx < nStrands; strandIdx++)
      {
         CComPtr<IPoint2d> point;
         points->get_Item(strandIdx,&point);
         Float64 y;
         point->get_Y(&y);

         cg += y;
      }

      cg = cg / (Float64)nStrands;

      // compute unadjusted location of cg
      Float64 vert_adjust;
      hpGridFiller->get_VerticalStrandAdjustment(&vert_adjust);

      cg -= vert_adjust;

      if (measurementType==hsoCGFROMTOP)
      {
         Float64 height;
         pStrandMover->get_TopElevation(&height);

         Float64 dist = height - cg;
         absOffset  = dist - offset;

      }
      else if ( measurementType==hsoCGFROMBOTTOM)
      {
         // bottom is a Y=0.0
         absOffset =  offset - cg;
      }
      else if (measurementType==hsoECCENTRICITY)
      {
         CComPtr<IBeamFactory> factory;
         pGdrEntry->GetBeamFactory(&factory);

         CComPtr<IGirderSection> gdrSection;
         factory->CreateGirderSection(m_pBroker,INVALID_ID,pGdrEntry->GetDimensions(),-1,-1,&gdrSection);

         CComQIPtr<IShape> shape(gdrSection);
         CComPtr<IShapeProperties> props;
         shape->get_ShapeProperties(&props);
         Float64 Yb;
         props->get_Ybottom(&Yb);

         Float64 ecc = Yb-cg;

         absOffset = ecc - offset;
      }
   }
   else if (measurementType==hsoTOP2TOP || measurementType==hsoTOP2BOTTOM)
   {
      // get location of highest strand at zero offset
      CComQIPtr<IStrandGridFiller> hpGridFiller(startHPGrid);

      CIndexArrayWrapper fill(rHarpedFillArray);

      Float64 fill_top, fill_bottom;
      hpGridFiller->get_FilledGridBoundsEx(&fill,&fill_bottom,&fill_top);

      Float64 vert_adjust;
      hpGridFiller->get_VerticalStrandAdjustment(&vert_adjust);

      Float64 toploc = fill_top-vert_adjust;

      if (measurementType==hsoTOP2TOP)
      {
         Float64 height;
         pStrandMover->get_TopElevation(&height);

         Float64 dist = height - toploc;
         absOffset  = dist - offset;
      }
      else  // measurementType==hsoTOP2BOTTOM
      {
         absOffset =  offset - toploc;
      }
   }
   else if (measurementType==hsoBOTTOM2BOTTOM)
   {
      CComQIPtr<IStrandGridFiller> hpGridFiller(startHPGrid);

      CIndexArrayWrapper fill(rHarpedFillArray);

      Float64 fill_top, fill_bottom;
      hpGridFiller->get_FilledGridBoundsEx(&fill,&fill_bottom,&fill_top);

      Float64 vert_adjust;
      hpGridFiller->get_VerticalStrandAdjustment(&vert_adjust);

      Float64 botloc = fill_bottom-vert_adjust;

      absOffset =  offset - botloc;
   }
   else
   {
      ATLASSERT(0);
   }

   absOffset = IsZero(absOffset) ? 0.0 : absOffset;
   return absOffset;
}

Float64 CBridgeAgentImp::ComputeHarpedOffsetFromAbsoluteEnd(const CSegmentKey& segmentKey, const ConfigStrandFillVector& rHarpedFillArray, HarpedStrandOffsetType measurementType, Float64 absoluteOffset)
{
   // all we really need to know is the distance and direction between the coords, compute absolute
   // from zero
   Float64 absol = ComputeAbsoluteHarpedOffsetEnd(segmentKey, rHarpedFillArray, measurementType, 0.0);

   Float64 off = 0.0;
   // direction depends if meassured from bottom up or top down
   if (measurementType==hsoLEGACY || measurementType==hsoCGFROMTOP || measurementType==hsoTOP2TOP
      || measurementType==hsoECCENTRICITY)
   {
      off = absol - absoluteOffset ;
   }
   else if (measurementType==hsoCGFROMBOTTOM || measurementType==hsoTOP2BOTTOM || 
            measurementType==hsoBOTTOM2BOTTOM)
   {
      off = absoluteOffset - absol;
   }
   else
      ATLASSERT(0); 

   return off;
}

Float64 CBridgeAgentImp::ComputeHarpedOffsetFromAbsoluteEnd(LPCTSTR strGirderName,const ConfigStrandFillVector& rHarpedFillArray, HarpedStrandOffsetType measurementType, Float64 absoluteOffset)
{
   // all we really need to know is the distance and direction between the coords, compute absolute
   // from zero
   Float64 absol = ComputeAbsoluteHarpedOffsetEnd(strGirderName, rHarpedFillArray, measurementType, 0.0);

   Float64 off = 0.0;
   // direction depends if meassured from bottom up or top down
   if (measurementType==hsoLEGACY || measurementType==hsoCGFROMTOP || measurementType==hsoTOP2TOP
      || measurementType==hsoECCENTRICITY)
   {
      off = absol - absoluteOffset ;
   }
   else if (measurementType==hsoCGFROMBOTTOM || measurementType==hsoTOP2BOTTOM || 
            measurementType==hsoBOTTOM2BOTTOM)
   {
      off = absoluteOffset - absol;
   }
   else
      ATLASSERT(0); 

   return off;
}

Float64 CBridgeAgentImp::ComputeHarpedOffsetFromAbsoluteHp(const CSegmentKey& segmentKey, const ConfigStrandFillVector& rHarpedFillArray, HarpedStrandOffsetType measurementType, Float64 absoluteOffset)
{
   Float64 absol = ComputeAbsoluteHarpedOffsetHp(segmentKey, rHarpedFillArray, measurementType, 0.0);

   Float64 off = 0.0;
   // direction depends if meassured from bottom up or top down
   if (measurementType==hsoCGFROMTOP || measurementType==hsoTOP2TOP || measurementType==hsoECCENTRICITY)
   {
      off = absol - absoluteOffset ;
   }
   else if (measurementType==hsoLEGACY || measurementType==hsoCGFROMBOTTOM || measurementType==hsoTOP2BOTTOM || 
            measurementType==hsoBOTTOM2BOTTOM )
   {
      off = absoluteOffset - absol;
   }
   else
      ATLASSERT(0);

   return off;
}

Float64 CBridgeAgentImp::ComputeHarpedOffsetFromAbsoluteHp(LPCTSTR strGirderName,const ConfigStrandFillVector& rHarpedFillArray, HarpedStrandOffsetType measurementType, Float64 absoluteOffset)
{
   Float64 absol = ComputeAbsoluteHarpedOffsetHp(strGirderName, rHarpedFillArray, measurementType, 0.0);

   Float64 off = 0.0;
   // direction depends if meassured from bottom up or top down
   if (measurementType==hsoCGFROMTOP || measurementType==hsoTOP2TOP || measurementType==hsoECCENTRICITY)
   {
      off = absol - absoluteOffset ;
   }
   else if (measurementType==hsoLEGACY || measurementType==hsoCGFROMBOTTOM || measurementType==hsoTOP2BOTTOM || 
            measurementType==hsoBOTTOM2BOTTOM )
   {
      off = absoluteOffset - absol;
   }
   else
      ATLASSERT(0);

   return off;
}

void CBridgeAgentImp::ComputeValidHarpedOffsetForMeasurementTypeEnd(const CSegmentKey& segmentKey, const ConfigStrandFillVector& rHarpedFillArray, HarpedStrandOffsetType measurementType, Float64* lowRange, Float64* highRange)
{
   CComPtr<IPrecastGirder> girder;
   GetGirder(segmentKey, &girder);

   CIndexArrayWrapper fill(rHarpedFillArray);

   Float64 absDown, absUp;
   HRESULT hr = girder->GetHarpedEndAdjustmentBoundsEx(&fill, &absDown, &absUp);
   ATLASSERT(SUCCEEDED(hr));

   Float64 offDown = ComputeHarpedOffsetFromAbsoluteEnd(segmentKey,  rHarpedFillArray, measurementType, absDown);
   Float64 offUp =   ComputeHarpedOffsetFromAbsoluteEnd(segmentKey,  rHarpedFillArray, measurementType, absUp);

   *lowRange = offDown;
   *highRange = offUp;
}

void CBridgeAgentImp::ComputeValidHarpedOffsetForMeasurementTypeEnd(LPCTSTR strGirderName,const ConfigStrandFillVector& rHarpedFillArray,HarpedStrandOffsetType measurementType, Float64* lowRange, Float64* highRange)
{
   Float64 absDown, absUp;
   GetHarpedEndOffsetBoundsEx(strGirderName,rHarpedFillArray, &absDown,&absUp);

   Float64 offDown = ComputeHarpedOffsetFromAbsoluteEnd(strGirderName,  rHarpedFillArray, measurementType, absDown);
   Float64 offUp =   ComputeHarpedOffsetFromAbsoluteEnd(strGirderName,  rHarpedFillArray, measurementType, absUp);

   *lowRange = offDown;
   *highRange = offUp;
}

void CBridgeAgentImp::ComputeValidHarpedOffsetForMeasurementTypeHp(const CSegmentKey& segmentKey,const ConfigStrandFillVector& rHarpedFillArray, HarpedStrandOffsetType measurementType, Float64* lowRange, Float64* highRange)
{
   CComPtr<IPrecastGirder> girder;
   GetGirder(segmentKey, &girder);

   CIndexArrayWrapper fill(rHarpedFillArray);

   Float64 absDown, absUp;
   HRESULT hr = girder->GetHarpedHpAdjustmentBoundsEx(&fill, &absDown, &absUp);
   ATLASSERT(SUCCEEDED(hr));

   Float64 offDown = ComputeHarpedOffsetFromAbsoluteHp(segmentKey,  rHarpedFillArray, measurementType, absDown);
   Float64 offUp   = ComputeHarpedOffsetFromAbsoluteHp(segmentKey,  rHarpedFillArray, measurementType, absUp);

   *lowRange = offDown;
   *highRange = offUp;
}

void CBridgeAgentImp::ComputeValidHarpedOffsetForMeasurementTypeHp(LPCTSTR strGirderName,const ConfigStrandFillVector& rHarpedFillArray, HarpedStrandOffsetType measurementType, Float64* lowRange, Float64* highRange)
{
   Float64 absDown, absUp;
   GetHarpedHpOffsetBoundsEx(strGirderName,rHarpedFillArray, &absDown,&absUp);

   Float64 offDown = ComputeHarpedOffsetFromAbsoluteHp(strGirderName,  rHarpedFillArray, measurementType, absDown);
   Float64 offUp =   ComputeHarpedOffsetFromAbsoluteHp(strGirderName,  rHarpedFillArray, measurementType, absUp);

   *lowRange = offDown;
   *highRange = offUp;
}

Float64 CBridgeAgentImp::ConvertHarpedOffsetEnd(const CSegmentKey& segmentKey,const ConfigStrandFillVector& rHarpedFillArray, HarpedStrandOffsetType fromMeasurementType, Float64 offset, HarpedStrandOffsetType toMeasurementType)
{
   Float64 abs_offset = ComputeAbsoluteHarpedOffsetEnd(segmentKey,rHarpedFillArray,fromMeasurementType,offset);
   Float64 result = ComputeHarpedOffsetFromAbsoluteEnd(segmentKey,rHarpedFillArray,toMeasurementType,abs_offset);
   return result;
}

Float64 CBridgeAgentImp::ConvertHarpedOffsetEnd(LPCTSTR strGirderName,const ConfigStrandFillVector& rHarpedFillArray, HarpedStrandOffsetType fromMeasurementType, Float64 offset, HarpedStrandOffsetType toMeasurementType)
{
   Float64 abs_offset = ComputeAbsoluteHarpedOffsetEnd(strGirderName,rHarpedFillArray,fromMeasurementType,offset);
   Float64 result = ComputeHarpedOffsetFromAbsoluteEnd(strGirderName,rHarpedFillArray,toMeasurementType,abs_offset);
   return result;
}

Float64 CBridgeAgentImp::ConvertHarpedOffsetHp(const CSegmentKey& segmentKey,const ConfigStrandFillVector& rHarpedFillArray, HarpedStrandOffsetType fromMeasurementType, Float64 offset, HarpedStrandOffsetType toMeasurementType)
{
   Float64 abs_offset = ComputeAbsoluteHarpedOffsetHp(segmentKey,rHarpedFillArray,fromMeasurementType,offset);
   Float64 result = ComputeHarpedOffsetFromAbsoluteHp(segmentKey,rHarpedFillArray,toMeasurementType,abs_offset);
   return result;
}

Float64 CBridgeAgentImp::ConvertHarpedOffsetHp(LPCTSTR strGirderName,const ConfigStrandFillVector& rHarpedFillArray, HarpedStrandOffsetType fromMeasurementType, Float64 offset, HarpedStrandOffsetType toMeasurementType)
{
   Float64 abs_offset = ComputeAbsoluteHarpedOffsetHp(strGirderName,rHarpedFillArray,fromMeasurementType,offset);
   Float64 result = ComputeHarpedOffsetFromAbsoluteHp(strGirderName,rHarpedFillArray,toMeasurementType,abs_offset);
   return result;
}

//std::vector<pgsPointOfInterest> CBridgeAgentImp::GetGirderPointsOfInterest(const CSegmentKey& segmentKey,EventIndexType event,PoiAttributeType attrib,Uint32 mode)
//{
//   // make sure we have POI before doing anything else
//   ValidatePointsOfInterest(span,gdr);
//
//   GirderIDType gdrID = ::HashSpanGirder(span,gdr);
//   SegmentIndexType segIdx = 0;
//
//   // set up the span indices (ALL_SPANS doesn't work in loops)
//   SpanIndexType startSpan = span;
//   SpanIndexType nSpans = startSpan + 1;
//   if ( span == ALL_SPANS )
//   {
//      startSpan = 0;
//      nSpans = GetSpanCount_Private();
//   }
//
//   Uint32 mgrMode = (mode == POIFIND_AND ? POIMGR_AND : POIMGR_OR);
//
//   GET_IFACE(ILiveLoads,pLiveLoads);
//   GET_IFACE(ILimitStateForces,pLimitStateForces);
//   bool bPermit = pLimitStateForces->IsStrengthIIApplicable(span, gdr);
//
//   // collect the desired POI into a vector
//   std::vector<pgsPointOfInterest> poi;
//   for ( SpanIndexType spanIdx = startSpan; spanIdx < nSpans; spanIdx++ )
//   {
//      // deal with girder index when there are different number of girders in each span
//      GirderIndexType gdrIdx = gdr;
//      GirderIndexType nGirders = GetGirderCount(spanIdx);
//      if ( nGirders <= gdr )
//         gdrIdx = nGirders-1;
//
//      // if the request includes Critical Section, make sure the critical section POI's have been located
//      if ( 0 <= EventCompare(event,pgsTypes::BridgeSite1) && (attrib & POI_CRITSECTSHEAR1 || attrib & POI_CRITSECTSHEAR2) )
//      {
//         pgsPointOfInterest poi_left,poi_right;
//         GetCriticalSection(pgsTypes::StrengthI,spanIdx,gdrIdx,&poi_left,&poi_right);
//
//         if ( bPermit )
//            GetCriticalSection(pgsTypes::StrengthII,spanIdx,gdrIdx,&poi_left,&poi_right);
//      }
//
//      std::vector<pgsPointOfInterest> vPoi;
//      m_GirderPoiMgr.GetPointsOfInterest( ::HashSpanGirder(spanIdx,gdrIdx), 0, event, attrib, mgrMode, &vPoi );
//      poi.insert(poi.end(),vPoi.begin(),vPoi.end());
//
//      if ( EventCompare(pgsTypes::GirderPlacement,event) <= 0 )
//      {
//         // remove all poi that are before the first bearing or after the last bearing
//         Float64 start_dist = GetSegmentStartEndDistance(segmentKey);
//         Float64 end_dist   = GetSegmentLength(segmentKey) - GetSegmentEndEndDistance(segmentKey);
//
//         PoiBefore::m_SSMbrID = gdrID;
//         PoiBefore::m_SegmentIdx = segIdx;
//         PoiBefore::m_Before = start_dist;
//
//         PoiAfter::m_SSMbrID = gdrID;
//         PoiAfter::m_SegmentIdx = segIdx;
//         PoiAfter::m_After   = end_dist;
//         std::vector<pgsPointOfInterest>::iterator found( std::find_if(poi.begin(),poi.end(),&PoiBefore::Compare) );
//         while ( found != poi.end() )
//         {
//            poi.erase(found);
//            found = std::find_if(poi.begin(),poi.end(),&PoiBefore::Compare);
//         }
//
//         found = std::find_if(poi.begin(),poi.end(),&PoiAfter::Compare);
//         while ( found != poi.end() )
//         {
//            poi.erase(found);
//            found = std::find_if(poi.begin(),poi.end(),&PoiAfter::Compare);
//         }
//      }
//   }
//
//   return poi;
//}
//
//std::vector<pgsPointOfInterest> CBridgeAgentImp::GetGirderPointsOfInterest(const CSegmentKey& segmentKey,const std::vector<EventIndexType>& events,PoiAttributeType attrib,Uint32 mode)
//{
//   // make sure we have POI before doing anything else
//   ValidatePointsOfInterest(span,gdr);
//
//   // set up the span indices (ALL_SPANS doesn't work in loops)
//   SpanIndexType startSpan = span;
//   SpanIndexType nSpans = startSpan + 1;
//   if ( span == ALL_SPANS )
//   {
//      startSpan = 0;
//      nSpans = GetSpanCount_Private();
//   }
//
//   Uint32 mgrMode = (mode == POIFIND_AND ? POIMGR_AND : POIMGR_OR);
//
//   GET_IFACE(ILimitStateForces,pLimitStateForces);
//   bool bPermit = pLimitStateForces->IsStrengthIIApplicable(span, gdr);
//
//   // collect the desired POI into a vector
//   std::vector<pgsPointOfInterest> poi;
//   for ( SpanIndexType spanIdx = startSpan; spanIdx < nSpans; spanIdx++ )
//   {
//      // deal with girder index when there are different number of girders in each span
//      GirderIndexType gdrIdx = gdr;
//      GirderIndexType nGirders = GetGirderCount(spanIdx);
//      if ( nGirders <= gdr )
//         gdrIdx = nGirders-1;
//
//      // if the request includes Critical Section, make sure the critical section POI's have been located
//      std::vector<EventIndexType> sortedEvents(events);
//      std::sort(sortedEvents.begin(),sortedEvents.end());
//      std::vector<EventIndexType>::iterator eventBegin(sortedEvents.begin());
//      std::vector<EventIndexType>::iterator eventEnd(sortedEvents.end());
//      
//      bool bCriticalSection = false;
//      if ( std::find(eventBegin,eventEnd,pgsTypes::BridgeSite1) != eventEnd ||
//           std::find(eventBegin,eventEnd,pgsTypes::BridgeSite2) != eventEnd ||
//           std::find(eventBegin,eventEnd,pgsTypes::BridgeSite3) != eventEnd )
//      {
//         bCriticalSection = true;
//      }
//
//      if ( bCriticalSection && (attrib & POI_CRITSECTSHEAR1 || attrib & POI_CRITSECTSHEAR2) )
//      {
//         pgsPointOfInterest poi_left,poi_right;
//         GetCriticalSection(pgsTypes::StrengthI,spanIdx,gdrIdx,&poi_left,&poi_right);
//
//         if ( bPermit )
//            GetCriticalSection(pgsTypes::StrengthII,spanIdx,gdrIdx,&poi_left,&poi_right);
//      }
//
//      std::vector<pgsPointOfInterest> vPoi;
//      m_GirderPoiMgr.GetPointsOfInterest( ::HashSpanGirder(spanIdx,gdrIdx), 0, events, attrib, mgrMode, &vPoi );
//      poi.insert(poi.end(),vPoi.begin(),vPoi.end());
//   }
//
//   return poi;
//}
//
//std::vector<pgsPointOfInterest> CBridgeAgentImp::GetGirderTenthPointPOIs(EventIndexType event,const CSegmentKey& segmentKey)
//{
//   ValidatePointsOfInterest(span,gdr);
//   std::vector<pgsPointOfInterest> poi;
//   m_GirderPoiMgr.GetTenthPointPOIs( event, ::HashSpanGirder(span,gdr), 0, &poi );
//   return poi;
//}
//
//std::vector<pgsPointOfInterest> CBridgeAgentImp::GetCriticalSections(pgsTypes::LimitState limitState,GirderIDType gdrID)
//{
//   std::vector<EventIndexType> events;
//   events.push_back(pgsTypes::CastingYard); // for neg moment capacity at CS (need camber to build capacity model... camber starts in casting yard)
//   events.push_back(pgsTypes::BridgeSite3); // for shear
//
//   int idx = limitState == pgsTypes::StrengthI ? 0 : 1;
//   std::set<SpanGirderHashType>::iterator found;
//   found = m_CriticalSectionState[idx].find( gdrID );
//   if ( found == m_CriticalSectionState[idx].end() )
//   {
//#pragma Reminder("UPDATE: Need to compute critical section for shear along the length of a superstructure/girder")
//      SpanIndexType spanIdx;
//      GirderIndexType gdrIdx;
//      ::UnhashSpanGirder(gdrID,&spanIdx,&gdrIdx);
//      ATLASSERT(segIdx == 0);
//
//      // Critical section not previously computed.
//      // Do it now.
//      GET_IFACE(IShearCapacity,pShearCap);
//      Float64 xl, xr; // distance from end of girder for left and right critical section
//      pShearCap->GetCriticalSection(limitState,spanIdx,gdrIdx,&xl,&xr);
//
//      PoiAttributeType attrib = (limitState == pgsTypes::StrengthI ? POI_CRITSECTSHEAR1 : POI_CRITSECTSHEAR2);
//      m_PoiMgr.AddPointOfInterest( pgsPointOfInterest(events,segmentKey,xl, attrib) );
//      m_PoiMgr.AddPointOfInterest( pgsPointOfInterest(events,segmentKey,xr, attrib) );
//
//      m_CriticalSectionState[idx].insert( gdrID );
//   }
//
//   std::vector<pgsPointOfInterest> vPoi;
//   m_PoiMgr.GetPointsOfInterest(segmentKey,pgsTypes::BridgeSite3,(limitState == pgsTypes::StrengthI ? POI_CRITSECTSHEAR1 : POI_CRITSECTSHEAR2),POIMGR_AND,&vPoi);
//   ATLASSERT( vPoi.size() == 2 ); // this won't be true for spliced girders
//   return vPoi;
//}
//
//std::vector<pgsPointOfInterest> CBridgeAgentImp::GetCriticalSections(pgsTypes::LimitState limitState,GirderIDType gdrID,const GDRCONFIG& config)
//{
//#pragma Reminder("UPDATE: Need to compute critical section for shear along the length of a superstructure/girder")
//   SpanIndexType spanIdx;
//   GirderIndexType gdrIdx;
//   ::UnhashSpanGirder(gdrID,&spanIdx,&gdrIdx);
//   ATLASSERT(segIdx == 0);
//
//   std::vector<EventIndexType> events;
//   events.push_back(pgsTypes::CastingYard); // for neg moment capacity at CS (need camber to build capacity model... camber starts in casting yard)
//   events.push_back(pgsTypes::BridgeSite3); // for shear
//
//   GET_IFACE(IShearCapacity,pShearCap);
//   Float64 xl, xr; // distance from end of girder for left and right critical section
//   pShearCap->GetCriticalSection(limitState,spanIdx,gdrIdx,config,&xl,&xr);
//
//   PoiAttributeType attrib = (limitState == pgsTypes::StrengthI ? POI_CRITSECTSHEAR1 : POI_CRITSECTSHEAR2);
//   pgsPointOfInterest csLeft( events,segmentKey,xl,attrib);
//   pgsPointOfInterest csRight(events,segmentKey,xr,attrib);
//
//   std::vector<pgsPointOfInterest> vPoi;
//   vPoi.push_back(csLeft);
//   vPoi.push_back(csRight);
//
//   return vPoi;
//}
//
//pgsPointOfInterest CBridgeAgentImp::GetGirderPointOfInterest(EventIndexType event,GirderIDType gdrID,Float64 distFromStart)
//{
//   ValidateGirderPointsOfInterest(gdrID);
//   return m_GirderPoiMgr.GetPointOfInterest(event,gdrID,INVALID_INDEX,distFromStart);
//}
//
//pgsPointOfInterest CBridgeAgentImp::GetGirderPointOfInterest(GirderIDType gdrID,Float64 distFromStart)
//{
//   ValidateGirderPointsOfInterest(gdrID);
//   return m_GirderPoiMgr.GetPointOfInterest(gdrID,INVALID_INDEX,distFromStart);
//}

/////////////////////////////////////////////////////////////////////////
// IPointOfInterest
std::vector<pgsPointOfInterest> CBridgeAgentImp::GetPointsOfInterest(const CSegmentKey& segmentKey)
{
   ValidatePointsOfInterest(segmentKey);
   return m_PoiMgr.GetPointsOfInterest(segmentKey);
}

pgsPointOfInterest CBridgeAgentImp::GetPointOfInterest(PoiIDType poiID)
{
   ValidatePointsOfInterest(CGirderKey(ALL_GROUPS,ALL_GIRDERS));
   return m_PoiMgr.GetPointOfInterest(poiID);
}

pgsPointOfInterest CBridgeAgentImp::GetPointOfInterest(const CSegmentKey& segmentKey,Float64 distFromStart)
{
   //ValidatePointsOfInterest(segmentKey);
   return m_PoiMgr.GetPointOfInterest(segmentKey,distFromStart);
}

pgsPointOfInterest CBridgeAgentImp::GetNearestPointOfInterest(const CSegmentKey& segmentKey,Float64 distFromStart)
{
   ValidatePointsOfInterest(segmentKey);
   return m_PoiMgr.GetNearestPointOfInterest(segmentKey,distFromStart);
}

std::vector<pgsPointOfInterest> CBridgeAgentImp::GetPointsOfInterest(const CSegmentKey& segmentKey,PoiAttributeType attrib,Uint32 mode)
{
   ValidatePointsOfInterest(segmentKey);
   std::vector<pgsPointOfInterest> vPoi;
   m_PoiMgr.GetPointsOfInterest(segmentKey,attrib,mode,&vPoi);

#pragma Reminder("UPDATE: remove obsolete code") // is this code obsolete? remove it!
   //// remove all poi that are before the first bearing or after the last bearing
   //GET_IFACE(IBridgeDescription,pIBridgeDesc);
   //const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   //const CTimelineManager* pTimelineMgr = pIBridgeDesc->GetTimelineManager();
   //EventIndexType erectionEventIdx = pTimelineMgr->GetSegmentErectionEventIndex(segmentKey);
   //GroupIndexType nGroups = pBridgeDesc->GetGirderGroupCount();
   //if ( erectionEventIdx <= eventIdx )
   //{
   //   if ( segmentKey.groupIndex == 0 || segmentKey.groupIndex == ALL_GROUPS )
   //   {
   //      CSegmentKey searchKey(segmentKey);
   //      searchKey.groupIndex = 0;
   //      searchKey.segmentIndex = 0;
   //      Float64 start_dist = GetSegmentStartEndDistance(searchKey);

   //      PoiBefore::m_SegmentKey = searchKey;
   //      PoiBefore::m_Before     = start_dist;

   //      std::vector<pgsPointOfInterest>::iterator found( std::find_if(vPoi.begin(),vPoi.end(),&PoiBefore::Compare) );
   //      while ( found != vPoi.end() )
   //      {
   //         vPoi.erase(found);
   //         found = std::find_if(vPoi.begin(),vPoi.end(),&PoiBefore::Compare);
   //      }
   //   }

   //   if ( segmentKey.groupIndex == nGroups-1 || segmentKey.groupIndex == ALL_GROUPS )
   //   {
   //      CSegmentKey searchKey(segmentKey);
   //      searchKey.groupIndex = nGroups-1;
   //      searchKey.segmentIndex = pBridgeDesc->GetGirderGroup(searchKey.groupIndex)->GetGirder(searchKey.girderIndex)->GetSegmentCount()-1;

   //      Float64 end_dist   = GetSegmentLength(searchKey) - GetSegmentEndEndDistance(searchKey);

   //      PoiAfter::m_SegmentKey = searchKey;
   //      PoiAfter::m_After      = end_dist;

   //      std::vector<pgsPointOfInterest>::iterator found( std::find_if(vPoi.begin(),vPoi.end(),&PoiAfter::Compare) );
   //      while ( found != vPoi.end() )
   //      {
   //         vPoi.erase(found);
   //         found = std::find_if(vPoi.begin(),vPoi.end(),&PoiAfter::Compare);
   //      }
   //   }
   //}

   return vPoi;
}

std::vector<pgsPointOfInterest> CBridgeAgentImp::GetSegmentTenthPointPOIs(PoiAttributeType reference,const CSegmentKey& segmentKey)
{
   ValidatePointsOfInterest(segmentKey);

   std::vector<pgsPointOfInterest> vPoi;
   m_PoiMgr.GetTenthPointPOIs(reference,segmentKey,&vPoi);
   return vPoi;
}

std::vector<pgsPointOfInterest> CBridgeAgentImp::GetCriticalSections(pgsTypes::LimitState limitState,const CGirderKey& girderKey)
{
   GET_IFACE(ILibrary,pLib);
   GET_IFACE(ISpecification,pSpec);
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( pSpec->GetSpecification().c_str() );

   // LRFD 2004 and later, critical section is only a function of dv, which comes from the calculation of Mu,
   // so critical section is not a function of the limit state. We will work with the Strength I limit state
   if ( lrfdVersionMgr::ThirdEdition2004 <= pSpecEntry->GetSpecificationType() )
      limitState = pgsTypes::StrengthI;

   int idx = limitState == pgsTypes::StrengthI ? 0 : 1;
   std::set<CGirderKey>::iterator found;
   found = m_CriticalSectionState[idx].find( girderKey );
   if ( found == m_CriticalSectionState[idx].end() )
   {
      // Critical sections not previously computed.
      // Do it now.
      GET_IFACE(IShearCapacity,pShearCap);
      const std::vector<CRITSECTDETAILS>& vCSDetails(pShearCap->GetCriticalSectionDetails(limitState,girderKey));

      std::vector<CRITSECTDETAILS>::const_iterator iter(vCSDetails.begin());
      std::vector<CRITSECTDETAILS>::const_iterator end(vCSDetails.end());
      for ( ; iter != end; iter++ )
      {
         const CRITSECTDETAILS& csDetails(*iter);
         if ( csDetails.bAtFaceOfSupport )
            m_PoiMgr.AddPointOfInterest(csDetails.poiFaceOfSupport);
         else
            m_PoiMgr.AddPointOfInterest(csDetails.pCriticalSection->Poi);
      }

      m_CriticalSectionState[idx].insert( girderKey );
   }

   std::vector<pgsPointOfInterest> vPoi;
   m_PoiMgr.GetPointsOfInterest(CSegmentKey(girderKey,ALL_SEGMENTS),(limitState == pgsTypes::StrengthI ? POI_CRITSECTSHEAR1 : POI_CRITSECTSHEAR2),POIMGR_OR,&vPoi);
   return vPoi;
}

std::vector<pgsPointOfInterest> CBridgeAgentImp::GetCriticalSections(pgsTypes::LimitState limitState,const CGirderKey& girderKey,const GDRCONFIG& config)
{
   std::vector<pgsPointOfInterest> vPoi;

   PoiAttributeType attrib = (limitState == pgsTypes::StrengthI ? POI_CRITSECTSHEAR1 : POI_CRITSECTSHEAR2);

   GET_IFACE(IShearCapacity,pShearCap);
   std::vector<Float64> vcsLoc(pShearCap->GetCriticalSections(limitState,girderKey,config));
   std::vector<Float64>::iterator iter(vcsLoc.begin());
   std::vector<Float64>::iterator end(vcsLoc.end());
   for ( ; iter != end; iter++ )
   {
      Float64 x = *iter;
      pgsPointOfInterest poi = GetPointOfInterest(girderKey,x);
      poi.SetAttributes(attrib);
      vPoi.push_back(poi);
   }

   return vPoi;
}

pgsPointOfInterest CBridgeAgentImp::GetPointOfInterest(const CGirderKey& girderKey,Float64 poiDistFromStartOfGirder)
{
   //poiDistFromStartOfGirder = distance start of girder, measured in POI coordinates... that is,
   // this distance is measured from the physical start end of the girder
   Float64 running_distance = 0;
   SegmentIndexType nSegments = GetSegmentCount(girderKey);
   for ( SegmentIndexType segIdx = 0; segIdx < nSegments; segIdx++ )
   {
      CSegmentKey segmentKey(girderKey,segIdx);
      Float64 segmentLength = GetSegmentLayoutLength(segmentKey);

      // if this is the first segment, remove the start offset distance so that segmentLength
      // is measured on the same basis as poiDistFromStartOfGirder
      if ( segIdx == 0 )
      {
         segmentLength -= GetSegmentStartBearingOffset(segmentKey) - GetSegmentStartEndDistance(segmentKey);
      }
      
      // if this is the last segment, remove the end offset distance so that segmentLength is
      // measured on the same basis as poiDistFromStartOfGirder
      if ( segIdx == nSegments-1 )
      {
         segmentLength -= GetSegmentEndBearingOffset(segmentKey) - GetSegmentEndEndDistance(segmentKey);
      }

      if ( running_distance <= poiDistFromStartOfGirder && poiDistFromStartOfGirder <= running_distance+segmentLength )
      {
         // the POI occurs in this segment

#pragma Reminder("UPDATE: search the POI's to see if there is a real one at this location")
         //If there is a real one, return it otherwise return this temporary POI that doesn't
         // have a meaningful ID... also, add segment coordinate
         
         Float64 Xpoi = poiDistFromStartOfGirder - running_distance; // this is the distance from the
         // CLPier/CLTempSupport to the POI (basically a segment coordinate unless this is in
         // the first segment, then Xpoi is measured from the start face of the segment)
         if (segIdx != 0) 
         {
            // if this is not the first segment, adjust Xpoi so that it is measured from the start
            // face of the segment
            Xpoi -= GetSegmentStartBearingOffset(segmentKey) - GetSegmentStartEndDistance(segmentKey);
         }
         pgsPointOfInterest poi(segmentKey,Xpoi);
         return poi;
      }
      else
      {
         running_distance += segmentLength;
      }
   }

   ATLASSERT(false); // should have found it or you are asking for a POI that is off the bridge
   return pgsPointOfInterest();
}

pgsPointOfInterest CBridgeAgentImp::GetPointOfInterest(const CGirderKey& girderKey,PierIndexType pierIdx)
{
   // Get the distance along the girder to the pier.
   Float64 Xg = GetPierLocation(pierIdx,girderKey.girderIndex);
   return GetPointOfInterest(girderKey,Xg);
}

pgsPointOfInterest CBridgeAgentImp::GetPointOfInterest(const CGirderKey& girderKey,SpanIndexType spanIdx,Float64 distFromStartOfSpan)
{
   // convert spanIdx and distFromStartOfSpan to the distance along the girder
   Float64 Xg = 0;
   for ( SpanIndexType idx = 0; idx < spanIdx; idx++ )
   {
      Float64 Ls = GetSpanLength(idx,girderKey.girderIndex);
      Xg += Ls;
   }

   return GetPointOfInterest(girderKey,Xg);
}

std::vector<pgsPointOfInterest> CBridgeAgentImp::GetPointsOfInterestInRange(Float64 xLeft,const pgsPointOfInterest& poi,Float64 xRight)
{
   ValidatePointsOfInterest(poi.GetSegmentKey());

   Float64 xMin = poi.GetDistFromStart() - xLeft;
   Float64 xMax = poi.GetDistFromStart() + xRight;

   std::vector<pgsPointOfInterest> vPoi;
   m_PoiMgr.GetPointsOfInterestInRange(poi.GetSegmentKey(),xMin,xMax,&vPoi);
   return vPoi;
}

SpanIndexType CBridgeAgentImp::GetSpan(const pgsPointOfInterest& poi)
{
   SpanIndexType segStartSpanIdx, segEndSpanIdx;
   GetSpansForSegment(poi.GetSegmentKey(),&segStartSpanIdx,&segEndSpanIdx);

   if ( segStartSpanIdx == segEndSpanIdx )
      return segStartSpanIdx; // easy... segment starts and ends in same span

   // sum span lengths from start of girder through the span where the segment starts
   SpanIndexType gdrStartSpanIdx, gdrEndSpanIdx;
   GetGirderGroupSpans(poi.GetSegmentKey().groupIndex,&gdrStartSpanIdx,&gdrEndSpanIdx);
   Float64 running_span_length = 0;
   for ( SpanIndexType spanIdx = gdrStartSpanIdx; spanIdx <= segStartSpanIdx; spanIdx++ )
   {
      running_span_length += GetSpanLength(spanIdx,poi.GetSegmentKey().girderIndex);
   }

   // sum segment lengths from start of girder to segment before the segment in question
   Float64 running_segment_length = 0;
   for ( SegmentIndexType segIdx = 0; segIdx < poi.GetSegmentKey().segmentIndex; segIdx++ )
   {
      CSegmentKey thisSegment(poi.GetSegmentKey());
      thisSegment.segmentIndex = segIdx;
      running_segment_length += GetSegmentLength(thisSegment);
   }

   // compute distance from start of current segment to the end of the span it begins in
   Float64 s = running_span_length - running_segment_length;
   if ( poi.GetDistFromStart() < s )
      return segStartSpanIdx;

   for ( SpanIndexType spanIdx = segStartSpanIdx+1; spanIdx < segEndSpanIdx; spanIdx++ )
   {
      s += GetSpanLength(spanIdx,poi.GetSegmentKey().girderIndex);
      if ( poi.GetDistFromStart() < s )
         return spanIdx;
   }

   return segEndSpanIdx;
}

Float64 CBridgeAgentImp::ConvertPoiToSegmentCoordinate(const pgsPointOfInterest& poi)
{
#if defined _DEBUG
   const CSegmentKey& segmentKey(poi.GetSegmentKey());
   Float64 CLPierToStartOfGirderDistance = GetSegmentStartBearingOffset(segmentKey) - GetSegmentStartEndDistance(segmentKey);
   Float64 Xs = poi.GetDistFromStart() + CLPierToStartOfGirderDistance;

   if ( poi.HasSegmentCoordinate() )
   {
      ATLASSERT(IsEqual(Xs,poi.GetSegmentCoordinate()));
   }
   return Xs;
#else
   if ( poi.HasSegmentCoordinate() )
      return poi.GetSegmentCoordinate();

   const CSegmentKey& segmentKey(poi.GetSegmentKey());
   Float64 CLPierToStartOfGirderDistance = GetSegmentStartBearingOffset(segmentKey) - GetSegmentStartEndDistance(segmentKey);
   Float64 Xs = poi.GetDistFromStart() + CLPierToStartOfGirderDistance;
   return Xs;
#endif
}

pgsPointOfInterest CBridgeAgentImp::ConvertSegmentCoordinateToPoi(const CSegmentKey& segmentKey,Float64 Xs)
{
   Float64 CLPierToStartOfGirderDistance = GetSegmentStartBearingOffset(segmentKey) - GetSegmentStartEndDistance(segmentKey);
   return GetPointOfInterest(segmentKey,Xs-CLPierToStartOfGirderDistance);
}

Float64 CBridgeAgentImp::ConvertSegmentCoordinateToPoiCoordinate(const CSegmentKey& segmentKey,Float64 Xs)
{
   Float64 brg_offset = GetSegmentStartBearingOffset(segmentKey);
   Float64 end_dist   = GetSegmentStartEndDistance(segmentKey);
   Float64 start_offset = brg_offset - end_dist;
   Float64 Xpoi = Xs - start_offset;
   return Xpoi;
}

Float64 CBridgeAgentImp::ConvertPoiCoordinateToSegmentCoordinate(const CSegmentKey& segmentKey,Float64 Xpoi)
{
   Float64 brg_offset = GetSegmentStartBearingOffset(segmentKey);
   Float64 end_dist   = GetSegmentStartEndDistance(segmentKey);
   Float64 start_offset = brg_offset - end_dist;
   Float64 Xs = Xpoi + start_offset;
   return Xs;
}

Float64 CBridgeAgentImp::ConvertPoiToGirderCoordinate(const pgsPointOfInterest& poi)
{
   // this method converts a POI that is measured from the left end
   // of a segment to a location measured from the left end of the girder

   const CSegmentKey& segmentKey = poi.GetSegmentKey();

   GroupIndexType  grpIdx = segmentKey.groupIndex;
   GirderIndexType gdrIdx = segmentKey.girderIndex;

   // must be a real segment
   ATLASSERT(grpIdx != INVALID_INDEX);
   ATLASSERT(gdrIdx != INVALID_INDEX);
   ATLASSERT(segmentKey.segmentIndex != INVALID_INDEX);

   // Sum the layout length of the segment from the start of the girder, up to, but not including
   // the segment we are interested in
   Float64 Xg = 0; // distance along the girder measured from the CL Pier at the start of the girder
   if ( poi.HasGirderCoordinate() )
   {
      Xg = poi.GetGirderCoordinate();
   }
   else
   {
      for ( SegmentIndexType segIdx = 0; segIdx < segmentKey.segmentIndex; segIdx++ )
      {
         CSegmentKey thisSegmentKey(grpIdx,gdrIdx,segIdx);

         // CL Pier - CL Pier length of the segment centerline
         Float64 L = GetSegmentLayoutLength(thisSegmentKey);

         Xg += L;
      }
      
      // add the distance from the start of the segment to the poi
      Float64 Xs = ConvertPoiToSegmentCoordinate(poi);
      Xg += Xs;

      // We want to measure from the left face of the girder so adjust for the connection geometry

      // |<------- Xg ----->>
      // |<-- brg offset->|
      // |   |<-end dist->|
      // |<->|-- deduct this distance (start_offset)
      // |   |
      // |   +---------------------------------------
      // |   |
      // |   +---------------------------------------
      CSegmentKey firstSegmentKey(segmentKey.groupIndex,segmentKey.girderIndex,0);
      Float64 start_offset = GetSegmentStartBearingOffset(firstSegmentKey) - GetSegmentStartEndDistance(firstSegmentKey);

      Xg -= start_offset;
   }

   return Xg;
}

pgsPointOfInterest CBridgeAgentImp::ConvertGirderCoordinateToPoi(const CGirderKey& girderKey,Float64 Xg)
{
   CSegmentKey segmentKey(girderKey,0);
   Float64 CLPierToStartOfGirderDistance = GetSegmentStartBearingOffset(segmentKey) - GetSegmentStartEndDistance(segmentKey);
   Float64 Xgp = Xg + CLPierToStartOfGirderDistance;
   return ConvertGirderPathCoordinateToPoi(girderKey,Xgp);
}

Float64 CBridgeAgentImp::ConvertPoiToGirderPathCoordinate(const pgsPointOfInterest& poi)
{
#if defined _DEBUG
   Float64 Xg = ConvertPoiToGirderCoordinate(poi);
   
   CSegmentKey segmentKey(poi.GetSegmentKey());
   segmentKey.segmentIndex = 0;
   Float64 CLPierToStartOfGirderDistance = GetSegmentStartBearingOffset(segmentKey) - GetSegmentStartEndDistance(segmentKey);
   Float64 Xgp = Xg + CLPierToStartOfGirderDistance;

   if ( poi.HasGirderPathCoordinate() )
   {
      ATLASSERT(IsEqual(Xgp,poi.GetGirderPathCoordinate()));
   }
   return Xgp;
#else
   if ( poi.HasGirderPathCoordinate() )
      return poi.GetGirderPathCoordinate();

   Float64 Xg = ConvertPoiToGirderCoordinate(poi);

   CSegmentKey segmentKey(poi.GetSegmentKey());
   segmentKey.segmentIndex = 0;
   Float64 CLPierToStartOfGirderDistance = GetSegmentStartBearingOffset(segmentKey) - GetSegmentStartEndDistance(segmentKey);
   Float64 Xgp = Xg + CLPierToStartOfGirderDistance;
   return Xgp;
#endif
}

pgsPointOfInterest CBridgeAgentImp::ConvertGirderPathCoordinateToPoi(const CGirderKey& girderKey,Float64 Xgp)
{
   CSegmentKey segmentKey(girderKey,0);
   Float64 CLPierToStartOfGirderDistance = GetSegmentStartBearingOffset(segmentKey) - GetSegmentStartEndDistance(segmentKey);

   return GetPointOfInterest(girderKey,Xgp - CLPierToStartOfGirderDistance);
}

Float64 CBridgeAgentImp::ConvertGirderCoordinateToGirderPathCoordinate(const CGirderKey& girderKey,Float64 distFromStartOfGirder)
{
   // distFromStartOfGirder is measured from the left face of the girder.... we want to convert this to
   // girder path coordinates. The origin is at the intersection of the CL Girder and CL Pier at the start of the girder
   CSegmentKey segmentKey(girderKey,0);
   Float64 CLPierToStartOfGirderDistance = GetSegmentStartBearingOffset(segmentKey) - GetSegmentStartEndDistance(segmentKey);
   return distFromStartOfGirder + CLPierToStartOfGirderDistance;
}

Float64 CBridgeAgentImp::ConvertGirderPathCoordinateToGirderCoordinate(const CGirderKey& girderKey,Float64 Xg)
{
   CSegmentKey segmentKey(girderKey,0);
   Float64 CLPierToStartOfGirderDistance = GetSegmentStartBearingOffset(segmentKey) - GetSegmentStartEndDistance(segmentKey);
   return Xg - CLPierToStartOfGirderDistance;
}

PoiAttributeType g_TargetAttributes;
bool RemovePOI(const pgsPointOfInterest& poi)
{
   return poi.HasAttribute(g_TargetAttributes);
}

void CBridgeAgentImp::RemovePointsOfInterest(std::vector<pgsPointOfInterest>& vPOI,PoiAttributeType attributes)
{
   g_TargetAttributes = attributes;
   std::vector<pgsPointOfInterest>::iterator new_end = std::remove_if(vPOI.begin(),vPOI.end(),RemovePOI);
   std::vector<pgsPointOfInterest>::size_type newSize = new_end - vPOI.begin();
   vPOI.resize(newSize);
}

/////////////////////////////////////////////////////////////////////////
// ISectionProperties
//
pgsTypes::SectionPropertyMode CBridgeAgentImp::GetSectionPropertiesMode()
{
   GET_IFACE(ILibrary,       pLib);
   GET_IFACE(ISpecification, pSpec);
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( pSpec->GetSpecification().c_str() );
   pgsTypes::SectionPropertyMode sectPropMode = pSpecEntry->GetSectionPropertyMode();
   return sectPropMode;
}

Float64 CBridgeAgentImp::GetHg(IntervalIndexType intervalIdx,const pgsPointOfInterest& poi)
{
   // Height of the girder is invariant to the section property type. The height will
   // always be the same for gross and transformed properties.
   GET_IFACE(ILibrary,       pLib);
   GET_IFACE(ISpecification, pSpec);
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( pSpec->GetSpecification().c_str() );
   pgsTypes::SectionPropertyType sectPropType = (pgsTypes::SectionPropertyType)(pSpecEntry->GetSectionPropertyMode());

   SectProp& props = GetSectionProperties(intervalIdx,poi,sectPropType);
   Float64 Yt, Yb;
   props.ShapeProps->get_Ytop(&Yt);
   props.ShapeProps->get_Ybottom(&Yb);

   return Yt+Yb;
}

Float64 CBridgeAgentImp::GetAg(IntervalIndexType intervalIdx,const pgsPointOfInterest& poi)
{
   GET_IFACE(ILibrary,       pLib);
   GET_IFACE(ISpecification, pSpec);
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( pSpec->GetSpecification().c_str() );
   pgsTypes::SectionPropertyType sectPropType = (pgsTypes::SectionPropertyType)(pSpecEntry->GetSectionPropertyMode());
   return GetAg(sectPropType,intervalIdx,poi);
}

Float64 CBridgeAgentImp::GetIx(IntervalIndexType intervalIdx,const pgsPointOfInterest& poi)
{
   GET_IFACE(ILibrary,       pLib);
   GET_IFACE(ISpecification, pSpec);
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( pSpec->GetSpecification().c_str() );
   pgsTypes::SectionPropertyType sectPropType = (pgsTypes::SectionPropertyType)(pSpecEntry->GetSectionPropertyMode());
   return GetIx(sectPropType,intervalIdx,poi);
}

Float64 CBridgeAgentImp::GetIy(IntervalIndexType intervalIdx,const pgsPointOfInterest& poi)
{
   GET_IFACE(ILibrary,       pLib);
   GET_IFACE(ISpecification, pSpec);
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( pSpec->GetSpecification().c_str() );
   pgsTypes::SectionPropertyType sectPropType = (pgsTypes::SectionPropertyType)(pSpecEntry->GetSectionPropertyMode());
   return GetIy(sectPropType,intervalIdx,poi);
}

Float64 CBridgeAgentImp::GetYt(IntervalIndexType intervalIdx,const pgsPointOfInterest& poi)
{
   GET_IFACE(ILibrary,       pLib);
   GET_IFACE(ISpecification, pSpec);
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( pSpec->GetSpecification().c_str() );
   pgsTypes::SectionPropertyType sectPropType = (pgsTypes::SectionPropertyType)(pSpecEntry->GetSectionPropertyMode());
   return GetYt(sectPropType,intervalIdx,poi);
}

Float64 CBridgeAgentImp::GetYb(IntervalIndexType intervalIdx,const pgsPointOfInterest& poi)
{
   GET_IFACE(ILibrary,       pLib);
   GET_IFACE(ISpecification, pSpec);
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( pSpec->GetSpecification().c_str() );
   pgsTypes::SectionPropertyType sectPropType = (pgsTypes::SectionPropertyType)(pSpecEntry->GetSectionPropertyMode());
   return GetYb(sectPropType,intervalIdx,poi);
}

Float64 CBridgeAgentImp::GetSt(IntervalIndexType intervalIdx,const pgsPointOfInterest& poi)
{
   GET_IFACE(ILibrary,       pLib);
   GET_IFACE(ISpecification, pSpec);
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( pSpec->GetSpecification().c_str() );
   pgsTypes::SectionPropertyType sectPropType = (pgsTypes::SectionPropertyType)(pSpecEntry->GetSectionPropertyMode());
   return GetSt(sectPropType,intervalIdx,poi);
}

Float64 CBridgeAgentImp::GetSb(IntervalIndexType intervalIdx,const pgsPointOfInterest& poi)
{
   GET_IFACE(ILibrary,       pLib);
   GET_IFACE(ISpecification, pSpec);
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( pSpec->GetSpecification().c_str() );
   pgsTypes::SectionPropertyType sectPropType = (pgsTypes::SectionPropertyType)(pSpecEntry->GetSectionPropertyMode());
   return GetSb(sectPropType,intervalIdx,poi);
}

Float64 CBridgeAgentImp::GetYtGirder(IntervalIndexType intervalIdx,const pgsPointOfInterest& poi)
{
   GET_IFACE(ILibrary,       pLib);
   GET_IFACE(ISpecification, pSpec);
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( pSpec->GetSpecification().c_str() );
   pgsTypes::SectionPropertyType sectPropType = (pgsTypes::SectionPropertyType)(pSpecEntry->GetSectionPropertyMode());
   return GetYtGirder(sectPropType,intervalIdx,poi);
}

Float64 CBridgeAgentImp::GetStGirder(IntervalIndexType intervalIdx,const pgsPointOfInterest& poi)
{
   GET_IFACE(ILibrary,       pLib);
   GET_IFACE(ISpecification, pSpec);
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( pSpec->GetSpecification().c_str() );
   pgsTypes::SectionPropertyType sectPropType = (pgsTypes::SectionPropertyType)(pSpecEntry->GetSectionPropertyMode());
   return GetStGirder(sectPropType,intervalIdx,poi);
}

Float64 CBridgeAgentImp::GetKt(IntervalIndexType intervalIdx,const pgsPointOfInterest& poi)
{
   GET_IFACE(ILibrary,       pLib);
   GET_IFACE(ISpecification, pSpec);
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( pSpec->GetSpecification().c_str() );
   pgsTypes::SectionPropertyType sectPropType = (pgsTypes::SectionPropertyType)(pSpecEntry->GetSectionPropertyMode());
   return GetKt(sectPropType,intervalIdx,poi);
}

Float64 CBridgeAgentImp::GetKb(IntervalIndexType intervalIdx,const pgsPointOfInterest& poi)
{
   GET_IFACE(ILibrary,       pLib);
   GET_IFACE(ISpecification, pSpec);
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( pSpec->GetSpecification().c_str() );
   pgsTypes::SectionPropertyType sectPropType = (pgsTypes::SectionPropertyType)(pSpecEntry->GetSectionPropertyMode());
   return GetKb(sectPropType,intervalIdx,poi);
}

Float64 CBridgeAgentImp::GetEIx(IntervalIndexType intervalIdx,const pgsPointOfInterest& poi)
{
   GET_IFACE(ILibrary,       pLib);
   GET_IFACE(ISpecification, pSpec);
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( pSpec->GetSpecification().c_str() );
   pgsTypes::SectionPropertyType sectPropType = (pgsTypes::SectionPropertyType)(pSpecEntry->GetSectionPropertyMode());
   return GetEIx(sectPropType,intervalIdx,poi);
}

Float64 CBridgeAgentImp::GetAg(IntervalIndexType intervalIdx,const pgsPointOfInterest& poi,Float64 fcgdr)
{
   GET_IFACE(ILibrary,       pLib);
   GET_IFACE(ISpecification, pSpec);
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( pSpec->GetSpecification().c_str() );
   pgsTypes::SectionPropertyType sectPropType = (pgsTypes::SectionPropertyType)(pSpecEntry->GetSectionPropertyMode());
   return GetAg(sectPropType,intervalIdx,poi,fcgdr);
}

Float64 CBridgeAgentImp::GetIx(IntervalIndexType intervalIdx,const pgsPointOfInterest& poi,Float64 fcgdr)
{
   GET_IFACE(ILibrary,       pLib);
   GET_IFACE(ISpecification, pSpec);
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( pSpec->GetSpecification().c_str() );
   pgsTypes::SectionPropertyType sectPropType = (pgsTypes::SectionPropertyType)(pSpecEntry->GetSectionPropertyMode());
   return GetIx(sectPropType,intervalIdx,poi,fcgdr);
}

Float64 CBridgeAgentImp::GetIy(IntervalIndexType intervalIdx,const pgsPointOfInterest& poi,Float64 fcgdr)
{
   GET_IFACE(ILibrary,       pLib);
   GET_IFACE(ISpecification, pSpec);
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( pSpec->GetSpecification().c_str() );
   pgsTypes::SectionPropertyType sectPropType = (pgsTypes::SectionPropertyType)(pSpecEntry->GetSectionPropertyMode());
   return GetIy(sectPropType,intervalIdx,poi,fcgdr);
}

Float64 CBridgeAgentImp::GetYt(IntervalIndexType intervalIdx,const pgsPointOfInterest& poi,Float64 fcgdr)
{
   GET_IFACE(ILibrary,       pLib);
   GET_IFACE(ISpecification, pSpec);
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( pSpec->GetSpecification().c_str() );
   pgsTypes::SectionPropertyType sectPropType = (pgsTypes::SectionPropertyType)(pSpecEntry->GetSectionPropertyMode());
   return GetYt(sectPropType,intervalIdx,poi,fcgdr);
}

Float64 CBridgeAgentImp::GetYb(IntervalIndexType intervalIdx,const pgsPointOfInterest& poi,Float64 fcgdr)
{
   GET_IFACE(ILibrary,       pLib);
   GET_IFACE(ISpecification, pSpec);
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( pSpec->GetSpecification().c_str() );
   pgsTypes::SectionPropertyType sectPropType = (pgsTypes::SectionPropertyType)(pSpecEntry->GetSectionPropertyMode());
   return GetYb(sectPropType,intervalIdx,poi,fcgdr);
}

Float64 CBridgeAgentImp::GetSt(IntervalIndexType intervalIdx,const pgsPointOfInterest& poi,Float64 fcgdr)
{
   GET_IFACE(ILibrary,       pLib);
   GET_IFACE(ISpecification, pSpec);
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( pSpec->GetSpecification().c_str() );
   pgsTypes::SectionPropertyType sectPropType = (pgsTypes::SectionPropertyType)(pSpecEntry->GetSectionPropertyMode());
   return GetSt(sectPropType,intervalIdx,poi,fcgdr);
}

Float64 CBridgeAgentImp::GetSb(IntervalIndexType intervalIdx,const pgsPointOfInterest& poi,Float64 fcgdr)
{
   GET_IFACE(ILibrary,       pLib);
   GET_IFACE(ISpecification, pSpec);
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( pSpec->GetSpecification().c_str() );
   pgsTypes::SectionPropertyType sectPropType = (pgsTypes::SectionPropertyType)(pSpecEntry->GetSectionPropertyMode());
   return GetSb(sectPropType,intervalIdx,poi,fcgdr);
}

Float64 CBridgeAgentImp::GetYtGirder(IntervalIndexType intervalIdx,const pgsPointOfInterest& poi,Float64 fcgdr)
{
   GET_IFACE(ILibrary,       pLib);
   GET_IFACE(ISpecification, pSpec);
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( pSpec->GetSpecification().c_str() );
   pgsTypes::SectionPropertyType sectPropType = (pgsTypes::SectionPropertyType)(pSpecEntry->GetSectionPropertyMode());
   return GetYtGirder(sectPropType,intervalIdx,poi,fcgdr);
}

Float64 CBridgeAgentImp::GetStGirder(IntervalIndexType intervalIdx,const pgsPointOfInterest& poi,Float64 fcgdr)
{
   GET_IFACE(ILibrary,       pLib);
   GET_IFACE(ISpecification, pSpec);
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( pSpec->GetSpecification().c_str() );
   pgsTypes::SectionPropertyType sectPropType = (pgsTypes::SectionPropertyType)(pSpecEntry->GetSectionPropertyMode());
   return GetStGirder(sectPropType,intervalIdx,poi,fcgdr);
}

Float64 CBridgeAgentImp::GetAg(pgsTypes::SectionPropertyType spType,IntervalIndexType intervalIdx,const pgsPointOfInterest& poi)
{
   SectProp& props = GetSectionProperties(intervalIdx,poi,spType);
   Float64 area;
   props.ShapeProps->get_Area(&area);
   return area;
}

Float64 CBridgeAgentImp::GetIx(pgsTypes::SectionPropertyType spType,IntervalIndexType intervalIdx,const pgsPointOfInterest& poi)
{
   SectProp& props = GetSectionProperties(intervalIdx,poi,spType);
   Float64 ixx;
   props.ShapeProps->get_Ixx(&ixx);
   return ixx;
}

Float64 CBridgeAgentImp::GetIy(pgsTypes::SectionPropertyType spType,IntervalIndexType intervalIdx,const pgsPointOfInterest& poi)
{
   SectProp& props = GetSectionProperties(intervalIdx,poi,spType);
   Float64 iyy;
   props.ShapeProps->get_Iyy(&iyy);
   return iyy;
}

Float64 CBridgeAgentImp::GetYt(pgsTypes::SectionPropertyType spType,IntervalIndexType intervalIdx,const pgsPointOfInterest& poi)
{
   SectProp& props = GetSectionProperties(intervalIdx,poi,spType);
   Float64 yt;
   props.ShapeProps->get_Ytop(&yt);
   return yt;
}

Float64 CBridgeAgentImp::GetYb(pgsTypes::SectionPropertyType spType,IntervalIndexType intervalIdx,const pgsPointOfInterest& poi)
{
   SectProp& props = GetSectionProperties(intervalIdx,poi,spType);
   Float64 yb;
   props.ShapeProps->get_Ybottom(&yb);
   return yb;
}

Float64 CBridgeAgentImp::GetSt(pgsTypes::SectionPropertyType spType,IntervalIndexType intervalIdx,const pgsPointOfInterest& poi)
{
   SectProp& props = GetSectionProperties(intervalIdx,poi,spType);

   Float64 ixx;
   props.ShapeProps->get_Ixx(&ixx);
   
   Float64 yt;
   props.ShapeProps->get_Ytop(&yt);

   Float64 St = (IsZero(yt) ? 0 : -ixx/yt);

   IntervalIndexType compositeDeckInterval = m_IntervalManager.GetCompositeDeckInterval();
   if ( compositeDeckInterval <= intervalIdx && IsCompositeDeck() )
   {
      // if there is a composite deck and the deck is on, return St (Stop deck) in terms of deck material
      Float64 Eg = (poi.HasAttribute(POI_CLOSURE) ? GetClosurePourEc(poi.GetSegmentKey(),intervalIdx) : GetSegmentEc(poi.GetSegmentKey(),intervalIdx));
      Float64 Es = GetDeckEc(intervalIdx);

      Float64 n = Eg/Es;
      St *= n;
   }

   return St;
}

Float64 CBridgeAgentImp::GetSb(pgsTypes::SectionPropertyType spType,IntervalIndexType intervalIdx,const pgsPointOfInterest& poi)
{
   SectProp& props = GetSectionProperties(intervalIdx,poi,spType);
   
   Float64 ixx;
   props.ShapeProps->get_Ixx(&ixx);
   
   Float64 yb;
   props.ShapeProps->get_Ybottom(&yb);

   Float64 Sb = (IsZero(yb) ? 0 : ixx/yb);

   return Sb;
}

Float64 CBridgeAgentImp::GetYtGirder(pgsTypes::SectionPropertyType spType,IntervalIndexType intervalIdx,const pgsPointOfInterest& poi)
{
   SectProp& props = GetSectionProperties(intervalIdx,poi,spType);
   return props.YtopGirder;
}

Float64 CBridgeAgentImp::GetStGirder(pgsTypes::SectionPropertyType spType,IntervalIndexType intervalIdx,const pgsPointOfInterest& poi)
{
   SectProp& props = GetSectionProperties(intervalIdx,poi,spType);
   Float64 Yt = props.YtopGirder;
   Float64 Ix;
   props.ShapeProps->get_Ixx(&Ix);
   Float64 St = (IsZero(Yt) ? 0 : -Ix/Yt);
   return St;
}

Float64 CBridgeAgentImp::GetKt(pgsTypes::SectionPropertyType spType,IntervalIndexType intervalIdx,const pgsPointOfInterest& poi)
{
   SectProp& props = GetSectionProperties(intervalIdx,poi,spType);

   Float64 yb;
   props.ShapeProps->get_Ybottom(&yb);
   
   Float64 area;
   props.ShapeProps->get_Area(&area);

   Float64 Ix;
   props.ShapeProps->get_Ixx(&Ix);

   Float64 k = (IsZero(area*yb) ? 0 : -Ix/(area*yb));
   return k;
}

Float64 CBridgeAgentImp::GetKb(pgsTypes::SectionPropertyType spType,IntervalIndexType intervalIdx,const pgsPointOfInterest& poi)
{
   SectProp& props = GetSectionProperties(intervalIdx,poi,spType);

   Float64 yt;
   props.ShapeProps->get_Ytop(&yt);
   
   Float64 area;
   props.ShapeProps->get_Area(&area);

   Float64 Ix;
   props.ShapeProps->get_Ixx(&Ix);

   Float64 k = (IsZero(area*yt) ? 0 : Ix/(area*yt));
   return k;
}

Float64 CBridgeAgentImp::GetEIx(pgsTypes::SectionPropertyType spType,IntervalIndexType intervalIdx,const pgsPointOfInterest& poi)
{
   SectProp& props = GetSectionProperties(intervalIdx,poi,spType);

   Float64 EIx;
   props.ElasticProps->get_EIxx(&EIx);
   return EIx;
}

Float64 CBridgeAgentImp::GetAg(pgsTypes::SectionPropertyType spType,IntervalIndexType intervalIdx,const pgsPointOfInterest& poi,Float64 fcgdr)
{
   const CSegmentKey& segmentKey = poi.GetSegmentKey();

   bool bEcChanged;
   Float64 E = GetSegmentEc(segmentKey,intervalIdx,fcgdr,&bEcChanged);

   // if the "trial" girder strength is the same as the real girder strength
   // don't do a bunch of extra work. Return the properties for the real girder
   if ( !bEcChanged )
   {
      return GetAg(spType,intervalIdx,poi);
   }

   CComPtr<IShapeProperties> sprops;
   GetShapeProperties(spType,intervalIdx,poi,E,&sprops);

   Float64 Ag;
   sprops->get_Area(&Ag);
   return Ag;
}

Float64 CBridgeAgentImp::GetIx(pgsTypes::SectionPropertyType spType,IntervalIndexType intervalIdx,const pgsPointOfInterest& poi,Float64 fcgdr)
{
   const CSegmentKey& segmentKey = poi.GetSegmentKey();

   // Compute what the secant modulus would be if f'c = fcgdr
   bool bEcChanged;
   Float64 E = GetSegmentEc(segmentKey,intervalIdx,fcgdr,&bEcChanged);

   // if the "trial" girder strength is the same as the real girder strength
   // don't do a bunch of extra work. Return the properties for the real girder
   if ( !bEcChanged )
   {
      return GetIx(spType,intervalIdx,poi);
   }

   CComPtr<IShapeProperties> sprops;
   GetShapeProperties(spType,intervalIdx,poi,E,&sprops);

   Float64 Ix;
   sprops->get_Ixx(&Ix);
   return Ix;
}

Float64 CBridgeAgentImp::GetIy(pgsTypes::SectionPropertyType spType,IntervalIndexType intervalIdx,const pgsPointOfInterest& poi,Float64 fcgdr)
{
   const CSegmentKey& segmentKey = poi.GetSegmentKey();

   bool bEcChanged;
   Float64 E = GetSegmentEc(segmentKey,intervalIdx,fcgdr,&bEcChanged);

   // if the "trial" girder strength is the same as the real girder strength
   // don't do a bunch of extra work. Return the properties for the real girder
   if ( !bEcChanged )
   {
      return GetIy(spType,intervalIdx,poi);
   }

   CComPtr<IShapeProperties> sprops;
   GetShapeProperties(spType,intervalIdx,poi,E,&sprops);

   Float64 Iy;
   sprops->get_Iyy(&Iy);
   return Iy;
}

Float64 CBridgeAgentImp::GetYt(pgsTypes::SectionPropertyType spType,IntervalIndexType intervalIdx,const pgsPointOfInterest& poi,Float64 fcgdr)
{
   const CSegmentKey& segmentKey = poi.GetSegmentKey();

   bool bEcChanged;
   Float64 E = GetSegmentEc(segmentKey,intervalIdx,fcgdr,&bEcChanged);

   // if the "trial" girder strength is the same as the real girder strength
   // don't do a bunch of extra work. Return the properties for the real girder
   if ( !bEcChanged )
   {
      return GetYt(spType,intervalIdx,poi);
   }

   CComPtr<IShapeProperties> sprops;
   GetShapeProperties(spType,intervalIdx,poi,E,&sprops);

   Float64 yt;
   sprops->get_Ytop(&yt);
   return yt;
}

Float64 CBridgeAgentImp::GetYb(pgsTypes::SectionPropertyType spType,IntervalIndexType intervalIdx,const pgsPointOfInterest& poi,Float64 fcgdr)
{
   const CSegmentKey& segmentKey = poi.GetSegmentKey();

   bool bEcChanged;
   Float64 E = GetSegmentEc(segmentKey,intervalIdx,fcgdr,&bEcChanged);

   // if the "trial" girder strength is the same as the real girder strength
   // don't do a bunch of extra work. Return the properties for the real girder
   if ( !bEcChanged )
   {
      return GetYb(spType,intervalIdx,poi);
   }

   CComPtr<IShapeProperties> sprops;
   GetShapeProperties(spType,intervalIdx,poi,E,&sprops);

   Float64 yb;
   sprops->get_Ybottom(&yb);
   return yb;
}

Float64 CBridgeAgentImp::GetSt(pgsTypes::SectionPropertyType spType,IntervalIndexType intervalIdx,const pgsPointOfInterest& poi,Float64 fcgdr)
{
   const CSegmentKey& segmentKey = poi.GetSegmentKey();

   bool bEcChanged;
   Float64 E = GetSegmentEc(segmentKey,intervalIdx,fcgdr,&bEcChanged);

   // if the "trial" girder strength is the same as the real girder strength
   // don't do a bunch of extra work. Return the properties for the real girder
   if ( !bEcChanged )
   {
      return GetSt(spType,intervalIdx,poi);
   }

   CComPtr<IShapeProperties> sprops;
   GetShapeProperties(spType,intervalIdx,poi,E,&sprops);
   Float64 ixx;
   sprops->get_Ixx(&ixx);
   
   Float64 yt;
   sprops->get_Ytop(&yt);

   Float64 St = (IsZero(yt) ? 0 : -ixx/yt);

   IntervalIndexType compositeDeckInterval = m_IntervalManager.GetCompositeDeckInterval();
   if ( compositeDeckInterval <= intervalIdx && IsCompositeDeck() )
   {
      // if there is a composite deck and the deck is on, return St (Stop deck) in terms of deck material
      Float64 Eg = E;
      Float64 Es = GetDeckEc(intervalIdx);

      Float64 n = Eg/Es;
      St *= n;
   }

   return St;
}

Float64 CBridgeAgentImp::GetSb(pgsTypes::SectionPropertyType spType,IntervalIndexType intervalIdx,const pgsPointOfInterest& poi,Float64 fcgdr)
{
   const CSegmentKey& segmentKey = poi.GetSegmentKey();

   bool bEcChanged;
   Float64 E = GetSegmentEc(segmentKey,intervalIdx,fcgdr,&bEcChanged);

   // if the "trial" girder strength is the same as the real girder strength
   // don't do a bunch of extra work. Return the properties for the real girder
   if ( !bEcChanged )
   {
      return GetSb(spType,intervalIdx,poi);
   }

   CComPtr<IShapeProperties> sprops;
   GetShapeProperties(intervalIdx,poi,E,&sprops);

   Float64 ixx;
   sprops->get_Ixx(&ixx);
   
   Float64 yb;
   sprops->get_Ybottom(&yb);

   Float64 Sb = (IsZero(yb) ? 0 : ixx/yb);

   return Sb;
}

Float64 CBridgeAgentImp::GetYtGirder(pgsTypes::SectionPropertyType spType,IntervalIndexType intervalIdx,const pgsPointOfInterest& poi,Float64 fcgdr)
{
   const CSegmentKey& segmentKey = poi.GetSegmentKey();

   bool bEcChanged;
   Float64 E = GetSegmentEc(segmentKey,intervalIdx,fcgdr,&bEcChanged);

   // if the "trial" girder strength is the same as the real girder strength
   // don't do a bunch of extra work. Return the properties for the real girder
   if ( !bEcChanged )
   {
      return GetYtGirder(spType,intervalIdx,poi);
   }

   CComPtr<IShapeProperties> sprops;
   GetShapeProperties(intervalIdx,poi,E,&sprops);

   IntervalIndexType compositeDeckInterval = m_IntervalManager.GetCompositeDeckInterval();
   Float64 YtopGirder;
   if ( intervalIdx < compositeDeckInterval )
   {
      sprops->get_Ytop(&YtopGirder);
   }
   else
   {
      IntervalIndexType castDeckInterval = m_IntervalManager.GetCastDeckInterval();
      Float64 Hg = GetHg(castDeckInterval,poi);
      Float64 Yb = GetYb(spType,intervalIdx,poi,fcgdr);
      YtopGirder = Hg - Yb;
   }

   return YtopGirder;
}

Float64 CBridgeAgentImp::GetStGirder(pgsTypes::SectionPropertyType spType,IntervalIndexType intervalIdx,const pgsPointOfInterest& poi,Float64 fcgdr)
{
   Float64 Ixx = GetIx(spType,intervalIdx,poi,fcgdr);
   Float64 Yt  = GetYtGirder(spType,intervalIdx,poi,fcgdr);
   return (IsZero(Yt) ? 0 : -Ixx/Yt);
}

Float64 CBridgeAgentImp::GetNetAg(IntervalIndexType intervalIdx,const pgsPointOfInterest& poi)
{
   SectProp& props = GetSectionProperties(intervalIdx,poi,SPT_NET_GIRDER);
   Float64 area;
   props.ShapeProps->get_Area(&area);
   return area;
}

Float64 CBridgeAgentImp::GetNetIg(IntervalIndexType intervalIdx,const pgsPointOfInterest& poi)
{
   SectProp& props = GetSectionProperties(intervalIdx,poi,SPT_NET_GIRDER);
   Float64 ixx;
   props.ShapeProps->get_Ixx(&ixx);
   return ixx;
}

Float64 CBridgeAgentImp::GetNetYbg(IntervalIndexType intervalIdx,const pgsPointOfInterest& poi)
{
   SectProp& props = GetSectionProperties(intervalIdx,poi,SPT_NET_GIRDER);
   Float64 yb;
   props.ShapeProps->get_Ybottom(&yb);
   return yb;
}

Float64 CBridgeAgentImp::GetNetYtg(IntervalIndexType intervalIdx,const pgsPointOfInterest& poi)
{
   SectProp& props = GetSectionProperties(intervalIdx,poi,SPT_NET_GIRDER);
   Float64 yt;
   props.ShapeProps->get_Ytop(&yt);
   return yt;
}

Float64 CBridgeAgentImp::GetNetAd(IntervalIndexType intervalIdx,const pgsPointOfInterest& poi)
{
   SectProp& props = GetSectionProperties(intervalIdx,poi,SPT_NET_DECK);
   Float64 area;
   props.ShapeProps->get_Area(&area);
   return area;
}

Float64 CBridgeAgentImp::GetNetId(IntervalIndexType intervalIdx,const pgsPointOfInterest& poi)
{
   SectProp& props = GetSectionProperties(intervalIdx,poi,SPT_NET_DECK);
   Float64 ixx;
   props.ShapeProps->get_Ixx(&ixx);
   return ixx;
}

Float64 CBridgeAgentImp::GetNetYbd(IntervalIndexType intervalIdx,const pgsPointOfInterest& poi)
{
   SectProp& props = GetSectionProperties(intervalIdx,poi,SPT_NET_DECK);
   Float64 yb;
   props.ShapeProps->get_Ybottom(&yb);
   return yb;
}

Float64 CBridgeAgentImp::GetNetYtd(IntervalIndexType intervalIdx,const pgsPointOfInterest& poi)
{
   SectProp& props = GetSectionProperties(intervalIdx,poi,SPT_NET_DECK);
   Float64 yt;
   props.ShapeProps->get_Ytop(&yt);
   return yt;
}

Float64 CBridgeAgentImp::GetQSlab(const pgsPointOfInterest& poi)
{
   GET_IFACE(ILibrary,       pLib);
   GET_IFACE(ISpecification, pSpec);
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( pSpec->GetSpecification().c_str() );
   pgsTypes::SectionPropertyType sectPropType = (pgsTypes::SectionPropertyType)(pSpecEntry->GetSectionPropertyMode());

   IntervalIndexType compositeDeckIntervalIdx = m_IntervalManager.GetCompositeDeckInterval();
   SectProp& props = GetSectionProperties(compositeDeckIntervalIdx,poi,sectPropType);
   return props.Qslab;
}

Float64 CBridgeAgentImp::GetAcBottomHalf(const pgsPointOfInterest& poi)
{
   GET_IFACE(ILibrary,       pLib);
   GET_IFACE(ISpecification, pSpec);
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( pSpec->GetSpecification().c_str() );
   pgsTypes::SectionPropertyType sectPropType = (pgsTypes::SectionPropertyType)(pSpecEntry->GetSectionPropertyMode());

   IntervalIndexType compositeDeckInterval = m_IntervalManager.GetCompositeDeckInterval();
   SectProp& props = GetSectionProperties(compositeDeckInterval,poi,sectPropType);
   return props.AcBottomHalf;
}

Float64 CBridgeAgentImp::GetAcTopHalf(const pgsPointOfInterest& poi)
{
   GET_IFACE(ILibrary,       pLib);
   GET_IFACE(ISpecification, pSpec);
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( pSpec->GetSpecification().c_str() );
   pgsTypes::SectionPropertyType sectPropType = (pgsTypes::SectionPropertyType)(pSpecEntry->GetSectionPropertyMode());

   IntervalIndexType compositeDeckInterval = m_IntervalManager.GetCompositeDeckInterval();
   SectProp& props = GetSectionProperties(compositeDeckInterval,poi,sectPropType);
   return props.AcTopHalf;
}

Float64 CBridgeAgentImp::GetTributaryFlangeWidth(const pgsPointOfInterest& poi)
{
   Float64 tfw = 0;

   const CSegmentKey& segmentKey = poi.GetSegmentKey();

   if ( IsCompositeDeck() )
   {
      GirderIDType leftGdrID, gdrID, rightGdrID;
      GetAdjacentSuperstructureMemberIDs(segmentKey,&leftGdrID,&gdrID,&rightGdrID);
      HRESULT hr = m_EffFlangeWidthTool->TributaryFlangeWidthBySSMbr(m_Bridge,gdrID,poi.GetDistFromStart(),leftGdrID,rightGdrID,&tfw);
      ATLASSERT(SUCCEEDED(hr));
   }

   return tfw;
}

Float64 CBridgeAgentImp::GetTributaryFlangeWidthEx(const pgsPointOfInterest& poi, Float64* pLftFw, Float64* pRgtFw)
{
   Float64 tfw = 0;
   *pLftFw = 0;
   *pRgtFw = 0;

   if ( IsCompositeDeck() )
   {
      const CSegmentKey& segmentKey = poi.GetSegmentKey();

      GirderIDType leftGdrID,gdrID,rightGdrID;
      GetAdjacentSuperstructureMemberIDs(segmentKey,&leftGdrID,&gdrID,&rightGdrID);

      Float64 dist_from_start_of_girder = poi.GetDistFromStart();
      HRESULT hr = m_EffFlangeWidthTool->TributaryFlangeWidthBySegmentEx(m_Bridge,gdrID,segmentKey.segmentIndex,dist_from_start_of_girder,leftGdrID,rightGdrID,pLftFw,pRgtFw,&tfw);
      ATLASSERT(SUCCEEDED(hr));
   }

   return tfw;
}

Float64 CBridgeAgentImp::GetEffectiveFlangeWidth(const pgsPointOfInterest& poi)
{
   Float64 efw = 0;

   GET_IFACE(ILibrary,       pLib);
   GET_IFACE(ISpecification, pSpec);
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( pSpec->GetSpecification().c_str() );

   if ( pSpecEntry->GetEffectiveFlangeWidthMethod() == pgsTypes::efwmLRFD )
   {
      CComQIPtr<IPGSuperEffectiveFlangeWidthTool> eff_tool(m_EffFlangeWidthTool);
      ATLASSERT(eff_tool);
      eff_tool->put_UseTributaryWidth(VARIANT_FALSE);

      if ( IsCompositeDeck() )
      {
         const CSegmentKey& segmentKey = poi.GetSegmentKey();

         GirderIDType leftGdrID,gdrID,rightGdrID;
         GetAdjacentSuperstructureMemberIDs(segmentKey,&leftGdrID,&gdrID,&rightGdrID);

          HRESULT hr = m_EffFlangeWidthTool->EffectiveFlangeWidthBySSMbr(m_Bridge,gdrID,poi.GetDistFromStart(),leftGdrID,rightGdrID,&efw);
          ATLASSERT(SUCCEEDED(hr));
      }
   }
   else
   {
      CComQIPtr<IPGSuperEffectiveFlangeWidthTool> eff_tool(m_EffFlangeWidthTool);
      ATLASSERT(eff_tool);
      eff_tool->put_UseTributaryWidth(VARIANT_TRUE);

      efw = GetTributaryFlangeWidth(poi);
   }

   return efw;
}

Float64 CBridgeAgentImp::GetEffectiveDeckArea(const pgsPointOfInterest& poi)
{
    Float64 efw   = GetEffectiveFlangeWidth(poi);
    Float64 tSlab = GetStructuralSlabDepth(poi);

    return efw*tSlab;
}

Float64 CBridgeAgentImp::GetTributaryDeckArea(const pgsPointOfInterest& poi)
{
    Float64 tfw   = GetTributaryFlangeWidth(poi);
    Float64 tSlab = GetStructuralSlabDepth(poi);

    return tfw*tSlab;
}

Float64 CBridgeAgentImp::GetGrossDeckArea(const pgsPointOfInterest& poi)
{
    Float64 tfw   = GetTributaryFlangeWidth(poi);
    Float64 tSlab = GetGrossSlabDepth(poi);

    return tfw*tSlab;
}

Float64 CBridgeAgentImp::GetDistTopSlabToTopGirder(const pgsPointOfInterest& poi)
{
   // camber effects are ignored
   VALIDATE( BRIDGE );

   // top of girder reference chord elevation
   Float64 yc = GetTopGirderReferenceChordElevation(poi);

   // get station and offset for poi
   Float64 station,offset;
   GetStationAndOffset(poi,&station,&offset);
   offset = IsZero(offset) ? 0 : offset;

   // top of alignment elevation above girder
   Float64 ya = GetElevation(station,offset);

   Float64 slab_offset = GetSlabOffset(poi);

   return ya - yc + slab_offset;
}

void CBridgeAgentImp::ReportEffectiveFlangeWidth(const CGirderKey& girderKey,rptChapter* pChapter,IEAFDisplayUnits* pDisplayUnits)
{
   GET_IFACE(ILibrary,       pLib);
   GET_IFACE(ISpecification, pSpec);
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( pSpec->GetSpecification().c_str() );

   CComQIPtr<IPGSuperEffectiveFlangeWidthTool> eff_tool(m_EffFlangeWidthTool);
   ATLASSERT(eff_tool);
   if ( pSpecEntry->GetEffectiveFlangeWidthMethod() == pgsTypes::efwmLRFD )
   {
      eff_tool->put_UseTributaryWidth(VARIANT_FALSE);
   }
   else
   {
      eff_tool->put_UseTributaryWidth(VARIANT_TRUE);
   }

   CComQIPtr<IReportEffectiveFlangeWidth> report(m_EffFlangeWidthTool);
   report->ReportEffectiveFlangeWidth(m_pBroker,m_Bridge,girderKey,pChapter,pDisplayUnits);
}

Float64 CBridgeAgentImp::GetPerimeter(const pgsPointOfInterest& poi)
{
   GET_IFACE(ILibrary,       pLib);
   GET_IFACE(ISpecification, pSpec);
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( pSpec->GetSpecification().c_str() );
   pgsTypes::SectionPropertyType sectPropType = (pgsTypes::SectionPropertyType)(pSpecEntry->GetSectionPropertyMode());

   // want the perimeter of the plain girder...
   IntervalIndexType releasePrestressInterval = m_IntervalManager.GetPrestressReleaseInterval();
   SectProp& props = GetSectionProperties(releasePrestressInterval,poi,sectPropType);
   return props.Perimeter;
}

Float64 CBridgeAgentImp::GetSurfaceArea(const CSegmentKey& segmentKey)
{
   const GirderLibraryEntry* pGdrEntry = GetGirderLibraryEntry(segmentKey);

   CComPtr<IBeamFactory> factory;
   pGdrEntry->GetBeamFactory(&factory);

   return factory->GetSurfaceArea(m_pBroker,segmentKey,true);
}

Float64 CBridgeAgentImp::GetVolume(const CSegmentKey& segmentKey)
{
   const GirderLibraryEntry* pGdrEntry = GetGirderLibraryEntry(segmentKey);

   CComPtr<IBeamFactory> factory;
   pGdrEntry->GetBeamFactory(&factory);

   return factory->GetVolume(m_pBroker,segmentKey);
}

Float64 CBridgeAgentImp::GetBridgeEIxx(Float64 distFromStart)
{
   IntervalIndexType index = m_IntervalManager.GetIntervalCount() - 1;

   CComPtr<ISection> section;
   m_SectCutTool->CreateBridgeSection(m_Bridge,distFromStart,index,bscStructurallyContinuousOnly,&section);

   CComPtr<IElasticProperties> eprops;
   section->get_ElasticProperties(&eprops);

   Float64 EIxx;
   eprops->get_EIxx(&EIxx);

   return EIxx;
}

Float64 CBridgeAgentImp::GetBridgeEIyy(Float64 distFromStart)
{
   IntervalIndexType index = m_IntervalManager.GetIntervalCount() - 1;

   CComPtr<ISection> section;
   m_SectCutTool->CreateBridgeSection(m_Bridge,distFromStart,index,bscStructurallyContinuousOnly,&section);

   CComPtr<IElasticProperties> eprops;
   section->get_ElasticProperties(&eprops);

   Float64 EIyy;
   eprops->get_EIyy(&EIyy);

   return EIyy;
}

Float64 CBridgeAgentImp::GetSegmentWeightPerLength(const CSegmentKey& segmentKey)
{
   IntervalIndexType releaseIntervalIdx = GetPrestressReleaseInterval(segmentKey);
   Float64 ag = GetAg(pgsTypes::sptGross,releaseIntervalIdx,pgsPointOfInterest(segmentKey,0.00));
   Float64 dens = GetSegmentWeightDensity(segmentKey,releaseIntervalIdx);
   Float64 weight_per_length = ag * dens * unitSysUnitsMgr::GetGravitationalAcceleration();
   return weight_per_length;
}

Float64 CBridgeAgentImp::GetSegmentWeight(const CSegmentKey& segmentKey)
{
   Float64 weight_per_length = GetSegmentWeightPerLength(segmentKey);
   Float64 length = GetSegmentLength(segmentKey);
   return weight_per_length * length;
}

Float64 CBridgeAgentImp::GetSegmentHeightAtPier(const CSegmentKey& segmentKey,PierIndexType pierIdx)
{
   CComPtr<IPoint2d> pntSupport1,pntEnd1,pntBrg1,pntBrg2,pntEnd2,pntSupport2;
   GetSegmentEndPoints(segmentKey,&pntSupport1,&pntEnd1,&pntBrg1,&pntBrg2,&pntEnd2,&pntSupport2);

   CComPtr<IPoint2d> pntPier;
   GetSegmentPierIntersection(segmentKey,pierIdx,&pntPier);

   Float64 dist_from_start;
   pntPier->DistanceEx(pntEnd1,&dist_from_start);

   // make sure we don't go past the end of the segment since we are going to ask for
   // the segment height at the release stage.
   dist_from_start = min(dist_from_start,GetSegmentLength(segmentKey));

   pgsPointOfInterest poi(segmentKey,dist_from_start);

   return GetHg(GetPrestressReleaseInterval(segmentKey),poi);
}

Float64 CBridgeAgentImp::GetSegmentHeightAtTemporarySupport(const CSegmentKey& segmentKey,SupportIndexType tsIdx)
{
   CComPtr<IPoint2d> pntSupport1,pntEnd1,pntBrg1,pntBrg2,pntEnd2,pntSupport2;
   GetSegmentEndPoints(segmentKey,&pntSupport1,&pntEnd1,&pntBrg1,&pntBrg2,&pntEnd2,&pntSupport2);

   CComPtr<IPoint2d> pntTS;
   GetSegmentTempSupportIntersection(segmentKey,tsIdx,&pntTS);

   Float64 dist_from_start;
   pntTS->DistanceEx(pntEnd1,&dist_from_start);

   pgsPointOfInterest poi(segmentKey,dist_from_start);

   return GetHg(GetCompositeClosurePourInterval(segmentKey),poi);
}

/////////////////////////////////////////////////////////////////////////
// IShapes
//
void CBridgeAgentImp::GetSegmentShape(IntervalIndexType intervalIdx,const pgsPointOfInterest& poi,bool bOrient,pgsTypes::SectionCoordinateType coordinateType,IShape** ppShape)
{
   VALIDATE(BRIDGE);
#pragma Reminder("UPDATE: these shapes can probably be cached")
   const CSegmentKey& segmentKey  = poi.GetSegmentKey();

   Float64 distFromStartOfSegment = ConvertPoiToSegmentCoordinate(poi);

   // returns a copy of the shape so we can move it around without cloning it
   GirderIDType leftGdrID,gdrID,rightGdrID;
   GetAdjacentSuperstructureMemberIDs(segmentKey,&leftGdrID,&gdrID,&rightGdrID);
   
   HRESULT hr = m_SectCutTool->CreateGirderShapeBySegment(m_Bridge,gdrID,segmentKey.segmentIndex,distFromStartOfSegment,leftGdrID,rightGdrID,intervalIdx,ppShape);
   ATLASSERT(SUCCEEDED(hr));

   // Right now, ppShape is in Bridge Section Coordinates

   if ( coordinateType == pgsTypes::scGirder )
   {
      // Convert to girder section coordinates....
      // the bottom center point is located at (0,0)
      CComQIPtr<IXYPosition> position(*ppShape);

      CComPtr<IPoint2d> point;
      position->get_LocatorPoint(lpBottomCenter,&point);

      point->Move(0,0);

      position->put_LocatorPoint(lpBottomCenter,point);
   }

   if ( bOrient )
   {
      Float64 orientation = GetOrientation(poi.GetSegmentKey());

      Float64 rotation_angle = -orientation;

      CComQIPtr<IXYPosition> position(*ppShape);
      CComPtr<IPoint2d> top_center;
      position->get_LocatorPoint(lpTopCenter,&top_center);
      position->RotateEx(top_center,rotation_angle);
   }
}

void CBridgeAgentImp::GetSlabShape(Float64 station,IShape** ppShape)
{
   VALIDATE(BRIDGE);
   ShapeContainer::iterator found = m_DeckShapes.find(station);

   if ( found != m_DeckShapes.end() )
   {
      (*ppShape) = found->second;
      (*ppShape)->AddRef();
   }
   else
   {
      HRESULT hr = m_SectCutTool->CreateSlabShape(m_Bridge,station,ppShape);
      ATLASSERT(SUCCEEDED(hr));

      if ( *ppShape )
         m_DeckShapes.insert(std::make_pair(station,CComPtr<IShape>(*ppShape)));
   }
}

void CBridgeAgentImp::GetLeftTrafficBarrierShape(Float64 station,IShape** ppShape)
{
   VALIDATE(BRIDGE);
   ShapeContainer::iterator found = m_LeftBarrierShapes.find(station);
   if ( found != m_LeftBarrierShapes.end() )
   {
      found->second->Clone(ppShape);
   }
   else
   {
      CComPtr<IShape> shape;
      HRESULT hr = m_SectCutTool->CreateLeftBarrierShape(m_Bridge,station,&shape);

      if ( SUCCEEDED(hr) )
      {
         m_LeftBarrierShapes.insert(std::make_pair(station,CComPtr<IShape>(shape)));
         shape->Clone(ppShape);
      }
   }
}

void CBridgeAgentImp::GetRightTrafficBarrierShape(Float64 station,IShape** ppShape)
{
   VALIDATE(BRIDGE);
   ShapeContainer::iterator found = m_RightBarrierShapes.find(station);
   if ( found != m_RightBarrierShapes.end() )
   {
      found->second->Clone(ppShape);
   }
   else
   {
      CComPtr<IShape> shape;
      HRESULT hr = m_SectCutTool->CreateRightBarrierShape(m_Bridge,station,&shape);

      if ( SUCCEEDED(hr) )
      {
         m_RightBarrierShapes.insert(std::make_pair(station,CComPtr<IShape>(shape)));
         shape->Clone(ppShape);
      }
   }
}

/////////////////////////////////////////////////////////////////////////
// IBarriers
//
Float64 CBridgeAgentImp::GetAtb(pgsTypes::TrafficBarrierOrientation orientation)
{
   CComPtr<IShapeProperties> props;
   GetBarrierProperties(orientation,&props);

   if ( props == NULL )
      return 0;

   Float64 area;
   props->get_Area(&area);

   return area;
}

Float64 CBridgeAgentImp::GetItb(pgsTypes::TrafficBarrierOrientation orientation)
{
   CComPtr<IShapeProperties> props;
   GetBarrierProperties(orientation,&props);

   if ( props == NULL )
      return 0;

   Float64 Ix;
   props->get_Ixx(&Ix);

   return Ix;
}

Float64 CBridgeAgentImp::GetYbtb(pgsTypes::TrafficBarrierOrientation orientation)
{
   CComPtr<IShapeProperties> props;
   GetBarrierProperties(orientation,&props);

   if ( props == NULL )
      return 0;

   Float64 Yb;
   props->get_Ybottom(&Yb);

   return Yb;
}

Float64 CBridgeAgentImp::GetSidewalkWeight(pgsTypes::TrafficBarrierOrientation orientation)
{
   VALIDATE(BRIDGE);
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();

   IntervalIndexType railingSystemIntervalIdx = GetRailingSystemInterval();

   const CRailingSystem* pRailingSystem = NULL;
   if ( orientation == pgsTypes::tboLeft )
      pRailingSystem = pBridgeDesc->GetLeftRailingSystem();
   else
      pRailingSystem = pBridgeDesc->GetRightRailingSystem();

   Float64 Wsw = 0;
   if ( pRailingSystem->bUseSidewalk )
   {
      // real dl width of sidwalk
      Float64 intEdge, extEdge;
      GetSidewalkDeadLoadEdges(orientation, &intEdge, &extEdge);

      Float64 w = fabs(intEdge-extEdge);
      Float64 tl = pRailingSystem->LeftDepth;
      Float64 tr = pRailingSystem->RightDepth;
      Float64 area = w*(tl + tr)/2;
      Float64 density = GetRailingSystemWeightDensity(orientation,railingSystemIntervalIdx);
      Float64 mpl = area * density; // mass per length
      Float64 g = unitSysUnitsMgr::GetGravitationalAcceleration();
      Wsw = mpl * g;
   }

   return Wsw;
}

bool CBridgeAgentImp::HasSidewalk(pgsTypes::TrafficBarrierOrientation orientation)
{
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();

   const CRailingSystem* pRailingSystem = NULL;
   if ( orientation == pgsTypes::tboLeft )
      pRailingSystem = pBridgeDesc->GetLeftRailingSystem();
   else
      pRailingSystem = pBridgeDesc->GetRightRailingSystem();

   return pRailingSystem->bUseSidewalk;
}

Float64 CBridgeAgentImp::GetExteriorBarrierWeight(pgsTypes::TrafficBarrierOrientation orientation)
{
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();

   IntervalIndexType railingSystemIntervalIdx = GetRailingSystemInterval();

   const CRailingSystem* pRailingSystem = NULL;
   if ( orientation == pgsTypes::tboLeft )
      pRailingSystem = pBridgeDesc->GetLeftRailingSystem();
   else
      pRailingSystem = pBridgeDesc->GetRightRailingSystem();

   Float64 Wext = 0; // weight of exterior barrier
   if ( pRailingSystem->GetExteriorRailing()->GetWeightMethod() == TrafficBarrierEntry::Compute )
   {
      CComPtr<IPolyShape> polyshape;
      pRailingSystem->GetExteriorRailing()->CreatePolyShape(orientation,&polyshape);

      CComQIPtr<IShape> shape(polyshape);
      CComPtr<IShapeProperties> props;
      shape->get_ShapeProperties(&props);
      Float64 area;
      props->get_Area(&area);

      Float64 density = GetRailingSystemWeightDensity(orientation,railingSystemIntervalIdx);
      Float64 mplBarrier = area * density;
      Float64 g = unitSysUnitsMgr::GetGravitationalAcceleration();
      Wext = mplBarrier * g;
   }
   else
   {
      Wext = pRailingSystem->GetExteriorRailing()->GetWeight();
   }

   return Wext;
}

Float64 CBridgeAgentImp::GetInteriorBarrierWeight(pgsTypes::TrafficBarrierOrientation orientation)
{
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();

   IntervalIndexType railingSystemIntervalIdx = GetRailingSystemInterval();

   const CRailingSystem* pRailingSystem = NULL;
   if ( orientation == pgsTypes::tboLeft )
      pRailingSystem = pBridgeDesc->GetLeftRailingSystem();
   else
      pRailingSystem = pBridgeDesc->GetRightRailingSystem();

   Float64 Wint = 0.0; // weight of interior barrier
   if ( pRailingSystem->bUseInteriorRailing )
   {
      if ( pRailingSystem->GetInteriorRailing()->GetWeightMethod() == TrafficBarrierEntry::Compute )
      {
         CComPtr<IPolyShape> polyshape;
         pRailingSystem->GetInteriorRailing()->CreatePolyShape(orientation,&polyshape);

         CComQIPtr<IShape> shape(polyshape);
         CComPtr<IShapeProperties> props;
         shape->get_ShapeProperties(&props);
         Float64 area;
         props->get_Area(&area);

         Float64 density = GetRailingSystemWeightDensity(orientation,railingSystemIntervalIdx);
         Float64 mplBarrier = area * density;
         Float64 g = unitSysUnitsMgr::GetGravitationalAcceleration();
         Wint = mplBarrier * g;
      }
      else
      {
         Wint = pRailingSystem->GetInteriorRailing()->GetWeight();
      }
   }

   return Wint;
}

bool CBridgeAgentImp::HasInteriorBarrier(pgsTypes::TrafficBarrierOrientation orientation)
{
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();

   const CRailingSystem* pRailingSystem = NULL;
   if ( orientation == pgsTypes::tboLeft )
      pRailingSystem = pBridgeDesc->GetLeftRailingSystem();
   else
      pRailingSystem = pBridgeDesc->GetRightRailingSystem();

   return  pRailingSystem->bUseInteriorRailing;
}

Float64 CBridgeAgentImp::GetExteriorBarrierCgToDeckEdge(pgsTypes::TrafficBarrierOrientation orientation)
{
   // Use generic bridge - barriers have been placed properly
   CComPtr<ISidewalkBarrier> pSwBarrier;
   Float64 sign;
   if ( orientation == pgsTypes::tboLeft )
   {
      m_Bridge->get_LeftBarrier(&pSwBarrier);
      sign = 1.0;
   }
   else
   {
      m_Bridge->get_RightBarrier(&pSwBarrier);
      sign = -1.0;
   }

   CComPtr<IBarrier> pBarrier;
   pSwBarrier->get_ExteriorBarrier(&pBarrier);

   CComQIPtr<IShape> shape;
   pBarrier->get_Shape(&shape);

   CComPtr<IShapeProperties> props;
   shape->get_ShapeProperties(&props);

   CComPtr<IPoint2d> cgpoint;
   props->get_Centroid(&cgpoint);

   Float64 xcg;
   cgpoint->get_X(&xcg);

   return xcg*sign;
}

Float64 CBridgeAgentImp::GetInteriorBarrierCgToDeckEdge(pgsTypes::TrafficBarrierOrientation orientation)
{
   // Use generic bridge - barriers have been placed properly
   CComPtr<ISidewalkBarrier> pSwBarrier;
   Float64 sign;
   if ( orientation == pgsTypes::tboLeft )
   {
      m_Bridge->get_LeftBarrier(&pSwBarrier);
      sign = 1.0;
   }
   else
   {
      m_Bridge->get_RightBarrier(&pSwBarrier);
      sign = -1.0;
   }

   CComPtr<IBarrier> pBarrier;
   pSwBarrier->get_InteriorBarrier(&pBarrier);

   CComQIPtr<IShape> shape;
   pBarrier->get_Shape(&shape);

   if (shape)
   {
      CComPtr<IShapeProperties> props;
      shape->get_ShapeProperties(&props);

      CComPtr<IPoint2d> cgpoint;
      props->get_Centroid(&cgpoint);

      Float64 xcg;
      cgpoint->get_X(&xcg);

      return xcg*sign;
   }
   else
   {
      ATLASSERT(0); // client should be checking this
      return 0.0;
   }
}


Float64 CBridgeAgentImp::GetInterfaceWidth(pgsTypes::TrafficBarrierOrientation orientation)
{
   // This is the offset from the edge of deck to the curb line (basically the connection 
   // width of the barrier)
   CComPtr<ISidewalkBarrier> barrier;

   if ( orientation == pgsTypes::tboLeft )
      m_Bridge->get_LeftBarrier(&barrier);
   else
      m_Bridge->get_RightBarrier(&barrier);

   Float64 offset;
   barrier->get_ExteriorCurbWidth(&offset);
   return offset;
}


void CBridgeAgentImp::GetSidewalkDeadLoadEdges(pgsTypes::TrafficBarrierOrientation orientation, Float64* pintEdge, Float64* pextEdge)
{
   VALIDATE(BRIDGE);

   CComPtr<ISidewalkBarrier> barrier;
   if ( orientation == pgsTypes::tboLeft )
   {
      m_Bridge->get_LeftBarrier(&barrier);
   }
   else
   {
      m_Bridge->get_RightBarrier(&barrier);
   }

   VARIANT_BOOL has_sw;
   barrier->get_HasSidewalk(&has_sw);

   Float64 width = 0;
   if ( has_sw!=VARIANT_FALSE )
   {
      CComPtr<IShape> swShape;
      barrier->get_SidewalkShape(&swShape);

      // slab extends to int side of int box if it exists
      CComPtr<IRect2d> bbox;
      swShape->get_BoundingBox(&bbox);

      if ( orientation == pgsTypes::tboLeft )
      {
         bbox->get_Left(pextEdge);
         bbox->get_Right(pintEdge);
      }
      else
      {
         bbox->get_Left(pintEdge);
         bbox->get_Right(pextEdge);
      }
   }
   else
   {
      ATLASSERT(0); // client should not call this if no sidewalk
   }
}

void CBridgeAgentImp::GetSidewalkPedLoadEdges(pgsTypes::TrafficBarrierOrientation orientation, Float64* pintEdge, Float64* pextEdge)
{
   VALIDATE(BRIDGE);

   CComPtr<ISidewalkBarrier> swbarrier;
   if ( orientation == pgsTypes::tboLeft )
   {
      m_Bridge->get_LeftBarrier(&swbarrier);
   }
   else
   {
      m_Bridge->get_RightBarrier(&swbarrier);
   }

   VARIANT_BOOL has_sw;
   swbarrier->get_HasSidewalk(&has_sw);
   if(has_sw!=VARIANT_FALSE)
   {
      CComPtr<IBarrier> extbarrier;
      swbarrier->get_ExteriorBarrier(&extbarrier);

      // Sidewalk width for ped - sidewalk goes from interior edge of exterior barrier to sw width
      CComPtr<IShape> pextbarshape;
      extbarrier->get_Shape(&pextbarshape);
      CComPtr<IRect2d> bbox;
      pextbarshape->get_BoundingBox(&bbox);

      // exterior edge
      if ( orientation == pgsTypes::tboLeft )
      {
         bbox->get_Right(pextEdge);
      }
      else
      {
         bbox->get_Left(pextEdge);
         *pextEdge *= -1.0;
      }

      Float64 width;
      swbarrier->get_SidewalkWidth(&width);

      *pintEdge = *pextEdge + width;
   }
   else
   {
      ATLASSERT(0); // client should not call this if no sidewalk
      *pintEdge = 0.0;
      *pextEdge = 0.0;
   }
}

pgsTypes::TrafficBarrierOrientation CBridgeAgentImp::GetNearestBarrier(const CSegmentKey& segmentKey)
{
   GirderIndexType nGirders = GetGirderCount(segmentKey.groupIndex);
   if ( segmentKey.girderIndex < nGirders/2 )
      return pgsTypes::tboLeft;
   else
      return pgsTypes::tboRight;
}

/////////////////////////////////////////////////////////////////////////
// IGirder
//
bool CBridgeAgentImp::IsPrismatic(IntervalIndexType intervalIdx,const CSegmentKey& segmentKey)
{
#pragma Reminder("REVIEW: review this method")
   // This method makes assuptions about gdrID and the id's of the adjacent girders
   // This may be problematic depeneding on the gdrID encoding scheme

   // SHOULD THIS BE USING CGirderKey instead of CSegmentKey?

   VALIDATE( BRIDGE );

   // assume non-prismatic for all transformed sections
   GET_IFACE(ILibrary,pLib);
   GET_IFACE(ISpecification,pSpec);
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry(pSpec->GetSpecification().c_str());
   if ( pSpecEntry->GetSectionPropertyMode() == pgsTypes::spmTransformed )
      return false;

#pragma Reminder("UPDATE: assuming precast girder bridge")
   SpanIndexType spanIdx = segmentKey.groupIndex;
   GirderIndexType gdrIdx = segmentKey.girderIndex;
   //ATLASSERT(segmentKey.segmentIndex == 0);

   const GirderLibraryEntry* pGirderEntry = GetGirderLibraryEntry(segmentKey);

   CComPtr<IBeamFactory> beamFactory;
   pGirderEntry->GetBeamFactory(&beamFactory);

   bool bPrismaticGirder = beamFactory->IsPrismatic(m_pBroker,segmentKey);
   if ( !bPrismaticGirder )
      return false; // if the bare girder is non-prismiatc... it will always be non-prismatic

   if ( IsCompositeDeck() )
   {
      // there is a composite deck... compare the interval when the deck is made composite
      // against the interval we are evaluating prismatic-ness to

      // if the event we are evaluating is before the composite event then
      // we just have a bare girder... return its prismatic-ness
      if ( intervalIdx < m_IntervalManager.GetCompositeDeckInterval() )
         return bPrismaticGirder;
   }

#pragma Reminder("UPDATE: assuming always non-prismatic girder")
   // The code below works pretty well. It was for precast girder bridges
   // so it has to be updated. Need to compute overhangs at ends and mid-point of
   // segments. Segments can be within a span, full span length, or start in one span
   // and end in another span... update the code below to deal with this...
   // Probably need functions that return slab overhangs based on segment start/end and distFromStart
   return false;

   //// we have a prismatic made composite and are evaluating the girder in a composite event.
   //// check to see if the composite section is non-prismatic

   //// check the direction of each girder... if they aren't parallel, then the composite properties
   //// are non-prismatic
   //// for exterior girders, if the deck edge offset isn't the same at each end of the span,
   //// then the girder will be considered to be non-prismatic as well.
   ////
   //// this may not be the best way to figure it out, but it mostly works well and there isn't
   //// any harm in being wrong except that section properties will be reported verbosely for
   //// prismatic beams when a compact reporting would suffice.
   //CComPtr<IDirection> dirLeftGirder, dirThisGirder, dirRightGirder;
   //Float64 startOverhang = -1;
   //Float64 endOverhang = -1;
   //Float64 midspanOverhang = -1;
   //Float64 leftDir,thisDir,rightDir;
   //bool bIsExterior = IsExteriorGirder(segmentKey);
   //if ( bIsExterior )
   //{
   //   GirderIndexType nGirders = GetGirderCount(spanIdx);
   //   PierIndexType prevPierIdx = spanIdx;
   //   PierIndexType nextPierIdx = prevPierIdx+1;
   //   Float64 prevPierStation = GetPierStation(prevPierIdx);
   //   Float64 nextPierStation = GetPierStation(nextPierIdx);
   //   Float64 spanLength = nextPierStation - prevPierStation;

   //   if ( nGirders == 1 )
   //   {
   //      // if there is only one girder, then consider the section prismatic if the left and right 
   //      // overhangs match at both ends and mid-span of the span
   //      Float64 startOverhangLeft = GetLeftSlabOverhang(prevPierIdx);
   //      Float64 endOverhangLeft   = GetLeftSlabOverhang(nextPierIdx);
   //      Float64 midspanOverhangLeft = GetLeftSlabOverhang(spanIdx,spanLength/2);

   //      bool bLeftEqual = IsEqual(startOverhangLeft,midspanOverhangLeft) && IsEqual(midspanOverhangLeft,endOverhangLeft);


   //      Float64 startOverhangRight = GetRightSlabOverhang(prevPierIdx);
   //      Float64 endOverhangRight   = GetRightSlabOverhang(nextPierIdx);
   //      Float64 midspanOverhangRight = GetRightSlabOverhang(spanIdx,spanLength/2);

   //      bool bRightEqual = IsEqual(startOverhangRight,midspanOverhangRight) && IsEqual(midspanOverhangRight,endOverhangRight);

   //      return (bLeftEqual && bRightEqual);
   //   }
   //   else
   //   {
   //      if ( gdrIdx == 0 )
   //      {
   //         // left exterior girder
   //         startOverhang = GetLeftSlabOverhang(prevPierIdx);
   //         endOverhang   = GetLeftSlabOverhang(nextPierIdx);
   //         midspanOverhang = GetLeftSlabOverhang(spanIdx,spanLength/2);

   //         GetSegmentBearing(segmentKey,  &dirThisGirder);
   //         GetSegmentBearing(CSegmentKey(spanIdx,gdrIdx+1,segmentKey.segmentIndex),&dirRightGirder);
   //         dirThisGirder->get_Value(&thisDir);
   //         dirRightGirder->get_Value(&rightDir);
   //      }
   //      else
   //      {
   //         // right exterior girder
   //         GetSegmentBearing(CSegmentKey(spanIdx,gdrIdx-1,segmentKey.segmentIndex),&dirLeftGirder);
   //         GetSegmentBearing(segmentKey,  &dirThisGirder);
   //         dirLeftGirder->get_Value(&leftDir);
   //         dirThisGirder->get_Value(&thisDir);

   //         startOverhang = GetRightSlabOverhang(prevPierIdx);
   //         endOverhang   = GetRightSlabOverhang(nextPierIdx);
   //         midspanOverhang = GetRightSlabOverhang(spanIdx,spanLength/2);
   //      }
   //   }
   //}
   //else
   //{
   //   // if interior girders are not parallel, then they aren't going to be
   //   // prismatic if they have a composite deck
   //   CSegmentKey leftSegmentKey,rightSegmentKey;
   //   GetAdjacentGirderKeys(segmentKey,&leftSegmentKey,&rightSegmentKey);
   //   GetSegmentBearing(leftSegmentKey,&dirLeftGirder);
   //   GetSegmentBearing(segmentKey,&dirThisGirder);
   //   GetSegmentBearing(rightSegmentKey,&dirRightGirder);

   //   dirLeftGirder->get_Value(&leftDir);
   //   dirThisGirder->get_Value(&thisDir);
   //   dirRightGirder->get_Value(&rightDir);
   //}

   //if ( dirLeftGirder == NULL )
   //{
   //   return (IsEqual(thisDir,rightDir) && IsEqual(startOverhang,midspanOverhang) && IsEqual(midspanOverhang,endOverhang) ? true : false);
   //}
   //else if ( dirRightGirder == NULL )
   //{
   //   return (IsEqual(leftDir,thisDir) && IsEqual(startOverhang,midspanOverhang) && IsEqual(midspanOverhang,endOverhang) ? true : false);
   //}
   //else
   //{
   //   return (IsEqual(leftDir,thisDir) && IsEqual(thisDir,rightDir) ? true : false);
   //}
}

bool CBridgeAgentImp::IsSymmetric(IntervalIndexType intervalIdx,const CSegmentKey& segmentKey)
{
#pragma Reminder("REVIEW")
   // SHOULD THIS BE USING CGirderKey instead of CSegmentKey?

   IntervalIndexType liveLoadIntervalIdx = m_IntervalManager.GetLiveLoadInterval();

   // need to look at girder spacing to see if it is constant
   if ( liveLoadIntervalIdx <= intervalIdx )
      return false;

#pragma Reminder("UPDATE: Figure out if girder is symmetric")
   // other places in the program can gain huge speed efficiencies if the 
   // girder is symmetric (only half the work needs to be done)
   Float64 start_length = GetSegmentStartEndDistance(segmentKey);
   Float64 end_length   = GetSegmentEndEndDistance(segmentKey);
   
   if ( !IsEqual(start_length,end_length) )
      return false; // different end lengths

   Float64 girder_length = GetSegmentLength(segmentKey);

   Float64 left_hp, right_hp;
   GetHarpingPointLocations(segmentKey,&left_hp,&right_hp);
   if ( !IsEqual(left_hp,girder_length - right_hp) )
      return false; // different harping point locations

   if ( !IsDebondingSymmetric(segmentKey) )
      return false;

   return true;
}

MatingSurfaceIndexType CBridgeAgentImp::GetNumberOfMatingSurfaces(const CGirderKey& girderKey)
{
   VALIDATE( BRIDGE );

   CSegmentKey segmentKey(girderKey,0);

   CComPtr<IGirderSection> girder_section;
   HRESULT hr = GetGirderSection(pgsPointOfInterest(segmentKey,0.00),pgsTypes::scBridge,&girder_section);
   ATLASSERT(SUCCEEDED(hr));

   FlangeIndexType count;
   girder_section->get_MatingSurfaceCount(&count);

   return count;
}

Float64 CBridgeAgentImp::GetMatingSurfaceLocation(const pgsPointOfInterest& poi,MatingSurfaceIndexType webIdx)
{
   VALIDATE( BRIDGE );

   CComPtr<IGirderSection> girder_section;
   HRESULT hr = GetGirderSection(poi,pgsTypes::scBridge,&girder_section);
   ATLASSERT(SUCCEEDED(hr));

   Float64 location;
   girder_section->get_MatingSurfaceLocation(webIdx,&location);
   
   return location;
}

Float64 CBridgeAgentImp::GetMatingSurfaceWidth(const pgsPointOfInterest& poi,MatingSurfaceIndexType webIdx)
{
   VALIDATE( BRIDGE );

   CComPtr<IGirderSection> girder_section;
   HRESULT hr = GetGirderSection(poi,pgsTypes::scBridge,&girder_section);
   ATLASSERT(SUCCEEDED(hr));

   Float64 width;
   girder_section->get_MatingSurfaceWidth(webIdx,&width);

   return width;
}

FlangeIndexType CBridgeAgentImp::GetNumberOfTopFlanges(const CSegmentKey& segmentKey)
{
   VALIDATE( BRIDGE );

   CComPtr<IGirderSection> girder_section;
   HRESULT hr = GetGirderSection(pgsPointOfInterest(segmentKey,0.00),pgsTypes::scBridge,&girder_section);
   ATLASSERT(SUCCEEDED(hr));

   FlangeIndexType nFlanges;
   girder_section->get_TopFlangeCount(&nFlanges);
   return nFlanges;
}

Float64 CBridgeAgentImp::GetTopFlangeLocation(const pgsPointOfInterest& poi,FlangeIndexType flangeIdx)
{
   VALIDATE( BRIDGE );

   CComPtr<IGirderSection> girder_section;
   HRESULT hr = GetGirderSection(poi,pgsTypes::scBridge,&girder_section);
   ATLASSERT(SUCCEEDED(hr));

   Float64 location;
   girder_section->get_TopFlangeLocation(flangeIdx,&location);
   return location;
}

Float64 CBridgeAgentImp::GetTopFlangeWidth(const pgsPointOfInterest& poi,FlangeIndexType flangeIdx)
{
   VALIDATE( BRIDGE );

   CComPtr<IGirderSection> girder_section;
   HRESULT hr = GetGirderSection(poi,pgsTypes::scBridge,&girder_section);
   ATLASSERT(SUCCEEDED(hr));

   Float64 width;
   girder_section->get_TopFlangeWidth(flangeIdx,&width);
   return width;
}

Float64 CBridgeAgentImp::GetTopFlangeThickness(const pgsPointOfInterest& poi,FlangeIndexType flangeIdx)
{
   VALIDATE( BRIDGE );

   CComPtr<IGirderSection> girder_section;
   HRESULT hr = GetGirderSection(poi,pgsTypes::scBridge,&girder_section);
   ATLASSERT(SUCCEEDED(hr));

   Float64 thickness;
   girder_section->get_TopFlangeThickness(flangeIdx,&thickness);
   return thickness;
}

Float64 CBridgeAgentImp::GetTopFlangeSpacing(const pgsPointOfInterest& poi,FlangeIndexType flangeIdx)
{
   VALIDATE( BRIDGE );

   CComPtr<IGirderSection> girder_section;
   HRESULT hr = GetGirderSection(poi,pgsTypes::scBridge,&girder_section);
   ATLASSERT(SUCCEEDED(hr));

   Float64 spacing;
   girder_section->get_TopFlangeSpacing(flangeIdx,&spacing);
   return spacing;
}

Float64 CBridgeAgentImp::GetTopFlangeWidth(const pgsPointOfInterest& poi)
{
   MatingSurfaceIndexType nMS = GetNumberOfMatingSurfaces(poi.GetSegmentKey());
   Float64 wtf = 0;
   for ( MatingSurfaceIndexType msIdx = 0; msIdx < nMS; msIdx++ )
   {
      Float64 msw;
      msw = GetMatingSurfaceWidth(poi,msIdx);
      wtf += msw;
}

   return wtf;
}

Float64 CBridgeAgentImp::GetTopWidth(const pgsPointOfInterest& poi)
{
   VALIDATE( BRIDGE );

   CComPtr<IGirderSection> girder_section;
   HRESULT hr = GetGirderSection(poi,pgsTypes::scBridge,&girder_section);
   ATLASSERT(SUCCEEDED(hr));

   Float64 width;
   girder_section->get_TopWidth(&width);

   return width;
}

FlangeIndexType CBridgeAgentImp::GetNumberOfBottomFlanges(const CSegmentKey& segmentKey)
{
   VALIDATE( BRIDGE );

   CComPtr<IGirderSection> girder_section;
   HRESULT hr = GetGirderSection(pgsPointOfInterest(segmentKey,0.00),pgsTypes::scBridge,&girder_section);
   ATLASSERT(SUCCEEDED(hr));

   FlangeIndexType nFlanges;
   girder_section->get_BottomFlangeCount(&nFlanges);
   return nFlanges;
}

Float64 CBridgeAgentImp::GetBottomFlangeLocation(const pgsPointOfInterest& poi,FlangeIndexType flangeIdx)
{
   VALIDATE( BRIDGE );

   CComPtr<IGirderSection> girder_section;
   HRESULT hr = GetGirderSection(poi,pgsTypes::scBridge,&girder_section);
   ATLASSERT(SUCCEEDED(hr));

   Float64 location;
   girder_section->get_BottomFlangeLocation(flangeIdx,&location);
   return location;
}

Float64 CBridgeAgentImp::GetBottomFlangeWidth(const pgsPointOfInterest& poi,FlangeIndexType flangeIdx)
{
   VALIDATE( BRIDGE );

   CComPtr<IGirderSection> girder_section;
   HRESULT hr = GetGirderSection(poi,pgsTypes::scBridge,&girder_section);
   ATLASSERT(SUCCEEDED(hr));

   Float64 width;
   girder_section->get_BottomFlangeWidth(flangeIdx,&width);
   return width;
}

Float64 CBridgeAgentImp::GetBottomFlangeThickness(const pgsPointOfInterest& poi,FlangeIndexType flangeIdx)
{
   VALIDATE( BRIDGE );

   CComPtr<IGirderSection> girder_section;
   HRESULT hr = GetGirderSection(poi,pgsTypes::scBridge,&girder_section);
   ATLASSERT(SUCCEEDED(hr));

   Float64 thickness;
   girder_section->get_BottomFlangeThickness(flangeIdx,&thickness);
   return thickness;
}

Float64 CBridgeAgentImp::GetBottomFlangeSpacing(const pgsPointOfInterest& poi,FlangeIndexType flangeIdx)
{
   VALIDATE( BRIDGE );

   CComPtr<IGirderSection> girder_section;
   HRESULT hr = GetGirderSection(poi,pgsTypes::scBridge,&girder_section);
   ATLASSERT(SUCCEEDED(hr));

   Float64 spacing;
   girder_section->get_BottomFlangeSpacing(flangeIdx,&spacing);
   return spacing;
}

Float64 CBridgeAgentImp::GetBottomFlangeWidth(const pgsPointOfInterest& poi)
{
   VALIDATE( BRIDGE );

   CComPtr<IGirderSection> girder_section;
   HRESULT hr = GetGirderSection(poi,pgsTypes::scBridge,&girder_section);
   ATLASSERT(SUCCEEDED(hr));

   Float64 width = 0;
   FlangeIndexType nFlanges;
   girder_section->get_BottomFlangeCount(&nFlanges);
   for ( FlangeIndexType idx = 0; idx < nFlanges; idx++ )
   {
      Float64 w;
      girder_section->get_BottomFlangeWidth(idx,&w);
      width += w;
   }

   return width;
}

Float64 CBridgeAgentImp::GetBottomWidth(const pgsPointOfInterest& poi)
{
   VALIDATE( BRIDGE );

   CComPtr<IGirderSection> girder_section;
   HRESULT hr = GetGirderSection(poi,pgsTypes::scBridge,&girder_section);
   ATLASSERT(SUCCEEDED(hr));

   Float64 width;
   girder_section->get_BottomWidth(&width);

   return width;
}

Float64 CBridgeAgentImp::GetMinWebWidth(const pgsPointOfInterest& poi)
{
   VALIDATE( BRIDGE );

   CComPtr<IGirderSection> girder_section;
   HRESULT hr = GetGirderSection(poi,pgsTypes::scBridge,&girder_section);
   ATLASSERT(SUCCEEDED(hr));

   Float64 width;
   girder_section->get_MinWebThickness(&width);

   return width;
}

Float64 CBridgeAgentImp::GetMinTopFlangeThickness(const pgsPointOfInterest& poi)
{
   VALIDATE( BRIDGE );

   CComPtr<IGirderSection> girder_section;
   HRESULT hr = GetGirderSection(poi,pgsTypes::scBridge,&girder_section);
   ATLASSERT(SUCCEEDED(hr));

   Float64 ttf;
   girder_section->get_MinTopFlangeThickness(&ttf);
   return ttf;
}

Float64 CBridgeAgentImp::GetMinBottomFlangeThickness(const pgsPointOfInterest& poi)
{
   VALIDATE( BRIDGE );

   CComPtr<IGirderSection> girder_section;
   HRESULT hr = GetGirderSection(poi,pgsTypes::scBridge,&girder_section);
   ATLASSERT(SUCCEEDED(hr));

   Float64 tbf;
   girder_section->get_MinBottomFlangeThickness(&tbf);
   return tbf;
}

Float64 CBridgeAgentImp::GetHeight(const pgsPointOfInterest& poi)
{
   VALIDATE( BRIDGE );

   CComPtr<IGirderSection> girder_section;
   HRESULT hr = GetGirderSection(poi,pgsTypes::scBridge,&girder_section);
   ATLASSERT(SUCCEEDED(hr));

   Float64 height;
   girder_section->get_GirderHeight(&height);

   return height;
}

Float64 CBridgeAgentImp::GetShearWidth(const pgsPointOfInterest& poi)
{
   VALIDATE( BRIDGE );

   CComPtr<IGirderSection> girder_section;
   HRESULT hr = GetGirderSection(poi,pgsTypes::scBridge,&girder_section);
   ATLASSERT(SUCCEEDED(hr));

   Float64 shear_width;
   girder_section->get_ShearWidth(&shear_width);
   return shear_width;
}

Float64 CBridgeAgentImp::GetShearInterfaceWidth(const pgsPointOfInterest& poi)
{
   VALIDATE( BRIDGE );

   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CDeckDescription2* pDeck = pBridgeDesc->GetDeckDescription();

   CComPtr<IGirderSection> girder_section;
   HRESULT hr = GetGirderSection(poi,pgsTypes::scBridge,&girder_section);
   ATLASSERT(SUCCEEDED(hr));

   const CSegmentKey& segmentKey = poi.GetSegmentKey();
   Float64 wMating = 0; // sum of mating surface widths... less deck panel support width

   if ( pDeck->DeckType == pgsTypes::sdtCompositeCIP || pDeck->DeckType == pgsTypes::sdtCompositeOverlay )
   {
      MatingSurfaceIndexType nMatingSurfaces = GetNumberOfMatingSurfaces(segmentKey);
      for ( MatingSurfaceIndexType i = 0; i < nMatingSurfaces; i++ )
      {
         wMating += GetMatingSurfaceWidth(poi,i);
      }
   }
   else if ( pDeck->DeckType == pgsTypes::sdtCompositeSIP )
   {
      // SIP Deck Panel System... Area beneath the deck panesl aren't part of the
      // shear transfer area
      MatingSurfaceIndexType nMatingSurfaces = GetNumberOfMatingSurfaces(segmentKey);
      Float64 panel_support = pDeck->PanelSupport;
      for ( MatingSurfaceIndexType i = 0; i < nMatingSurfaces; i++ )
      {
         CComPtr<ISuperstructureMember> ssMbr;
         GetSuperstructureMember(segmentKey,&ssMbr);
         LocationType locationType;
         ssMbr->get_LocationType(&locationType);

         if ( (locationType == ltLeftExteriorGirder && i == 0) ||
              (locationType == ltRightExteriorGirder && i == nMatingSurfaces-1)
            )
            wMating += GetMatingSurfaceWidth(poi,i) - panel_support;
         else
            wMating += GetMatingSurfaceWidth(poi,i) - 2*panel_support;
      }

      if ( wMating < 0 )
      {
         wMating = 0;

         CString strMsg;

         GET_IFACE(IDocumentType,pDocType);
         if ( pDocType->IsPGSuperDocument() )
            strMsg.Format(_T("Span %d Girder %s, Deck panel support width exceeds half the width of the supporting flange. An interface shear width of 0.0 will be used"),LABEL_SPAN(segmentKey.groupIndex),LABEL_GIRDER(segmentKey.girderIndex));
         else
            strMsg.Format(_T("Group %d Girder %s, Deck panel support width exceeds half the width of the supporting flange. An interface shear width of 0.0 will be used"),LABEL_GROUP(segmentKey.groupIndex),LABEL_GIRDER(segmentKey.girderIndex));

         pgsBridgeDescriptionStatusItem* pStatusItem = 
            new pgsBridgeDescriptionStatusItem(m_StatusGroupID,m_scidBridgeDescriptionWarning,pgsBridgeDescriptionStatusItem::Deck,strMsg);

         GET_IFACE(IEAFStatusCenter,pStatusCenter);
         pStatusCenter->Add(pStatusItem);

      }
   }
   else
   {
      // all other deck types are non-composite so there is no
      // interface width for horizontal shear
      wMating = 0;
   }

   return wMating;
}

WebIndexType CBridgeAgentImp::GetWebCount(const CGirderKey& girderKey)
{
   VALIDATE( BRIDGE );

   CSegmentKey segmentKey(girderKey,0);

   CComPtr<IGirderSection> girder_section;
   HRESULT hr = GetGirderSection(pgsPointOfInterest(segmentKey,0.00),pgsTypes::scBridge,&girder_section);
   ATLASSERT(SUCCEEDED(hr));

   WebIndexType count;
   girder_section->get_WebCount(&count);

   return count;
}

Float64 CBridgeAgentImp::GetWebLocation(const pgsPointOfInterest& poi,WebIndexType webIdx)
{
   VALIDATE( BRIDGE );

   CComPtr<IGirderSection> girder_section;
   HRESULT hr = GetGirderSection(poi,pgsTypes::scBridge,&girder_section);
   ATLASSERT(SUCCEEDED(hr));

   Float64 location;
   girder_section->get_WebLocation(webIdx,&location);
   
   return location;
}

Float64 CBridgeAgentImp::GetWebSpacing(const pgsPointOfInterest& poi,SpacingIndexType spaceIdx)
{
   VALIDATE( BRIDGE );

   CComPtr<IGirderSection> girder_section;
   HRESULT hr = GetGirderSection(poi,pgsTypes::scBridge,&girder_section);
   ATLASSERT(SUCCEEDED(hr));

   Float64 spacing;
   girder_section->get_WebSpacing(spaceIdx,&spacing);

   return spacing;
}

Float64 CBridgeAgentImp::GetWebThickness(const pgsPointOfInterest& poi,WebIndexType webIdx)
{
   VALIDATE( BRIDGE );

   CComPtr<IGirderSection> girder_section;
   HRESULT hr = GetGirderSection(poi,pgsTypes::scBridge,&girder_section);
   ATLASSERT(SUCCEEDED(hr));

   Float64 thickness;
   girder_section->get_WebThickness(webIdx,&thickness);
   
   return thickness;
}

Float64 CBridgeAgentImp::GetCL2ExteriorWebDistance(const pgsPointOfInterest& poi)
{
   VALIDATE( BRIDGE );

   const CSegmentKey& segmentKey = poi.GetSegmentKey();

   CComPtr<IGirderSection> girder_section;
   HRESULT hr = GetGirderSection(poi,pgsTypes::scBridge,&girder_section);
   ATLASSERT(SUCCEEDED(hr));

   GET_IFACE(IBarriers,         pBarriers);
   pgsTypes::TrafficBarrierOrientation side = pBarriers->GetNearestBarrier(segmentKey);
   DirectionType dtside = (side==pgsTypes::tboLeft) ? qcbLeft : qcbRight;

   Float64 dist;
   girder_section->get_CL2ExteriorWebDistance(dtside, &dist);
   
   return dist;
}

Float64 CBridgeAgentImp::GetOrientation(const CSegmentKey& segmentKey)
{
   ValidateSegmentOrientation(segmentKey);

   CComPtr<ISegment> segment;
   GetSegment(segmentKey,&segment);

   Float64 orientation;
   segment->get_Orientation(&orientation);

   return orientation;
}

Float64 CBridgeAgentImp::GetTopGirderReferenceChordElevation(const pgsPointOfInterest& poi)
{
   // top of girder elevation at the poi, ignoring camber effects
   // camber effects are ignored
   VALIDATE( BRIDGE );

   const CSegmentKey& segmentKey = poi.GetSegmentKey();

   Float64 end_size = GetSegmentStartEndDistance(segmentKey);

   // get station at offset at start bearing
   Float64 station, offset;
   GetStationAndOffset(pgsPointOfInterest(segmentKey,end_size),&station,&offset);

   // the girder reference line passes through the deck at this station and offset
   Float64 Y_girder_ref_line_left_bearing = GetElevation(station,offset);

   // slope of the girder in the plane of the girder
   Float64 girder_slope = GetSegmentSlope(segmentKey);

   // top of girder elevation (ignoring camber effects)
   Float64 dist_from_left_bearing = poi.GetDistFromStart() - end_size;
   Float64 yc = Y_girder_ref_line_left_bearing + dist_from_left_bearing*girder_slope;

   return yc;
}

Float64 CBridgeAgentImp::GetTopGirderElevation(const pgsPointOfInterest& poi,MatingSurfaceIndexType matingSurfaceIndex)
{
   GDRCONFIG dummy_config;

   return GetTopGirderElevation(poi,false,dummy_config,matingSurfaceIndex);
}

Float64 CBridgeAgentImp::GetTopGirderElevation(const pgsPointOfInterest& poi,const GDRCONFIG& config,MatingSurfaceIndexType matingSurfaceIdx)
{
   return GetTopGirderElevation(poi,true,config,matingSurfaceIdx);
}

Float64 CBridgeAgentImp::GetTopGirderElevation(const pgsPointOfInterest& poi,bool bUseConfig,const GDRCONFIG& config,MatingSurfaceIndexType matingSurfaceIdx)
{
   VALIDATE(GIRDER);

   const CSegmentKey& segmentKey = poi.GetSegmentKey();

   Float64 end_size = GetSegmentStartEndDistance(segmentKey);

   // girder offset at start bearing
   Float64 station, zs;
   GetStationAndOffset(pgsPointOfInterest(segmentKey,end_size),&station,&zs);

   Float64 webOffset = (matingSurfaceIdx == INVALID_INDEX ? 0 : GetMatingSurfaceLocation(poi,matingSurfaceIdx));

   // roadway surface elevation at start bearing, directly above web centerline
   Float64 Ysb = GetElevation(station,zs + webOffset);

   Float64 girder_slope = GetSegmentSlope(segmentKey);

   Float64 dist_from_start_bearing = poi.GetDistFromStart() - end_size;

   Float64 slab_offset_at_start = GetSlabOffset(segmentKey,pgsTypes::metStart);

   // get the camber
   GET_IFACE(ICamber,pCamber);
   Float64 excess_camber = (bUseConfig ? pCamber->GetExcessCamber(poi,config,CREEP_MAXTIME)
                                       : pCamber->GetExcessCamber(poi,CREEP_MAXTIME) );

   Float64 top_gdr_elev = Ysb - slab_offset_at_start + girder_slope*dist_from_start_bearing + excess_camber;
   return top_gdr_elev;
}

void CBridgeAgentImp::GetProfileShape(const CSegmentKey& segmentKey,IShape** ppShape)
{
   const GirderLibraryEntry* pGirderEntry = GetGirderLibraryEntry(segmentKey);

   CComPtr<IBeamFactory> beamFactory;
   pGirderEntry->GetBeamFactory(&beamFactory);

   beamFactory->CreateGirderProfile(m_pBroker,m_StatusGroupID,segmentKey,pGirderEntry->GetDimensions(),ppShape);
}

bool CBridgeAgentImp::HasShearKey(const CGirderKey& girderKey,pgsTypes::SupportedBeamSpacing spacingType)
{
   const GirderLibraryEntry* pGirderEntry = GetGirderLibraryEntry(girderKey);
   CComPtr<IBeamFactory> beamFactory;
   pGirderEntry->GetBeamFactory(&beamFactory);

   return beamFactory->IsShearKey(pGirderEntry->GetDimensions(), spacingType);
}

void CBridgeAgentImp::GetShearKeyAreas(const CGirderKey& girderKey,pgsTypes::SupportedBeamSpacing spacingType,Float64* uniformArea, Float64* areaPerJoint)
{
   const GirderLibraryEntry* pGirderEntry = GetGirderLibraryEntry(girderKey);
   CComPtr<IBeamFactory> beamFactory;
   pGirderEntry->GetBeamFactory(&beamFactory);

   beamFactory->GetShearKeyAreas(pGirderEntry->GetDimensions(), spacingType, uniformArea, areaPerJoint);
}

void CBridgeAgentImp::GetSegment(const CGirderKey& girderKey,Float64 distFromStartOfGirder,SegmentIndexType* pSegIdx,Float64* pDistFromStartOfSegment)
{
   CComPtr<ISuperstructureMember> ssMbr;
   GetSuperstructureMember(girderKey,&ssMbr);

   CComPtr<ISegment> segment;
   ssMbr->GetDistanceFromStartOfSegment(distFromStartOfGirder,pDistFromStartOfSegment,pSegIdx,&segment);
}

////////////////////////////////////////////////////////////////////////
// IGirderLiftingPointsOfInterest
std::vector<pgsPointOfInterest> CBridgeAgentImp::GetLiftingPointsOfInterest(const CSegmentKey& segmentKey,PoiAttributeType attrib,Uint32 mode)
{
   ValidatePointsOfInterest(segmentKey);

   Uint32 mgrMode = (mode == POIFIND_AND ? POIMGR_AND : POIMGR_OR);

   std::vector<pgsPointOfInterest> poi;
   m_LiftingPoiMgr.GetPointsOfInterest(segmentKey, attrib, mgrMode, &poi );
   return poi;
}

std::vector<pgsPointOfInterest> CBridgeAgentImp::GetLiftingDesignPointsOfInterest(const CSegmentKey& segmentKey,Float64 overhang,PoiAttributeType attrib,Uint32 mode)
{
   // Generates points of interest for the supplied overhang.
   pgsPoiMgr poiMgr;
   LayoutHandlingPoi(segmentKey,10,overhang,overhang,attrib,POI_PICKPOINT,POI_LIFT_SEGMENT,&poiMgr);

   Uint32 mgrMode = (mode == POIFIND_AND ? POIMGR_AND : POIMGR_OR);
   std::vector<pgsPointOfInterest> poi;
   poiMgr.GetPointsOfInterest(segmentKey, attrib, mgrMode, &poi );
   return poi;
}

////////////////////////////////////////////////////////////////////////
// IGirderHaulingPointsOfInterest
std::vector<pgsPointOfInterest> CBridgeAgentImp::GetHaulingPointsOfInterest(const CSegmentKey& segmentKey,PoiAttributeType attrib,Uint32 mode)
{
   ValidatePointsOfInterest(segmentKey);

   Uint32 mgrMode = (mode == POIFIND_AND ? POIMGR_AND : POIMGR_OR);

   std::vector<pgsPointOfInterest> poi;
   m_HaulingPoiMgr.GetPointsOfInterest(segmentKey, attrib, mgrMode, &poi );
   return poi;
}

std::vector<pgsPointOfInterest> CBridgeAgentImp::GetHaulingDesignPointsOfInterest(const CSegmentKey& segmentKey,Float64 leftOverhang,Float64 rightOverhang,PoiAttributeType attrib,Uint32 mode)
{
   // Generates points of interest for the supplied overhang.
   pgsPoiMgr poiMgr;
   LayoutHandlingPoi(segmentKey,10,leftOverhang,rightOverhang,attrib,POI_BUNKPOINT,POI_HAUL_SEGMENT,&poiMgr);

   Uint32 mgrMode = (mode == POIFIND_AND ? POIMGR_AND : POIMGR_OR);
   std::vector<pgsPointOfInterest> poi;
   poiMgr.GetPointsOfInterest(segmentKey, attrib, mgrMode, &poi );
   return poi;
}

Float64 CBridgeAgentImp::GetMinimumOverhang(const CSegmentKey& segmentKey)
{
   Float64 gdrLength = GetSegmentLength(segmentKey);

   GET_IFACE(IGirderHaulingSpecCriteria,pCriteria);
   Float64 maxDistBetweenSupports = pCriteria->GetAllowableDistanceBetweenSupports();

   Float64 minOverhang = (gdrLength - maxDistBetweenSupports)/2;
   if ( minOverhang < 0 )
      minOverhang = 0;

   return minOverhang;
}

////////////////////////////////////////////////////////////////////////
// IUserDefinedLoads
bool CBridgeAgentImp::DoUserLoadsExist(const CSpanGirderKey& spanGirderKey)
{
   IntervalIndexType nIntervals = m_IntervalManager.GetIntervalCount();
   for ( IntervalIndexType intervalIdx = 0; intervalIdx < nIntervals; intervalIdx++ )
   {
      const std::vector<IUserDefinedLoads::UserPointLoad>* uplv = GetPointLoads(intervalIdx,spanGirderKey);
      if (uplv != NULL && !uplv->empty())
         return true;

      const std::vector<IUserDefinedLoads::UserDistributedLoad>* udlv = GetDistributedLoads(intervalIdx,spanGirderKey);
      if (udlv != NULL && !udlv->empty())
         return true;

      const std::vector<IUserDefinedLoads::UserMomentLoad>* umlv = GetMomentLoads(intervalIdx,spanGirderKey);
      if (umlv != NULL && !umlv->empty())
         return true;
   }

   return false;
}

bool CBridgeAgentImp::DoUserLoadsExist(const CGirderKey& girderKey)
{
   SpanIndexType startSpanIdx = GetGirderGroupStartSpan(girderKey.groupIndex);
   SpanIndexType endSpanIdx   = GetGirderGroupEndSpan(girderKey.groupIndex);

   if (startSpanIdx == endSpanIdx )
   {
      return DoUserLoadsExist(CSpanGirderKey(startSpanIdx,girderKey.girderIndex));
   }
   else
   {
      for (SpanIndexType spanIdx = startSpanIdx; spanIdx <= endSpanIdx; spanIdx++ )
      {
         if ( DoUserLoadsExist(CSpanGirderKey(spanIdx,girderKey.girderIndex)) )
            return true;
      }
   }
   return false;
}

const std::vector<IUserDefinedLoads::UserPointLoad>* CBridgeAgentImp::GetPointLoads(IntervalIndexType intervalIdx,const CSpanGirderKey& spanGirderKey)
{
   std::vector<UserPointLoad>* vec = GetUserPointLoads(intervalIdx,spanGirderKey);
   return vec;
}

const std::vector<IUserDefinedLoads::UserDistributedLoad>* CBridgeAgentImp::GetDistributedLoads(IntervalIndexType intervalIdx,const CSpanGirderKey& spanGirderKey)
{
   std::vector<UserDistributedLoad>* vec = GetUserDistributedLoads(intervalIdx,spanGirderKey);
   return vec;
}

const std::vector<IUserDefinedLoads::UserMomentLoad>* CBridgeAgentImp::GetMomentLoads(IntervalIndexType intervalIdx,const CSpanGirderKey& spanGirderKey)
{
   std::vector<UserMomentLoad>* vec = GetUserMomentLoads(intervalIdx,spanGirderKey);
   return vec;
}

/////////////////////////////////////////////////////
// ITempSupport
void CBridgeAgentImp::GetControlPoints(SupportIndexType tsIdx,IPoint2d** ppLeft,IPoint2d** ppAlignment,IPoint2d** ppBridge,IPoint2d** ppRight)
{
   VALIDATE( BRIDGE );

   CComPtr<IBridgeGeometry> geometry;
   m_Bridge->get_BridgeGeometry(&geometry);

   CComPtr<IPierLine> pier;
   SupportIDType tsID = ::GetTempSupportLineID(tsIdx);
   geometry->FindPierLine( tsID, &pier);
   pier->get_AlignmentPoint(ppAlignment);

   // left and right edge points... intersect CL pier with deck edge
   CComPtr<ILine2d> line;
   pier->get_Centerline(&line);

   // get the left and right segment girder line that touch this temporary support
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();

   const CTemporarySupportData* pTS = pBridgeDesc->GetTemporarySupport(tsIdx);
   Float64 tsStation = pTS->GetStation();

   // need to get the girder group this temporary support belongs with
   const CGirderGroupData* pGroup = pBridgeDesc->GetGirderGroup(pTS->GetSpan());
   GroupIndexType grpIdx = pGroup->GetIndex();

   // use left girder to get the number of segments as every girder in a group has the same number of segments
   GirderIndexType gdrIdx = 0;
   const CSplicedGirderData* pGirder = pBridgeDesc->GetGirderGroup(grpIdx)->GetGirder(gdrIdx);
   SegmentIndexType nSegments = pGirder->GetSegmentCount();
   SegmentIndexType segIdx = INVALID_INDEX;
   for ( segIdx = 0; segIdx < nSegments; segIdx++ )
   {
      const CPrecastSegmentData* pSegment = pGirder->GetSegment(segIdx);
      Float64 startStation,endStation;
      pSegment->GetStations(startStation,endStation);
      if ( ::InRange(startStation,tsStation,endStation) )
         break;
   }
   
   ATLASSERT( segIdx < nSegments ); // if this fails, the temp support wasn't found

   // since we don't have deck edges just yet, use the first and last girderline
   CComPtr<IGirderLine> left_girderline;
   LineIDType segID = ::GetGirderSegmentLineID(grpIdx,gdrIdx,segIdx);
   geometry->FindGirderLine( segID, &left_girderline);
   CComPtr<IPath> left_path;
   left_girderline->get_Path(&left_path);
   left_path->Intersect(line,*ppAlignment,ppLeft);

   GirderIndexType nGirderLines = pBridgeDesc->GetGirderGroup(grpIdx)->GetGirderCount();

   CComPtr<IGirderLine> right_girderline;
   segID = ::GetGirderSegmentLineID(grpIdx,nGirderLines-1,segIdx);
   geometry->FindGirderLine( segID, &right_girderline);
   CComPtr<IPath> right_path;
   right_girderline->get_Path( &right_path);
   right_path->Intersect(line,*ppAlignment,ppRight);
}

void CBridgeAgentImp::GetDirection(SupportIndexType tsIdx,IDirection** ppDirection)
{
   VALIDATE(BRIDGE);

   CComPtr<IBridgeGeometry> geometry;
   m_Bridge->get_BridgeGeometry(&geometry);

   CComPtr<IPierLine> pier;
   SupportIDType tsID = ::GetTempSupportLineID(tsIdx);
   geometry->FindPierLine( tsID, &pier );

   pier->get_Direction(ppDirection);
}

void CBridgeAgentImp::GetSkew(SupportIndexType tsIdx,IAngle** ppAngle)
{
   VALIDATE(BRIDGE);

   CComPtr<IBridgeGeometry> geometry;
   m_Bridge->get_BridgeGeometry(&geometry);

   CComPtr<IPierLine> pier;
   SupportIDType tsID = ::GetTempSupportLineID(tsIdx);
   geometry->FindPierLine(tsID,&pier);

   pier->get_Skew(ppAngle);
}

/////////////////////////////////////////////////////
// IGirderSegment
void CBridgeAgentImp::GetSegmentEndPoints(const CSegmentKey& segmentKey,IPoint2d** ppSupport1,IPoint2d** ppEnd1,IPoint2d** ppBrg1,IPoint2d** ppBrg2,IPoint2d** ppEnd2,IPoint2d** ppSupport2)
{
   CComPtr<IGirderLine> girderLine;
   GetGirderLine(segmentKey,&girderLine);
   girderLine->GetEndPoints(ppSupport1,ppEnd1,ppBrg1,ppBrg2,ppEnd2,ppSupport2);
}

Float64 CBridgeAgentImp::GetSegmentLength(const CSegmentKey& segmentKey)
{
   // returns the end-to-end length of the segment.
   CComPtr<IGirderLine> girderLine;
   GetGirderLine(segmentKey,&girderLine);

   Float64 length;
   girderLine->get_GirderLength(&length);

   return length;
}

Float64 CBridgeAgentImp::GetSegmentSpanLength(const CSegmentKey& segmentKey)
{
   // returns the CL-Brg to CL-Brg span length
   CComPtr<IGirderLine> girderLine;
   GetGirderLine(segmentKey,&girderLine);

   Float64 length;
   girderLine->get_SpanLength(&length);

   return length;
}

Float64 CBridgeAgentImp::GetSegmentLayoutLength(const CSegmentKey& segmentKey)
{
   // Pier to pier length
   CComPtr<IGirderLine> girderLine;
   GetGirderLine(segmentKey,&girderLine);

   Float64 length;
   girderLine->get_LayoutLength(&length);

   return length;
}

Float64 CBridgeAgentImp::GetSegmentPlanLength(const CSegmentKey& segmentKey)
{
   // returns the length of the segment adjusted for slope
   Float64 length = GetSegmentLength(segmentKey);
   Float64 slope  = GetSegmentSlope(segmentKey);

   Float64 plan_length = length*sqrt(1.0 + slope*slope);

   return plan_length;
}

Float64 CBridgeAgentImp::GetSegmentSlope(const CSegmentKey& segmentKey)
{
   std::vector<pgsPointOfInterest> vPoi = GetPointsOfInterest(segmentKey,POI_ERECTED_SEGMENT | POI_0L,POIFIND_AND);
   ATLASSERT(vPoi.size() == 1);
   pgsPointOfInterest poiStart(vPoi.front());

   vPoi = GetPointsOfInterest(segmentKey,POI_ERECTED_SEGMENT | POI_10L,POIFIND_AND);
   ATLASSERT(vPoi.size() == 1);
   pgsPointOfInterest poiEnd(vPoi.front());

   Float64 sta1,offset1;
   Float64 sta2,offset2;
   GetStationAndOffset(poiStart, &sta1,&offset1);
   GetStationAndOffset(poiEnd,   &sta2,&offset2);

   Float64 elev1,elev2;
   elev1 = GetElevation(sta1,offset1);
   elev2 = GetElevation(sta2,offset2);

   Float64 slabOffset1,slabOffset2;
   slabOffset1 = GetSlabOffset(segmentKey,pgsTypes::metStart);
   slabOffset2 = GetSlabOffset(segmentKey,pgsTypes::metEnd);

   Float64 dy = (elev2-slabOffset2) - (elev1-slabOffset1);

   Float64 length = poiEnd.GetDistFromStart() - poiStart.GetDistFromStart();
   Float64 slope = dy/length;

   return slope;
}

void CBridgeAgentImp::GetSegmentProfile(const CSegmentKey& segmentKey,bool bIncludeClosure,IShape** ppShape)
{
   CComPtr<ISegment> segment;
   GetSegment(segmentKey,&segment);

   segment->get_Profile(bIncludeClosure ? VARIANT_TRUE : VARIANT_FALSE,ppShape);
}

void CBridgeAgentImp::GetSegmentProfile(const CSegmentKey& segmentKey,const CSplicedGirderData* pGirder,bool bIncludeClosure,IShape** ppShape)
{
   // X-values for start and end of the segment under consideration
   // measured from the intersection of the CL Pier and CL Girder/Segment at
   // the start of the girder
   Float64 xStart,xEnd;
   GetSegmentRange(segmentKey,&xStart,&xEnd);

   boost::shared_ptr<mathFunction2d> f = CreateGirderProfile(pGirder);
   CComPtr<IPolyShape> polyShape;
   polyShape.CoCreateInstance(CLSID_PolyShape);

   std::vector<Float64> xValues;

   const CPrecastSegmentData* pSegment = pGirder->GetSegment(segmentKey.segmentIndex);

   Float64 segmentLength = GetSegmentLayoutLength(segmentKey);

   // if not including closure pours, adjust the start/end locations to be
   // at the actual start/end of the segment
   if ( !bIncludeClosure )
   {
      Float64 startBrgOffset, endBrgOffset;
      GetSegmentBearingOffset(segmentKey,&startBrgOffset,&endBrgOffset);

      Float64 startEndDist, endEndDist;
      GetSegmentEndDistance(segmentKey,pGirder,&startEndDist,&endEndDist);

      xStart += startBrgOffset - startEndDist;
      xEnd   -= endBrgOffset   - endEndDist;
   }

   Float64 variationLength[4];
   for ( int i = 0; i < 4; i++ )
   {
      variationLength[i] = pSegment->GetVariationLength((pgsTypes::SegmentZoneType)i);
      if ( variationLength[i] < 0 )
      {
         ATLASSERT(-1.0 <= variationLength[i] && variationLength[i] <= 0.0);
         variationLength[i] *= -segmentLength;
      }
   }

   // capture key values in segment
   pgsTypes::SegmentVariationType variationType = pSegment->GetVariationType();
   if ( variationType == pgsTypes::svtLinear )
   {
      xValues.push_back(xStart + variationLength[pgsTypes::sztLeftPrismatic] );
      xValues.push_back(xEnd   - variationLength[pgsTypes::sztRightPrismatic] );
   }
   else if ( variationType == pgsTypes::svtParabolic )
   {
      Float64 xStartParabola = xStart + variationLength[pgsTypes::sztLeftPrismatic];
      Float64 xEndParabola   = xEnd   - variationLength[pgsTypes::sztRightPrismatic];
      xValues.push_back(xStartParabola);
      for ( int i = 0; i < 5; i++ )
      {
         Float64 x = i*(xEndParabola-xStartParabola)/5 + xStartParabola;
         xValues.push_back(x);
      }
      xValues.push_back(xEndParabola);
   }
   else if ( variationType == pgsTypes::svtDoubleLinear )
   {
      xValues.push_back(xStart + variationLength[pgsTypes::sztLeftPrismatic] );
      xValues.push_back(xStart + variationLength[pgsTypes::sztLeftPrismatic]  + variationLength[pgsTypes::sztLeftTapered] );
      xValues.push_back(xEnd   - variationLength[pgsTypes::sztRightPrismatic] - variationLength[pgsTypes::sztRightTapered] );
      xValues.push_back(xEnd   - variationLength[pgsTypes::sztRightPrismatic] );
   }
   else if ( variationType == pgsTypes::svtDoubleParabolic )
   {
      // left parabola
      Float64 xStartParabola = xStart + variationLength[pgsTypes::sztLeftPrismatic];
      Float64 xEndParabola   = xStart + variationLength[pgsTypes::sztLeftPrismatic]  + variationLength[pgsTypes::sztLeftTapered];

      xValues.push_back(xStartParabola);
      for ( int i = 0; i < 5; i++ )
      {
         Float64 x = i*(xEndParabola-xStartParabola)/5 + xStartParabola;
         xValues.push_back(x);
      }
      xValues.push_back(xEndParabola);

      // right parabola
      xStartParabola = xEnd   - variationLength[pgsTypes::sztRightPrismatic] - variationLength[pgsTypes::sztRightTapered];
      xEndParabola   = xEnd   - variationLength[pgsTypes::sztRightPrismatic];
      xValues.push_back(xStartParabola);
      for ( int i = 0; i < 5; i++ )
      {
         Float64 x = i*(xEndParabola-xStartParabola)/5 + xStartParabola;
         xValues.push_back(x);
      }
      xValues.push_back(xEndParabola);
   }

   // fill up with other points
   for ( int i = 0; i < 11; i++ )
   {
      Float64 x = i*(xEnd - xStart)/10 + xStart;
      xValues.push_back(x);
   }

   std::sort(xValues.begin(),xValues.end());

   std::vector<Float64>::iterator iter(xValues.begin());
   std::vector<Float64>::iterator end(xValues.end());
   for ( ; iter != end; iter++ )
   {
      Float64 x = *iter;
      Float64 y = f->Evaluate(x);
      polyShape->AddPoint(x,-y);
   }

   // points across the top of the segment
   polyShape->AddPoint(xEnd,0);
   polyShape->AddPoint(xStart,0);

   polyShape->get_Shape(ppShape);
}

void CBridgeAgentImp::GetSegmentEndDistance(const CSegmentKey& segmentKey,Float64* pStartEndDistance,Float64* pEndEndDistance)
{
   CComPtr<IGirderLine> girderLine;
   GetGirderLine(segmentKey,&girderLine);

   girderLine->get_EndDistance(etStart,pStartEndDistance);
   girderLine->get_EndDistance(etEnd,  pEndEndDistance);
}

void CBridgeAgentImp::GetSegmentEndDistance(const CSegmentKey& segmentKey,const CSplicedGirderData* pGirder,Float64* pStartEndDistance,Float64* pEndEndDistance)
{
   const CPrecastSegmentData* pSegment = pGirder->GetSegment(segmentKey.segmentIndex);
   const CClosurePourData* pLeftClosure  = pSegment->GetLeftClosure();
   const CClosurePourData* pRightClosure = pSegment->GetRightClosure();

   // Assume pGirder is not associated with our bridge, but rather a detached copy that is
   // being used in an editing situation.

   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();

   // Left End
   Float64 leftEndDistance;
   ConnectionLibraryEntry::EndDistanceMeasurementType leftMeasureType;
   CComPtr<IAngle> leftSkewAngle;
   if ( pLeftClosure )
   {
      const CTemporarySupportData* pTS = pLeftClosure->GetTemporarySupport();
      const CPierData2* pPier = pLeftClosure->GetPier();
      ATLASSERT( pTS != NULL || pPier != NULL );

      if ( pTS )
      {
         pTS->GetGirderEndDistance(&leftEndDistance,&leftMeasureType);
         GetSkew(pTS->GetIndex(),&leftSkewAngle);
      }
      else
      {
         pPier->GetGirderEndDistance(pgsTypes::Ahead,&leftEndDistance,&leftMeasureType);
         GetPierSkew(pPier->GetIndex(),&leftSkewAngle);
      }
   }
   else
   {
      const CPierData2* pPier = pBridgeDesc->GetPier(pGirder->GetPierIndex(pgsTypes::metStart));
      pPier->GetGirderEndDistance(pgsTypes::Ahead,&leftEndDistance,&leftMeasureType);
      GetPierSkew(pPier->GetIndex(),&leftSkewAngle);
   }
   
   // Adjust for measurement datum
   if ( leftMeasureType == ConnectionLibraryEntry::FromPierNormalToPier )
   {
      Float64 skew;
      leftSkewAngle->get_Value(&skew);

      Float64 leftBrgOffset, rightBrgOffset;
      GetSegmentBearingOffset(segmentKey,&leftBrgOffset,&rightBrgOffset);

      leftEndDistance = leftBrgOffset - leftEndDistance/cos(fabs(skew));
   }
   else if ( leftMeasureType == ConnectionLibraryEntry::FromPierAlongGirder )
   {
      Float64 leftBrgOffset, rightBrgOffset;
      GetSegmentBearingOffset(segmentKey,&leftBrgOffset,&rightBrgOffset);

      leftEndDistance = leftBrgOffset - leftEndDistance;
   }
   else if ( leftMeasureType == ConnectionLibraryEntry::FromBearingNormalToPier )
   {
      Float64 skew;
      leftSkewAngle->get_Value(&skew);
      leftEndDistance /= cos(fabs(skew));
   }
#if defined _DEBUG
   else
   {
      // end distance is already in the format we want it
      ATLASSERT( leftMeasureType == ConnectionLibraryEntry::FromBearingAlongGirder );
   }
#endif

   Float64 rightEndDistance;
   ConnectionLibraryEntry::EndDistanceMeasurementType rightMeasureType;
   CComPtr<IAngle> rightSkewAngle;
   if ( pRightClosure )
   {
      const CTemporarySupportData* pTS = pRightClosure->GetTemporarySupport();
      const CPierData2* pPier = pRightClosure->GetPier();
      ATLASSERT( pTS != NULL || pPier != NULL );

      if ( pTS )
      {
         pTS->GetGirderEndDistance(&rightEndDistance,&rightMeasureType);
         GetSkew(pTS->GetIndex(),&rightSkewAngle);
      }
      else
      {
         pPier->GetGirderEndDistance(pgsTypes::Back,&rightEndDistance,&rightMeasureType);
         GetPierSkew(pPier->GetIndex(),&rightSkewAngle);
      }
   }
   else
   {
      const CPierData2* pPier = pBridgeDesc->GetPier(pGirder->GetPierIndex(pgsTypes::metEnd));
      pPier->GetGirderEndDistance(pgsTypes::Back,&rightEndDistance,&rightMeasureType);
      GetPierSkew(pPier->GetIndex(),&rightSkewAngle);
   }
   
   // Adjust for measurement datum
   if ( rightMeasureType == ConnectionLibraryEntry::FromPierNormalToPier )
   {
      Float64 skew;
      rightSkewAngle->get_Value(&skew);

      Float64 leftBrgOffset, rightBrgOffset;
      GetSegmentBearingOffset(segmentKey,&leftBrgOffset,&rightBrgOffset);

      rightEndDistance = rightBrgOffset - rightEndDistance/cos(fabs(skew));
   }
   else if ( rightMeasureType == ConnectionLibraryEntry::FromPierAlongGirder )
   {
      Float64 leftBrgOffset, rightBrgOffset;
      GetSegmentBearingOffset(segmentKey,&leftBrgOffset,&rightBrgOffset);

      rightEndDistance = rightBrgOffset - rightEndDistance;
   }
   else if ( rightMeasureType == ConnectionLibraryEntry::FromBearingNormalToPier )
   {
      Float64 skew;
      rightSkewAngle->get_Value(&skew);
      rightEndDistance /= cos(fabs(skew));
   }
#if defined _DEBUG
   else
   {
      // end distance is already in the format we want it
      ATLASSERT( rightMeasureType == ConnectionLibraryEntry::FromBearingAlongGirder );
   }
#endif

   *pStartEndDistance = leftEndDistance;
   *pEndEndDistance   = rightEndDistance;
}

void CBridgeAgentImp::GetSegmentBearingOffset(const CSegmentKey& segmentKey,Float64* pStartBearingOffset,Float64* pEndBearingOffset)
{
   CComPtr<IGirderLine> girderLine;
   GetGirderLine(segmentKey,&girderLine);

   girderLine->get_BearingOffset(etStart,pStartBearingOffset);
   girderLine->get_BearingOffset(etEnd,  pEndBearingOffset);
}

void CBridgeAgentImp::GetSegmentStorageSupportLocations(const CSegmentKey& segmentKey,Float64* pDistFromLeftEnd,Float64* pDistFromRightEnd)
{
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CPrecastSegmentData* pSegment = pIBridgeDesc->GetPrecastSegmentData(segmentKey);
   *pDistFromLeftEnd  = pSegment->HandlingData.LeftStoragePoint;
   *pDistFromRightEnd = pSegment->HandlingData.RightStoragePoint;
}

void CBridgeAgentImp::GetSegmentBottomFlangeProfile(const CSegmentKey& segmentKey,IPoint2dCollection** points)
{
   GroupIndexType grpIdx = segmentKey.groupIndex;
   GirderIndexType gdrIdx = segmentKey.girderIndex;

   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CSplicedGirderData* pGirder = pIBridgeDesc->GetBridgeDescription()->GetGirderGroup(grpIdx)->GetGirder(gdrIdx);
   GetSegmentBottomFlangeProfile(segmentKey,pGirder,points);
}

void CBridgeAgentImp::GetSegmentBottomFlangeProfile(const CSegmentKey& segmentKey,const CSplicedGirderData* pGirder,IPoint2dCollection** ppPoints)
{
   Float64 xStart,xEnd;
   GetSegmentRange(segmentKey,&xStart,&xEnd);

   Float64 segmentLength = GetSegmentLayoutLength(segmentKey);

   boost::shared_ptr<mathFunction2d> girder_depth         = CreateGirderProfile(pGirder);
   boost::shared_ptr<mathFunction2d> bottom_flange_height = CreateGirderBottomFlangeProfile(pGirder);

   CComPtr<IPoint2dCollection> points;
   points.CoCreateInstance(CLSID_Point2dCollection);

   std::vector<Float64> xValues;

   const CPrecastSegmentData* pSegment = pGirder->GetSegment(segmentKey.segmentIndex);

   // Get variation lengths and convert from fractional measure if necessary (neg lengths are fractional)
   Float64 variationLength[4];
   for ( int i = 0; i < 4; i++ )
   {
      variationLength[i] = pSegment->GetVariationLength((pgsTypes::SegmentZoneType)i);
      if ( variationLength[i] < 0 )
      {
         ATLASSERT(-1.0 <= variationLength[i] && variationLength[i] <= 0.0);
         variationLength[i] *= -segmentLength;
      }
   }

   pgsTypes::SegmentVariationType variationType = pSegment->GetVariationType();
   if ( variationType == pgsTypes::svtLinear )
   {
      xValues.push_back(xStart + variationLength[pgsTypes::sztLeftPrismatic] );
      xValues.push_back(xEnd   - variationLength[pgsTypes::sztRightPrismatic] );
   }
   else if ( variationType == pgsTypes::svtParabolic )
   {
      Float64 xStartParabola = xStart + variationLength[pgsTypes::sztLeftPrismatic];
      Float64 xEndParabola   = xEnd   - variationLength[pgsTypes::sztRightPrismatic];
      xValues.push_back(xStartParabola);
      for ( int i = 0; i < 5; i++ )
      {
         Float64 x = i*(xEndParabola-xStartParabola)/5 + xStartParabola;
         xValues.push_back(x);
      }
      xValues.push_back(xEndParabola);
   }
   else if ( variationType == pgsTypes::svtDoubleLinear )
   {
      xValues.push_back(xStart + variationLength[pgsTypes::sztLeftPrismatic] );
      xValues.push_back(xStart + variationLength[pgsTypes::sztLeftPrismatic]  + variationLength[pgsTypes::sztLeftTapered] );
      xValues.push_back(xEnd   - variationLength[pgsTypes::sztRightPrismatic] - variationLength[pgsTypes::sztRightTapered] );
      xValues.push_back(xEnd   - variationLength[pgsTypes::sztRightPrismatic] );
   }
   else if ( variationType == pgsTypes::svtDoubleParabolic )
   {
      // left parabola
      Float64 xStartParabola = xStart + variationLength[pgsTypes::sztLeftPrismatic];
      Float64 xEndParabola   = xStart + variationLength[pgsTypes::sztLeftPrismatic]  + variationLength[pgsTypes::sztLeftTapered];

      xValues.push_back(xStartParabola);
      for ( int i = 0; i < 5; i++ )
      {
         Float64 x = i*(xEndParabola-xStartParabola)/5 + xStartParabola;
         xValues.push_back(x);
      }
      xValues.push_back(xEndParabola);

      // right parabola
      xStartParabola = xEnd   - variationLength[pgsTypes::sztRightPrismatic] - variationLength[pgsTypes::sztRightTapered];
      xEndParabola   = xEnd   - variationLength[pgsTypes::sztRightPrismatic];
      xValues.push_back(xStartParabola);
      for ( int i = 0; i < 5; i++ )
      {
         Float64 x = i*(xEndParabola-xStartParabola)/5 + xStartParabola;
         xValues.push_back(x);
      }
      xValues.push_back(xEndParabola);
   }

   // fill up with other points
   for ( int i = 0; i < 11; i++ )
   {
      Float64 x = i*(xEnd - xStart)/10 + xStart;
      xValues.push_back(x);
   }

   std::sort(xValues.begin(),xValues.end());

   std::vector<Float64>::iterator iter(xValues.begin());
   std::vector<Float64>::iterator end(xValues.end());
   for ( ; iter != end; iter++ )
   {
      Float64 x = *iter;
      Float64 y = -girder_depth->Evaluate(x) + bottom_flange_height->Evaluate(x);
      
      CComPtr<IPoint2d> point;
      point.CoCreateInstance(CLSID_Point2d);
      point->Move(x,y);
      points->Add(point);
   }

   points.CopyTo(ppPoints);
}

boost::shared_ptr<mathFunction2d> CBridgeAgentImp::CreateGirderProfile(const CSplicedGirderData* pGirder)
{
   return CreateGirderProfile(pGirder,true);
}

boost::shared_ptr<mathFunction2d> CBridgeAgentImp::CreateGirderBottomFlangeProfile(const CSplicedGirderData* pGirder)
{
   return CreateGirderProfile(pGirder,false);
}

boost::shared_ptr<mathFunction2d> CBridgeAgentImp::CreateGirderProfile(const CSplicedGirderData* pGirder,bool bGirderProfile)
{
#pragma Reminder("UPDATE: use the methods from WBFL::GenericBridge") // remove this implementation and use the code in WBFL Generic Bridge
   // Generic Bridge does not have "What if" versions... that is, what if the girder looked like pGirder
   boost::shared_ptr<mathCompositeFunction2d> pCompositeFunction( new mathCompositeFunction2d() );

   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();

   GroupIndexType grpIdx = pGirder->GetGirderGroupIndex();
   GirderIndexType gdrIdx = pGirder->GetIndex();
   CSegmentKey segmentKey(grpIdx,gdrIdx,INVALID_INDEX);

   Float64 xSegmentStart = 0; // start of segment
   Float64 xStart        = 0; // start of curve section
   Float64 xEnd          = 0; // end of curve section

   bool bParabola = false;
   Float64 xParabolaStart,xParabolaEnd;
   Float64 yParabolaStart,yParabolaEnd;
   Float64 slopeParabola;

   // go down each segment and create piece-wise functions for each part of the spliced girder profile
   SegmentIndexType nSegments = pGirder->GetSegmentCount();
   for ( SegmentIndexType segIdx = 0; segIdx < nSegments; segIdx++ )
   {
      segmentKey.segmentIndex = segIdx;

      xStart = xSegmentStart;

      const CPrecastSegmentData* pSegment = pGirder->GetSegment(segIdx);

      pgsTypes::SegmentVariationType  variation_type = pSegment->GetVariationType();

      Float64 segment_length = GetSegmentLayoutLength(segmentKey);

      Float64 h1,h2,h3,h4;
      if ( bGirderProfile )
      {
         // we are creating a girder profile
         h1 = pSegment->GetVariationHeight(pgsTypes::sztLeftPrismatic);
         h2 = pSegment->GetVariationHeight(pgsTypes::sztRightPrismatic);
         h3 = pSegment->GetVariationHeight(pgsTypes::sztLeftTapered);
         h4 = pSegment->GetVariationHeight(pgsTypes::sztRightTapered);
      }
      else
      {
         // we are creating a bottom flange profile
         h1 = pSegment->GetVariationBottomFlangeDepth(pgsTypes::sztLeftPrismatic);
         h2 = pSegment->GetVariationBottomFlangeDepth(pgsTypes::sztRightPrismatic);
         h3 = pSegment->GetVariationBottomFlangeDepth(pgsTypes::sztLeftTapered);
         h4 = pSegment->GetVariationBottomFlangeDepth(pgsTypes::sztRightTapered);
      }

      Float64 left_prismatic_length  = pSegment->GetVariationLength(pgsTypes::sztLeftPrismatic);
      Float64 left_taper_length      = pSegment->GetVariationLength(pgsTypes::sztLeftTapered);
      Float64 right_taper_length     = pSegment->GetVariationLength(pgsTypes::sztRightTapered);
      Float64 right_prismatic_length = pSegment->GetVariationLength(pgsTypes::sztRightPrismatic);

      // deal with fractional measure (fraction measures are < 0)
      if ( left_prismatic_length < 0 )
      {
         ATLASSERT(-1.0 <= left_prismatic_length && left_prismatic_length <= 0.0);
         left_prismatic_length *= -segment_length;
      }

      if ( left_taper_length < 0 )
      {
         ATLASSERT(-1.0 <= left_taper_length && left_taper_length <= 0.0);
         left_taper_length *= -segment_length;
      }

      if ( right_taper_length < 0 )
      {
         ATLASSERT(-1.0 <= right_taper_length && right_taper_length <= 0.0);
         right_taper_length *= -segment_length;
      }

      if ( right_prismatic_length < 0 )
      {
         ATLASSERT(-1.0 <= right_prismatic_length && right_prismatic_length <= 0.0);
         right_prismatic_length *= -segment_length;
      }

         // the girder profile has been modified by the user input


      if ( 0 < left_prismatic_length )
      {
         // create a prismatic segment
         mathLinFunc2d func(0.0, h1);
         xEnd = xStart + left_prismatic_length;
         pCompositeFunction->AddFunction(xStart,xEnd,func);
         slopeParabola = 0;
         xStart = xEnd;
      }

      if ( variation_type == pgsTypes::svtLinear )
      {
         // create a linear taper segment
         Float64 taper_length = segment_length - left_prismatic_length - right_prismatic_length;
         Float64 slope = (h2 - h1)/taper_length;
         Float64 b = h1 - slope*xStart;

         mathLinFunc2d func(slope, b);
         xEnd = xStart + taper_length;
         pCompositeFunction->AddFunction(xStart,xEnd,func);
         xStart = xEnd;
         slopeParabola = slope;
      }
      else if ( variation_type == pgsTypes::svtDoubleLinear )
      {
         // create a linear taper for left side of segment
         Float64 slope = (h3 - h1)/left_taper_length;
         Float64 b = h1 - slope*xStart;

         mathLinFunc2d left_func(slope, b);
         xEnd = xStart + left_taper_length;
         pCompositeFunction->AddFunction(xStart,xEnd,left_func);
         xStart = xEnd;

         // create a linear segment between left and right tapers
         Float64 taper_length = segment_length - left_prismatic_length - left_taper_length - right_prismatic_length - right_taper_length;
         slope = (h4 - h3)/taper_length;
         b = h3 - slope*xStart;

         mathLinFunc2d middle_func(slope, b);
         xEnd = xStart + taper_length;
         pCompositeFunction->AddFunction(xStart,xEnd,middle_func);
         xStart = xEnd;

         // create a linear taper for right side of segment
         slope = (h2 - h4)/right_taper_length;
         b = h4 - slope*xStart;

         mathLinFunc2d right_func(slope, b);
         xEnd = xStart + right_taper_length;
         pCompositeFunction->AddFunction(xStart,xEnd,right_func);
         xStart = xEnd;
         slopeParabola = slope;
      }
      else if ( variation_type == pgsTypes::svtParabolic )
      {
         if ( !bParabola )
         {
            // this is the start of a parabolic segment
            bParabola = true;
            xParabolaStart = xStart;
            yParabolaStart = h1;
         }

         // Parabola ends in this segment if
         // 1) this segment has a prismatic segment on the right end -OR-
         // 2) this is the last segment -OR-
         // 3) the next segment starts with a prismatic segment -OR-
         // 4) the next segment has a linear transition

         const CPrecastSegmentData* pNextSegment = (segIdx == nSegments-1 ? NULL : pGirder->GetSegment(segIdx+1));

         Float64 next_segment_left_prismatic_length = pNextSegment->GetVariationLength(pgsTypes::sztLeftPrismatic);
         if ( next_segment_left_prismatic_length < 0 )
         {
            ATLASSERT(-1.0 <= next_segment_left_prismatic_length && next_segment_left_prismatic_length < 0.0);
            next_segment_left_prismatic_length *= -GetSegmentLayoutLength(pNextSegment->GetSegmentKey());
         }

         if (
              0 < right_prismatic_length || // parabola ends in this segment -OR-
              segIdx == nSegments-1      || // this is the last segment (parabola ends here) -OR-
              0 < next_segment_left_prismatic_length || // next segment starts with prismatic section -OR-
              (pNextSegment->GetVariationType() == pgsTypes::svtNone || pNextSegment->GetVariationType() == pgsTypes::svtLinear || pNextSegment->GetVariationType() == pgsTypes::svtDoubleLinear) // next segment is linear 
            )
         {
            // parabola ends in this segment
            Float64 xParabolaEnd = xStart + segment_length - right_prismatic_length;
            Float64 yParabolaEnd = h2;

            if ( yParabolaEnd < yParabolaStart )
            {
               // slope at end is zero
               mathPolynomial2d func = GenerateParabola2(xParabolaStart,yParabolaStart,xParabolaEnd,yParabolaEnd,0.0);
               pCompositeFunction->AddFunction(xParabolaStart,xParabolaEnd,func);
            }
            else
            {
               // slope at start is zero
               mathPolynomial2d func = GenerateParabola1(xParabolaStart,yParabolaStart,xParabolaEnd,yParabolaEnd,slopeParabola);
               pCompositeFunction->AddFunction(xParabolaStart,xParabolaEnd,func);
            }

            bParabola = false;
            xStart = xParabolaEnd;
         }
         else
         {
            // parabola ends further down the girderline
            // do nothing???
         }
      }
      else if ( variation_type == pgsTypes::svtDoubleParabolic )
      {
         // left parabola ends in this segment
         if ( !bParabola )
         {
            // not currently in a parabola, based the start point on this segment
            xParabolaStart = xSegmentStart + left_prismatic_length;
            yParabolaStart = h1;
         }

#pragma Reminder("BUG: Assuming slope at start is zero, but it may not be if tangent to a linear segment")
         xParabolaEnd = xSegmentStart + left_prismatic_length + left_taper_length;
         yParabolaEnd = h3;
         mathPolynomial2d func_left_parabola;
         if ( yParabolaEnd < yParabolaStart )
            func_left_parabola = GenerateParabola2(xParabolaStart,yParabolaStart,xParabolaEnd,yParabolaEnd,0.0);
         else
            func_left_parabola = GenerateParabola1(xParabolaStart,yParabolaStart,xParabolaEnd,yParabolaEnd,slopeParabola);

         pCompositeFunction->AddFunction(xParabolaStart,xParabolaEnd,func_left_parabola);

         // parabola on right side of this segment starts here
         xParabolaStart = xSegmentStart + segment_length - right_prismatic_length - right_taper_length;
         yParabolaStart = h4;
         bParabola = true;

         if ( !IsZero(xParabolaStart - xParabolaEnd) )
         {
            // create a line segment between parabolas
            Float64 taper_length = segment_length - left_prismatic_length - left_taper_length - right_prismatic_length - right_taper_length;
            Float64 slope = -(h4 - h3)/taper_length;
            Float64 b = h3 - slope*xParabolaEnd;

            mathLinFunc2d middle_func(slope, b);
            pCompositeFunction->AddFunction(xParabolaEnd,xParabolaStart,middle_func);
            slopeParabola = slope;
         }

         // parabola ends in this segment if
         // 1) this is the last segment
         // 2) right prismatic section length > 0
         // 3) next segment is not parabolic
         const CPrecastSegmentData* pNextSegment = (segIdx == nSegments-1 ? 0 : pGirder->GetSegment(segIdx+1));
         if ( 0 < right_prismatic_length || 
              segIdx == nSegments-1      || 
              (pNextSegment->GetVariationType() == pgsTypes::svtNone || pNextSegment->GetVariationType() == pgsTypes::svtLinear || pNextSegment->GetVariationType() == pgsTypes::svtDoubleLinear) // next segment is linear 
            )
         {
            bParabola = false;
            xParabolaEnd = xSegmentStart + segment_length - right_prismatic_length;
            yParabolaEnd = h2;

     
            mathPolynomial2d func_right_parabola;
            if ( yParabolaEnd < yParabolaStart )
            {
               // compute slope at end of parabola
               if ( pNextSegment )
               {
                  CSegmentKey nextSegmentKey(segmentKey);
                  nextSegmentKey.segmentIndex++;

                  Float64 next_segment_length = GetSegmentLayoutLength(nextSegmentKey);
                  Float64 next_segment_left_prismatic_length = pNextSegment->GetVariationLength(pgsTypes::sztLeftPrismatic);
                  if ( next_segment_left_prismatic_length < 0 )
                  {
                     ATLASSERT(-1.0 <= next_segment_left_prismatic_length && next_segment_left_prismatic_length < 0.0);
                     next_segment_left_prismatic_length *= -next_segment_length;
                  }

                  Float64 next_segment_right_prismatic_length = pNextSegment->GetVariationLength(pgsTypes::sztRightPrismatic);
                  if ( next_segment_right_prismatic_length < 0 )
                  {
                     ATLASSERT(-1.0 <= next_segment_right_prismatic_length && next_segment_right_prismatic_length < 0.0);
                     next_segment_right_prismatic_length *= -next_segment_length;
                  }

                  Float64 next_segment_left_tapered_length = pNextSegment->GetVariationLength(pgsTypes::sztLeftTapered);
                  if ( next_segment_left_tapered_length < 0 )
                  {
                     ATLASSERT(-1.0 <= next_segment_left_tapered_length && next_segment_left_tapered_length < 0.0);
                     next_segment_left_tapered_length *= -next_segment_length;
                  }

                  if ( pNextSegment->GetVariationType() == pgsTypes::svtLinear )
                  {
                     // next segment is linear
                     if ( IsZero(next_segment_left_prismatic_length) )
                     {
                        Float64 dist = next_segment_length - next_segment_left_prismatic_length - next_segment_right_prismatic_length;
                        slopeParabola = -(pNextSegment->GetVariationHeight(pgsTypes::sztRightPrismatic) - pNextSegment->GetVariationHeight(pgsTypes::sztLeftPrismatic))/dist;
                     }
                  }
                  else if ( pNextSegment->GetVariationType() == pgsTypes::svtDoubleLinear )
                  {
                     if ( IsZero(next_segment_left_prismatic_length) )
                     {
                        Float64 dist = next_segment_left_tapered_length;
                        slopeParabola = -(pNextSegment->GetVariationHeight(pgsTypes::sztLeftTapered) - pNextSegment->GetVariationHeight(pgsTypes::sztLeftPrismatic))/dist;
                     }
                  }
               }
               else
               {
                  slopeParabola = 0;
               }
               func_right_parabola = GenerateParabola2(xParabolaStart,yParabolaStart,xParabolaEnd,yParabolaEnd,slopeParabola);
            }
            else
            {
               func_right_parabola = GenerateParabola1(xParabolaStart,yParabolaStart,xParabolaEnd,yParabolaEnd,slopeParabola);
            }

            pCompositeFunction->AddFunction(xParabolaStart,xParabolaEnd,func_right_parabola);
         }
         else
         {
            // parabola ends further down the girderline
            bParabola = true;
         }

         xStart = xSegmentStart + segment_length - right_prismatic_length;
      }

      if ( 0 < right_prismatic_length )
      {
         // create a prismatic segment
         mathLinFunc2d func(0.0, h2);
         xEnd = xStart + right_prismatic_length;
         pCompositeFunction->AddFunction(xStart,xEnd,func);
         slopeParabola = 0;
         xStart = xEnd;
      }

      xSegmentStart += segment_length;
   }

   return pCompositeFunction;
}

void CBridgeAgentImp::GetSegmentRange(const CSegmentKey& segmentKey,Float64* pXStart,Float64* pXEnd)
{
   Float64 x = 0;
   
   for ( SegmentIndexType s = 0; s < segmentKey.segmentIndex; s++ )
   {
      CSegmentKey key(segmentKey.groupIndex,segmentKey.girderIndex,s);
      x += GetSegmentLayoutLength(key);
   }

   *pXStart = x;
   *pXEnd   = x + GetSegmentLayoutLength(segmentKey);
}

void CBridgeAgentImp::GetSegmentDirection(const CSegmentKey& segmentKey,IDirection** ppDirection)
{
   VALIDATE(BRIDGE);

   CComPtr<IGirderLine> girderLine;
   GetGirderLine(segmentKey,&girderLine);

   girderLine->get_Direction(ppDirection);
}

Float64 CBridgeAgentImp::GetTopWidth(const CSegmentKey& segmentKey)
{
   const GirderLibraryEntry* pLibEntry = GetGirderLibraryEntry(segmentKey);

   CComPtr<IBeamFactory> factory;
   pLibEntry->GetBeamFactory(&factory);

   CComPtr<IGirderSection> section;
   factory->CreateGirderSection(m_pBroker,m_StatusGroupID,pLibEntry->GetDimensions(),-1,-1,&section);

   Float64 top_width;
   section->get_TopWidth(&top_width);

   return top_width;
}

///////////////////////////////////////////////////////////////////////////////////////////
// IClosurePour
void CBridgeAgentImp::GetClosurePourProfile(const CClosureKey& closureKey,IShape** ppShape)
{
   GroupIndexType      grpIdx     = closureKey.groupIndex;
   GirderIndexType     gdrIdx     = closureKey.girderIndex;
   CollectionIndexType closureIdx = closureKey.segmentIndex;

   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CGirderGroupData* pGroup = pBridgeDesc->GetGirderGroup(grpIdx);
   const CSplicedGirderData* pGirder = pGroup->GetGirder(gdrIdx);
   const CClosurePourData* pClosurePour = pGirder->GetClosurePour(closureIdx);
   
   const CPrecastSegmentData* pLeftSegment = pClosurePour->GetLeftSegment();
   SegmentIndexType leftSegIdx = pLeftSegment->GetIndex();
   
   const CPrecastSegmentData* pRightSegment = pClosurePour->GetRightSegment();
   SegmentIndexType rightSegIdx = pRightSegment->GetIndex();

   CSegmentKey leftSegmentKey(grpIdx,gdrIdx,leftSegIdx);
   CSegmentKey rightSegmentKey(grpIdx,gdrIdx,rightSegIdx);

   Float64 leftSegStartOffset, leftSegEndOffset;
   GetSegmentEndDistance(leftSegmentKey,&leftSegStartOffset,&leftSegEndOffset);

   Float64 rightSegStartOffset, rightSegEndOffset;
   GetSegmentEndDistance(rightSegmentKey,pGirder,&rightSegStartOffset,&rightSegEndOffset);

   Float64 leftSegStartBrgOffset, leftSegEndBrgOffset;
   GetSegmentBearingOffset(leftSegmentKey,&leftSegStartBrgOffset,&leftSegEndBrgOffset);

   Float64 rightSegStartBrgOffset, rightSegEndBrgOffset;
   GetSegmentBearingOffset(rightSegmentKey,&rightSegStartBrgOffset,&rightSegEndBrgOffset);

   Float64 xLeftStart,xLeftEnd;
   GetSegmentRange(leftSegmentKey,&xLeftStart,&xLeftEnd);

   Float64 xRightStart,xRightEnd;
   GetSegmentRange(rightSegmentKey,&xRightStart,&xRightEnd);

   Float64 xStart = xLeftEnd;
   Float64 xEnd = xRightStart;

   xStart -= (leftSegEndBrgOffset - leftSegEndOffset);
   xEnd   += (rightSegStartBrgOffset - rightSegStartOffset);

   boost::shared_ptr<mathFunction2d> f = CreateGirderProfile(pGirder);
   CComPtr<IPolyShape> polyShape;
   polyShape.CoCreateInstance(CLSID_PolyShape);

   std::vector<Float64> xValues;

   // fill up with other points
   for ( int i = 0; i < 5; i++ )
   {
      Float64 x = i*(xEnd - xStart)/4 + xStart;
      xValues.push_back(x);
   }

   std::sort(xValues.begin(),xValues.end());

   std::vector<Float64>::iterator iter(xValues.begin());
   std::vector<Float64>::iterator end(xValues.end());
   for ( ; iter != end; iter++ )
   {
      Float64 x = *iter;
      Float64 y = f->Evaluate(x);
      polyShape->AddPoint(x,-y);
   }

   // points across the top of the segment
   polyShape->AddPoint(xEnd,0);
   polyShape->AddPoint(xStart,0);

   polyShape->get_Shape(ppShape);
}

void CBridgeAgentImp::GetClosurePourSize(const CClosureKey& closureKey,Float64* pLeft,Float64* pRight)
{
   GroupIndexType      grpIdx     = closureKey.groupIndex;
   GirderIndexType     gdrIdx     = closureKey.girderIndex;
   CollectionIndexType closureIdx = closureKey.segmentIndex;

   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CGirderGroupData* pGroup = pBridgeDesc->GetGirderGroup(grpIdx);
   const CSplicedGirderData* pGirder = pGroup->GetGirder(gdrIdx);
   
   if ( pGirder->GetClosurePourCount() <= closureIdx )
   {
      ATLASSERT(false); // shouldn't get here
      *pLeft = 0;
      *pRight = 0;
      return;
   }

   const CClosurePourData* pClosurePour = pGirder->GetClosurePour(closureIdx);
   
   const CPrecastSegmentData* pLeftSegment = pClosurePour->GetLeftSegment();
   SegmentIndexType leftSegIdx = pLeftSegment->GetIndex();
   
   const CPrecastSegmentData* pRightSegment = pClosurePour->GetRightSegment();
   SegmentIndexType rightSegIdx = pRightSegment->GetIndex();

   CSegmentKey leftSegmentKey(grpIdx,gdrIdx,leftSegIdx);
   CSegmentKey rightSegmentKey(grpIdx,gdrIdx,rightSegIdx);

   Float64 leftSegStartEndDist, leftSegEndEndDist;
   GetSegmentEndDistance(leftSegmentKey,&leftSegStartEndDist,&leftSegEndEndDist);

   Float64 rightSegStartEndDist, rightSegEndEndDist;
   GetSegmentEndDistance(rightSegmentKey,&rightSegStartEndDist,&rightSegEndEndDist);

   Float64 leftSegStartBrgOffset, leftSegEndBrgOffset;
   GetSegmentBearingOffset(leftSegmentKey,&leftSegStartBrgOffset,&leftSegEndBrgOffset);

   Float64 rightSegStartBrgOffset, rightSegEndBrgOffset;
   GetSegmentBearingOffset(rightSegmentKey,&rightSegStartBrgOffset,&rightSegEndBrgOffset);

   *pLeft  = leftSegEndBrgOffset    - leftSegEndEndDist;
   *pRight = rightSegStartBrgOffset - rightSegStartEndDist;
}

Float64 CBridgeAgentImp::GetClosurePourLength(const CClosureKey& closureKey)
{
   Float64 left, right;
   GetClosurePourSize(closureKey,&left,&right);
   return left+right;
}

///////////////////////////////////////////////////////////////////////////////////////////
// ISplicedGirder
Float64 CBridgeAgentImp::GetSplicedGirderLayoutLength(const CGirderKey& girderKey)
{
   Float64 length = 0;
   CollectionIndexType nSegments = GetSegmentCount(girderKey);
   for ( SegmentIndexType segIdx = 0; segIdx < nSegments; segIdx++ )
   {
      CSegmentKey segmentKey(girderKey.groupIndex,girderKey.girderIndex,segIdx);

      // segment layout length goes from CL pier to CL pier (or temporary support)
      // so there is no need to include the closure pour lengths
      length += GetSegmentLayoutLength(segmentKey);
   }

   return length;
}

Float64 CBridgeAgentImp::GetSplicedGirderSpanLength(const CGirderKey& girderKey)
{
   Float64 layout_length = GetSplicedGirderLayoutLength(girderKey);

   // layout length goes from between the back of pavement seats at the end piers
   // need to deduct for connection geometry at end piers

   Float64 startBrgOffset, endBrgOffset;
   CSegmentKey startSegmentKey(girderKey.groupIndex,girderKey.girderIndex,0);
   GetSegmentBearingOffset(startSegmentKey,&startBrgOffset,&endBrgOffset);
   layout_length -= startBrgOffset;

   SegmentIndexType nSegments = GetSegmentCount(girderKey);
   CSegmentKey endSegmentKey(girderKey.groupIndex,girderKey.girderIndex,nSegments-1);
   GetSegmentBearingOffset(endSegmentKey,&startBrgOffset,&endBrgOffset);
   layout_length -= endBrgOffset;

   return layout_length;
}

Float64 CBridgeAgentImp::GetSplicedGirderLength(const CGirderKey& girderKey)
{
   Float64 layout_length = GetSplicedGirderLayoutLength(girderKey);

   // layout length goes from between the back of pavement seats at the end piers
   // need to deduct for connection geometry at end piers

   CSegmentKey startSegmentKey(girderKey.groupIndex,girderKey.girderIndex,0);
   Float64 startBrgOffset, endBrgOffset;
   GetSegmentBearingOffset(startSegmentKey,&startBrgOffset,&endBrgOffset);
   layout_length -= startBrgOffset;

   Float64 startEndDist, endEndDist;
   GetSegmentEndDistance(startSegmentKey,&startEndDist,&endEndDist);
   layout_length += startEndDist;

   SegmentIndexType nSegments = GetSegmentCount(girderKey);
   CSegmentKey endSegmentKey(girderKey.groupIndex,girderKey.girderIndex,nSegments-1);

   GetSegmentBearingOffset(endSegmentKey,&startBrgOffset,&endBrgOffset);
   layout_length -= endBrgOffset;

   GetSegmentEndDistance(endSegmentKey,&startEndDist,&endEndDist);
   layout_length += endEndDist;

   return layout_length;
}

///////////////////////////////////////////////////////////////////////////////////////////
// ITendonGeometry
void CBridgeAgentImp::GetDuctCenterline(const CGirderKey& girderKey,DuctIndexType ductIdx,IPoint2dCollection** ppPoints)
{
   // Gets the duct centerline from the generic bridge model
   CComPtr<IPoint2dCollection> points;
   points.CoCreateInstance(CLSID_Point2dCollection);

   CComPtr<ISuperstructureMember> ssMbr;
   GetSuperstructureMember(girderKey,&ssMbr);

   CComQIPtr<IItemData> itemData(ssMbr);
   CComPtr<IUnknown> unk;
   itemData->GetItemData(CComBSTR(_T("Tendons")),&unk);

   CComQIPtr<ITendonCollection> tendons(unk);

   CComPtr<ITendon> tendon;
   tendons->get_Item(ductIdx,&tendon);

   CComPtr<IPoint3d> pntStart, pntEnd;
   tendon->get_Start(&pntStart);
   tendon->get_End(&pntEnd);

   Float64 z1, z2;
   pntStart->get_Z(&z1);
   pntEnd->get_Z(&z2);

   IndexType nPoints = 20*GetSpanCount();
   for ( IndexType i = 0; i < nPoints; i++ )
   {
      Float64 z = i*(z2-z1)/(nPoints-1);

      CComPtr<IPoint3d> pnt;
      tendon->get_CG(z,tmPath,&pnt);

      Float64 y;
      pnt->get_Y(&y);

      CComPtr<IPoint2d> p;
      p.CoCreateInstance(CLSID_Point2d);
      p->Move(z,y);
      
      points->Add(p);
   }

   points.CopyTo(ppPoints);
}

DuctIndexType CBridgeAgentImp::GetDuctCount(const CGirderKey& girderKey)
{
   CComPtr<ISuperstructureMember> ssMbr;
   GetSuperstructureMember(girderKey,&ssMbr);

   CComQIPtr<IItemData> itemData(ssMbr);

   CComPtr<IUnknown> unk;
   itemData->GetItemData(CComBSTR(_T("Tendons")),&unk);

   CComQIPtr<ITendonCollection> tendons(unk);

   DuctIndexType nDucts;
   tendons->get_Count(&nDucts);
   return nDucts;
}

void CBridgeAgentImp::GetDuctCenterline(const CGirderKey& girderKey,DuctIndexType ductIdx,const CSplicedGirderData* pGirder,IPoint2dCollection** ppPoints)
{
   // Get the duct centerline based on the spliced girder input data
   const CDuctData* pDuctData = pGirder->GetPostTensioning()->GetDuct(ductIdx);

   switch ( pDuctData->DuctGeometryType )
   {
   case CDuctGeometry::Linear:
      CreateDuctCenterline(girderKey,pDuctData->LinearDuctGeometry,ppPoints);
      break;

   case CDuctGeometry::Parabolic:
      CreateDuctCenterline(girderKey,pDuctData->ParabolicDuctGeometry,ppPoints);
      break;

   case CDuctGeometry::Offset:
      GetDuctCenterline(girderKey,pDuctData->OffsetDuctGeometry.RefDuctIdx,pGirder,ppPoints);
      CreateDuctCenterline(girderKey,pDuctData->OffsetDuctGeometry,ppPoints);
      break;
   }
}

mathPolynomial2d CBridgeAgentImp::GenerateParabola1(Float64 x1,Float64 y1,Float64 x2,Float64 y2,Float64 slope)
{
   // slope is known at left end
   Float64 A = ((y2-y1) - (x2-x1)*slope)/((x2-x1)*(x2-x1));
   Float64 B = slope - 2*A*x1;
   Float64 C = y1 - A*x1*x1 - B*x1;

   std::vector<Float64> coefficients;
   coefficients.push_back(A);
   coefficients.push_back(B);
   coefficients.push_back(C);

   return mathPolynomial2d(coefficients);
}

mathPolynomial2d CBridgeAgentImp::GenerateParabola2(Float64 x1,Float64 y1,Float64 x2,Float64 y2,Float64 slope)
{
   // slope is known at right end
   Float64 A = -((y2-y1) - (x2-x1)*slope)/((x2-x1)*(x2-x1));
   Float64 B = slope - 2*A*x2;
   Float64 C = y1 - A*x1*x1 - B*x1;

   std::vector<Float64> coefficients;
   coefficients.push_back(A);
   coefficients.push_back(B);
   coefficients.push_back(C);

   return mathPolynomial2d(coefficients);
}

void CBridgeAgentImp::GenerateReverseParabolas(Float64 x1,Float64 y1,Float64 x2,Float64 x3,Float64 y3,mathPolynomial2d* pLeftParabola,mathPolynomial2d* pRightParabola)
{
   Float64 y2 = (y3*(x2-x1) + y1*(x3-x2))/(x3 - x1);

   *pLeftParabola  = GenerateParabola1(x1,y1,x2,y2,0.0);
   *pRightParabola = GenerateParabola2(x2,y2,x3,y3,0.0);
}

mathCompositeFunction2d CBridgeAgentImp::CreateDuctCenterline(const CGirderKey& girderKey,const CLinearDuctGeometry& geometry)
{
   mathCompositeFunction2d fnCenterline;

   Float64 x1 = 0;
   Float64 y1 = 0;
   CollectionIndexType nPoints = geometry.GetPointCount();
   for ( CollectionIndexType idx = 1; idx < nPoints; idx++ )
   {
      Float64 distFromPrev;
      Float64 offset;
      CDuctGeometry::OffsetType offsetType;
      geometry.GetPoint(idx-1,&distFromPrev,&offset,&offsetType);

      x1 += distFromPrev;
      y1 = ConvertDuctOffsetToDuctElevation(girderKey,x1,offset,offsetType);

      geometry.GetPoint(idx,&distFromPrev,&offset,&offsetType);

      Float64 x2,y2;
      x2 = x1 + distFromPrev;
      y2 = ConvertDuctOffsetToDuctElevation(girderKey,x2,offset,offsetType);

      Float64 m = (y2-y1)/(x2-x1);
      Float64 b = y2 - m*x2;

      mathLinFunc2d fn(m,b);

      fnCenterline.AddFunction(x1,x2,fn);

      x1 = x2;
      y1 = y2;
   }

   return fnCenterline;
}

void CBridgeAgentImp::CreateDuctCenterline(const CGirderKey& girderKey,const CLinearDuctGeometry& geometry,IPoint2dCollection** ppPoints)
{
   CComPtr<IPoint2dCollection> points;
   points.CoCreateInstance(CLSID_Point2dCollection);
   points.CopyTo(ppPoints);

   Float64 x = 0;
   CollectionIndexType nPoints = geometry.GetPointCount();
   for ( CollectionIndexType idx = 0; idx < nPoints; idx++ )
   {
      Float64 distFromPrev;
      Float64 offset;
      CDuctGeometry::OffsetType offsetType;
      geometry.GetPoint(idx,&distFromPrev,&offset,&offsetType);

      x += distFromPrev;

      Float64 y = ConvertDuctOffsetToDuctElevation(girderKey,x,offset,offsetType);

      CComPtr<IPoint2d> point;
      point.CoCreateInstance(CLSID_Point2d);
      point->Move(x,y);
      points->Add(point);
   }
}

void CBridgeAgentImp::GetDuctPoint(const pgsPointOfInterest& poi,DuctIndexType ductIdx,IPoint2d** ppPoint)
{
   Float64 Xg = ConvertPoiToGirderPathCoordinate(poi);

   const CGirderKey& girderKey(poi.GetSegmentKey());

   // Xg is in girder path coordinates.... need to get the distance from the start of the girder
   Float64 start_offset = GetSegmentStartBearingOffset(CSegmentKey(girderKey,0)) - GetSegmentStartEndDistance(CSegmentKey(girderKey,0));
   Float64 dist_from_start_of_girder = Xg - start_offset;
   ATLASSERT(0 <= dist_from_start_of_girder);

   CComPtr<ISuperstructureMember> ssMbr;
   GetSuperstructureMember(girderKey,&ssMbr);

   CComQIPtr<IItemData> itemData(ssMbr);

   CComPtr<IUnknown> unk;
   itemData->GetItemData(CComBSTR(_T("Tendons")),&unk);

   CComQIPtr<ITendonCollection> tendons(unk);
   CComPtr<ITendon> tendon;
   tendons->get_Item(ductIdx,&tendon);

   CComPtr<IPoint3d> cg;
   tendon->get_CG(dist_from_start_of_girder,tmPath,&cg);

   Float64 x,y,z;
   cg->Location(&x,&y,&z);
   ATLASSERT(IsEqual(z,dist_from_start_of_girder));

   CComPtr<IPoint2d> pnt;
   pnt.CoCreateInstance(CLSID_Point2d);
   pnt->Move(x,y);

   Float64 Hg = GetHeight(poi);

   pnt->Offset(0,Hg);

   pnt.CopyTo(ppPoint);
}

void CBridgeAgentImp::GetDuctPoint(const CGirderKey& girderKey,Float64 Xg,DuctIndexType ductIdx,IPoint2d** ppPoint)
{
   pgsPointOfInterest poi = ConvertGirderPathCoordinateToPoi(girderKey,Xg);
   GetDuctPoint(poi,ductIdx,ppPoint);
}

StrandIndexType CBridgeAgentImp::GetStrandCount(const CGirderKey& girderKey,DuctIndexType ductIdx)
{
   CComPtr<ISuperstructureMember> ssMbr;
   GetSuperstructureMember(girderKey,&ssMbr);

   CComQIPtr<IItemData> itemData(ssMbr);

   CComPtr<IUnknown> unk;
   itemData->GetItemData(CComBSTR(_T("Tendons")),&unk);

   CComQIPtr<ITendonCollection> tendons(unk);
   CComPtr<ITendon> tendon;
   tendons->get_Item(ductIdx,&tendon);

   StrandIndexType nStrands;
   tendon->get_StrandCount(&nStrands);

   return nStrands;
}

Float64 CBridgeAgentImp::GetTendonArea(const CGirderKey& girderKey,IntervalIndexType intervalIdx,DuctIndexType ductIdx)
{
   if ( intervalIdx < GetStressTendonInterval(girderKey,ductIdx) )
      return 0;

   CComPtr<ISuperstructureMember> ssMbr;
   GetSuperstructureMember(girderKey,&ssMbr);

   CComQIPtr<IItemData> itemData(ssMbr);

   CComPtr<IUnknown> unk;
   itemData->GetItemData(CComBSTR(_T("Tendons")),&unk);

   CComQIPtr<ITendonCollection> tendons(unk);
   CComPtr<ITendon> tendon;
   tendons->get_Item(ductIdx,&tendon);

   Float64 Apt;
   tendon->get_TendonArea(&Apt);

   return Apt;
}

Float64 CBridgeAgentImp::GetPjack(const CGirderKey& girderKey,DuctIndexType ductIdx)
{
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CPTData* pPTData = pIBridgeDesc->GetPostTensioning(girderKey);
   WebIndexType nWebs = GetWebCount(girderKey);
   return pPTData->GetPjack(ductIdx/nWebs);
}

Float64 CBridgeAgentImp::GetDuctOffset(IntervalIndexType intervalIdx,const pgsPointOfInterest& poi,DuctIndexType ductIdx)
{
   // Returns the distance from the top of the non-composite girder to the CG of the tendon

   // if interval is before tendon is installed, there isn't an eccentricity with respect to the girder cross section
   if ( intervalIdx < GetStressTendonInterval(poi.GetSegmentKey(),ductIdx) )
      return 0;

   CComPtr<ISuperstructureMember> ssMbr;
   GetSuperstructureMember(poi.GetSegmentKey(),&ssMbr);

   CComQIPtr<IItemData> itemData(ssMbr);

   CComPtr<IUnknown> unk;
   itemData->GetItemData(CComBSTR(_T("Tendons")),&unk);

   CComQIPtr<ITendonCollection> tendons(unk);
   CComPtr<ITendon> tendon;
   tendons->get_Item(ductIdx,&tendon);

   StrandIndexType nStrands;
   tendon->get_StrandCount(&nStrands);
   if ( nStrands == 0 )
      return 0;

   Float64 Xg = ConvertPoiToGirderCoordinate(poi);
   CComPtr<IPoint3d> cg;
   tendon->get_CG(Xg,tmTendon /*account for tendon offset from center of duct*/,&cg);

   Float64 ecc;
   cg->get_Y(&ecc);

   return ecc;
}

Float64 CBridgeAgentImp::GetEccentricity(IntervalIndexType intervalIdx,const pgsPointOfInterest& poi,DuctIndexType ductIdx)
{
   ATLASSERT(ductIdx != INVALID_INDEX);

   // if interval is before tendon is installed, there isn't an eccentricity with respect to the girder cross section
   IntervalIndexType stressTendonIntervalIdx = GetStressTendonInterval(poi.GetSegmentKey(),ductIdx);
   if ( intervalIdx < stressTendonIntervalIdx )
      return 0;

   Float64 duct_offset = GetDuctOffset(intervalIdx,poi,ductIdx); // distance from top of non-composite girder to CG of tendon
   // negative value indicates the duct is below the top of the girder

   if ( GetCompositeDeckInterval() <= stressTendonIntervalIdx )
   {
      // need to adjust for the height of the deck
      Float64 ts = GetStructuralSlabDepth(poi);
      duct_offset -= ts; // distance from top of deck.. negative indicating below top of deck
   }

   // Get Ytop
   Float64 Ytop = GetYt(intervalIdx,poi); // distance from CG to top of girder

   Float64 ecc = -(duct_offset + Ytop);

   // NOTE: ecc is positive if it is below the CG
   return ecc;
}

Float64 CBridgeAgentImp::GetAngularChange(const pgsPointOfInterest& poi,DuctIndexType ductIdx,pgsTypes::MemberEndType endType)
{
   // get the poi at the start of this girder
   CSegmentKey segmentKey(poi.GetSegmentKey());

   pgsPointOfInterest poi1, poi2;
   if ( endType == pgsTypes::metStart )
   {
      // angular changes are measured from the start of the girder (i.e. jacking at the left end)
      segmentKey.segmentIndex = 0; // always occurs in segment 0
      poi1 = GetPointOfInterest(segmentKey,0.0); // poi at start of first segment
      poi2 = poi;
   }
   else
   {
      // angular changes are measured from the end of the girder (i.e. jacking at the right end)
      poi2 = poi;
      SegmentIndexType nSegments = GetSegmentCount(segmentKey);
      segmentKey.segmentIndex = nSegments-1;
      Float64 L = GetSegmentLength(segmentKey);
      poi1 = GetPointOfInterest(segmentKey,L); // poi at end of last segment
   }

   return GetAngularChange(poi1,poi2,ductIdx);
}

Float64 CBridgeAgentImp::GetAngularChange(const pgsPointOfInterest& poi1,const pgsPointOfInterest& poi2,DuctIndexType ductIdx)
{
   // POIs must be on the same girder
   ATLASSERT(CGirderKey(poi1.GetSegmentKey()) == CGirderKey(poi2.GetSegmentKey()));

   // Get the tendon
   CComPtr<ISuperstructureMember> ssMbr;
   GetSuperstructureMember(poi1.GetSegmentKey(),&ssMbr);

   CComQIPtr<IItemData> itemData(ssMbr);

   CComPtr<IUnknown> unk;
   itemData->GetItemData(CComBSTR(_T("Tendons")),&unk);

   CComQIPtr<ITendonCollection> tendons(unk);
   CComPtr<ITendon> tendon;
   tendons->get_Item(ductIdx,&tendon);

   Float64 alpha = 0.0; // angular change

   // Get all the POI for this girder
   ATLASSERT(CGirderKey(poi1.GetSegmentKey()) == CGirderKey(poi2.GetSegmentKey()));
   GroupIndexType  grpIdx = poi1.GetSegmentKey().groupIndex;
   GirderIndexType gdrIdx = poi1.GetSegmentKey().girderIndex;

   std::vector<pgsPointOfInterest> vPoi( GetPointsOfInterest(CSegmentKey(grpIdx,gdrIdx,ALL_SEGMENTS)) );
   // vPoi is sorted left to right... if we are measuring angular change from right to left, order of the
   // elements in the vector have to be reversed
   bool bLeftToRight = true;
   if ( poi2 < poi1 )
   {
      bLeftToRight = false;
      std::reverse(vPoi.begin(),vPoi.end());
   }
   std::vector<pgsPointOfInterest>::iterator iter(vPoi.begin());
   pgsPointOfInterest prevPoi = *iter++;

   std::vector<pgsPointOfInterest>::iterator end(vPoi.end());
   for ( ; iter != end; iter++ )
   {
      pgsPointOfInterest& poi = *iter;
      if ( bLeftToRight )
      {
         if ( prevPoi < poi1 )
            continue; // before the start poi... continue with next poi
         
         if ( poi2 < poi ) // after end poi... just break out of the loop
            break;
      }
      else
      {
         if ( poi1 < prevPoi )
            continue; // prev poi is before the first poi (going right to left)

         if ( poi < poi2 )
            break; // current poi is after the second poi (going right to left)
      }

      Float64 Xg1 = ConvertPoiToGirderCoordinate(prevPoi);
      Float64 Xg2 = ConvertPoiToGirderCoordinate(poi);
         
      CComPtr<IVector3d> slope1, slope2;
      tendon->get_Slope(Xg1,tmTendon,&slope1);
      tendon->get_Slope(Xg2,tmTendon,&slope2);

      Float64 delta_alpha;
      slope2->AngleBetween(slope1,&delta_alpha);

      alpha += delta_alpha;

      prevPoi = poi;
   }

   return alpha;
}

//////////////////////////////////////////////////////////////////////////
// IIntervals
IntervalIndexType CBridgeAgentImp::GetIntervalCount()
{
   VALIDATE(BRIDGE);
   return m_IntervalManager.GetIntervalCount();
}

EventIndexType CBridgeAgentImp::GetStartEvent(IntervalIndexType idx)
{
   VALIDATE(BRIDGE);
   return m_IntervalManager.GetStartEvent(idx);
}

EventIndexType CBridgeAgentImp::GetEndEvent(IntervalIndexType idx)
{
   VALIDATE(BRIDGE);
   return m_IntervalManager.GetEndEvent(idx);
}

Float64 CBridgeAgentImp::GetStart(IntervalIndexType idx)
{
   VALIDATE(BRIDGE);
   return m_IntervalManager.GetStart(idx);
}

Float64 CBridgeAgentImp::GetMiddle(IntervalIndexType idx)
{
   VALIDATE(BRIDGE);
   return m_IntervalManager.GetMiddle(idx);
}

Float64 CBridgeAgentImp::GetEnd(IntervalIndexType idx)
{
   VALIDATE(BRIDGE);
   return m_IntervalManager.GetEnd(idx);
}

Float64 CBridgeAgentImp::GetDuration(IntervalIndexType idx)
{
   VALIDATE(BRIDGE);
   return m_IntervalManager.GetDuration(idx);
}

LPCTSTR CBridgeAgentImp::GetDescription(IntervalIndexType idx)
{
   VALIDATE(BRIDGE);
   return m_IntervalManager.GetDescription(idx);
}

IntervalIndexType CBridgeAgentImp::GetInterval(EventIndexType eventIdx)
{
   VALIDATE(BRIDGE);
   return m_IntervalManager.GetInterval(eventIdx);
}

IntervalIndexType CBridgeAgentImp::GetCastClosurePourInterval(const CClosureKey& closureKey)
{
   VALIDATE(BRIDGE);
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   EventIndexType castClosurePourEventIdx = pIBridgeDesc->GetCastClosurePourEventIndex(closureKey.groupIndex,closureKey.segmentIndex);
   return m_IntervalManager.GetInterval(castClosurePourEventIdx);
}

IntervalIndexType CBridgeAgentImp::GetCompositeClosurePourInterval(const CClosureKey& closureKey)
{
   // closure pours are composite with the girder in the interval after they were cast
   return GetCastClosurePourInterval(closureKey)+2;
}

IntervalIndexType CBridgeAgentImp::GetCastDeckInterval()
{
   VALIDATE(BRIDGE);
   return m_IntervalManager.GetCastDeckInterval();
}

IntervalIndexType CBridgeAgentImp::GetCompositeDeckInterval()
{
   VALIDATE(BRIDGE);
   return m_IntervalManager.GetCompositeDeckInterval();
}

IntervalIndexType CBridgeAgentImp::GetStressStrandInterval(const CSegmentKey& segmentKey)
{
   VALIDATE(BRIDGE);
   return m_IntervalManager.GetStressStrandInterval();
}

IntervalIndexType CBridgeAgentImp::GetPrestressReleaseInterval(const CSegmentKey& segmentKey)
{
   VALIDATE(BRIDGE);
   return m_IntervalManager.GetPrestressReleaseInterval();
}

IntervalIndexType CBridgeAgentImp::GetLiftSegmentInterval(const CSegmentKey& segmentKey)
{
   VALIDATE(BRIDGE);
   return m_IntervalManager.GetLiftingInterval();
}

IntervalIndexType CBridgeAgentImp::GetStorageInterval(const CSegmentKey& segmentKey)
{
   VALIDATE(BRIDGE);
   return m_IntervalManager.GetStorageInterval();
}

IntervalIndexType CBridgeAgentImp::GetHaulSegmentInterval(const CSegmentKey& segmentKey)
{
   VALIDATE(BRIDGE);
   return m_IntervalManager.GetHaulingInterval();
}

IntervalIndexType CBridgeAgentImp::GetFirstErectedSegmentInterval()
{
   VALIDATE(BRIDGE);
   return m_IntervalManager.GetFirstErectedSegmentInterval();
}

IntervalIndexType CBridgeAgentImp::GetErectSegmentInterval(const CSegmentKey& segmentKey)
{
   VALIDATE(BRIDGE);
   return m_IntervalManager.GetFirstErectedSegmentInterval();
}

IntervalIndexType CBridgeAgentImp::GetTemporaryStrandInstallationInterval(const CSegmentKey& segmentKey)
{
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CPrecastSegmentData* pSegment = pIBridgeDesc->GetPrecastSegmentData(segmentKey);

   IntervalIndexType intervalIdx;
   switch( pSegment->Strands.TempStrandUsage )
   {
   case pgsTypes::ttsPretensioned:
      intervalIdx = GetPrestressReleaseInterval(segmentKey);
      break;

   case pgsTypes::ttsPTBeforeLifting:
      intervalIdx = GetLiftSegmentInterval(segmentKey);
      break;
      
   case pgsTypes::ttsPTAfterLifting:
   case pgsTypes::ttsPTBeforeShipping:
      intervalIdx = GetHaulSegmentInterval(segmentKey);
      break;
   }

   return intervalIdx;
}

IntervalIndexType CBridgeAgentImp::GetTemporaryStrandRemovalInterval(const CSegmentKey& segmentKey)
{
   VALIDATE(BRIDGE);
   return m_IntervalManager.GetTemporaryStrandRemovalInterval();
}

IntervalIndexType CBridgeAgentImp::GetLiveLoadInterval()
{
   VALIDATE(BRIDGE);
   return m_IntervalManager.GetLiveLoadInterval();
}

IntervalIndexType CBridgeAgentImp::GetOverlayInterval()
{
   VALIDATE(BRIDGE);
   return m_IntervalManager.GetOverlayInterval();
}

IntervalIndexType CBridgeAgentImp::GetRailingSystemInterval()
{
   VALIDATE(BRIDGE);
   return m_IntervalManager.GetRailingSystemInterval();
}

IntervalIndexType CBridgeAgentImp::GetStressTendonInterval(const CGirderKey& girderKey,DuctIndexType ductIdx)
{
   VALIDATE(BRIDGE);

   // Convert the actual duct index to the index value for the raw input.
   // There is one duct per web. If there is multiple webs, such as with U-Beams,
   // ducts 0 and 1, 2 and 3, 4 and 5, etc. are stressed together. The input for
   // the pairs of ducts is stored as index = 0 for duct 0 and 1, index = 1 for duct 2 and 3, etc.
   WebIndexType nWebs = GetWebCount(girderKey);
   DuctIndexType index = ductIdx/nWebs;

   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   EventIndexType ptEventIdx = pIBridgeDesc->GetStressTendonEventIndex(girderKey,index);
   return m_IntervalManager.GetInterval(ptEventIdx);
}

IntervalIndexType CBridgeAgentImp::GetTemporarySupportRemovalInterval(SupportIDType tsID)
{
   VALIDATE(BRIDGE);
   return m_IntervalManager.GetTemporarySupportRemovalInterval(tsID);
}

std::vector<IntervalIndexType> CBridgeAgentImp::GetSpecCheckIntervals(const CGirderKey& girderKey)
{
   std::vector<IntervalIndexType> vIntervals;

#pragma Reminder("UPDATE: should spec check any interval where a load is added")
#pragma Reminder("UPDATE: spec check temporary strand removal intervals if there are temporary strands modeled")
   vIntervals.push_back(GetPrestressReleaseInterval(CSegmentKey(girderKey,0)));
   //vIntervals.push_back(GetErectSegmentInterval(CSegmentKey(girderKey,0)));
   //vIntervals.push_back(GetLiftSegmentInterval(CSegmentKey(girderKey,0)));
   //vIntervals.push_back(GetHaulSegmentInterval(CSegmentKey(girderKey,0)));
   vIntervals.push_back(GetTemporaryStrandRemovalInterval(CSegmentKey(girderKey,0)));
   vIntervals.push_back(GetCastDeckInterval());
   vIntervals.push_back(GetRailingSystemInterval());

   DuctIndexType nDucts = GetDuctCount(girderKey);
   for ( DuctIndexType ductIdx = 0; ductIdx < nDucts; ductIdx++ )
   {
      vIntervals.push_back(GetStressTendonInterval(girderKey,ductIdx));
   }

   vIntervals.push_back(GetIntervalCount()-1); // last interval

   // sort and remove any duplicates
   std::sort(vIntervals.begin(),vIntervals.end());
   vIntervals.erase( std::unique(vIntervals.begin(),vIntervals.end()), vIntervals.end() );

   return vIntervals;
}


////////////////////////////////////
Float64 CBridgeAgentImp::ConvertDuctOffsetToDuctElevation(const CGirderKey& girderKey,Float64 Xg,Float64 offset,CDuctGeometry::OffsetType offsetType)
{
   // Returns the elevation of the duct relative to the top of the girder
   if ( offsetType == CDuctGeometry::TopGirder )
   {
      return -offset;
   }
   else
   {
      ATLASSERT(offsetType == pgsTypes::BottomGirder);
      pgsPointOfInterest poi = ConvertGirderCoordinateToPoi(girderKey,Xg);
      Float64 Hg = GetHeight(poi);
      return offset - Hg;
   }
}

mathCompositeFunction2d CBridgeAgentImp::CreateDuctCenterline(const CGirderKey& girderKey,const CParabolicDuctGeometry& geometry)
{
   mathCompositeFunction2d fnCenterline;

   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CGirderGroupData* pGroup = pBridgeDesc->GetGirderGroup(girderKey.groupIndex);
   PierIndexType startPierIdx = pGroup->GetPierIndex(pgsTypes::metStart);
   PierIndexType endPierIdx   = pGroup->GetPierIndex(pgsTypes::metEnd);
   SpanIndexType startSpanIdx = (SpanIndexType)startPierIdx;

   Float64 x1 = 0; // x is measured from the start face of the girder

   //
   // Start to first low point
   //
   SpanIndexType nSpans = geometry.GetSpanCount();

   // start point
   Float64 dist,offset;
   CDuctGeometry::OffsetType offsetType;
   geometry.GetStartPoint(&dist,&offset,&offsetType);

   x1 += dist;
   Float64 y1 = ConvertDuctOffsetToDuctElevation(girderKey,x1,offset,offsetType);

   // low point
   geometry.GetLowPoint(startSpanIdx,&dist,&offset,&offsetType);
   if ( dist < 0 ) // fraction of the span length
      dist *= -GetSpanLength(startSpanIdx,girderKey.girderIndex);

   Float64 x2 = x1 + dist;
   Float64 y2 = ConvertDuctOffsetToDuctElevation(girderKey,x2,offset,offsetType);

   mathPolynomial2d leftParabola = GenerateParabola2(x1,y1,x2,y2,0.0);
   fnCenterline.AddFunction(x1,x2,leftParabola);

   x1 = x2; // start next group of parabolas at the low point
   y1 = y2;

   //
   // Low Point to High Point to Low Point
   //
   Float64 x3,y3;
   mathPolynomial2d rightParabola;
   Float64 startStation = GetPierStation(startPierIdx);
   Float64 Ls = 0;
   for ( PierIndexType pierIdx = startPierIdx+1; pierIdx < endPierIdx; pierIdx++ )
   {
      SpanIndexType prevSpanIdx = SpanIndexType(pierIdx-1);
      SpanIndexType nextSpanIdx = prevSpanIdx+1;

      Float64 distLeftIP, highOffset, distRightIP;
      CDuctGeometry::OffsetType highOffsetType;
      geometry.GetHighPoint(pierIdx,&distLeftIP,&highOffset,&highOffsetType,&distRightIP);

      // low to high point
      Ls += GetSpanLength(prevSpanIdx,girderKey.girderIndex);
      x3 = Ls;
      y3 = ConvertDuctOffsetToDuctElevation(girderKey,x3,highOffset,highOffsetType);

      if ( distLeftIP < 0 ) // fraction of distance between low and high point
         distLeftIP *= -(x3-x1);

      x2 = x3 - distLeftIP; // inflection point measured from high point

      GenerateReverseParabolas(x1,y1,x2,x3,y3,&leftParabola,&rightParabola);
      fnCenterline.AddFunction(x1,x2,leftParabola);
      fnCenterline.AddFunction(x2,x3,rightParabola);

      // high to low point
      geometry.GetLowPoint(pierIdx,&dist,&offset,&offsetType);
      x1 = x3; 
      y1 = y3;

      if ( dist < 0 ) // fraction of span length
         dist *= -GetSpanLength(nextSpanIdx,girderKey.girderIndex);

      x3 = x1 + dist; // low point, measured from previous high point
      y3 = ConvertDuctOffsetToDuctElevation(girderKey,x3,offset,offsetType);

      if ( distRightIP < 0 ) // fraction of distance between high and low point
         distRightIP *= -(x3 - x1);

      x2 = x1 + distRightIP; // inflection point measured from high point

      GenerateReverseParabolas(x1,y1,x2,x3,y3,&leftParabola,&rightParabola);
      fnCenterline.AddFunction(x1,x2,leftParabola);
      fnCenterline.AddFunction(x2,x3,rightParabola);

      x1 = x3;
      y1 = y3;
   }

   //
   // last low point to end
   //
   geometry.GetEndPoint(&dist,&offset,&offsetType);
   if ( dist < 0 ) // fraction of last span length
      dist *= -GetSpanLength(startSpanIdx + nSpans-1,girderKey.girderIndex);

   x2 = pGroup->GetLength() - dist; // dist is measured from the end of the bridge
   y2 = ConvertDuctOffsetToDuctElevation(girderKey,x1,offset,offsetType);
   rightParabola = GenerateParabola1(x1,y1,x2,y2,0.0);
   fnCenterline.AddFunction(x1,x2,rightParabola);

   return fnCenterline;
}

void CBridgeAgentImp::CreateDuctCenterline(const CGirderKey& girderKey,const CParabolicDuctGeometry& geometry,IPoint2dCollection** ppPoints)
{
   mathCompositeFunction2d fnCenterline = CreateDuctCenterline(girderKey,geometry);
   IndexType nFunctions = fnCenterline.GetFunctionCount();

   CComPtr<IPoint2dCollection> points;
   points.CoCreateInstance(CLSID_Point2dCollection);
   points.CopyTo(ppPoints);

   for ( IndexType fnIdx = 0; fnIdx < nFunctions; fnIdx++ )
   {
      const mathFunction2d* pFN;
      Float64 xMin,xMax;
      fnCenterline.GetFunction(fnIdx,&pFN,&xMin,&xMax);
      for ( int i = 0; i < 11; i++ )
      {
         Float64 x = xMin + i*(xMax-xMin)/10;
         Float64 y = fnCenterline.Evaluate(x);

         CComPtr<IPoint2d> p;
         p.CoCreateInstance(CLSID_Point2d);
         p->Move(x,y);
         points->Add(p);
      }
   }
}

void CBridgeAgentImp::CreateDuctCenterline(const CGirderKey& girderKey,const COffsetDuctGeometry& geometry,IPoint2dCollection** ppPoints)
{
   ATLASSERT(*ppPoints != NULL); // should contain the points from the duct that this duct offsets from

   CollectionIndexType nPoints;
   (*ppPoints)->get_Count(&nPoints);
   for ( CollectionIndexType idx = 0; idx < nPoints; idx++ )
   {
      CComPtr<IPoint2d> point;
      (*ppPoints)->get_Item(idx,&point);

      Float64 x,y;
      point->Location(&x,&y);
      Float64 offset = geometry.GetOffset(x);
      y += offset;

      point->put_Y(y);
   }
}

//////////////////////////
std::vector<IUserDefinedLoads::UserPointLoad>* CBridgeAgentImp::GetUserPointLoads(IntervalIndexType intervalIdx,const CSpanGirderKey& spanGirderKey)
{
   ValidateUserLoads();

   PointLoadCollection::iterator found( m_PointLoads[intervalIdx].find(spanGirderKey) );

   if (found != m_PointLoads[intervalIdx].end())
   {
      return &(found->second);
   }

   return 0;
}

std::vector<IUserDefinedLoads::UserDistributedLoad>* CBridgeAgentImp::GetUserDistributedLoads(IntervalIndexType intervalIdx,const CSpanGirderKey& spanGirderKey)
{
   ValidateUserLoads();

   DistributedLoadCollection::iterator found( m_DistributedLoads[intervalIdx].find(spanGirderKey) );

   if (found != m_DistributedLoads[intervalIdx].end())
   {
      return &(found->second);
   }

   return 0;
}


std::vector<IUserDefinedLoads::UserMomentLoad>* CBridgeAgentImp::GetUserMomentLoads(IntervalIndexType intervalIdx,const CSpanGirderKey& spanGirderKey)
{
   ValidateUserLoads();

   MomentLoadCollection::iterator found( m_MomentLoads[intervalIdx].find(spanGirderKey) );

   if (found != m_MomentLoads[intervalIdx].end())
   {
      return &(found->second);
   }

   return 0;
}

////////////////////////////////////////////////////////////////////////
// IBridgeDescriptionEventSink
//
HRESULT CBridgeAgentImp::OnBridgeChanged(CBridgeChangedHint* pHint)
{
//   LOG(_T("OnBridgeChanged Event Received"));
   INVALIDATE( CLEAR_ALL );
   return S_OK;
}

HRESULT CBridgeAgentImp::OnGirderFamilyChanged()
{
//   LOG(_T("OnGirderFamilyChanged Event Received"));
   INVALIDATE( CLEAR_ALL );
   return S_OK;
}

HRESULT CBridgeAgentImp::OnGirderChanged(const CGirderKey& girderKey,Uint32 lHint)
{
   INVALIDATE( CLEAR_ALL );
   return S_OK;
}

HRESULT CBridgeAgentImp::OnLiveLoadChanged()
{
   // No changes necessary to bridge model
   LOG(_T("OnLiveLoadChanged Event Received"));
   return S_OK;
}

HRESULT CBridgeAgentImp::OnLiveLoadNameChanged(LPCTSTR strOldName,LPCTSTR strNewName)
{
   // No changes necessary to bridge model
   LOG(_T("OnLiveLoadNameChanged Event Received"));
   return S_OK;
}

HRESULT CBridgeAgentImp::OnConstructionLoadChanged()
{
   LOG(_T("OnConstructionLoadChanged Event Received"));
   return S_OK;
}

////////////////////////////////////////////////////////////////////////
// ISpecificationEventSink
HRESULT CBridgeAgentImp::OnSpecificationChanged()
{
//   LOG(_T("OnSpecificationChanged Event Received"));
   INVALIDATE( CLEAR_ALL );
   return S_OK;
}

HRESULT CBridgeAgentImp::OnAnalysisTypeChanged()
{
   // nothing to do here!
   return S_OK;
}

void CBridgeAgentImp::GetAlignment(IAlignment** ppAlignment)
{
   VALIDATE( COGO_MODEL );

   CComPtr<IPathCollection> alignments;
   m_CogoModel->get_Alignments(&alignments);

   CComPtr<IPath> path;
   alignments->get_Item(999,&path);
   path->QueryInterface(ppAlignment);
}

void CBridgeAgentImp::GetProfile(IProfile** ppProfile)
{
   CComPtr<IAlignment> alignment;
   GetAlignment(&alignment);

   alignment->get_Profile(ppProfile);
}

void CBridgeAgentImp::GetBarrierProperties(pgsTypes::TrafficBarrierOrientation orientation,IShapeProperties** props)
{
   CComPtr<ISidewalkBarrier> barrier;
   if ( orientation == pgsTypes::tboLeft )
      m_Bridge->get_LeftBarrier(&barrier);
   else
      m_Bridge->get_RightBarrier(&barrier);

   if ( barrier == NULL )
   {
      *props = 0;
      return;
   }

   CComPtr<IShape> shape;
   barrier->get_Shape(&shape);

   shape->get_ShapeProperties(props);
}

CBridgeAgentImp::SectProp CBridgeAgentImp::GetSectionProperties(IntervalIndexType intervalIdx,const pgsPointOfInterest& poi,int sectPropType)
{
   VALIDATE(BRIDGE);
   ATLASSERT(sectPropType == SPT_GROSS || sectPropType == SPT_TRANSFORMED || sectPropType == SPT_NET_GIRDER || sectPropType == SPT_NET_DECK);

   const CSegmentKey& segmentKey = poi.GetSegmentKey();

   USES_CONVERSION;

   // Find properties and return... if not found, compute them now
   PoiIntervalKey poiKey(intervalIdx,poi);
   SectPropContainer::iterator found( m_SectProps[sectPropType].find(poiKey) );
   if ( found != m_SectProps[sectPropType].end() )
   {
      return (*found).second;
   }

   //
   //   ... not found ... compute, store, return stored value
   //
   SectProp props; 

   IntervalIndexType releaseIntervalIdx = GetPrestressReleaseInterval(segmentKey);
   IntervalIndexType compositeDeckIntervalIdx = GetCompositeDeckInterval();

   // if the POI is at a closure pour and the closure pour isn't composite with the girder yet, then
   // the POI is at a non-structural section so the section properties are zero
   bool bIsSection = true;
   if ( (poi.HasAttribute(POI_CLOSURE) && intervalIdx < GetCompositeClosurePourInterval(segmentKey)) )
      bIsSection = false; // at a non-structural section

   // if net deck properties are requested and the interval is before the deck is made composite
   // then the deck is not yet structural and its section properties are taken to be zero
   bool bIsDeckComposite = true;
   if ( sectPropType == SPT_NET_DECK && intervalIdx < compositeDeckIntervalIdx )
      bIsDeckComposite = false;

   // if the girder concrete has been cured yet, then the properties are taken to be zero
   bool bIsGirderConcreteCured = true;
   if ( intervalIdx < releaseIntervalIdx )
      bIsGirderConcreteCured = false;

   // if we can compute properties do it... otherwise the properties are taken to be zero
   if ( bIsGirderConcreteCured && bIsDeckComposite && bIsSection )
   {
      Float64 distFromStartOfSegment = ConvertPoiToSegmentCoordinate(poi); // needs to be in the generic bridge segment coordinate system

      GET_IFACE(ILibrary,       pLib);
      GET_IFACE(ISpecification, pSpec);
      const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( pSpec->GetSpecification().c_str() );

      if ( pSpecEntry->GetEffectiveFlangeWidthMethod() == pgsTypes::efwmTribWidth )
      {
         CComQIPtr<IPGSuperEffectiveFlangeWidthTool> eff_tool(m_EffFlangeWidthTool);
         ATLASSERT(eff_tool);
         eff_tool->put_UseTributaryWidth(VARIANT_TRUE);
      }

      GirderIDType leftGdrID,gdrID,rightGdrID;
      GetAdjacentSuperstructureMemberIDs(segmentKey,&leftGdrID,&gdrID,&rightGdrID);
      if ( sectPropType == SPT_GROSS || sectPropType == SPT_TRANSFORMED || sectPropType == SPT_NET_GIRDER )
      {
         // use tool to create section
         CComPtr<ISection> section;
         HRESULT hr = m_SectCutTool->CreateGirderSectionBySegment(m_Bridge,gdrID,segmentKey.segmentIndex,distFromStartOfSegment,leftGdrID,rightGdrID,intervalIdx,(SectionPropertyMethod)sectPropType,&section);
         ATLASSERT(SUCCEEDED(hr));

         // get mass properties
         CComPtr<IMassProperties> massprops;
         section->get_MassProperties(&massprops);

         // get elastic properties of section
         CComPtr<IElasticProperties> eprop;
         section->get_ElasticProperties(&eprop);

         // transform section properties
         // this transformed all materials into equivalent girder concrete
         // we always do this because the deck must be transformed, even for net properties
         CComPtr<IShapeProperties> shapeprops;
         Float64 Egdr = (poi.HasAttribute(POI_CLOSURE) ? GetClosurePourAgeAdjustedEc(segmentKey,intervalIdx) : GetSegmentAgeAdjustedEc(segmentKey,intervalIdx)); // this method returns the correct E even for gross and tranformed properties without time-step losses
         eprop->TransformProperties(Egdr,&shapeprops);

         props.Section      = section;
         props.ElasticProps = eprop;
         props.ShapeProps   = shapeprops;
         props.MassProps    = massprops;

         Float64 Ag;
         shapeprops->get_Area(&Ag);

         LOG(_T("Interval = ") << intervalIdx << _T(" Group = ") << LABEL_SPAN(segmentKey.groupIndex) << _T(" Girder = ") << LABEL_GIRDER(segmentKey.girderIndex) << _T(" Segment = ") << LABEL_SEGMENT(segmentKey.segmentIndex) << _T(" x = ") << ::ConvertFromSysUnits(poi.GetDistFromStart(),unitMeasure::Feet) << _T(" ft") << _T(" Ag = ") << ::ConvertFromSysUnits(Ag,unitMeasure::Inch2) << _T(" in2") << _T(" Eg = ") << ::ConvertFromSysUnits(Egdr,unitMeasure::KSI) << _T(" KSI"));

         // Assuming section is a Composite section and beam is exactly the first piece
         CComPtr<ISegment> segment;
         GetSegment(segmentKey,&segment);
         CComPtr<IShape> shape;
         segment->get_PrimaryShape(distFromStartOfSegment,&shape);

         //
         shape->get_Perimeter(&props.Perimeter);

         Float64 Yb;
         shapeprops->get_Ybottom(&Yb);

         // Q slab (from procedure in WBFL Sections library documentation)
         CComQIPtr<IXYPosition> position(shape);
         CComPtr<IPoint2d> top_center;
         position->get_LocatorPoint(lpTopCenter,&top_center);

         // Ytop Girder
         CComPtr<IShapeProperties> beamprops;
         shape->get_ShapeProperties(&beamprops);
         Float64 Ytbeam, Ybbeam;
         beamprops->get_Ytop(&Ytbeam);
         beamprops->get_Ybottom(&Ybbeam);
         Float64 hg = Ytbeam + Ybbeam;
         props.YtopGirder = hg - Yb;

         // Create clipping line through beam/slab interface
         CComPtr<ILine2d> line;
         line.CoCreateInstance(CLSID_Line2d);
         CComPtr<IPoint2d> p;
         CComPtr<IVector2d> v;
         line->GetExplicit(&p,&v);
         Float64 x,y;
         top_center->Location(&x,&y);
         p->Move(x,y);
         v->put_Direction(M_PI); // direct line to the left so the beam is clipped out
         v->put_Magnitude(1.0);
         line->SetExplicit(p,v);

         CComPtr<ISection> slabSection;
         section->ClipWithLine(line,&slabSection);

         CComPtr<IElasticProperties> epSlab;
         slabSection->get_ElasticProperties(&epSlab);

         Float64 EAslab;
         epSlab->get_EA(&EAslab);
         Float64 Aslab = EAslab / Egdr; // Transformed to equivalent girder material

         CComPtr<IPoint2d> cg_section;
         shapeprops->get_Centroid(&cg_section);

         CComPtr<IPoint2d> cg_slab;
         epSlab->get_Centroid(&cg_slab);

         Float64 Yg, Ys;
         cg_slab->get_Y(&Ys);
         cg_section->get_Y(&Yg);

         Float64 Qslab = Aslab*(Ys - Yg);
         props.Qslab = Qslab;

         // Area on bottom half of composite section for LRFD Fig 5.8.3.4.2-3
         Float64 h = GetHalfElevation(poi);

         CComPtr<IPoint2d> bottom_center;
         position->get_LocatorPoint(lpBottomCenter,&bottom_center);
         bottom_center->get_Y(&y);
         h += y;

         CComPtr<IPoint2d> p1, p2;
         p1.CoCreateInstance(CLSID_Point2d);
         p2.CoCreateInstance(CLSID_Point2d);

         p1->Move(-99999,h);
         p2->Move( 99999,h);

         line->ThroughPoints(p1,p2);

         CComPtr<ISection> clipped_section;
         props.Section->ClipWithLine(line,&clipped_section);

         CComPtr<IElasticProperties> bottomHalfElasticProperties;
         clipped_section->get_ElasticProperties(&bottomHalfElasticProperties);

         CComPtr<IShapeProperties> bottomHalfShapeProperties;
         bottomHalfElasticProperties->TransformProperties(Egdr,&bottomHalfShapeProperties);

         Float64 area;
         bottomHalfShapeProperties->get_Area(&area);
         props.AcBottomHalf = area;

         // Area on top half of composite section for LRFD Fig. 5.8.3.4.2-3
         CComPtr<IShapeProperties> fullShapeProperties;
         props.ElasticProps->TransformProperties(Egdr,&fullShapeProperties);
         fullShapeProperties->get_Area(&area);
         props.AcTopHalf = area - props.AcBottomHalf; // top + bottom = full  ==> top = full - botton
      }
      else if ( sectPropType == SPT_NET_DECK )
      {
         CComPtr<ISection> deckSection;
         HRESULT hr = m_SectCutTool->CreateNetDeckSection(m_Bridge,gdrID,segmentKey.segmentIndex,distFromStartOfSegment,leftGdrID,rightGdrID,intervalIdx,&deckSection);
         ATLASSERT(SUCCEEDED(hr));

         // get mass properties
         CComPtr<IMassProperties> massprops;
         deckSection->get_MassProperties(&massprops);

         // get elastic properties of section
         CComPtr<IElasticProperties> eprop;
         deckSection->get_ElasticProperties(&eprop);

         // transform section properties
         // this transformed all materials into equivalent girder concrete
         // we always do this because the deck must be transformed, even for net properties
         CComPtr<IShapeProperties> shapeprops;
         Float64 Edeck = GetDeckAgeAdjustedEc(intervalIdx); // this method returns the correct E even for gross and tranformed properties without time-step losses
         eprop->TransformProperties(Edeck,&shapeprops);

         props.Section      = deckSection;
         props.ElasticProps = eprop;
         props.ShapeProps   = shapeprops;
         props.MassProps    = massprops;
      }
      else
      {
         ATLASSERT(false); // should never get here
      }
   }
   else
   {
      // prestressing has not been released into segment yet so essentially we don't have a segment 
      // -OR-
      // we are at a closure pour and it hasn't reached strength enough to be effective
      // create default properties (all zero values)
      props.Section.CoCreateInstance(CLSID_CompositeSectionEx);
      props.Section->get_ElasticProperties(&props.ElasticProps);
      props.Section->get_MassProperties(&props.MassProps);
      props.ElasticProps->TransformProperties(1.0,&props.ShapeProps);
   }


   // don't store if not a real POI
   if ( poi.GetID() == INVALID_ID )
      return props;

   // Store the properties
   std::pair<SectPropContainer::iterator,bool> result( m_SectProps[sectPropType].insert(std::make_pair(poiKey,props)) );
   ATLASSERT(result.second == true);
   found = result.first; 
   ATLASSERT(found == m_SectProps[sectPropType].find(poiKey));
   ATLASSERT(found != m_SectProps[sectPropType].end());
   return (*found).second;
}

Float64 CBridgeAgentImp::GetHalfElevation(const pgsPointOfInterest& poi)
{
   Float64 deck_thickness = GetStructuralSlabDepth(poi);
   Float64 girder_depth = GetHeight(poi);
   Float64 td2 = (deck_thickness + girder_depth)/2.;
   return td2;    // line cut is at 1/2 of composite section depth
}

HRESULT CBridgeAgentImp::GetSlabOverhangs(Float64 distFromStartOfBridge,Float64* pLeft,Float64* pRight)
{
   Float64 station = GetPierStation(0);
   station += distFromStartOfBridge;

   SpanIndexType spanIdx = GetSpanIndex(distFromStartOfBridge);
   if ( spanIdx == INVALID_INDEX )
   {
      if ( distFromStartOfBridge < 0 )
         spanIdx = 0;
      else
         spanIdx = GetSpanCount()-1;
   }

   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CSpanData2* pSpan = pBridgeDesc->GetSpan(spanIdx);
   const CGirderGroupData* pGroup = pBridgeDesc->GetGirderGroup(pSpan);
   GroupIndexType grpIdx = pGroup->GetIndex();
   GirderIndexType nGirders = pGroup->GetGirderCount();

   GirderIDType leftGdrID  = ::GetSuperstructureMemberID(grpIdx,0);
   GirderIDType rightGdrID = ::GetSuperstructureMemberID(grpIdx,nGirders-1);

   m_BridgeGeometryTool->DeckOverhang(m_Bridge,station,leftGdrID, NULL,qcbLeft,pLeft);
   m_BridgeGeometryTool->DeckOverhang(m_Bridge,station,rightGdrID,NULL,qcbRight,pRight);

   return S_OK;
}

HRESULT CBridgeAgentImp::GetGirderSection(const pgsPointOfInterest& poi,pgsTypes::SectionCoordinateType csType,IGirderSection** gdrSection)
{
   const CSegmentKey& segmentKey = poi.GetSegmentKey();

   CComPtr<ISegment> segment;
   GetSegment(segmentKey,&segment);

   CComPtr<IShape> girder_shape;
   segment->get_PrimaryShape(poi.GetDistFromStart(),&girder_shape);

   if ( csType == pgsTypes::scGirder )
   {
      // Convert to girder section coordinates....
      // the bottom center point is located at (0,0)
      CComQIPtr<IXYPosition> position(girder_shape);

      CComPtr<IPoint2d> point;
      position->get_LocatorPoint(lpBottomCenter,&point);

      point->Move(0,0);

      position->put_LocatorPoint(lpBottomCenter,point);
   }


   CComQIPtr<IGirderSection> girder_section(girder_shape);

   ASSERT(girder_section != NULL);

   (*gdrSection) = girder_section;
   (*gdrSection)->AddRef();

   return S_OK;
}

HRESULT CBridgeAgentImp::GetSuperstructureMember(const CGirderKey& girderKey,ISuperstructureMember* *ssmbr)
{
   VALIDATE(BRIDGE);
   return ::GetSuperstructureMember(m_Bridge,girderKey,ssmbr);
}

HRESULT CBridgeAgentImp::GetSegment(const CSegmentKey& segmentKey,ISegment** segment)
{
   VALIDATE(BRIDGE);
   return ::GetSegment(m_Bridge,segmentKey,segment);
}

HRESULT CBridgeAgentImp::GetGirder(const CSegmentKey& segmentKey,IPrecastGirder** girder)
{
   VALIDATE(BRIDGE);
   return ::GetGirder(m_Bridge,segmentKey,girder);
}

HRESULT CBridgeAgentImp::GetGirder(const pgsPointOfInterest& poi,IPrecastGirder** girder)
{
   return GetGirder(poi.GetSegmentKey(),girder);
}

Float64 CBridgeAgentImp::GetGrossSlabDepth()
{
   // Private method for getting gross slab depth
   // Gross slab depth is needed during validation of bridge model,
   // but public method causes validation. This causes a recusion problem
   
   // #pragma Reminder("Assumes constant thickness deck")
   CComPtr<IBridgeDeck> deck;
   m_Bridge->get_Deck(&deck);

   CComQIPtr<ICastSlab>    cip(deck);
   CComQIPtr<IPrecastSlab> sip(deck);
   CComQIPtr<IOverlaySlab> overlay(deck);

   Float64 deck_thickness;
   if ( cip != NULL )
   {
      cip->get_GrossDepth(&deck_thickness);
   }
   else if ( sip != NULL )
   {
      Float64 cast_depth;
      sip->get_CastDepth(&cast_depth);

      Float64 panel_depth;
      sip->get_PanelDepth(&panel_depth);

      deck_thickness = cast_depth + panel_depth;
   }
   else if ( overlay != NULL )
   {
      overlay->get_GrossDepth(&deck_thickness);
   }
   else
   {
      ATLASSERT(false); // bad deck type
      deck_thickness = 0;
   }

   return deck_thickness;
}

Float64 CBridgeAgentImp::GetCastDepth()
{
   // #pragma Reminder("Assumes constant thickness deck")
   CComPtr<IBridgeDeck> deck;
   m_Bridge->get_Deck(&deck);

   CComQIPtr<ICastSlab>    cip(deck);
   CComQIPtr<IPrecastSlab> sip(deck);
   CComQIPtr<IOverlaySlab> overlay(deck);

   Float64 cast_depth;
   if ( cip != NULL )
   {
      cip->get_GrossDepth(&cast_depth);
   }
   else if ( sip != NULL )
   {
      sip->get_CastDepth(&cast_depth);
   }
   else if ( overlay != NULL )
   {
      overlay->get_GrossDepth(&cast_depth);
   }
   else
   {
      ATLASSERT(false); // bad deck type
      cast_depth = 0;
   }

   return cast_depth;
}

Float64 CBridgeAgentImp::GetPanelDepth()
{
   // Private method for getting gross slab depth
   // Gross slab depth is needed during validation of bridge model,
   // but public method causes validation. This causes a recusion problem
//#pragma Reminder("Assumes constant thickness deck")
   CComPtr<IBridgeDeck> deck;
   m_Bridge->get_Deck(&deck);

   CComQIPtr<ICastSlab>    cip(deck);
   CComQIPtr<IPrecastSlab> sip(deck);
   CComQIPtr<IOverlaySlab> overlay(deck);

   Float64 panel_depth;
   if ( cip != NULL )
   {
      panel_depth = 0;
   }
   else if ( sip != NULL )
   {
      sip->get_PanelDepth(&panel_depth);
   }
   else if ( overlay != NULL )
   {
      panel_depth = 0;
   }
   else
   {
      ATLASSERT(false); // bad deck type
      panel_depth = 0;
   }

   return panel_depth;
}


Float64 CBridgeAgentImp::GetSlabOverhangDepth()
{
   CComPtr<IBridgeDeck> deck;
   m_Bridge->get_Deck(&deck);

   CComQIPtr<ICastSlab>    cip(deck);
   CComQIPtr<IPrecastSlab> sip(deck);
   CComQIPtr<IOverlaySlab> overlay(deck);

   Float64 overhang_depth;
   if ( cip != NULL )
   {
      cip->get_OverhangDepth(&overhang_depth);
   }
   else if ( sip != NULL )
   {
      sip->get_CastDepth(&overhang_depth);
   }
   else if ( overlay != NULL )
   {
      overlay->get_GrossDepth(&overhang_depth);
   }
   else
   {
      ATLASSERT(false); // bad deck type
      // Not implemented yet... need to return the top flange thickness of exterior girder if there is not slab
      overhang_depth = 0;
   }

   return overhang_depth;
}

void CBridgeAgentImp::LayoutDeckRebar(const CDeckDescription2* pDeck,IBridgeDeck* deck)
{
   const CDeckRebarData& rebar_data = pDeck->DeckRebarData;

   CComPtr<IBridgeDeckRebarLayout> rebar_layout;
   rebar_layout.CoCreateInstance(CLSID_BridgeDeckRebarLayout);
   deck->putref_RebarLayout(rebar_layout);
   rebar_layout->putref_Bridge(m_Bridge);
   rebar_layout->putref_EffectiveFlangeWidthTool(m_EffFlangeWidthTool);

   Float64 deck_height;
   deck->get_GrossDepth(&deck_height);

   // Get the interval where the rebar is first introducted into the system.
   // Technically, the rebar is first introduced to the system when the deck
   // concrete is cast, however the rebar isn't performing any structural function
   // until the concrete is cured
   IntervalIndexType intervalIdx = m_IntervalManager.GetCompositeDeckInterval();

   // Create a rebar factory. This does the work of creating rebar objects
   CComPtr<IRebarFactory> rebar_factory;
   rebar_factory.CoCreateInstance(CLSID_RebarFactory);

   // Rebar factory needs a unit server object for units conversion
   CComPtr<IUnitServer> unitServer;
   unitServer.CoCreateInstance(CLSID_UnitServer);
   HRESULT hr = ConfigureUnitServer(unitServer);
   ATLASSERT(SUCCEEDED(hr));

   CComPtr<IUnitConvert> unit_convert;
   unitServer->get_UnitConvert(&unit_convert);

   // First layout rebar that runs the full length of the deck
   CComPtr<IBridgeDeckRebarLayoutItem> deck_rebar_layout_item;
   deck_rebar_layout_item.CoCreateInstance(CLSID_BridgeDeckRebarLayoutItem);
   deck_rebar_layout_item->putref_Bridge(m_Bridge);
   rebar_layout->Add(deck_rebar_layout_item);

   // Top Mat - Bars
   if ( rebar_data.TopRebarSize != matRebar::bsNone )
   {
      // create the rebar object
      BarSize       matSize  = GetBarSize(rebar_data.TopRebarSize);
      MaterialSpec  matSpec  = GetRebarSpecification(rebar_data.TopRebarType);
      RebarGrade    matGrade = GetRebarGrade(rebar_data.TopRebarGrade);

      CComPtr<IRebar> rebar;
      rebar_factory->CreateRebar(matSpec,matGrade,matSize,unit_convert,intervalIdx,&rebar);

      Float64 db;
      rebar->get_NominalDiameter(&db);

      // create the rebar pattern (definition of rebar in the cross section)
      CComPtr<IBridgeDeckRebarPattern> rebar_pattern;
      rebar_pattern.CoCreateInstance(CLSID_BridgeDeckRebarPattern);

      // Locate rebar from the bottom of the deck
      Float64 Y = deck_height - rebar_data.TopCover - db/2;

      // create the rebar pattern
      rebar_pattern->putref_Rebar(rebar);
      rebar_pattern->putref_RebarLayoutItem(deck_rebar_layout_item);
      rebar_pattern->put_Spacing( rebar_data.TopSpacing );
      rebar_pattern->put_Location( Y );
      rebar_pattern->put_IsLumped(VARIANT_FALSE);

      // add this pattern to the layout
      deck_rebar_layout_item->AddRebarPattern(rebar_pattern);
   }

   // Top Mat - Lump Sum
   if ( !IsZero(rebar_data.TopLumpSum) )
   {
      MaterialSpec  matSpec  = GetRebarSpecification(rebar_data.TopRebarType);
      RebarGrade    matGrade = GetRebarGrade(rebar_data.TopRebarGrade);

      // create a dummy #3 bar, then change the diameter and bar area to match
      // the lump sum bar
      CComPtr<IRebar> rebar;
      rebar_factory->CreateRebar(matSpec,matGrade,bs3,unit_convert,intervalIdx,&rebar);
      rebar->put_NominalDiameter(0.0);
      rebar->put_NominalArea(rebar_data.TopLumpSum);

      Float64 Y = deck_height - rebar_data.TopCover;

      // create the rebar pattern (definition of rebar in the cross section)
      CComPtr<IBridgeDeckRebarPattern> rebar_pattern;
      rebar_pattern.CoCreateInstance(CLSID_BridgeDeckRebarPattern);

      // create the rebar pattern
      rebar_pattern->putref_Rebar(rebar);
      rebar_pattern->putref_RebarLayoutItem(deck_rebar_layout_item);
      rebar_pattern->put_Spacing( 0.0 );
      rebar_pattern->put_Location( Y );
      rebar_pattern->put_IsLumped(VARIANT_TRUE);

      // add this pattern to the layout
      deck_rebar_layout_item->AddRebarPattern(rebar_pattern);
   }

   // Bottom Mat - Rebar
   if ( rebar_data.BottomRebarSize != matRebar::bsNone )
   {
      // create the rebar object
      BarSize       matSize  = GetBarSize(rebar_data.BottomRebarSize);
      MaterialSpec  matSpec  = GetRebarSpecification(rebar_data.BottomRebarType);
      RebarGrade    matGrade = GetRebarGrade(rebar_data.BottomRebarGrade);

      CComPtr<IRebar> rebar;
      rebar_factory->CreateRebar(matSpec,matGrade,matSize,unit_convert,intervalIdx,&rebar);

      Float64 db;
      rebar->get_NominalDiameter(&db);

      // create the rebar pattern (definition of rebar in the cross section)
      CComPtr<IBridgeDeckRebarPattern> rebar_pattern;
      rebar_pattern.CoCreateInstance(CLSID_BridgeDeckRebarPattern);

      // Locate rebar from the bottom of the deck
      Float64 Y = rebar_data.BottomCover + db/2;

      // create the rebar pattern
      rebar_pattern->putref_Rebar(rebar);
      rebar_pattern->putref_RebarLayoutItem(deck_rebar_layout_item);
      rebar_pattern->put_Spacing( rebar_data.BottomSpacing );
      rebar_pattern->put_Location( Y );
      rebar_pattern->put_IsLumped(VARIANT_FALSE);

      // add this pattern to the layout
      deck_rebar_layout_item->AddRebarPattern(rebar_pattern);
   }

   // Bottom Mat - Lump Sum
   if ( !IsZero(rebar_data.BottomLumpSum) )
   {
      MaterialSpec  matSpec  = GetRebarSpecification(rebar_data.BottomRebarType);
      RebarGrade    matGrade = GetRebarGrade(rebar_data.BottomRebarGrade);

      CComPtr<IRebar> rebar;
      rebar_factory->CreateRebar(matSpec,matGrade,bs3,unit_convert,intervalIdx,&rebar);
      rebar->put_NominalDiameter(0.0);
      rebar->put_NominalArea(rebar_data.BottomLumpSum);

      Float64 Y = rebar_data.BottomCover;

      // create the rebar pattern (definition of rebar in the cross section)
      CComPtr<IBridgeDeckRebarPattern> rebar_pattern;
      rebar_pattern.CoCreateInstance(CLSID_BridgeDeckRebarPattern);

      // create the rebar pattern
      rebar_pattern->putref_Rebar(rebar);
      rebar_pattern->putref_RebarLayoutItem(deck_rebar_layout_item);
      rebar_pattern->put_Spacing( 0.0 );
      rebar_pattern->put_Location( Y );
      rebar_pattern->put_IsLumped(VARIANT_TRUE);

      // add this pattern to the layout
      deck_rebar_layout_item->AddRebarPattern(rebar_pattern);
   }

   // Negative moment rebar over piers
   std::vector<CDeckRebarData::NegMomentRebarData>::const_iterator iter(rebar_data.NegMomentRebar.begin());
   std::vector<CDeckRebarData::NegMomentRebarData>::const_iterator end(rebar_data.NegMomentRebar.end());
   for ( ; iter != end; iter++ )
   {
      const CDeckRebarData::NegMomentRebarData& nm_rebar_data = *iter;

      CComPtr<INegativeMomentBridgeDeckRebarLayoutItem> nm_deck_rebar_layout_item;
      nm_deck_rebar_layout_item.CoCreateInstance(CLSID_NegativeMomentBridgeDeckRebarLayoutItem);
      nm_deck_rebar_layout_item->putref_Bridge(m_Bridge);
      rebar_layout->Add(nm_deck_rebar_layout_item);

      nm_deck_rebar_layout_item->put_Pier( nm_rebar_data.PierIdx );
      nm_deck_rebar_layout_item->put_LeftCutoff(  nm_rebar_data.LeftCutoff );
      nm_deck_rebar_layout_item->put_RightCutoff( nm_rebar_data.RightCutoff );

      if ( nm_rebar_data.RebarSize != matRebar::bsNone )
      {
         // Rebar
         BarSize       matSize  = GetBarSize(nm_rebar_data.RebarSize);
         MaterialSpec  matSpec  = GetRebarSpecification(nm_rebar_data.RebarType);
         RebarGrade    matGrade = GetRebarGrade(nm_rebar_data.RebarGrade);

         CComPtr<IRebar> rebar;
         rebar_factory->CreateRebar(matSpec,matGrade,matSize,unit_convert,intervalIdx,&rebar);

         Float64 db;
         rebar->get_NominalDiameter(&db);

         // create the rebar pattern (definition of rebar in the cross section)
         CComPtr<IBridgeDeckRebarPattern> rebar_pattern;
         rebar_pattern.CoCreateInstance(CLSID_BridgeDeckRebarPattern);

         // Locate rebar from the bottom of the deck
         Float64 Y;
         if ( nm_rebar_data.Mat == CDeckRebarData::TopMat )
            Y = deck_height - rebar_data.TopCover - db/2;
         else
            Y = rebar_data.BottomCover + db/2;

         // create the rebar pattern
         rebar_pattern->putref_Rebar(rebar);
         rebar_pattern->putref_RebarLayoutItem(nm_deck_rebar_layout_item);
         rebar_pattern->put_Spacing( nm_rebar_data.Spacing );
         rebar_pattern->put_Location( Y );
         rebar_pattern->put_IsLumped(VARIANT_FALSE);

         // add this pattern to the layout
         nm_deck_rebar_layout_item->AddRebarPattern(rebar_pattern);
      }

      // Lump Sum
      if ( !IsZero(nm_rebar_data.LumpSum) )
      {
         MaterialSpec  matSpec  = GetRebarSpecification(nm_rebar_data.RebarType);
         RebarGrade    matGrade = GetRebarGrade(nm_rebar_data.RebarGrade);

         // create a dummy #3 bar, then change the diameter and bar area to match
         // the lump sum bar
         CComPtr<IRebar> rebar;
         rebar_factory->CreateRebar(matSpec,matGrade,bs3,unit_convert,intervalIdx,&rebar);
         rebar->put_NominalDiameter(0.0);
         rebar->put_NominalArea(nm_rebar_data.LumpSum);

         Float64 Y;
         if ( nm_rebar_data.Mat == CDeckRebarData::TopMat )
            Y = deck_height - rebar_data.TopCover;
         else
            Y = rebar_data.BottomCover;

         // create the rebar pattern (definition of rebar in the cross section)
         CComPtr<IBridgeDeckRebarPattern> rebar_pattern;
         rebar_pattern.CoCreateInstance(CLSID_BridgeDeckRebarPattern);

         // create the rebar pattern
         rebar_pattern->putref_Rebar(rebar);
         rebar_pattern->putref_RebarLayoutItem(nm_deck_rebar_layout_item);
         rebar_pattern->put_Spacing( 0.0 );
         rebar_pattern->put_Location( Y );
         rebar_pattern->put_IsLumped(VARIANT_TRUE);

         // add this pattern to the layout
         nm_deck_rebar_layout_item->AddRebarPattern(rebar_pattern);
      }
   }
}

void CBridgeAgentImp::LayoutSegmentRebar(const CSegmentKey& segmentKey)
{
/*
   // Get the rebar input data
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CPrecastSegmentData* pSegment = pIBridgeDesc->GetPrecastSegmentData(segmentKey);
   const CLongitudinalRebarData& rebar_data = pSegment->LongitudinalRebarData;

   // If there aren't any rows of rebar data defined for this segment,
   // leave now
   const std::vector<CLongitudinalRebarData::RebarRow>& rebar_rows = rebar_data.RebarRows;
   if ( rebar_rows.size() == 0 )
      return;

   // Get the interval where the rebar is first introducted into the system.
   // Technically, the rebar is first introduced to the system when the segment
   // concrete is cast, however the rebar isn't doing anything structural
   // until the concrete is cured and the prestressing is released
   IntervalIndexType intervalIdx = m_IntervalManager.GetPrestressReleaseInterval();

   // Get the PrecastGirder object that will create the rebar model in
   // Given the basic input, this guy will compute the actual coordinates of
   // rebar at any cross section along the segment
   CComPtr<IPrecastGirder> girder;
   GetGirder(segmentKey,&girder);

   CComPtr<IRebarLayout> rebar_layout;
   girder->get_RebarLayout(&rebar_layout);

   // Gather some information for layout of the rebar along the length of the
   // girder and within the girder cross section
   Float64 gdr_length = GetSegmentLength(segmentKey);
   pgsPointOfInterest startPoi(segmentKey,0.00);
   pgsPointOfInterest endPoi(segmentKey,gdr_length);

   CComPtr<IShape> startShape, endShape;
   GetSegmentShapeDirect(startPoi,&startShape);
   GetSegmentShapeDirect(endPoi,  &endShape);

   CComPtr<IRect2d> bbStart, bbEnd;
   startShape->get_BoundingBox(&bbStart);
   endShape->get_BoundingBox(&bbEnd);

   CComPtr<IPoint2d> topCenter, bottomCenter;
   bbStart->get_TopCenter(&topCenter);
   bbStart->get_BottomCenter(&bottomCenter);

   Float64 startTopY, startBottomY;
   topCenter->get_Y(&startTopY);
   bottomCenter->get_Y(&startBottomY);

   topCenter.Release();
   bottomCenter.Release();

   bbEnd->get_TopCenter(&topCenter);
   bbEnd->get_BottomCenter(&bottomCenter);

   // elevation of the top and bottom of the girder
   Float64 endTopY, endBottomY;
   topCenter->get_Y(&endTopY);
   bottomCenter->get_Y(&endBottomY);

   // Get some basic information about the rebar so we can build a rebar material object
   // We need to map PGSuper data into WBFLGenericBridge data
   MaterialSpec matSpec  = GetRebarSpecification(rebar_data.BarType);
   RebarGrade   matGrade = GetRebarGrade(rebar_data.BarGrade);

   // Create a rebar factory. This does the work of creating rebar objects
   CComPtr<IRebarFactory> rebar_factory;
   rebar_factory.CoCreateInstance(CLSID_RebarFactory);

   // Need a unit server object for units conversion
   CComPtr<IUnitServer> unitServer;
   unitServer.CoCreateInstance(CLSID_UnitServer);
   HRESULT hr = ConfigureUnitServer(unitServer);
   ATLASSERT(SUCCEEDED(hr));

   CComPtr<IUnitConvert> unit_convert;
   unitServer->get_UnitConvert(&unit_convert);


   // PGSuper rebar input defines the rebars to run full length of the girder
   // Create a rebar layout item (this defines the layout of a rebar pattern
   // over the length of a girder)
   CComPtr<IFlexRebarLayoutItem> flex_layout_item;
   flex_layout_item.CoCreateInstance(CLSID_FlexRebarLayoutItem);

   flex_layout_item->put_Position(lpCenter); // length of rebar are measured from center of girder (50% each side of centerline)
   flex_layout_item->put_LengthFactor(1.0);  // rebar are 100% of the length of the girder (could be something else, but that's not the way the input is set up)
   flex_layout_item->putref_Girder(girder);  // this is the girder

   CComQIPtr<IRebarLayoutItem> rebar_layout_item(flex_layout_item);

   IndexType nRows = rebar_rows.size();
   for ( IndexType idx = 0; idx < nRows; idx++ )
   {
      // get the data for this row
      const CLongitudinalRebarData::RebarRow& info = rebar_rows[idx];

      // if there are bars in this row, and the bars have size, define
      // the rebar pattern for this row. Rebar pattern is the definition of the
      // rebar within the cross section
      if ( 0 < info.BarSize && 0 < info.NumberOfBars )
      {
         // Create the rebar object
         BarSize bar_size = GetBarSize(info.BarSize);
         CComPtr<IRebar> rebar;
         rebar_factory->CreateRebar(matSpec,matGrade,bar_size,unit_convert,intervalIdx,&rebar);

         Float64 db;
         rebar->get_NominalDiameter(&db);

         // create the rebar pattern (definition of rebar in the cross section)
         CComPtr<IRebarRowPattern> row_pattern;
         row_pattern.CoCreateInstance(CLSID_RebarRowPattern);

         // Locate rebar from the bottom of the girder
         Float64 yStart, yEnd;
         if (info.Face == pgsTypes::GirderBottom)
         {
            // input is measured from bottom of girder
            yStart = info.Cover + db/2;
            yEnd   = yStart;
         }
         else
         {
            // input is measured from top of girder
            yStart = startTopY - info.Cover - db/2 - startBottomY;
            yEnd   = endTopY   - info.Cover - db/2 - endBottomY;
         }

         // create the anchor points for the rebar pattern on the vertical
         // centerline of the girder
         CComPtr<IPoint2d> startAnchor;
         startAnchor.CoCreateInstance(CLSID_Point2d);
         startAnchor->Move(0,yStart);

         CComPtr<IPoint2d> endAnchor;
         endAnchor.CoCreateInstance(CLSID_Point2d);
         endAnchor->Move(0,yEnd);

         // create the rebar pattern
         row_pattern->putref_Rebar(rebar);
         row_pattern->put_AnchorPoint(etStart,startAnchor);
         row_pattern->put_AnchorPoint(etEnd,  endAnchor);
         row_pattern->put_Count( info.NumberOfBars );
         row_pattern->put_Spacing( info.BarSpacing );
         row_pattern->put_Orientation( rroHCenter );

         // add this pattern to the layout
         rebar_layout_item->AddRebarPattern(row_pattern);
      }
   }

   rebar_layout->Add(rebar_layout_item);
*/
   // Get the rebar input data
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CPrecastSegmentData* pSegment = pIBridgeDesc->GetPrecastSegmentData(segmentKey);
   const CLongitudinalRebarData& rebar_data = pSegment->LongitudinalRebarData;

   const std::vector<CLongitudinalRebarData::RebarRow>& rebar_rows = rebar_data.RebarRows;
   if ( 0 < rebar_rows.size() )
   {
      // Get the interval where the rebar is first introducted into the system.
      // Technically, the rebar is first introduced to the system when the segment
      // concrete is cast, however the rebar isn't doing anything structural
      // until the concrete is cured and the prestressing is released
      IntervalIndexType intervalIdx = m_IntervalManager.GetPrestressReleaseInterval();

      // Get the PrecastGirder object that will create the rebar model in
      // Given the basic input, this guy will compute the actual coordinates of
      // rebar at any cross section along the segment
      CComPtr<IPrecastGirder> girder;
      GetGirder(segmentKey,&girder);

      Float64 segment_length = GetSegmentLength(segmentKey);

      CComPtr<IRebarLayout> rebar_layout;
      girder->get_RebarLayout(&rebar_layout);

      GET_IFACE(ILongitudinalRebar,pLongRebar);
      std::_tstring strRebarName = pLongRebar->GetSegmentLongitudinalRebarMaterial(segmentKey);
      
      matRebar::Grade grade;
      matRebar::Type type;
      pLongRebar->GetSegmentLongitudinalRebarMaterial(segmentKey,type,grade);

      MaterialSpec matSpec = GetRebarSpecification(type);
      RebarGrade matGrade  = GetRebarGrade(grade);

      CComPtr<IRebarFactory> rebar_factory;
      rebar_factory.CoCreateInstance(CLSID_RebarFactory);

//#pragma Reminder("The unitserver below leaks memory because it is referenced to strongly by all of its unit types, but has a weak ref to them. Not easy to resolve")
      CComPtr<IUnitServer> unitServer;
      unitServer.CoCreateInstance(CLSID_UnitServer);
      HRESULT hr = ConfigureUnitServer(unitServer);
      ATLASSERT(SUCCEEDED(hr));

      CComPtr<IUnitConvert> unit_convert;
      unitServer->get_UnitConvert(&unit_convert);

      for ( RowIndexType idx = 0; idx < rebar_rows.size(); idx++ )
      {
         const CLongitudinalRebarData::RebarRow& info = rebar_rows[idx];

         Float64 startLayout(0), endLayout(0);

         if (info.GetRebarStartEnd(segment_length, &startLayout, &endLayout))
         {
            CComPtr<IFixedLengthRebarLayoutItem> fixedlength_layout_item;
            hr = fixedlength_layout_item.CoCreateInstance(CLSID_FixedLengthRebarLayoutItem);

            fixedlength_layout_item->put_Start(startLayout);
            fixedlength_layout_item->put_End(endLayout);

            // Set pattern for layout
            pgsPointOfInterest startPoi(segmentKey,startLayout);
            pgsPointOfInterest endPoi(segmentKey,endLayout);

            CComPtr<IShape> startShape, endShape;
            GetSegmentShapeDirect(startPoi,&startShape);
            GetSegmentShapeDirect(endPoi,  &endShape);

            CComPtr<IRect2d> bbStart, bbEnd;
            startShape->get_BoundingBox(&bbStart);
            endShape->get_BoundingBox(&bbEnd);

            CComPtr<IPoint2d> topCenter, bottomCenter;
            bbStart->get_TopCenter(&topCenter);
            bbStart->get_BottomCenter(&bottomCenter);

            Float64 startTopY, startBottomY;
            topCenter->get_Y(&startTopY);
            bottomCenter->get_Y(&startBottomY);

            topCenter.Release();
            bottomCenter.Release();

            bbEnd->get_TopCenter(&topCenter);
            bbEnd->get_BottomCenter(&bottomCenter);

            Float64 endTopY, endBottomY;
            topCenter->get_Y(&endTopY);
            bottomCenter->get_Y(&endBottomY);

            BarSize bar_size = GetBarSize(info.BarSize);
            CComPtr<IRebar> rebar;
            rebar_factory->CreateRebar(matSpec,matGrade,bar_size,unit_convert,intervalIdx,&rebar);

            Float64 db;
            rebar->get_NominalDiameter(&db);

            CComPtr<IRebarRowPattern> row_pattern;
            row_pattern.CoCreateInstance(CLSID_RebarRowPattern);

            Float64 yStart, yEnd;
            if (info.Face == pgsTypes::GirderBottom)
            {
               yStart = info.Cover + db/2;
               yEnd   = yStart;
            }
            else
            {
               yStart = startTopY - info.Cover - db/2 - startBottomY;
               yEnd   = endTopY   - info.Cover - db/2 - endBottomY;
            }

            CComPtr<IPoint2d> startAnchor;
            startAnchor.CoCreateInstance(CLSID_Point2d);
            startAnchor->Move(0,yStart);

            CComPtr<IPoint2d> endAnchor;
            endAnchor.CoCreateInstance(CLSID_Point2d);
            endAnchor->Move(0,yEnd);

            row_pattern->putref_Rebar(rebar);
            row_pattern->put_AnchorPoint(etStart,startAnchor);
            row_pattern->put_AnchorPoint(etEnd,  endAnchor);
            row_pattern->put_Count( info.NumberOfBars );
            row_pattern->put_Spacing( info.BarSpacing );
            row_pattern->put_Orientation( rroHCenter );

            fixedlength_layout_item->AddRebarPattern(row_pattern);

            rebar_layout->Add(fixedlength_layout_item);
         }
      }
   }
}

void CBridgeAgentImp::LayoutClosurePourRebar(const CClosureKey& closureKey)
{
   // Get the rebar input data
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CClosurePourData* pClosure = pIBridgeDesc->GetClosurePourData(closureKey);
   const CLongitudinalRebarData& rebar_data = pClosure->GetRebar();

   // If there aren't any rows of rebar data defined for this segment,
   // leave now
   const std::vector<CLongitudinalRebarData::RebarRow>& rebar_rows = rebar_data.RebarRows;
   if ( rebar_rows.size() == 0 )
      return;

   // Get the interval where the rebar is first introducted into the system.
   // Technically, the rebar is first introduced to the system when the closure pour
   // concrete is cast, however the rebar isn't doing anything structural
   // until the concrete is cured
   IntervalIndexType intervalIdx = GetCompositeClosurePourInterval(closureKey);

   // Get the keys for the segments on either side of the closure
   CSegmentKey prevSegmentKey(pClosure->GetLeftSegment()->GetSegmentKey());
   CSegmentKey nextSegmentKey(pClosure->GetRightSegment()->GetSegmentKey());

   // Get the PrecastGirder object that will create the rebar model in
   // Given the basic input, this guy will compute the actual coordinates of
   // rebar at any cross section along the segment
   CComPtr<IPrecastGirder> girder;
   GetGirder(prevSegmentKey,&girder);

   CComPtr<IRebarLayout> rebar_layout;
   girder->get_RebarLayout(&rebar_layout);

   // Gather some information for layout of the rebar along the length of the
   // girder and within the girder cross section
   Float64 closure_length = GetClosurePourLength(closureKey);
   Float64 segment_length = GetSegmentLength(prevSegmentKey);
   pgsPointOfInterest leftPoi(prevSegmentKey,segment_length);
   pgsPointOfInterest rightPoi(nextSegmentKey,0,0);

   CComPtr<IShape> leftShape, rightShape;
   GetSegmentShapeDirect(leftPoi,  &leftShape);
   GetSegmentShapeDirect(rightPoi, &rightShape);

   CComPtr<IRect2d> bbLeft, bbRight;
   leftShape->get_BoundingBox(&bbLeft);
   rightShape->get_BoundingBox(&bbRight);

   CComPtr<IPoint2d> topCenter, bottomCenter;
   bbLeft->get_TopCenter(&topCenter);
   bbLeft->get_BottomCenter(&bottomCenter);

   Float64 leftTopY, leftBottomY;
   topCenter->get_Y(&leftTopY);
   bottomCenter->get_Y(&leftBottomY);

   topCenter.Release();
   bottomCenter.Release();

   bbRight->get_TopCenter(&topCenter);
   bbRight->get_BottomCenter(&bottomCenter);

   // elevation of the top and bottom of the girder
   Float64 rightTopY, rightBottomY;
   topCenter->get_Y(&rightTopY);
   bottomCenter->get_Y(&rightBottomY);

   // Get some basic information about the rebar so we can build a rebar material object
   // We need to map PGSuper data into WBFLGenericBridge data
   MaterialSpec matSpec  = GetRebarSpecification(rebar_data.BarType);
   RebarGrade   matGrade = GetRebarGrade(rebar_data.BarGrade);

   // Create a rebar factory. This does the work of creating rebar objects
   CComPtr<IRebarFactory> rebar_factory;
   rebar_factory.CoCreateInstance(CLSID_RebarFactory);

   // Need a unit server object for units conversion
   CComPtr<IUnitServer> unitServer;
   unitServer.CoCreateInstance(CLSID_UnitServer);
   HRESULT hr = ConfigureUnitServer(unitServer);
   ATLASSERT(SUCCEEDED(hr));

   CComPtr<IUnitConvert> unit_convert;
   unitServer->get_UnitConvert(&unit_convert);

   // PGSuper rebar input defines the rebars to run full length of the closure
   // Create a rebar layout item (this defines the layout of a rebar pattern
   // over the length of a girder)
   CComPtr<IFixedLengthRebarLayoutItem> fixed_layout_item;
   fixed_layout_item.CoCreateInstance(CLSID_FixedLengthRebarLayoutItem);
   ATLASSERT(fixed_layout_item);

   fixed_layout_item->put_Start(segment_length);
   fixed_layout_item->put_End(segment_length + closure_length);

   CComQIPtr<IRebarLayoutItem> rebar_layout_item(fixed_layout_item);

   IndexType nRows = rebar_rows.size();
   for ( IndexType idx = 0; idx < nRows; idx++ )
   {
      // get the data for this row
      const CLongitudinalRebarData::RebarRow& info = rebar_rows[idx];

      // if there are bars in this row, and the bars have size, define
      // the rebar pattern for this row. Rebar pattern is the definition of the
      // rebar within the cross section
      if ( 0 < info.BarSize && 0 < info.NumberOfBars )
      {
         // Create the rebar object
         BarSize bar_size = GetBarSize(info.BarSize);
         CComPtr<IRebar> rebar;
         rebar_factory->CreateRebar(matSpec,matGrade,bar_size,unit_convert,intervalIdx,&rebar);

         Float64 db;
         rebar->get_NominalDiameter(&db);

         // create the rebar pattern (definition of rebar in the cross section)
         CComPtr<IRebarRowPattern> row_pattern;
         row_pattern.CoCreateInstance(CLSID_RebarRowPattern);

         // Locate rebar from the bottom of the girder
         Float64 yLeft, yRight;
         if (info.Face == pgsTypes::GirderBottom)
         {
            // input is measured from bottom of girder
            yLeft = info.Cover + db/2;
            yRight= yLeft;
         }
         else
         {
            // input is measured from top of girder
            yLeft  = leftTopY  - info.Cover - db/2 - leftBottomY;
            yRight = rightTopY - info.Cover - db/2 - rightBottomY;
         }

         // create the anchor points for the rebar pattern on the vertical
         // centerline of the girder
         CComPtr<IPoint2d> leftAnchor;
         leftAnchor.CoCreateInstance(CLSID_Point2d);
         leftAnchor->Move(0,yLeft);

         CComPtr<IPoint2d> rightAnchor;
         rightAnchor.CoCreateInstance(CLSID_Point2d);
         rightAnchor->Move(0,yRight);

         // create the rebar pattern
         row_pattern->putref_Rebar(rebar);
         row_pattern->put_AnchorPoint(etStart, leftAnchor);
         row_pattern->put_AnchorPoint(etEnd,   rightAnchor);
         row_pattern->put_Count( info.NumberOfBars );
         row_pattern->put_Spacing( info.BarSpacing );
         row_pattern->put_Orientation( rroHCenter );

         // add this pattern to the layout
         rebar_layout_item->AddRebarPattern(row_pattern);
      }
   }

   rebar_layout->Add(rebar_layout_item);
}

void CBridgeAgentImp::CheckBridge()
{
   GET_IFACE(IEAFStatusCenter,pStatusCenter);
   GET_IFACE(IBridge,pBridge);
   GET_IFACE(ILibrary,pLib);

   GET_IFACE(IDocumentType,pDocType);

   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();

   // make sure all girders have positive lengths
   GroupIndexType nGroups = pBridgeDesc->GetGirderGroupCount();
   for ( GroupIndexType grpIdx = 0; grpIdx < nGroups; grpIdx++ )
   {
      GirderIndexType nGirders = pBridgeDesc->GetGirderGroup(grpIdx)->GetGirderCount();
      for( GirderIndexType gdrIdx = 0; gdrIdx < nGirders; gdrIdx++ )
      {
         const CSplicedGirderData* pGirder = pBridgeDesc->GetGirderGroup(grpIdx)->GetGirder(gdrIdx);
         SegmentIndexType nSegments = pGirder->GetSegmentCount();
         for ( SegmentIndexType segIdx = 0; segIdx < nSegments; segIdx++ )
         {
            CSegmentKey segmentKey(grpIdx,gdrIdx,segIdx);
            CComPtr<IPrecastGirder> girder;
            GetGirder(segmentKey, &girder);

            Float64 sLength;
            girder->get_SpanLength(&sLength);

            if ( sLength <= 0 )
            {
               std::_tostringstream os;
               if ( pDocType->IsPGSuperDocument() )
               {
                  os << _T("Span ") << LABEL_SPAN(grpIdx) << _T(" Girder ") << LABEL_GIRDER(gdrIdx)
                     << _T(" does not have a positive length.") << std::endl;
               }
               else
               {
                  os << _T("Group ") << LABEL_GROUP(grpIdx) << _T(" Girder ") << LABEL_GIRDER(gdrIdx)
                     << _T(" Segment ") << LABEL_SEGMENT(segIdx) << _T(" does not have a positive length.") << std::endl;
               }

               pgsBridgeDescriptionStatusItem* pStatusItem = 
                  new pgsBridgeDescriptionStatusItem(m_StatusGroupID,m_scidBridgeDescriptionError,pgsBridgeDescriptionStatusItem::General,os.str().c_str());

               pStatusCenter->Add(pStatusItem);

               os << _T("See Status Center for Details");
               THROW_UNWIND(os.str().c_str(),XREASON_NEGATIVE_GIRDER_LENGTH);
            }

            // Check location of temporary strands... usually in the top half of the girder
            const GirderLibraryEntry* pGirderEntry = GetGirderLibraryEntry(segmentKey);
            Float64 h_start = pGirderEntry->GetBeamHeight(pgsTypes::metStart);
            Float64 h_end   = pGirderEntry->GetBeamHeight(pgsTypes::metEnd);

            CComPtr<IStrandGrid> strand_grid;
            girder->get_TemporaryStrandGrid(etStart,&strand_grid);
            GridIndexType nPoints;
            strand_grid->get_GridPointCount(&nPoints);
            for ( GridIndexType idx = 0; idx < nPoints; idx++ )
            {
               CComPtr<IPoint2d> point;
               strand_grid->get_GridPoint(idx,&point);

               Float64 Y;
               point->get_Y(&Y);

               if ( Y < h_start/2 || Y < h_end/2 )
               {
                  CString strMsg;
                  if ( pDocType->IsPGSuperDocument() )
                  {
                     strMsg.Format(_T("Span %d Girder %s, Temporary strands are not in the top half of the girder"),LABEL_SPAN(grpIdx),LABEL_GIRDER(gdrIdx));
                  }
                  else
                  {
                     strMsg.Format(_T("Group %d Girder %s Segment %d, Temporary strands are not in the top half of the girder"),LABEL_GROUP(grpIdx),LABEL_GIRDER(gdrIdx),LABEL_SEGMENT(segIdx));
                  }
                  pgsInformationalStatusItem* pStatusItem = new pgsInformationalStatusItem(m_StatusGroupID,m_scidInformationalWarning,strMsg);
                  pStatusCenter->Add(pStatusItem);
               }
            }

            // Check that longitudinal rebars are located within section bounds
            std::vector<RowIndexType> outBoundRows = CheckLongRebarGeometry(segmentKey);
            if (!outBoundRows.empty())
            {
               // We have bars out of bounds.
               std::vector<RowIndexType>::iterator iter(outBoundRows.begin());
               std::vector<RowIndexType>::iterator end(outBoundRows.end());
               for ( ; iter != end; iter++ )
               {
                  RowIndexType row = *iter;

                  CString strMsg;
                  strMsg.Format(_T("Group %d Girder %s Segment %d: Longitudinal rebar row %d contains one or more bars located outside of the girder section."),LABEL_GROUP(segmentKey.groupIndex),LABEL_GIRDER(segmentKey.girderIndex),LABEL_SEGMENT(segmentKey.segmentIndex),LABEL_ROW(row));
                  pgsInformationalStatusItem* pStatusItem = new pgsInformationalStatusItem(m_StatusGroupID,m_scidInformationalWarning,strMsg);
                  pStatusCenter->Add(pStatusItem);
               }
            }
         } // next segment
      } // next girder
   } // next group

#if defined _DEBUG
   // Dumps the cogo model to a file so it can be viewed/debugged
   CComPtr<IStructuredSave2> save;
   save.CoCreateInstance(CLSID_StructuredSave2);
   save->Open(CComBSTR(_T("CogoModel.cogo")));

   save->BeginUnit(CComBSTR(_T("Cogo")),1.0);

   CComQIPtr<IStructuredStorage2> ss(m_CogoModel);
   ss->Save(save);

   save->EndUnit();
   save->Close();
#endif _DEBUG
}

SpanIndexType CBridgeAgentImp::GetSpanCount_Private()
{
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   return pBridgeDesc->GetSpanCount();
}

bool CBridgeAgentImp::ComputeNumPermanentStrands(StrandIndexType totalPermanent,const CSegmentKey& segmentKey,StrandIndexType* numStraight,StrandIndexType* numHarped)
{
   VALIDATE( GIRDER );

   CComPtr<IPrecastGirder> girder;
   GetGirder(segmentKey,&girder);

   HRESULT hr = m_StrandFillers[segmentKey].ComputeNumPermanentStrands(girder, totalPermanent, numStraight, numHarped);
   ATLASSERT(SUCCEEDED(hr));

   return hr==S_OK;
}

bool CBridgeAgentImp::ComputeNumPermanentStrands(StrandIndexType totalPermanent,LPCTSTR strGirderName, StrandIndexType* numStraight, StrandIndexType* numHarped)
{
   GET_IFACE(ILibrary,pLib);
   const GirderLibraryEntry* pGirderEntry = pLib->GetGirderEntry( strGirderName );
   return pGirderEntry->GetPermStrandDistribution(totalPermanent, numStraight, numHarped);
}

StrandIndexType CBridgeAgentImp::GetMaxNumPermanentStrands(const CSegmentKey& segmentKey)
{
   VALIDATE( GIRDER );

   CComPtr<IPrecastGirder> girder;
   GetGirder(segmentKey,&girder);

   StrandIndexType maxNum;
   HRESULT hr = m_StrandFillers[segmentKey].GetMaxNumPermanentStrands(girder, &maxNum);
   ATLASSERT(SUCCEEDED(hr));

   return maxNum;
}

StrandIndexType CBridgeAgentImp::GetMaxNumPermanentStrands(LPCTSTR strGirderName)
{
   GET_IFACE(ILibrary,pLib);
   const GirderLibraryEntry* pGirderEntry = pLib->GetGirderEntry( strGirderName );
   std::vector<GirderLibraryEntry::PermanentStrandType> permStrands = pGirderEntry->GetPermanentStrands();
   return permStrands.size()-1;
}

StrandIndexType CBridgeAgentImp::GetPreviousNumPermanentStrands(const CSegmentKey& segmentKey,StrandIndexType curNum)
{
   VALIDATE( GIRDER );

   CComPtr<IPrecastGirder> girder;
   GetGirder(segmentKey,&girder);

   StrandIndexType nextNum;
   HRESULT hr = m_StrandFillers[segmentKey].GetPreviousNumberOfPermanentStrands(girder, curNum, &nextNum);
   ATLASSERT(SUCCEEDED(hr));

   return nextNum;
}

StrandIndexType CBridgeAgentImp::GetPreviousNumPermanentStrands(LPCTSTR strGirderName,StrandIndexType curNum)
{
   GET_IFACE(ILibrary,pLib);
   const GirderLibraryEntry* pGdrEntry = pLib->GetGirderEntry(strGirderName);

   CStrandFiller strandFiller;
   strandFiller.Init(pGdrEntry);

   StrandIndexType prevNum;
   strandFiller.GetPreviousNumberOfPermanentStrands(NULL,curNum,&prevNum);

   return prevNum;
}

StrandIndexType CBridgeAgentImp::GetNextNumPermanentStrands(const CSegmentKey& segmentKey,StrandIndexType curNum)
{
   VALIDATE( GIRDER );

   CComPtr<IPrecastGirder> girder;
   GetGirder(segmentKey,&girder);

   StrandIndexType nextNum;
   HRESULT hr = m_StrandFillers[segmentKey].GetNextNumberOfPermanentStrands(girder, curNum, &nextNum);
   ATLASSERT(SUCCEEDED(hr));

   return nextNum;
}

StrandIndexType CBridgeAgentImp::GetNextNumPermanentStrands(LPCTSTR strGirderName,StrandIndexType curNum)
{
   GET_IFACE(ILibrary,pLib);
   const GirderLibraryEntry* pGdrEntry = pLib->GetGirderEntry(strGirderName);

   CStrandFiller strandFiller;
   strandFiller.Init(pGdrEntry);

   StrandIndexType nextNum;
   strandFiller.GetNextNumberOfPermanentStrands(NULL,curNum,&nextNum);

   return nextNum;
}

bool CBridgeAgentImp::ComputePermanentStrandIndices(LPCTSTR strGirderName,const PRESTRESSCONFIG& rconfig, pgsTypes::StrandType strType, IIndexArray** permIndices)
{
   CComPtr<IIndexArray> permStrands;  // array index = permanent strand for each strand of type
   permStrands.CoCreateInstance(CLSID_IndexArray);
   ATLASSERT(permStrands);

   GET_IFACE(ILibrary,pLib);
   const GirderLibraryEntry* pGdrEntry = pLib->GetGirderEntry(strGirderName);

   const ConfigStrandFillVector& rStraightFillVec( rconfig.GetStrandFill(pgsTypes::Straight) );
   const ConfigStrandFillVector& rHarpedFillVec(   rconfig.GetStrandFill(pgsTypes::Harped) );

   GridIndexType maxStraightGrid = pGdrEntry->GetNumStraightStrandCoordinates();
   GridIndexType maxHarpedGrid = pGdrEntry->GetNumHarpedStrandCoordinates();
   ATLASSERT(maxStraightGrid == rStraightFillVec.size()); // this function won't play well without this
   ATLASSERT(maxHarpedGrid == rHarpedFillVec.size());

   GridIndexType maxPermGrid = pGdrEntry->GetPermanentStrandGridSize();
   // Loop over all available permanent strands and add index for strType if it's filled
   StrandIndexType permIdc = 0;
   for (GridIndexType idxPermGrid=0; idxPermGrid<maxPermGrid; idxPermGrid++)
   {
      GridIndexType localGridIdx;
      GirderLibraryEntry::psStrandType type;
      pGdrEntry->GetGridPositionFromPermStrandGrid(idxPermGrid, &type, &localGridIdx);

      if (type==pgsTypes::Straight)
      {
         // If filled, use count from fill vec, otherwise use x coordinate
         StrandIndexType strCnt;
         bool isFilled = rStraightFillVec[localGridIdx] > 0;
         if (isFilled)
         {
            strCnt = rStraightFillVec[localGridIdx];
         }
         else
         {
            Float64 start_x, start_y, end_x, end_y;
            bool can_debond;
            pGdrEntry->GetStraightStrandCoordinates(localGridIdx, &start_x, &start_y, &end_x, &end_y, &can_debond);
            strCnt = (start_x > 0.0 || end_x > 0.0) ? 2 : 1;
         }

         permIdc += strCnt;

         // if strand is filled, add its permanent index to the collection
         if (isFilled && strType==pgsTypes::Straight)
         {
            if (strCnt==1)
            {
               permStrands->Add(permIdc-1);
            }
            else
            {
               permStrands->Add(permIdc-2);
               permStrands->Add(permIdc-1);
            }
         }
      }
      else
      {
         ATLASSERT(type==pgsTypes::Harped);
         // If filled, use count from fill vec, otherwise use x coordinate
         StrandIndexType strCnt;
         bool isFilled = rHarpedFillVec[localGridIdx] > 0;
         if (isFilled)
         {
            strCnt = rHarpedFillVec[localGridIdx];
         }
         else
         {
            Float64 startx, starty, hpx, hpy, endx, endy;
            pGdrEntry->GetHarpedStrandCoordinates(localGridIdx, &startx, &starty, &hpx, &hpy, &endx, &endy);
            strCnt = (startx > 0.0 || hpx > 0.0 || endx > 0.0) ? 2 : 1;
         }

         permIdc += strCnt;

         // if strand is filled, add its permanent index to the collection
         if (isFilled && strType==pgsTypes::Harped)
         {
            permStrands->Add(permIdc-1);

            if (strCnt==2)
            {
               permStrands->Add(permIdc-2);
            }
         }
      }
   }

   permStrands.CopyTo(permIndices);
   return true;
}

bool CBridgeAgentImp::IsValidNumStrands(const CSegmentKey& segmentKey,pgsTypes::StrandType type,StrandIndexType curNum)
{
   VALIDATE( GIRDER );

   CComPtr<IPrecastGirder> girder;
   GetGirder(segmentKey,&girder);

   VARIANT_BOOL bIsValid;
   HRESULT hr = E_FAIL;
   switch( type )
   {
   case pgsTypes::Straight:
      hr = m_StrandFillers[segmentKey].IsValidNumStraightStrands(girder, curNum, &bIsValid);
      break;

   case pgsTypes::Harped:
      hr = m_StrandFillers[segmentKey].IsValidNumHarpedStrands(girder, curNum, &bIsValid);
      break;

   case pgsTypes::Temporary:
      hr = m_StrandFillers[segmentKey].IsValidNumTemporaryStrands(girder, curNum, &bIsValid);
      break;
   }
   ATLASSERT(SUCCEEDED(hr));

   return bIsValid == VARIANT_TRUE ? true : false;
}

bool CBridgeAgentImp::IsValidNumStrands(LPCTSTR strGirderName,pgsTypes::StrandType type,StrandIndexType curNum)
{
   GET_IFACE(ILibrary,pLib);
   const GirderLibraryEntry* pGdrEntry = pLib->GetGirderEntry(strGirderName);

   CStrandFiller strandFiller;
   strandFiller.Init(pGdrEntry);

   VARIANT_BOOL isvalid(VARIANT_FALSE);
   HRESULT hr(E_FAIL);
   switch( type )
   {
   case pgsTypes::Straight:
      {
         CComPtr<IStrandGrid> startGrid, endGrid;
         startGrid.CoCreateInstance(CLSID_StrandGrid);
         endGrid.CoCreateInstance(CLSID_StrandGrid);
         pGdrEntry->ConfigureStraightStrandGrid(startGrid,endGrid);

         CComQIPtr<IStrandGridFiller> gridFiller(startGrid);

         hr = strandFiller.IsValidNumStraightStrands(gridFiller,curNum,&isvalid);
      }
      break;

   case pgsTypes::Harped:
      {
         CComPtr<IStrandGrid> startGrid, startHPGrid, endHPGrid, endGrid;
         startGrid.CoCreateInstance(CLSID_StrandGrid);
         startHPGrid.CoCreateInstance(CLSID_StrandGrid);
         endHPGrid.CoCreateInstance(CLSID_StrandGrid);
         endGrid.CoCreateInstance(CLSID_StrandGrid);
         pGdrEntry->ConfigureHarpedStrandGrids(startGrid,startHPGrid,endHPGrid,endGrid);

         CComQIPtr<IStrandGridFiller> startFiller(startGrid);
         CComQIPtr<IStrandGridFiller> endFiller(endGrid);

         hr = strandFiller.IsValidNumHarpedStrands(pGdrEntry->OddNumberOfHarpedStrands(),startFiller,endFiller,curNum,&isvalid);
      }
      break;

   case pgsTypes::Temporary:
      {
         CComPtr<IStrandGrid> startGrid, endGrid;
         startGrid.CoCreateInstance(CLSID_StrandGrid);
         endGrid.CoCreateInstance(CLSID_StrandGrid);
         pGdrEntry->ConfigureTemporaryStrandGrid(startGrid,endGrid);

         CComQIPtr<IStrandGridFiller> gridFiller(startGrid);

         hr = strandFiller.IsValidNumTemporaryStrands(gridFiller,curNum,&isvalid);
      }
      break;
   }
   ATLASSERT(SUCCEEDED(hr));

   return isvalid==VARIANT_TRUE;
}

StrandIndexType CBridgeAgentImp::GetNextNumStraightStrands(const CSegmentKey& segmentKey,StrandIndexType curNum)
{
   VALIDATE( GIRDER );

   CComPtr<IPrecastGirder> girder;
   GetGirder(segmentKey,&girder);

   StrandIndexType nextNum;
   HRESULT hr = m_StrandFillers[segmentKey].GetNextNumberOfStraightStrands(girder, curNum, &nextNum);
   ATLASSERT(SUCCEEDED(hr));

   return nextNum;
}

StrandIndexType CBridgeAgentImp::GetNextNumStraightStrands(LPCTSTR strGirderName,StrandIndexType curNum)
{
   GET_IFACE(ILibrary,pLib);
   const GirderLibraryEntry* pGdrEntry = pLib->GetGirderEntry(strGirderName);

   CStrandFiller strandFiller;
   strandFiller.Init(pGdrEntry);

   CComPtr<IStrandGrid> startGrid, endGrid;
   startGrid.CoCreateInstance(CLSID_StrandGrid);
   endGrid.CoCreateInstance(CLSID_StrandGrid);
   pGdrEntry->ConfigureStraightStrandGrid(startGrid,endGrid);

   CComQIPtr<IStrandGridFiller> gridFiller(startGrid);

   StrandIndexType nextNum;
   strandFiller.GetNextNumberOfStraightStrands(gridFiller,curNum,&nextNum);

   return nextNum;
}

StrandIndexType CBridgeAgentImp::GetNextNumHarpedStrands(const CSegmentKey& segmentKey,StrandIndexType curNum)
{
   VALIDATE( GIRDER );

   CComPtr<IPrecastGirder> girder;
   GetGirder(segmentKey,&girder);

   StrandIndexType nextNum;
   HRESULT hr = m_StrandFillers[segmentKey].GetNextNumberOfHarpedStrands(girder, curNum, &nextNum);
   ATLASSERT(SUCCEEDED(hr));

   return nextNum;
}

StrandIndexType CBridgeAgentImp::GetNextNumHarpedStrands(LPCTSTR strGirderName,StrandIndexType curNum)
{
   GET_IFACE(ILibrary,pLib);
   const GirderLibraryEntry* pGdrEntry = pLib->GetGirderEntry(strGirderName);

   CStrandFiller strandFiller;
   strandFiller.Init(pGdrEntry);


   CComPtr<IStrandGrid> startGrid, startHPGrid, endHPGrid, endGrid;
   startGrid.CoCreateInstance(CLSID_StrandGrid);
   startHPGrid.CoCreateInstance(CLSID_StrandGrid);
   endHPGrid.CoCreateInstance(CLSID_StrandGrid);
   endGrid.CoCreateInstance(CLSID_StrandGrid);
   pGdrEntry->ConfigureHarpedStrandGrids(startGrid,startHPGrid,endHPGrid,endGrid);

   CComQIPtr<IStrandGridFiller> gridFiller(startGrid);

   StrandIndexType nextNum;
   strandFiller.GetNextNumberOfHarpedStrands(pGdrEntry->OddNumberOfHarpedStrands(),gridFiller,curNum,&nextNum);

   return nextNum;
}

StrandIndexType CBridgeAgentImp::GetNextNumTempStrands(const CSegmentKey& segmentKey,StrandIndexType curNum)
{
   VALIDATE( GIRDER );

   CComPtr<IPrecastGirder> girder;
   GetGirder(segmentKey,&girder);

   StrandIndexType nextNum;
   HRESULT hr = m_StrandFillers[segmentKey].GetNextNumberOfTemporaryStrands(girder, curNum, &nextNum);
   ATLASSERT(SUCCEEDED(hr));

   return nextNum;
}

StrandIndexType CBridgeAgentImp::GetNextNumTempStrands(LPCTSTR strGirderName,StrandIndexType curNum)
{
   GET_IFACE(ILibrary,pLib);
   const GirderLibraryEntry* pGdrEntry = pLib->GetGirderEntry(strGirderName);

   CStrandFiller strandFiller;
   strandFiller.Init(pGdrEntry);

   CComPtr<IStrandGrid> startGrid, endGrid;
   startGrid.CoCreateInstance(CLSID_StrandGrid);
   endGrid.CoCreateInstance(CLSID_StrandGrid);
   pGdrEntry->ConfigureTemporaryStrandGrid(startGrid,endGrid);

   CComQIPtr<IStrandGridFiller> gridFiller(startGrid);

   StrandIndexType nextNum;
   strandFiller.GetNextNumberOfTemporaryStrands(gridFiller,curNum,&nextNum);

   return nextNum;
}

StrandIndexType CBridgeAgentImp::GetPrevNumStraightStrands(const CSegmentKey& segmentKey,StrandIndexType curNum)
{
   VALIDATE( GIRDER );

   CComPtr<IPrecastGirder> girder;
   GetGirder(segmentKey,&girder);

   StrandIndexType nextNum;
   HRESULT hr = m_StrandFillers[segmentKey].GetPreviousNumberOfStraightStrands(girder, curNum, &nextNum);
   ATLASSERT(SUCCEEDED(hr));

   return nextNum;
}

StrandIndexType CBridgeAgentImp::GetPrevNumStraightStrands(LPCTSTR strGirderName,StrandIndexType curNum)
{
   GET_IFACE(ILibrary,pLib);
   const GirderLibraryEntry* pGdrEntry = pLib->GetGirderEntry(strGirderName);

   CStrandFiller strandFiller;
   strandFiller.Init(pGdrEntry);

   CComPtr<IStrandGrid> startGrid, endGrid;
   startGrid.CoCreateInstance(CLSID_StrandGrid);
   endGrid.CoCreateInstance(CLSID_StrandGrid);
   pGdrEntry->ConfigureStraightStrandGrid(startGrid,endGrid);

   CComQIPtr<IStrandGridFiller> gridFiller(startGrid);

   StrandIndexType prevNum;
   strandFiller.GetPrevNumberOfStraightStrands(gridFiller,curNum,&prevNum);

   return prevNum;
}

StrandIndexType CBridgeAgentImp::GetPrevNumHarpedStrands(const CSegmentKey& segmentKey,StrandIndexType curNum)
{
   VALIDATE( GIRDER );

   CComPtr<IPrecastGirder> girder;
   GetGirder(segmentKey,&girder);

   StrandIndexType nextNum;
   HRESULT hr = m_StrandFillers[segmentKey].GetPreviousNumberOfHarpedStrands(girder, curNum, &nextNum);
   ATLASSERT(SUCCEEDED(hr));

   return nextNum;
}

StrandIndexType CBridgeAgentImp::GetPrevNumHarpedStrands(LPCTSTR strGirderName,StrandIndexType curNum)
{
   GET_IFACE(ILibrary,pLib);
   const GirderLibraryEntry* pGdrEntry = pLib->GetGirderEntry(strGirderName);

   CStrandFiller strandFiller;
   strandFiller.Init(pGdrEntry);

   CComPtr<IStrandGrid> startGrid, startHPGrid, endHPGrid, endGrid;
   startGrid.CoCreateInstance(CLSID_StrandGrid);
   startHPGrid.CoCreateInstance(CLSID_StrandGrid);
   endHPGrid.CoCreateInstance(CLSID_StrandGrid);
   endGrid.CoCreateInstance(CLSID_StrandGrid);
   pGdrEntry->ConfigureHarpedStrandGrids(startGrid,startHPGrid,endHPGrid,endGrid);

   CComQIPtr<IStrandGridFiller> gridFiller(startGrid);

   StrandIndexType prevNum;
   strandFiller.GetPrevNumberOfHarpedStrands(pGdrEntry->OddNumberOfHarpedStrands(),gridFiller,curNum,&prevNum);

   return prevNum;
}

StrandIndexType CBridgeAgentImp::GetPrevNumTempStrands(const CSegmentKey& segmentKey,StrandIndexType curNum)
{
   VALIDATE( GIRDER );

   CComPtr<IPrecastGirder> girder;
   GetGirder(segmentKey,&girder);

   StrandIndexType nextNum;
   HRESULT hr = m_StrandFillers[segmentKey].GetPreviousNumberOfTemporaryStrands(girder, curNum, &nextNum);
   ATLASSERT(SUCCEEDED(hr));

   return nextNum;
}

StrandIndexType CBridgeAgentImp::GetPrevNumTempStrands(LPCTSTR strGirderName,StrandIndexType curNum)
{
   GET_IFACE(ILibrary,pLib);
   const GirderLibraryEntry* pGdrEntry = pLib->GetGirderEntry(strGirderName);

   CStrandFiller strandFiller;
   strandFiller.Init(pGdrEntry);

   CComPtr<IStrandGrid> startGrid, endGrid;
   startGrid.CoCreateInstance(CLSID_StrandGrid);
   endGrid.CoCreateInstance(CLSID_StrandGrid);
   pGdrEntry->ConfigureTemporaryStrandGrid(startGrid,endGrid);

   CComQIPtr<IStrandGridFiller> gridFiller(startGrid);

   StrandIndexType prevNum;
   strandFiller.GetPrevNumberOfTemporaryStrands(gridFiller,curNum,&prevNum);

   return prevNum;
}

Float64 CBridgeAgentImp::GetCutLocation(const pgsPointOfInterest& poi)
{
   GET_IFACE(IBridge, pBridge);

   const CSegmentKey& segmentKey = poi.GetSegmentKey();

   Float64 dist_from_start_of_girder = poi.GetDistFromStart();

   Float64 start_brg_offset = pBridge->GetSegmentStartBearingOffset(segmentKey);
   Float64 start_end_size   = pBridge->GetSegmentStartEndDistance(segmentKey);
   Float64 dist_from_start_pier = dist_from_start_of_girder - start_end_size + start_brg_offset;

   return dist_from_start_pier;
}

void CBridgeAgentImp::GetSegmentShapeDirect(const pgsPointOfInterest& poi,IShape** ppShape)
{
   CComPtr<ISection> section;

   const CSegmentKey& segmentKey = poi.GetSegmentKey();
   CComPtr<ISegment> segment;
   GetSegment(segmentKey,&segment);

   Float64 distFromStartOfGirder = poi.GetDistFromStart();
   segment->get_PrimaryShape(distFromStartOfGirder,ppShape);
}

BarSize CBridgeAgentImp::GetBarSize(matRebar::Size size)
{
   switch(size)
   {
   case matRebar::bs3:  return bs3;
   case matRebar::bs4:  return bs4;
   case matRebar::bs5:  return bs5;
   case matRebar::bs6:  return bs6;
   case matRebar::bs7:  return bs7;
   case matRebar::bs8:  return bs8;
   case matRebar::bs9:  return bs9;
   case matRebar::bs10: return bs10;
   case matRebar::bs11: return bs11;
   case matRebar::bs14: return bs14;
   case matRebar::bs18: return bs18;
   default:
      ATLASSERT(false); // should not get here
   }
   
   ATLASSERT(false); // should not get here
   return bs3;
}

RebarGrade CBridgeAgentImp::GetRebarGrade(matRebar::Grade grade)
{
   RebarGrade matGrade;
   switch(grade)
   {
   case matRebar::Grade40: matGrade = Grade40; break;
   case matRebar::Grade60: matGrade = Grade60; break;
   case matRebar::Grade75: matGrade = Grade75; break;
   case matRebar::Grade80: matGrade = Grade80; break;
   case matRebar::Grade100: matGrade = Grade100; break;
   default:
      ATLASSERT(false);
   }

#if defined _DEBUG
      if ( matGrade == Grade100 )
      {
         ATLASSERT(lrfdVersionMgr::SixthEditionWith2013Interims <= lrfdVersionMgr::GetVersion());
      }
#endif

   return matGrade;
}

MaterialSpec CBridgeAgentImp::GetRebarSpecification(matRebar::Type type)
{
   return (type == matRebar::A615 ? msA615 : (type == matRebar::A706 ? msA706 : msA1035));
}

Float64 CBridgeAgentImp::GetAsTensionSideOfGirder(const pgsPointOfInterest& poi,bool bDevAdjust,bool bTensionTop)
{
   const CSegmentKey& segmentKey = poi.GetSegmentKey();

   CComPtr<IRebarSection> rebar_section;
   GetRebars(poi,&rebar_section);

   CComPtr<IEnumRebarSectionItem> enum_items;
   rebar_section->get__EnumRebarSectionItem(&enum_items);

   Float64 cl = GetHalfElevation(poi);

   Float64 As = 0;

   CComPtr<IRebarSectionItem> item;
   while ( enum_items->Next(1,&item,NULL) != S_FALSE )
   {
      CComPtr<IRebar> rebar;
      item->get_Rebar(&rebar);

      Float64 as;
      rebar->get_NominalArea(&as);

      CComPtr<IPoint2d> location;
      item->get_Location(&location);

      Float64 y;
      location->get_Y(&y);

      // include bar if tension on top and y is greater than centerline or if
      // tension is not top (tension is bottom) and y is less than centerline
      bool bIncludeBar = ( (bTensionTop && cl < y) || (!bTensionTop && y <= cl) ) ? true : false;

      if ( bIncludeBar )
      {
         Float64 fra = 1.0;
         if ( bDevAdjust )
         {
            fra = GetDevLengthFactor(segmentKey,item);
         }

         As += fra*as;
      }

      item.Release();
   }

   return As;
}

Float64 CBridgeAgentImp::GetApsTensionSide(const pgsPointOfInterest& poi,DevelopmentAdjustmentType devAdjust,bool bTensionTop)
{
   GDRCONFIG dummy_config;
   return GetApsTensionSide(poi,false,dummy_config,devAdjust,bTensionTop);
}

Float64 CBridgeAgentImp::GetApsTensionSide(const pgsPointOfInterest& poi, const GDRCONFIG& config,DevelopmentAdjustmentType devAdjust,bool bTensionTop)
{
   return GetApsTensionSide(poi,true,config,devAdjust,bTensionTop);
}

Float64 CBridgeAgentImp::GetApsTensionSide(const pgsPointOfInterest& poi,bool bUseConfig,const GDRCONFIG& config,DevelopmentAdjustmentType devAdjust,bool bTensionTop)
{
   VALIDATE( GIRDER );

   GET_IFACE(IPretensionForce,pPSForce);
   GET_IFACE(IStrandGeometry,pStrandGeom);

   Float64 cl = GetHalfElevation(poi);
   Float64 Aps = 0.0;

   const CSegmentKey& segmentKey = poi.GetSegmentKey();

   CComPtr<IPrecastGirder> girder;
   GetGirder(segmentKey,&girder);

   const matPsStrand* pStrand = GetStrandMaterial(segmentKey,pgsTypes::Straight);
   Float64 aps = pStrand->GetNominalArea();

   Float64 dist_from_start = poi.GetDistFromStart();
   Float64 segment_length = GetSegmentLength(segmentKey);

   // Only use approximate bond method if poi is in mid-section of beam (within CSS's).
   Float64 min_dist_from_ends = min(dist_from_start, segment_length-dist_from_start);
   bool use_approximate = devAdjust==dlaApproximate && min_dist_from_ends > cl*3.0; // Factor here is balance between performance
                                                                                    // and accuracy.

   // For approximate development length adjustment, take development length information at mid span and use for entire girder
   // adjusted for distance to ends of strands
   STRANDDEVLENGTHDETAILS dla_det;
   if(use_approximate)
   {
      std::vector<pgsPointOfInterest> vPoi( GetPointsOfInterest(segmentKey,POI_ERECTED_SEGMENT | POI_MIDSPAN,POIFIND_AND) );
      ATLASSERT(vPoi.size() == 1);
      const pgsPointOfInterest& rpoi = vPoi.front();

      if ( bUseConfig )
         dla_det = pPSForce->GetDevLengthDetails(rpoi, config, false);
      else
         dla_det = pPSForce->GetDevLengthDetails(rpoi, false);
   }

   // Get straight strand locations
   CComPtr<IPoint2dCollection> strand_points;
   if( bUseConfig )
   {
      CIndexArrayWrapper strand_fill(config.PrestressConfig.GetStrandFill(pgsTypes::Straight));
      girder->get_StraightStrandPositionsEx(dist_from_start,&strand_fill,&strand_points);
   }
   else
   {
      girder->get_StraightStrandPositions(dist_from_start,&strand_points);
   }

   StrandIndexType Ns;
   strand_points->get_Count(&Ns);

   StrandIndexType strandIdx;
   for(strandIdx = 0; strandIdx < Ns; strandIdx++)
   {
      CComPtr<IPoint2d> strand_point;
      strand_points->get_Item(strandIdx, &strand_point);

      Float64 y;
      strand_point->get_Y(&y);

      // include bar if tension on top and y is greater than centerline or if
      // tension is not top (tension is bottom) and y is less than centerline
      bool bIncludeBar = ( (bTensionTop && cl < y) || (!bTensionTop && y <= cl) ) ? true : false;

      if ( bIncludeBar )
      {
         Float64 debond_factor;
         if(devAdjust==dlaNone)
         {
            debond_factor = 1.0;
         }
         else if(use_approximate)
         {
            // Use mid-span development length details to approximate debond factor
            // determine minimum bonded length from poi
            Float64 bond_start, bond_end;
            bool bDebonded;
            if (bUseConfig)
               bDebonded = pStrandGeom->IsStrandDebonded(segmentKey,strandIdx,pgsTypes::Straight,config,&bond_start,&bond_end);
            else
               bDebonded = pStrandGeom->IsStrandDebonded(segmentKey,strandIdx,pgsTypes::Straight,&bond_start,&bond_end);

            Float64 left_bonded_length, right_bonded_length;
            if ( bDebonded )
            {
               // measure bonded length
               left_bonded_length = dist_from_start - bond_start;
               right_bonded_length = bond_end - dist_from_start;
            }
            else
            {
               // no debonding, bond length is to ends of girder
               left_bonded_length  = dist_from_start;
               right_bonded_length = segment_length - dist_from_start;
            }

            Float64 bonded_length = min(left_bonded_length, right_bonded_length);

            debond_factor = GetDevLengthAdjustment(bonded_length, dla_det.ld, dla_det.lt, dla_det.fps, dla_det.fpe);
         }
         else
         {
            // Full adjustment for development
            if ( bUseConfig )
               debond_factor = pPSForce->GetStrandBondFactor(poi,config,strandIdx,pgsTypes::Straight);
            else
               debond_factor = pPSForce->GetStrandBondFactor(poi,strandIdx,pgsTypes::Straight);
         }

         Aps += debond_factor*aps;
      }
   }

   // harped strands
   pStrand = GetStrandMaterial(segmentKey,pgsTypes::Harped);
   aps = pStrand->GetNominalArea();

   StrandIndexType Nh = (bUseConfig ? config.PrestressConfig.GetNStrands(pgsTypes::Harped) : pStrandGeom->GetNumStrands(segmentKey,pgsTypes::Harped));

   strand_points.Release();
   if(bUseConfig)
   {
      CIndexArrayWrapper strand_fill(config.PrestressConfig.GetStrandFill(pgsTypes::Harped));

      // save and restore precast girders shift values, before/after geting point locations
      Float64 t_end_shift, t_hp_shift;
      girder->get_HarpedStrandAdjustmentEnd(&t_end_shift);
      girder->get_HarpedStrandAdjustmentHP(&t_hp_shift);

      girder->put_HarpedStrandAdjustmentEnd(config.PrestressConfig.EndOffset);
      girder->put_HarpedStrandAdjustmentHP(config.PrestressConfig.HpOffset);

      girder->get_HarpedStrandPositionsEx(dist_from_start, &strand_fill, &strand_points);

      girder->put_HarpedStrandAdjustmentEnd(t_end_shift);
      girder->put_HarpedStrandAdjustmentHP(t_hp_shift);
   }
   else
   {
      girder->get_HarpedStrandPositions(dist_from_start,&strand_points);
   }

   StrandIndexType nstst; // reality test
   strand_points->get_Count(&nstst);
   ATLASSERT(nstst==Nh);

   for(strandIdx = 0; strandIdx < Nh; strandIdx++)
   {
      CComPtr<IPoint2d> strand_point;
      strand_points->get_Item(strandIdx, &strand_point);

      Float64 y;
      strand_point->get_Y(&y);

      // include bar if tension on top and y is greater than centerline or if
      // tension is not top (tension is bottom) and y is less than centerline
      bool bIncludeBar = ( (bTensionTop && cl < y) || (!bTensionTop && y <= cl) ) ? true : false;

      if ( bIncludeBar )
      {
         Float64 debond_factor = 1.;
         bool use_one = use_approximate || devAdjust==dlaNone;
         if ( bUseConfig )
            debond_factor = use_one ? 1.0 : pPSForce->GetStrandBondFactor(poi,config,strandIdx,pgsTypes::Harped);
         else
            debond_factor = use_one ? 1.0 : pPSForce->GetStrandBondFactor(poi,strandIdx,pgsTypes::Harped);

         Aps += debond_factor*aps;
      }
   }

   return Aps;
}

Float64 CBridgeAgentImp::GetAsDeckMats(const pgsPointOfInterest& poi,ILongRebarGeometry::DeckRebarType drt,bool bTopMat,bool bBottomMat)
{
   Float64 As, Yb;
   GetDeckMatData(poi,drt,bTopMat,bBottomMat,&As,&Yb);
   return As;
}

Float64 CBridgeAgentImp::GetLocationDeckMats(const pgsPointOfInterest& poi,ILongRebarGeometry::DeckRebarType drt,bool bTopMat,bool bBottomMat)
{
   Float64 As, Yb;
   GetDeckMatData(poi,drt,bTopMat,bBottomMat,&As,&Yb);
   return Yb;
}

void CBridgeAgentImp::GetDeckMatData(const pgsPointOfInterest& poi,ILongRebarGeometry::DeckRebarType drt,bool bTopMat,bool bBottomMat,Float64* pAs,Float64* pYb)
{
   // computes the area of the deck rebar mats and their location (cg) measured from the bottom of the deck.
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CDeckDescription2* pDeck = pBridgeDesc->GetDeckDescription();
   const CDeckRebarData& rebarData = pDeck->DeckRebarData;

   Float64 tSlab = GetGrossSlabDepth(poi);
   Float64 Weff = GetEffectiveFlangeWidth(poi);

   Float64 topCover    = rebarData.TopCover;
   Float64 bottomCover = rebarData.BottomCover;

   //
   // full length reinforcement
   //
   const matRebar* pBar;
   Float64 As_Top    = 0;
   Float64 As_Bottom = 0;
   Float64 YbAs_Top    = 0;
   Float64 YbAs_Bottom = 0;

   if ( drt == ILongRebarGeometry::Primary || drt == ILongRebarGeometry::All )
   {
      // top mat
      if ( bTopMat )
      {
         if ( rebarData.TopRebarSize != matRebar::bsNone )
         {
            pBar = lrfdRebarPool::GetInstance()->GetRebar( rebarData.TopRebarType, rebarData.TopRebarGrade, rebarData.TopRebarSize );
            Float64 db = pBar->GetNominalDimension();
            Float64 Yb = tSlab - topCover - db/2;
            IndexType nBars = IndexType(Weff/rebarData.TopSpacing) + 1;
            Float64 As = nBars*pBar->GetNominalArea()/Weff;
            YbAs_Top += Yb*As;
            As_Top   += As;
         }

         YbAs_Top += rebarData.TopLumpSum*(tSlab - topCover);
         As_Top   += rebarData.TopLumpSum;
      }

      // bottom mat
      if ( bBottomMat )
      {
         if ( rebarData.BottomRebarSize != matRebar::bsNone )
         {
            pBar = lrfdRebarPool::GetInstance()->GetRebar( rebarData.BottomRebarType, rebarData.BottomRebarGrade, rebarData.BottomRebarSize );
            Float64 db = pBar->GetNominalDimension();
            Float64 Yb = bottomCover + db/2;
            IndexType nBars = IndexType(Weff/rebarData.BottomSpacing) + 1;
            Float64 As = nBars*pBar->GetNominalArea()/Weff;
            YbAs_Bottom += Yb*As;
            As_Bottom   += As;
         }

         YbAs_Bottom += bottomCover*rebarData.BottomLumpSum;
         As_Bottom   += rebarData.BottomLumpSum;
      }
   }

   if ( drt == ILongRebarGeometry::Supplemental || drt == ILongRebarGeometry::All )
   {
      //
      // negative moment reinforcement
      //
      const CSegmentKey& segmentKey = poi.GetSegmentKey();

      Float64 start_end_size = GetSegmentStartEndDistance(segmentKey);
      Float64 end_end_size   = GetSegmentEndEndDistance(segmentKey);
      Float64 start_offset   = GetSegmentStartBearingOffset(segmentKey);
      Float64 end_offset     = GetSegmentEndBearingOffset(segmentKey);

      Float64 dist_from_cl_prev_pier = poi.GetDistFromStart() + start_offset - start_end_size;
      Float64 dist_to_cl_next_pier   = GetSegmentLength(segmentKey) - poi.GetDistFromStart() + end_offset - end_end_size;

      PierIndexType prev_pier = GetGenericBridgePierIndex(segmentKey,pgsTypes::metStart);
      PierIndexType next_pier = prev_pier + 1;

      std::vector<CDeckRebarData::NegMomentRebarData>::const_iterator iter(rebarData.NegMomentRebar.begin());
      std::vector<CDeckRebarData::NegMomentRebarData>::const_iterator end(rebarData.NegMomentRebar.end());
      for ( ; iter != end; iter++ )
      {
         const CDeckRebarData::NegMomentRebarData& nmRebarData = *iter;

         if ( (bTopMat    && (nmRebarData.Mat == CDeckRebarData::TopMat)) ||
              (bBottomMat && (nmRebarData.Mat == CDeckRebarData::BottomMat)) )
         {
            if ( ( nmRebarData.PierIdx == prev_pier && IsLE(dist_from_cl_prev_pier,nmRebarData.RightCutoff) ) ||
                 ( nmRebarData.PierIdx == next_pier && IsLE(dist_to_cl_next_pier,  nmRebarData.LeftCutoff)  ) )
            {
               if ( nmRebarData.RebarSize != matRebar::bsNone )
               {
/*
                  pBar = lrfdRebarPool::GetInstance()->GetRebar( nmRebarData.RebarType, nmRebarData.RebarGrade, nmRebarData.RebarSize);
                  Float64 db = pBar->GetNominalDimension();
                  IndexType nBars = IndexType(Weff/nmRebarData.Spacing) + 1;
                  Float64 As = nBars*pBar->GetNominalArea()/Weff;

                  if (nmRebarData.Mat == CDeckRebarData::TopMat)
                  {
                     Float64 Yb = tSlab - topCover - db/2;
                     YbAs_Top += Yb*As;
                     As_Top   += As;
                  }
                  else
                  {
                     Float64 Yb = bottomCover + db/2;
                     YbAs_Bottom += Yb*As;
                     As_Bottom   += As;
                  }
*/
                  // Explicit rebar. Reduce area for development if needed.
                  pBar = lrfdRebarPool::GetInstance()->GetRebar( nmRebarData.RebarType, nmRebarData.RebarGrade, nmRebarData.RebarSize);

                  Float64 As = pBar->GetNominalArea()/nmRebarData.Spacing;

                  // Get development length of bar
                  // remove metric from bar name
                  std::_tstring barst(pBar->GetName());
                  std::size_t sit = barst.find(_T(" "));
                  if (sit != std::_tstring::npos)
                     barst.erase(sit, barst.size()-1);

                  CComBSTR barname(barst.c_str());
                  REBARDEVLENGTHDETAILS devdet = GetRebarDevelopmentLengthDetails(barname, pBar->GetNominalArea(), 
                                                                                  pBar->GetNominalDimension(), pBar->GetYieldStrength(), 
                                                                                  pDeck->Concrete.Type, pDeck->Concrete.Fc, 
                                                                                  pDeck->Concrete.bHasFct, pDeck->Concrete.Fct);
                  Float64 ld = devdet.ldb;

                  Float64 bar_cutoff; // cutoff length from POI
                  if ( nmRebarData.PierIdx == prev_pier )
                  {
                     bar_cutoff = nmRebarData.RightCutoff - dist_from_cl_prev_pier;
                  }
                  else
                  {
                     bar_cutoff = nmRebarData.LeftCutoff - dist_to_cl_next_pier;
                  }

                  ATLASSERT(bar_cutoff >= -TOLERANCE);

                  // Reduce As for development if needed
                  if (bar_cutoff < ld)
                  {
                     As = As * bar_cutoff / ld;
                  }

                  if (nmRebarData.Mat == CDeckRebarData::TopMat)
                     As_Top += As;
                  else
                     As_Bottom += As;
               }

               // Lump sum bars are not adjusted for development
               if (nmRebarData.Mat == CDeckRebarData::TopMat)
               {
                  Float64 Yb = tSlab - topCover;
                  YbAs_Top += Yb*nmRebarData.LumpSum;
                  As_Top   += nmRebarData.LumpSum;
               }
               else
               {
                  YbAs_Bottom += bottomCover*nmRebarData.LumpSum;
                  As_Bottom   += nmRebarData.LumpSum;
               }
            }
         }
      }
   }

   *pAs = (As_Bottom + As_Top)*Weff;
   *pYb = (IsZero(*pAs) ? 0 : (YbAs_Bottom + YbAs_Top)/(As_Bottom + As_Top));
}

void CBridgeAgentImp::GetShapeProperties(IntervalIndexType intervalIdx,const pgsPointOfInterest& poi,Float64 Ecgdr,IShapeProperties** ppShapeProps)
{
   GET_IFACE(ILibrary,       pLib);
   GET_IFACE(ISpecification, pSpec);
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( pSpec->GetSpecification().c_str() );
   pgsTypes::SectionPropertyType sectPropType = (pgsTypes::SectionPropertyType)(pSpecEntry->GetSectionPropertyMode());

   GetShapeProperties(sectPropType,intervalIdx,poi,Ecgdr,ppShapeProps);
}

void CBridgeAgentImp::GetShapeProperties(pgsTypes::SectionPropertyType sectPropType,IntervalIndexType intervalIdx,const pgsPointOfInterest& poi,Float64 Ecgdr,IShapeProperties** ppShapeProps)
{
   // We are going to be messing with the secant modulus... create a clone
   // that we can throw away when we are done with it
   SectProp& props = GetSectionProperties(intervalIdx,poi,sectPropType);
   CComPtr<ISection> s;
   props.Section->Clone(&s);

   // Assuming section is a Composite section and beam is exactly the first piece
   CComQIPtr<ICompositeSectionEx> cmpsection(s);
   CollectionIndexType nItems;
   cmpsection->get_Count(&nItems);

   if ( 0 < nItems )
   {
      CComPtr<ICompositeSectionItemEx> csi;
      cmpsection->get_Item(0,&csi); // this should be the beam

      Float64 Ec;
      csi->get_Efg(&Ec);

      //Update E for the girder
      csi->put_Efg(Ecgdr);

      // change background materials
      for ( CollectionIndexType i = 1; i < nItems; i++ )
      {
         csi.Release();
         cmpsection->get_Item(i,&csi);

         Float64 E;
         csi->get_Ebg(&E);
         if ( IsEqual(Ec,E) )
         {
            csi->put_Ebg(Ecgdr);
         }
      }
   }

   CComPtr<IElasticProperties> eprops;
   s->get_ElasticProperties(&eprops);

   eprops->TransformProperties(Ecgdr,ppShapeProps);
}

void CBridgeAgentImp::GetSlabEdgePoint(Float64 station, IDirection* direction,DirectionType side,IPoint2d** point)
{
   VALIDATE(BRIDGE);

   HRESULT hr = m_BridgeGeometryTool->DeckEdgePoint(m_Bridge,station,direction,side,point);
   ATLASSERT(SUCCEEDED(hr));
}

void CBridgeAgentImp::GetSlabEdgePoint(Float64 station, IDirection* direction,DirectionType side,IPoint3d** point)
{
   VALIDATE(BRIDGE);

   CComPtr<IPoint2d> pnt2d;
   GetSlabEdgePoint(station,direction,side,&pnt2d);

   Float64 x,y;
   pnt2d->Location(&x,&y);

   Float64 normal_station,offset;
   GetStationAndOffset(pnt2d,&normal_station,&offset);

   Float64 elev = GetElevation(normal_station,offset);

   CComPtr<IPoint3d> pnt3d;
   pnt3d.CoCreateInstance(CLSID_Point3d);
   pnt3d->Move(x,y,elev);

   (*point) = pnt3d;
   (*point)->AddRef();
}

SegmentIndexType CBridgeAgentImp::GetSegmentIndex(const CSplicedGirderData* pGirder,Float64 distFromStartOfBridge)
{
   // convert distance from start of bridge to a station so it can be
   // compared with pier and temp support stations
   const CPierData2* pStartPier = pGirder->GetPier(pgsTypes::metStart);
   Float64 station = distFromStartOfBridge + pStartPier->GetStation();

   SegmentIndexType nSegments = pGirder->GetSegmentCount();
   for ( SegmentIndexType segIdx = 0; segIdx < nSegments; segIdx++ )
   {
      const CPrecastSegmentData* pSegment = pGirder->GetSegment(segIdx);
      Float64 startStation,endStation;
      pSegment->GetStations(startStation,endStation);

      if ( ::InRange(startStation,station,endStation) )
         return segIdx;
   }

   return INVALID_INDEX;
}

SpanIndexType CBridgeAgentImp::GetSpanIndex(Float64 distFromStartOfBridge)
{
   if ( distFromStartOfBridge < 0 )
      return INVALID_INDEX;

   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();

   Float64 span_station = pBridgeDesc->GetPier(0)->GetStation() + distFromStartOfBridge;

   PierIndexType nPiers = pBridgeDesc->GetPierCount();
   for ( PierIndexType pierIdx = 0; pierIdx < nPiers; pierIdx++ )
   {
      if ( span_station < pBridgeDesc->GetPier(pierIdx)->GetStation() )
         return (SpanIndexType)(pierIdx-1);
   }

   return INVALID_INDEX;
}

void CBridgeAgentImp::GetGirderLine(const CSegmentKey& segmentKey,IGirderLine** ppGirderLine)
{
   VALIDATE(BRIDGE);
   CComPtr<ISegment> segment;
   GetSegment(segmentKey,&segment);
   segment->get_GirderLine(ppGirderLine);
}

void CBridgeAgentImp::GetSegmentAtPier(PierIndexType pierIdx,const CGirderKey& girderKey,ISegment** ppSegment)
{
   CSegmentKey segmentKey( GetSegmentAtPier(pierIdx,girderKey) );
   ATLASSERT(segmentKey.groupIndex != INVALID_INDEX && segmentKey.girderIndex != INVALID_INDEX && segmentKey.segmentIndex != INVALID_INDEX);
   GetSegment(segmentKey,ppSegment);
}

void CBridgeAgentImp::GetTemporarySupportLine(SupportIndexType tsIdx,IPierLine** ppPierLine)
{
   VALIDATE(BRIDGE);

   CComPtr<IBridgeGeometry> bridgeGeometry;
   m_Bridge->get_BridgeGeometry(&bridgeGeometry);

   PierIDType pierID = ::GetTempSupportLineID(tsIdx);

   bridgeGeometry->FindPierLine(pierID,ppPierLine);
}

void CBridgeAgentImp::GetPierLine(PierIndexType pierIdx,IPierLine** ppPierLine)
{
   VALIDATE(BRIDGE);

   CComPtr<IBridgeGeometry> bridgeGeometry;
   m_Bridge->get_BridgeGeometry(&bridgeGeometry);

   PierIDType pierID = ::GetPierLineID(pierIdx);

   bridgeGeometry->FindPierLine(pierID,ppPierLine);
}

PierIndexType CBridgeAgentImp::GetGenericBridgePierIndex(const CSegmentKey& segmentKey,pgsTypes::MemberEndType endType)
{
   // Returns the index of a pier object in the generic bridge model
   // The pier index can be for a regular pier or a temporary support
   // ONLY USE THIS PIER INDEX FOR ACCESSING PIER OBJECTS IN THE GENERIC BRIDGE MODEL (m_Bridge)
   CComPtr<IGirderLine> girderLine;
   GetGirderLine(segmentKey,&girderLine);

   CComPtr<IPierLine> pierLine;
   if ( endType == pgsTypes::metStart )
      girderLine->get_StartPier(&pierLine);
   else
      girderLine->get_EndPier(&pierLine);

   PierIndexType pierIdx;
   pierLine->get_Index(&pierIdx);
   return pierIdx;
}

const GirderLibraryEntry* CBridgeAgentImp::GetGirderLibraryEntry(const CGirderKey& girderKey)
{
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CGirderGroupData* pGroup = pBridgeDesc->GetGirderGroup(girderKey.groupIndex);
   const CSplicedGirderData* pGirder = pGroup->GetGirder(girderKey.girderIndex);
   return pGirder->GetGirderLibraryEntry();
}

GroupIndexType CBridgeAgentImp::GetGirderGroupAtPier(PierIndexType pierIdx,pgsTypes::PierFaceType pierFace)
{
   // returns the girder group index for the girder group touching the subject pier
   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();

   // Get the pier
   const CPierData2* pPier = pBridgeDesc->GetPier(pierIdx);

   // Get the span on the side of the pier that goes with the specified pier face
   const CSpanData2* pSpan = NULL;
   if ( pierFace == pgsTypes::Back )
   {
      pSpan = pPier->GetPrevSpan();
   }
   else
   {
      pSpan = pPier->GetNextSpan();
   }

   // get the girder group that contains this span
   const CGirderGroupData* pGroup = NULL;
   if ( pSpan )
   {
      pGroup = pBridgeDesc->GetGirderGroup(pSpan);
   }

   // get the group index
   if ( pGroup )
   {
      return pGroup->GetIndex();
   }
   else
   {
      ATLASSERT(false); // should never get here
      return INVALID_INDEX;
   }
}

void CBridgeAgentImp::CreateTendons(const CBridgeDescription2* pBridgeDesc,const CGirderKey& girderKey,ISuperstructureMember* pSSMbr,ITendonCollection** ppTendons)
{
   const CGirderGroupData* pGroup = pBridgeDesc->GetGirderGroup(girderKey.groupIndex);
   const CSplicedGirderData* pGirder = pGroup->GetGirder(girderKey.girderIndex);
   const CPTData* pPTData = pGirder->GetPostTensioning();
   DuctIndexType nDucts = pPTData->GetDuctCount();

   WebIndexType nWebs = GetWebCount(girderKey);

   const matPsStrand* pStrand = pPTData->pStrand;

   GirderIDType gdrID = pGirder->GetID();

   const CTimelineManager* pTimelineMgr = pBridgeDesc->GetTimelineManager();

   CComPtr<ITendonCollection> tendons;
   tendons.CoCreateInstance(CLSID_TendonCollection);
   for ( DuctIndexType ductIdx = 0; ductIdx < nDucts; ductIdx++ )
   {
      const CDuctData* pDuctData = pPTData->GetDuct(ductIdx);
      CComPtr<ITendonCollection> webTendons;

      switch (pDuctData->DuctGeometryType)
      {
      case CDuctGeometry::Parabolic:
         CreateParabolicTendon(girderKey,pSSMbr,pDuctData->ParabolicDuctGeometry,&webTendons);
         break;

      case CDuctGeometry::Linear:
         CreateLinearTendon(girderKey,pSSMbr,pDuctData->LinearDuctGeometry,&webTendons);
         break;

      case CDuctGeometry::Offset:
         CreateOffsetTendon(girderKey,pSSMbr,pDuctData->OffsetDuctGeometry,tendons,&webTendons);
         break;
      }

      IntervalIndexType stressTendonIntervalIdx = GetStressTendonInterval(girderKey,ductIdx);

      CComPtr<IPrestressingStrand> tendonMaterial;
      tendonMaterial.CoCreateInstance(CLSID_PrestressingStrand);
      tendonMaterial->put_Name( CComBSTR(pStrand->GetName().c_str()) );
      tendonMaterial->put_Grade((StrandGrade)pStrand->GetGrade());
      tendonMaterial->put_Type((StrandType)pStrand->GetType());
      tendonMaterial->put_Size((StrandSize)pStrand->GetSize());

      // the tendons aren't part of the cross section when then are installed because
      // they are not yet grouted. assume the tendons are grouted into the section
      // in the interval that follows the interval when they are installed
      tendonMaterial->put_InstallationStage(stressTendonIntervalIdx+1);

      DuctIndexType nTendons; 
      webTendons->get_Count(&nTendons);
      ATLASSERT(nTendons != 0); // should equal number of webs
      for (DuctIndexType tendonIdx = 0; tendonIdx < nTendons; tendonIdx++ )
      {
         CComPtr<ITendon> tendon;
         webTendons->get_Item(tendonIdx,&tendon);

         tendon->put_DuctDiameter( CDuctSize::DuctSizes[pDuctData->Size].Diameter );
         tendon->put_StrandCount(pDuctData->nStrands);
         tendon->putref_Material(tendonMaterial);

#pragma Reminder("UPDATE: need to model duct CG to strand CG offset")
         // need offset in X and Y direction as ducts can be 3D

         tendons->Add(tendon);
      }
   }

   tendons.CopyTo(ppTendons);
}

void CBridgeAgentImp::CreateParabolicTendon(const CGirderKey& girderKey,ISuperstructureMember* pSSMbr,const CParabolicDuctGeometry& ductGeometry,ITendonCollection** ppTendons)
{
   CComPtr<ITendonCollection> tendons;
   tendons.CoCreateInstance(CLSID_TendonCollection);

   PierIndexType startPierIdx = ductGeometry.GetGirder()->GetPierIndex(pgsTypes::metStart);
   PierIndexType endPierIdx   = ductGeometry.GetGirder()->GetPierIndex(pgsTypes::metEnd);
   SpanIndexType startSpanIdx = (SpanIndexType)startPierIdx;
   SpanIndexType endSpanIdx   = (SpanIndexType)(endPierIdx-1);

   // x = left/right position in the girder cross section
   // y = elevation in the girder cross section (0 is at the top of the girder)
   // z = distance along girder

   SegmentIndexType nSegments;
   pSSMbr->get_SegmentCount(&nSegments);

   CComPtr<ISegment> segment;
   pSSMbr->get_Segment(0,&segment);

   CComPtr<IShape> shape;
   segment->get_PrimaryShape(0,&shape);

   CComQIPtr<IGirderSection> section(shape);
   WebIndexType nWebs;
   section->get_WebCount(&nWebs);

   for ( WebIndexType webIdx = 0; webIdx < nWebs; webIdx++ )
   {
      Float64 Ls = 0;

      CComPtr<ITendon> tendon;
      tendon.CoCreateInstance(CLSID_Tendon);

      tendons->Add(tendon);

      Float64 x1 = 0;
      Float64 z1 = 0;


      //
      // Start to first low point
      //

      // start point
      Float64 dist,offset;
      CDuctGeometry::OffsetType offsetType;
      ductGeometry.GetStartPoint(&dist,&offset,&offsetType);

      z1 += dist;
      Float64 y1 = ConvertDuctOffsetToDuctElevation(girderKey,z1,offset,offsetType);

      // low point
      ductGeometry.GetLowPoint(startSpanIdx,&dist,&offset,&offsetType);
      // distance is measured from the left end of the girder

      if ( dist < 0 ) // fraction of the distance between start and high point at first interior pier
      {
         // get the cl-brg to cl-pier length for this girder in the start span
         Float64 L = GetSpanLength(startSpanIdx,girderKey.girderIndex);

         // adjust for the end distance at the start of the girder
         Float64 end_dist = GetSegmentStartEndDistance(CSegmentKey(girderKey.groupIndex,girderKey.girderIndex,0));
         L += end_dist;

         if ( startSpanIdx == endSpanIdx )
         {
            // there is only one span for this tendon... adjust for the end distance at the end of the girder
            end_dist = GetSegmentEndEndDistance(CSegmentKey(girderKey.groupIndex,girderKey.girderIndex,nSegments-1));
            L += end_dist;
         }

         dist *= -L;
      }

      Float64 x2 = x1; // dummy (x1 and x2 are computed from the web plane below)
      Float64 z2 = z1 + dist; // this is the location of the low point
      Float64 y2 = ConvertDuctOffsetToDuctElevation(girderKey,z2,offset,offsetType);

      // locate the tendon horizontally in the girder cross section
      CComPtr<IPlane3d> webPlane;
      section->get_WebPlane(webIdx,&webPlane);

      webPlane->GetX(y1,z1,&x1);
      webPlane->GetX(y2,z2,&x2);

      // create a tendon segment
      CComPtr<IParabolicTendonSegment> parabolicTendonSegment;
      parabolicTendonSegment.CoCreateInstance(CLSID_ParabolicTendonSegment);

      CComPtr<IPoint3d> pntStart;
      CComPtr<IPoint3d> pntEnd;

      pntStart.CoCreateInstance(CLSID_Point3d);
      pntStart->Move(x1,y1,z1);

      pntEnd.CoCreateInstance(CLSID_Point3d);
      pntEnd->Move(x2,y2,z2);

      parabolicTendonSegment->put_Start(pntStart);
      parabolicTendonSegment->put_End(pntEnd);
      parabolicTendonSegment->put_Slope(0.0);
      parabolicTendonSegment->put_SlopeEnd(qcbRight);
      tendon->AddSegment(parabolicTendonSegment);

      // x1,y1,z1 become coordinates at the low point
      x1 = x2;
      y1 = y2;
      z1 = z2;

      //
      // Low Point to High Point to Low Point
      //
      Float64 x3, y3, z3;
      for ( PierIndexType pierIdx = startPierIdx+1; pierIdx < endPierIdx; pierIdx++ )
      {
         SpanIndexType prevSpanIdx = SpanIndexType(pierIdx-1);
         SpanIndexType nextSpanIdx = prevSpanIdx+1;

         Float64 distLeftIP, highOffset, distRightIP;
         CDuctGeometry::OffsetType highOffsetType;
         ductGeometry.GetHighPoint(pierIdx,&distLeftIP,&highOffset,&highOffsetType,&distRightIP);

         // low to inflection point
         Float64 L = GetSpanLength(prevSpanIdx,girderKey.girderIndex);
         if ( prevSpanIdx == startSpanIdx )
         {
            // adjust for the end distance at the start of the girder
            Float64 end_dist = GetSegmentStartEndDistance(CSegmentKey(girderKey.groupIndex,girderKey.girderIndex,0));
            L += end_dist;
         }
         Ls += L;

         x3 = x2; // dummy
         z3 = Ls;
         y3 = ConvertDuctOffsetToDuctElevation(girderKey,z3,highOffset,highOffsetType);

         if ( distLeftIP < 0 ) // fraction of distance between low and high point
            distLeftIP *= -(z3-z1);

         z2 = z3 - distLeftIP; // inflection point measured from high point... want z2 measured from start of span

         // elevation at inflection point (slope at low and high points must be zero)
         Float64 y2 = (y3*(z2-z1) + y1*(z3-z2))/(z3-z1);

         webPlane->GetX(y2,z2,&x2);
         webPlane->GetX(y3,z3,&x3);

         CComPtr<IParabolicTendonSegment> leftParabolicTendonSegment;
         leftParabolicTendonSegment.CoCreateInstance(CLSID_ParabolicTendonSegment);

         CComPtr<IPoint3d> pntStart;
         pntStart.CoCreateInstance(CLSID_Point3d);
         pntStart->Move(x1,y1,z1);

         CComPtr<IPoint3d> pntInflection;
         pntInflection.CoCreateInstance(CLSID_Point3d);
         pntInflection->Move(x2,y2,z2);

         leftParabolicTendonSegment->put_Start(pntStart);
         leftParabolicTendonSegment->put_End(pntInflection);
         leftParabolicTendonSegment->put_Slope(0.0);
         leftParabolicTendonSegment->put_SlopeEnd(qcbLeft);
         tendon->AddSegment(leftParabolicTendonSegment);

         // inflection to high point
         CComPtr<IParabolicTendonSegment> rightParabolicTendonSegment;
         rightParabolicTendonSegment.CoCreateInstance(CLSID_ParabolicTendonSegment);

         CComPtr<IPoint3d> pntEnd;
         pntEnd.CoCreateInstance(CLSID_Point3d);
         pntEnd->Move(x3,y3,z3);

         rightParabolicTendonSegment->put_Start(pntInflection);
         rightParabolicTendonSegment->put_End(pntEnd);
         rightParabolicTendonSegment->put_Slope(0.0);
         rightParabolicTendonSegment->put_SlopeEnd(qcbRight);
         tendon->AddSegment(rightParabolicTendonSegment);

         // high to inflection point
         ductGeometry.GetLowPoint(pierIdx,&dist,&offset,&offsetType);
         x1 = x3;
         z1 = z3; 
         y1 = y3;

         L = GetSpanLength(nextSpanIdx,girderKey.girderIndex);
         if ( nextSpanIdx == endSpanIdx )
         {
            // adjust for the end distance at the end of the girder
            Float64 end_dist = GetSegmentEndEndDistance(CSegmentKey(girderKey.groupIndex,girderKey.girderIndex,nSegments-1));
            L += end_dist;
         }

         if ( dist < 0 ) // fraction of span length
            dist *= -L;

         z3 = z1 + dist; // low point, measured from previous high point
         y3 = ConvertDuctOffsetToDuctElevation(girderKey,z3,offset,offsetType);

         if ( distRightIP < 0 ) // fraction of distance between high and low point
            distRightIP *= -(z3 - z1);

         z2 = z1 + distRightIP; // inflection point measured from high point
         y2 = (y3*(z2-z1) + y1*(z3-z2))/(z3-z1);

         webPlane->GetX(y2,z2,&x2);
         webPlane->GetX(y3,z3,&x3);

         leftParabolicTendonSegment.Release();
         leftParabolicTendonSegment.CoCreateInstance(CLSID_ParabolicTendonSegment);

         pntStart.Release();
         pntStart.CoCreateInstance(CLSID_Point3d);
         pntStart->Move(x1,y1,z1);

         pntInflection.Release();
         pntInflection.CoCreateInstance(CLSID_Point3d);
         pntInflection->Move(x2,y2,z2);

         leftParabolicTendonSegment->put_Start(pntStart);
         leftParabolicTendonSegment->put_End(pntInflection);
         leftParabolicTendonSegment->put_Slope(0.0);
         leftParabolicTendonSegment->put_SlopeEnd(qcbLeft);
         tendon->AddSegment(leftParabolicTendonSegment);

         // inflection point to low point
         rightParabolicTendonSegment.Release();
         rightParabolicTendonSegment.CoCreateInstance(CLSID_ParabolicTendonSegment);

         pntEnd.Release();
         pntEnd.CoCreateInstance(CLSID_Point3d);
         pntEnd->Move(x3,y3,z3);

         rightParabolicTendonSegment->put_Start(pntInflection);
         rightParabolicTendonSegment->put_End(pntEnd);
         rightParabolicTendonSegment->put_Slope(0.0);
         rightParabolicTendonSegment->put_SlopeEnd(qcbRight);
         tendon->AddSegment(rightParabolicTendonSegment);

         z1 = z3;
         y1 = y3;
         x1 = x3;
      }

      //
      // last low point to end
      //

      // distance is measured from the right end of the girder!!!
      ductGeometry.GetEndPoint(&dist,&offset,&offsetType);
      if ( dist < 0 ) // fraction of last span length
      {
         // get the cl-brg to cl-brg length for this girder in the end span
         Float64 L = GetSpanLength(endSpanIdx,girderKey.girderIndex);

         // adjust for the end distance at the end of the girder
         Float64 end_dist = GetSegmentEndEndDistance(CSegmentKey(girderKey.groupIndex,girderKey.girderIndex,nSegments-1));
         L += end_dist;

         if ( startSpanIdx == endSpanIdx )
         {
            // this is only one span for this tendon... adjust for end distance at the start of the girder
            end_dist = GetSegmentEndEndDistance(CSegmentKey(girderKey.groupIndex,girderKey.girderIndex,0));
            L += end_dist;
         }

         dist *= -L;
      }

      Float64 Lg = GetSplicedGirderLength(girderKey); // End-to-end length of full spliced girder

      x2 = x1; // dummy
      z2 = Lg - dist; // dist is measured from the end of the bridge
      y2 = ConvertDuctOffsetToDuctElevation(girderKey,z2,offset,offsetType);

      webPlane->GetX(y1,z1,&x1);
      webPlane->GetX(y2,z2,&x2);

      parabolicTendonSegment.Release();
      parabolicTendonSegment.CoCreateInstance(CLSID_ParabolicTendonSegment);

      pntStart.Release();
      pntStart.CoCreateInstance(CLSID_Point3d);
      pntStart->Move(x1,y1,z1);

      pntEnd.Release();
      pntEnd.CoCreateInstance(CLSID_Point3d);
      pntEnd->Move(x2,y2,z2);

      parabolicTendonSegment->put_Start(pntStart);
      parabolicTendonSegment->put_End(pntEnd);
      parabolicTendonSegment->put_Slope(0.0);
      parabolicTendonSegment->put_SlopeEnd(qcbLeft);
      tendon->AddSegment(parabolicTendonSegment);
   }

   tendons.CopyTo(ppTendons);
}

void CBridgeAgentImp::CreateLinearTendon(const CGirderKey& girderKey,ISuperstructureMember* pSSMbr,const CLinearDuctGeometry& ductGeometry,ITendonCollection** ppTendons)
{
#pragma Reminder("UPDATE: use the girder section to get the WebPlane to locate the ducts in the webs")
   CComPtr<ITendonCollection> tendons;
   tendons.CoCreateInstance(CLSID_TendonCollection);

   CComPtr<ISegment> segment;
   pSSMbr->get_Segment(0,&segment);

   CComPtr<IShape> shape;
   segment->get_PrimaryShape(0,&shape);

   CComQIPtr<IGirderSection> section(shape);
   WebIndexType nWebs;
   section->get_WebCount(&nWebs);

   for ( WebIndexType webIdx = 0; webIdx < nWebs; webIdx++ )
   {
      CComPtr<ITendon> tendon;
      tendon.CoCreateInstance(CLSID_Tendon);

      tendons->Add(tendon);

      Float64 xStart = 0;
      Float64 zStart = 0;
      Float64 offset;
      CDuctGeometry::OffsetType offsetType;
      ductGeometry.GetPoint(0,&zStart,&offset,&offsetType);

      Float64 yStart = ConvertDuctOffsetToDuctElevation(girderKey,zStart,offset,offsetType);

      CComPtr<IPlane3d> webPlane;
      section->get_WebPlane(webIdx,&webPlane);

      webPlane->GetX(yStart,zStart,&xStart);

      CollectionIndexType nPoints = ductGeometry.GetPointCount();
      if ( nPoints == 1 )
      {
         Float64 Lg = GetSplicedGirderLength(girderKey);
         CComPtr<IPoint3d> pntStart;
         pntStart.CoCreateInstance(CLSID_Point3d);
         pntStart->Move(xStart,yStart,zStart);

         CComPtr<IPoint3d> pntEnd;
         pntEnd.CoCreateInstance(CLSID_Point3d);
         pntEnd->Move(xStart,yStart,zStart+Lg);

         CComPtr<ILinearTendonSegment> linearTendonSegment;
         linearTendonSegment.CoCreateInstance(CLSID_LinearTendonSegment);
         linearTendonSegment->put_Start(pntStart);
         linearTendonSegment->put_End(pntEnd);

         CComQIPtr<ITendonSegment> tendonSegment(linearTendonSegment);
         tendon->AddSegment(tendonSegment);
      }
      else
      {
         for ( CollectionIndexType pointIdx = 1; pointIdx < nPoints; pointIdx++ )
         {
            Float64 distFromPrev;
            ductGeometry.GetPoint(pointIdx,&distFromPrev,&offset,&offsetType);

            Float64 xEnd = xStart;
            Float64 zEnd = zStart + distFromPrev;
            Float64 yEnd = ConvertDuctOffsetToDuctElevation(girderKey,zEnd,offset,offsetType);

            CComPtr<IPoint3d> pntStart;
            pntStart.CoCreateInstance(CLSID_Point3d);
            pntStart->Move(xStart,yStart,zStart);

            CComPtr<IPoint3d> pntEnd;
            pntEnd.CoCreateInstance(CLSID_Point3d);
            pntEnd->Move(xEnd,yEnd,zEnd);

            CComPtr<ILinearTendonSegment> linearTendonSegment;
            linearTendonSegment.CoCreateInstance(CLSID_LinearTendonSegment);
            linearTendonSegment->put_Start(pntStart);
            linearTendonSegment->put_End(pntEnd);

            CComQIPtr<ITendonSegment> tendonSegment(linearTendonSegment);
            tendon->AddSegment(tendonSegment);
         }
      }
   }

   tendons.CopyTo(ppTendons);
}

void CBridgeAgentImp::CreateOffsetTendon(const CGirderKey& girderKey,ISuperstructureMember* pSSMbr,const COffsetDuctGeometry& ductGeometry,ITendonCollection* refTendons,ITendonCollection** ppTendons)
{
   CComPtr<ITendonCollection> tendons;
   tendons.CoCreateInstance(CLSID_TendonCollection);

   GET_IFACE(IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription2* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CGirderGroupData*    pGroup      = pBridgeDesc->GetGirderGroup(girderKey.groupIndex);
   const CSplicedGirderData*  pGirder     = pGroup->GetGirder(girderKey.girderIndex);
   const CPTData*             pPTData     = pGirder->GetPostTensioning();

   CComPtr<ISegment> segment;
   pSSMbr->get_Segment(0,&segment);

   CComPtr<IShape> shape;
   segment->get_PrimaryShape(0,&shape);

   CComQIPtr<IGirderSection> section(shape);
   WebIndexType nWebs;
   section->get_WebCount(&nWebs);

   for ( WebIndexType webIdx = 0; webIdx < nWebs; webIdx++ )
   {
      CComPtr<IOffsetTendon> offsetTendon;
      offsetTendon.CoCreateInstance(CLSID_OffsetTendon);

      CComPtr<IPlane3d> webPlane;
      section->get_WebPlane(webIdx,&webPlane);

      tendons->Add(offsetTendon);

      CComPtr<ITendon> refTendon;
      refTendons->get_Item(ductGeometry.RefDuctIdx+webIdx,&refTendon);
      offsetTendon->putref_RefTendon(refTendon);

      Float64 z = 0;
      std::vector<COffsetDuctGeometry::Point>::const_iterator iter( ductGeometry.Points.begin() );
      std::vector<COffsetDuctGeometry::Point>::const_iterator iterEnd( ductGeometry.Points.end() );
      for ( ; iter != iterEnd; iter++ )
      {
         const COffsetDuctGeometry::Point& point = *iter;
         z += point.distance;

         CComPtr<IPoint3d> refPoint;
         refTendon->get_CG(z,tmPath,&refPoint);

         Float64 x;
         Float64 y;
         refPoint->get_X(&x);
         refPoint->get_Y(&y);
         y += point.offset;

         Float64 x_offset;
         webPlane->GetX(y,z,&x_offset);
         offsetTendon->AddOffset(z,x_offset-x,point.offset);
      }
   }

   tendons.CopyTo(ppTendons);
}

/// Strand filler-related functions
CContinuousStrandFiller* CBridgeAgentImp::GetContinuousStrandFiller(const CSegmentKey& segmentKey)
{
   StrandFillerCollection::iterator its = m_StrandFillers.find(segmentKey);
   if (its != m_StrandFillers.end())
   {
      CStrandFiller& pcfill = its->second;
      CContinuousStrandFiller* pfiller = dynamic_cast<CContinuousStrandFiller*>(&(pcfill));
      return pfiller;
   }
   else
   {
      ATLASSERT(0); // This will go badly. Filler should have been created already
      return NULL;
   }
}

CDirectStrandFiller* CBridgeAgentImp::GetDirectStrandFiller(const CSegmentKey& segmentKey)
{
   StrandFillerCollection::iterator its = m_StrandFillers.find(segmentKey);
   if (its != m_StrandFillers.end())
   {
      CStrandFiller& pcfill = its->second;
      CDirectStrandFiller* pfiller = dynamic_cast<CDirectStrandFiller*>(&(pcfill));
      return pfiller;
   }
   else
   {
      ATLASSERT(0); // This will go badly. Filler should have been created already
      return NULL;
   }
}

void CBridgeAgentImp::InitializeStrandFiller(const GirderLibraryEntry* pGirderEntry, const CSegmentKey& segmentKey)
{
   StrandFillerCollection::iterator its = m_StrandFillers.find(segmentKey);
   if (its != m_StrandFillers.end())
   {
      its->second.Init(pGirderEntry);
   }
   else
   {
      CStrandFiller filler;
      filler.Init(pGirderEntry);
      std::pair<StrandFillerCollection::iterator, bool>  sit = m_StrandFillers.insert(std::make_pair(segmentKey,filler));
      ATLASSERT(sit.second);
   }
}

void CBridgeAgentImp::CreateStrandMover(LPCTSTR strGirderName,IStrandMover** ppStrandMover)
{
   GET_IFACE(ILibrary,pLib);
   const GirderLibraryEntry* pGirderEntry = pLib->GetGirderEntry(strGirderName);

   // get harped strand adjustment limits
   pgsTypes::GirderFace endTopFace, endBottomFace;
   Float64 endTopLimit, endBottomLimit;
   pGirderEntry->GetEndAdjustmentLimits(&endTopFace, &endTopLimit, &endBottomFace, &endBottomLimit);

   IBeamFactory::BeamFace etf = endTopFace    == pgsTypes::GirderBottom ? IBeamFactory::BeamBottom : IBeamFactory::BeamTop;
   IBeamFactory::BeamFace ebf = endBottomFace == pgsTypes::GirderBottom ? IBeamFactory::BeamBottom : IBeamFactory::BeamTop;

   pgsTypes::GirderFace hpTopFace, hpBottomFace;
   Float64 hpTopLimit, hpBottomLimit;
   pGirderEntry->GetHPAdjustmentLimits(&hpTopFace, &hpTopLimit, &hpBottomFace, &hpBottomLimit);

   IBeamFactory::BeamFace htf = hpTopFace    == pgsTypes::GirderBottom ? IBeamFactory::BeamBottom : IBeamFactory::BeamTop;
   IBeamFactory::BeamFace hbf = hpBottomFace == pgsTypes::GirderBottom ? IBeamFactory::BeamBottom : IBeamFactory::BeamTop;

   // only allow end adjustents if increment is non-negative
   Float64 end_increment = pGirderEntry->IsVerticalAdjustmentAllowedEnd() ?  pGirderEntry->GetEndStrandIncrement() : -1.0;
   Float64 hp_increment  = pGirderEntry->IsVerticalAdjustmentAllowedHP()  ?  pGirderEntry->GetHPStrandIncrement()  : -1.0;

   // create the strand mover
   CComPtr<IBeamFactory> beamFactory;
   pGirderEntry->GetBeamFactory(&beamFactory);
   beamFactory->CreateStrandMover(pGirderEntry->GetDimensions(), 
                                  etf, endTopLimit, ebf, endBottomLimit, 
                                  htf, hpTopLimit,  hbf, hpBottomLimit, 
                                  end_increment, hp_increment, ppStrandMover);

   ATLASSERT(*ppStrandMover != NULL);
}
