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

#include "StdAfx.h"
#include <Reporting\BridgeDescChapterBuilder.h>

#include <EAF\EAFDisplayUnits.h>
#include <IFace\Bridge.h>
#include <IFace\Project.h>
#include <IFace\Alignment.h>

#include <PsgLib\ConnectionLibraryEntry.h>
#include <PsgLib\ConcreteLibraryEntry.h>
#include <PsgLib\GirderLibraryEntry.h>
#include <PsgLib\TrafficBarrierEntry.h>


#include <PgsExt\BridgeDescription.h>
#include <PgsExt\GirderData.h>
#include <PgsExt\PierData.h>
#include <PgsExt\GirderLabel.h>

#include <Material\PsStrand.h>

#include <WBFLCogo.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

static void write_alignment_data(IBroker* pBroker,IEAFDisplayUnits* pDisplayUnits,rptChapter* pChapter,Uint16 level);
static void write_profile_data(IBroker* pBroker,IEAFDisplayUnits* pDisplayUnits,rptChapter* pChapter,Uint16 level);
static void write_crown_data(IBroker* pBroker,IEAFDisplayUnits* pDisplayUnits,rptChapter* pChapter,Uint16 level);
static void write_bridge_data(IBroker* pBroker,IEAFDisplayUnits* pDisplayUnits,rptChapter* pChapter,Uint16 level,SpanIndexType span,GirderIndexType gdr);
static void write_pier_data(IBroker* pBroker,IEAFDisplayUnits* pDisplayUnits,rptChapter* pChapter,Uint16 level);
static void write_span_data(IBroker* pBroker,IEAFDisplayUnits* pDisplayUnits,rptChapter* pChapter,Uint16 level);
static void write_girder_spacing(IBroker* pBroker,IEAFDisplayUnits* pDisplayUnits,rptRcTable* pTable,const CGirderSpacing* pGirderSpacing,RowIndexType row,ColumnIndexType col);
static void write_ps_data(IBroker* pBroker,IEAFDisplayUnits* pDisplayUnits,rptChapter* pChapter,Uint16 level,SpanIndexType span, GirderIndexType gdr);
static void write_slab_data(IBroker* pBroker,IEAFDisplayUnits* pDisplayUnits,rptChapter* pChapter,Uint16 level);
static void write_concrete_details(IBroker* pBroker,IEAFDisplayUnits* pDisplayUnits,rptChapter* pChapter,SpanIndexType span,GirderIndexType gdr,Uint16 level);
static void write_deck_reinforcing_data(IBroker* pBroker,IEAFDisplayUnits* pDisplayUnits,rptChapter* pChapter,Uint16 level);

/****************************************************************************
CLASS
   CBridgeDescChapterBuilder
****************************************************************************/


////////////////////////// PUBLIC     ///////////////////////////////////////

//======================== LIFECYCLE  =======================================
CBridgeDescChapterBuilder::CBridgeDescChapterBuilder(bool bSelect) :
CPGSuperChapterBuilder(bSelect)
{
}

//======================== OPERATORS  =======================================
//======================== OPERATIONS =======================================
LPCTSTR CBridgeDescChapterBuilder::GetName() const
{
   return TEXT("Bridge Description");
}

rptChapter* CBridgeDescChapterBuilder::Build(CReportSpecification* pRptSpec,Uint16 level) const
{
   CSpanGirderReportSpecification* pSGRptSpec = dynamic_cast<CSpanGirderReportSpecification*>(pRptSpec);
   CGirderReportSpecification* pGdrRptSpec    = dynamic_cast<CGirderReportSpecification*>(pRptSpec);
   CComPtr<IBroker> pBroker;
   SpanIndexType span;
   GirderIndexType gdr;

   if ( pSGRptSpec )
   {
      pSGRptSpec->GetBroker(&pBroker);
      span = pSGRptSpec->GetSpan();
      gdr = pSGRptSpec->GetGirder();
   }
   else if ( pGdrRptSpec )
   {
      pGdrRptSpec->GetBroker(&pBroker);
      span = ALL_SPANS;
      gdr = pGdrRptSpec->GetGirder();
   }
   else
   {
      span = ALL_SPANS;
      gdr  = ALL_GIRDERS;
   }

   rptChapter* pChapter = CPGSuperChapterBuilder::Build(pRptSpec,level);

   GET_IFACE2(pBroker,IEAFDisplayUnits,pDisplayUnits);
   write_alignment_data( pBroker, pDisplayUnits, pChapter, level);
   write_profile_data( pBroker, pDisplayUnits, pChapter, level);
   write_crown_data( pBroker, pDisplayUnits, pChapter, level);
   write_bridge_data( pBroker, pDisplayUnits, pChapter, level, span, gdr);
   write_concrete_details(pBroker,pDisplayUnits,pChapter, span, gdr,level);
   write_pier_data( pBroker, pDisplayUnits, pChapter, level);
   write_span_data( pBroker, pDisplayUnits, pChapter, level);
   write_ps_data( pBroker, pDisplayUnits, pChapter, level, span, gdr );
   write_slab_data( pBroker, pDisplayUnits, pChapter, level );
   write_deck_reinforcing_data( pBroker, pDisplayUnits, pChapter, level );

   return pChapter;
}

CChapterBuilder* CBridgeDescChapterBuilder::Clone() const
{
   return new CBridgeDescChapterBuilder;
}

void CBridgeDescChapterBuilder::WriteAlignmentData(IBroker* pBroker, IEAFDisplayUnits* pDisplayUnits, rptChapter* pChapter,Uint16 level)
{
   write_alignment_data( pBroker, pDisplayUnits, pChapter, level);
}

void CBridgeDescChapterBuilder::WriteProfileData(IBroker* pBroker, IEAFDisplayUnits* pDisplayUnits, rptChapter* pChapter,Uint16 level)
{
   write_profile_data( pBroker, pDisplayUnits, pChapter, level);
}

void CBridgeDescChapterBuilder::WriteCrownData(IBroker* pBroker, IEAFDisplayUnits* pDisplayUnits, rptChapter* pChapter,Uint16 level)
{
   write_crown_data( pBroker, pDisplayUnits, pChapter, level);
}

//======================== ACCESS     =======================================
//======================== INQUIRY    =======================================

////////////////////////// PROTECTED  ///////////////////////////////////////

//======================== LIFECYCLE  =======================================
//======================== OPERATORS  =======================================
//======================== OPERATIONS =======================================
//======================== ACCESS     =======================================
//======================== INQUIRY    =======================================

////////////////////////// PRIVATE    ///////////////////////////////////////

