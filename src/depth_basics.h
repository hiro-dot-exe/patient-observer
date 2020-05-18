#ifndef KINECT_PATIENTS_OBSERVER_DEPTH_BASICS_H_
#define KINECT_PATIENTS_OBSERVER_DEPTH_BASICS_H_

class ImageRenderer;
class Observer;

class DepthBasics {
public:
  DepthBasics();
  ~DepthBasics();

  /// <summary>
  /// Handles window messages, passes most to the class instance to handle.
  /// </summary>
  /// <param name="hWnd">window message is for</param>
  /// <param name="uMsg">message</param>
  /// <param name="wParam">message data</param>
  /// <param name="lParam">additional message data</param>
  /// <returns>result of message processing</returns>
  static LRESULT CALLBACK MessageRouter(HWND hWnd, UINT uMsg,
                                        WPARAM wParam, LPARAM lParam);
  /// <summary>
  /// Handle windows messages for a class instance.
  /// </summary>
  /// <param name="hWnd">window message is for</param>
  /// <param name="uMsg">message</param>
  /// <param name="wParam">message data</param>
  /// <param name="lParam">additional message data</param>
  /// <returns>result of message processing</returns>
  LRESULT CALLBACK DlgProc(HWND hWnd, UINT uMsg,
                           WPARAM wParam, LPARAM lParam);
  /// <summary>
  /// Creates the main window and begins processing.
  /// </summary>
  /// <param name="hInstance"></param>
  /// <param name="nCmdShow"></param>
  int Run(HINSTANCE hInstance, int nCmdShow);

private:
  static const int cWindowWidth;   // [DLU]
  static const int cWindowHeight;  // [DLU]

  /// <summary>
  /// Main processing function.
  /// </summary>
  void Update();
  /// <summary>
  /// Initializes the default Kinect sensor.
  /// </summary>
  /// <returns>S_OK on success, otherwise failure code</returns>
  HRESULT InitializeDefaultSensor();
  /// <summary>
  /// Handle new depth data.
  /// <param name="pBuffer">pointer to frame data</param>
  /// </summary>
  void ProcessDepth(const UINT16 *pBuffer);
  /// <summary>
  /// Set the status bar message.
  /// </summary>
  /// <param name="szMessage">message to display</param>
  /// <param name="nShowTimeMsec">time in milliseconds for which to
  /// ignore future status messages</param>
  /// <param name="bForce">force status update</param>
  bool SetStatusMessage(_In_z_ WCHAR *szMessage,
                        DWORD nShowTimeMsec, bool bForce);

  HWND m_hWnd;
  INT64 m_nLastCounter;
  double m_fFreq;
  INT64 m_nNextStatusTime;
  DWORD m_nFramesSinceUpdate;
  // Current Kinect.
  IKinectSensor *m_pKinectSensor;
  // Depth reader.
  IDepthFrameReader *m_pDepthFrameReader;
  // Direct2D.
  ImageRenderer *m_pDrawDepth;
  ID2D1Factory *m_pD2DFactory;
  RGBQUAD *m_pDepthRGBX;
  // Observer.
  Observer *m_pObserver;
};

#endif  // KINECT_PATIENTS_OBSERVER_DEPTH_BASICS_H_