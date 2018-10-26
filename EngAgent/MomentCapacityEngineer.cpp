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
#include "MomentCapacityEngineer.h"
#include "..\PGSuperException.h"

#include <IFace\Bridge.h>
#include <IFace\AnalysisResults.h>
#include <IFace\MomentCapacity.h>
#include <IFace\PrestressForce.h>
#include <IFace\Project.h>
#include <IFace\StatusCenter.h>
#include <IFace\DisplayUnits.h>

#include <PgsExt\statusitem.h>
#include <PgsExt\GirderLabel.h>

#include <Lrfd\ConcreteUtil.h>

#include <algorithm>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

DIAG_DEFINE_GROUP(MomCap,DIAG_GROUP_DISABLE,0);

static const double ANGLE_TOL=1.0e-6;
static const double D_TOL=1.0e-10;

/****************************************************************************
CLASS
   pgsMomentCapacityEngineer
****************************************************************************/

////////////////////////// PUBLIC     ///////////////////////////////////////

//======================== LIFECYCLE  =======================================
pgsMomentCapacityEngineer::pgsMomentCapacityEngineer(IBroker* pBroker,AgentIDType agentID)
{
   m_pBroker = pBroker;
   m_AgentID = agentID;
   CREATE_LOGFILE("MomentCapacity");
}

pgsMomentCapacityEngineer::pgsMomentCapacityEngineer(const pgsMomentCapacityEngineer& rOther)
{
   MakeCopy(rOther);
}

pgsMomentCapacityEngineer::~pgsMomentCapacityEngineer()
{
   CLOSE_LOGFILE;
}

//======================== OPERATORS  =======================================
pgsMomentCapacityEngineer& pgsMomentCapacityEngineer::operator= (const pgsMomentCapacityEngineer& rOther)
{
   if( this != &rOther )
   {
      MakeAssignment(rOther);
   }

   return *this;
}

//======================== OPERATIONS =======================================
void pgsMomentCapacityEngineer::SetBroker(IBroker* pBroker)
{
   m_pBroker = pBroker;
}

void pgsMomentCapacityEngineer::SetAgentID(AgentIDType agentID)
{
   m_AgentID = agentID;

   GET_IFACE(IStatusCenter,pStatusCenter);
   m_scidUnknown = pStatusCenter->RegisterCallback( new pgsUnknownErrorStatusCallback() );
}

void pgsMomentCapacityEngineer::ComputeMomentCapacity(pgsTypes::Stage stage,const pgsPointOfInterest& poi,bool bPositiveMoment,MOMENTCAPACITYDETAILS* pmcd)
{
   SpanIndexType span  = poi.GetSpan();
   GirderIndexType gdr = poi.GetGirder();

   GET_IFACE(IPrestressForce, pPrestressForce);
   GET_IFACE(IBridgeMaterial,pMaterial);
   const matPsStrand* pStrand = pMaterial->GetStrand(span,gdr);

   Float64 Eps = pStrand->GetE();
   Float64 fpe = 0.0; // effective prestress after all losses
   Float64 e_initial = 0.0; // initial strain in the prestressing strands (strain at effect prestress)
   if ( bPositiveMoment )
   {
      // only for positive moment... strands are ignored for negative moment analysis
      fpe = pPrestressForce->GetStrandStress(poi,pgsTypes::Permanent,pgsTypes::AfterLosses);
      e_initial = fpe/Eps;
   }

   GET_IFACE(IStrandGeometry,pStrandGeom);
   StrandIndexType Ns = pStrandGeom->GetNumStrands(span,gdr,pgsTypes::Straight);
   StrandIndexType Nh = pStrandGeom->GetNumStrands(span,gdr,pgsTypes::Harped);

   GET_IFACE(IBridge,pBridge);
   GDRCONFIG config = pBridge->GetGirderConfiguration(span,gdr);

   pgsBondTool bondTool(m_pBroker,poi);

   ComputeMomentCapacity(stage,poi,config,fpe,e_initial,bondTool,bPositiveMoment,pmcd);
}

void pgsMomentCapacityEngineer::ComputeMomentCapacity(pgsTypes::Stage stage,const pgsPointOfInterest& poi,const GDRCONFIG& config,bool bPositiveMoment,MOMENTCAPACITYDETAILS* pmcd)
{
   SpanIndexType span  = poi.GetSpan();
   GirderIndexType gdr = poi.GetGirder();

   GET_IFACE(IPrestressForce, pPrestressForce);
   GET_IFACE(IBridgeMaterial,pMaterial);
   const matPsStrand* pStrand = pMaterial->GetStrand(span,gdr);

   Float64 Eps = pStrand->GetE();
   Float64 fpe = 0.0; // effective prestress after all losses
   Float64 e_initial = 0.0; // initial strain in the prestressing strands (strain at effect prestress)
   if ( bPositiveMoment )
   {
      // only for positive moment... strands are ignored for negative moment analysis
      fpe = pPrestressForce->GetStrandStress(poi,pgsTypes::Permanent,config,pgsTypes::AfterLosses);
      e_initial = fpe/Eps;
   }

   pgsBondTool bondTool(m_pBroker,poi,config);

   ComputeMomentCapacity(stage,poi,config,fpe,e_initial,bondTool,bPositiveMoment,pmcd);
}