//======================== LIFECYCLE  =======================================
//======================== OPERATORS  =======================================
//======================== OPERATIONS =======================================
//======================== ACCESS     =======================================
//======================== INQUERY    =======================================
void write_alignment_data(IBroker* pBroker,IEAFDisplayUnits* pDisplayUnits,rptChapter* pChapter,Uint16 level)
{
   USES_CONVERSION;

   GET_IFACE2(pBroker, IRoadwayData, pAlignment ); 
   rptParagraph* pPara;

   INIT_UV_PROTOTYPE( rptLengthUnitValue, length, pDisplayUnits->GetAlignmentLengthUnit(), false );

   CComPtr<IDirectionDisplayUnitFormatter> direction_formatter;
   direction_formatter.CoCreateInstance(CLSID_DirectionDisplayUnitFormatter);
   direction_formatter->put_BearingFormat(VARIANT_TRUE);

   CComPtr<IAngleDisplayUnitFormatter> angle_formatter;
   angle_formatter.CoCreateInstance(CLSID_AngleDisplayUnitFormatter);

   pPara = new rptParagraph( pgsReportStyleHolder::GetHeadingStyle() );
   *pChapter << pPara;
   *pPara << "Alignment Details" << rptNewLine;

   pPara = new rptParagraph;
   *pChapter << pPara;

   AlignmentData2 alignment = pAlignment->GetAlignmentData2();

   CComBSTR bstrBearing;
   direction_formatter->Format(alignment.Direction,CComBSTR("�,\',\""),&bstrBearing);
   *pPara << "Direction: " << RPT_BEARING(OLE2A(bstrBearing)) << rptNewLine;

   *pPara << "Ref. Point: " << rptRcStation(alignment.RefStation, &pDisplayUnits->GetStationFormat())
          << " "
          << "(E (X) " << length.SetValue(alignment.xRefPoint);
   *pPara << ", " 
          << "N (Y) " << length.SetValue(alignment.yRefPoint) << ")" << rptNewLine;

   if ( alignment.HorzCurves.size() == 0 )
      return;

   GET_IFACE2(pBroker, IRoadway, pRoadway );

   pPara = new rptParagraph;
   *pChapter << pPara;

   rptRcTable* pTable = pgsReportStyleHolder::CreateDefaultTable(alignment.HorzCurves.size()+1,"Horizontal Curve Data");
   *pPara << pTable << rptNewLine;

   pTable->SetColumnStyle(0,pgsReportStyleHolder::GetTableCellStyle(CB_NONE | CJ_LEFT));
   pTable->SetStripeRowColumnStyle(0,pgsReportStyleHolder::GetTableStripeRowCellStyle(CB_NONE | CJ_LEFT));

   RowIndexType row = 0;

   (*pTable)(row++,0) << "Curve Parameters";
   (*pTable)(row++,0) << "Back Tangent Bearing";
   (*pTable)(row++,0) << "Forward Tangent Bearing";
   (*pTable)(row++,0) << "TS Station";
   (*pTable)(row++,0) << "SC Station";
   (*pTable)(row++,0) << "PC Station";
   (*pTable)(row++,0) << "PI Station";
   (*pTable)(row++,0) << "PT Station";
   (*pTable)(row++,0) << "CS Station";
   (*pTable)(row++,0) << "ST Station";
   (*pTable)(row++,0) << "Total Delta (" << Sub2(symbol(DELTA),"c") << ")";
   (*pTable)(row++,0) << "Entry Spiral Length";
   (*pTable)(row++,0) << "Exit Spiral Length";
   (*pTable)(row++,0) << "Delta (" << Sub2(symbol(DELTA),"cc") << ")";
   (*pTable)(row++,0) << "Degree Curvature (DC)";
   (*pTable)(row++,0) << "Radius (R)";
   (*pTable)(row++,0) << "Tangent (T)";
   (*pTable)(row++,0) << "Length (L)";
   (*pTable)(row++,0) << "Chord (C)";
   (*pTable)(row++,0) << "External (E)";
   (*pTable)(row++,0) << "Mid Ordinate (MO)";

   length.ShowUnitTag(true);

   ColumnIndexType col = 1;
   std::vector<HorzCurveData>::iterator iter;
   for ( iter = alignment.HorzCurves.begin(); iter != alignment.HorzCurves.end(); iter++, col++ )
   {
      HorzCurveData& hc_data = *iter;
      row = 0;

      (*pTable)(row++,col) << "Curve " << col;

      if ( IsZero(hc_data.Radius) )
      {
         CComPtr<IDirection> bkTangent;
         pRoadway->GetBearing(hc_data.PIStation - ::ConvertToSysUnits(1.0,unitMeasure::Feet),&bkTangent);

         CComPtr<IDirection> fwdTangent;
         pRoadway->GetBearing(hc_data.PIStation + ::ConvertToSysUnits(1.0,unitMeasure::Feet),&fwdTangent);

         double bk_tangent_value;
         bkTangent->get_Value(&bk_tangent_value);

         double fwd_tangent_value;
         fwdTangent->get_Value(&fwd_tangent_value);

         CComBSTR bstrBkTangent;
         direction_formatter->Format(bk_tangent_value,CComBSTR("�,\',\""),&bstrBkTangent);

         CComBSTR bstrFwdTangent;
         direction_formatter->Format(fwd_tangent_value,CComBSTR("�,\',\""),&bstrFwdTangent);
         (*pTable)(row++,col) << RPT_BEARING(OLE2A(bstrBkTangent));
         (*pTable)(row++,col) << RPT_BEARING(OLE2A(bstrFwdTangent));

         (*pTable)(row++,col) << "";
         (*pTable)(row++,col) << "";
         (*pTable)(row++,col) << "";

         (*pTable)(row++,col) << rptRcStation(hc_data.PIStation, &pDisplayUnits->GetStationFormat());

         (*pTable)(row++,col) << "";
         (*pTable)(row++,col) << "";
         (*pTable)(row++,col) << "";
         (*pTable)(row++,col) << "";
         (*pTable)(row++,col) << "";
         (*pTable)(row++,col) << "";
         (*pTable)(row++,col) << "";
         (*pTable)(row++,col) << "";
         (*pTable)(row++,col) << "";
         (*pTable)(row++,col) << "";
         (*pTable)(row++,col) << "";
         (*pTable)(row++,col) << "";
         (*pTable)(row++,col) << "";
         (*pTable)(row++,col) << "";
      }
      else
      {
         CComPtr<IHorzCurve> hc;
         pRoadway->GetCurve(col-1,&hc);

         CComPtr<IDirection> bkTangent;
         CComPtr<IDirection> fwdTangent;
         hc->get_BkTangentBrg(&bkTangent);
         hc->get_FwdTangentBrg(&fwdTangent);

         double bk_tangent_value;
         bkTangent->get_Value(&bk_tangent_value);

         double fwd_tangent_value;
         fwdTangent->get_Value(&fwd_tangent_value);

         CComBSTR bstrBkTangent;
         direction_formatter->Format(bk_tangent_value,CComBSTR("�,\',\""),&bstrBkTangent);

         CComBSTR bstrFwdTangent;
         direction_formatter->Format(fwd_tangent_value,CComBSTR("�,\',\""),&bstrFwdTangent);

         double bk_tangent_length;
         hc->get_BkTangentLength(&bk_tangent_length);

         double total_length;
         hc->get_TotalLength(&total_length);

         double ts,sc,cs,st;
         ts = hc_data.PIStation - bk_tangent_length;
         sc = hc_data.PIStation - bk_tangent_length + hc_data.EntrySpiral;
         cs = hc_data.PIStation - bk_tangent_length + total_length - hc_data.ExitSpiral;
         st = hc_data.PIStation - bk_tangent_length + total_length;
         (*pTable)(row++,col) << RPT_BEARING(OLE2A(bstrBkTangent));
         (*pTable)(row++,col) << RPT_BEARING(OLE2A(bstrFwdTangent));
         if ( IsEqual(ts,sc) )
         {
            (*pTable)(row++,col) << "-";
            (*pTable)(row++,col) << "-";
            (*pTable)(row++,col) << rptRcStation(ts, &pDisplayUnits->GetStationFormat());
         }
         else
         {
            (*pTable)(row++,col) << rptRcStation(ts, &pDisplayUnits->GetStationFormat());
            (*pTable)(row++,col) << rptRcStation(sc, &pDisplayUnits->GetStationFormat());
            (*pTable)(row++,col) << "-";
         }
         (*pTable)(row++,col) << rptRcStation(hc_data.PIStation, &pDisplayUnits->GetStationFormat());

         if ( IsEqual(cs,st) )
         {
            (*pTable)(row++,col) << rptRcStation(cs, &pDisplayUnits->GetStationFormat());
            (*pTable)(row++,col) << "-";
            (*pTable)(row++,col) << "-";
         }
         else
         {
            (*pTable)(row++,col) << "-";
            (*pTable)(row++,col) << rptRcStation(cs, &pDisplayUnits->GetStationFormat());
            (*pTable)(row++,col) << rptRcStation(st, &pDisplayUnits->GetStationFormat());
         }

         CurveDirectionType direction;
         hc->get_Direction(&direction);

         CComPtr<IAngle> delta;
         hc->get_CurveAngle(&delta);
         double delta_value;
         delta->get_Value(&delta_value);
         delta_value *= (direction == cdRight ? -1 : 1);
         CComBSTR bstrDelta;
         angle_formatter->put_Signed(VARIANT_FALSE);
         angle_formatter->Format(delta_value,CComBSTR("�,\',\""),&bstrDelta);
         (*pTable)(row++,col) << RPT_ANGLE(OLE2A(bstrDelta));

         // Entry Spiral Data
         (*pTable)(row++,col) << length.SetValue(hc_data.EntrySpiral);
         (*pTable)(row++,col) << length.SetValue(hc_data.ExitSpiral);

         // Circular curve data
         delta.Release();
         hc->get_CircularCurveAngle(&delta);
         delta->get_Value(&delta_value);
         delta_value *= (direction == cdRight ? -1 : 1);
         bstrDelta.Empty();
         angle_formatter->put_Signed(VARIANT_FALSE);
         angle_formatter->Format(delta_value,CComBSTR("�,\',\""),&bstrDelta);

         CComBSTR bstrDC;
         delta.Release();
         hc->get_DegreeCurvature(::ConvertToSysUnits(100.0,unitMeasure::Feet),dcHighway,&delta);
         delta->get_Value(&delta_value);
         angle_formatter->put_Signed(VARIANT_TRUE);
         angle_formatter->Format(delta_value,CComBSTR("�,\',\""),&bstrDC);


         double tangent;
         hc->get_Tangent(&tangent);

         double curve_length;
         hc->get_CurveLength(&curve_length);

         double external;
         hc->get_External(&external);

         double chord;
         hc->get_Chord(&chord);
         
         double mid_ordinate;
         hc->get_MidOrdinate(&mid_ordinate);

         (*pTable)(row++,col) << RPT_ANGLE(OLE2A(bstrDelta));
         (*pTable)(row++,col) << RPT_ANGLE(OLE2A(bstrDC));
         (*pTable)(row++,col) << length.SetValue(hc_data.Radius);
         (*pTable)(row++,col) << length.SetValue(tangent);
         (*pTable)(row++,col) << length.SetValue(curve_length);
         (*pTable)(row++,col) << length.SetValue(chord);
         (*pTable)(row++,col) << length.SetValue(external);
         (*pTable)(row++,col) << length.SetValue(mid_ordinate);
      }
   }
}

