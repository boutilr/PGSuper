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
#include <PgsExt\PointOfInterest.h>
#include <PgsExt\GirderLabel.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

Float64 pgsPointOfInterest::ms_Tol = 1.0e-06;

/****************************************************************************
CLASS
   pgsPointOfInterest
****************************************************************************/


////////////////////////// PUBLIC     ///////////////////////////////////////

//======================== LIFECYCLE  =======================================
pgsPointOfInterest::pgsPointOfInterest()
{
   m_ID = -1;
   m_Span = INVALID_INDEX;
   m_Girder = INVALID_INDEX;
   m_DistFromStart = 0;
}

pgsPointOfInterest::pgsPointOfInterest(SpanIndexType span,GirderIndexType gdr,Float64 distFromStart) :
m_ID(-1),
m_Span(span),
m_Girder(gdr),
m_DistFromStart(distFromStart)
{
   ATLASSERT( !(distFromStart < 0) );  // must be zero or more.

   ASSERTVALID;
}
pgsPointOfInterest::pgsPointOfInterest(pgsTypes::Stage stage,SpanIndexType span,GirderIndexType gdr,Float64 distFromStart,PoiAttributeType attrib) :
m_ID(-1),
m_Span(span),
m_Girder(gdr),
m_DistFromStart(distFromStart)
{
   ATLASSERT( !(distFromStart < 0) );  // must be zero or more.

   m_Stages.insert( std::make_pair(stage,attrib) );
   ASSERTVALID;
}

pgsPointOfInterest::pgsPointOfInterest(std::vector<pgsTypes::Stage> stages,SpanIndexType span,GirderIndexType gdr,Float64 distFromStart,PoiAttributeType attrib) :
m_ID(-1),
m_Span(span),
m_Girder(gdr),
m_DistFromStart(distFromStart)
{
   ATLASSERT( !(distFromStart < 0) );  // must be zero or more.

   std::vector<pgsTypes::Stage>::iterator iter;
   for ( iter = stages.begin(); iter != stages.end(); iter++ )
   {
      m_Stages.insert( std::make_pair(*iter,attrib) );
   }

   ASSERTVALID;
}

pgsPointOfInterest::pgsPointOfInterest(const pgsPointOfInterest& rOther)
{
   MakeCopy(rOther);
   ASSERTVALID;
}

pgsPointOfInterest::~pgsPointOfInterest()
{
}

//======================== OPERATORS  =======================================
pgsPointOfInterest& pgsPointOfInterest::operator= (const pgsPointOfInterest& rOther)
{
   if( this != &rOther )
   {
      MakeAssignment(rOther);
   }

   ASSERTVALID;
   return *this;
}

bool pgsPointOfInterest::operator<(const pgsPointOfInterest& rOther) const
{
   if ( GetSpan() < rOther.GetSpan() )
      return true;
   
   if ( GetSpan() > rOther.GetSpan() )
      return false;

   if ( GetGirder() < rOther.GetGirder() )
      return true;

   if ( GetGirder() > rOther.GetGirder() )
      return false;

   if ( GetDistFromStart() < rOther.GetDistFromStart() && !IsEqual(GetDistFromStart(), rOther.GetDistFromStart()) )
      return true;

   if ( GetDistFromStart() > rOther.GetDistFromStart() && !IsEqual(GetDistFromStart(), rOther.GetDistFromStart()) )
      return false;

   //// if everything else is equal, compare ID's
   //if ( IsEqual( GetDistFromStart(), rOther.GetDistFromStart() ) )
   //   return GetID() < rOther.GetID();

   return false;
}

bool pgsPointOfInterest::operator==(const pgsPointOfInterest& rOther) const
{
   if ( GetID() == rOther.GetID() && GetSpan() == rOther.GetSpan() && GetGirder() == rOther.GetGirder() && IsEqual(GetDistFromStart(),rOther.GetDistFromStart()) )
      return true;
   else
      return false;
}

//======================== OPERATIONS =======================================
void pgsPointOfInterest::SetLocation(SpanIndexType span,GirderIndexType gdr,Float64 distFromStart)
{
   SetSpan(span);
   SetGirder(gdr);
   SetDistFromStart(distFromStart);
}

