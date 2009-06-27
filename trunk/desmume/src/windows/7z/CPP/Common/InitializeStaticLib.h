// Common/InitializeStaticLib.h
//
// it's necessary to include this from one cpp file in order to use 7zip as a static library,
// otherwise the linker will optimize away some important internals of 7zip.

#ifndef __COMMON_INITIALIZESTATICLIB_H
#define __COMMON_INITIALIZESTATICLIB_H

#define FORCE_REF(dec, var) extern dec var; void* var##ref = (void*)&var;

#include "../7zip/Common/DeclareCodecs.h"
#include "../7zip/Common/DeclareArcs.h"

FORCE_REF(struct CCRCTableInit, g_CRCTableInit)


// these don't seem to be necessary with my compiler,
// but they're here in case a different compiler more aggressively strips out unreferenced code
FORCE_REF(class CBZip2CrcTableInit, g_BZip2CrcTableInit)
namespace NCrypto { struct CAesTabInit; FORCE_REF(CAesTabInit, g_AesTabInit) }
namespace NBitl { struct CInverterTableInitializer; FORCE_REF(CInverterTableInitializer, g_InverterTableInitializer) }
namespace NCompress { namespace NRar3 { class CDistInit; FORCE_REF(CDistInit, g_DistInit) }}
namespace NArchive { namespace NLzh { class CCRCTableInit; FORCE_REF(CCRCTableInit, g_CRCTableInit) }}
namespace NArchive { namespace N7z { class SignatureInitializer; FORCE_REF(SignatureInitializer, g_SignatureInitializer) }}
namespace NArchive{ namespace NRar{ namespace NHeader{ class CMarkerInitializer; FORCE_REF(CMarkerInitializer, g_MarkerInitializer) }}}
namespace NArchive { namespace NZip { namespace NSignature{ class CMarkersInitializer; FORCE_REF(CMarkersInitializer, g_MarkerInitializer) }}}


#endif
