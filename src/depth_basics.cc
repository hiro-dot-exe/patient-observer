#include "depth_basics.h"
#include <strsafe.h>
#include "resource.h"
#include "image_renderer.h"
#include "observer.h"
#include "kinect_option.h"

const int DepthBasics::cWindowWidth = 383;
const int DepthBasics::cWindowHeight = 318;

/// <summary>
/// Entry point for the application.
/// </summary>
/// <param name="hInstance">handle to the application instance</param>
/// <param name="hPrevInstance">always 0</param>
/// <param name="lpCmdLine">command line arguments</param>
/// <param name="nCmdShow">whether to display minimized, maximized,
/// or normally</param>
/// <returns>status</returns>
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                      _In_opt_ HINSTANCE hPrevInstance,
                      _In_ LPWSTR lpCmdLine,
                      _In_ int nShowCmd) {
  UNREFERENCED_PARAMETER(hPrevInstance);
  UNREFERENCED_PARAMETER(lpCmdLine);

  DepthBasics application;
  application.Run(hInstance, nShowCmd);
}

DepthBasics::DepthBasics()
    : m_hWnd(NULL),
      m_nLastCounter(0),
      m_nFramesSinceUpdate(0),
      m_fFreq(0),
      m_nNextStatusTime(0LL),
      m_pKinectSensor(NULL),
      m_pDepthFrameReader(NULL),
      m_pD2DFactory(NULL),
      m_pDrawDepth(NULL),
      m_pDepthRGBX(NULL),
      m_pObserver(NULL) {
  LARGE_INTEGER qpf = {0};
  if (QueryPerformanceFrequency(&qpf))
    m_fFreq = double(qpf.QuadPart);

  // Create heap storage for depth pixel data in RGBX format.
  m_pDepthRGBX = new RGBQUAD[KinectOption::cDepthBufferSize];

  // Create an instance of Observer.
  m_pObserver = new Observer();
}

DepthBasics::~DepthBasics() {
  // Clean up Direct2D renderer.
  if (m_pDrawDepth) {
    delete m_pDrawDepth;
    m_pDrawDepth = NULL;
  }

  if (m_pDepthRGBX) {
    delete[] m_pDepthRGBX;
    m_pDepthRGBX = NULL;
  }

  if (m_pObserver) {
    delete m_pObserver;
    m_pObserver = NULL;
  }

  // Clean up Direct2D.
  SafeRelease(m_pD2DFactory);

  // Done with depth frame reader.
  SafeRelease(m_pDepthFrameReader);

  // Close the Kinect Sensor.
  if (m_pKinectSensor)
    m_pKinectSensor->Close();

  SafeRelease(m_pKinectSensor);
}

LRESULT CALLBACK DepthBasics::MessageRouter(HWND hWnd, UINT uMsg,
                                            WPARAM wParam, LPARAM lParam) {
  DepthBasics *pThis = NULL;

  if (WM_INITDIALOG == uMsg) {
    pThis = reinterpret_cast<DepthBasics *>(lParam);
    SetWindowLongPtr(hWnd, GWLP_USERDATA,
                     reinterpret_cast<LONG_PTR>(pThis));
  } else {
    pThis = reinterpret_cast<DepthBasics *>(
        ::GetWindowLongPtr(hWnd, GWLP_USERDATA));
  }

  if (pThis)
    return pThis->DlgProc(hWnd, uMsg, wParam, lParam);

  return 0;
}

LRESULT CALLBACK DepthBasics::DlgProc(HWND hWnd, UINT message,
                                      WPARAM wParam, LPARAM lParam) {
  UNREFERENCED_PARAMETER(wParam);
  UNREFERENCED_PARAMETER(lParam);

  HRESULT hr;

  switch (message) {
  case WM_INITDIALOG:
    // Bind application window handle.
    m_hWnd = hWnd;

    // Initialize Direct2D.
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pD2DFactory);

    // Create and initialize a new Direct2D image renderer.
    // (take a look at image_renderer.h)
    // We'll use this to draw the data
    // we receive from the Kinect to the screen.
    m_pDrawDepth = new ImageRenderer(m_pObserver);
    hr = m_pDrawDepth->Initialize(
        GetDlgItem(m_hWnd, IDC_VIDEOVIEW),
        m_pD2DFactory,
        KinectOption::cDepthBufferWidth,
        KinectOption::cDepthBufferHeight,
        KinectOption::cDepthBufferWidth * sizeof(RGBQUAD));
    if (FAILED(hr)) {
      SetStatusMessage(L"Failed to initialize the Direct2D draw device.",
                       10000, true);
    }

    // Get and initialize the default Kinect sensor.
    InitializeDefaultSensor();

    break;

  case WM_CLOSE:
    // If the titlebar X is clicked, destroy app.
    DestroyWindow(hWnd);
    break;

  case WM_DESTROY:
    // Quit the main message pump.
    PostQuitMessage(0);
    break;

  case WM_RBUTTONDOWN:
    // Initialize the observer.
    m_pObserver->InitializeOnlyBackgroundNext();
    break;

  case WM_RBUTTONDBLCLK:
    // Initialize the observer.
    m_pObserver->InitializeAllNext();
    break;

  case WM_LBUTTONDOWN:
  case WM_LBUTTONDBLCLK:
    // NOTE: You can get clicked coordinates with uncommenting below.
    //// Get clicked position.
    //int x = LOWORD(lParam);
    //int y = HIWORD(lParam);

    //// Convert DLU into px.
    //// https://support.microsoft.com/ja-jp/kb/145994
    //tagRECT rc = {0};
    //rc.right = cWindowWidth * 1L;
    //rc.bottom = cWindowHeight * 1L;
    //MapDialogRect(hWnd, &rc);
    //x = x * KinectOption::cDepthBufferWidth / rc.right;
    //y = y * KinectOption::cDepthBufferHeight / rc.bottom;

    //// To prevent from accessing out of memory.
    //x = min(KinectOption::cDepthBufferWidth - 1, max(0, x));
    //y = min(KinectOption::cDepthBufferHeight - 1, max(0, y));

    break;
  }

  return false;
}

