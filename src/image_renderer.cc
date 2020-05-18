#include "image_renderer.h"
#include "observer.h"

const FLOAT ImageRenderer::cStrokeWidth = 1.5F;

ImageRenderer::ImageRenderer(Observer *observer)
    : m_hWnd(0),
      m_sourceWidth(0),
      m_sourceHeight(0),
      m_sourceStride(0),
      m_pD2DFactory(NULL),
      m_pRenderTarget(NULL),
      m_pBitmap(NULL),
      m_pDWriteFactory(NULL),
      m_pTextFormat(NULL),
      m_pGreenBrush(NULL),
      m_pLightGreenBrush(NULL),
      m_pOrangeBrush(NULL),
      m_pObserver(observer) {
}

ImageRenderer::~ImageRenderer() {
  DiscardResources();
  SafeRelease(m_pD2DFactory);
  SafeRelease(m_pDWriteFactory);
  SafeRelease(m_pTextFormat);
}

HRESULT ImageRenderer::Initialize(HWND hWnd, ID2D1Factory *pD2DFactory,
                                  int sourceWidth, int sourceHeight,
                                  int sourceStride) {
  if (NULL == pD2DFactory)
    return E_INVALIDARG;

  m_hWnd = hWnd;

  // One factory for the entire application so save a pointer here.
  m_pD2DFactory = pD2DFactory;
  m_pD2DFactory->AddRef();

  // Get the frame size.
  m_sourceWidth = sourceWidth;
  m_sourceHeight = sourceHeight;
  m_sourceStride = sourceStride;

  // Create a DirectWrite factory.
  HRESULT hr = DWriteCreateFactory(
      DWRITE_FACTORY_TYPE_SHARED,
      __uuidof(m_pDWriteFactory),
      reinterpret_cast<IUnknown **>(&m_pDWriteFactory));
  // Create a DirectWrite text format object.
  if (SUCCEEDED(hr)) {
    hr = m_pDWriteFactory->CreateTextFormat(
        L"",    // System default font.
        NULL,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        30.0F,  // Font size.
        L"",    // Locale.
        &m_pTextFormat);
  }

  return hr;
}

HRESULT ImageRenderer::Draw(BYTE *pImage, unsigned long cbImage) {
  // Incorrectly sized image data passed in.
  if (cbImage < (m_sourceHeight - 1) * m_sourceStride + m_sourceWidth * 4)
    return E_INVALIDARG;

  // Create the resources for this draw device.
  // They will be recreated if previously lost.
  HRESULT hr = EnsureResources();
  if (FAILED(hr))
    return hr;

  // Copy the image that was passed in into the direct2d bitmap.
  hr = m_pBitmap->CopyFromMemory(NULL, pImage, m_sourceStride);
  if (FAILED(hr))
    return hr;

  // Draw.
  m_pRenderTarget->BeginDraw();
  m_pRenderTarget->DrawBitmap(m_pBitmap);
  DrawBedArea();
  DrawPatientArea();
  DrawShoulderPosition();
  DrawHeadPosition();
  DrawPatientState();
  hr = m_pRenderTarget->EndDraw();

  // Device lost, need to recreate the render target,
  // dispose it now and retry drawing.
  if (hr == D2DERR_RECREATE_TARGET) {
    hr = S_OK;
    DiscardResources();
  }

  return hr;
}

