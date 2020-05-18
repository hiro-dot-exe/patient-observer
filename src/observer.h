#ifndef KINECT_PATIENTS_OBSERVER_OBSERVER_H_
#define KINECT_PATIENTS_OBSERVER_OBSERVER_H_

#include <vector>
#include "vector.h"
#include "kinect_option.h"  // KinectOption::cDepthBufferSize

class Observer {
public:
  enum VariableStatus { eUnknown = -1 };
  enum PatientState {
    eNone,
    eStanding,
    eSittingOnEdge,
    eSitting,
    eLying,
    eLyingOnSide,
  };

  /// <summary>
  /// Frame information to draw graph.
  /// </summary>
  struct Log {
    double probabilityPatientOnBed;
    PatientState state;
  };

  // To judge a patient's state.
  static const double cBorderProbabilityStanding;
  static const double cBorderProbabilitySittingOnEdge;

  Observer();

  /// <summary>
  /// Main processing function.
  /// </summary>
  /// <param name="pBuffer">pointer to depth frame data</param>
  void Observe(const UINT16 *pBuffer);
  /// <summary>
  /// Register bed corners calculating the normal around a clicked point.
  /// </summary>
  /// <param name="x">x of a clicked point</param>
  /// <param name="y">y of a clicked point</param>
  void RegisterBedCorners(int x, int y);

  // Accessors.
  int GetHeadPosition() const { return m_headPosition; }
  int GetShoulderPosition() const { return m_shoulderPosition; }
  int GetRelativeHeadSize() const { return m_relativeHeadSize; }
  std::vector<int> GetPatientCorners() const { return m_patientCorners; }
  Vector GetBedNormal() const { return m_bedNormal; }
  std::vector<int> GetBedCorners() const { return m_bedCorners; }
  const std::vector<Log> &GetLog() const { return m_logs; }
  PatientState GetState() const {
    return m_logs.empty() ? eNone : m_logs.back().state;
  }
  double GetProbabilityPatientOnBed() const {
    return m_logs.empty() ? 0.0 : m_logs.back().probabilityPatientOnBed;
  }
  bool IsThereSomething(int id) const {
    id = max(id, 0);
    id = min(id, KinectOption::cDepthBufferSize - 1);
    return 0 < m_pDifference[id];
  }
  void InitializeAllNext() {
    m_initializeNext = true;
    m_initializeOnlyBackground = false;
  }
  void InitializeOnlyBackgroundNext() {
    m_initializeNext = true;
    m_initializeOnlyBackground = true;
  }

private:
  // To get differences of depths.
  static const int cDepthNoiseBorder;       // [mm]
  static const int cDepthOnBedNoiseBorder;  // [mm]
  // To search for a patient area.
  static const int cDepthNoiseBorderToSearchForPatientArea;  // [mm]
  static const int cNumSkipToSearchForPatientArea;
  // To define a bed area.
  static const int cNormalsDegreeTolerance;           // [degree]
  static const int cNeighborPixelsDistanceTolerance;  // [mm]
  // To find a head.
  static const int cHeadWidth;  // [mm]
  // To judge a patient's state.
  static const int cShoulderHeightBorderTurningAndLying;  // [mm]
  static const int cHeadHeightBorderSittingAndLying;      // [mm]
  static const int cDistanceHeadAndShoulder;              // [mm]
  static const int cDistanceHeadAndHip;                   // [mm]

  void Initialize(const UINT16 *pBuffer);
  void LoadConstants();
  void InterpolateDepth(UINT16 *pBuffer) const;
  void CalculateDepthDifferences(const UINT16 *pBuffer);
  void UpdateBackgroundWithoutPatient(const UINT16 *pBuffer);
  // Judge a patient's state.
  void JudgePatientState(const UINT16 *pBuffer);
  void ReduceNoiseOfPatientState();
  double CalculateProbabilityOnBed(const UINT16 *pBuffer) const;
  bool IsLyingOnSide(const UINT16 *pBuffer);
  // Track a head.
  void TrackHead(const UINT16 *pBuffer);
  int SearchForHead(const UINT16 *pBuffer) const;
  bool IsHead(int id, int depth) const;
  // Register a bed.
  Vector GetTempBedNormal(int clickedId);
  void GetAverageBedNormal();
  void GetAverageQuiltHeight(const UINT16 *pBuffer);
  void CalculateCoordinatesOfBedCorners();
  bool IsInnerBed(int id) const;
  bool IsOnBed(int id, int depth, double *height = NULL) const;
  bool IsBedAreaDefined() const {
    return 4 <= m_bedCorners.size() && 4 <= m_coordinatesBedCorners.size();
  }
  // Search for a patient.
  void SearchForPatientArea(const UINT16 *pBuffer);
  bool IsInnerPatientArea(int id) const;

  // To get difference of depths.
  bool m_initializeNext;
  bool m_initializeOnlyBackground;
  UINT16 m_pBackground[KinectOption::cDepthBufferSize];  // [mm]
  UINT16 m_pDifference[KinectOption::cDepthBufferSize];  // [mm]
  // Patient.
  int m_headPosition;
  int m_shoulderPosition;
  int m_depthAtHead;       // [mm]
  int m_relativeHeadSize;  // [px]
  std::vector<int> m_patientCorners;
  // Bed area.
  double m_quiltHeight;
  Vector m_bedNormal;
  std::vector<int> m_bedCorners;
  std::vector<Vector> m_coordinatesBedCorners;
  // To draw graph.
  std::vector<Log> m_logs;
};

#endif  // KINECT_PATIENTS_OBSERVER_OBSERVER_H_