//======================== ACCESS     =======================================
PoiIDType pgsPointOfInterest::GetID() const
{
   return m_ID;
}

void pgsPointOfInterest::SetSpan(SpanIndexType span)
{
   m_Span = span;
}

SpanIndexType pgsPointOfInterest::GetSpan() const
{
   return m_Span;
}

void pgsPointOfInterest::SetGirder(GirderIndexType gdr)
{
   m_Girder = gdr;
}

GirderIndexType pgsPointOfInterest::GetGirder() const
{
   return m_Girder;
}

void pgsPointOfInterest::SetDistFromStart(Float64 distFromStart)
{
   ATLASSERT( !(distFromStart < 0) );
   m_DistFromStart = distFromStart;
}

Float64 pgsPointOfInterest::GetDistFromStart() const
{
   return m_DistFromStart;
}

void pgsPointOfInterest::SetAttributes(pgsTypes::Stage stage,PoiAttributeType attrib)
{
   StageContainer::iterator found = m_Stages.find(stage);
   if ( found != m_Stages.end() )
   {
      found->second = attrib;
   }
   else
   {
      m_Stages.insert( std::make_pair(stage,attrib) );
   }

   ASSERTVALID;
}

void pgsPointOfInterest::SetAttributes(const std::vector<pgsTypes::Stage>& stages,PoiAttributeType attrib)
{
   std::vector<pgsTypes::Stage>::const_iterator iter;
   for ( iter = stages.begin(); iter != stages.end(); iter++ )
   {
      SetAttributes(*iter,attrib);
   }
}

PoiAttributeType pgsPointOfInterest::GetAttributes(pgsTypes::Stage stage) const
{
   StageContainer::const_iterator found = m_Stages.find(stage);
   if ( found != m_Stages.end() )
      return found->second;

   return 0;
}

bool pgsPointOfInterest::HasAttribute(pgsTypes::Stage stage,PoiAttributeType attribute) const
{
   return sysFlags<PoiAttributeType>::IsSet(GetAttributes(stage),attribute);
}

void pgsPointOfInterest::AddStage(pgsTypes::Stage stage,PoiAttributeType attribute)
{
   m_Stages.insert( std::make_pair(stage,attribute) );
}

void pgsPointOfInterest::AddStages(std::vector<pgsTypes::Stage> stages,PoiAttributeType attribute)
{
   std::vector<pgsTypes::Stage>::iterator iter;
   for ( iter = stages.begin(); iter != stages.end(); iter++ )
   {
      m_Stages.insert( std::make_pair(*iter,attribute) );
   }
}

void pgsPointOfInterest::RemoveStage(pgsTypes::Stage stage)
{
   StageContainer::iterator found = m_Stages.find(stage);
   if ( found != m_Stages.end() )
   {
      m_Stages.erase(found);
   }
}

bool pgsPointOfInterest::HasStage(pgsTypes::Stage stage) const
{
   StageContainer::const_iterator found = m_Stages.find(stage);
   return ( found != m_Stages.end() ? true : false );
}

std::vector<pgsTypes::Stage> pgsPointOfInterest::GetStages() const
{
   std::vector<pgsTypes::Stage> stages;
   StageContainer::const_iterator iter;
   for ( iter = m_Stages.begin(); iter != m_Stages.end(); iter++ )
   {
      stages.push_back( iter->first );
   }
   return stages;
}

Uint32 pgsPointOfInterest::GetStageCount() const
{
   return m_Stages.size();
}

void pgsPointOfInterest::SetTolerance(Float64 tol)
{
   ms_Tol = tol;
}

Float64 pgsPointOfInterest::GetTolerance()
{
   return ms_Tol;
}

//======================== INQUIRY    =======================================

bool pgsPointOfInterest::IsFlexureStress(pgsTypes::Stage stage) const
{
   return sysFlags<PoiAttributeType>::IsSet( GetAttributes(stage), POI_FLEXURESTRESS );
}

bool pgsPointOfInterest::IsFlexureCapacity(pgsTypes::Stage stage) const
{
   return sysFlags<PoiAttributeType>::IsSet( GetAttributes(stage), POI_FLEXURECAPACITY );
}

