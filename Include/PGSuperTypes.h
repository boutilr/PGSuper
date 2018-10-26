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

#ifndef INCLUDED_PGSUPERTYPES_H_
#define INCLUDED_PGSUPERTYPES_H_
#pragma once

#include <WBFLTypes.h>
#include <EAF\EAFTypes.h>
#include <MathEx.h>
#include <vector>

static long g_Ncopies = 0;

class dbgDumpContext;
#include <Material\Rebar.h>


#define TOOLTIP_WIDTH 300
#define TOOLTIP_DURATION 10000

// defines the method used for prestress loss computations
#define LOSSES_AASHTO_REFINED       0
#define LOSSES_AASHTO_LUMPSUM       1
#define LOSSES_GENERAL_LUMPSUM      3
#define LOSSES_WSDOT_LUMPSUM        4 // same as PPR = 1.0 in aashto eqn's
#define LOSSES_AASHTO_LUMPSUM_2005  5 // 2005 AASHTO code
#define LOSSES_AASHTO_REFINED_2005  6 // 2005 AASHTO code
#define LOSSES_WSDOT_LUMPSUM_2005   7 // 2005 AASHTO, WSDOT (includes initial relaxation loss)
#define LOSSES_WSDOT_REFINED_2005   8 // 2005 AASHTO, WSDOT (includes initial relaxation loss)
#define LOSSES_WSDOT_REFINED        9
#define LOSSES_TXDOT_REFINED_2004   10 // TxDOT's May, 09 decision is to use refined losses from AASHTO 2004

// defines the mehod used for computing creep coefficients
#define CREEP_LRFD              0
#define CREEP_WSDOT             1

#define CREEP_SPEC_PRE_2005     1 // creep based on pre 2005 provisions
#define CREEP_SPEC_2005         2 // creep based on 2005 interim provisions

// curing method (effects ti for loss and creep calcuations)
#define CURING_NORMAL           0
#define CURING_ACCELERATED      1

// defines the creep time frame (ie. D40 and D120 for WSDOT)
#define CREEP_MINTIME           0
#define CREEP_MAXTIME           1

// methods for distribution factor calculation
#define LLDF_LRFD               0
#define LLDF_WSDOT              1
// #define LLDF_DIRECT_INPUT       2 // Value is stored in project, not library.
#define LLDF_TXDOT              2 

// analysis or spec check method type
#define LRFD_METHOD               0
#define WSDOT_METHOD              1 // wsdot-modified range for check

// maximum girder spacing - used to indicate there is not an upper limit on girder spacing
// can't use DBL_MAX because in many places girder spacing is multiplied by a skew correction factor
// this can cause the floating point number to overflow
#define MAX_GIRDER_SPACING        DBL_MAX/3

// this can be any number of girders, however we'll set the limit to something practical
// 26*2 = 52 = 2 times through the alphabet... Girder A - Girder AZ
#define MAX_GIRDERS_PER_SPAN              52

// need to have a reasonable upper limit of skew angle... use 88 degrees
#define MAX_SKEW_ANGLE ::ToRadians(88.0)

struct pgsTypes
{
      enum Stage { CastingYard = 0, 
                   Lifting = 5, 
                   Hauling = 4, 
                   GirderPlacement = 7,
                   TemporaryStrandRemoval = 6,
                   BridgeSite1 = 1, // slab casting
                   BridgeSite2 = 2, // superimposed dead loads
                   BridgeSite3 = 3  // final with live load
                 };

      static const Uint32 MaxStages = GirderPlacement+1; 
   // NOTES:
   //       1) If you add new stages, be sure to update MaxStages
   //       2) Added Lifting and Hauling as stages because they make sense in some contexts. Constants have been
   //          defined so as not to mess up code that currently uses this enum in loops or array indices

   enum StressType { Tension, Compression };
   
   enum LimitState { ServiceI   = 0,   // Full dead load and live load
                     ServiceIA  = 1,   // Half dead load and live load (Fatigue) - not used after LRFD 2008
                     ServiceIII = 2, 
                     StrengthI  = 3,
                     StrengthII = 4,
                     FatigueI   = 5,   // added in LRFD 2009 (replaces Service IA)
                     StrengthI_Inventory = 6,
                     StrengthI_Operating = 7,
                     ServiceIII_Inventory = 8,
                     ServiceIII_Operating = 9,
                     StrengthI_LegalRoutine = 10,
                     StrengthI_LegalSpecial = 11,
                     ServiceIII_LegalRoutine = 12,
                     ServiceIII_LegalSpecial = 13,
                     StrengthII_PermitRoutine = 14,
                     ServiceI_PermitRoutine = 15,
                     StrengthII_PermitSpecial = 16,
                     ServiceI_PermitSpecial = 17
                   }; 

