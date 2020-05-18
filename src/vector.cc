#include "vector.h"
#define _USE_MATH_DEFINES  // M_PI
#include <math.h>          // M_PI, sqrt(), pow(), acos()

Vector::Vector(double initX, double initY, double initZ)
    : x(initX), y(initY), z(initZ) {
}

Vector::Vector() : x(0.0), y(0.0), z(0.0) {
}

Vector Vector::Add(const Vector &v) const {
  Vector answer;
  answer.x = x + v.x;
  answer.y = y + v.y;
  answer.z = z + v.z;
  return answer;
}

Vector Vector::Subtract(const Vector &v) const {
  Vector answer;
  answer.x = x - v.x;
  answer.y = y - v.y;
  answer.z = z - v.z;
  return answer;
}

Vector Vector::Cross(const Vector &v) const {
  Vector answer;
  answer.x = y * v.z - z * v.y;
  answer.y = z * v.x - x * v.z;
  answer.z = x * v.y - y * v.x;
  return answer;
}

double Vector::Dot(const Vector &v) const {
  return x * v.x + y * v.y + z * v.z;
}

double Vector::Length() const {
  return sqrt(pow(x, 2) + pow(y, 2) + pow(z, 2));
}

Vector Vector::Normalize() const {
  Vector answer;
  double length = Length();
  if (0 < length) {
    answer.x = x / length;
    answer.y = y / length;
    answer.z = z / length;
  }
  return answer;
}

double Vector::AngleRadian(const Vector &v) const {
  double angle = 0.0;
  if (0 < Length() && 0 < v.Length())
    angle = acos(Dot(v) / Length() / v.Length());
  return angle;
}

double Vector::AngleDegree(const Vector &v) const {
  return AngleRadian(v) * 180 / M_PI;
}

Vector Vector::Normal(const Vector &v1, const Vector &v2) const {
  Vector normal;
  normal.x = (y - v2.y) * (v1.z - z) - (z - v2.z) * (v1.y - y);
  normal.y = (z - v2.z) * (v1.x - x) - (x - v2.x) * (v1.z - z);
  normal.z = (x - v2.x) * (v1.y - y) - (y - v2.y) * (v1.x - x);
  return normal;
}