#ifndef KINECT_PATIENTS_OBSERVER_VECTOR_H_
#define KINECT_PATIENTS_OBSERVER_VECTOR_H_

/// <summary>
/// Basically 3D vector but can be used as 2D with care.
/// </summary>
struct Vector {
  Vector(double initX, double initY, double initZ);
  Vector();

  Vector Add(const Vector &v) const;
  Vector Subtract(const Vector &v) const;
  Vector Cross(const Vector &v) const;
  double Dot(const Vector &v) const;
  double Length() const;
  Vector Normalize() const;
  double AngleRadian(const Vector &v) const;
  double AngleDegree(const Vector &v) const;
  Vector Normal(const Vector &v1, const Vector &v2) const;

  double x;
  double y;
  double z;
};

#endif  // KINECT_PATIENTS_OBSERVER_VECTOR_H_