void pgsMomentCapacityEngineer::ComputeMomentCapacity(pgsTypes::Stage stage,const pgsPointOfInterest& poi,const GDRCONFIG& config,double fpe,double e_initial,pgsBondTool& bondTool,bool bPositiveMoment,MOMENTCAPACITYDETAILS* pmcd)
{
   GET_IFACE(IBridge, pBridge);
   GET_IFACE(IPrestressForce, pPrestressForce);
   GET_IFACE(ILongRebarGeometry, pLongRebarGeom);

   SpanIndexType span  = poi.GetSpan();
   GirderIndexType gdr = poi.GetGirder();

   StrandIndexType Ns = config.Nstrands[pgsTypes::Straight];
   StrandIndexType Nh = config.Nstrands[pgsTypes::Harped];

   // create a problem to solve
   CComPtr<IGeneralSection> section;
   CComPtr<IPoint2d> pntCompression; // needed to figure out the result geometry
   CComPtr<ISize2d> szOffset; // distance to offset coordinates from bridge model to capacity model
   std::map<long,double> bond_factors[2];
   double dt; // depth from top of section to extreme layer of tensile reinforcement
   BuildCapacityProblem(stage,poi,config,e_initial,bondTool,bPositiveMoment,&section,&pntCompression,&szOffset,&dt,bond_factors);

#if defined _DEBUG_SECTION_DUMP
   DumpSection(poi,section,bond_factors[0],bond_factors[1],bPositiveMoment);
#endif // _DEBUG_SECTION_DUMP

   // create solver
   CComPtr<IMomentCapacitySolver> solver;
   HRESULT hr = solver.CoCreateInstance(CLSID_MomentCapacitySolver);

   if ( FAILED(hr) )
   {
      THROW_SHUTDOWN("Installation Problem - Unable to create RC Capacity Solver",XREASON_COMCREATE_ERROR,true);
   }

   solver->putref_Section(section);
   //solver->put_Slices(30);
   solver->put_Slices(10);
   solver->put_SliceGrowthFactor(3);
   solver->put_MaxIterations(50);

   // Set the convergence tolerance to 0.1N. This is more than accurate enough for the
   // output display. Output accurace for SI = 0.01kN = 10N, for US = 0.01kip = 45N
   solver->put_AxialTolerance(0.1);

   // determine neutral axis angle
   // compression is on the left side of the neutral axis
   double na_angle = (bPositiveMoment ? 0.00 : M_PI);

   // compressive strain limit
   double ec = -0.003; 

   CComPtr<IMomentCapacitySolution> solution;

#if defined _DEBUG
   CTime startTime = CTime::GetCurrentTime();
#endif // _DEBUG

   hr = solver->Solve(0.00,na_angle,ec,smFixedCompressiveStrain,&solution);

   if ( hr == RC_E_MATERIALFAILURE )
   {
      WATCHX(MomCap,0,"Exceeded material strain limit");
      hr = S_OK;
   }
   
   // It is ok if this assert fires... All it means is that the solver didn't find a solution
   // on its first try. The purpose of this assert is to help gauge how often this happens.
   // Second and third attempts are made below
   ATLASSERT(SUCCEEDED(hr));
   if ( hr == RC_E_INITCONCRETE )   ATLASSERT(SUCCEEDED(hr));
   if ( hr == RC_E_SOLUTIONNOTFOUND )   ATLASSERT(SUCCEEDED(hr));
   if ( hr == RC_E_BEAMNOTSYMMETRIC )   ATLASSERT(SUCCEEDED(hr));
   if ( hr == RC_E_MATERIALFAILURE )   ATLASSERT(SUCCEEDED(hr));

   if (FAILED(hr))
   {
      // Try again with more slices
      //solver->put_Slices(50);
      solver->put_Slices(20);
      solver->put_SliceGrowthFactor(2);
      solver->put_AxialTolerance(1.0);
      hr = solver->Solve(0.00,na_angle,ec,smFixedCompressiveStrain,&solution);

      if ( hr == RC_E_MATERIALFAILURE )
      {
         WATCHX(MomCap,0,"Exceeded material strain limit");
         hr = S_OK;
      }

      if ( FAILED(hr) )
      {
         // Try again with more slices
         //solver->put_Slices(100);
         solver->put_Slices(50);
         solver->put_SliceGrowthFactor(2);
         solver->put_AxialTolerance(10.0);
         hr = solver->Solve(0.00,na_angle,ec,smFixedCompressiveStrain,&solution);

         if ( hr == RC_E_MATERIALFAILURE )
         {
            WATCHX(MomCap,0,"Exceeded material strain limit");
            hr = S_OK;
         }

         if ( FAILED(hr) )
         {
            GET_IFACE(IStatusCenter,pStatusCenter);
            GET_IFACE(IDisplayUnits,pDisplayUnits);

            const unitmgtLengthData& unit = pDisplayUnits->GetSpanLengthUnit();
            CString msg;
            msg.Format("An unknown error occured while computing %s moment capacity for Span %d Girder %s at %f %s from the left end of the girder (%d)",
                        (bPositiveMoment ? "positive" : "negative"),
                        poi.GetSpan()+1,
                        LABEL_GIRDER(poi.GetGirder()),
                        ::ConvertFromSysUnits(poi.GetDistFromStart(),unit.UnitOfMeasure),
                        unit.UnitOfMeasure.UnitTag().c_str(),
                        hr);
            pgsUnknownErrorStatusItem* pStatusItem = new pgsUnknownErrorStatusItem(m_AgentID,m_scidUnknown,__FILE__,__LINE__,msg);
            pStatusCenter->Add(pStatusItem);
            THROW_UNWIND(msg,-1);
         }
      }
   }


#if defined _DEBUG
   CTime endTime = CTime::GetCurrentTime();
   CTimeSpan duration = endTime - startTime;
   WATCHX(MomCap,0,"Duration = " << duration.GetTotalSeconds() << " seconds");
#endif // _DEBUG

   pmcd->CapacitySolution = solution;

   double Fz,Mx,My;
   CComPtr<IPlane3d> strains;
   solution->get_Fz(&Fz);
   solution->get_Mx(&Mx);
   solution->get_My(&My);
   solution->get_StrainPlane(&strains);

   ATLASSERT( IsZero(Fz,0.1) );
   ATLASSERT( Mx != 0.0 ? IsZero(My/Mx,0.05) : true );  // when there is an odd number of harped strands, the strands aren't always symmetrical
                                     // this will cause a small amount of off axis bending.
                                     // Only assert if the ratio of My/Mx is larger that the tolerance for zero

   double mn;
   mn = -Mx;

   pmcd->Mn  = mn;

   pmcd->PPR = (bPositiveMoment ? pLongRebarGeom->GetPPRBottomHalf(poi,config) : 0.0);

   pmcd->Phi = 0.9 + 0.1*(pmcd->PPR);

   double C,T;
   solution->get_CompressionResultant(&C);
   solution->get_TensionResultant(&T);
   ATLASSERT(IsZero(C+T,0.5));
   
   pmcd->C = C;
   pmcd->T = T;

   CComPtr<IPoint2d> cgC, cgT;
   solution->get_CompressionResultantLocation(&cgC);
   solution->get_TensionResultantLocation(&cgT);

   double fps_avg = 0;

   if ( !IsZero(mn) )
   {
      pmcd->MomentArm = fabs(mn/T);

      double x1,y1, x2,y2;
      pntCompression->get_X(&x1);
      pntCompression->get_Y(&y1);

      cgC->get_X(&x2);
      cgC->get_Y(&y2);

      pmcd->dc = sqrt((x2-x1)*(x2-x1) + (y2-y1)*(y2-y1));
      pmcd->de = pmcd->dc + pmcd->MomentArm;

      CComPtr<IPlane3d> strainPlane;
      solution->get_StrainPlane(&strainPlane);
      double x,y,z;
      x = 0;
      z = 0;
      strainPlane->GetY(x,z,&y);

      pmcd->c = sqrt((x1-x)*(x1-x) + (y1-y)*(y1-y));

      double dx,dy;
      szOffset->get_Dx(&dx);
      szOffset->get_Dy(&dy);

      // determine average stress in strands
      CComPtr<IStressStrain> ssStrand;
      ssStrand.CoCreateInstance(CLSID_PSPowerFormula);
      GET_IFACE(IStrandGeometry,pStrandGeom);
      for ( int i = 0; i < 2; i++ ) // straight and harped strands
      {
         pgsTypes::StrandType strandType = (pgsTypes::StrandType)(i);
         CComPtr<IPoint2dCollection> points;
         pStrandGeom->GetStrandPositionsEx(poi, (i == 0 ? Ns : Nh), strandType, &points);

         long strandPos = 0;
         CComPtr<IEnumPoint2d> enum_points;
         points->get__Enum(&enum_points);
         CComPtr<IPoint2d> point;
         while ( enum_points->Next(1,&point,NULL) != S_FALSE )
         {
            double bond_factor = bond_factors[i][strandPos++];

            point->get_X(&x);
            point->get_Y(&y);

            strainPlane->GetZ(x-dx,y-dy,&z);
            double stress;
            ssStrand->ComputeStress(z+e_initial,&stress);

            fps_avg += bond_factor*stress;

            point.Release();
         }
      }

      fps_avg /= (Ns+Nh);
   }
   else
   {
      // dimensions have no meaning if no moment capacity
      pmcd->c          = 0.0;
      pmcd->dc         = 0.0;
      pmcd->MomentArm  = 0.0;
      pmcd->de         = 0.0;
      
      fps_avg    = 0.0;
   }

   WATCHX(MomCap,0, "X = " << ::ConvertFromSysUnits(poi.GetDistFromStart(),unitMeasure::Feet) << " ft" << "   Mn = " << ::ConvertFromSysUnits(mn,unitMeasure::KipFeet) << " kip-ft" << " My/Mx = " << My/mn << " fps_avg = " << ::ConvertFromSysUnits(fps_avg,unitMeasure::KSI) << " KSI");

   pmcd->fps = fps_avg;
   pmcd->dt = dt;
   pmcd->bOverReinforced = false;

   GET_IFACE(ISpecification, pSpec);
   pmcd->Method = pSpec->GetMomentCapacityMethod();

   GET_IFACE(ILibrary,pLib);
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( pSpec->GetSpecification().c_str() );

   if ( pmcd->Method == LRFD_METHOD && pSpecEntry->GetSpecificationType() < lrfdVersionMgr::ThirdEditionWith2006Interims)
   {
      pmcd->bOverReinforced = (pmcd->c / pmcd->de > 0.42) ? true : false;
      if ( pmcd->bOverReinforced )
      {
         GET_IFACE(IBridgeMaterial,pMaterial);
         Float64 fc = pMaterial->GetFcSlab();

         Float64 Beta1 = lrfdConcreteUtil::Beta1(fc);
         Float64 de = pmcd->de;
         Float64 c  = pmcd->c;

         Float64 hf;
         Float64 b;
         Float64 bw;
         if ( bPositiveMoment )
         {
            hf = pBridge->GetStructuralSlabDepth(poi);

            GET_IFACE(ISectProp2,pProps);
            b = pProps->GetEffectiveFlangeWidth(poi);

            GET_IFACE(IGirder, pGdr);
            bw = pGdr->GetMinWebWidth(poi);
         }
         else
         {
            GET_IFACE(IGirder, pGdr);
            hf = pGdr->GetMinBottomFlangeThickness(poi);
            b  = pGdr->GetBottomWidth(poi);
            bw = pGdr->GetMinWebWidth(poi);
         }

         pmcd->FcSlab = fc;
         pmcd->b = b;
         pmcd->bw = bw;
         pmcd->hf = hf;
         pmcd->Beta1Slab = Beta1;

         if ( c <= hf )
         {
            pmcd->bRectSection = true;
            pmcd->MnMin = (0.36*Beta1 - 0.08*Beta1*Beta1)*fc*b*de*de;
         }
         else
         {
            // T-section behavior
            pmcd->bRectSection = false;
            pmcd->MnMin = (0.36*Beta1 - 0.08*Beta1*Beta1)*fc*bw*de*de 
                        + 0.85*Beta1*fc*(b - bw)*hf*(de - 0.5*hf);
         }

         if ( !bPositiveMoment )
            pmcd->MnMin *= -1;
      }
      else
      {
         // Dummy values
         pmcd->bRectSection = true;
         pmcd->MnMin = 0;
      }
   }
   else
   {
      // WSDOT method 2005... LRFD 2006 and later
      if ( IsZero(pmcd->c) ) 
         pmcd->Phi = 0.9; // there is no moment capacity, use 0.9 for phi instead of dividing by zero
      else
         pmcd->Phi = 0.65 + 0.15*(pmcd->dt/pmcd->c - 1) + pmcd->PPR*(0.1*(pmcd->dt/pmcd->c - 1) - 0.067);

      pmcd->Phi = ForceIntoRange(0.75,pmcd->Phi,0.9 + 0.1*pmcd->PPR);
   }

   pmcd->fpe = fpe;
   pmcd->e_initial = e_initial;

#if defined _DEBUG
   m_Log << "Dist from end " << poi.GetDistFromStart() << endl;
   m_Log << "-------------------------" << endl;
   m_Log << endl;
#endif
}