   enum StressLocation { BottomGirder, TopGirder, TopSlab };
   // Note that Permanent was added below when input for total permanent strands was added in 12/06
   enum StrandType { Straight, Harped, Temporary, Permanent };
   enum LossStage { Jacking, BeforeXfer, AfterXfer, AtLifting, AtShipping, BeforeTemporaryStrandRemoval, AfterTemporaryStrandRemoval, AfterDeckPlacement, AfterSIDL, AfterLosses, AfterTemporaryStrandInstallation };
   enum AnalysisType { Simple, Continuous, Envelope };

   // temporary top strand usage
   enum TTSUsage
   {
      ttsPretensioned     = 0,
      ttsPTBeforeShipping = 1,
      ttsPTAfterLifting   = 2,
      ttsPTBeforeLifting  = 3,
   };


   enum LiveLoadType
   {
      lltDesign     = 0,   // for design limit states
      lltPermit     = 1,   // for permit limit state during design (Strength II)
      lltFatigue    = 2,   // for fatigue limit states
      lltPedestrian = 3,   // for pedestrian loads to be combined in any limit state
      lltLegalRating_Routine = 4,  // for legal load ratings for routine commercial traffic
      lltLegalRating_Special = 5,  // for legal load ratings for specialized hauling vehicles
      lltPermitRating_Routine = 6,  // for routine permit load ratings
      lltPermitRating_Special = 7   // for special permit load ratings
   };

   enum DebondLengthControl   // which criteria controlled for max debond length
   {mdbDefault, mbdFractional, mdbHardLength};


   enum EffectiveFlangeWidthMethod
   {
      efwmLRFD,       // effective flange width is based on LRFD
      efwmTribWidth   // effective flange width is always the tributary width
   };

   enum MovePierOption
   {
      MoveBridge,
      AdjustPrevSpan,
      AdjustNextSpan,
      AdjustAdjacentSpans
   };

   typedef enum MeasurementType
   {
      NormalToItem    = 0, // measrued normal to the item (alignment, cl pier, cl bearing, cl girder, etc)
      AlongItem       = 1, // measured along centerline of item (pier, abutment, bearing, girder, etc)
   } MeasurementType;

   typedef enum MeasurementLocation
   {
      AtCenterlinePier    = 0, // measrued at centerline pier/abutment
      AtCenterlineBearing = 1, // measured at centerline bearing
   } MeasurementLocation;

   typedef enum PierFaceType
   {
      Back  = 0,
      Ahead = 1
   } PierFaceType;

   typedef enum GirderOrientationType
   {
      Plumb = 0,     // Girder is plumb
      StartNormal,   // Girder is normal to deck at start of span
      MidspanNormal, // Girder is normal to deck at midspan
      EndNormal      // Girder is normal to deck at end of span
   } GirderOrientationType;

   // member can be girder, span, column, etc
   typedef enum MemberEndType
   {
      metStart = 0,  // start of member
      metEnd   = 1   // end of member
   } MemberEndType;

   typedef enum DeckOverhangTaper
   {
      None            = 0,  // No taper, constant thickness deck in overhanges
      TopTopFlange    = 1,  // Taper overhang to top of girder top flange
      BottomTopFlange = 2   // Taper overhang to bottom of girder top flange
   } DeckOverhangTaper;

   typedef enum OffsetMeasurementType
   {
      omtAlignment,  // offset measured from alignment
      omtBridge      // offset measured from CL bridge
   } OffsetMeasurementType;

   typedef enum DeckPointTransitionType
   {
      dptLinear,  // deck edge is linear between deck points
      dptSpline,  // deck edge is a cubic spline between points
      dptParallel // deck edge parallels alignment. deck points on both sides of transtion must be the same
   } DeckPointTransitionType;

   enum WearingSurfaceType
   {
      wstSacrificialDepth  = 0,
      wstOverlay           = 1,
      wstFutureOverlay     = 2
   };

   typedef enum SupportedDeckType 
   {
      // NOTE: Assigning explicit values to guarentee consistency with deck type constants
      //       used prior to adding non-composite sections to PGSuper
      sdtCompositeCIP = 0,       // Composite cast-in-place deck
      sdtCompositeSIP = 1,       // Composite stay-in-place deck panels
      sdtCompositeOverlay,       // Composite structural overlay (for adjacent beams)
      sdtNone                    // No deck is used... but an asphalt wearing surface can be
   } SupportedDeckType;


   typedef enum SupportedBeamSpacing
   {
      sbsUniform,                // Spacing is a constant value everywhere in the bridge
      sbsGeneral,                // Spacing is defined pier by pier (or span by span). Spacing between each girder can be different
      sbsUniformAdjacent,        // Beams are adjacent and joint width is the same in all spans.
      sbsGeneralAdjacent,        // Beams are adjacent and joint width can vary by span
      sbsConstantAdjacent        // Beams are adjacent and vary in width based on girder spacing. The same girder spacing is used in all spans
   } SupportedBeamSpacing;


