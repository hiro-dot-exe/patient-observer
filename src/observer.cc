#include "observer.h"
#define _USE_MATH_DEFINES  // M_PI
#include <math.h>          // M_PI
#include <queue>
#include <map>

const double Observer::cBorderProbabilityStanding = 0.55;
const double Observer::cBorderProbabilitySittingOnEdge = 0.93;

// To get differences of depths.
const int Observer::cDepthNoiseBorder = 300;
const int Observer::cDepthOnBedNoiseBorder = 100;
// To search for a patient area.
const int Observer::cDepthNoiseBorderToSearchForPatientArea = 20;
const int Observer::cNumSkipToSearchForPatientArea = 5;
// To define a bed area.
const int Observer::cNormalsDegreeTolerance = 50;
const int Observer::cNeighborPixelsDistanceTolerance = 25;
// To find a head.
const int Observer::cHeadWidth = 140;
// To judge a patient's state.
const int Observer::cShoulderHeightBorderTurningAndLying = 200;
const int Observer::cHeadHeightBorderSittingAndLying = 550;
const int Observer::cDistanceHeadAndShoulder = 250;
const int Observer::cDistanceHeadAndHip = 750;

Observer::Observer()
    : m_headPosition(eUnknown),
      m_shoulderPosition(eUnknown),
      m_depthAtHead(eUnknown),
      m_relativeHeadSize(eUnknown),
      m_quiltHeight(0.0) {
  InitializeAllNext();
}

void Observer::Observe(const UINT16 *pBuffer) {
  // Copy the given depth buffer and interpolate depth
  // to protect the original.
  UINT16 pTempBuffer[KinectOption::cDepthBufferSize];
  memcpy(pTempBuffer, pBuffer, sizeof(pTempBuffer));
  InterpolateDepth(pTempBuffer);

  // Initialize as needed.
  if (m_initializeNext)
    return Initialize(pTempBuffer);
  
  // Get a patient's area.
  CalculateDepthDifferences(pTempBuffer);
  TrackHead(pTempBuffer);
  SearchForPatientArea(pTempBuffer);  // From the tracked head.
  UpdateBackgroundWithoutPatient(pTempBuffer);
  if (m_headPosition != eUnknown)
    CalculateDepthDifferences(pTempBuffer);  // Mask the buffer.

  // Add a new frame to draw graph within set range.
  static const int cMaxLogs = 100;
  if (cMaxLogs <= m_logs.size())
    m_logs.erase(m_logs.begin());
  m_logs.push_back({});

  JudgePatientState(pTempBuffer);
  ReduceNoiseOfPatientState();

  static const double cEpsilon = 1e-2;
  if (GetProbabilityPatientOnBed() < cEpsilon)
    GetAverageQuiltHeight(pTempBuffer);
}

