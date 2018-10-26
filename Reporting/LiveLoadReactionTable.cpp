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

#include "StdAfx.h"
#include <Reporting\LiveLoadReactionTable.h>
#include <Reporting\ReportNotes.h>
#include <Reporting\ReactionInterfaceAdapters.h>

#include <IFace\Project.h>
#include <IFace\Bridge.h>
#include <EAF\EAFDisplayUnits.h>
#include <IFace\AnalysisResults.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/****************************************************************************
CLASS
   CLiveLoadReactionTable
****************************************************************************/


////////////////////////// PUBLIC     ///////////////////////////////////////

//======================== LIFECYCLE  =======================================
CLiveLoadReactionTable::CLiveLoadReactionTable()
{
}

CLiveLoadReactionTable::CLiveLoadReactionTable(const CLiveLoadReactionTable& rOther)
{
   MakeCopy(rOther);
}

CLiveLoadReactionTable::~CLiveLoadReactionTable()
{
}

//======================== OPERATORS  =======================================
CLiveLoadReactionTable& CLiveLoadReactionTable::operator= (const CLiveLoadReactionTable& rOther)
{
   if( this != &rOther )
   {
      MakeAssignment(rOther);
   }

   return *this;
}

//======================== OPERATIONS =======================================
void CLiveLoadReactionTable::Build(IBroker* pBroker, rptChapter* pChapter,
                                          SpanIndexType span,GirderIndexType girder,
                                          IEAFDisplayUnits* pDisplayUnits, TableType tableType,
                                          pgsTypes::Stage stage, pgsTypes::AnalysisType analysisType) const
{
   // Build table
   INIT_UV_PROTOTYPE( rptLengthUnitValue, location, pDisplayUnits->GetSpanLengthUnit(), false );
   INIT_UV_PROTOTYPE( rptForceSectionValue, reaction, pDisplayUnits->GetShearUnit(), false );

   GET_IFACE2(pBroker,IBridge,pBridge);

   bool bPermit = false;

   rptParagraph* p = new rptParagraph;
   *pChapter << p;

   rptRcTable* p_table=0;

   PierIndexType nPiers = pBridge->GetPierCount();

   PierIndexType startPier = (span == ALL_SPANS ? 0 : span);
   PierIndexType endPier   = (span == ALL_SPANS ? nPiers : startPier+2 );

   ColumnIndexType nCols;

   GET_IFACE2(pBroker,ILiveLoads,pLiveLoads);
   bPermit = pLiveLoads->IsLiveLoadDefined(pgsTypes::lltPermit);

   GET_IFACE2(pBroker,IProductLoads,pProductLoads);
   bool bPedLoading = pProductLoads->HasPedestrianLoad(startPier,girder);

	if ( analysisType == pgsTypes::Envelope )
		nCols = 9;
	else
		nCols = 7;

   if ( bPermit )
      nCols += 4;

   if ( bPedLoading )
      nCols += 2;

 	p_table = pgsReportStyleHolder::CreateDefaultTable(nCols, tableType==PierReactionsTable ?_T("Total Girderline Reactions at Abutments and Piers"): _T("Girder Bearing Reactions") );

   p_table->SetNumberOfHeaderRows(2);

   p_table->SetRowSpan(0,0,2);
   p_table->SetRowSpan(1,0,SKIP_CELL);
   (*p_table)(0,0) << _T("");

 	ColumnIndexType col1 = 1;
   ColumnIndexType col2 = 1;
	if ( analysisType == pgsTypes::Envelope )
	{
      p_table->SetColumnSpan(0,col1,2);
      (*p_table)(0,col1++) << symbol(SUM) << _T("DC");
      (*p_table)(1,col2++) << COLHDR(_T("Max"), rptForceUnitTag, pDisplayUnits->GetShearUnit() );
      (*p_table)(1,col2++) << COLHDR(_T("Min"), rptForceUnitTag, pDisplayUnits->GetShearUnit() );

      p_table->SetColumnSpan(0,col1,2);
      (*p_table)(0,col1++) << symbol(SUM) << _T("DW");
      (*p_table)(1,col2++) << COLHDR(_T("Max"), rptForceUnitTag, pDisplayUnits->GetShearUnit() );
      (*p_table)(1,col2++) << COLHDR(_T("Min"), rptForceUnitTag, pDisplayUnits->GetShearUnit() );
	}
	else
	{
      p_table->SetRowSpan(0,col1,2);
		(*p_table)(0,col1++) << COLHDR(symbol(SUM) << _T("DC"),          rptForceUnitTag, pDisplayUnits->GetShearUnit() );

      p_table->SetRowSpan(0,col1,2);
		(*p_table)(0,col1++) << COLHDR(symbol(SUM) << _T("DW"),          rptForceUnitTag, pDisplayUnits->GetShearUnit() );

      p_table->SetRowSpan(1,col2++,SKIP_CELL);
      p_table->SetRowSpan(1,col2++,SKIP_CELL);
	}

   if ( bPedLoading )
   {
      p_table->SetColumnSpan(0,col1,2);
      (*p_table)(0,col1++) << _T("* PL");
		(*p_table)(1,col2++) << COLHDR(_T("Max"),       rptForceUnitTag, pDisplayUnits->GetShearUnit() );
		(*p_table)(1,col2++) << COLHDR(_T("Min"),       rptForceUnitTag, pDisplayUnits->GetShearUnit() );
   }

   p_table->SetColumnSpan(0,col1,2);
   (*p_table)(0,col1++) << _T("* LL Design");
	(*p_table)(1,col2++) << COLHDR(_T("Max"),       rptForceUnitTag, pDisplayUnits->GetShearUnit() );
	(*p_table)(1,col2++) << COLHDR(_T("Min"),       rptForceUnitTag, pDisplayUnits->GetShearUnit() );

	if ( bPermit )
	{
      p_table->SetColumnSpan(0,col1,2);
      (*p_table)(0,col1++) << _T("* LL Permit");
		(*p_table)(1,col2++) << COLHDR(_T("Max"),       rptForceUnitTag, pDisplayUnits->GetShearUnit() );
		(*p_table)(1,col2++) << COLHDR(_T("Min"),       rptForceUnitTag, pDisplayUnits->GetShearUnit() );
	}

   p_table->SetColumnSpan(0,col1,2);
   (*p_table)(0,col1++) << _T("Strength I");
	(*p_table)(1,col2++) << COLHDR(_T("Max"), rptForceUnitTag, pDisplayUnits->GetShearUnit() );
	(*p_table)(1,col2++) << COLHDR(_T("Min"), rptForceUnitTag, pDisplayUnits->GetShearUnit() );

	if ( bPermit )
	{
      p_table->SetColumnSpan(0,col1,2);
		(*p_table)(0,col1++) << _T("Strength II");
		(*p_table)(1,col2++) << COLHDR(_T("Max"), rptForceUnitTag, pDisplayUnits->GetShearUnit() );
		(*p_table)(1,col2++) << COLHDR(_T("Min"), rptForceUnitTag, pDisplayUnits->GetShearUnit() );
	}

   for ( ColumnIndexType i = col1; i < nCols; i++ )
      p_table->SetColumnSpan(0,i,SKIP_CELL);

   *p << p_table;
   *p << LIVELOAD_PER_GIRDER_NO_IMPACT << rptNewLine;

   // Get the interface pointers we need
   GET_IFACE2(pBroker,ICombinedForces,pCmbForces);
   GET_IFACE2(pBroker,ILimitStateForces,pLsForces);
   GET_IFACE2(pBroker,IBearingDesign,pBearingDesign);

   std::auto_ptr<ICmbLsReactionAdapter> pForces;
   if(  tableType==PierReactionsTable )
   {
      pForces =  std::auto_ptr<ICmbLsReactionAdapter>(new CombinedLsForcesReactionAdapter(pCmbForces,pLsForces));
   }

   else
   {
      pForces =  std::auto_ptr<ICmbLsReactionAdapter>(new CmbLsBearingDesignReactionAdapter(pBearingDesign, span) );
   }

   (*p_table)(0,0) << _T("");

   // Fill up the table
   Float64 min, max;
   RowIndexType row = 2;
   for ( PierIndexType pier = startPier; pier < endPier; pier++ )
   {
      if (! pForces->DoReportAtPier(pier, girder) )
      {
         continue; // don't report piers that have no bearing information
      }


      if (pier == 0 || pier == pBridge->GetPierCount()-1 )
         (*p_table)(row,0) << _T("Abutment ") << LABEL_PIER(pier);
      else
         (*p_table)(row,0) << _T("Pier ") << LABEL_PIER(pier);

     ColumnIndexType col = 1;

     if ( analysisType == pgsTypes::Envelope )
     {
        (*p_table)(row,col++) << reaction.SetValue( pForces->GetReaction( lcDC, stage, pier, girder, ctCummulative, MaxSimpleContinuousEnvelope ) );
        (*p_table)(row,col++) << reaction.SetValue( pForces->GetReaction( lcDC, stage, pier, girder, ctCummulative, MinSimpleContinuousEnvelope ) );
        (*p_table)(row,col++) << reaction.SetValue( pForces->GetReaction( lcDW, stage, pier, girder, ctCummulative, MaxSimpleContinuousEnvelope ) );
        (*p_table)(row,col++) << reaction.SetValue( pForces->GetReaction( lcDW, stage, pier, girder, ctCummulative, MinSimpleContinuousEnvelope ) );

        if ( bPedLoading )
        {
           pForces->GetCombinedLiveLoadReaction( pgsTypes::lltPedestrian, pgsTypes::BridgeSite3, pier, girder, MaxSimpleContinuousEnvelope, false, &min, &max );
           (*p_table)(row,col++) << reaction.SetValue( max );

           pForces->GetCombinedLiveLoadReaction( pgsTypes::lltPedestrian, pgsTypes::BridgeSite3, pier, girder, MinSimpleContinuousEnvelope, false, &min, &max );
           (*p_table)(row,col++) << reaction.SetValue( min );
        }

        pForces->GetCombinedLiveLoadReaction( pgsTypes::lltDesign, pgsTypes::BridgeSite3, pier, girder, MaxSimpleContinuousEnvelope, false, &min, &max );
        (*p_table)(row,col++) << reaction.SetValue( max );

        pForces->GetCombinedLiveLoadReaction( pgsTypes::lltDesign, pgsTypes::BridgeSite3, pier, girder, MinSimpleContinuousEnvelope, false, &min, &max );
        (*p_table)(row,col++) << reaction.SetValue( min );

        if ( bPermit )
        {
           pForces->GetCombinedLiveLoadReaction( pgsTypes::lltPermit, pgsTypes::BridgeSite3, pier, girder, MaxSimpleContinuousEnvelope, false, &min, &max );
           (*p_table)(row,col++) << reaction.SetValue( max );

           pForces->GetCombinedLiveLoadReaction( pgsTypes::lltPermit, pgsTypes::BridgeSite3, pier, girder, MinSimpleContinuousEnvelope, false, &min, &max );
           (*p_table)(row,col++) << reaction.SetValue( min );
        }

        pForces->GetReaction( pgsTypes::StrengthI, stage, pier, girder, MaxSimpleContinuousEnvelope, false, &min, &max );
        (*p_table)(row,col++) << reaction.SetValue( max );

        pForces->GetReaction( pgsTypes::StrengthI, stage, pier, girder, MinSimpleContinuousEnvelope, false, &min, &max );
        (*p_table)(row,col++) << reaction.SetValue( min );

        if ( bPermit )
        {
           pForces->GetReaction( pgsTypes::StrengthII, stage, pier, girder, MaxSimpleContinuousEnvelope, false, &min, &max );
           (*p_table)(row,col++) << reaction.SetValue( max );

           pForces->GetReaction( pgsTypes::StrengthII, stage, pier, girder, MinSimpleContinuousEnvelope, false, &min, &max );
           (*p_table)(row,col++) << reaction.SetValue( min );
        }
     }
     else
     {
        BridgeAnalysisType bat = (analysisType == pgsTypes::Simple ? SimpleSpan : ContinuousSpan);
        (*p_table)(row,col++) << reaction.SetValue( pForces->GetReaction( lcDC, stage, pier, girder, ctCummulative, bat ) );
        (*p_table)(row,col++) << reaction.SetValue( pForces->GetReaction( lcDW, stage, pier, girder, ctCummulative, bat ) );

        if ( bPedLoading )
        {
           pForces->GetCombinedLiveLoadReaction( pgsTypes::lltPedestrian, pgsTypes::BridgeSite3, pier, girder, bat, false, &min, &max );
           (*p_table)(row,col++) << reaction.SetValue( max );
           (*p_table)(row,col++) << reaction.SetValue( min );
        }

        pForces->GetCombinedLiveLoadReaction( pgsTypes::lltDesign, pgsTypes::BridgeSite3, pier, girder, bat, false, &min, &max );
        (*p_table)(row,col++) << reaction.SetValue( max );
        (*p_table)(row,col++) << reaction.SetValue( min );

        if ( bPermit )
        {
           pForces->GetCombinedLiveLoadReaction( pgsTypes::lltPermit, pgsTypes::BridgeSite3, pier, girder, bat, false, &min, &max );
           (*p_table)(row,col++) << reaction.SetValue( max );
           (*p_table)(row,col++) << reaction.SetValue( min );
        }

        pForces->GetReaction( pgsTypes::StrengthI, stage, pier, girder, bat, false, &min, &max );
        (*p_table)(row,col++) << reaction.SetValue( max );
        (*p_table)(row,col++) << reaction.SetValue( min );

        if ( bPermit )
        {
           pForces->GetReaction( pgsTypes::StrengthII, stage, pier, girder, bat, false, &min, &max );
           (*p_table)(row,col++) << reaction.SetValue( max );
           (*p_table)(row,col++) << reaction.SetValue( min );
        }
     }

      row++;
   }
}

//======================== ACCESS     =======================================
//======================== INQUIRY    =======================================

////////////////////////// PROTECTED  ///////////////////////////////////////

//======================== LIFECYCLE  =======================================
//======================== OPERATORS  =======================================
//======================== OPERATIONS =======================================
void CLiveLoadReactionTable::MakeCopy(const CLiveLoadReactionTable& rOther)
{
   // Add copy code here...
}

void CLiveLoadReactionTable::MakeAssignment(const CLiveLoadReactionTable& rOther)
{
   MakeCopy( rOther );
}
