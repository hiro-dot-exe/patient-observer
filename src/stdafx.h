#ifndef KINECT_PATIENTS_OBSERVER_STDAFX_H_
#define KINECT_PATIENTS_OBSERVER_STDAFX_H_

// Exclude rarely-used stuff from Windows headers.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>   // Windows header files.
#include <windowsx.h>  // Windows header files.
#include <Shlobj.h>    // Windows header files.
#include <d2d1.h>      // Direct2D header files.
#include <Kinect.h>    // Kinect header files.
#include <dwrite.h>    // Text and font header files.

#pragma comment (lib, "d2d1.lib")

#ifdef _UNICODE
#if defined _M_IX86
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
#endif

// Safe release for interfaces.
template<class Interface>
inline void SafeRelease(Interface *&pInterfaceToRelease) {
  if (pInterfaceToRelease != NULL) {
    pInterfaceToRelease->Release();
    pInterfaceToRelease = NULL;
  }
}

#endif  // KINECT_PATIENTS_OBSERVER_STDAFX_H_