void write_profile_data(IBroker* pBroker,IEAFDisplayUnits* pDisplayUnits,rptChapter* pChapter,Uint16 level)
{
   GET_IFACE2(pBroker, IRoadway, pRoadway);
   GET_IFACE2(pBroker, IRoadwayData, pAlignment ); 
   rptParagraph* pPara;

   INIT_UV_PROTOTYPE( rptLengthUnitValue, length, pDisplayUnits->GetAlignmentLengthUnit(), true );

   pPara = new rptParagraph( pgsReportStyleHolder::GetHeadingStyle() );
   *pChapter << pPara;
   *pPara << "Profile Details" << rptNewLine;

   ProfileData2 profile = pAlignment->GetProfileData2();

   pPara = new rptParagraph;
   *pChapter << pPara;
   *pPara << "Station: " << rptRcStation(profile.Station, &pDisplayUnits->GetStationFormat()) << rptNewLine;
   *pPara << "Elevation: " << length.SetValue(profile.Elevation) << rptNewLine;
   *pPara << "Grade: " << profile.Grade*100 << "%" << rptNewLine;

   if ( profile.VertCurves.size() == 0 )
      return;


   // Setup the table
   rptRcTable* pTable = pgsReportStyleHolder::CreateDefaultTable(profile.VertCurves.size()+1,"Vertical Curve Data");

   pTable->SetColumnStyle(0,pgsReportStyleHolder::GetTableCellStyle(CB_NONE | CJ_LEFT));
   pTable->SetStripeRowColumnStyle(0,pgsReportStyleHolder::GetTableStripeRowCellStyle(CB_NONE | CJ_LEFT));

   *pPara << pTable << rptNewLine;

   RowIndexType row = 0;
   ColumnIndexType col = 0;
   (*pTable)(row++,col) << "Curve Parameters";
   (*pTable)(row++,col) << "BVC Station";
   (*pTable)(row++,col) << "BVC Elevation";
   (*pTable)(row++,col) << "PVI Station";
   (*pTable)(row++,col) << "PVI Elevation";
   (*pTable)(row++,col) << "EVC Station";
   (*pTable)(row++,col) << "EVC Elevation";
   (*pTable)(row++,col) << "Entry Grade";
   (*pTable)(row++,col) << "Exit Grade";
   (*pTable)(row++,col) << "L1";
   (*pTable)(row++,col) << "L2";
   (*pTable)(row++,col) << "Length";
   (*pTable)(row++,col) << "High Pt Station";
   (*pTable)(row++,col) << "High Pt Elevation";
   (*pTable)(row++,col) << "Low Pt Station";
   (*pTable)(row++,col) << "Low Pt Elevation";

   col++;

   std::vector<VertCurveData>::iterator iter;
   for ( iter = profile.VertCurves.begin(); iter != profile.VertCurves.end(); iter++ )
   {
      row = 0;

      VertCurveData& vcd = *iter;

      (*pTable)(row++,col) << "Curve " << col;
      if ( IsZero(vcd.L1) && IsZero(vcd.L2) )
      {
         Float64 pvi_elevation = pRoadway->GetElevation(vcd.PVIStation,0.0);
         Float64 g1;
         if ( iter == profile.VertCurves.begin() )
            g1 = profile.Grade;
         else
            g1 = (*(iter-1)).ExitGrade;

         Float64 g2 = vcd.ExitGrade;

         (*pTable)(row++,col) << "";
         (*pTable)(row++,col) << "";
         (*pTable)(row++,col) << rptRcStation(vcd.PVIStation, &pDisplayUnits->GetStationFormat());
         (*pTable)(row++,col) << length.SetValue(pvi_elevation);
         (*pTable)(row++,col) << "";
         (*pTable)(row++,col) << "";
         (*pTable)(row++,col) << g1*100 << "%";
         (*pTable)(row++,col) << g2*100 << "%";
         (*pTable)(row++,col) << "";
         (*pTable)(row++,col) << "";
         (*pTable)(row++,col) << "";
         (*pTable)(row++,col) << "";
         (*pTable)(row++,col) << "";
         (*pTable)(row++,col) << "";
         (*pTable)(row++,col) << "";
      }
      else
      {
         CComPtr<IVertCurve> vc;
         pRoadway->GetVertCurve(col-1,&vc);

         CComPtr<IProfilePoint> bvc;
         vc->get_BVC(&bvc);

         CComPtr<IStation> bvc_station;
         bvc->get_Station(&bvc_station);

         double bvc_station_value;
         bvc_station->get_Value(&bvc_station_value);

         double bvc_elevation;
         bvc->get_Elevation(&bvc_elevation);


         CComPtr<IProfilePoint> pvi;
         vc->get_PVI(&pvi);

         CComPtr<IStation> pvi_station;
         pvi->get_Station(&pvi_station);

         double pvi_station_value;
         pvi_station->get_Value(&pvi_station_value);

         double pvi_elevation;
         pvi->get_Elevation(&pvi_elevation);


         CComPtr<IProfilePoint> evc;
         vc->get_EVC(&evc);

         CComPtr<IStation> evc_station;
         evc->get_Station(&evc_station);

         double evc_station_value;
         evc_station->get_Value(&evc_station_value);

         double evc_elevation;
         evc->get_Elevation(&evc_elevation);

         double g1,g2;
         vc->get_EntryGrade(&g1);
         vc->get_ExitGrade(&g2);

         double L1,L2, Length;
         vc->get_L1(&L1);
         vc->get_L2(&L2);
         vc->get_Length(&Length);

         CComPtr<IProfilePoint> high;
         vc->get_HighPoint(&high);

         CComPtr<IStation> high_station;
         high->get_Station(&high_station);

         double high_station_value;
         high_station->get_Value(&high_station_value);

         double high_elevation;
         high->get_Elevation(&high_elevation);

         CComPtr<IProfilePoint> low;
         vc->get_LowPoint(&low);

         CComPtr<IStation> low_station;
         low->get_Station(&low_station);

         double low_station_value;
         low_station->get_Value(&low_station_value);

         double low_elevation;
         low->get_Elevation(&low_elevation);

         (*pTable)(row++,col) << rptRcStation(bvc_station_value, &pDisplayUnits->GetStationFormat());
         (*pTable)(row++,col) << length.SetValue(bvc_elevation);
         (*pTable)(row++,col) << rptRcStation(pvi_station_value, &pDisplayUnits->GetStationFormat());
         (*pTable)(row++,col) << length.SetValue(pvi_elevation);
         (*pTable)(row++,col) << rptRcStation(evc_station_value, &pDisplayUnits->GetStationFormat());
         (*pTable)(row++,col) << length.SetValue(evc_elevation);
         (*pTable)(row++,col) << g1*100 << "%";
         (*pTable)(row++,col) << g2*100 << "%";
         (*pTable)(row++,col) << length.SetValue(L1);
         (*pTable)(row++,col) << length.SetValue(L2);
         (*pTable)(row++,col) << length.SetValue(Length);
         (*pTable)(row++,col) << rptRcStation(high_station_value, &pDisplayUnits->GetStationFormat());
         (*pTable)(row++,col) << length.SetValue(high_elevation);
         (*pTable)(row++,col) << rptRcStation(low_station_value, &pDisplayUnits->GetStationFormat());
         (*pTable)(row++,col) << length.SetValue(low_elevation);
      }

      col++;
   }
}

void write_crown_data(IBroker* pBroker,IEAFDisplayUnits* pDisplayUnits,rptChapter* pChapter,Uint16 level)
{
   GET_IFACE2(pBroker, IRoadwayData, pAlignment ); 
   rptParagraph* pPara;
   pPara = new rptParagraph;
   *pChapter << pPara;

   // Setup the table
   rptRcTable* pTable = pgsReportStyleHolder::CreateDefaultTable(5,"Superelevation Details");
   *pPara << pTable << rptNewLine;

   std::string strSlopeTag = pDisplayUnits->GetAlignmentLengthUnit().UnitOfMeasure.UnitTag();

   (*pTable)(0,0) << "Section";
   (*pTable)(0,1) << "Station";
   (*pTable)(0,2) << "Left Slope" << rptNewLine << "(" << strSlopeTag << "/" << strSlopeTag << ")";
   (*pTable)(0,3) << "Right Slope" << rptNewLine << "(" << strSlopeTag << "/" << strSlopeTag << ")";
   (*pTable)(0,4) << COLHDR("Crown Point Offset", rptLengthUnitTag, pDisplayUnits->GetAlignmentLengthUnit() );

   INIT_UV_PROTOTYPE( rptLengthUnitValue, length, pDisplayUnits->GetAlignmentLengthUnit(), false );

   RoadwaySectionData section = pAlignment->GetRoadwaySectionData();

   RowIndexType row = pTable->GetNumberOfHeaderRows();
   std::vector<CrownData2>::iterator iter;
   for ( iter = section.Superelevations.begin(); iter != section.Superelevations.end(); iter++ )
   {
      CrownData2& crown = *iter;
      (*pTable)(row,0) << row;
      (*pTable)(row,1) << rptRcStation(crown.Station,&pDisplayUnits->GetStationFormat());
      (*pTable)(row,2) << crown.Left;
      (*pTable)(row,3) << crown.Right;
      (*pTable)(row,4) << RPT_OFFSET(crown.CrownPointOffset,length);

      row++;
   }
}