void pgsMomentCapacityEngineer::ComputeMinMomentCapacity(pgsTypes::Stage stage,const pgsPointOfInterest& poi,bool bPositiveMoment,MINMOMENTCAPDETAILS* pmmcd)
{
   GET_IFACE(IMomentCapacity,pMomentCapacity);

   MOMENTCAPACITYDETAILS mcd;
   pMomentCapacity->GetMomentCapacityDetails(stage,poi,bPositiveMoment,&mcd);

   CRACKINGMOMENTDETAILS cmd;
   pMomentCapacity->GetCrackingMomentDetails(stage,poi,bPositiveMoment,&cmd);

   ComputeMinMomentCapacity(stage,poi,bPositiveMoment,mcd,cmd,pmmcd);
}

void pgsMomentCapacityEngineer::ComputeMinMomentCapacity(pgsTypes::Stage stage,const pgsPointOfInterest& poi,const GDRCONFIG& config,bool bPositiveMoment,MINMOMENTCAPDETAILS* pmmcd)
{
   GET_IFACE(IMomentCapacity,pMomentCapacity);

   MOMENTCAPACITYDETAILS mcd;
   pMomentCapacity->GetMomentCapacityDetails(stage,poi,config,bPositiveMoment,&mcd);

   CRACKINGMOMENTDETAILS cmd;
   pMomentCapacity->GetCrackingMomentDetails(stage,poi,config,bPositiveMoment,&cmd);

   ComputeMinMomentCapacity(stage,poi,bPositiveMoment,mcd,cmd,pmmcd);
}

void pgsMomentCapacityEngineer::ComputeMinMomentCapacity(pgsTypes::Stage stage,const pgsPointOfInterest& poi,bool bPositiveMoment,const MOMENTCAPACITYDETAILS& mcd,const CRACKINGMOMENTDETAILS& cmd,MINMOMENTCAPDETAILS* pmmcd)
{
   Float64 Mr;     // Nominal resistance (phi*Mn)
   Float64 MrMin;  // Minimum nominal resistance - min(MrMin1,MrMin2)
   Float64 MrMin1; // 1.2Mcr
   Float64 MrMin2; // 1.33Mu
   Float64 Mu_StrengthI;
   Float64 Mu_StrengthII;
   Float64 Mu;     // Limit State Moment
   Float64 Mcr;    // Cracking moment

   GET_IFACE(ILiveLoads,pLiveLoads);
   bool bPermit = pLiveLoads->IsLiveLoadDefined(pgsTypes::lltPermit);

   GET_IFACE(ILimitStateForces,pLimitStateForces);

   GET_IFACE(ILibrary,pLib);
   GET_IFACE(ISpecification,pSpec);
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( pSpec->GetSpecification().c_str() );
   bool bAfter2002 = ( pSpecEntry->GetSpecificationType() >= lrfdVersionMgr::SecondEditionWith2003Interims ? true : false );

   pgsTypes::AnalysisType analysisType = pSpec->GetAnalysisType();
   BridgeAnalysisType bat;
   if (analysisType == pgsTypes::Simple)
   {
      bat = SimpleSpan;
   }
   else if (analysisType == pgsTypes::Continuous)
   {
      bat = ContinuousSpan;
   }
   else
   {
      // envelope
      if ( bPositiveMoment )
         bat = MaxSimpleContinuousEnvelope;
      else
         bat = MinSimpleContinuousEnvelope;
   }

   Mr = mcd.Phi * mcd.Mn;

   if ( bAfter2002 )
      Mcr = max(cmd.Mcr,cmd.McrLimit);
   else
      Mcr = cmd.Mcr;

   Float64 MuMin_StrengthI, MuMax_StrengthI;
   pLimitStateForces->GetMoment(pgsTypes::StrengthI,stage,poi,bat,&MuMin_StrengthI,&MuMax_StrengthI);
   Mu_StrengthI = (bPositiveMoment ? MuMax_StrengthI : MuMin_StrengthI);

   if ( bPermit )
   {
      Float64 MuMin_StrengthII, MuMax_StrengthII;
      pLimitStateForces->GetMoment(pgsTypes::StrengthII,stage,poi,bat,&MuMin_StrengthII,&MuMax_StrengthII);
      Mu_StrengthII = (bPositiveMoment ? MuMax_StrengthII : MuMin_StrengthII);
   }
   else
   {
      Mu_StrengthII = (bPositiveMoment ? DBL_MAX : -DBL_MAX);
   }

   pgsTypes::LimitState ls; // limit state of controlling Mu (least magnitude)
   if ( fabs(Mu_StrengthI) < fabs(Mu_StrengthII) )
   {
      Mu = Mu_StrengthI;
      ls = pgsTypes::StrengthI;
   }
   else
   {
      Mu = Mu_StrengthII;
      ls = pgsTypes::StrengthII;
   }

   MrMin1 = 1.20*Mcr;
   MrMin2 = 1.33*Mu;
   MrMin =  (bPositiveMoment ? min(MrMin1,MrMin2) : max(MrMin1,MrMin2));

   if (ls==pgsTypes::StrengthI)
   {
      pmmcd->LimitState = "Strength I";
   }
   else 
   {
      ATLASSERT(ls==pgsTypes::StrengthII);
      pmmcd->LimitState = "Strength II";
   }

   pmmcd->Mr     = Mr;
   pmmcd->MrMin  = MrMin;
   pmmcd->MrMin1 = MrMin1;
   pmmcd->MrMin2 = MrMin2;
   pmmcd->Mu     = Mu;
   pmmcd->Mcr    = Mcr;
}