   typedef enum SlabOffsetType
   {
      sotBridge,  // a single slab offset is used for the entire bridge
      sotSpan,    // the slab offset is defined at both ends of a span (or both faces of a pier)
      sotGirder,  // the slab offset is defined at both ends of each girder (can vary from girder to girder within a span)
   } SlabOffsetType;

   // Define connectivity (per AASHTO jargon) of adjacent beams.
   // This is only used if SupportedBeamSpacing==sbsUniformAdjacent or sbsGeneralAdjacent
   typedef enum AdjacentTransverseConnectivity
   {
      atcConnectedAsUnit,
      atcConnectedRelativeDisplacement
   } AdjacentTransverseConnectivity;

   typedef std::vector<SupportedDeckType>    SupportedDeckTypes;
   typedef std::vector<SupportedBeamSpacing> SupportedBeamSpacings;

   typedef enum RemovePierType
   {
      PrevPier,
      NextPier
   } RemovePierType;

   enum DistributionFactorMethod
   { Calculated = 0, DirectlyInput = 1, LeverRule=2 };

   enum OverlayLoadDistributionType
   { olDistributeEvenly=0, olDistributeTributaryWidth=1 };

   enum GirderLocation
   { Interior = 0, Exterior = 1 };

   enum GirderFace 
   {GirderTop, GirderBottom};

   // NOTE: Enum values are out of order so that they match values used in earlier
   // versions of the software
   enum PierConnectionType { Hinged = 1, 
                             Roller = 6,
                             ContinuousAfterDeck = 2,        // continuous are for interior piers only
                             ContinuousBeforeDeck = 3, 
                             IntegralAfterDeck = 4,          // this two are fixed both sides (unless abutments. obviously)
                             IntegralBeforeDeck = 5,
                             IntegralAfterDeckHingeBack  = 7, // interior piers only, left or right hinge
                             IntegralBeforeDeckHingeBack = 8,
                             IntegralAfterDeckHingeAhead = 9,
                             IntegralBeforeDeckHingeAhead = 10};

   // Method for computing prestress transfer length
   enum PrestressTransferComputationType { ptUsingSpecification=60, // use current spec
                                           ptMinuteValue=0 };       // As close to zero length as we are comfortable

   enum TrafficBarrierOrientation
   {
      tboLeft,
      tboRight
   };

   // Defines how the traffic barrier/railing system loading is to be distributed
   enum TrafficBarrierDistribution
   {
      tbdGirder,
      tbdMatingSurface,
      tbdWebs
   };

   enum SplittingDirection
   {
      sdVertical,
      sdHorizontal
   };

   // enum to represent condition factors (MBE 6A.4.2.3)
   enum ConditionFactorType
   {
      cfGood,
      cfFair,
      cfPoor,
      cfOther
   };

   enum LoadRatingType
   {
      lrDesign_Inventory,  // design rating at the inventory level
      lrDesign_Operating,  // design rating at the operating level
      lrLegal_Routine,     // legal rating for routine commercial traffic
      lrLegal_Special,     // legal rating for specialized hauling vehicles
      lrPermit_Routine,    // routine permit ratings
      lrPermit_Special     // special permit ratings
   };

   enum SpecialPermitType
   {
      ptSingleTripWithEscort,    // special or limited crossing permit
      ptSingleTripWithTraffic,   // special or limited crossing permit
      ptMultipleTripWithTraffic,  // special or limited crossing permit
   };

   // describes the model used for determining live load factors for rating
   enum LiveLoadFactorType
   {
      gllSingleValue,  // a single constanst value is used
      gllStepped,      // ADTT < a1 gll = g1, otherwise gll = g2
      gllLinear,       // ADTT < a1 gll = g1, ADTT > a2 gll = g2
      gllBilinear,     // ADTT < a1 gll = g1, ADTT = a2, gll = g2, ADTT > a3, gll = g3
      gllBilinearWithWeight // same as glBilinear with second set of values base on vehicle weight
   };

   enum LiveLoadFactorModifier
   {
      gllmInterpolate, // linear interpolate between ADTT values
      gllmRoundUp      // round up the ADTT value to match a control value
   };

   typedef enum ConcreteType
   {
      Normal,
      AllLightweight,
      SandLightweight
   } ConcreteType;
};


//-----------------------------------------------------------------------------
// Struct for stirrup information.
struct STIRRUPCONFIG
{
   // First, some needed types
   // =========================
   struct SHEARZONEDATA // Primary shear zone
   {
      Float64 ZoneLength;
      Float64 BarSpacing;
      matRebar::Size VertBarSize;
      Float64 nVertBars;
      Float64 nHorzInterfaceBars;
      matRebar::Size ConfinementBarSize;