void write_bridge_data(IBroker* pBroker,IEAFDisplayUnits* pDisplayUnits,rptChapter* pChapter,Uint16 level,SpanIndexType span,GirderIndexType gdr)
{
   GET_IFACE2(pBroker, IGirderData, pGirderData);

   rptParagraph* pPara;
   pPara = new rptParagraph;
   *pChapter << pPara;



   // Setup the table
   rptRcTable* pTable = pgsReportStyleHolder::CreateTableNoHeading(2,"General Bridge Information");
   *pPara << pTable << rptNewLine;

   GET_IFACE2(pBroker,IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   SpanIndexType nSpans = pBridgeDesc->GetSpanCount();

   RowIndexType row = 0;

   (*pTable)(row,0) << "Number of Spans";
   (*pTable)(row,1) << nSpans;
   row++;


   // collect all the girder types in use and report the unique names
   std::set<std::string> strGirderNames;
   const CSpanData* pSpan = pBridgeDesc->GetSpan(0);
   while ( pSpan )
   {
      const CGirderTypes* pGirderTypes = pSpan->GetGirderTypes();
      GroupIndexType nGirderGroups = pGirderTypes->GetGirderGroupCount();
      for ( GroupIndexType grpIdx = 0; grpIdx < nGirderGroups; grpIdx++ )
      {
         GirderIndexType firstGdrIdx,lastGdrIdx;
         std::string strGirderName;
         pGirderTypes->GetGirderGroup(grpIdx,&firstGdrIdx,&lastGdrIdx,strGirderName);
         strGirderNames.insert(strGirderName);
      }

      pSpan = pSpan->GetNextPier()->GetNextSpan();
   }

   (*pTable)(row,0) << (1 < strGirderNames.size() ? "Girder Types" : "Girder Type");
   std::set<std::string>::iterator iter;
   for ( iter = strGirderNames.begin(); iter != strGirderNames.end(); iter++ )
   {
      if ( iter != strGirderNames.begin() )
         (*pTable)(row,1) << rptNewLine;

      (*pTable)(row,1) << (*iter);
   }

   row++;
}


void write_concrete_details(IBroker* pBroker,IEAFDisplayUnits* pDisplayUnits,rptChapter* pChapter,SpanIndexType span,GirderIndexType gdr,Uint16 level)
{
   INIT_UV_PROTOTYPE( rptLengthUnitValue,  cmpdim,  pDisplayUnits->GetComponentDimUnit(), false );
   INIT_UV_PROTOTYPE( rptStressUnitValue,  stress,  pDisplayUnits->GetStressUnit(),       false );
   INIT_UV_PROTOTYPE( rptDensityUnitValue, density, pDisplayUnits->GetDensityUnit(),      false );
   INIT_UV_PROTOTYPE( rptStressUnitValue,  modE,    pDisplayUnits->GetModEUnit(),         false );

   rptParagraph* pPara = new rptParagraph;
   *pChapter << pPara;

   GET_IFACE2(pBroker, IBridgeMaterialEx, pMaterial);
   GET_IFACE2(pBroker, IGirderData, pGirderData);


   bool bK1 = (lrfdVersionMgr::ThirdEditionWith2005Interims <= lrfdVersionMgr::GetVersion());

   rptRcTable* pTable = pgsReportStyleHolder::CreateDefaultTable(bK1 ? 16 : 10,"Concrete Properties");
   pTable->SetColumnStyle(0, pgsReportStyleHolder::GetTableCellStyle( CB_NONE | CJ_LEFT) );
   pTable->SetStripeRowColumnStyle(0, pgsReportStyleHolder::GetTableStripeRowCellStyle( CB_NONE | CJ_LEFT) );
   pTable->SetColumnStyle(1, pgsReportStyleHolder::GetTableCellStyle( CB_NONE | CJ_LEFT) );
   pTable->SetStripeRowColumnStyle(1, pgsReportStyleHolder::GetTableStripeRowCellStyle( CB_NONE | CJ_LEFT) );

   *pPara << pTable << rptNewLine;

   ColumnIndexType col = 0;
   RowIndexType row = 0;
   (*pTable)(row,col++) << "Element";
   (*pTable)(row,col++) << "Type";
   (*pTable)(row,col++) << COLHDR(RPT_FCI, rptStressUnitTag, pDisplayUnits->GetStressUnit() );
   (*pTable)(row,col++) << COLHDR(RPT_ECI, rptStressUnitTag, pDisplayUnits->GetStressUnit() );
   (*pTable)(row,col++) << COLHDR(RPT_FC,  rptStressUnitTag, pDisplayUnits->GetStressUnit() );
   (*pTable)(row,col++) << COLHDR(RPT_EC,  rptStressUnitTag, pDisplayUnits->GetStressUnit() );
   (*pTable)(row,col++) << COLHDR(Sub2(symbol(gamma),"w"), rptDensityUnitTag, pDisplayUnits->GetDensityUnit() );
   (*pTable)(row,col++) << COLHDR(Sub2(symbol(gamma),"s"), rptDensityUnitTag, pDisplayUnits->GetDensityUnit() );
   (*pTable)(row,col++) << COLHDR(Sub2("D","agg"), rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit() );
   (*pTable)(row,col++) << COLHDR(Sub2("f","ct"),  rptStressUnitTag, pDisplayUnits->GetStressUnit() );
   if ( bK1 )
   {
      pTable->SetNumberOfHeaderRows(2);
      for ( int i = 0; i < 10; i++ )
      {
         pTable->SetRowSpan(0,i,2); 
         pTable->SetRowSpan(1,i,-1);
      }

      pTable->SetColumnSpan(0,10,2);
      pTable->SetColumnSpan(0,11,-1);
      (*pTable)(0,10) << Sub2("E","c");
      (*pTable)(1,10) << Sub2("K","1");
      (*pTable)(1,11) << Sub2("K","2");

      pTable->SetColumnSpan(0,12,2);
      pTable->SetColumnSpan(0,13,-1);
      (*pTable)(0,12) << "Creep";
      (*pTable)(1,12) << Sub2("K","1");
      (*pTable)(1,13) << Sub2("K","2");

      pTable->SetColumnSpan(0,14,2);
      pTable->SetColumnSpan(0,15,-1);
      (*pTable)(0,14) << "Shrinkage";
      (*pTable)(1,14) << Sub2("K","1");
      (*pTable)(1,15) << Sub2("K","2");
   }


   row = pTable->GetNumberOfHeaderRows();

   GET_IFACE2(pBroker,IBridge,pBridge);
   SpanIndexType nSpans = pBridge->GetSpanCount();
   SpanIndexType firstSpanIdx = (span == ALL_SPANS ? 0 : span);
   SpanIndexType lastSpanIdx  = (span == ALL_SPANS ? nSpans : firstSpanIdx+1);

   for ( SpanIndexType spanIdx = firstSpanIdx; spanIdx < lastSpanIdx; spanIdx++ )
   {
      GirderIndexType nGirders = pBridge->GetGirderCount(spanIdx);
      GirderIndexType firstGirderIdx = min(nGirders-1,(gdr == ALL_GIRDERS ? 0 : gdr));
      GirderIndexType lastGirderIdx  = min(nGirders,  (gdr == ALL_GIRDERS ? nGirders : firstGirderIdx + 1));
      
      for ( GirderIndexType gdrIdx = firstGirderIdx; gdrIdx < lastGirderIdx; gdrIdx++ )
      {
         col = 0;
         (*pTable)(row,col++) << "Span " << LABEL_SPAN(spanIdx) << " Girder " << LABEL_GIRDER(gdrIdx);
         (*pTable)(row,col++) << matConcrete::GetTypeName( (matConcrete::Type)pMaterial->GetGdrConcreteType(spanIdx,gdrIdx), true );
         (*pTable)(row,col++) << stress.SetValue( pMaterial->GetFciGdr(spanIdx,gdrIdx) );
         (*pTable)(row,col++) << modE.SetValue( pMaterial->GetEciGdr(spanIdx,gdrIdx) );
         (*pTable)(row,col++) << stress.SetValue( pMaterial->GetFcGdr(spanIdx,gdrIdx) );
         (*pTable)(row,col++) << modE.SetValue( pMaterial->GetEcGdr(spanIdx,gdrIdx) );
         (*pTable)(row,col++) << density.SetValue( pMaterial->GetWgtDensityGdr(spanIdx,gdrIdx) );

         CGirderData girderData = pGirderData->GetGirderData(spanIdx, gdrIdx);

         if (girderData.Material.bUserEc )
         {
            (*pTable)(row,col++) << "n/a";
         }
         else
         {
            (*pTable)(row,col++) << density.SetValue( pMaterial->GetStrDensityGdr(spanIdx,gdrIdx) );
         }

         (*pTable)(row,col++) << cmpdim.SetValue( pMaterial->GetMaxAggrSizeGdr(spanIdx,gdrIdx) );

         if ( pMaterial->DoesGdrConcreteHaveAggSplittingStrength(spanIdx,gdrIdx) )
            (*pTable)(row,col++) << stress.SetValue( pMaterial->GetGdrConcreteAggSplittingStrength(spanIdx,gdrIdx) );
         else
            (*pTable)(row,col++) << "n/a";


         if (bK1)
         {
            (*pTable)(row,col++) << pMaterial->GetEccK1Gdr(spanIdx,gdrIdx);
            (*pTable)(row,col++) << pMaterial->GetEccK2Gdr(spanIdx,gdrIdx);
            (*pTable)(row,col++) << pMaterial->GetCreepK1Gdr(spanIdx,gdrIdx);
            (*pTable)(row,col++) << pMaterial->GetCreepK2Gdr(spanIdx,gdrIdx);
            (*pTable)(row,col++) << pMaterial->GetShrinkageK1Gdr(spanIdx,gdrIdx);
            (*pTable)(row,col++) << pMaterial->GetShrinkageK2Gdr(spanIdx,gdrIdx);
         }
         row++;
      } // gdrIdx
   } // spanIdx

   GET_IFACE2(pBroker,IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();

   if ( pBridgeDesc->GetDeckDescription()->DeckType != pgsTypes::sdtNone )
   {
      col = 0;
      (*pTable)(row,col++) << "Slab";
      (*pTable)(row,col++) << matConcrete::GetTypeName( (matConcrete::Type)pMaterial->GetSlabConcreteType(), true );
      (*pTable)(row,col++) << "-";
      (*pTable)(row,col++) << "-";
      (*pTable)(row,col++) << stress.SetValue( pMaterial->GetFcSlab() );
      (*pTable)(row,col++) << modE.SetValue( pMaterial->GetEcSlab() );
      (*pTable)(row,col++) << density.SetValue( pMaterial->GetWgtDensitySlab() );

      if (pBridgeDesc->GetDeckDescription()->SlabUserEc)
      {
         (*pTable)(row,col++) << "n/a";
      }
      else
      {
         (*pTable)(row,col++) << density.SetValue( pMaterial->GetStrDensitySlab() );
      }

      (*pTable)(row,col++) << cmpdim.SetValue( pMaterial->GetMaxAggrSizeSlab() );

      if ( pMaterial->DoesSlabConcreteHaveAggSplittingStrength() )
         (*pTable)(row,col++) << stress.SetValue( pMaterial->GetSlabConcreteAggSplittingStrength() );
      else
         (*pTable)(row,col++) << "n/a";

      if (bK1)
      {
         (*pTable)(row,col++) << pMaterial->GetEccK1Slab();
         (*pTable)(row,col++) << pMaterial->GetEccK2Slab();
         (*pTable)(row,col++) << pMaterial->GetCreepK1Slab();
         (*pTable)(row,col++) << pMaterial->GetCreepK2Slab();
         (*pTable)(row,col++) << pMaterial->GetShrinkageK1Slab();
         (*pTable)(row,col++) << pMaterial->GetShrinkageK2Slab();
      }
   }

   (*pPara) << Sub2(symbol(gamma),"w") << " =  Unit weight including reinforcement (used for dead load calculations)" << rptNewLine;
   (*pPara) << Sub2(symbol(gamma),"s") << " =  Unit weight (used to compute " << Sub2("E","c") << ")" << rptNewLine;
   (*pPara) << Sub2("D","agg") << " =  Maximum aggregate size" << rptNewLine;
   (*pPara) << Sub2("f","ct") << " =  Average splitting tensile strength of lightweight aggregate concrete" << rptNewLine;
   if ( bK1 )
   {
      (*pPara) << Sub2("K","1") << " = Correction factor for aggregate type in predicting average value" << rptNewLine;
      (*pPara) << Sub2("K","2") << " = Correction factor for aggregate type in predicting upper and lower bounds" << rptNewLine;
   }
}

void write_pier_data(IBroker* pBroker,IEAFDisplayUnits* pDisplayUnits,rptChapter* pChapter,Uint16 level)
{
   USES_CONVERSION;

   GET_IFACE2(pBroker, ILibrary,     pLib );
   GET_IFACE2(pBroker, IBridge,      pBridge ); 
   GET_IFACE2(pBroker, IRoadwayData, pAlignment);

   GET_IFACE2(pBroker,IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();

   INIT_UV_PROTOTYPE( rptLengthUnitValue, xdim,   pDisplayUnits->GetXSectionDimUnit(),  false );
   INIT_UV_PROTOTYPE( rptLengthUnitValue, cmpdim, pDisplayUnits->GetComponentDimUnit(), false );
   INIT_UV_PROTOTYPE( rptLengthUnitValue, offset, pDisplayUnits->GetAlignmentLengthUnit(), true );

   CComPtr<IAngleDisplayUnitFormatter> angle_formatter;
   angle_formatter.CoCreateInstance(CLSID_AngleDisplayUnitFormatter);
   angle_formatter->put_Signed(VARIANT_FALSE);

   CComPtr<IDirectionDisplayUnitFormatter> direction_formatter;
   direction_formatter.CoCreateInstance(CLSID_DirectionDisplayUnitFormatter);
   direction_formatter->put_BearingFormat(VARIANT_TRUE);

   rptParagraph* pPara = new rptParagraph( pgsReportStyleHolder::GetHeadingStyle() );
   *pChapter << pPara;
   *pPara << "Piers" << rptNewLine;
   pPara = new rptParagraph;
   *pChapter << pPara;

   rptRcTable* pTable = pgsReportStyleHolder::CreateDefaultTable(7,"Pier Layout and Connections");
   *pPara << pTable << rptNewLine;

   pTable->SetNumberOfHeaderRows(2);

   pTable->SetColumnStyle(0,pgsReportStyleHolder::GetTableCellStyle(CB_NONE | CJ_LEFT));
   pTable->SetStripeRowColumnStyle(0,pgsReportStyleHolder::GetTableStripeRowCellStyle(CB_NONE | CJ_LEFT));

   pTable->SetRowSpan(0,0,2);
   pTable->SetRowSpan(1,0,-1);
   (*pTable)(0,0) << "";

   pTable->SetRowSpan(0,1,2);
   pTable->SetRowSpan(1,1,-1);
   (*pTable)(0,1) << "Station";

   pTable->SetRowSpan(0,2,2);
   pTable->SetRowSpan(1,2,-1);
   (*pTable)(0,2) << "Bearing";

   pTable->SetRowSpan(0,3,2);
   pTable->SetRowSpan(1,3,-1);
   (*pTable)(0,3) << "Skew Angle";

   pTable->SetColumnSpan(0,4,2);
   pTable->SetColumnSpan(0,5,-1);
   (*pTable)(0,4) << "Connection Geometry";
   (*pTable)(1,4) << "Back";
   (*pTable)(1,5) << "Ahead";

   pTable->SetRowSpan(0,6,2);
   pTable->SetRowSpan(1,6,-1);
   (*pTable)(0,6) << "Boundary" << rptNewLine << "Condition";


   const CPierData* pPier = pBridgeDesc->GetPier(0);
   RowIndexType row = pTable->GetNumberOfHeaderRows();
   while ( pPier != NULL )
   {
      PierIndexType pierIdx = pPier->GetPierIndex();

      CComPtr<IDirection> bearing;
      CComPtr<IAngle> skew;

      pBridge->GetPierDirection( pierIdx, &bearing );
      pBridge->GetPierSkew( pierIdx, &skew );

      double skew_value;
      skew->get_Value(&skew_value);

      CComBSTR bstrAngle;
      angle_formatter->Format(skew_value,CComBSTR("�,\',\""),&bstrAngle);

      double bearing_value;
      bearing->get_Value(&bearing_value);

      CComBSTR bstrBearing;
      direction_formatter->Format(bearing_value,CComBSTR("�,\',\""),&bstrBearing);

      if ( pPier->GetPrevSpan() == NULL || pPier->GetNextSpan() == NULL )
         (*pTable)(row,0) << "Abutment " << LABEL_PIER(pierIdx);
      else
         (*pTable)(row,0) << "Pier " << LABEL_PIER(pierIdx);

      (*pTable)(row,1) << rptRcStation(pPier->GetStation(), &pDisplayUnits->GetStationFormat() );
      (*pTable)(row,2) << RPT_BEARING(OLE2A(bstrBearing));
      (*pTable)(row,3) << RPT_ANGLE(OLE2A(bstrAngle));

      if ( pPier->GetPrevSpan() )
      {
         (*pTable)(row,4) << pPier->GetConnection(pgsTypes::Back);
      }
      else
      {
         (*pTable)(row,4) << "";
      }

      if ( pPier->GetNextSpan() )
      {
         (*pTable)(row,5) << pPier->GetConnection(pgsTypes::Ahead);
      }
      else
      {
         (*pTable)(row,5) << "";
      }

      (*pTable)(row,6) << CPierData::AsString(pPier->GetConnectionType());

      if ( pPier->GetNextSpan() )
         pPier = pPier->GetNextSpan()->GetNextPier();
      else
         pPier = NULL;

      row++;
   }
}


void write_span_data(IBroker* pBroker,IEAFDisplayUnits* pDisplayUnits,rptChapter* pChapter,Uint16 level)
{
   rptParagraph* pPara;
   pPara = new rptParagraph;
   *pChapter << pPara;

   GET_IFACE2(pBroker,IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();

   // Setup the table
   rptRcTable* pTable = pgsReportStyleHolder::CreateDefaultTable(6,"Spans");
   *pPara << pTable << rptNewLine;

   pTable->SetNumberOfHeaderRows(2);
   pTable->SetRowSpan(0,0,2);
   (*pTable)(0,0) << "Span";

   pTable->SetRowSpan(0,1,2);
   (*pTable)(0,1) << "# Girders";

   pTable->SetRowSpan(1,4,-1);
   pTable->SetRowSpan(1,5,-1);

   pTable->SetColumnSpan(0,2,2);
   (*pTable)(0,2) << "Start of Span";

   if (IsGirderSpacing(pBridgeDesc->GetGirderSpacingType()) )
      (*pTable)(1,0) << "Girder Spacing";
   else
      (*pTable)(1,0) << "Joint Spacing";

   (*pTable)(1,1) << "Datum";

   pTable->SetColumnSpan(0,3,2);
   pTable->SetColumnSpan(0,4,-1);
   pTable->SetColumnSpan(0,5,-1);
   (*pTable)(0,3) << "End of Span";

   if (IsGirderSpacing(pBridgeDesc->GetGirderSpacingType()) )
      (*pTable)(1,2) << "Girder Spacing";
   else
      (*pTable)(1,2) << "Joint Spacing";

   (*pTable)(1,3) << "Datum";

   SpanIndexType nSpans = pBridgeDesc->GetSpanCount();

   RowIndexType row = 2; // data starts on row to because we have 2 heading rows

   for (SpanIndexType spanIdx = 0; spanIdx < nSpans; spanIdx++)
   {
      const CSpanData* pSpan = pBridgeDesc->GetSpan(spanIdx);
      GirderIndexType nGirders = pSpan->GetGirderCount();

      (*pTable)(row,0) << LABEL_SPAN(spanIdx);
      (*pTable)(row,1) << nGirders;

      if ( 1 < nGirders )
      {
         const CGirderSpacing* pStartGirderSpacing = pSpan->GetGirderSpacing(pgsTypes::metStart);
         write_girder_spacing(pBroker,pDisplayUnits,pTable,pStartGirderSpacing,row,2);

         const CGirderSpacing* pEndGirderSpacing = pSpan->GetGirderSpacing(pgsTypes::metEnd);
         write_girder_spacing(pBroker,pDisplayUnits,pTable,pEndGirderSpacing,row,4);
      }
      else
      {
         (*pTable)(row,2) << "-";
         (*pTable)(row,3) << "-";
         (*pTable)(row,4) << "-";
         (*pTable)(row,5) << "-";
      }

      row++;
   }

   pPara = new rptParagraph(pgsReportStyleHolder::GetFootnoteStyle());
   *pChapter << pPara;
   
   if (IsGirderSpacing(pBridgeDesc->GetGirderSpacingType()) )
      *pPara << "Girder Spacing Datum" << rptNewLine;
   else
      *pPara << "Joint Spacing Datum" << rptNewLine;

   *pPara << "(1) Measured normal to the alignment at the centerline of abutment/pier" << rptNewLine;
   *pPara << "(2) Measured normal to the alignment at the centerline of bearing" << rptNewLine;
   *pPara << "(3) Measured at and along the centerline of abutment/pier" << rptNewLine;
   *pPara << "(4) Measured at and along the centerline of bearing" << rptNewLine;
}

void write_girder_spacing(IBroker* pBroker,IEAFDisplayUnits* pDisplayUnits,rptRcTable* pTable,const CGirderSpacing* pGirderSpacing,RowIndexType row,ColumnIndexType col)
{
   GET_IFACE2(pBroker,IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();

   bool bIsGirderSpacing = IsGirderSpacing( pBridgeDesc->GetGirderSpacingType() );

   INIT_UV_PROTOTYPE( rptLengthUnitValue,  xdim, (bIsGirderSpacing ? pDisplayUnits->GetXSectionDimUnit() :  pDisplayUnits->GetComponentDimUnit()),  true );

   GroupIndexType nSpacingGroups = pGirderSpacing->GetSpacingGroupCount();
   for ( GroupIndexType grpIdx = 0; grpIdx < nSpacingGroups; grpIdx++ )
   {
      if ( grpIdx != 0 )
         (*pTable)(row,col) << rptNewLine;

      GirderIndexType firstGdrIdx, lastGdrIdx;
      double spacing;
      pGirderSpacing->GetSpacingGroup(grpIdx,&firstGdrIdx,&lastGdrIdx,&spacing);

      ATLASSERT( firstGdrIdx != lastGdrIdx );
      (*pTable)(row,col) << "Between " << LABEL_GIRDER(firstGdrIdx) << " & " << LABEL_GIRDER(lastGdrIdx) << " : " << xdim.SetValue(spacing);
   }

   if ( pGirderSpacing->GetMeasurementType() == pgsTypes::NormalToItem )
   {
      if ( pGirderSpacing->GetMeasurementLocation() == pgsTypes::AtCenterlinePier )
      {
         (*pTable)(row,col+1) << "1";
      }
      else
      {
         (*pTable)(row,col+1) << "2";
      }
   }
   else
   {
      if ( pGirderSpacing->GetMeasurementLocation() == pgsTypes::AtCenterlinePier )
      {
         (*pTable)(row,col+1) << "3";
      }
      else
      {
         (*pTable)(row,col+1) << "4";
      }
   }
}

void write_ps_data(IBroker* pBroker,IEAFDisplayUnits* pDisplayUnits,rptChapter* pChapter,Uint16 level,SpanIndexType span,GirderIndexType gdr)
{
   INIT_UV_PROTOTYPE( rptLengthUnitValue,  xdim,    pDisplayUnits->GetXSectionDimUnit(),  true );
   INIT_UV_PROTOTYPE( rptLengthUnitValue,  cmpdim,  pDisplayUnits->GetComponentDimUnit(), true );
   INIT_UV_PROTOTYPE( rptForceUnitValue,   force,   pDisplayUnits->GetGeneralForceUnit(), true );
   INIT_UV_PROTOTYPE( rptStressUnitValue,  stress,  pDisplayUnits->GetStressUnit(),       true );
   INIT_UV_PROTOTYPE( rptDensityUnitValue, density, pDisplayUnits->GetDensityUnit(),      true );
   INIT_UV_PROTOTYPE( rptStressUnitValue,  modE,    pDisplayUnits->GetModEUnit(),         true );
   INIT_UV_PROTOTYPE( rptAreaUnitValue,    area,    pDisplayUnits->GetAreaUnit(),         true );
   INIT_UV_PROTOTYPE( rptLengthUnitValue,  dia,     pDisplayUnits->GetComponentDimUnit(), true );

   rptParagraph* pPara;
   pPara = new rptParagraph;
   *pChapter << pPara;

   GET_IFACE2(pBroker, IGirderData,       pGirderData);
   GET_IFACE2(pBroker, IBridge,           pBridge ); 
   GET_IFACE2(pBroker, ILibrary,          pLib );
   GET_IFACE2(pBroker, IStrandGeometry,   pStrand);
   GET_IFACE2(pBroker, IBridgeDescription,pIBridgeDesc);

   const CBridgeDescription* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();

   SpanIndexType nSpans = pBridge->GetSpanCount();
   SpanIndexType firstSpanIdx = (span == ALL_SPANS ? 0 : span);
   SpanIndexType lastSpanIdx  = (span == ALL_SPANS ? nSpans : firstSpanIdx+1);

   for ( SpanIndexType spanIdx = firstSpanIdx; spanIdx < lastSpanIdx; spanIdx++ )
   {
      GirderIndexType nGirders = pBridge->GetGirderCount(spanIdx);
      GirderIndexType firstGirderIdx = min(nGirders-1,(gdr == ALL_GIRDERS ? 0 : gdr));
      GirderIndexType lastGirderIdx  = min(nGirders,  (gdr == ALL_GIRDERS ? nGirders : firstGirderIdx + 1));
      
      for ( GirderIndexType gdrIdx = firstGirderIdx; gdrIdx < lastGirderIdx; gdrIdx++ )
      {
         // Setup the table
         std::ostringstream os;
         os << "Span " << LABEL_SPAN(spanIdx) << " Girder "<<LABEL_GIRDER(gdrIdx) <<std::endl;
         rptRcTable* pTable = pgsReportStyleHolder::CreateTableNoHeading(2,os.str());
         pTable->SetColumnStyle(0,pgsReportStyleHolder::GetTableCellStyle(CB_NONE | CJ_LEFT));
         pTable->SetStripeRowColumnStyle(0,pgsReportStyleHolder::GetTableStripeRowCellStyle(CB_NONE | CJ_LEFT));
         pTable->SetColumnStyle(1,pgsReportStyleHolder::GetTableCellStyle(CB_NONE | CJ_RIGHT));
         pTable->SetStripeRowColumnStyle(1,pgsReportStyleHolder::GetTableStripeRowCellStyle(CB_NONE | CJ_RIGHT));
         *pPara << pTable << rptNewLine;


         RowIndexType row = 0;

         CGirderData girderData = pGirderData->GetGirderData(spanIdx,gdrIdx);

         (*pTable)(row,0) << "Girder Type";
         (*pTable)(row,1) << pBridgeDesc->GetSpan(spanIdx)->GetGirderTypes()->GetGirderName(gdrIdx);
         row++;

         (*pTable)(row,0) << "Slab Offset at Start (\"A\" Dimension)";
         (*pTable)(row,1) << cmpdim.SetValue(pBridge->GetSlabOffset(spanIdx,gdrIdx,pgsTypes::metStart));
         row++;

         (*pTable)(row,0) << "Slab Offset at End (\"A\" Dimension)";
         (*pTable)(row,1) << cmpdim.SetValue(pBridge->GetSlabOffset(spanIdx,gdrIdx,pgsTypes::metEnd));
         row++;

         (*pTable)(row,0) << "Number of Straight Strands";
         (*pTable)(row,1) << pStrand->GetNumStrands(spanIdx,gdrIdx,pgsTypes::Straight);
         StrandIndexType nDebonded = pStrand->GetNumDebondedStrands(spanIdx,gdrIdx,pgsTypes::Straight);
         if ( nDebonded != 0 )
            (*pTable)(row,1) << " (" << nDebonded << " debonded)";
         row++;

         (*pTable)(row,0) << "Straight Strand P" << Sub("jack");
         (*pTable)(row,1) << force.SetValue(girderData.Pjack[pgsTypes::Straight]);
         row++;

         (*pTable)(row,0) << "Number of Harped Strands";
         (*pTable)(row,1) << pStrand->GetNumStrands(spanIdx,gdrIdx,pgsTypes::Harped);
         nDebonded = pStrand->GetNumDebondedStrands(spanIdx,gdrIdx,pgsTypes::Harped);
         if ( nDebonded != 0 )
            (*pTable)(row,1) << " (" << nDebonded << " debonded)";
         row++;

         (*pTable)(row,0) << "Harped Strand P" << Sub("jack");
         (*pTable)(row,1) << force.SetValue(girderData.Pjack[pgsTypes::Harped]);
         row++;

         if ( 0 < pStrand->GetMaxStrands(spanIdx,gdrIdx,pgsTypes::Temporary) )
         {
            (*pTable)(row,0) << "Number of Temporary Strands";
            (*pTable)(row,1) << pStrand->GetNumStrands(spanIdx,gdrIdx,pgsTypes::Temporary);
            nDebonded = pStrand->GetNumDebondedStrands(spanIdx,gdrIdx,pgsTypes::Temporary);
            if ( nDebonded != 0 )
               (*pTable)(row,1) << " (" << nDebonded << " debonded)";
            row++;

            (*pTable)(row,0) << "Temporary Strand P" << Sub("jack");
            (*pTable)(row,1) << force.SetValue(girderData.Pjack[pgsTypes::Temporary]);
            row++;
         }

         std::string endoff;
         if( girderData.HsoEndMeasurement==hsoLEGACY)
         {    // Method used pre-version 6.0
            endoff = "Distance from top-most location in harped strand grid to top-most harped strand at girder ends";
         }
         else if( girderData.HsoEndMeasurement==hsoCGFROMTOP)
         {
            endoff = "Distance from top of girder to CG of harped strands at girder ends";
         }
         else if( girderData.HsoEndMeasurement==hsoCGFROMBOTTOM)
         {
            endoff = "Distance from bottom of girder to CG of harped strands at girder ends";
         }
         else if( girderData.HsoEndMeasurement==hsoTOP2TOP)
         {
            endoff = "Distance from top of girder to top-most harped strand at girder ends";
         }
         else if( girderData.HsoEndMeasurement==hsoTOP2BOTTOM)
         {
            endoff = "Distance from bottom of girder to top-most harped strand at girder ends";
         }
         else if( girderData.HsoEndMeasurement==hsoBOTTOM2BOTTOM)
         {
            endoff = "Distance from bottom of girder to lowest harped strand at girder ends";
         }
         else if( girderData.HsoEndMeasurement==hsoECCENTRICITY)
         {
            endoff = "Eccentricity of harped strand group at girder ends";
         }
         else
            ATLASSERT(0);


         (*pTable)(row,0) << endoff;
         (*pTable)(row,1) << cmpdim.SetValue(girderData.HpOffsetAtEnd);
         row++;

         std::string hpoff;
         if( girderData.HsoHpMeasurement==hsoLEGACY)
         {    // Method used pre-version 6.0
            hpoff = "Distance from lowest location in harped strand grid to lowest harped strand at harping points";
         }
         else if( girderData.HsoHpMeasurement==hsoCGFROMTOP)
         {
            hpoff = "Distance from top of girder to CG of harped strands at harping points";
         }
         else if( girderData.HsoHpMeasurement==hsoCGFROMBOTTOM)
         {
            hpoff = "Distance from bottom of girder to CG of harped strands at harping points";
         }
         else if( girderData.HsoHpMeasurement==hsoTOP2TOP)
         {
            hpoff = "Distance from top of girder to top-most harped strand at harping points";
         }
         else if( girderData.HsoHpMeasurement==hsoTOP2BOTTOM)
         {
            hpoff = "Distance from bottom of girder to top-most harped strand at harping points";
         }
         else if( girderData.HsoHpMeasurement==hsoBOTTOM2BOTTOM)
         {
            hpoff = "Distance from bottom of girder to lowest harped strand at harping points";
         }
         else if( girderData.HsoHpMeasurement==hsoECCENTRICITY)
         {
            hpoff = "Eccentricity of harped strand group at harping points";
         }
         else
            ATLASSERT(0);


         (*pTable)(row,0) << hpoff;
         (*pTable)(row,1) << cmpdim.SetValue(girderData.HpOffsetAtHp);
         row++;

         (*pTable)(row,0) << "Release Strength " << RPT_FCI;
         (*pTable)(row,1) << stress.SetValue( girderData.Material.Fci );
         row++;

         (*pTable)(row,0) << "Final Strength " << RPT_FC;
         (*pTable)(row,1) << stress.SetValue( girderData.Material.Fc );
         row++;

         const matPsStrand* pstrand = pGirderData->GetStrandMaterial(spanIdx,gdrIdx);
         CHECK(pstrand!=0);

         (*pTable)(row,0) << "Prestressing Strand";
         if ( IS_SI_UNITS(pDisplayUnits) )
         {
            (*pTable)(row,1) << dia.SetValue(pstrand->GetNominalDiameter()) << " Dia.";
            std::string strData;

            strData += " ";
            strData += (pstrand->GetGrade() == matPsStrand::Gr1725 ? "Grade 1725" : "Grade 1860");
            strData += " ";
            strData += (pstrand->GetType() == matPsStrand::LowRelaxation ? "Low Relaxation" : "Stress Relieved");

            (*pTable)(row,1) << strData;
         }
         else
         {
            Float64 diam = pstrand->GetNominalDiameter();

            // special designator for 1/2" special (as per High concrete)
            if (IsEqual(diam,0.013208))
               (*pTable)(row,1) << " 1/2\" Special, ";

            (*pTable)(row,1) << dia.SetValue(diam) << " Dia.";
            std::string strData;

            strData += " ";
            strData += (pstrand->GetGrade() == matPsStrand::Gr1725 ? "Grade 250" : "Grade 270");
            strData += " ";
            strData += (pstrand->GetType() == matPsStrand::LowRelaxation ? "Low Relaxation" : "Stress Relieved");

            (*pTable)(row,1) << strData;
         }
         row++;

         switch ( girderData.TempStrandUsage )
         {
         case pgsTypes::ttsPTBeforeLifting:
            *pPara << "NOTE: Temporary strands post-tensioned immediately before lifting" << rptNewLine;
            break;

         case pgsTypes::ttsPTAfterLifting:
            *pPara << "NOTE: Temporary strands post-tensioned immediately after lifting" << rptNewLine;
            break;

         case pgsTypes::ttsPTBeforeShipping:
            *pPara << "NOTE: Temporary strands post-tensioned immediately before shipping" << rptNewLine;
            break;
         }
      } // gdrIdx
   } // spanIdx
}


void write_slab_data(IBroker* pBroker,IEAFDisplayUnits* pDisplayUnits,rptChapter* pChapter,Uint16 level)
{
   rptParagraph* pPara1 = new rptParagraph;
   pPara1->SetStyleName(pgsReportStyleHolder::GetHeadingStyle());
   *pChapter << pPara1;

   *pPara1 << "Deck Geometry";


   rptParagraph* pPara2 = new rptParagraph;
   *pChapter << pPara2;

   INIT_UV_PROTOTYPE( rptLengthUnitValue, dim,      pDisplayUnits->GetComponentDimUnit(),  true );
   INIT_UV_PROTOTYPE( rptLengthUnitValue, overhang, pDisplayUnits->GetXSectionDimUnit(),   true );
   INIT_UV_PROTOTYPE( rptStressUnitValue, olay,     pDisplayUnits->GetOverlayWeightUnit(), true );
   INIT_UV_PROTOTYPE( rptStressUnitValue, stress,   pDisplayUnits->GetStressUnit(),        true );

   rptRcTable* table = pgsReportStyleHolder::CreateTableNoHeading(1,"");
   table->EnableRowStriping(false);
   *pPara2 << table;

   GET_IFACE2(pBroker,IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CDeckDescription* pDeck = pBridgeDesc->GetDeckDescription();
   
   pgsTypes::SupportedDeckType deckType = pDeck->DeckType;

   const GirderLibraryEntry* pGdrEntry = pBridgeDesc->GetSpan(0)->GetGirderTypes()->GetGirderLibraryEntry(0);
   CComPtr<IBeamFactory> pFactory;
   pGdrEntry->GetBeamFactory(&pFactory);
   std::string strPicture = pFactory->GetSlabDimensionsImage(deckType);

   // Slab Types
   (*table)(0,0) << Bold("Deck Type") << rptNewLine;
   switch( deckType )
   {
      case pgsTypes::sdtCompositeCIP:
         (*table)(0,0) << "Composite Cast-in-Place Deck" << rptNewLine;
         break;

      case pgsTypes::sdtCompositeSIP:
         (*table)(0,0) << "Composite Stay-in-Place Deck Panels" << rptNewLine;
         break;

      case pgsTypes::sdtCompositeOverlay:
         (*table)(0,0) << "Composite Overlay" << rptNewLine;
         break;

      case pgsTypes::sdtNone:
         (*table)(0,0) << "No Deck" << rptNewLine;
         break;

      default:
         ATLASSERT(false); // should never get here
   }

   (*table)(0,0) << rptNewLine;

   // Slab Dimensions
   if ( deckType != pgsTypes::sdtNone )
   {
      (*table)(0,0) << Bold("Cross Section") << rptNewLine;
      if ( deckType == pgsTypes::sdtCompositeCIP )
      {
         (*table)(0,0) << "Gross Depth" << " = " << dim.SetValue(pDeck->GrossDepth) << rptNewLine;
      }
      else if  ( deckType == pgsTypes::sdtCompositeSIP )
      {
         (*table)(0,0) << "Cast Depth" << " = " << dim.SetValue(pDeck->GrossDepth) << rptNewLine;
         (*table)(0,0) << "Panel Depth" << " = " << dim.SetValue(pDeck->PanelDepth) << rptNewLine;
         (*table)(0,0) << "Panel Support" << " = " << dim.SetValue(pDeck->PanelSupport) << rptNewLine;
      }
      else if ( deckType == pgsTypes::sdtCompositeOverlay )
      {
         (*table)(0,0) << "Overlay Deck" << rptNewLine;
         (*table)(0,0) << "Gross Depth" << " = " << dim.SetValue(pDeck->GrossDepth) << rptNewLine;
      }

      if ( deckType == pgsTypes::sdtCompositeCIP || deckType == pgsTypes::sdtCompositeSIP )
      {
         (*table)(0,0) << "Overhang Edge Depth" << " = " << dim.SetValue(pDeck->OverhangEdgeDepth) << rptNewLine;
         (*table)(0,0) << "Overhang Taper" << " = ";
         switch(pDeck->OverhangTaper)
         {
         case pgsTypes::None:
            (*table)(0,0) << "None" << rptNewLine;
            break;

         case pgsTypes::TopTopFlange:
            (*table)(0,0) << "Taper overhang to top of girder top flange" << rptNewLine;
            break;

         case pgsTypes::BottomTopFlange:
            (*table)(0,0) << "Taper overhang to bottom of girder top flange" << rptNewLine;
            break;
         }
      }

      (*table)(0,0) << "Fillet = " << dim.SetValue(pDeck->Fillet) << rptNewLine;

      if ( pBridgeDesc->GetSlabOffsetType() == pgsTypes::sotBridge )
      {
         (*table)(0,0) << "Slab Offset (\"A\" Dimension) = " << dim.SetValue(pBridgeDesc->GetSlabOffset()) << rptNewLine;
      }
      else
      {
         (*table)(0,0) << "Slab offset by girder" << rptNewLine;
      }
      (*table)(0,0) << rptNewLine;
   }

   // Plan (Edge of Deck)
   if ( IsAdjustableWidthDeck(pDeck->DeckType) )
   {
      (*table)(0,0) << Bold("Plan (Edge of Deck)") << rptNewLine;

      rptRcTable* deckTable = pgsReportStyleHolder::CreateDefaultTable(4,"");
      (*deckTable)(0,0) << "Station";
      (*deckTable)(0,1) << "Measured" << rptNewLine << "From";
      (*deckTable)(0,2) << COLHDR("Left Offset", rptLengthUnitTag, pDisplayUnits->GetXSectionDimUnit() );
      (*deckTable)(0,3) << COLHDR("Right Offset", rptLengthUnitTag, pDisplayUnits->GetXSectionDimUnit() );

      overhang.ShowUnitTag(false);
      (*table)(0,0) << deckTable << rptNewLine;
      std::vector<CDeckPoint>::const_iterator iter;
      int row = deckTable->GetNumberOfHeaderRows();
      for ( iter = pDeck->DeckEdgePoints.begin(); iter != pDeck->DeckEdgePoints.end(); iter++ )
      {
         const CDeckPoint& dp = *iter;
         (*deckTable)(row,0) << rptRcStation(dp.Station, &pDisplayUnits->GetStationFormat());

         if ( dp.MeasurementType == pgsTypes::omtAlignment )
            (*deckTable)(row,1) << "Alignment";
         else
            (*deckTable)(row,1) << "Bridge Line";

         (*deckTable)(row,2) << overhang.SetValue(dp.LeftEdge);
         (*deckTable)(row,3) << overhang.SetValue(dp.RightEdge);

         row++;

         if ( iter != pDeck->DeckEdgePoints.end()-1 )
         {
            deckTable->SetColumnSpan(row,0,2);
            deckTable->SetColumnSpan(row,1,-1);
            (*deckTable)(row,0) << "Transition";

            switch( dp.LeftTransitionType )
            {
               case pgsTypes::dptLinear:
                  (*deckTable)(row,2) << "Linear";
                  break;

               case pgsTypes::dptSpline:
                  (*deckTable)(row,2) << "Spline";
                  break;

               case pgsTypes::dptParallel:
                  (*deckTable)(row,2) << "Parallel";
                  break;
            }

            switch( dp.RightTransitionType )
            {
               case pgsTypes::dptLinear:
                  (*deckTable)(row,3) << "Linear";
                  break;

               case pgsTypes::dptSpline:
                  (*deckTable)(row,3) << "Spline";
                  break;

               case pgsTypes::dptParallel:
                  (*deckTable)(row,3) << "Parallel";
                  break;
            }
         }

         row++;
      }
   } // end if

   // Roadway Surfacing
   (*table)(0,0) << Bold("Wearing Surface") << rptNewLine;
   switch( pDeck->WearingSurface )
   {
   case pgsTypes::wstSacrificialDepth:
      (*table)(0,0) << "Wearing Surface: Sacrificial Depth of Concrete Slab" << rptNewLine;
      break;
   case pgsTypes::wstOverlay:
      (*table)(0,0) << "Wearing Surface: Overlay" << rptNewLine;
      break;
   case pgsTypes::wstFutureOverlay:
      (*table)(0,0) << "Wearing Surface: Future Overlay" << rptNewLine;
      break;
   default:
      ATLASSERT(false); // shouldn't get here
      break;
   }

   if ( pDeck->WearingSurface == pgsTypes::wstOverlay || pDeck->WearingSurface == pgsTypes::wstFutureOverlay )
   {
      (*table)(0,0) << "Overlay Weight" << " = " << olay.SetValue(pDeck->OverlayWeight) << rptNewLine;
   }

   if ( pDeck->WearingSurface == pgsTypes::wstSacrificialDepth || pDeck->WearingSurface == pgsTypes::wstFutureOverlay )
   {
      if ( deckType != pgsTypes::sdtNone )
         (*table)(0,0) << "Sacrificial Depth" << " = " << dim.SetValue(pDeck->SacrificialDepth) << rptNewLine;
   }

   (*table)(0,0) << rptNewLine;

   // Picture
   (*table)(1,0) << rptRcImage(pgsReportStyleHolder::GetImagePath() + strPicture );
}

void write_deck_reinforcing_data(IBroker* pBroker,IEAFDisplayUnits* pDisplayUnits,rptChapter* pChapter,Uint16 level)
{
   GET_IFACE2(pBroker,IBridgeDescription,pIBridgeDesc);
   const CBridgeDescription* pBridgeDesc = pIBridgeDesc->GetBridgeDescription();
   const CDeckDescription* pDeck = pBridgeDesc->GetDeckDescription();

   INIT_UV_PROTOTYPE( rptLengthUnitValue, cover, pDisplayUnits->GetComponentDimUnit(), true );
   INIT_UV_PROTOTYPE( rptLengthUnitValue, spacing, pDisplayUnits->GetComponentDimUnit(), false );
   INIT_UV_PROTOTYPE( rptLengthUnitValue, cutoff, pDisplayUnits->GetXSectionDimUnit(), false );
   INIT_UV_PROTOTYPE( rptAreaPerLengthValue, As, pDisplayUnits->GetAvOverSUnit(), false );

   const CDeckRebarData& deckRebar = pDeck->DeckRebarData;

   rptParagraph* pPara;
   pPara = new rptParagraph;
   pPara->SetStyleName(pgsReportStyleHolder::GetHeadingStyle());
   *pChapter << pPara;

   *pPara << "Deck Reinforcement";

   pPara = new rptParagraph;
   *pChapter << pPara;

   *pPara << "Reinforcement: " << deckRebar.strRebarMaterial << rptNewLine;
   *pPara << "Top Mat Cover: " << cover.SetValue(deckRebar.TopCover) << rptNewLine;
   *pPara << "Bottom Mat Cover: " << cover.SetValue(deckRebar.BottomCover) << rptNewLine;

   pPara = new rptParagraph;
   *pChapter << pPara;
   *pPara << "Primary Reinforcement - Longitudinal reinforcement running the full length of the bridge" << rptNewLine;
   
   rptRcTable* pTable = pgsReportStyleHolder::CreateDefaultTable(4,"");
   *pPara << pTable << rptNewLine;

   (*pTable)(0,0) << "Mat";
   (*pTable)(0,1) << "Bars";
   (*pTable)(0,2) << "";
   (*pTable)(0,3) << COLHDR("Lump Sum", rptAreaPerLengthUnitTag, pDisplayUnits->GetAvOverSUnit() );

   (*pTable)(1,0) << "Top";
   if ( deckRebar.TopRebarKey == INVALID_BAR_SIZE )
      (*pTable)(1,1) << "None @ " << spacing.SetValue( deckRebar.TopSpacing );
   else
      (*pTable)(1,1) << "#" << deckRebar.TopRebarKey << " @ " << spacing.SetValue( deckRebar.TopSpacing );

   (*pTable)(1,2) << "AND";
   (*pTable)(1,3) << As.SetValue( deckRebar.TopLumpSum );

   (*pTable)(2,0) << "Bottom";
   if ( deckRebar.BottomRebarKey == INVALID_BAR_SIZE )
      (*pTable)(2,1) << "None @ " << spacing.SetValue( deckRebar.BottomSpacing );
   else
      (*pTable)(2,1) << "#" << deckRebar.BottomRebarKey << " @ " << spacing.SetValue( deckRebar.BottomSpacing );

   (*pTable)(2,2) << "AND";
   (*pTable)(2,3) << As.SetValue( deckRebar.BottomLumpSum );

   pPara = new rptParagraph;
   *pChapter << pPara;
   *pPara << "Supplemental Reinforcement - Negative moment reinforcement at continuous piers" << rptNewLine;

   if ( deckRebar.NegMomentRebar.size() == 0 )
   {
      *pPara << rptNewLine;
      *pPara << "No supplemental reinforcement";
      *pPara << rptNewLine;
   }
   else
   {
      pTable = pgsReportStyleHolder::CreateDefaultTable(7,"");
      *pPara << pTable << rptNewLine;
      (*pTable)(0,0) << "Pier";
      (*pTable)(0,1) << "Mat";
      (*pTable)(0,2) << COLHDR(Sub2("A","s"), rptAreaPerLengthUnitTag, pDisplayUnits->GetAvOverSUnit() );
      (*pTable)(0,3) << "Bar";
      (*pTable)(0,4) << COLHDR("Spacing",rptLengthUnitTag, pDisplayUnits->GetComponentDimUnit() );
      (*pTable)(0,5) << COLHDR("Left" << rptNewLine << "Cutoff",rptLengthUnitTag, pDisplayUnits->GetXSectionDimUnit() );
      (*pTable)(0,6) << COLHDR("Right" << rptNewLine << "Cutoff",rptLengthUnitTag, pDisplayUnits->GetXSectionDimUnit() );

      RowIndexType row = pTable->GetNumberOfHeaderRows();
      std::vector<CDeckRebarData::NegMomentRebarData>::const_iterator iter;
      for ( iter = deckRebar.NegMomentRebar.begin(); iter != deckRebar.NegMomentRebar.end(); iter++ )
      {
         const CDeckRebarData::NegMomentRebarData& negMomentRebar = *iter;
         (*pTable)(row,0) << (negMomentRebar.PierIdx+1L);
         (*pTable)(row,1) << (negMomentRebar.Mat == CDeckRebarData::TopMat ? "Top" : "Bottom");
         (*pTable)(row,2) << As.SetValue(negMomentRebar.LumpSum);
         if ( negMomentRebar.RebarKey < 0 )
            (*pTable)(row,3) << "None"; 
         else
            (*pTable)(row,3) << "#" << negMomentRebar.RebarKey; 

         (*pTable)(row,4) << spacing.SetValue(negMomentRebar.Spacing);
         (*pTable)(row,5) << cutoff.SetValue(negMomentRebar.LeftCutoff);
         (*pTable)(row,6) << cutoff.SetValue(negMomentRebar.RightCutoff);

         row++;
      }
   }
}