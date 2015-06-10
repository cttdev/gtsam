/* ----------------------------------------------------------------------------

 * GTSAM Copyright 2010, Georgia Tech Research Corporation,
 * Atlanta, Georgia 30332-0415
 * All Rights Reserved
 * Authors: Frank Dellaert, et al. (see THANKS for the full author list)

 * See LICENSE for the license information

 * -------------------------------------------------------------------------- */

/**
 * @file   Similarity3.cpp
 * @brief  Implementation of Similarity3 transform
 * @author Paul Drews
 */

#include <gtsam_unstable/geometry/Similarity3.h>

#include <gtsam/geometry/Pose3.h>
#include <gtsam/base/Manifold.h>

namespace gtsam {

Similarity3::Similarity3() :
    R_(), t_(), s_(1) {
}

Similarity3::Similarity3(double s) :
    s_(s) {
}

Similarity3::Similarity3(const Rot3& R, const Point3& t, double s) :
    R_(R), t_(t), s_(s) {
}

Similarity3::Similarity3(const Matrix3& R, const Vector3& t, double s) :
    R_(R), t_(t), s_(s) {
}

bool Similarity3::equals(const Similarity3& sim, double tol) const {
  return R_.equals(sim.R_, tol) && t_.equals(sim.t_, tol) && s_ < (sim.s_ + tol)
      && s_ > (sim.s_ - tol);
}

bool Similarity3::operator==(const Similarity3& other) const {
  return (R_.equals(other.R_)) && (t_ == other.t_) && (s_ == other.s_);
}

void Similarity3::print(const std::string& s) const {
  std::cout << std::endl;
  std::cout << s;
  rotation().print("R:\n");
  translation().print("t: ");
  std::cout << "s: " << scale() << std::endl;
}

Similarity3 Similarity3::identity() {
  return Similarity3();
}
Similarity3 Similarity3::operator*(const Similarity3& T) const {
  return Similarity3(R_ * T.R_, ((1.0 / T.s_) * t_) + R_ * T.t_, s_ * T.s_);
}

Similarity3 Similarity3::inverse() const {
  Rot3 Rt = R_.inverse();
  Point3 sRt = R_.inverse() * (-s_ * t_);
  return Similarity3(Rt, sRt, 1.0 / s_);
}

Point3 Similarity3::transform_from(const Point3& p, //
    OptionalJacobian<3, 7> H1, OptionalJacobian<3, 3> H2) const {
  if (H1) {
    const Matrix3 R = R_.matrix();
    Matrix3 DR = s_ * R * skewSymmetric(-p.x(), -p.y(), -p.z());
    *H1 << DR, R, R * p.vector();
  }
  if (H2)
    *H2 = s_ * R_.matrix(); // just 3*3 sub-block of matrix()
  return R_ * (s_ * p) + t_;
  // TODO: Effect of scale change is this, right?
  // sR t * (1+v)I 0 * p = s(1+v)R t * p = s(1+v)Rp + t = sRp + vRp + t
  // 0001   000    1   1   000     1   1
}

Point3 Similarity3::operator*(const Point3& p) const {
  return transform_from(p);
}

Matrix7 Similarity3::AdjointMap() const {
  const Matrix3 R = R_.matrix();
  const Vector3 t = t_.vector();
  Matrix3 A = s_ * skewSymmetric(t) * R;
  Matrix7 adj;
  adj << s_ * R, A, -s_ * t, // 3*7
  Z_3x3, R, Matrix31::Zero(), // 3*7
  Matrix16::Zero(), 1; // 1*7
  return adj;
}

Vector7 Similarity3::Logmap(const Similarity3& s, OptionalJacobian<7, 7> Hm) {
  // To get the logmap, calculate w and lambda, then solve for u as show at ethaneade.org
  // www.ethaneade.org/latex2html/lie/node29.html
  Vector3 w = Rot3::Logmap(s.R_);
  double lambda = log(s.s_);

  Matrix33 wx = skewSymmetric(w[0], w[1], w[2]);
  double lambdasquared = lambda * lambda;
  double thetasquared = w.transpose() * w;
  double theta = sqrt(thetasquared);
  double X = sin(theta)/theta;
  double Y = (1-cos(theta))/thetasquared;
  double Z = (1-X)/thetasquared;
  double W = (0.5-Y)/thetasquared;
  double alpha = lambdasquared / (lambdasquared * thetasquared);
  double beta = (exp(-lambda)-1+lambda)/lambdasquared;
  double gama = Y - (lambda * Z);
  double mu = (1-lambda+(0.5*lambdasquared)-exp(-lambda))/(lambdasquared*lambda);
  double upsilon = Z-(lambda*W);
  double A = (1-exp(-lambda))/lambda;
  double B = alpha*(beta-gama)+gama;
  double C = alpha*(mu-upsilon)+upsilon;
  Matrix33 V = A*Matrix33::Identity() + B*wx + C*wx*wx;
  Vector3 u = V.inverse()*s.t_.vector();

  Vector7 result;
  result << w, u, lambda;
  return result;
}

Similarity3 Similarity3::Expmap(const Vector7& v, OptionalJacobian<7, 7> Hm) {
  Matrix31 w(v.head<3>());
  Matrix33 wx = skewSymmetric(w[0], w[1], w[2]);
  double lambda = v[6];
  double lambdasquared = lambda * lambda;
  Matrix31 u(v.segment<3>(3));
  double thetasquared = w.transpose() * w;
  double theta = sqrt(thetasquared);
  double X = sin(theta)/theta;
  double Y = (1-cos(theta))/thetasquared;
  double Z = (1-X)/thetasquared;
  double W = (0.5-Y)/thetasquared;
  double alpha = lambdasquared / (lambdasquared * thetasquared);
  double beta = (exp(-lambda)-1+lambda)/lambdasquared;
  double gama = Y - (lambda * Z);
  double mu = (1-lambda+(0.5*lambdasquared)-exp(-lambda))/(lambdasquared*lambda);
  double upsilon = Z-(lambda*W);
  double A = (1-exp(-lambda))/lambda;
  double B = alpha*(beta-gama)+gama;
  double C = alpha*(mu-upsilon)+upsilon;
  Matrix33 V = A*Matrix33::Identity() + B*wx + C*wx*wx;
  return Similarity3(Rot3::Expmap(w), Point3(V*u), 1.0/exp(-lambda));
}

Similarity3 Similarity3::ChartAtOrigin::Retract(const Vector7& v,
    ChartJacobian H) {
  // Will retracting or localCoordinating R work if R is not a unit rotation?
  // Also, how do we actually get s out?  Seems like we need to store it somewhere.
//  Rot3 r; //Create a zero rotation to do our retraction.
//  return Similarity3( //
//      r.retract(v.head<3>()), // retract rotation using v[0,1,2]
//      Point3(v.segment<3>(3)), // Retract the translation
//      1.0 + v[6]); //finally, update scale using v[6]

  // Use the Expmap
  return Similarity3::Expmap(v);
}

Vector7 Similarity3::ChartAtOrigin::Local(const Similarity3& other,
    ChartJacobian H) {
//  Rot3 r; //Create a zero rotation to do the retraction
//  Vector7 v;
//  v.head<3>() = r.localCoordinates(other.R_);
//  v.segment<3>(3) = other.t_.vector();
//  //v.segment<3>(3) = translation().localCoordinates(other.translation());
//  v[6] = other.s_ - 1.0;
//  return v;

  // Use the Logmap
  return Similarity3::Logmap(other);
}

const Matrix4 Similarity3::matrix() const {
  Matrix4 T;
  T.topRows<3>() << s_ * R_.matrix(), t_.vector();
  T.bottomRows<1>() << 0, 0, 0, 1;
  return T;
}

Similarity3::operator Pose3() const {
  return Pose3(R_, s_ * t_);
}

}
