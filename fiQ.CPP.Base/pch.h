// pch.h: This is a precompiled header file.
// Files listed below are compiled only once, improving build performance for future builds.
// This also affects IntelliSense performance, including code completion and many code browsing features.
// However, files listed here are ALL re-compiled if any one of them is updated between builds.
// Do not add files here that you will be updating frequently as this negates the performance advantage.

#ifndef PCH_H
#define PCH_H

#define FD_SETSIZE 255		// Redefine FD_SETSIZE to allow up to 255 sockets per select

#ifndef WINVER				// Allow use of features specific to Windows 2008 R2/Win7 or later.
#define WINVER 0x0601		// Change this to the appropriate value to target other versions of Windows.
#endif
#ifndef _WIN32_WINNT		// Allow use of features specific to Windows 2008 R2/Win7 or later.
#define _WIN32_WINNT 0x0601	// Change this to the appropriate value to target other versions of Windows.
#endif						

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#endif
#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN			// Exclude rarely-used stuff from Windows headers
#endif

//#ifndef _SQLNCLI_OLEDB_IGNORE_DEPRECATION_WARNING_
//#define _SQLNCLI_OLEDB_IGNORE_DEPRECATION_WARNING_	// Silence deprecation of SQL Native Client (to be replaced)
//#endif
//
//#ifndef DBINITCONSTANTS
//#define DBINITCONSTANTS			// Used by OLEDB
//#endif

#include <memory> // Must be before stdlib to support memory functions
#ifdef _DEBUG // In debug mode, ensure that CRT debugger enables memory leak tracking:
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#else
#include <stdlib.h>
#endif

#include <stdio.h>

#include <algorithm>
//#include <cctype>
//#include <comdef.h>
//#include <direct.h>
//#include <exception>
//#include <fcntl.h>
//#include "io.h"
//#include <inttypes.h>
//#include <math.h>
//#include <objbase.h>
#include <locale>
#include <codecvt>

// STL containers:
#include <deque>
#include <map>
#include <set>
#include <string>
//#include <tuple>
#include <vector>

//#include <sys/stat.h>
//#include <tchar.h>
#include <time.h>
//#include <sys/timeb.h>

#include "Tools/gsl.h" // Include local version - may use legit one someday

#include <windows.h>

#endif //PCH_H