void Observer::RegisterBedCorners(int x, int y) {
  // Redefine bed corners if they were registered already.
  if (IsBedAreaDefined())
    m_bedCorners.clear();

  // Calculate the normal around the clicked point.
  int clickedId = KinectOption::GetId(x, y);
  Vector tempBedNormal = GetTempBedNormal(clickedId);

  // Search for a bed area.
  bool bed[KinectOption::cDepthBufferSize] = {false};
  std::queue<int> task; task.push(clickedId);
  std::map<int, bool> isSearched;
  while (!task.empty()) {
    // Pop a point.
    int current = task.front(); task.pop();
    bed[current] = true;

    // Search for 4-neighbor of the point recursively.
    static const int cDx[4] = {0, -1, 1, 0};
    static const int cDy[4] = {-1, 0, 0, 1};
    for (int i = 0; i < 4; ++i) {
      // Skip a point checked already.
      int next = KinectOption::GetNextId(current, cDx[i], cDy[i]);
      if (0 < isSearched.count(next))
        continue;
      isSearched[next] = true;

      // Skip a lost depth.
      if (!KinectOption::IsAvailableDepth(m_pBackground[next]))
        continue;

      // Calculate the normal around the current point.
      Vector normal = KinectOption::CalculateNormal(next, m_pBackground);
      normal = normal.Normalize();

      // Calculate the angle between the normal of bed and
      // around current point.
      double angleDegree = tempBedNormal.AngleDegree(normal);

      double distance = KinectOption::CalculateWorldDistance(
          current, m_pBackground[current],
          next, m_pBackground[next]);

      bool isBed = distance < cNeighborPixelsDistanceTolerance &&
                   angleDegree < cNormalsDegreeTolerance;
      if (isBed)
        task.push(next);
    }
  }

  // Find bed corners from the bed area.
  double minDistance[4] = {DBL_MAX, DBL_MAX, DBL_MAX, DBL_MAX};
  int bedCorners[4];
  for (int i = 0; i < KinectOption::cDepthBufferSize; ++i) {
    if (!bed[i])
      continue;

    // Regard a point that minimize the distance to
    // a corner of the screen as a bed corner.
    for (int j = 0; j < 4; ++j) {
      double distance = KinectOption::CalculateScreenDistance(
          i,
          KinectOption::cScreenCornersId[j]);
      if (distance < minDistance[j]) {
        minDistance[j] = distance;
        bedCorners[j] = i;
      }
    }
  }

  for (int i = 0; i < 4; ++i)
    m_bedCorners.push_back(bedCorners[i]);
  CalculateCoordinatesOfBedCorners();

  // Recalculate a bed normal using the defined area.
  GetAverageBedNormal();
}

void Observer::Initialize(const UINT16 *pBuffer) {
  if (!m_initializeNext)
    return;

  // Set current depth buffer into "m_pBackground".
  memcpy(m_pBackground, pBuffer, sizeof(m_pBackground));
  memset(m_pDifference, 0, sizeof(m_pDifference));

  m_logs.clear();

  if (!m_initializeOnlyBackground) {
    LoadConstants();

    // Search for a bed area around the center of the screen.
    m_bedCorners.clear();
    RegisterBedCorners(KinectOption::cDepthBufferXCenter,
                       KinectOption::cDepthBufferYCenter);
  }
  
  // Prevent to reinitialize.
  m_initializeNext = false;
  m_initializeOnlyBackground = false;
}

void Observer::LoadConstants() {
  // At first, initialize constants with defaults.


  // Open the configuration file from "cConstantsFileUrl".
  static const char cConstantsFileUrl[] = "constants.ini";
  FILE *pConstants;
  errno_t error = fopen_s(&pConstants, cConstantsFileUrl, "r");
  if (error != 0)
    return;

  // Read constants.
  // http://stackoverflow.com/questions/17214026/
  while (!feof(pConstants)) {
    char name[128];
    double value;
    fscanf_s(pConstants, "%s%lf%*[\r\n ]", name, 128, &value);

    // Interpret what the value is.

  }

  fclose(pConstants);

  // Correct constants.

}