      // pre-computed values
      Float64 VertABar; // Area of single bar

      // This struct is complex enough to need a good constructor
      SHEARZONEDATA():
      ZoneLength(0.0), VertBarSize(matRebar::bsNone), BarSpacing(0.0), nVertBars(0.0), 
      nHorzInterfaceBars(0.0), ConfinementBarSize(matRebar::bsNone), VertABar(0.0)
      {;}

      bool operator==(const SHEARZONEDATA& other) const
      {
         if( !IsEqual(ZoneLength , other.ZoneLength) ) return false;
         if(VertBarSize != other.VertBarSize) return false;
         if( !IsEqual(BarSpacing , other.BarSpacing) ) return false;
         if( !IsEqual(nVertBars , other.nVertBars) ) return false;
         if( !IsEqual(nHorzInterfaceBars , other.nHorzInterfaceBars) ) return false;
         if(ConfinementBarSize != other.ConfinementBarSize) return false;

         return true;
      }

      bool operator!=(const SHEARZONEDATA& other) const
      {
         return !operator==(other);
      }
   };

   typedef std::vector<SHEARZONEDATA> ShearZoneVec;
   typedef ShearZoneVec::iterator ShearZoneIterator;
   typedef ShearZoneVec::const_iterator ShearZoneConstIterator;
   typedef ShearZoneVec::const_reverse_iterator ShearZoneConstReverseIterator;

   struct HORIZONTALINTERFACEZONEDATA // Additional horizontal interface zone
   {
      Float64 ZoneLength;
      Float64 BarSpacing;
      matRebar::Size BarSize;
      Float64 nBars;

      // pre-computed values
      Float64 ABar; // area of single bar

      // default constructor
      HORIZONTALINTERFACEZONEDATA():
      ZoneLength(0.0), BarSize(matRebar::bsNone),BarSpacing(0.0),nBars(0.0), ABar(0.0)
      {;}

      bool operator==(const HORIZONTALINTERFACEZONEDATA& other) const
      {
         if( !IsEqual(ZoneLength , other.ZoneLength) ) return false;
         if(BarSize != other.BarSize) return false;
         if( !IsEqual(BarSpacing , other.BarSpacing) ) return false;
         if( !IsEqual(nBars , other.nBars) ) return false;

         return true;
      }

      bool operator!=(const HORIZONTALINTERFACEZONEDATA& other) const
      {
         return !operator==(other);
      }
   };

   typedef std::vector<HORIZONTALINTERFACEZONEDATA> HorizontalInterfaceZoneVec;
   typedef HorizontalInterfaceZoneVec::iterator HorizontalInterfaceZoneIterator;
   typedef HorizontalInterfaceZoneVec::const_iterator HorizontalInterfaceZoneConstIterator;

   // Our Data
   // ========
   ShearZoneVec ShearZones;
   HorizontalInterfaceZoneVec HorizontalInterfaceZones;

   bool bIsRoughenedSurface;
   bool bUsePrimaryForSplitting;
   bool bAreZonesSymmetrical;

   matRebar::Size SplittingBarSize; // additional splitting bars
   Float64 SplittingBarSpacing;
   Float64 SplittingZoneLength;
   Float64 nSplittingBars;

   matRebar::Size ConfinementBarSize; // additional confinement bars - only used if primary not used for confinement
   Float64 ConfinementBarSpacing;
   Float64 ConfinementZoneLength;

   STIRRUPCONFIG():
   bIsRoughenedSurface(true), bUsePrimaryForSplitting(false), bAreZonesSymmetrical(true),
   SplittingBarSize(matRebar::bsNone), SplittingBarSpacing(0.0), SplittingZoneLength(0.0), nSplittingBars(0.0),
   ConfinementBarSize(matRebar::bsNone), ConfinementBarSpacing(0.0), ConfinementZoneLength(0.0)
   {;}

   bool operator==(const STIRRUPCONFIG& other) const
   {
      if(ShearZones != other.ShearZones) return false;
      if(HorizontalInterfaceZones != other.HorizontalInterfaceZones) return false;

      if(bIsRoughenedSurface != other.bIsRoughenedSurface) return false;
      if(bUsePrimaryForSplitting != other.bUsePrimaryForSplitting) return false;
      if(bAreZonesSymmetrical != other.bAreZonesSymmetrical) return false;
      if(SplittingBarSize != other.SplittingBarSize) return false;

      if ( !IsEqual(SplittingBarSpacing,  other.SplittingBarSpacing) ) return false;
      if ( !IsEqual(SplittingZoneLength,  other.SplittingZoneLength) ) return false;
      if ( !IsEqual(nSplittingBars,  other.nSplittingBars) ) return false;

      if(ConfinementBarSize != other.ConfinementBarSize) return false;
      if ( !IsEqual(ConfinementBarSpacing,  other.ConfinementBarSpacing) ) return false;
      if ( !IsEqual(ConfinementZoneLength,  other.ConfinementZoneLength) ) return false;


      return true;
   }