void pgsMomentCapacityEngineer::ComputeCrackingMoment(pgsTypes::Stage stage,const pgsPointOfInterest& poi,bool bPositiveMoment,CRACKINGMOMENTDETAILS* pcmd)
{
   GET_IFACE(IPrestressForce,pPrestressForce);
   GET_IFACE(IStrandGeometry,pStrandGeom);
   GET_IFACE(IPrestressStresses,pPrestress);
   Float64 fcpe; // Stress at bottom of non-composite girder due to prestress

   if ( bPositiveMoment )
   {
      // Get stress at bottom of girder due to prestressing
      // Using negative because we want the amount tensile stress required to overcome the
      // precompression
      Float64 P = pPrestressForce->GetStrandForce(poi,pgsTypes::Permanent,pgsTypes::AfterLosses);
      Float64 ns_eff;
      Float64 e = pStrandGeom->GetEccentricity( poi, false, &ns_eff );

      fcpe = -pPrestress->GetStress(poi,pgsTypes::BottomGirder,P,e);
   }
   else
   {
      // no precompression in the slab
      fcpe = 0;
   }

   ComputeCrackingMoment(stage,poi,fcpe,bPositiveMoment,pcmd);
}

void pgsMomentCapacityEngineer::ComputeCrackingMoment(pgsTypes::Stage stage,const pgsPointOfInterest& poi,const GDRCONFIG& config,bool bPositiveMoment,CRACKINGMOMENTDETAILS* pcmd)
{
   GET_IFACE(IPrestressForce,pPrestressForce);
   GET_IFACE(IStrandGeometry,pStrandGeom);
   GET_IFACE(IPrestressStresses,pPrestress);
   Float64 fcpe; // Stress at bottom of non-composite girder due to prestress

   if ( bPositiveMoment )
   {
      // Get stress at bottom of girder due to prestressing
      // Using negative because we want the amount tensile stress required to overcome the
      // precompression
      Float64 P = pPrestressForce->GetStrandForce(poi,pgsTypes::Permanent,config,pgsTypes::AfterLosses);
      Float64 ns_eff;
      Float64 e = pStrandGeom->GetEccentricity( poi, config, false, &ns_eff);

      fcpe = -pPrestress->GetStress(poi,pgsTypes::BottomGirder,P,e);
   }
   else
   {
      // no precompression in the slab
      fcpe = 0;
   }

   ComputeCrackingMoment(stage,config,poi,fcpe,bPositiveMoment,pcmd);
}

void pgsMomentCapacityEngineer::ComputeCrackingMoment(pgsTypes::Stage stage,const GDRCONFIG& config,const pgsPointOfInterest& poi,Float64 fcpe,bool bPositiveMoment,CRACKINGMOMENTDETAILS* pcmd)
{
   Float64 Mdnc; // Dead load moment on non-composite girder
   Float64 fr;   // Rupture stress
   Float64 Sb;   // Bottom section modulus of non-composite girder
   Float64 Sbc;  // Bottom section modulus of composite girder

   // Get dead load moment on non-composite girder
   Mdnc = GetNonCompositeDeadLoadMoment(stage,poi,config,bPositiveMoment);
   fr = GetModulusOfRupture(config,bPositiveMoment);

   GetSectionProperties(stage,poi,config,bPositiveMoment,&Sb,&Sbc);

   ComputeCrackingMoment(fr,fcpe,Mdnc,Sb,Sbc,pcmd);
}

void pgsMomentCapacityEngineer::ComputeCrackingMoment(pgsTypes::Stage stage,const pgsPointOfInterest& poi,Float64 fcpe,bool bPositiveMoment,CRACKINGMOMENTDETAILS* pcmd)
{
   Float64 Mdnc; // Dead load moment on non-composite girder
   Float64 fr;   // Rupture stress
   Float64 Sb;   // Bottom section modulus of non-composite girder
   Float64 Sbc;  // Bottom section modulus of composite girder

   // Get dead load moment on non-composite girder
   Mdnc = GetNonCompositeDeadLoadMoment(stage,poi,bPositiveMoment);
   fr = GetModulusOfRupture(poi,bPositiveMoment);

   GetSectionProperties(stage,poi,bPositiveMoment,&Sb,&Sbc);

   ComputeCrackingMoment(fr,fcpe,Mdnc,Sb,Sbc,pcmd);
}

double pgsMomentCapacityEngineer::GetNonCompositeDeadLoadMoment(pgsTypes::Stage stage,const pgsPointOfInterest& poi,const GDRCONFIG& config,bool bPositiveMoment)
{
   GET_IFACE(IProductForces,pProductForces);
   double Mdnc = GetNonCompositeDeadLoadMoment(stage,poi,bPositiveMoment);
   // add effect of different slab offset
   double deltaSlab = pProductForces->GetDesignSlabPadMomentAdjustment(config.Fc,config.SlabOffset[pgsTypes::metStart],config.SlabOffset[pgsTypes::metEnd],poi);
   Mdnc += deltaSlab;

   return Mdnc;
}

double pgsMomentCapacityEngineer::GetNonCompositeDeadLoadMoment(pgsTypes::Stage stage,const pgsPointOfInterest& poi,bool bPositiveMoment)
{
   GET_IFACE(IProductLoads,pProductLoads);
   GET_IFACE(IProductForces,pProductForces);

   Float64 Mdnc = 0; // Dead load moment on non-composite girder

   SpanIndexType span  = poi.GetSpan();
   GirderIndexType gdr = poi.GetGirder();

   pgsTypes::Stage girderLoadStage = pProductLoads->GetGirderDeadLoadStage(span,gdr);

   if ( bPositiveMoment )
   {
      // Girder moment
      Mdnc += pProductForces->GetMoment(girderLoadStage,pftGirder,poi, SimpleSpan);

      // Slab moment
      Mdnc += pProductForces->GetMoment(pgsTypes::BridgeSite1,pftSlab,poi, SimpleSpan);

      // Diaphragm moment
      Mdnc += pProductForces->GetMoment(pgsTypes::BridgeSite1,pftDiaphragm,poi, SimpleSpan);

      // Shear Key moment
      Mdnc += pProductForces->GetMoment(pgsTypes::BridgeSite1,pftShearKey,poi, SimpleSpan);

      // User DC and User DW
      Mdnc += pProductForces->GetMoment(pgsTypes::BridgeSite1,pftUserDC,poi, SimpleSpan);
      Mdnc += pProductForces->GetMoment(pgsTypes::BridgeSite1,pftUserDW,poi, SimpleSpan);
   }

   return Mdnc;
}

double pgsMomentCapacityEngineer::GetModulusOfRupture(const pgsPointOfInterest& poi,bool bPositiveMoment)
{
   GET_IFACE(IProductForces,pProductForces);
   GET_IFACE(IBridgeMaterial,pMaterial);
   GET_IFACE(ISectProp2,pSectProp2);
   GET_IFACE(IStrandGeometry,pStrandGeom);

   Float64 fr;   // Rupture stress
   // Compute modulus of rupture
   if ( bPositiveMoment )
      fr = pMaterial->GetFlexureModRupture(pMaterial->GetFcGdr(poi.GetSpan(),poi.GetGirder()));
   else
      fr = pMaterial->GetFlexureModRupture(pMaterial->GetFcSlab());

   return fr;
}

double pgsMomentCapacityEngineer::GetModulusOfRupture(const GDRCONFIG& config,bool bPositiveMoment)
{
   GET_IFACE(IProductForces,pProductForces);
   GET_IFACE(IBridgeMaterial,pMaterial);
   GET_IFACE(ISectProp2,pSectProp2);
   GET_IFACE(IStrandGeometry,pStrandGeom);

   Float64 fr;   // Rupture stress
   // Compute modulus of rupture
   if ( bPositiveMoment )
      fr = pMaterial->GetFlexureModRupture(config.Fc);
   else
      fr = pMaterial->GetFlexureModRupture(pMaterial->GetFcSlab());

   return fr;
}