void Observer::InterpolateDepth(UINT16 *pBuffer) const {
  UINT16 pTempBuffer[KinectOption::cDepthBufferSize];
  memcpy(pTempBuffer, pBuffer, sizeof(pTempBuffer));

  for (int i = 0; i < KinectOption::cDepthBufferSize; ++i) {
    if (KinectOption::IsAvailableDepth(pTempBuffer[i]))
      continue;

    // Interpolate average depth of 8-neighbor.
    double sumDepth = 0;
    double sumWeight = 0;
    static const double cWeight[8] = {0.7, 1, 0.7, 1, 1, 0.7, 1, 0.7};
    static const int cDx[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
    static const int cDy[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
    for (int j = 0; j < 8; ++j) {
      int next = KinectOption::GetNextId(i, cDx[j], cDy[j]);
      if (KinectOption::IsAvailableDepth(pTempBuffer[next])) {
        sumDepth += cWeight[j] * pTempBuffer[next];
        sumWeight += cWeight[j];
      }
    }
    pBuffer[i] = (sumWeight == 0.0) ? 0 :
        static_cast<UINT16>(sumDepth / sumWeight);
  }
}

void Observer::CalculateDepthDifferences(const UINT16 *pBuffer) {
  // Claculate the difference between first depth buffer and given one.
  for (int i = 0; i < KinectOption::cDepthBufferSize; ++i) {
    bool isCorrect = KinectOption::IsAvailableDepth(m_pBackground[i]) &&
                     KinectOption::IsAvailableDepth(pBuffer[i]);
    if (!isCorrect) {
      m_pDifference[i] = 0;
    } else {
      m_pDifference[i] = max(0, m_pBackground[i] - pBuffer[i]);

      // Ignore noise.
      if (IsOnBed(i, pBuffer[i])) {
        if (m_pDifference[i] < cDepthOnBedNoiseBorder)
          m_pDifference[i] = 0;
      } else {
        if (m_pDifference[i] < cDepthNoiseBorder)
          m_pDifference[i] = 0;
      }
    }
  }
}

void Observer::UpdateBackgroundWithoutPatient(const UINT16 *pBuffer) {
  if (m_headPosition == eUnknown)
    return;

  for (int i = 0; i < KinectOption::cDepthBufferSize; ++i) {
    if (!IsInnerPatientArea(i))
      m_pBackground[i] = pBuffer[i];
  }
}

void Observer::JudgePatientState(const UINT16 *pBuffer) {
  m_shoulderPosition = eUnknown;

  // There is no head.
  if (eUnknown == m_headPosition) {
    m_logs.back().state = eNone;
    return;
  }

  // There is a head. ->
  double probabilityPatientOnBed = CalculateProbabilityOnBed(pBuffer);
  m_logs.back().probabilityPatientOnBed = probabilityPatientOnBed;

  // Judge a patient's state from probability that a patient is on a bed.
  // The relationship is as follows.
  // |-> 0.0     |-> cBorderProbabilityStanding       |-> 1.0
  // |           |                |-> cBorderProbabilitySittingOnEdge
  // | eStanding | eSittingOnEdge | eSitting = eLying |
  PatientState state;
  if (cBorderProbabilitySittingOnEdge < probabilityPatientOnBed) {
    double headHeight;
    IsOnBed(m_headPosition, m_depthAtHead, &headHeight);
    state = cHeadHeightBorderSittingAndLying < headHeight ? eSitting :
            IsLyingOnSide(pBuffer) ? eLyingOnSide : eLying;
  } else if (cBorderProbabilityStanding < probabilityPatientOnBed) {
    state = eSittingOnEdge;
  } else {
    state = eStanding;
  }

  m_logs.back().state = state;
}

void Observer::ReduceNoiseOfPatientState() {
  static double prevState = eNone;
  if (m_logs.size() <= 1)  // When "Initialize()" was called.
    prevState = eNone;  // Reset a state.
  double currentState = GetState();

  // Low-pass filter.
  static const double cFilterStrength = 0.92;
  double newState = (1.0 - cFilterStrength) * currentState +
      cFilterStrength * prevState;

  // Convert the type of the filtered state into "PatientState".
  m_logs.back().state = static_cast<PatientState>(static_cast<int>(
      round(newState)));

  prevState = newState;
}

double Observer::CalculateProbabilityOnBed(const UINT16 *pBuffer) const {
  if (!IsBedAreaDefined())
    return 0.0;

  // Count the number of changed pixels.
  int numPixelsInnerBed = 0;
  int numPixelsOuterBed = 0;
  for (int i = 0; i < KinectOption::cDepthBufferSize; ++i) {
    // Skip a pixel whose depth changed little.
    if (!IsThereSomething(i))
      continue;

    // Check whether a pixel is an inner of the bed.
    if (IsOnBed(i, pBuffer[i]))
      ++numPixelsInnerBed;
    else
      ++numPixelsOuterBed;  
  }

  // Calculate max() to avoid 0 division.
  double probabilityPatientOnBed = 1.0 * numPixelsInnerBed /
      max(1, numPixelsInnerBed + numPixelsOuterBed);

  return probabilityPatientOnBed;
}

bool Observer::IsLyingOnSide(const UINT16 *pBuffer) {
  int dxHeadAndShoulder = static_cast<int>(
      KinectOption::ConvertIntoScreenLength(cDistanceHeadAndShoulder,
                                            m_depthAtHead));
  int dxHeadAndHip = static_cast<int>(
      KinectOption::ConvertIntoScreenLength(cDistanceHeadAndHip,
                                            m_depthAtHead));

  // Get a body shape.
  std::vector<double> bodyHeights;
  std::vector<int> bodyHeightsIds;
  for (int dx = dxHeadAndShoulder; dx <= dxHeadAndHip; ++dx) {
    int x = KinectOption::GetNextX(
        m_headPosition,
        KinectOption::IsLeftSide(m_headPosition) ? dx : -dx);

    // Search for the highest point along the current x line.
    double highestHeight = DBL_MIN;
    int idAthighestPoint;
    for (int y = 0; y < KinectOption::cDepthBufferHeight; ++y) {
      int id = KinectOption::GetId(x, y);
      double height;
      if (IsThereSomething(id) && IsOnBed(id, pBuffer[id], &height) &&
          highestHeight < height) {
        highestHeight = height;
        idAthighestPoint = id;
      }
    }

    if (highestHeight != DBL_MIN) {
      bodyHeights.push_back(highestHeight);
      bodyHeightsIds.push_back(idAthighestPoint);
    }
  }

  // Regard the lowest point in "bodyHeights" as shoulder.
  double shoulderHeight = bodyHeights.empty() ? 0.0 : DBL_MAX;
  for (int i = 0; i < static_cast<int>(bodyHeights.size()) - 1; ++i) {
    double height = bodyHeights[i];
    if (height < shoulderHeight) {
      shoulderHeight = height;
      m_shoulderPosition = bodyHeightsIds[i];
    }
  }

  return cShoulderHeightBorderTurningAndLying < shoulderHeight - m_quiltHeight;
}

void Observer::TrackHead(const UINT16 *pBuffer) {
  m_headPosition = SearchForHead(pBuffer);
  m_depthAtHead = (m_headPosition == eUnknown) ? eUnknown :
      pBuffer[m_headPosition];
  
  // Calculate variables related with a head.
  m_relativeHeadSize = eUnknown;
  if (eUnknown != m_headPosition) {
    m_relativeHeadSize = static_cast<int>(
        KinectOption::ConvertIntoScreenLength(cHeadWidth, m_depthAtHead));
  }
}

int Observer::SearchForHead(const UINT16 *pBuffer) const {
  // Search for a topmost position where a head can exist.
  int headTopmost = eUnknown;
  int minDepth = INT_MAX;
  for (int id = 0; id < KinectOption::cDepthBufferSize; ++id) {
    int depth = pBuffer[id];

    // Check skippable of this pixel for faster searching.
    if (!IsThereSomething(id) || minDepth <= depth)
      continue;

    // Check whether a head can be here.
    // NOTE: This function call costs time.
    if (IsHead(id, depth)) {
      minDepth = depth;
      headTopmost = id;
    }
  }

  bool isThereNoHead = (headTopmost == eUnknown);
  if (isThereNoHead)
    return eUnknown;

  // Search for the nearest position to an edge where a head can exist
  // if a patient is lying at a previous frame.
  int headNearestEdge = eUnknown;
  for (int dx = 0; dx < KinectOption::cDepthBufferWidth; ++dx) {
    int x = KinectOption::IsLeftSide(m_headPosition) ? dx :
        KinectOption::cDepthBufferWidth - 1 - dx;
    for (int y = 0; y < KinectOption::cDepthBufferHeight; ++y) {
      int id = KinectOption::GetId(x, y);
      int depth = pBuffer[id];

      // Check skippable of this pixel for faster searching.
      if (!IsThereSomething(id))
        continue;

      // Check whether a head can be here.
      // NOTE: This function call costs time.
      if (IsHead(id, depth)) {
        headNearestEdge = id;
        break;
      }
    }
    if (headNearestEdge != eUnknown)
      break;
  }

  // Choose the most suitable position as a head
  // with weighting each distance.
  // Calculate weight.
  static const double cWeightHeadTopmostRising = 3.0;
  double weightHeadTopmost = cWeightHeadTopmostRising;
  bool isRising = (GetState() <= eSitting);
  if (!isRising) {
    double headHeight;
    IsOnBed(m_headPosition, m_depthAtHead, &headHeight);
    double coefficientRising = headHeight / cHeadHeightBorderSittingAndLying;
    weightHeadTopmost = coefficientRising;
  }

  // Determine a head.
  double distanceHeadNearestEdge = KinectOption::CalculateWorldDistance(
      headNearestEdge, pBuffer[headNearestEdge],
      m_headPosition, m_depthAtHead);
  double distanceHeadTopmost = KinectOption::CalculateWorldDistance(
      headTopmost, pBuffer[headTopmost],
      m_headPosition, m_depthAtHead);
  return (distanceHeadNearestEdge * weightHeadTopmost < distanceHeadTopmost) ?
      headNearestEdge : headTopmost;
}

bool Observer::IsHead(int id, int depth) const {
  static const double cRatioCircumscribedSquareToCircle = M_PI / 4;
  int currentHeadSize = static_cast<int>(
      KinectOption::ConvertIntoScreenLength(cHeadWidth, depth));
  int searchArea = static_cast<int>(pow(currentHeadSize, 2));
  int minAreaToRegardAsHead = static_cast<int>(
      cRatioCircumscribedSquareToCircle * searchArea);

  // Get depth data around the current position.
  int area = 0;
  int counter = 0;
  for (int dy = -currentHeadSize / 2; dy < currentHeadSize / 2; ++dy) {
    for (int dx = -currentHeadSize / 2; dx < currentHeadSize / 2; ++dx) {
      int idAroundCurrent = KinectOption::GetNextId(id, dx, dy);
      area += IsThereSomething(idAroundCurrent) ? 1 : 0;

      // If the area is larger than the setting,
      // regard the current position as a head position.
      if (minAreaToRegardAsHead <= area)
        return true;

      // If won't find a head any more, stop searching.
      int remainingAllArea = searchArea - (++counter);
      int remainingNeededArea = minAreaToRegardAsHead - area;
      if (remainingAllArea < remainingNeededArea)
        return false;
    }
  }

  return false;
}

Vector Observer::GetTempBedNormal(int clickedId) {
  int clickedDepth = m_pBackground[clickedId];
  static const int cWidthToGetTempBedNormal = 150;
  int currentSize = static_cast<int>(KinectOption::ConvertIntoScreenLength(
      cWidthToGetTempBedNormal,  // ID.
      clickedDepth));            // Depth at ID.

  // Average normals around the clicked position.
  Vector normalSum;
  for (int dy = -currentSize / 2; dy < currentSize / 2; ++dy) {
    for (int dx = -currentSize / 2; dx < currentSize / 2; ++dx) {
      int id = KinectOption::GetNextId(clickedId, dx, dy);
      Vector normal = KinectOption::CalculateNormal(id, m_pBackground);
      normal = normal.Normalize();
      normalSum = normalSum.Add(normal);
    }
  }
  
  bool isInvalid = (0.0 == normalSum.Length());
  if (isInvalid) {
    InitializeAllNext();
    return normalSum;
  }
  return normalSum.Normalize();
}

void Observer::GetAverageBedNormal() {
  // Average normals on the bed area.
  Vector normalSum;
  for (int i = 0; i < KinectOption::cDepthBufferSize; ++i) {
    if (IsInnerBed(i)) {
      Vector normal = KinectOption::CalculateNormal(i, m_pBackground);
      normal = normal.Normalize();
      normalSum = normalSum.Add(normal);
    }
  }

  bool isInvalid = (0.0 == normalSum.Length());
  if (isInvalid) {
    InitializeAllNext();
    return;
  }
  m_bedNormal = normalSum.Normalize();
}

void Observer::GetAverageQuiltHeight(const UINT16 *pBuffer) {
  static const double cAirRatio = 0.1;
  double sumHeight = 0.0;
  int counter = 0;
  for (int i = 0; i < KinectOption::cDepthBufferSize; ++i) {
    double height;
    if (IsOnBed(i, pBuffer[i], &height)) {
      sumHeight += height;
      ++counter;
    }
  }
  m_quiltHeight = sumHeight / counter * (1.0 - cAirRatio);
}

void Observer::CalculateCoordinatesOfBedCorners() {
  m_coordinatesBedCorners.clear();
  for (int i = 0; i < static_cast<int>(m_bedCorners.size()); ++i) {
    int id = m_bedCorners[i];
    int depth = m_pBackground[id];
    Vector coordinates = KinectOption::ConvertIntoWorldCoordinates(id, depth);
    m_coordinatesBedCorners.push_back(coordinates);
  }
}

bool Observer::IsInnerBed(int id) const {
  if (!IsBedAreaDefined())
    return false;

  // With crossing number algorithm.
  // http://geomalgorithms.com/a03-_inclusion.html
  // Count the number of intersections.
  int numIntersections = 0;
  int x = KinectOption::GetX(id);
  int y = KinectOption::GetY(id);
  for (int i = 0; i < static_cast<int>(m_bedCorners.size()); ++i) {
    int id1 = m_bedCorners[i];
    int id2 = m_bedCorners[(i + 1) % m_bedCorners.size()];
    int x1 = KinectOption::GetX(id1);
    int y1 = KinectOption::GetY(id1);
    int x2 = KinectOption::GetX(id2);
    int y2 = KinectOption::GetY(id2);

    // Y-coordinate of given pixel is in y-coordinates of a side.
    bool isYInRange = (y1 <= y && y < y2) || (y2 <= y && y < y1);
    if (isYInRange && x < x1 + 1.0 * (y - y1) / (y2 - y1) * (x2 - x1))
      ++numIntersections;
  }

  // Judge.
  return numIntersections % 2 == 1;
}

bool Observer::IsOnBed(int id, int depth, double *height) const {
  if (!IsBedAreaDefined() || !KinectOption::IsAvailableDepth(depth))
    return false;

  // Convert viewport coordinates into world them.
  Vector p = KinectOption::ConvertIntoWorldCoordinates(id, depth);

  // Check whether the point is on the bed.
  for (int i = 0; i < 2; ++i) {
    // With the Möller–Trumbore intersection algorithm
    // from "Practical Analysis of Optimized Ray-Triangle Intersection".
    // http://stackoverflow.com/questions/37652337/
    static const double cEpsilon = 1e-5;
    Vector e1 = m_coordinatesBedCorners[2].Subtract(
        m_coordinatesBedCorners[0]);
    Vector e2 = m_coordinatesBedCorners[i * 2 + 1].Subtract(
        m_coordinatesBedCorners[0]);
    Vector qVec, pVec = m_bedNormal.Cross(e2);
    double det = e1.Dot(pVec);

    if (cEpsilon < det) {
      Vector tVec = p.Subtract(m_coordinatesBedCorners[0]);
      double u = tVec.Dot(pVec);
      if (u < 0.0 || det < u)
        continue;

      qVec = tVec.Cross(e1);
      double v = m_bedNormal.Dot(qVec);
      if (v < 0.0 || det < u + v)
        continue;
    } else if (det < -cEpsilon) {
      Vector tVec = p.Subtract(m_coordinatesBedCorners[0]);
      double u = tVec.Dot(pVec);
      if (0.0 < u || u < det)
        continue;

      qVec = tVec.Cross(e1);
      double v = m_bedNormal.Dot(qVec);
      if (0.0 < v || u + v < det)
        continue;
    } else {
      continue;
    }

    // Get the distance between the given point and the intersection.
    if (height != NULL)
      *height = e2.Dot(qVec) / det;

    return true;
  }

  return false;
}

void Observer::SearchForPatientArea(const UINT16 *pBuffer) {
  m_patientCorners.clear();
  if (m_headPosition == eUnknown)
    return;

  // Search for a patient area.
  bool patient[KinectOption::cDepthBufferSize] = {false};
  std::queue<int> task; task.push(m_headPosition);
  std::map<int, bool> isSearched;
  while (!task.empty()) {
    // Pop a point.
    int current = task.front(); task.pop();
    patient[current] = true;

    // Search for 4-neighbor of the point recursively.
    static const int cDx[4] = {0, -1, 1, 0};
    static const int cDy[4] = {-1, 0, 0, 1};
    for (int i = 0; i < 4; ++i) {
      // Skip a point checked already.
      int next = KinectOption::GetNextId(
          current,
          cDx[i] * (1 + cNumSkipToSearchForPatientArea),
          cDy[i] * (1 + cNumSkipToSearchForPatientArea));
      if (0 < isSearched.count(next))
        continue;
      isSearched[next] = true;

      // Skip a point whose depth has changed little.
      if (m_pBackground[next] - pBuffer[next] <=
          cDepthNoiseBorderToSearchForPatientArea) {
        patient[next] = true;  // Space.
        continue;
      }

      task.push(next);
    }
  }

  // Find the corners of the patient area.
  int xRange[2] = {INT_MAX, INT_MIN};  // xRange[0]: minX, xRange[1]: maxX
  int yRange[2] = {INT_MAX, INT_MIN};  // yRange[0]: minY, yRange[1]: maxY
  for (int y = 0; y < KinectOption::cDepthBufferHeight; ++y) {
    for (int x = 0; x < KinectOption::cDepthBufferWidth; ++x) {
      if (!patient[KinectOption::GetId(x, y)])
        continue;
      xRange[0] = min(xRange[0], x);
      xRange[1] = max(xRange[1], x);
      yRange[0] = min(yRange[0], y);
      yRange[1] = max(yRange[1], y);
    }
  }
  for (int xId = 0; xId < 2; ++xId) {
    for (int yId = 0; yId < 2; ++yId) {
      int y = xId ? yRange[1 - yId] : yRange[yId];  // To loop.
      int corner = KinectOption::GetId(xRange[xId], y);
      m_patientCorners.push_back(corner);
    }
  }
}

// TODO: Create a new class, "Plane" and "ScreenPoint"
// and merge with "IsInnerBed()".
bool Observer::IsInnerPatientArea(int id) const {
  if (m_patientCorners.size() < 4)
    return false;

  // With crossing number algorithm.
  // http://geomalgorithms.com/a03-_inclusion.html
  // Count the number of intersections.
  int numIntersections = 0;
  int x = KinectOption::GetX(id);
  int y = KinectOption::GetY(id);
  for (int i = 0; i < static_cast<int>(m_patientCorners.size()); ++i) {
    int id1 = m_patientCorners[i];
    int id2 = m_patientCorners[(i + 1) % m_patientCorners.size()];
    int x1 = KinectOption::GetX(id1);
    int y1 = KinectOption::GetY(id1);
    int x2 = KinectOption::GetX(id2);
    int y2 = KinectOption::GetY(id2);

    // Y-coordinate of given pixel is in y-coordinates of a side.
    bool isYInRange = (y1 <= y && y < y2) || (y2 <= y && y < y1);
    if (isYInRange && x < x1 + 1.0 * (y - y1) / (y2 - y1) * (x2 - x1))
      ++numIntersections;
  }

  // Judge.
  return numIntersections % 2 == 1;
}