   bool operator!=(const STIRRUPCONFIG& other) const
   {
      return !operator==(other);
   }
};
// NOTE: Data here is not used, but may be useful for someone in the future.
//       This is a preliminary design for modelling longitudinal rebar.
//       After some effort, I determined that the data is not necessessary for the 
//       design algorithm since it is only used for longitudinal reinforcement for shear,
//       and does not need to be iterated on. (e.g., the algoritm just picks a value
//       and submits it directly to the final design).
//-----------------------------------------------------------------------------
// Struct for longitudinal rebar information.
/*
struct LONGITUDINALREBARCONFIG
{
public:
   struct RebarRow 
   {
      pgsTypes::GirderFace  Face;
      matRebar::Size BarSize;
      Int32       NumberOfBars;
      Float64     Cover;
      Float64     BarSpacing;

      bool operator==(const RebarRow& other) const
      {
         if(Face != other.Face) return false;
         if(BarSize != other.BarSize) return false;
         if ( !IsEqual(Cover,  other.Cover) ) return false;
         if ( !IsEqual(BarSpacing,  other.BarSpacing) ) return false;
         if ( !IsEqual(NumberOfBars,  other.NumberOfBars) ) return false;

         return true;
      };
   };

   matRebar::Type BarType;
   matRebar::Grade BarGrade;
   std::vector<RebarRow> RebarRows;

   bool operator==(const LONGITUDINALREBARCONFIG& other) const
   {
      if(BarType != other.BarType) return false;
      if(BarGrade != other.BarGrade) return false;
      if(RebarRows != other.RebarRows) return false;
   }

   bool operator!=(const LONGITUDINALREBARCONFIG& other) const
   {
      return !operator==(other);
   }

};
*/

//-----------------------------------------------------------------------------
// Individual strand debond information
struct DEBONDINFO
{
   StrandIndexType strandIdx; // index of strand that is debonded
   double LeftDebondLength;   // length of debond at left end of the girder
   double RightDebondLength;  // length of debond at right end of the girder

   bool operator<(const DEBONDINFO& other) const
   {
      return strandIdx < other.strandIdx;
   }

   bool operator==(const DEBONDINFO& other) const
   {
      bool bEqual = true;
      bEqual &= (strandIdx == other.strandIdx);
      bEqual &= IsEqual(LeftDebondLength,other.LeftDebondLength);
      bEqual &= IsEqual(RightDebondLength,other.RightDebondLength);
      return bEqual;
   }
};
// handy typedefs for storing debond info
typedef std::vector<DEBONDINFO>               DebondInfoCollection;
typedef DebondInfoCollection::iterator        DebondInfoIterator;
typedef DebondInfoCollection::const_iterator  DebondInfoConstIterator;

//-----------------------------------------------------------------------------
// Girder configuration
struct GDRCONFIG
{
   // use on of the pgsTypes::StrandType constants to for array index
   StrandIndexType Nstrands[3];  // Number of strands
   std::vector<DEBONDINFO> Debond[3]; // Information about debonded strands (key is strand index)
   Float64 EndOffset; // Offset of harped strands at end of girder
   Float64 HpOffset;  // Offset of harped strands at the harping point
   Float64 Pjack[3];  // Jacking force
   pgsTypes::TTSUsage TempStrandUsage;
   Float64 Fc;        // 28 day concrete strength
   Float64 Fci;       // Concrete release strength
   pgsTypes::ConcreteType ConcType;
   bool bHasFct;
   Float64 Fct;

   bool bUserEci;
   bool bUserEc;
   Float64 Eci;
   Float64 Ec;

   Float64 SlabOffset[2]; // slab offset at start and end of the girder (use pgsTypes::MemberEndType for array index)

   STIRRUPCONFIG StirrupConfig; // All of our transverse rebar information

//   LONGITUDINALREBARCONFIG LongitudinalRebarConfig; // Girder-length rebars
   GDRCONFIG()
   {;}

   GDRCONFIG(const GDRCONFIG& rOther)
   {
      MakeCopy( rOther );
   }

   GDRCONFIG& operator=(const GDRCONFIG& rOther)
   {
      if ( this != &rOther )
         MakeAssignment( rOther );

      return *this;
   }