void pgsMomentCapacityEngineer::GetSectionProperties(pgsTypes::Stage stage,const pgsPointOfInterest& poi,bool bPositiveMoment,double* pSb,double* pSbc)
{
   GET_IFACE(ISectProp2,pSectProp2);

   Float64 Sb;   // Bottom section modulus of non-composite girder
   Float64 Sbc;  // Bottom section modulus of composite girder

   // Get the section moduli
   if ( bPositiveMoment )
   {
      Sb  = pSectProp2->GetSb(pgsTypes::BridgeSite1,poi);
      Sbc = pSectProp2->GetSb(stage,poi);
   }
   else
   {
      Sbc = pSectProp2->GetSt(stage,poi);
      Sb  = Sbc;
   }

   *pSb  = Sb;
   *pSbc = Sbc;
}

void pgsMomentCapacityEngineer::GetSectionProperties(pgsTypes::Stage stage,const pgsPointOfInterest& poi,const GDRCONFIG& config,bool bPositiveMoment,double* pSb,double* pSbc)
{
   GET_IFACE(ISectProp2,pSectProp2);

   Float64 Sb;   // Bottom section modulus of non-composite girder
   Float64 Sbc;  // Bottom section modulus of composite girder

   // Get the section moduli
   if ( bPositiveMoment )
   {
      Sb  = pSectProp2->GetSb(pgsTypes::BridgeSite1,poi,config.Fc);
      Sbc = pSectProp2->GetSb(stage,poi,config.Fc);
   }
   else
   {
      Sbc = pSectProp2->GetSt(stage,poi,config.Fc);
      Sb  = Sbc;
   }

   *pSb  = Sb;
   *pSbc = Sbc;
}

void pgsMomentCapacityEngineer::ComputeCrackingMoment(double fr,double fcpe,double Mdnc,double Sb,double Sbc,CRACKINGMOMENTDETAILS* pcmd)
{
   double Mcr = (fr + fcpe)*Sbc - Mdnc*(Sbc/Sb - 1);

   GET_IFACE(ILibrary,pLib);
   GET_IFACE(ISpecification,pSpec);
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( pSpec->GetSpecification().c_str() );
   bool bAfter2002 = ( pSpecEntry->GetSpecificationType() >= lrfdVersionMgr::SecondEditionWith2003Interims ? true : false );
   if ( bAfter2002 )
   {
      Float64 McrLimit = Sbc*fr;
      pcmd->McrLimit = McrLimit;
   }

   pcmd->Mcr  = Mcr;
   pcmd->Mdnc = Mdnc;
   pcmd->fr   = fr;
   pcmd->fcpe = fcpe;
   pcmd->Sb   = Sb;
   pcmd->Sbc  = Sbc;
}

//======================== ACCESS     =======================================
//======================== INQUIRY    =======================================

////////////////////////// PROTECTED  ///////////////////////////////////////

//======================== LIFECYCLE  =======================================
//======================== OPERATORS  =======================================
//======================== OPERATIONS =======================================
void pgsMomentCapacityEngineer::MakeCopy(const pgsMomentCapacityEngineer& rOther)
{
   // Add copy code here...
   m_pBroker = rOther.m_pBroker;
}

void pgsMomentCapacityEngineer::MakeAssignment(const pgsMomentCapacityEngineer& rOther)
{
   MakeCopy( rOther );
}

//======================== ACCESS     =======================================
//======================== INQUIRY    =======================================

////////////////////////// PRIVATE    ///////////////////////////////////////

//======================== LIFECYCLE  =======================================
//======================== OPERATORS  =======================================
//======================== OPERATIONS =======================================
void pgsMomentCapacityEngineer::CreateStrandMaterial(SpanIndexType span,GirderIndexType gdr,IStressStrain** ppSS)
{
   GET_IFACE(IBridgeMaterial,pMaterial);
   const matPsStrand* pStrand = pMaterial->GetStrand(span,gdr);

   StrandGradeType grade = pStrand->GetGrade() == matPsStrand::Gr1725 ? sgtGrade250 : sgtGrade270;
   ProductionMethodType type = pStrand->GetType() == matPsStrand::LowRelaxation ? pmtLowRelaxation : pmtStressRelieved;

   CComPtr<IPowerFormula> powerFormula;
   powerFormula.CoCreateInstance(CLSID_PSPowerFormula);
   powerFormula->put_Grade(grade);
   powerFormula->put_ProductionMethod(type);

   CComQIPtr<IStressStrain> ssStrand(powerFormula);
   (*ppSS) = ssStrand;
   (*ppSS)->AddRef();
}

