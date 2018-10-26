///////////////////////////////////////////////////////////////////////
// PGSuper Beam Factory
// Copyright � 1999-2011  Washington State Department of Transportation
//                        Bridge and Structures Office
//
// This library is a part of the Washington Bridge Foundation Libraries
// and was developed as part of the Alternate Route Project
//
// This library is free software; you can redistribute it and/or modify it under
// the terms of the Alternate Route Library Open Source License as published by 
// the Washington State Department of Transportation, Bridge and Structures Office.
//
// This program is distributed in the hope that it will be useful, but is distributed 
// AS IS, WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
// or FITNESS FOR A PARTICULAR PURPOSE. See the Alternate Route Library Open Source 
// License for more details.
//
// You should have received a copy of the Alternate Route Library Open Source License 
// along with this program; if not, write to the Washington State Department of 
// Transportation, Bridge and Structures Office, P.O. Box  47340, 
// Olympia, WA 98503, USA or e-mail Bridge_Support@wsdot.wa.gov
///////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////
// The intent of licensing this interface with the ARLOSL is to provide
// third party developers a method of developing proprietary plug-ins
// to the software.
// 
// Any changes made to the interfaces defined in this file are
// are subject to the terms of the Alternate Route Library Open Source License.
//
// Components that implement the interfaces defined in this file are
// governed by the terms and conditions deemed appropriate by legal 
// copyright holder of said software.
///////////////////////////////////////////////////////////////////////

#pragma once

#include <Units\Units.h>
#include <PGSuperTypes.h>
#include <PgsExt\PoiMgr.h>

struct IDistFactorEngineer;
struct IEffFlangeEngineer;
struct IPsLossEngineer;
struct IBroker;

interface IShape;

struct ISuperstructureMember;
struct IGirderSection;
struct IStrandMover;

/*****************************************************************************
INTERFACE
   IBeamFactory

   Interface for creating generic precast beams and related objects.

DESCRIPTION
   Interface for creating generic precast beams and related objects.      
*****************************************************************************/
// {D3810B3E-91D6-4aed-A748-8ABEB87FCF44}
DEFINE_GUID(IID_IBeamFactory, 
0xd3810b3e, 0x91d6, 0x4aed, 0xa7, 0x48, 0x8a, 0xbe, 0xb8, 0x7f, 0xcf, 0x44);
interface IBeamFactory : IUnknown
{
   typedef std::pair<std::_tstring,double> Dimension;
   typedef std::vector<Dimension> Dimensions;

   enum BeamFace {BeamTop, BeamBottom};

   //---------------------------------------------------------------------------------
   // Creates a new girder section using the supplied dimensions
   virtual void CreateGirderSection(IBroker* pBroker,StatusGroupIDType statusGroupID,SpanIndexType spanIdx,GirderIndexType gdrIdx,const IBeamFactory::Dimensions& dimensions,IGirderSection** ppSection) = 0;

   //---------------------------------------------------------------------------------
   // Creates a new girder profile shape using the supplied dimensions
   // This shape is used to draw the girder in profile (elevation)
   virtual void CreateGirderProfile(IBroker* pBroker,StatusGroupIDType statusGroupID,SpanIndexType spanIdx,GirderIndexType gdrIdx,const IBeamFactory::Dimensions& dimensions,IShape** ppShape) = 0;

   //---------------------------------------------------------------------------------
   // Lays out the girder along the given superstructure member. This function must
   // create the segments that describe the girder line
   virtual void LayoutGirderLine(IBroker* pBroker,StatusGroupIDType statusGroupID,SpanIndexType spanIdx,GirderIndexType gdrIdx,ISuperstructureMember* ssmbr) = 0;

   //---------------------------------------------------------------------------------
   // Adds Points of interest at all cross section changes.
   // Note to implementer: use the <section change type> | POI_TABULAR | POI_GRAPHICAL attribute. Add other attributes as needed (such as POI_ALLACTIONS).
   // Section change type attributes are POI_SECTCHANGE_LEFTFACE, POI_SECTCHANGE_RIGHTFACE, and POI_SECTCHANGE_TRANSITION.
   // Use POI_SECTCHANGE_RIGHTFACE at start of member and POI_SECTCHANGE_LEFT face at end of member. Also,
   // use when there is an abrupt change in the section. Use POI_SECTCHANGE_TRANSITION wherever there is a smooth transition point.
   // For the casting yard POIs, put at least one poi at the start and end of the girder. 
   // For the bridge site POIs, put at least one poi at the stand and end bearings. DO NOT put bridge site POIs
   // before or after the girder bearings
   virtual void LayoutSectionChangePointsOfInterest(IBroker* pBroker,SpanIndexType spanIdx,GirderIndexType gdrIdx,pgsPoiMgr* pPoiMgr) = 0;