   // Check equality for only flexural date (not stirrups)
   bool IsFlexuralDataEqual(const GDRCONFIG& other) const
   {
      for ( Uint16 type = pgsTypes::Straight; type <= pgsTypes::Temporary; type++ )
      {
         if (Nstrands[type] != other.Nstrands[type]) return false;
         if (Debond[type]   != other.Debond[type])   return false;
         if (!IsEqual(Pjack[type], other.Pjack[type])) return false;
      }

      if (TempStrandUsage != other.TempStrandUsage) return false;

      if ( !IsEqual(EndOffset, other.EndOffset) ) return false;
      if ( !IsEqual(HpOffset,  other.HpOffset) ) return false;
      if ( !IsEqual(Fci, other.Fci) ) return false;
      if ( !IsEqual(Fc,  other.Fc) ) return false;

      if (bUserEci != other.bUserEci) return false;
      if (bUserEc  != other.bUserEc)  return false;
      if (!IsEqual(Eci,other.Eci)) return false;
      if (!IsEqual(Ec, other.Ec)) return false;

      if ( !IsEqual(SlabOffset[pgsTypes::metStart],other.SlabOffset[pgsTypes::metStart]) ) return false;
      if ( !IsEqual(SlabOffset[pgsTypes::metEnd],  other.SlabOffset[pgsTypes::metEnd])   ) return false;

      return true;
   }

   bool operator==(const GDRCONFIG& other) const
   {
       if(!IsFlexuralDataEqual(other))
           return false;

      if (StirrupConfig != other.StirrupConfig) return false;

      return true;
   }

   bool operator!=(const GDRCONFIG& other) const
   {
      return !operator==(other);
   }

private:
void MakeCopy( const GDRCONFIG& rOther )
{
   g_Ncopies++;

   Nstrands[0] = rOther.Nstrands[0];
   Nstrands[1] = rOther.Nstrands[1];
   Nstrands[2] = rOther.Nstrands[2];
   Debond[0] = rOther.Debond[0];
   Debond[1] = rOther.Debond[1];
   Debond[2] = rOther.Debond[2];
   EndOffset = rOther.EndOffset;
   HpOffset = rOther.HpOffset;
   Pjack[0] = rOther.Pjack[0];
   Pjack[1] = rOther.Pjack[1];
   Pjack[2] = rOther.Pjack[2];
   TempStrandUsage = rOther.TempStrandUsage;
   Fc = rOther.Fc;
   Fci = rOther.Fci;
   ConcType = rOther.ConcType;
   bHasFct = rOther.bHasFct;
   Fct = rOther.Fct;

   bUserEci = rOther.bUserEci;
   bUserEc = rOther.bUserEc;
   Eci = rOther.Eci;
   Ec = rOther.Ec;

   SlabOffset[0] = rOther.SlabOffset[0];
   SlabOffset[1] = rOther.SlabOffset[1];

   StirrupConfig = rOther.StirrupConfig;
}

void MakeAssignment( const GDRCONFIG& rOther )
{
   MakeCopy( rOther );
}

};


struct HANDLINGCONFIG
{
   GDRCONFIG GdrConfig;
   Float64 LeftOverhang;
   Float64 RightOverhang;  // overhang closest to cab of truck when used from hauling
};

enum arFlexuralDesignType { dtNoDesign, dtDesignForHarping, dtDesignForDebonding, dtDesignFullyBonded };
enum arDesignStrandFillType { ftGridOrder, ftMinimizeHarping };
enum arDesignStirrupLayoutType { slLayoutStirrups, slRetainExistingLayout };

struct arDesignOptions
{
   arFlexuralDesignType doDesignForFlexure;
   bool doDesignSlabOffset;
   bool doDesignLifting;
   bool doDesignHauling;
   bool doDesignSlope;
   bool doDesignHoldDown;

   arDesignStrandFillType doStrandFillType;

   bool doDesignForShear;

   arDesignStirrupLayoutType doDesignStirrupLayout;

   arDesignOptions(): doDesignForFlexure(dtNoDesign), doDesignSlabOffset(false), doDesignLifting(false), doDesignHauling(false),
                      doDesignSlope(false), doDesignHoldDown(false), doDesignForShear(false), 
                      doStrandFillType(ftMinimizeHarping), doDesignStirrupLayout(slLayoutStirrups)
   {;}
};


// measurement methods of harped strand offsets
typedef enum HarpedStrandOffsetType
{
   hsoLEGACY        = -1,    // Method used pre-version 6.0
   hsoCGFROMTOP     = 0,    // CG of harped strands from top
   hsoCGFROMBOTTOM  = 1,    // CG of   "       "      "  bottom
   hsoTOP2TOP       = 2,    // Top-most strand to top of girder
   hsoTOP2BOTTOM    = 3,    // Top-most strand to bottom of girder
   hsoBOTTOM2BOTTOM = 4,    // Bottom-most strand to bottom of girder
   hsoECCENTRICITY  = 5     // Eccentricity of total strand group
} HarpedStrandOffsetType;

enum ShearFlowMethod
{
   // compute horizontal interface shear flow between slab and girder using
   sfmLRFD,     // LRFD simplified equation
   sfmClassical // Classical equation (VQ/I)
};