void pgsMomentCapacityEngineer::BuildCapacityProblem(pgsTypes::Stage stage,const pgsPointOfInterest& poi,const GDRCONFIG& config,double e_initial,pgsBondTool& bondTool,bool bPositiveMoment,IGeneralSection** ppProblem,IPoint2d** pntCompression,ISize2d** szOffset,double* pdt,std::map<long,double>* pBondFactors)
{
   ATLASSERT( stage == pgsTypes::BridgeSite3 );

   GET_IFACE(IPrestressForce,pPrestressForce);

   GET_IFACE(IBridge,pBridge);
   GET_IFACE(IBridgeMaterial,pMaterial);
   GET_IFACE(ISectProp2, pSectProp);
   GET_IFACE(IStrandGeometry, pStrandGeom );
   GET_IFACE(ILongRebarGeometry, pRebarGeom);
   SpanIndexType span = poi.GetSpan();
   GirderIndexType gdr  = poi.GetGirder();
   double dist_from_start = poi.GetDistFromStart();

   double gdr_length = pBridge->GetGirderLength(span,gdr);

   double dt = 0; // depth from compression face to extreme layer of tensile reinforcement

   StrandIndexType Ns = config.Nstrands[pgsTypes::Straight];
   StrandIndexType Nh = config.Nstrands[pgsTypes::Harped];

   //
   // Create Materials
   //

   // strand
   CComPtr<IStressStrain> ssStrand;
   CreateStrandMaterial(span,gdr,&ssStrand);

   // girder concrete
   CComPtr<IUnconfinedConcrete> matGirder;
   matGirder.CoCreateInstance(CLSID_UnconfinedConcrete);
   matGirder->put_fc( config.Fc );
   CComQIPtr<IStressStrain> ssGirder(matGirder);

   // slab concrete
   CComPtr<IUnconfinedConcrete> matSlab;
   matSlab.CoCreateInstance(CLSID_UnconfinedConcrete);
   matSlab->put_fc( pMaterial->GetFcSlab() );
   CComQIPtr<IStressStrain> ssSlab(matSlab);

   // girder rebar
   CComPtr<IRebarModel> matGirderRebar;
   matGirderRebar.CoCreateInstance(CLSID_RebarModel);
   double E, Fy;
   pMaterial->GetLongitudinalRebarProperties(span,gdr,&E,&Fy);
   matGirderRebar->Init( Fy, E, 1.00 );
   CComQIPtr<IStressStrain> ssGirderRebar(matGirderRebar);

   // slab rebar
   CComPtr<IRebarModel> matSlabRebar;
   matSlabRebar.CoCreateInstance(CLSID_RebarModel);
   pMaterial->GetDeckRebarProperties(&E,&Fy);
   matSlabRebar->Init( Fy, E, 1.00 );
   CComQIPtr<IStressStrain> ssSlabRebar(matSlabRebar);

   //
   // Build the section
   //
   CComPtr<IGeneralSection> section;
   section.CoCreateInstance(CLSID_GeneralSection);

   
   // beam shape
   CComPtr<IShape> shapeBeam;
   pSectProp->GetGirderShape(poi,false,&shapeBeam);
   
   // the shape is positioned relative to the bridge cross section
   // move the beam such that it's bottom center is at (0,0)
   // this will give us a nice coordinate system to work with
   // NOTE: Later we will move the origin to the CG of the section
   CComQIPtr<IXYPosition> posBeam(shapeBeam);
   CComPtr<IPoint2d> origin;
   origin.CoCreateInstance(CLSID_Point2d);
   origin->Move(0,0);
   posBeam->put_LocatorPoint(lpBottomCenter,origin);

   // offset each shape so that the origin of the composite (if it is composite)
   // is located at the origin (this keeps the moment capacity solver happy)
   // Use the same offset to position the rebar
   CComPtr<IShapeProperties> props;
   shapeBeam->get_ShapeProperties(&props);
   CComPtr<IPoint2d> cgBeam;
   props->get_Centroid(&cgBeam);
   double dx,dy;
   cgBeam->get_X(&dx);
   cgBeam->get_Y(&dy);

   CComPtr<ISize2d> size;
   size.CoCreateInstance(CLSID_Size2d);
   size->put_Dx(dx);
   size->put_Dy(dy);
   *szOffset = size;
   (*szOffset)->AddRef();

   CComQIPtr<ICompositeShape> compBeam(shapeBeam);
   if ( compBeam )
   {
      // beam shape is composite

      CollectionIndexType shapeCount;
      compBeam->get_Count(&shapeCount);

      for ( CollectionIndexType idx = 0; idx < shapeCount; idx++ )
      {
         CComPtr<ICompositeShapeItem> csItem;
         compBeam->get_Item(idx,&csItem);

         CComPtr<IShape> shape;
         csItem->get_Shape(&shape);

         CComQIPtr<IXYPosition> position(shape);
         position->Offset(-dx,-dy);

         VARIANT_BOOL bVoid;
         csItem->get_Void(&bVoid);

         if ( bVoid == VARIANT_FALSE )
         {
            section->AddShape(shape,ssGirder,NULL,0.00);
         }
         else
         {
            // void shape... use only a background material (backgrounds are subtracted)
            section->AddShape(shape,NULL,ssGirder,0.00);
         }
      }
   }
   else
   {
      // beam shape isn't composite so just add it
      posBeam->Offset(-dx,-dy);
      section->AddShape(shapeBeam,ssGirder,NULL,0.00);
   }

   // so far there is no deck in the model.... if this is positive moment the compression point is top center, otherwise bottom center
   if ( bPositiveMoment )
      posBeam->get_LocatorPoint(lpTopCenter,pntCompression);
   else
      posBeam->get_LocatorPoint(lpBottomCenter,pntCompression);

   double Yc;
   (*pntCompression)->get_Y(&Yc);

   /////// -- NOTE -- //////
   // Development length, and hence the development length adjustment factor, require the result
   // of a moment capacity analysis. fps is needed to compute development length, yet, development
   // length is needed to adjust the effectiveness of the strands for moment capacity analysis.
   //
   // This causes a circular dependency. However, the development length calculation only needs
   // fps for the capacity at the mid-span section. Unless the bridge is extremely short, the
   // strands will be fully developed at mid-span.
   //
   // If the poi is around mid-span, assume a development length factor of 1.0 otherwise compute it.
   //
   double fra = 0.25; // 25% either side of centerline
   bool bNearMidSpan = false;
   if ( InRange(fra*gdr_length,dist_from_start,(1-fra)*gdr_length))
      bNearMidSpan = true;


   if ( bPositiveMoment ) // only model strands for positive moment
   {
      // strands
      const matPsStrand* pStrand = pMaterial->GetStrand(span,gdr);
      double aps = pStrand->GetNominalArea();
      double dps = pStrand->GetNominalDiameter();
      for ( int i = 0; i < 2; i++ ) // straight and harped strands
      {
         StrandIndexType nStrands = (i == 0 ? Ns : Nh);
         pgsTypes::StrandType strandType = (pgsTypes::StrandType)(i);
         CComPtr<IPoint2dCollection> points;
         pStrandGeom->GetStrandPositionsEx(poi, nStrands, strandType, &points);

/////////////////////////////////////////////
         // We know that we have symmetric section and that strands are generally in rows
         // create a single "lump of strand" for each row instead of modeling each strand 
         // individually. This will spead up the solver by quite a bit

         RowIndexType nStrandRows = pStrandGeom->GetNumRowsWithStrand(span,gdr,nStrands,strandType);
         for ( RowIndexType rowIdx = 0; rowIdx < nStrandRows; rowIdx++ )
         {
            double rowArea = 0;
            std::vector<StrandIndexType> strandIdxs = pStrandGeom->GetStrandsInRow(span,gdr,nStrands,rowIdx,strandType);

#if defined _DEBUG
            StrandIndexType nStrandsInRow = pStrandGeom->GetNumStrandInRow(span,gdr,nStrands,rowIdx,strandType);
            ATLASSERT( nStrandsInRow == strandIdxs.size() );
#endif

            ATLASSERT( 0 < strandIdxs.size() );
            std::vector<StrandIndexType>::iterator iter;
            for ( iter = strandIdxs.begin(); iter != strandIdxs.end(); iter++ )
            {
               StrandIndexType strandIdx = *iter;
               ATLASSERT( strandIdx < nStrands );

               bool bDebonded = bondTool.IsDebonded(strandIdx,strandType);
               if ( bDebonded )
               {
                  // strand is debonded... don't add it... go to the next strand
                  continue;
               }
   
               // get the bond factor (this will reduce the effective area of the strand if it isn't fully developed)
               double bond_factor = bondTool.GetBondFactor(strandIdx,strandType);

               // for negative moment, assume fully bonded
               if ( !bPositiveMoment )
                  bond_factor = 1.0;

               pBondFactors[i].insert( std::make_pair(strandIdx,bond_factor) );

               rowArea += bond_factor*aps;
            }

            // create a single equivalent rectangle for the area of reinforcement in this row
            double h = dps; // height is diamter of strand
            double w = rowArea/dps;

            CComPtr<IRectangle> bar_shape;
            bar_shape.CoCreateInstance(CLSID_Rect);
            bar_shape->put_Width(w);
            bar_shape->put_Height(h);

            // get one strand from the row and get it's Y value
            CComPtr<IPoint2d> point;
            points->get_Item(strandIdxs[0],&point);
            double rowY;
            point->get_Y(&rowY);
            point.Release();

            // position the "strand" rectangle
            CComQIPtr<IXYPosition> position(bar_shape);
            CComPtr<IPoint2d> hp;
            position->get_LocatorPoint(lpHookPoint,&hp);
            hp->Move(0,rowY);
            hp->Offset(-dx,-dy);

            // determine depth to lowest layer of strand
            double cy;
            hp->get_Y(&cy);
            dt = _cpp_max(dt,fabs(Yc-cy));

            CComQIPtr<IShape> shape(bar_shape);
            section->AddShape(shape,ssStrand,ssGirder,e_initial);


#if defined _DEBUG
            CComPtr<IShapeProperties> props;
            shape->get_ShapeProperties(&props);
            double area;
            props->get_Area(&area);
            ATLASSERT( IsEqual(area,rowArea) );
#endif // _DEBUG
         }

/////////////////////////////////////////////
         // This commentent out code block creates individual strands in the capacity problems.
         // This type of model will run much slower.
/*
         long strandIdx = 0;
         long strandPos = 0;
         CComPtr<IEnumPoint2d> enum_points;
         points->get__Enum(&enum_points);
         CComPtr<IPoint2d> point;
         while ( enum_points->Next(1,&point,NULL) != S_FALSE )
         {
            bool bDebonded = bondTool.IsDebonded(strandIdx,strandType);
            if ( bDebonded )
            {
               // strand is debonded... don't add it... go to the next strand
               point.Release();
               strandIdx++;
               continue;
            }

            // get the bond factor (this will reduce the effective area of the strand if it isn't fully developed)
            double bond_factor = bondTool.GetBondFactor(strandIdx,strandType);

            // for negative moment, assume fully bonded
            if ( !bPositiveMoment )
               bond_factor = 1.0;

            pBondFactors[i].insert( std::make_pair(strandPos++,bond_factor) );

//            // create an "area perfect" circle
//            double r = sqrt(aps/M_PI);
//            CComPtr<ICircle> bar_shape;
//            bar_shape.CoCreateInstance(CLSID_Circle);
//            bar_shape->put_Radius(r);
         

            // create an "area perfect" square
            // (clips are lot faster than a circle)
            double s = sqrt(bond_factor*aps);

            CComPtr<IRectangle> bar_shape;
            bar_shape.CoCreateInstance(CLSID_Rect);
            bar_shape->put_Width(s);
            bar_shape->put_Height(s);

            CComQIPtr<IXYPosition> position(bar_shape);
            CComPtr<IPoint2d> hp;
            position->get_LocatorPoint(lpHookPoint,&hp);
            hp->MoveEx(point);
            hp->Offset(-dx,-dy);

            double cy;
            hp->get_Y(&cy);
            dt = _cpp_max(dt,fabs(Yc-cy));

            CComQIPtr<IShape> shape(bar_shape);
            section->AddShape(shape,ssStrand,ssGirder,e_initial);


#if defined _DEBUG
            CComPtr<IShapeProperties> props;
            shape->get_ShapeProperties(&props);
            double area;
            props->get_Area(&area);
            ATLASSERT( IsEqual(area,aps*bond_factor) );
#endif // _DEBUG

            strandIdx++;
            point.Release();
         }
*/
/////////////////////////////////////////////
      } // next strand type
   }

   // girder rebar
   GET_IFACE(ILibrary,pLib);
   GET_IFACE(ISpecification, pSpec);
   const SpecLibraryEntry* pSpecEntry = pLib->GetSpecEntry( pSpec->GetSpecification().c_str() );
   if ( pSpecEntry->IncludeRebarForMoment() )
   {
      CComPtr<IRebarSection> rebar_section;
      pRebarGeom->GetRebars(poi,&rebar_section);
      
      CComPtr<IEnumRebarSectionItem> enumItems;
      rebar_section->get__EnumRebarSectionItem(&enumItems);

      CComPtr<IRebarSectionItem> item;
      while ( enumItems->Next(1,&item,NULL) != S_FALSE )
      {
         CComPtr<IPoint2d> location;
         item->get_Location(&location);

         double x,y;
         location->get_X(&x);
         location->get_Y(&y);

         CComPtr<IRebar> rebar;
         item->get_Rebar(&rebar);
         Float64 as;
         rebar->get_NominalArea(&as);

         double dev_length_factor = pRebarGeom->GetDevLengthFactor(span,gdr,item);

         // create an "area perfect" square
         // (clips are lot faster than a circle)
         double s = sqrt(dev_length_factor*as);

         CComPtr<IRectangle> square;
         square.CoCreateInstance(CLSID_Rect);
         square->put_Width(s);
         square->put_Height(s);
         
         CComPtr<IPoint2d> hp;
         square->get_HookPoint(&hp);
         hp->MoveEx(location);
         hp->Offset(-dx,-dy);

         double cy;
         hp->get_Y(&cy);
         dt = _cpp_max(dt,fabs(Yc-cy));

         CComQIPtr<IShape> shape(square);
         section->AddShape(shape,ssGirderRebar,ssGirder,0);

         item.Release();
      }
   }

   if ( (stage == pgsTypes::BridgeSite2 || stage == pgsTypes::BridgeSite3) &&
         pBridge->GetDeckType() != pgsTypes::sdtNone && pBridge->IsCompositeDeck() )
   {
      // deck
      double Weff = pSectProp->GetEffectiveFlangeWidth(poi);
      double Dslab = pBridge->GetStructuralSlabDepth(poi);

      // so far, dt is measured from top of girder (if positive moment)
      // since we have a deck, add Dslab so that dt is measured from top of slab
      if ( bPositiveMoment )
         dt += Dslab;

      CComPtr<IRectangle> rect;
      rect.CoCreateInstance(CLSID_Rect);
      rect->put_Height(Dslab);
      rect->put_Width(Weff);

      // put the bottom center of the deck rectangle right on the top center of the beam
      CComQIPtr<IXYPosition> posDeck(rect);

      CComPtr<IPoint2d> pntCommon;
      posBeam->get_LocatorPoint(lpTopCenter,&pntCommon);
      posDeck->put_LocatorPoint(lpBottomCenter,pntCommon);

      // if this is positive moment and we have a deck, the extreme compression point is top center
      if (bPositiveMoment)
         posDeck->get_LocatorPoint(lpTopCenter,pntCompression);

      CComQIPtr<IShape> shapeDeck(posDeck);

      section->AddShape(shapeDeck,ssSlab,NULL,0.00);


      // deck rebar if this is for negative moment
      if ( !bPositiveMoment )
      {
         double AsTop = pRebarGeom->GetAsTopMat(poi,ILongRebarGeometry::All);

         if ( !IsZero(AsTop) )
         {
            double coverTop = pRebarGeom->GetCoverTopMat();
            double equiv_height = AsTop / Weff; // model deck rebar as rectangles of equivalent area
            double equiv_width = Weff;
            if ( equiv_height < Dslab/16. )
            {
               // of the equivalent height is too sort, it doesn't model well
               equiv_height = Dslab/16.;
               equiv_width = AsTop/equiv_height;
            }
            CComPtr<IRectangle> topRect;
            topRect.CoCreateInstance(CLSID_Rect);
            topRect->put_Height(equiv_height);
            topRect->put_Width(equiv_width);

            // move the center of the rebar rectangle below the top of the deck rectangle by the cover amount.
            // center it horizontally
            CComQIPtr<IXYPosition> posTop(topRect);
            pntCommon.Release();
            posDeck->get_LocatorPoint(lpTopCenter,&pntCommon);
            pntCommon->Offset(0,-coverTop);
            posTop->put_LocatorPoint(lpCenterCenter,pntCommon);

            double cy;
            pntCommon->get_Y(&cy);
            dt = _cpp_max(dt,fabs(Yc-cy));

            CComQIPtr<IShape> shapeTop(posTop);
            section->AddShape(shapeTop,ssSlabRebar,ssSlab,0.00);
         }


         double AsBottom = pRebarGeom->GetAsBottomMat(poi,ILongRebarGeometry::All);
         if ( !IsZero(AsBottom) )
         {
            double coverBottom = pRebarGeom->GetCoverBottomMat();
            double equiv_height = AsBottom / Weff;
            double equiv_width = Weff;
            if ( equiv_height < Dslab/16. )
            {
               // of the equivalent height is too sort, it doesn't model well
               equiv_height = Dslab/16.;
               equiv_width = AsBottom/equiv_height;
            }
            CComPtr<IRectangle> botRect;
            botRect.CoCreateInstance(CLSID_Rect);
            botRect->put_Height(equiv_height);
            botRect->put_Width(equiv_width);

            // move the center of the rebar rectangle above the bottom of the deck rectangle by the cover amount.
            // center it horizontally
            CComQIPtr<IXYPosition> posBottom(botRect);
            pntCommon.Release();
            posDeck->get_LocatorPoint(lpBottomCenter,&pntCommon);
            pntCommon->Offset(0,coverBottom);
            posBottom->put_LocatorPoint(lpCenterCenter,pntCommon);

            double cy;
            pntCommon->get_Y(&cy);
            dt = _cpp_max(dt,fabs(Yc-cy));

            CComQIPtr<IShape> shapeBottom(posBottom);
            section->AddShape(shapeBottom,ssSlabRebar,ssSlab,0.00);
         }
      }
   }

   *pdt = dt;

   (*ppProblem) = section;
   (*ppProblem)->AddRef();
}

