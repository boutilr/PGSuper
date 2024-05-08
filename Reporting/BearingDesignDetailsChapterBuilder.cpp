///////////////////////////////////////////////////////////////////////
// PGSuper - Prestressed Girder SUPERstructure Design and Analysis
// Copyright � 1999-2023  Washington State Department of Transportation
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
#include <Reporting\BearingDesignDetailsChapterBuilder.h>
#include <Reporting\ReportNotes.h>
#include <Reporting\BearingReactionTable.h>
#include <Reporting\BearingRotationTable.h>
#include <Reporting\BearingShearDeformationTable.h>
#include <Reporting\PrestressRotationTable.h>
#include <Reporting\UserReactionTable.h>
#include <Reporting\UserRotationTable.h>

#include <Reporting\VehicularLoadResultsTable.h>
#include <Reporting\VehicularLoadReactionTable.h>
#include <Reporting\CombinedReactionTable.h>
#include <Reporting\ReactionInterfaceAdapters.h>

#include <IFace\Project.h>
#include <IFace\Bridge.h>
#include <IFace\AnalysisResults.h>
#include <IFace\BearingDesignParameters.h>
#include <EAF\EAFDisplayUnits.h>
#include <IFace\Intervals.h>
#include <IFace\DistributionFactors.h>

#include <PgsExt\PierData2.h>
#include <Reporting/BearingDesignPropertiesTable.h>


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/****************************************************************************
CLASS
   CBearingDesignParametersChapterBuilder
****************************************************************************/


////////////////////////// PUBLIC     ///////////////////////////////////////

//======================== LIFECYCLE  =======================================
CBearingDesignDetailsChapterBuilder::CBearingDesignDetailsChapterBuilder(bool bSelect) :
    CPGSuperChapterBuilder(bSelect)
{
}

//======================== OPERATORS  =======================================
//======================== OPERATIONS =======================================
LPCTSTR CBearingDesignDetailsChapterBuilder::GetName() const
{
    return TEXT("Bearing Design Details");
}