enum ShearCapacityMethod
{
   // NOTE: enum values are in a weird order... the constants are set so that they
   //       are consistent with previous versions of PGSuper. DO NOT CHANGE CONSTANTS
   scmBTEquations = 0, // LRFD 5.8.3.5
   scmVciVcw      = 2, // LRFD 5.8.3.6 (Vci, Vcw - added in 2007)
   scmWSDOT2001   = 1, // WSDOT BDM Method (June 2001 Design Memo)
   scmBTTables    = 3, // LRFD B5.1
   scmWSDOT2007   = 4  // WSDOT BDM Method (August 2007 Design Memo - Use new BT equations from to be published 2008 interims)
};

// functions to hash and un-hash span/girder for file loading and saving
inline DWORD HashGirderSpacing(pgsTypes::MeasurementLocation ml,pgsTypes::MeasurementType mt)
{
   return MAKELONG(ml,mt);
}

inline void UnhashGirderSpacing(DWORD_PTR girderSpacingHash,pgsTypes::MeasurementLocation *ml,pgsTypes::MeasurementType *mt)
{
   *ml = (pgsTypes::MeasurementLocation)LOWORD(girderSpacingHash);
   *mt = (pgsTypes::MeasurementType)HIWORD(girderSpacingHash);
}

inline bool IsGirderSpacing(pgsTypes::SupportedBeamSpacing sbs)
{
   // spacing type is a girder spacing and not a joint spacing
   if ( sbs == pgsTypes::sbsUniform || 
        sbs == pgsTypes::sbsGeneral || 
        sbs == pgsTypes::sbsConstantAdjacent
      )
      return true;
   else
      return false;
}

inline bool IsJointSpacing(pgsTypes::SupportedBeamSpacing sbs)
{
   return !IsGirderSpacing(sbs);
}

inline bool IsBridgeSpacing(pgsTypes::SupportedBeamSpacing sbs)
{
   // spacing type is for the whole bridge and not span-by-span
   if ( sbs == pgsTypes::sbsUniform || 
        sbs == pgsTypes::sbsUniformAdjacent || 
        sbs == pgsTypes::sbsConstantAdjacent 
      )
      return true;
   else
      return false;
}

inline bool IsConstantWidthDeck(pgsTypes::SupportedDeckType deckType)
{
   return (deckType == pgsTypes::sdtNone || deckType == pgsTypes::sdtCompositeOverlay);
}

inline bool IsAdjustableWidthDeck(pgsTypes::SupportedDeckType deckType)
{
   return !IsConstantWidthDeck(deckType);
}

inline bool IsSpanSpacing(pgsTypes::SupportedBeamSpacing sbs)
{
   return !IsBridgeSpacing(sbs);
}

inline bool IsSpreadSpacing(pgsTypes::SupportedBeamSpacing sbs)
{
   // spacing type is for spread beams
   if ( sbs == pgsTypes::sbsUniform || 
        sbs == pgsTypes::sbsGeneral
      )
      return true;
   else
      return false;
}

inline bool IsAdjacentSpacing(pgsTypes::SupportedBeamSpacing sbs)
{
   // spacing type is for adjacent beams
   return !IsSpreadSpacing(sbs);
}

inline bool IsRatingLimitState(pgsTypes::LimitState ls)
{
   if ( ls == pgsTypes::StrengthI_Inventory    ||
        ls == pgsTypes::StrengthI_Operating    ||
        ls == pgsTypes::StrengthI_LegalRoutine ||
        ls == pgsTypes::StrengthI_LegalSpecial ||
        ls == pgsTypes::StrengthII_PermitRoutine ||
        ls == pgsTypes::StrengthII_PermitSpecial 
      )
   {
      return true;
   }
   else
   {
      return false;
   }
}

inline bool IsStrengthLimitState(pgsTypes::LimitState ls)
{
   if ( ls == pgsTypes::StrengthI              || 
        ls == pgsTypes::StrengthII             ||
        ls == pgsTypes::StrengthI_Inventory    ||
        ls == pgsTypes::StrengthI_Operating    ||
        ls == pgsTypes::StrengthI_LegalRoutine ||
        ls == pgsTypes::StrengthI_LegalSpecial ||
        ls == pgsTypes::StrengthII_PermitRoutine ||
        ls == pgsTypes::StrengthII_PermitSpecial 
      )
   {
      return true;
   }
   else
   {
      return false;
   }
}

inline bool IsFatigueLimitState(pgsTypes::LimitState ls)
{
   return (ls == pgsTypes::FatigueI);
}

inline bool IsServiceLimitState(pgsTypes::LimitState ls)
{
   return !IsStrengthLimitState(ls) && !IsFatigueLimitState(ls);
}