HRESULT ImageRenderer::EnsureResources() {
  HRESULT hr = S_OK;
  if (NULL != m_pRenderTarget)
    return hr;

  D2D1_SIZE_U size = D2D1::SizeU(m_sourceWidth, m_sourceHeight);
  D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties();
  rtProps.pixelFormat = D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                                          D2D1_ALPHA_MODE_IGNORE);
  rtProps.usage = D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE;

  // Create a hWnd render target, in order to render
  // to the window set in initialize.
  hr = m_pD2DFactory->CreateHwndRenderTarget(
      rtProps,
      D2D1::HwndRenderTargetProperties(m_hWnd, size),
      &m_pRenderTarget);
  if (FAILED(hr))
    return hr;

  // Create a bitmap that we can copy image data into
  // and then render to the target.
  hr = m_pRenderTarget->CreateBitmap(
      size,
      D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                                               D2D1_ALPHA_MODE_IGNORE)),
      &m_pBitmap);
  if (FAILED(hr)) {
    SafeRelease(m_pRenderTarget);
    return hr;
  }

  // Create brushes.
  // http://colorhunt.co/c/41371
  hr = m_pRenderTarget->CreateSolidColorBrush(
      D2D1::ColorF(0.4F, 1.0F, 0.3F, 0.9F), &m_pGreenBrush);
  if (FAILED(hr)) {
    SafeRelease(m_pRenderTarget);
    SafeRelease(m_pBitmap);
    return hr;
  }
  hr = m_pRenderTarget->CreateSolidColorBrush(
      D2D1::ColorF(0.9F, 1.0F, 0.4F, 0.9F), &m_pLightGreenBrush);
  if (FAILED(hr)) {
    SafeRelease(m_pRenderTarget);
    SafeRelease(m_pBitmap);
    SafeRelease(m_pGreenBrush);
    return hr;
  }
  hr = m_pRenderTarget->CreateSolidColorBrush(
      D2D1::ColorF(1.0F, 0.3F, 0.1F, 0.9F), &m_pOrangeBrush);
  if (FAILED(hr)) {
    SafeRelease(m_pRenderTarget);
    SafeRelease(m_pBitmap);
    SafeRelease(m_pGreenBrush);
    SafeRelease(m_pLightGreenBrush);
    return hr;
  }

  return hr;
}

void ImageRenderer::DiscardResources() {
  SafeRelease(m_pRenderTarget);
  SafeRelease(m_pBitmap);
  SafeRelease(m_pGreenBrush);
  SafeRelease(m_pLightGreenBrush);
  SafeRelease(m_pOrangeBrush);
}

void ImageRenderer::DrawPatientState() {
  const Observer::PatientState cState = m_pObserver->GetState();
  const WCHAR *cStatesName =
      cState == Observer::eNone          ? L"None" :
      cState == Observer::eStanding      ? L"Standing" :
      cState == Observer::eSittingOnEdge ? L"Sitting on Edge" :
      cState == Observer::eSitting       ? L"Sitting" :
      cState == Observer::eLying         ? L"Lying" :
      cState == Observer::eLyingOnSide   ? L"Lying on Side" :
                                           L"Error";
  m_pRenderTarget->DrawText(
      cStatesName,
      wcslen(cStatesName),
      m_pTextFormat,
      D2D1::RectF(15.0F, 5.0F,
                  static_cast<FLOAT>(m_sourceWidth),
                  static_cast<FLOAT>(m_sourceHeight)),
      m_pLightGreenBrush);
}

void ImageRenderer::DrawHeadPosition() {
  int headPosition = m_pObserver->GetHeadPosition();
  if (Observer::eUnknown != headPosition) {
    int relativeHeadSize = m_pObserver->GetRelativeHeadSize();
    D2D1_ELLIPSE ellipse = D2D1::Ellipse(
        D2D1::Point2F(headPosition % m_sourceWidth * 1.0F,
                      headPosition / m_sourceWidth * 1.0F),
        relativeHeadSize / 2 * 1.0F,   // Radius.
        relativeHeadSize / 2 * 1.0F);  // Radius.
    m_pRenderTarget->DrawEllipse(ellipse, m_pOrangeBrush, cStrokeWidth);
  }
}

void ImageRenderer::DrawShoulderPosition() {
  int shoulderPosition = m_pObserver->GetShoulderPosition();
  if (Observer::eUnknown != shoulderPosition) {
    int relativeHeadSize = m_pObserver->GetRelativeHeadSize();
    D2D1_ELLIPSE ellipse = D2D1::Ellipse(
        D2D1::Point2F(shoulderPosition % m_sourceWidth * 1.0F,
                      shoulderPosition / m_sourceWidth * 1.0F),
        3,   // Radius.
        3);  // Radius.
    m_pRenderTarget->DrawEllipse(ellipse, m_pOrangeBrush, cStrokeWidth);
  }
}

void ImageRenderer::DrawPatientArea() {
  std::vector<int> corners = m_pObserver->GetPatientCorners();
  for (int i = 0; i < static_cast<int>(corners.size()); ++i) {
    D2D1_POINT_2F source = {
        corners[i] % m_sourceWidth * 1.0F,
        corners[i] / m_sourceWidth * 1.0F};
    D2D1_POINT_2F dest = {
        corners[(i + 1) % corners.size()] % m_sourceWidth * 1.0F,
        corners[(i + 1) % corners.size()] / m_sourceWidth * 1.0F};
    m_pRenderTarget->DrawLine(source, dest, m_pLightGreenBrush, cStrokeWidth);
  }
}