//======================== ACCESS     =======================================
//======================== INQUERY    =======================================

//======================== DEBUG      =======================================
#if defined _DEBUG
bool pgsMomentCapacityEngineer::AssertValid() const
{
   return true;
}

void pgsMomentCapacityEngineer::Dump(dbgDumpContext& os) const
{
   os << "Dump for pgsMomentCapacityEngineer" << endl;
}
#endif // _DEBUG

#if defined _UNITTEST
bool pgsMomentCapacityEngineer::TestMe(dbgLog& rlog)
{
   TESTME_PROLOGUE("pgsMomentCapacityEngineer");

   TEST_NOT_IMPLEMENTED("Unit Tests Not Implemented for pgsMomentCapacityEngineer");

   TESTME_EPILOG("MomentCapacityEngineer");
}
#endif // _UNITTEST

#if defined _DEBUG_SECTION_DUMP
void pgsMomentCapacityEngineer::DumpSection(const pgsPointOfInterest& poi,IGeneralSection* section, std::map<long,double> ssBondFactors,std::map<long,double> hsBondFactors,bool bPositiveMoment)
{
   std::ostringstream os;
   std::string strMn(bPositiveMoment ? "+M" : "-M"); 
   os << "GeneralSection_" << strMn << "_Span_" << LABEL_SPAN(poi.GetSpan()) << "_Girder_" << LABEL_GIRDER(poi.GetGirder()) << "_" << ::ConvertFromSysUnits(poi.GetDistFromStart(),unitMeasure::Feet) << ".txt";
   std::ofstream file(os.str().c_str());

   long shape_count;
   section->get_ShapeCount(&shape_count);
   for ( long idx = 0; idx < shape_count; idx++ )
   {
      file << (idx+1) << std::endl;

      CComPtr<IShape> shape;
      section->get_Shape(idx,&shape);

      CComPtr<IPoint2dCollection> points;
      shape->get_PolyPoints(&points);

      CComPtr<IPoint2d> point;
      CComPtr<IEnumPoint2d> enum_points;
      points->get__Enum(&enum_points);
      while ( enum_points->Next(1,&point,0) == S_OK )
      {
         double x,y;
         point->get_X(&x);
         point->get_Y(&y);

         file << ::ConvertFromSysUnits(x,unitMeasure::Inch) << "," << ::ConvertFromSysUnits(y,unitMeasure::Inch) << std::endl;

         point.Release();
      }
   }

   file << "done" << std::endl;

   file << "Straight Strand Bond Factors" << std::endl;
   std::map<long,double>::iterator iter;
   for ( iter = ssBondFactors.begin(); iter != ssBondFactors.end(); iter++ )
   {
      file << iter->first << ", " << iter->second << std::endl;
   }

   file << "Harped Strand Bond Factors" << std::endl;
   for ( iter = hsBondFactors.begin(); iter != hsBondFactors.end(); iter++ )
   {
      file << iter->first << ", " << iter->second << std::endl;
   }

   file.close();
}
#endif // _DEBUG_SECTION_DUMP