bool pgsPointOfInterest::IsShear(pgsTypes::Stage stage) const
{
   return sysFlags<PoiAttributeType>::IsSet( GetAttributes(stage), POI_SHEAR );
}

bool pgsPointOfInterest::IsDisplacement(pgsTypes::Stage stage) const
{
   return sysFlags<PoiAttributeType>::IsSet( GetAttributes(stage), POI_DISPLACEMENT );
}

bool pgsPointOfInterest::IsHarpingPoint(pgsTypes::Stage stage) const
{
   return sysFlags<PoiAttributeType>::IsSet( GetAttributes(stage), POI_HARPINGPOINT );
}

bool pgsPointOfInterest::IsConcentratedLoad(pgsTypes::Stage stage) const
{
   return sysFlags<PoiAttributeType>::IsSet( GetAttributes(stage), POI_CONCLOAD );
}

bool pgsPointOfInterest::IsMidSpan(pgsTypes::Stage stage) const
{
   return sysFlags<PoiAttributeType>::IsSet( GetAttributes(stage), POI_MIDSPAN );
}

bool pgsPointOfInterest::IsTabular(pgsTypes::Stage stage) const
{
   return sysFlags<PoiAttributeType>::IsSet( GetAttributes(stage), POI_TABULAR );
}

bool pgsPointOfInterest::IsGraphical(pgsTypes::Stage stage) const
{
   return sysFlags<PoiAttributeType>::IsSet( GetAttributes(stage), POI_GRAPHICAL );
}

bool pgsPointOfInterest::IsAtH(pgsTypes::Stage stage) const
{
   return sysFlags<PoiAttributeType>::IsSet( GetAttributes(stage), POI_H );
}

bool pgsPointOfInterest::IsAt15H(pgsTypes::Stage stage) const
{
   return sysFlags<PoiAttributeType>::IsSet( GetAttributes(stage), POI_15H );
}

Uint16 pgsPointOfInterest::IsATenthPoint(pgsTypes::Stage stage) const
{
   return GetAttributeTenthPoint(GetAttributes(stage));
}

void pgsPointOfInterest::MakeTenthPoint(pgsTypes::Stage stage,Uint16 tenthPoint)
{
   ATLASSERT(tenthPoint <= 11);
   PoiAttributeType attribute = GetAttributes(stage);
   SetAttributeTenthPoint(tenthPoint,&attribute);
   SetAttributes(stage,attribute);
}

void pgsPointOfInterest::MakeTenthPoint(const std::vector<pgsTypes::Stage>& stages,Uint16 tenthPoint)
{
   std::vector<pgsTypes::Stage>::const_iterator iter;
   for ( iter = stages.begin(); iter != stages.end(); iter++ )
   {
      MakeTenthPoint(*iter,tenthPoint);
   }
}

////////////////////////// PROTECTED  ///////////////////////////////////////

//======================== LIFECYCLE  =======================================
//======================== OPERATORS  =======================================
//======================== OPERATIONS =======================================
void pgsPointOfInterest::MakeCopy(const pgsPointOfInterest& rOther)
{
   // Add copy code here...
   m_ID            = rOther.m_ID;
   m_Span          = rOther.m_Span;
   m_Girder        = rOther.m_Girder;
   m_DistFromStart = rOther.m_DistFromStart;
   m_Stages        = rOther.m_Stages;

   ASSERTVALID;
}

void pgsPointOfInterest::MakeAssignment(const pgsPointOfInterest& rOther)
{
   MakeCopy( rOther );
}

//======================== ACCESS     =======================================
//======================== INQUIRY    =======================================
void pgsPointOfInterest::SetAttributeTenthPoint(Uint16 tenthPoint, PoiAttributeType* pattribute)
{
   ATLASSERT(tenthPoint <= 11);
   *pattribute |= PoiAttributeType(tenthPoint);
}

Uint16 pgsPointOfInterest::GetAttributeTenthPoint(PoiAttributeType attribute)
{
   Uint32 low32 = low_Uint32(attribute);
   Uint16 tenth_point = low_Uint16(low32);
   ATLASSERT(tenth_point <= 11);
   return tenth_point;
}

////////////////////////// PRIVATE    ///////////////////////////////////////

