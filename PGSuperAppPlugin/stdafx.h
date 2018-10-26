// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently,
// but are changed infrequently

#pragma once

#ifndef STRICT
#define STRICT
#endif


#include <PGSuperVersion.h>

#include <PGSuperAll.h>

#define _ATL_APARTMENT_THREADED
#define _ATL_NO_AUTOMATIC_NAMESPACE

#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS	// some CString constructors will be explicit

#ifndef _AFX_NO_OLE_SUPPORT
#include <afxole.h>         // MFC OLE classes
#include <afxodlgs.h>       // MFC OLE dialog classes
#include <afxdisp.h>        // MFC OLE automation classes
#endif // _AFX_NO_OLE_SUPPORT

#include <afxwin.h>
#include <afxMDIFrameWndEx.h>
#include <afxMDIChildWndEx.h>

#include "PGSuperAppPlugin\resource.h"
#include <atlbase.h>
#include <atlcom.h>
#include <atlctl.h>

#include <IFace\Tools.h>

#if defined NOGRID
#include "nogrid.h"
#else
#include <grid\gxall.h>
#endif // NOGRID

#include <HtmlHelp.h>
#include <WBFLCore.h>
#include <WBFLTools.h>
#include <WBFLGeometry.h>
#include <WBFLSections.h>
#include <WBFLCogo.h>
#include <WBFLGenericBridge.h>

#include <PgsExt\GirderLabel.h>
#include <afxwin.h>

#include <EAF\EAFUtilities.h> // so all files have EAFGetBroker
#include <EAF\EAFResources.h> // so all files have EAF resource identifiers
#include <EAF\EAFHints.h>     // so all files have EAF Doc/View hints

#include <afxdlgs.h>

#include "MakePgz\zip.h"
#include "MakePgz\unzip.h"

using namespace ATL;