pgsMomentCapacityEngineer::pgsBondTool::pgsBondTool(IBroker* pBroker,const pgsPointOfInterest& poi,const GDRCONFIG& config)
{
   m_pBroker    = pBroker;
   m_Poi        = poi;
   m_Config     = config;
   m_bUseConfig = true;
   Init();
}

pgsMomentCapacityEngineer::pgsBondTool::pgsBondTool(IBroker* pBroker,const pgsPointOfInterest& poi)
{
   m_pBroker    = pBroker;
   m_Poi        = poi;

   GET_IFACE(IBridge,pBridge);
   m_CurrentConfig = pBridge->GetGirderConfiguration(m_Poi.GetSpan(),m_Poi.GetGirder());
   m_bUseConfig = false;
   Init();
}

void pgsMomentCapacityEngineer::pgsBondTool::Init()
{
   GET_IFACE(IPrestressForce,pPrestressForce);
   m_pPrestressForce = pPrestressForce;

   GET_IFACE(IBridge,pBridge);
   m_GirderLength = pBridge->GetGirderLength(m_Poi.GetSpan(),m_Poi.GetGirder());

   m_DistFromStart = m_Poi.GetDistFromStart();

   GET_IFACE(IPointOfInterest,pPOI);
   std::vector<pgsPointOfInterest> vPOI = pPOI->GetPointsOfInterest(pgsTypes::BridgeSite3,m_Poi.GetSpan(),m_Poi.GetGirder(),POI_MIDSPAN);
   ASSERT( vPOI.size() == 1 );
   m_PoiMidSpan = vPOI[0];

   /////// -- NOTE -- //////
   // Development length, and hence the development length adjustment factor, require the result
   // of a moment capacity analysis. fps is needed to compute development length, yet, development
   // length is needed to adjust the effectiveness of the strands for moment capacity analysis.
   //
   // This causes a circular dependency. However, the development length calculation only needs
   // fps for the capacity at the mid-span section. Unless the bridge is extremely short, the
   // strands will be fully developed at mid-span.
   //
   // If the poi is around mid-span, assume a development length factor of 1.0 otherwise compute it.
   //
   double fra = 0.25; // 25% either side of centerline
   m_bNearMidSpan = false;
   if ( InRange(fra*m_GirderLength,m_DistFromStart,(1-fra)*m_GirderLength))
      m_bNearMidSpan = true;
}

Float64 pgsMomentCapacityEngineer::pgsBondTool::GetBondFactor(StrandIndexType strandIdx,pgsTypes::StrandType strandType)
{
   // NOTE: More tricky code here (see note above)
   //
   // If we have a section that isn't near mid-span, we have to compute a bond factor. The bond factor is
   // a function of development length and development length needs fps and fpe which are computed in the
   // moment capacity analysis... again, circular dependency here.
   //
   // To work around this, the bond factors for moment capacity analysis are computed based on fps and fpe
   // at mid span.
   double bond_factor = 1;
   if ( !m_bNearMidSpan )
   {
      if ( m_bUseConfig )
      {
         GET_IFACE(IStrandGeometry,pStrandGeom);
         Float64 bond_start, bond_end;
         bool bDebonded = pStrandGeom->IsStrandDebonded(m_Poi.GetSpan(),m_Poi.GetGirder(),strandIdx,strandType,m_Config,&bond_start,&bond_end);
         STRANDDEVLENGTHDETAILS dev_length = m_pPrestressForce->GetDevLengthDetails(m_PoiMidSpan,bDebonded);

         bond_factor = m_pPrestressForce->GetStrandBondFactor(m_Poi,m_Config,strandIdx,strandType,dev_length.fps,dev_length.fpe);
      }
      else
      {
         GET_IFACE(IStrandGeometry,pStrandGeom);
         Float64 bond_start, bond_end;
         bool bDebonded = pStrandGeom->IsStrandDebonded(m_Poi.GetSpan(),m_Poi.GetGirder(),strandIdx,strandType,&bond_start,&bond_end);
         STRANDDEVLENGTHDETAILS dev_length = m_pPrestressForce->GetDevLengthDetails(m_PoiMidSpan,bDebonded);

         bond_factor = m_pPrestressForce->GetStrandBondFactor(m_Poi,strandIdx,strandType,dev_length.fps,dev_length.fpe);
      }
   }

   return bond_factor;
}

bool pgsMomentCapacityEngineer::pgsBondTool::IsDebonded(StrandIndexType strandIdx,pgsTypes::StrandType strandType)
{
   bool bDebonded = false;

   GDRCONFIG& config = (m_bUseConfig ? m_Config : m_CurrentConfig);

   std::vector<DEBONDINFO>::const_iterator iter;
   for ( iter = config.Debond[strandType].begin(); iter != config.Debond[strandType].end(); iter++ )
   {
      const DEBONDINFO& di = *iter;

      if ( di.strandIdx == strandIdx &&
          ((m_DistFromStart < di.LeftDebondLength) || ((m_GirderLength - di.RightDebondLength) < m_DistFromStart)) )
      {
         // this strand is debonded at this POI... next strand
         bDebonded = true;
         break;
      }
   }

   return bDebonded;
}