   //---------------------------------------------------------------------------------
   // Creates an object that implements the IDistFactorEngineer interface. The returned
   // object is a COM object an must be managed through its reference count.
   //
   // Implementation Note: You must call SetBroker on the newly create object and supply
   // it with the pointer to the broker object provided by the caller.
   // const pointers have valid values to be used if non-NULL
   virtual void CreateDistFactorEngineer(IBroker* pBroker,StatusGroupIDType statusGroupID,const pgsTypes::SupportedDeckType* pDeckType, const pgsTypes::AdjacentTransverseConnectivity* pConnect,IDistFactorEngineer** ppEng) = 0;

   //---------------------------------------------------------------------------------
   // Creates an object that implements the IPsLossEngineer interface. The returned
   // object is a COM object an must be managed through its reference count.
   //
   // Implementation Note: You must call SetBroker on the newly create object and supply
   // it with the pointer to the broker object provided by the caller.
   virtual void CreatePsLossEngineer(IBroker* pBroker,StatusGroupIDType statusGroupID,SpanIndexType spanIdx,GirderIndexType gdrIdx,IPsLossEngineer** ppEng) = 0;

   //---------------------------------------------------------------------------------
   // The StrandMover object knows how to move harped strands within the section when
   // the group elevation is changed
   virtual void CreateStrandMover(const IBeamFactory::Dimensions& dimensions, 
                                  IBeamFactory::BeamFace endTopFace, double endTopLimit, IBeamFactory::BeamFace endBottomFace, double endBottomLimit, 
                                  IBeamFactory::BeamFace hpTopFace, double hpTopLimit, IBeamFactory::BeamFace hpBottomFace, double hpBottomLimit, 
                                  double endIncrement, double hpIncrement, IStrandMover** strandMover) = 0;

   //---------------------------------------------------------------------------------
   // Returns a vector of strings representing the names of the dimensions that are used
   // to describe the cross section.
   virtual std::vector<std::_tstring> GetDimensionNames() = 0;

   //---------------------------------------------------------------------------------
   // Returns a vector of length unit objects representing the units of measure of each
   // dimenions. If an item in the vector is NULL, the dimension is a scalar.
   virtual std::vector<const unitLength*> GetDimensionUnits(bool bSIUnits) = 0;

   //---------------------------------------------------------------------------------
   // Returns a defaults for the dimensions. Values are order to match the vector returned by
   // GetDimensionNames
   virtual std::vector<double> GetDefaultDimensions() = 0;

   //---------------------------------------------------------------------------------
   // Validates the dimensions. Return true if the dimensions are OK, otherwise false.
   // Return an error message through the strErrMsg pointer. If the error message
   // contains values, use the unit object to convert the value to display units and
   // append the appropreate unit tag
   virtual bool ValidateDimensions(const IBeamFactory::Dimensions& dimensions,bool bSIUnits,std::_tstring* strErrMsg) = 0;

   //---------------------------------------------------------------------------------
   // Saves the section dimensions to the storage unit
   virtual void SaveSectionDimensions(sysIStructuredSave* pSave,const IBeamFactory::Dimensions& dimensions) = 0;

   //---------------------------------------------------------------------------------
   // Load the section dimensions from the storage unit
   virtual IBeamFactory::Dimensions LoadSectionDimensions(sysIStructuredLoad* pLoad) = 0;

   //---------------------------------------------------------------------------------
   // Returns true if the non-composite beam section is prismatic
   virtual bool IsPrismatic(IBroker* pBroker,SpanIndexType spanIdx,GirderIndexType gdrIdx) = 0;

   //---------------------------------------------------------------------------------
   // Returns the volume of the member
   virtual Float64 GetVolume(IBroker* pBroker,SpanIndexType spanIdx,GirderIndexType gdrIdx) = 0;

   //---------------------------------------------------------------------------------
   // Returns the surface area of the member including that of internal voids
   // If bReduceForPoorlyVentilatedVoids is true, the surface area of internal voids is
   // reduce by 50% per LRFD 5.4.2.3.2
   virtual Float64 GetSurfaceArea(IBroker* pBroker,SpanIndexType spanIdx,GirderIndexType gdrIdx,bool bReduceForPoorlyVentilatedVoids) = 0;

