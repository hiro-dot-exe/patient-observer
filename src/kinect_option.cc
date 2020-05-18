#include "kinect_option.h"
#define _USE_MATH_DEFINES  // M_PI
#include <math.h>          // sqrt(), tan()
#include <cfloat>          // DBL_MAX
#include "vector.h"

const int KinectOption::cScreenCornersId[4] = {
    0,                                      // Upper left.
    cDepthBufferWidth - 1,                  // Upper right.
    cDepthBufferSize - 1,                   // Lower right.
    cDepthBufferSize - cDepthBufferWidth};  // Lower left.

// To convert viewport coordinates into world them.
// http://stackoverflow.com/questions/17832238/
const double KinectOption::cFX = 1.0 * cDepthBufferXCenter /
    tan(0.5 * cHorizontalFieldView * M_PI / 180);
const double KinectOption::cFY = 1.0 * cDepthBufferYCenter /
    tan(0.5 * cVerticalFieldView * M_PI / 180);
const double KinectOption::cCoefficientMmIntoPx = sqrt(cFX * cFY);

int KinectOption::GetX(int id) {
  return id % cDepthBufferWidth;
}

int KinectOption::GetY(int id) {
  return id / cDepthBufferWidth;
}

int KinectOption::GetId(int x, int y) {
  return x + y * cDepthBufferWidth;
}

int KinectOption::GetNextId(int src, int dx, int dy) {
  int xDest = GetX(src) + dx;
  int yDest = GetY(src) + dy;
  
  // Limit the coordinates to inside of the screen.
  xDest = max(0, min(cDepthBufferWidth - 1, xDest));
  yDest = max(0, min(cDepthBufferHeight - 1, yDest));

  return GetId(xDest, yDest);
}

int KinectOption::GetNextX(int src, int dx) {
  int dy = 0;
  return GetX(GetNextId(src, dx, dy));
}

int KinectOption::GetNextY(int src, int dy) {
  int dx = 0;
  return GetY(GetNextId(src, dx, dy));
}

Vector KinectOption::ConvertIntoWorldCoordinates(int id, UINT16 depth) {
  // Calculate viewport coodinates, "xV" and "yV",
  // transfered the origin to the center of the screen.
  int xV = GetX(id) - cDepthBufferXCenter;
  int yV = GetY(id) - cDepthBufferYCenter;

  // Convert viewport coordinates into world them.
  // http://stackoverflow.com/questions/17832238/
  Vector coordinates;
  coordinates.x = 1.0 * depth * xV / cFX;
  coordinates.y = 1.0 * depth * yV / cFY;
  coordinates.z = depth;
  return coordinates;
}

double KinectOption::CalculateScreenDistance(int id1, int id2) {
  Vector v1, v2;
  v1.x = GetX(id1);
  v1.y = GetY(id1);
  v2.x = GetX(id2);
  v2.y = GetY(id2);
  return (v1.Subtract(v2)).Length();
}

double KinectOption::CalculateWorldDistance(int id1, UINT16 depth1,
                                            int id2, UINT16 depth2) {
  Vector v1 = ConvertIntoWorldCoordinates(id1, depth1);
  Vector v2 = ConvertIntoWorldCoordinates(id2, depth2);
  return (v1.Subtract(v2)).Length();
}

double KinectOption::ConvertIntoScreenLength(double length, UINT16 depth) {
  double screenLength = 0.0;
  if (0 < depth)
    screenLength = cCoefficientMmIntoPx * length / depth;
  return screenLength;
}

Vector KinectOption::CalculateNormal(int id, const UINT16 *depth) {
  int idRight = GetNextId(id, 1, 0);
  int idBottom = GetNextId(id, 0, 1);

  Vector normal;
  if (IsAvailableDepth(depth[id]) &&
      IsAvailableDepth(depth[idRight]) &&
      IsAvailableDepth(depth[idBottom])) {
    Vector v1 = ConvertIntoWorldCoordinates(id, depth[id]);
    Vector v2 = ConvertIntoWorldCoordinates(idRight, depth[idRight]);
    Vector v3 = ConvertIntoWorldCoordinates(idBottom, depth[idBottom]);
    normal = v3.Normal(v1, v2);
  }

  return normal;
}

bool KinectOption::IsAvailableDepth(UINT16 depth) {
  return depth != 0;
}

bool KinectOption::IsXInRange(int x) {
  return 0 <= x && x < cDepthBufferWidth;
}

bool KinectOption::IsYInRange(int y) {
  return 0 <= y && y < cDepthBufferHeight;
}

bool KinectOption::IsLeftSide(int id) {
  return GetX(id) < cDepthBufferXCenter;
}

bool KinectOption::IsRightSide(int id) {
  return !IsLeftSide(id);
}