inline bool IsRatingLiveLoad(pgsTypes::LiveLoadType llType)
{
   if ( llType == pgsTypes::lltDesign              || // doubles as a design and rating live load
        llType == pgsTypes::lltLegalRating_Routine ||
        llType == pgsTypes::lltLegalRating_Special ||
        llType == pgsTypes::lltPermitRating_Routine || 
        llType == pgsTypes::lltPermitRating_Special
      )
   {
      return true;
   }
   else
   {
      return false;
   }
}

inline bool IsDesignLiveLoad(pgsTypes::LiveLoadType llType)
{
   if ( llType == pgsTypes::lltDesign )
      return true;
   else
      return !IsRatingLiveLoad(llType);
}

inline pgsTypes::LoadRatingType RatingTypeFromLimitState(pgsTypes::LimitState ls)
{
   pgsTypes::LoadRatingType ratingType;
   switch(ls)
   {
   case pgsTypes::StrengthI_Inventory:
   case pgsTypes::ServiceIII_Inventory:
      ratingType = pgsTypes::lrDesign_Inventory;
      break;

   case pgsTypes::StrengthI_Operating:
   case pgsTypes::ServiceIII_Operating:
      ratingType = pgsTypes::lrDesign_Operating;
      break;

   case pgsTypes::StrengthI_LegalRoutine:
   case pgsTypes::ServiceIII_LegalRoutine:
      ratingType = pgsTypes::lrLegal_Routine;
      break;

   case pgsTypes::StrengthI_LegalSpecial:
   case pgsTypes::ServiceIII_LegalSpecial:
      ratingType = pgsTypes::lrLegal_Special;
      break;

   case pgsTypes::StrengthII_PermitRoutine:
   case pgsTypes::ServiceI_PermitRoutine:
      ratingType = pgsTypes::lrPermit_Routine;
      break;

   case pgsTypes::StrengthII_PermitSpecial:
   case pgsTypes::ServiceI_PermitSpecial:
      ratingType = pgsTypes::lrPermit_Special;
      break;

   default:
      ATLASSERT(false); // either there is a new rating type or you used a design limit state
      ratingType = pgsTypes::lrDesign_Inventory;
   }

   return ratingType;
}

inline pgsTypes::LiveLoadType GetLiveLoadType(pgsTypes::LoadRatingType ratingType)
{
   pgsTypes::LiveLoadType llType;
   switch( ratingType )
   {
   case pgsTypes::lrDesign_Inventory:
   case pgsTypes::lrDesign_Operating:
      llType = pgsTypes::lltDesign;
      break;

   case pgsTypes::lrLegal_Routine:
      llType = pgsTypes::lltLegalRating_Routine;
      break;

   case pgsTypes::lrLegal_Special:
      llType = pgsTypes::lltLegalRating_Special;
      break;

   case pgsTypes::lrPermit_Routine:
      llType = pgsTypes::lltPermitRating_Routine;
      break;

   case pgsTypes::lrPermit_Special:
      llType = pgsTypes::lltPermitRating_Special;
      break;

   default:
      ATLASSERT(false); // SHOULD NEVER GET HERE
   }

   return llType;
}

inline CComBSTR GetLiveLoadTypeName(pgsTypes::LiveLoadType llType)
{
   CComBSTR bstrName;
   switch(llType)
   {
   case pgsTypes::lltDesign:
      bstrName = "Design";
      break;

   case pgsTypes::lltFatigue:
      bstrName = "Fatigue";
      break;

   case pgsTypes::lltLegalRating_Routine:
      bstrName = "Legal Load - Routine Commercial Traffic";
      break;

   case pgsTypes::lltLegalRating_Special:
      bstrName = "Legal Load - Specialized Hauling Vehicles";
      break;

   case pgsTypes::lltPedestrian:
      bstrName = "Pedestrian";
      break;

   case pgsTypes::lltPermit:
      bstrName = "Design Permit";
      break;

   case pgsTypes::lltPermitRating_Routine:
      bstrName = "Rating Permit - Routine/Annual Permit";
      break;

   case pgsTypes::lltPermitRating_Special:
      bstrName = "Rating Permit - Special/Limited Crossing Permit";
      break;

   default:
      ATLASSERT(false); // SHOULD NEVER GET HERE
   }

   return bstrName;
}

inline CComBSTR GetLiveLoadTypeName(pgsTypes::LoadRatingType ratingType)
{
   pgsTypes::LiveLoadType llType = ::GetLiveLoadType(ratingType);
   CComBSTR bstrName = GetLiveLoadTypeName(llType);

   if ( ratingType == pgsTypes::lrDesign_Inventory )
      bstrName += CComBSTR(" - Inventory");

   if ( ratingType == pgsTypes::lrDesign_Operating )
      bstrName += CComBSTR(" - Operating");

   return bstrName;
}

#endif // INCLUDED_PGSUPERTYPES_H_