   //---------------------------------------------------------------------------------
   // Returns the name of an image file that will be used in reports when the
   // cross section dimensions are reported. The image file must be in the same
   // directory as PGSuper.exe
   virtual std::_tstring GetImage() = 0;

   virtual std::_tstring GetSlabDimensionsImage(pgsTypes::SupportedDeckType deckType) = 0;
   virtual std::_tstring GetPositiveMomentCapacitySchematicImage(pgsTypes::SupportedDeckType deckType) = 0;
   virtual std::_tstring GetNegativeMomentCapacitySchematicImage(pgsTypes::SupportedDeckType deckType) = 0;
   virtual std::_tstring GetShearDimensionsSchematicImage(pgsTypes::SupportedDeckType deckType) = 0;
   virtual std::_tstring GetInteriorGirderEffectiveFlangeWidthImage(IBroker* pBroker,pgsTypes::SupportedDeckType deckType) = 0;
   virtual std::_tstring GetExteriorGirderEffectiveFlangeWidthImage(IBroker* pBroker,pgsTypes::SupportedDeckType deckType) = 0;

   //---------------------------------------------------------------------------------
   // Returns the class identifier for the beam factory
   virtual CLSID GetCLSID() = 0;

   //---------------------------------------------------------------------------------
   // Returns the class identifier for the beam family
   virtual CLSID GetFamilyCLSID() = 0;

   //---------------------------------------------------------------------------------
   // Returns a name string that identifies the general type of beam
   // this is not guarenteed to be unique
   virtual std::_tstring GetGirderFamilyName() = 0;

   //---------------------------------------------------------------------------------
   // Returns the name of the company, organization, and/or person that published the
   // beam factory
   virtual std::_tstring GetPublisher() = 0;

   //---------------------------------------------------------------------------------
   // Returns the instance handle for resources
   virtual HINSTANCE GetResourceInstance() = 0;

   //---------------------------------------------------------------------------------
   // Returns the string name of the image resource that is used in the girder
   // library entry dialog. This resource must be an enhanced meta file.
   // Height and Width of the image must be equal
   virtual LPCTSTR GetImageResourceName() = 0;

   //---------------------------------------------------------------------------------
   // Returns the icon associated with the beam type
   // Icon should support both 16x16 and 32x32 formates
   virtual HICON GetIcon() = 0;

   //---------------------------------------------------------------------------------
   // Returns the deck types that may be used with a giving spacing type
   virtual pgsTypes::SupportedDeckTypes GetSupportedDeckTypes(pgsTypes::SupportedBeamSpacing sbs) = 0;
   
   //---------------------------------------------------------------------------------
   // Returns all of methods of beam spacing measurement, supported by this beam
   virtual pgsTypes::SupportedBeamSpacings GetSupportedBeamSpacings() = 0;

   //---------------------------------------------------------------------------------
   // Returns allowable spacing distances for the given deck and spacing type.
   // Max spacing will be MAX_GIRDER_SPACING of no range is specified
   // Spacing is measured normal to the centerline of a typical girder
   virtual void GetAllowableSpacingRange(const IBeamFactory::Dimensions& dimensions, pgsTypes::SupportedDeckType sdt, pgsTypes::SupportedBeamSpacing sbs, double* minSpacing, double* maxSpacing) = 0;

   //---------------------------------------------------------------------------------
   // Returns the number of webs that the section has
   virtual WebIndexType GetNumberOfWebs(const IBeamFactory::Dimensions& dimensions) = 0;

   //---------------------------------------------------------------------------------
   // Returns the height of the beam at the specified end
   virtual Float64 GetBeamHeight(const IBeamFactory::Dimensions& dimensions,pgsTypes::MemberEndType endType) = 0;

   //---------------------------------------------------------------------------------
   // Returns the width of the beam at the specified end
   virtual Float64 GetBeamWidth(const IBeamFactory::Dimensions& dimensions,pgsTypes::MemberEndType endType) = 0;

   //---------------------------------------------------------------------------------
   // Shear key.
   // Area comes in two parts: first is contant for any spacing, second is factor to multiply by spacing
   virtual bool IsShearKey(const IBeamFactory::Dimensions& dimensions, pgsTypes::SupportedBeamSpacing spacingType)=0;
   virtual void GetShearKeyAreas(const IBeamFactory::Dimensions& dimensions, pgsTypes::SupportedBeamSpacing spacingType,Float64* uniformArea, Float64* areaPerJoint)=0;

};