int DepthBasics::Run(HINSTANCE hInstance, int nCmdShow) {
  MSG msg = {0};
  WNDCLASS wc;

  // Dialog custom window class.
  ZeroMemory(&wc, sizeof(wc));
  wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
  wc.cbWndExtra = DLGWINDOWEXTRA;
  wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
  wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCE(IDI_APP));
  wc.lpfnWndProc = DefDlgProcW;
  wc.lpszClassName = L"DepthBasicsAppDlgWndClass";

  if (!RegisterClassW(&wc))
    return 0;

  // Create main application window.
  HWND hWndApp = CreateDialogParamW(
      NULL,
      MAKEINTRESOURCE(IDD_APP),
      NULL,
      (DLGPROC)DepthBasics::MessageRouter,
      reinterpret_cast<LPARAM>(this));

  // Show window.
  ShowWindow(hWndApp, nCmdShow);

  // Main message loop.
  while (WM_QUIT != msg.message) {
    Update();

    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
      // If a dialog message will be taken care of by the dialog proc.
      if (hWndApp && IsDialogMessageW(hWndApp, &msg))
        continue;

      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
  }

  return static_cast<int>(msg.wParam);
}

void DepthBasics::Update() {
  if (!m_pDepthFrameReader)
    return;

  IDepthFrame *pDepthFrame = NULL;
  HRESULT hr = m_pDepthFrameReader->AcquireLatestFrame(&pDepthFrame);

  if (SUCCEEDED(hr)) {
    UINT nBufferSize = 0;
    UINT16 *pBuffer = NULL;
    hr = pDepthFrame->AccessUnderlyingBuffer(&nBufferSize, &pBuffer);

    // Observe a patient and display its report.
    if (SUCCEEDED(hr)) {
      m_pObserver->Observe(pBuffer);
      ProcessDepth(pBuffer);
    }
  }

  SafeRelease(pDepthFrame);
}


HRESULT DepthBasics::InitializeDefaultSensor() {
  HRESULT hr = GetDefaultKinectSensor(&m_pKinectSensor);
  if (FAILED(hr))
    return hr;

  if (m_pKinectSensor) {
    // Initialize the Kinect and get the depth reader.
    IDepthFrameSource *pDepthFrameSource = NULL;
    hr = m_pKinectSensor->Open();
    if (SUCCEEDED(hr))
      hr = m_pKinectSensor->get_DepthFrameSource(&pDepthFrameSource);
    if (SUCCEEDED(hr))
      hr = pDepthFrameSource->OpenReader(&m_pDepthFrameReader);
    SafeRelease(pDepthFrameSource);
  }

  if (!m_pKinectSensor || FAILED(hr)) {
    SetStatusMessage(L"No ready Kinect found!", 10000, true);
    return E_FAIL;
  }

  return hr;
}

void DepthBasics::ProcessDepth(const UINT16 *pBuffer) {
  // Display information.
  if (m_hWnd) {
    double fps = 0.0;
    LARGE_INTEGER qpcNow = {0};
    if (m_fFreq && QueryPerformanceCounter(&qpcNow) && m_nLastCounter) {
      ++m_nFramesSinceUpdate;
      fps = m_fFreq * m_nFramesSinceUpdate /
          static_cast<double>(qpcNow.QuadPart - m_nLastCounter);
    }

    WCHAR szStatusMessage[64];
    StringCchPrintf(szStatusMessage, _countof(szStatusMessage),
                    L" FPS = %0.2f", fps);

    if (SetStatusMessage(szStatusMessage, 1000, false)) {
      m_nLastCounter = qpcNow.QuadPart;
      m_nFramesSinceUpdate = 0;
    }
  }

  // Make sure we've received valid data.
  if (m_pDepthRGBX && pBuffer) {
    for (int i = 0; i < KinectOption::cDepthBufferSize; ++i) {
      RGBQUAD *pRGBX = &m_pDepthRGBX[i];
      USHORT depth = pBuffer[i];

      // Define a color of the current pixel.
      if (m_pObserver->IsThereSomething(i)) {
        BYTE intensity = static_cast<BYTE>(128 + (depth / 3) % 128);
        pRGBX->rgbRed = pRGBX->rgbGreen = intensity;
        pRGBX->rgbBlue = intensity * 2 / 3;  // Color the pixel yellow.
      } else {
        BYTE intensity = static_cast<BYTE>(
            KinectOption::IsAvailableDepth(depth) ?
            64 + (depth % 192) : 0);
        pRGBX->rgbRed = pRGBX->rgbGreen = pRGBX->rgbBlue = intensity;
      }
    }

    // Draw the data with Direct2D.
    m_pDrawDepth->Draw(reinterpret_cast<BYTE *>(m_pDepthRGBX),
                       KinectOption::cDepthBufferSize * sizeof(RGBQUAD));
  }
}

bool DepthBasics::SetStatusMessage(_In_z_ WCHAR *szMessage,
                                   DWORD nShowTimeMsec, bool bForce) {
  INT64 now = GetTickCount64();
  bool bNeedToRefresh = bForce || m_nNextStatusTime <= now;
  if (m_hWnd && bNeedToRefresh) {
    // Program status.
    SetDlgItemText(m_hWnd, IDC_STATUS, szMessage);

    m_nNextStatusTime = now + nShowTimeMsec;

    return true;
  }
  return false;
}