rptChapter* CBearingDesignDetailsChapterBuilder::Build(const std::shared_ptr<const WBFL::Reporting::ReportSpecification>& pRptSpec, Uint16 level) const
{
    auto pGirderRptSpec = std::dynamic_pointer_cast<const CGirderReportSpecification>(pRptSpec);
    CComPtr<IBroker> pBroker;
    pGirderRptSpec->GetBroker(&pBroker);
    const CGirderKey& girderKey(pGirderRptSpec->GetGirderKey());


    rptChapter* pChapter = CPGSuperChapterBuilder::Build(pRptSpec, level);

    GET_IFACE2(pBroker, IUserDefinedLoads, pUDL);
    bool are_user_loads = pUDL->DoUserLoadsExist(girderKey);

    GET_IFACE2(pBroker, IBearingDesign, pBearingDesign);

    bool bIncludeImpact = pBearingDesign->BearingLiveLoadReactionsIncludeImpact();

    GET_IFACE2(pBroker, ISpecification, pSpec);

    GET_IFACE2(pBroker, IEAFDisplayUnits, pDisplayUnits);
    INIT_UV_PROTOTYPE(rptStressUnitValue, stress, pDisplayUnits->GetStressUnit(), true);
    INIT_UV_PROTOTYPE(rptLengthUnitValue, length, pDisplayUnits->GetSpanLengthUnit(), true);
    INIT_UV_PROTOTYPE(rptLength4UnitValue, I, pDisplayUnits->GetMomentOfInertiaUnit(), true);
    INIT_UV_PROTOTYPE(rptLength2UnitValue, A, pDisplayUnits->GetAreaUnit(), true);
    INIT_UV_PROTOTYPE(rptLengthUnitValue, deflection, pDisplayUnits->GetDeflectionUnit(), true);
    INIT_UV_PROTOTYPE(rptTemperatureUnitValue, temperature, pDisplayUnits->GetTemperatureUnit(), true);


    GET_IFACE2(pBroker, IBearingDesignParameters, pBearingDesignParameters);
    DESIGNPROPERTIES details;
    pBearingDesignParameters->GetBearingDesignProperties(&details);

    rptParagraph* p = new rptParagraph(rptStyleManager::GetHeadingStyle());
    *pChapter << p;
    *p << _T("Bearing Movement Description") << rptNewLine;

    *p << rptRcImage(std::_tstring(rptStyleManager::GetImagePath()) + _T("bearing_orientation_description.png")) << rptNewLine;

    *p << _T("Bearing Design Properties") << rptNewLine;
    p = new rptParagraph;
    *pChapter << p;
    *p << Sub2(_T("F"), _T("y")) << _T(" = ") << stress.SetValue(details.Fy);
    *p << _T(" for steel shims per AASHTO M251 (ASTM A 1011 Gr. 36)") << rptNewLine;
    *p << Sub2(_T("F"), _T("th")) << _T(" = ") << stress.SetValue(details.Fth);
    *p << _T(" LRFD Article 6.6 (Table 6.6.1.2.3-1 for Category A)") << rptNewLine;
    *p << _T("Method B is used per WSDOT Policy (BDM Ch. 9.2)") << rptNewLine;

    ColumnIndexType nCols = 7;

    CString label = _T("Elastomer Shear Modulus (From Table 14.7.6.2-1)");
    rptRcTable* p_table = rptStyleManager::CreateDefaultTable(nCols, label);

    p_table->SetColumnStyle(0, rptStyleManager::GetTableCellStyle(CB_NONE | CJ_LEFT));
    p_table->SetStripeRowColumnStyle(0, rptStyleManager::GetTableStripeRowCellStyle(CB_NONE | CJ_LEFT));

    ColumnIndexType col = 0;
    p_table->SetNumberOfHeaderRows(2);
    p_table->SetRowSpan(0, col, 2);
    (*p_table)(0, col++) << _T("");
    p_table->SetColumnSpan(0, col, 2);
    (*p_table)(0, col) << _T("50 Hardness");
    (*p_table)(1, col++) << COLHDR(Sub2(_T("G"), _T("min")),
        rptStressUnitTag, pDisplayUnits->GetStressUnit());
    (*p_table)(1, col++) << COLHDR(Sub2(_T("G"), _T("max")),
        rptStressUnitTag, pDisplayUnits->GetStressUnit());
    p_table->SetColumnSpan(0, col, 2);
    (*p_table)(0, col) << _T("60 Hardness");
    (*p_table)(1, col++) << COLHDR(Sub2(_T("G"), _T("min")),
        rptStressUnitTag, pDisplayUnits->GetStressUnit());
    (*p_table)(1, col++) << COLHDR(Sub2(_T("G"), _T("max")),
        rptStressUnitTag, pDisplayUnits->GetStressUnit());
    p_table->SetColumnSpan(0, col, 2);
    (*p_table)(0, col) << _T("70 Hardness");
    (*p_table)(1, col++) << COLHDR(Sub2(_T("G"), _T("min")),
        rptStressUnitTag, pDisplayUnits->GetStressUnit());
    (*p_table)(1, col++) << COLHDR(Sub2(_T("G"), _T("max")),
        rptStressUnitTag, pDisplayUnits->GetStressUnit());


    col = 0;
    (*p_table)(2, col++) << _T("Design Value");
    (*p_table)(2, col++) << stress.SetValue(details.Gmin50);
    (*p_table)(2, col++) << stress.SetValue(details.Gmax50);
    (*p_table)(2, col++) << stress.SetValue(details.Gmin60);
    (*p_table)(2, col++) << stress.SetValue(details.Gmax60);
    (*p_table)(2, col++) << stress.SetValue(details.Gmin70);
    (*p_table)(2, col++) << stress.SetValue(details.Gmax70);

    GET_IFACE2(pBroker, IBridge, pBridge);

    *p << p_table;

    *p << CBearingReactionTable().BuildBearingReactionTable(pBroker, girderKey, pSpec->GetAnalysisType(), bIncludeImpact,
        true, true, are_user_loads, true, pDisplayUnits, true);
    *p << _T("*Live loads due not include impact") << rptNewLine;

    *p << CBearingRotationTable().BuildBearingRotationTable(pBroker, girderKey, pSpec->GetAnalysisType(), bIncludeImpact,
        true, true,are_user_loads, true, pDisplayUnits, true, true);
    *p << _T("*Live loads due not include impact") << rptNewLine;

    *p << CBearingRotationTable().BuildBearingRotationTable(pBroker, girderKey, pSpec->GetAnalysisType(), bIncludeImpact,
        true, true, are_user_loads, true, pDisplayUnits, true, false);

    *p << _T("*Live loads due not include impact") << rptNewLine;
    *p << _T("**Torsional rotations are calculated using ") << Sub2(symbol(theta), _T("t")) << _T(" = ") << Sub2(symbol(theta), _T("f")) << _T("tan") << Sub2(symbol(theta), _T("skew")) << rptNewLine << rptNewLine;


    GET_IFACE2(pBroker, ILiveLoadDistributionFactors, pLLDF);
    pLLDF->ReportReactionDistributionFactors(girderKey, pChapter, true);


    p = new rptParagraph(rptStyleManager::GetHeadingStyle());
    *pChapter << p;
    *p << rptNewLine;
    *p << _T("Shear Deformation Details") << rptNewLine;

    p = new rptParagraph(rptStyleManager::GetSubheadingStyle());
    *pChapter << p;
    *p << _T("Shortening due to temperature difference:") << rptNewLine;

    p = new rptParagraph;
    *pChapter << p;

    *p << _T("Temperature range is computed based on Procedure A (Article 3.12.2.1)") << rptNewLine;

    GET_IFACE2(pBroker, ILibrary, pLibrary);
    WBFL::System::Time time;
    bool bPrintDate = WBFL::System::Time::PrintDate(true);
    std::_tstring strServer;
    std::_tstring strConfiguration;
    std::_tstring strMasterLibFile;
    pLibrary->GetMasterLibraryInfo(strServer, strConfiguration, strMasterLibFile, time);

    *p << rptRcImage(std::_tstring(rptStyleManager::GetImagePath()) + _T("thermal_expansion.png")) << rptNewLine;

    *p << symbol(alpha) << _T(" = coefficient of thermal expansion") << rptNewLine;

    if (strConfiguration == _T("WSDOT"))
    {
        *p << Sub2(symbol(DELTA), _T("0")) << _T(" = 0.75 (WSDOT BDM Ch. 9.2.5A)") << rptNewLine;
    }
    else
    {
        *p << Sub2(symbol(DELTA), _T("0")) << _T(" = 0.65 (AASHTO LRFD Sect. 14.7.5.3.2)") << rptNewLine;
    }

    *p << Sub2(_T("L"), _T("pf")) << _T(" = ") << _T("Distance from the apparent point of fixity to bearing") << rptNewLine;

    SHEARDEFORMATIONDETAILS sfDetails;
    pBearingDesignParameters->GetBearingTableParameters(girderKey, &sfDetails);


    GET_IFACE2(pBroker, ILossParameters, pLossParams);
    if (pLossParams->GetLossMethod() != PrestressLossCriteria::LossMethodType::TIME_STEP)
    {
        *p << _T("The location of the point of fixity is one of the following:") << rptNewLine;
        *p << _T("-The midlength of the superstructure between expansion joints") << rptNewLine;
        *p << _T("-The central pier for a bridge with an even number of spans between expansion joints") << rptNewLine;
        *p << _T("-The midpoint of the central span for a bridge with an odd number of spans between expansion joints") << rptNewLine;

        *p << Sub2(_T("T"), _T("max")) << _T(" = maximum temperature used for design") << rptNewLine;
        *p << Sub2(_T("T"), _T("min")) << _T(" = minimum temperature used for design") << rptNewLine;

        p = new rptParagraph(rptStyleManager::GetSubheadingStyle());
        *pChapter << p;
        *p << _T("Shortening of bottom flange, ") << symbol(DELTA) << Sub2(_T("L"), _T("bf"));
        *p << _T(", due to tendon shortening, ") << symbol(DELTA) << Sub2(_T(" L"), _T("ten")) << _T(" :") << rptNewLine;

        p = new rptParagraph;
        *pChapter << p;

        *p << _T("Bottom flange shortening is calculated using PCI BDM Eq. 10.8.3.8.2-6:") << rptNewLine;
        *p << rptRcImage(std::_tstring(rptStyleManager::GetImagePath()) + _T("BottomFlangeShortening.png")) << rptNewLine;
        *p << _T("where") << rptNewLine;
        *p << rptRcImage(std::_tstring(rptStyleManager::GetImagePath()) + _T("radius_of_gyration.png")) << rptNewLine;
    }


    *p << CBearingShearDeformationTable().BuildBearingShearDeformationTable(pBroker, girderKey, pSpec->GetAnalysisType(),
        true, pDisplayUnits, true, true, &sfDetails);

    if (pLossParams->GetLossMethod() != PrestressLossCriteria::LossMethodType::TIME_STEP)
    {
        *p << _T("-Two-thirds of the total girder creep and shrinkage is assumed to occur before girders are erected") << rptNewLine;
    }
    *p << _T("-Bearing reset effects are not considered") << rptNewLine;
    if (pLossParams->GetLossMethod() != PrestressLossCriteria::LossMethodType::TIME_STEP)
    {
        *p << _T("-Deck shrinkage effects are not considered") << rptNewLine << rptNewLine;
    }

    *p << CBearingShearDeformationTable().BuildBearingShearDeformationTable(pBroker, girderKey, pSpec->GetAnalysisType(),
        true, pDisplayUnits, true, false, &sfDetails);

    if (pLossParams->GetLossMethod() != PrestressLossCriteria::LossMethodType::TIME_STEP)
    {
        *p << _T("-Two-thirds of the total girder creep and shrinkage is assumed to occur before girders are erected") << rptNewLine;
    }
    *p << _T("-Bearing reset effects are not considered") << rptNewLine;
    if (pLossParams->GetLossMethod() != PrestressLossCriteria::LossMethodType::TIME_STEP)
    {
        *p << _T("-Deck shrinkage effects are not considered") << rptNewLine << rptNewLine;
    }


    GET_IFACE2(pBroker, IBridgeDescription, pIBridgeDesc);
    GET_IFACE2(pBroker, IGirder, pGirder);

    p = new rptParagraph;
    *pChapter << p;

    rptRcTable* pTable = rptStyleManager::CreateDefaultTable(9, _T("Bearing Recess Geometry"));

    std::_tstring strSlopeTag = pDisplayUnits->GetAlignmentLengthUnit().UnitOfMeasure.UnitTag();

    INIT_FRACTIONAL_LENGTH_PROTOTYPE(recess_dimension, IS_US_UNITS(pDisplayUnits), 8, RoundOff, pDisplayUnits->GetComponentDimUnit(), false, true);

    col = 0;

    (*pTable)(0, col++) << _T("");
    (*pTable)(0, col++) << _T("Girder") << rptNewLine << _T("Slope") << rptNewLine << _T("(") << strSlopeTag << _T("/") << strSlopeTag << _T(")");
    (*pTable)(0, col++) << _T("Excess") << rptNewLine << _T("Camber") << rptNewLine << _T("Slope") << rptNewLine << _T("(") << strSlopeTag << _T("/") << strSlopeTag << _T(")");
    (*pTable)(0, col++) << _T("Bearing") << rptNewLine << _T("Recess") << rptNewLine << _T("Slope") << rptNewLine << _T("(") << strSlopeTag << _T("/") << strSlopeTag << _T(")");
    (*pTable)(0, col++) << _T("* Transverse") << rptNewLine << _T("Bearing") << rptNewLine << _T("Slope") << rptNewLine << _T("(") << strSlopeTag << _T("/") << strSlopeTag << _T(")");
    (*pTable)(0, col++) << _T("** L") << rptNewLine << _T("Recess") << rptNewLine << _T("Length");
    (*pTable)(0, col++) << _T("D") << rptNewLine << _T("Recess") << rptNewLine << _T("Height");
    (*pTable)(0, col++) << Sub2(_T("D"), _T("1"));
    (*pTable)(0, col++) << Sub2(_T("D"), _T("2"));

    RowIndexType row = pTable->GetNumberOfHeaderRows();

    bool bCheckTaperedSolePlate;
    Float64 taperedSolePlateThreshold;
    pSpec->GetTaperedSolePlateRequirements(&bCheckTaperedSolePlate, &taperedSolePlateThreshold);
    bool bNeedTaperedSolePlateTransverse = false; // if bearing recess slope exceeds "taperedSolePlateThreshold", a tapered bearing plate is required per LRFD 14.8.2
    bool bNeedTaperedSolePlateLongitudinal = false; // if bearing recess slope exceeds "taperedSolePlateThreshold", a tapered bearing plate is required per LRFD 14.8.2

    PierIndexType startPierIdx, endPierIdx;
    pBridge->GetGirderGroupPiers(girderKey.groupIndex, &startPierIdx, &endPierIdx);

    GET_IFACE2(pBroker, ICamber, pCamber);
    GET_IFACE2(pBroker, IPointOfInterest, pPoi);

    // we want the final configuration... that would be in the last interval
    GET_IFACE2(pBroker, IIntervals, pIntervals);
    IntervalIndexType intervalIdx = pIntervals->GetIntervalCount() - 1;

    // TRICKY: use adapter class to get correct reaction interfaces
    std::unique_ptr<IProductReactionAdapter> pForces(std::make_unique<BearingDesignProductReactionAdapter>(pBearingDesign, intervalIdx, girderKey));

    INIT_SCALAR_PROTOTYPE(rptRcScalar, scalar, pDisplayUnits->GetScalarFormat());

    for (PierIndexType pierIdx = startPierIdx; pierIdx <= endPierIdx; pierIdx++)
    {
        col = 0;

        if (!pForces->DoReportAtPier(pierIdx, girderKey))
        {
            // Don't report pier if information is not available
            continue;
        }

        (*pTable)(row, col++) << LABEL_PIER_EX(pBridge->IsAbutment(pierIdx), pierIdx);

        CSegmentKey segmentKey = pBridge->GetSegmentAtPier(pierIdx, girderKey);

        Float64 slope1 = pBridge->GetSegmentSlope(segmentKey);
        (*pTable)(row, col++) << scalar.SetValue(slope1);

        pgsPointOfInterest poi;
        pgsTypes::PierFaceType pierFace(pgsTypes::Back);
        if (pierIdx == startPierIdx)
        {
            PoiList vPoi;
            pPoi->GetPointsOfInterest(segmentKey, POI_0L | POI_ERECTED_SEGMENT, &vPoi);
            poi = vPoi.front();

            pierFace = pgsTypes::Ahead;
        }
        else if (pierIdx == endPierIdx)
        {
            PoiList vPoi;
            pPoi->GetPointsOfInterest(segmentKey, POI_10L | POI_ERECTED_SEGMENT, &vPoi);
            poi = vPoi.front();
        }
        else
        {
            poi = pPoi->GetPierPointOfInterest(girderKey, pierIdx);
        }

        Float64 slope2 = pCamber->GetExcessCamberRotation(poi, pgsTypes::CreepTime::Max);
        (*pTable)(row, col++) << scalar.SetValue(slope2);

        const CBearingData2* pbd = pIBridgeDesc->GetBearingData(pierIdx, pierFace, girderKey.girderIndex);

        Float64 slope3 = slope1 + slope2;
        (*pTable)(row, col++) << scalar.SetValue(slope3);

        Float64 transverse_slope = pGirder->GetOrientation(segmentKey);
        (*pTable)(row, col++) << scalar.SetValue(transverse_slope);

        bNeedTaperedSolePlateLongitudinal = taperedSolePlateThreshold < fabs(slope3); // see lrfd 14.8.2
        bNeedTaperedSolePlateTransverse = taperedSolePlateThreshold < fabs(transverse_slope); // see lrfd 14.8.2

        Float64 L = max(pbd->RecessLength, pbd->Length); // don't allow recess to be shorter than bearing
        Float64 D = pbd->RecessHeight;
        Float64 D1 = D + L * slope3 / 2;
        Float64 D2 = D - L * slope3 / 2;

        (*pTable)(row, col++) << recess_dimension.SetValue(L);
        (*pTable)(row, col++) << recess_dimension.SetValue(D);
        (*pTable)(row, col++) << recess_dimension.SetValue(D1);
        (*pTable)(row, col++) << recess_dimension.SetValue(D2);

        row++;
    }

    *p << pTable << rptNewLine;
    *p << _T("* Orientation of the girder cross section with respect to vertical, zero indicates plumb and positive values indicate girder is rotated clockwise") << rptNewLine;
    *p << _T("** L is maximum of input bearing length and recess length") << rptNewLine;

    if (bCheckTaperedSolePlate && (bNeedTaperedSolePlateLongitudinal || bNeedTaperedSolePlateTransverse))
    {
        *p << bgcolor(rptRiStyle::Yellow);
        *p << _T("The inclination of the underside of the girder to the horizontal exceeds ") << taperedSolePlateThreshold << _T(" rad.");
        if (bNeedTaperedSolePlateLongitudinal && !bNeedTaperedSolePlateTransverse)
        {
            *p << _T(" in the longitudinal direction.");
        }
        else if (!bNeedTaperedSolePlateLongitudinal && bNeedTaperedSolePlateTransverse)
        {
            *p << _T(" in the transverse direction.");
        }
        else
        {
            ATLASSERT(bNeedTaperedSolePlateLongitudinal && bNeedTaperedSolePlateTransverse);
            *p << _T(" in the longitudinal and transverse directions.");
        }
        *p << _T(" Per LRFD 14.8.2 a tapered sole plate shall be used in order to provide a level surface.") << rptNewLine;
        *p << bgcolor(rptRiStyle::White);
    }
    *p << rptRcImage(std::_tstring(rptStyleManager::GetImagePath()) + _T("BearingRecessSlope.png")) << rptNewLine;

    return pChapter;
}

std::unique_ptr<WBFL::Reporting::ChapterBuilder>CBearingDesignDetailsChapterBuilder::Clone() const
{
    return std::make_unique<CBearingDesignDetailsChapterBuilder>();
}