//======================== LIFECYCLE  =======================================
//======================== OPERATORS  =======================================
//======================== OPERATIONS =======================================
//======================== ACCESS     =======================================
//======================== INQUERY    =======================================

//======================== DEBUG      =======================================
#if defined _DEBUG
bool pgsPointOfInterest::AssertValid() const
{
   if ( m_DistFromStart < 0 )
      return false;

   return true;
}
#endif // _DEBUG

rptPointOfInterest::rptPointOfInterest(const unitLength* pUnitOfMeasure,
                                       Float64 zeroTolerance,
                                       bool bShowUnitTag) :
rptLengthUnitValue(pUnitOfMeasure,zeroTolerance,bShowUnitTag),m_bPrefixAttributes(true),m_bIncludeSpanAndGirder(false)
{
}

rptPointOfInterest::rptPointOfInterest(const rptPointOfInterest& rOther) :
rptLengthUnitValue( rOther )
{
   MakeCopy( rOther );
}

rptPointOfInterest& rptPointOfInterest::operator = (const rptPointOfInterest& rOther)
{
   if ( this != &rOther )
      MakeAssignment( rOther );

   return *this;
}

rptReportContent* rptPointOfInterest::CreateClone() const
{
   return new rptPointOfInterest( *this );
}

void rptPointOfInterest::IncludeSpanAndGirder(bool bIncludeSpanAndGirder)
{
   m_bIncludeSpanAndGirder = bIncludeSpanAndGirder;
}

bool rptPointOfInterest::IncludeSpanAndGirder() const
{
   return m_bIncludeSpanAndGirder;
}

rptReportContent& rptPointOfInterest::SetValue(pgsTypes::Stage stage,const pgsPointOfInterest& poi,Float64 endOffset)
{
   m_POI = poi;
   m_Stage = stage;
   return rptLengthUnitValue::SetValue( poi.GetDistFromStart() - endOffset );
}