void ImageRenderer::DrawBedArea() {
  std::vector<int> corners = m_pObserver->GetBedCorners();
  for (int i = 0; i < static_cast<int>(corners.size()); ++i) {
    D2D1_POINT_2F source = {
        corners[i] % m_sourceWidth * 1.0F,
        corners[i] / m_sourceWidth * 1.0F};
    D2D1_POINT_2F dest = {
        corners[(i + 1) % corners.size()] % m_sourceWidth * 1.0F,
        corners[(i + 1) % corners.size()] / m_sourceWidth * 1.0F};
    m_pRenderTarget->DrawLine(source, dest, m_pGreenBrush, cStrokeWidth);
  }
}

void ImageRenderer::DrawLevel() {
  // Center.
  D2D1_ELLIPSE ellipse = D2D1::Ellipse(
      D2D1::Point2F(m_sourceWidth * 1.0F - 35, 35),
      0.5F,   // Radius.
      0.5F);  // Radius.
  m_pRenderTarget->DrawEllipse(ellipse, m_pGreenBrush, cStrokeWidth);

  // Border.
  ellipse = D2D1::Ellipse(
      D2D1::Point2F(m_sourceWidth * 1.0F - 35, 35),
      30.0F,   // Radius.
      30.0F);  // Radius.
  m_pRenderTarget->DrawEllipse(ellipse, m_pGreenBrush, cStrokeWidth);

  // Current bed normal.
  Vector bedNormal = m_pObserver->GetBedNormal();
  ellipse = D2D1::Ellipse(
      D2D1::Point2F(static_cast<FLOAT>(m_sourceWidth - 35 + bedNormal.x * 30),
                    static_cast<FLOAT>(35 + bedNormal.y * 30)),
      1.0F,   // Radius.
      1.0F);  // Radius.
  m_pRenderTarget->DrawEllipse(ellipse, m_pLightGreenBrush, cStrokeWidth);
}

void ImageRenderer::Graph() {
  GraphPatientState();
  GraphProbabilityPatientOnBed();
}

void ImageRenderer::GraphPatientState() {
  const std::vector<Observer::Log>& cMovement =
      m_pObserver->GetLog();
  for (int i = 0; i < static_cast<int>(cMovement.size()) - 1; ++i) {
    D2D1_POINT_2F source, dest;
    source.x = 1 * i * 1.0F;
    dest.x = 1 * (i + 1) * 1.0F;

    // Patient's state.
    source.y = static_cast<FLOAT>(
        m_sourceHeight - 7 * cMovement[i].state);
    dest.y = static_cast<FLOAT>(
        m_sourceHeight - 7 * cMovement[i + 1].state);
    m_pRenderTarget->DrawLine(source, dest, m_pLightGreenBrush, cStrokeWidth);
  }
}

void ImageRenderer::GraphProbabilityPatientOnBed() {
  const std::vector<Observer::Log>& cMovement =
      m_pObserver->GetLog();
  for (int i = 0; i < static_cast<int>(cMovement.size()) - 1; ++i) {
    D2D1_POINT_2F source, dest;
    source.x = 4 * i * 1.0F + 105;
    dest.x = 4 * (i + 1) * 1.0F + 105;

    // Borders of probability that a patient is on bed.
    source.y = dest.y = static_cast<FLOAT>(
        m_sourceHeight - 35 * Observer::cBorderProbabilityStanding);
    m_pRenderTarget->DrawLine(source, dest, m_pGreenBrush, cStrokeWidth);
    source.y = dest.y = static_cast<FLOAT>(
        m_sourceHeight - 35 * Observer::cBorderProbabilitySittingOnEdge);
    m_pRenderTarget->DrawLine(source, dest, m_pGreenBrush, cStrokeWidth);
    source.y = dest.y = static_cast<FLOAT>(
        m_sourceHeight - 35 * 1.0F);
    m_pRenderTarget->DrawLine(source, dest, m_pGreenBrush, cStrokeWidth);

    // Probability that a patient is on bed.
    source.y = static_cast<FLOAT>(
        m_sourceHeight - 35 * cMovement[i].probabilityPatientOnBed);
    dest.y = static_cast<FLOAT>(
        m_sourceHeight - 35 * cMovement[i + 1].probabilityPatientOnBed);
    m_pRenderTarget->DrawLine(source, dest, m_pLightGreenBrush, cStrokeWidth);
  }
}