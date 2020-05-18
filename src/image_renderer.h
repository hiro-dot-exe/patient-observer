#ifndef KINECT_PATIENTS_OBSERVER_IMAGE_RENDERER_H_
#define KINECT_PATIENTS_OBSERVER_IMAGE_RENDERER_H_

class Observer;

/// <summary>
/// Manages the drawing of image data.
/// </summary>
class ImageRenderer {
public:
  explicit ImageRenderer(Observer *observer);
  virtual ~ImageRenderer();

  /// <summary>
  /// Set the window to draw to as well as the video format.
  /// Implied bits per pixel is 32.
  /// </summary>
  /// <param name="hWnd">window to draw to</param>
  /// <param name="pD2DFactory">already created D2D factory object</param>
  /// <param name="sourceWidth">width of image data to be drawn</param>
  /// <param name="sourceHeight">height of image data to be drawn</param>
  /// <param name="sourceStride">length (in bytes) of a single scanline</param>
  /// <returns>indicates success or failure</returns>
  HRESULT Initialize(HWND hwnd, ID2D1Factory *pD2DFactory,
                     int sourceWidth, int sourceHeight,
                     int sourceStride);
  /// <summary>
  /// Draws a 32 bit per pixel image of previously specified width, height,
  /// and stride to the associated hwnd.
  /// </summary>
  /// <param name="pImage">image data in RGBX format</param>
  /// <param name="cbImage">size of image data in bytes</param>
  /// <returns>indicates success or failure</returns>
  HRESULT Draw(BYTE *pImage, unsigned long cbImage);

private:
  static const FLOAT cStrokeWidth;

  /// <summary>
  /// Ensure necessary Direct2d resources are created.
  /// </summary>
  /// <returns>indicates success or failure</returns>
  HRESULT EnsureResources();
  /// <summary>
  /// Dispose of Direct2d resources.
  /// </summary>
  void DiscardResources();
  void DrawPatientState();
  void DrawHeadPosition();
  void DrawShoulderPosition();
  void DrawPatientArea();
  void DrawBedArea();
  void DrawLevel();
  void Graph();
  void GraphPatientState();
  void GraphProbabilityPatientOnBed();

  HWND m_hWnd;
  // Format information.
  UINT m_sourceWidth;
  UINT m_sourceHeight;
  LONG m_sourceStride;
  // Direct2D.
  ID2D1Factory *m_pD2DFactory;
  ID2D1HwndRenderTarget *m_pRenderTarget;
  ID2D1Bitmap *m_pBitmap;
  IDWriteFactory *m_pDWriteFactory;
  IDWriteTextFormat *m_pTextFormat;
  ID2D1SolidColorBrush *m_pGreenBrush;
  ID2D1SolidColorBrush *m_pLightGreenBrush;
  ID2D1SolidColorBrush *m_pOrangeBrush;
  // Observer.
  Observer *m_pObserver;
};

#endif  // KINECT_PATIENTS_OBSERVER_IMAGE_RENDERER_H_