/*! \file sns_ik_math_utils.hpp
 * \brief Math utilities for the SNS IK solvers
 * \author Fabrizio Flacco
 * \author Forrest Rogers-Marcovitz
 */
/*
 *    Copyright 2016 Rethink Robotics
 *
 *    Copyright 2012-2016 Fabrizio Flacco
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include <sns_ik/sns_ik_math_utils.hpp>

using namespace Eigen;
using namespace sns_ik;

namespace sns_ik {

bool pinv(const MatrixD &A, MatrixD *invA, Scalar eps) {

  //A (m x n) usually comes from a redundant task jacobian, therfore we consider m<n
  int m = A.rows() - 1;
  VectorD sigma;  //vector of singular values

  JacobiSVD<MatrixD> svd_A(A.transpose(), ComputeThinU | ComputeThinV);
  sigma = svd_A.singularValues();
  if (((m > 0) && (sigma(m) > eps)) || ((m == 0) && (A.array().abs() > eps).any())) {
    for (int i = 0; i <= m; i++) {
      sigma(i) = 1.0 / sigma(i);
    }
    (*invA) = svd_A.matrixU() * sigma.asDiagonal() * svd_A.matrixV().transpose();
    return true;
  } else {
    return false;
  }
}

bool pinv_P(const MatrixD &A, MatrixD *invA, MatrixD *P, Scalar eps) {

  //A (m x n) usually comes from a redundant task jacobian, therfore we consider m<n
  int m = A.rows() - 1;
  VectorD sigma;  //vector of singular values

  JacobiSVD<MatrixD> svd_A(A.transpose(), ComputeThinU | ComputeThinV);
  sigma = svd_A.singularValues();
  if (((m > 0) && (sigma(m) > eps)) || ((m == 0) && (A.array().abs() > eps).any())) {
    for (int i = 0; i <= m; i++) {
      sigma(i) = 1.0 / sigma(i);
    }
    (*invA) = svd_A.matrixU() * sigma.asDiagonal() * svd_A.matrixV().transpose();
    (*P) = ((*P) - svd_A.matrixU() * svd_A.matrixU().transpose()).eval();
    return true;
  } else {
    return false;
  }

}

bool pinv_damped(const MatrixD &A, MatrixD *invA, Scalar lambda_max, Scalar eps) {

  //A (m x n) usually comes from a redundant task jacobian, therfore we consider m<n
  int m = A.rows() - 1;
  VectorD sigma;  //vector of singular values
  Scalar lambda2;
  int r = 0;

  JacobiSVD<MatrixD> svd_A(A.transpose(), ComputeThinU | ComputeThinV);
  sigma = svd_A.singularValues();
  if (((m > 0) && (sigma(m) > eps)) || ((m == 0) && (A.array().abs() > eps).any())) {
    for (int i = 0; i <= m; i++) {
      sigma(i) = 1.0 / sigma(i);
    }
    (*invA) = svd_A.matrixU() * sigma.asDiagonal() * svd_A.matrixV().transpose();
    return true;
  } else {
    lambda2 = (1 - (sigma(m) / eps) * (sigma(m) / eps)) * lambda_max * lambda_max;
    for (int i = 0; i <= m; i++) {
      if (sigma(i) > EPSQ)
        r++;
      sigma(i) = (sigma(i) / (sigma(i) * sigma(i) + lambda2));
    }
    //only U till the rank
    MatrixD subU = svd_A.matrixU().block(0, 0, A.cols(), r);
    MatrixD subV = svd_A.matrixV().block(0, 0, A.rows(), r);

    (*invA) = subU * sigma.asDiagonal() * subV.transpose();
    return false;
  }

}

bool pinv_damped_P(const MatrixD &A, MatrixD *invA, MatrixD *P, Scalar lambda_max, Scalar eps) {

  //A (m x n) usually comes from a redundant task jacobian, therfore we consider m<n
  int m = A.rows() - 1;
  int r = 0;  //rank
  Scalar lambda2;

  JacobiSVD<MatrixD> svd_A(A.transpose(), ComputeThinU | ComputeThinV);
  VectorD sigma = svd_A.singularValues();

  if (((m > 0) && (sigma(m) > eps)) || ((m == 0) && (A.array().abs() > eps).any())) {
    for (int i = 0; i <= m; i++) {
      sigma(i) = 1.0 / sigma(i);
    }
    (*invA) = svd_A.matrixU() * sigma.asDiagonal() * svd_A.matrixV().transpose();
    (*P) = ((*P) - svd_A.matrixU() * svd_A.matrixU().transpose()).eval();
    return true;
  } else {
    lambda2 = (1 - (sigma(m) / eps) * (sigma(m) / eps)) * lambda_max * lambda_max;
    VectorD subSigma = VectorD::Ones(m + 1);
    for (int i = 0; i <= m; i++) {
      if (sigma(i) > EPSQ) {
        subSigma(r++) = (sigma(i) / (sigma(i) * sigma(i) + lambda2));
      }
    }

    //only U till the rank
    MatrixD subU = svd_A.matrixU().block(0, 0, A.cols(), r);
    MatrixD subV = svd_A.matrixV().block(0, 0, A.rows(), r);
    (*P) = ((*P) - subU * subU.transpose()).eval();

    (*invA) = subU * subSigma.head(r).asDiagonal() * subV.transpose();
    return false;
  }

}

bool pinv_QR(const MatrixD &A, MatrixD *invA, Scalar eps) {
  MatrixD At = A.transpose();
  HouseholderQR < MatrixD > qr = At.householderQr();
  int m = A.rows();
  //int n = A.cols();

  MatrixD Rt = MatrixD::Zero(m, m);
  bool invertible;

  MatrixD hR = (MatrixD) qr.matrixQR();
  MatrixD Y = ((MatrixD) qr.householderQ()).leftCols(m);

  //take the useful part of R
  for (int i = 0; i < m; i++) {
    for (int j = 0; j <= i; j++)
      Rt(i, j) = hR(j, i);
  }
  FullPivLU < MatrixD > invRt(Rt);

  invertible = fabs(invRt.determinant()) > eps;

  if (invertible) {
    *invA = Y * invRt.inverse();
    return true;
  } else {
    return false;
  }

}

bool pinv_QR_Z(const MatrixD &A, const MatrixD &Z0, MatrixD *invA, MatrixD *Z, Scalar lambda_max, Scalar eps) {
  VectorD sigma;  //vector of singular values
  Scalar lambda2;

  MatrixD AZ0t = (A * Z0).transpose();
  HouseholderQR < MatrixD > qr = AZ0t.householderQr();

  int m = A.rows();
  int p = Z0.cols();

  MatrixD Rt = MatrixD::Zero(m, m);
  bool invertible;
  MatrixD hR = (MatrixD) qr.matrixQR();
  MatrixD Y = ((MatrixD) qr.householderQ()).leftCols(m);

  //take the useful part of R
  for (int i = 0; i < m; i++) {
    for (int j = 0; j <= i; j++)
      Rt(i, j) = hR(j, i);
  }

  FullPivLU < MatrixD > invRt(Rt);
  invertible = fabs(invRt.determinant()) > eps;

  if (invertible) {
    *invA = Z0 * Y * invRt.inverse();
    *Z = Z0 * (((MatrixD) qr.householderQ()).rightCols(p - m));
    return true;
  } else {
    MatrixD R = MatrixD::Zero(m, m);
    //take the useful part of R
    for (int i = 0; i < m; i++) {
      for (int j = i; j < m; j++)  // TODO: is starting at i correct?
        R(i, j) = hR(i, j);
    }

    //perform the SVD of R
    JacobiSVD<MatrixD> svd_R(R, ComputeThinU | ComputeThinV);
    sigma = svd_R.singularValues();
    lambda2 = (1 - (sigma(m - 1) / eps) * (sigma(m - 1) / eps)) * lambda_max * lambda_max;
    for (int i = 0; i < m; i++) {
      sigma(i) = sigma(i) / (sigma(i) * sigma(i) + lambda2);
    }
    (*invA) = Z0 * Y * svd_R.matrixU() * sigma.asDiagonal() * svd_R.matrixV().transpose();

    *Z = Z0 * (((MatrixD) qr.householderQ()).rightCols(p - m));
    return false;
  }

}

bool pinv_forBarP(const MatrixD &W, const MatrixD &P, MatrixD *inv) {

  MatrixD tmp;
  bool invertible;

  int sizeBarW = (W.diagonal().array() > 0.99).cast<int>().sum();
  MatrixD barW(sizeBarW, W.cols());
  int rowsBarW = 0;

  for (int i = 0; i < W.rows(); i++) {
    if (W(i, i) > 0.99) {  //equal to 1 (safer)
      barW.row(rowsBarW++) = W.row(i);
    }
  }

  tmp = barW * P * barW.transpose();
  FullPivLU < MatrixD > inversePbar(tmp);

  invertible = inversePbar.isInvertible();

  if (invertible) {
    (*inv) = P * barW.transpose() * inversePbar.inverse() * barW;
    return true;
  } else {
    (*inv) = MatrixD::Zero(W.rows(), W.rows());
    return false;
  }
}

bool isIdentity(const MatrixD &A) {

  bool isIdentity = true;
  int n = A.rows();
  int i = 0;
  do {
    isIdentity &= (A(i, i) > 0.99);  // equal to 1.0 (safer)
    i++;
  } while (isIdentity && i < n);

  return isIdentity;
}

} // namespace sns_ikl
