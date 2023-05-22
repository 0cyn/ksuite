#pragma once

#include <binaryninjacore.h>

#ifdef __cplusplus
extern "C"
{
#endif
// FIXME
#define KSUITE_LIBRARY

#ifdef __GNUC__
#ifdef KSUITE_LIBRARY
#define KSUITE_FFI_API __attribute__((visibility("default")))
#else  // KSUITE_LIBRARY
#define KSUITE_FFI_API
#endif  // KSUITE_LIBRARY
#else       // __GNUC__
#ifdef _MSC_VER
		#ifndef DEMO_VERSION
			#ifdef KSUITE_LIBRARY
				#define KSUITE_FFI_API __declspec(dllexport)
			#else  // KSUITE_LIBRARY
				#define KSUITE_FFI_API __declspec(dllimport)
			#endif  // KSUITE_LIBRARY
		#else
			#define KSUITE_FFI_API
		#endif
	#else  // _MSC_VER
		#define KSUITE_FFI_API
	#endif  // _MSC_VER
#endif      // __GNUC__C

char** KSUITE_FFI_API BNDSCViewGetInstallNames(BNBinaryView *view, size_t* count);
bool KSUITE_FFI_API BNDSCViewLoadImageWithInstallName(BNBinaryView* view, char* name);

};

