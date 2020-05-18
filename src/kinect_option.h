#ifndef KINECT_PATIENTS_OBSERVER_KINECT_OPTION_H_
#define KINECT_PATIENTS_OBSERVER_KINECT_OPTION_H_

struct Vector;

struct KinectOption {
  // From the Kinect v2 specifications.
  // http://zugara.com/how-does-the-kinect-2-compare-to-the-kinect-1
  static const int cDepthBufferWidth = 512;    // [px]
  static const int cDepthBufferHeight = 424;   // [px]
  static const int cDepthBufferSize = cDepthBufferWidth * cDepthBufferHeight;
  static const int cHorizontalFieldView = 70;  // [degree]
  static const int cVerticalFieldView = 60;    // [degree]
  static const int cDepthBufferXCenter = cDepthBufferWidth / 2;
  static const int cDepthBufferYCenter = cDepthBufferHeight / 2;
  static const int cScreenCornersId[4];
  static const double cFX;
  static const double cFY;
  static const double cCoefficientMmIntoPx;

  static int GetX(int id);
  static int GetY(int id);
  static int GetId(int x, int y);
  static int GetNextId(int src, int dx, int dy);
  static int GetNextX(int src, int dx);
  static int GetNextY(int src, int dy);
  static Vector ConvertIntoWorldCoordinates(int id, UINT16 depth);
  static double CalculateScreenDistance(int id1, int id2);
  static double CalculateWorldDistance(int id1, UINT16 depth1,
                                       int id2, UINT16 depth2);
  static double ConvertIntoScreenLength(double length, UINT16 depth);
  static Vector CalculateNormal(int id, const UINT16 *depth);
  static bool IsAvailableDepth(UINT16 depth);
  static bool IsXInRange(int x);
  static bool IsYInRange(int y);
  static bool IsLeftSide(int id);
  static bool IsRightSide(int id);
};

#endif  // KINECT_PATIENTS_OBSERVER_KINECT_OPTION_H_