std::string rptPointOfInterest::AsString() const
{
   std::string strAttrib;
   Uint16 nAttributes = 0;
   PoiAttributeType attributes = m_POI.GetAttributes(m_Stage);

   strAttrib = "(";

   if ( m_POI.IsHarpingPoint(m_Stage) )
   {
      if ( 0 < nAttributes )
         strAttrib += ", ";

      strAttrib += "HP";
      nAttributes++;
   }

   if ( m_POI.IsAtH(m_Stage) )
   {
      if ( 0 < nAttributes )
         strAttrib += ", ";

      strAttrib += "H";
      nAttributes++;
   }

   if ( m_POI.IsAt15H(m_Stage) )
   {
      if ( 0 < nAttributes )
         strAttrib += ", ";

      strAttrib += "1.5H";
      nAttributes++;
   }
   
   if ( lrfdVersionMgr::ThirdEdition2004 <= lrfdVersionMgr::GetVersion() )
   {
      if ( m_POI.HasAttribute(m_Stage,POI_CRITSECTSHEAR1) || m_POI.HasAttribute(m_Stage,POI_CRITSECTSHEAR2) )
      {
         if ( 0 < nAttributes )
            strAttrib += ", ";

         strAttrib += "CS";
         nAttributes++;
      }
   }
   else
   {
      if ( m_POI.HasAttribute(m_Stage,POI_CRITSECTSHEAR1) )
      {
         if ( 0 < nAttributes )
            strAttrib += ", ";

         strAttrib += "DCS";
         nAttributes++;
      }
      
      if ( m_POI.HasAttribute(m_Stage,POI_CRITSECTSHEAR2) )
      {
         if ( 0 < nAttributes )
            strAttrib += ", ";

         strAttrib += "PCS";
         nAttributes++;
      }
   }

   if ( m_POI.HasAttribute(m_Stage,POI_PSXFER) )
   {
      if ( 0 < nAttributes )
         strAttrib += ", ";

      strAttrib += "PSXFR";
      nAttributes++;
   }

   if ( m_POI.HasAttribute(m_Stage,POI_PSDEV) )
   {
      if ( 0 < nAttributes )
         strAttrib += ", ";

      strAttrib += "Ld";
      nAttributes++;
   }

   if ( m_POI.HasAttribute(m_Stage,POI_DEBOND) )
   {
      if ( 0 < nAttributes )
         strAttrib += ", ";

      strAttrib += "Debond";
      nAttributes++;
   }

   if ( m_POI.HasAttribute(m_Stage,POI_BARCUTOFF) )
   {
      if ( 0 < nAttributes )
         strAttrib += ", ";

      strAttrib += "Bar Cutoff";
      nAttributes++;
   }

   if ( m_POI.HasAttribute(m_Stage,POI_PICKPOINT) )
   {
      if ( 0 < nAttributes )
         strAttrib += ", ";

      strAttrib += "Pick Point";
      nAttributes++;
   }

   if ( m_POI.HasAttribute(m_Stage,POI_BUNKPOINT) )
   {
      if ( 0 < nAttributes )
         strAttrib += ", ";

      strAttrib += "Bunk Point";
      nAttributes++;
   }

   if ( m_POI.HasAttribute(m_Stage,POI_FACEOFSUPPORT) )
   {
      if ( 0 < nAttributes )
         strAttrib += ", ";

      strAttrib += "FoS";
      nAttributes++;
   }

   //if ( m_POI.HasAttribute(m_Stage,POI_SECTCHANGE) )
   //{
   //   if ( 0 < nAttributes )
   //      strAttrib += ", ";

   //   strAttrib += "XS";
   //   nAttributes++;
   //}

   Uint16 tenpt = m_POI.IsATenthPoint(m_Stage);
   if (0 < tenpt)
   {
      CHECK(tenpt<12);
      // for the sake of efficiency, dont use a stringstream
      const char* span_label[]={"err","0.0L<sub>s</sub>","0.1L<sub>s</sub>","0.2L<sub>s</sub>","0.3L<sub>s</sub>","0.4L<sub>s</sub>",
         "0.5L<sub>s</sub>","0.6L<sub>s</sub>","0.7L<sub>s</sub>","0.8L<sub>s</sub>","0.9L<sub>s</sub>","1.0L<sub>s</sub>"};

      const char* girder_label[]={"err","0.0L<sub>g</sub>","0.1L<sub>g</sub>","0.2L<sub>g</sub>","0.3L<sub>g</sub>","0.4L<sub>g</sub>",
         "0.5L<sub>g</sub>","0.6L<sub>g</sub>","0.7L<sub>g</sub>","0.8L<sub>g</sub>","0.9L<sub>g</sub>","1.0L<sub>g</sub>"};


      if ( 0 < nAttributes )
         strAttrib += ", ";

      if ( m_Stage == pgsTypes::CastingYard )
         strAttrib += std::string(girder_label[tenpt]);
      else
         strAttrib += std::string(span_label[tenpt]);

      nAttributes++;
   }
   strAttrib += ")";

   std::string strValue = rptLengthUnitValue::AsString();

   std::string str;

   if ( m_bIncludeSpanAndGirder )
   {
      CString str1;
      str1.Format("Span %d Girder %s, ",LABEL_SPAN(m_POI.GetSpan()),LABEL_GIRDER(m_POI.GetGirder()));
      str = str1;
   }

   if ( nAttributes == 0 )
   {
      str += strValue;
   }
   else
   {
      if ( m_bPrefixAttributes )
         str += strAttrib + " " + strValue;
      else
         str += strValue + " " + strAttrib;
   }

   return str;
}

void rptPointOfInterest::MakeCopy(const rptPointOfInterest& rOther)
{
   m_POI                   = rOther.m_POI;
   m_Stage                 = rOther.m_Stage;
   m_bPrefixAttributes     = rOther.m_bPrefixAttributes;
   m_bIncludeSpanAndGirder = rOther.m_bIncludeSpanAndGirder;
}

void rptPointOfInterest::MakeAssignment(const rptPointOfInterest& rOther)
{
   rptLengthUnitValue::MakeAssignment( rOther );
   MakeCopy( rOther );
}

void rptPointOfInterest::PrefixAttributes(bool bPrefixAttributes)
{
   m_bPrefixAttributes = bPrefixAttributes;
}

bool rptPointOfInterest::PrefixAttributes() const
{
   return m_bPrefixAttributes;
}
