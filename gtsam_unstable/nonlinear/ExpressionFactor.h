/* ----------------------------------------------------------------------------

 * GTSAM Copyright 2010, Georgia Tech Research Corporation, 
 * Atlanta, Georgia 30332-0415
 * All Rights Reserved
 * Authors: Frank Dellaert, et al. (see THANKS for the full author list)

 * See LICENSE for the license information

 * -------------------------------------------------------------------------- */

/**
 * @file Expression.h
 * @date September 18, 2014
 * @author Frank Dellaert
 * @author Paul Furgale
 * @brief Expressions for Block Automatic Differentiation
 */

#include <gtsam_unstable/nonlinear/Expression.h>
#include <gtsam/nonlinear/NonlinearFactor.h>
#include <gtsam/base/Testable.h>

namespace gtsam {

/**
 * Factor that supports arbitrary expressions via AD
 */
template<class T>
class ExpressionFactor: public NoiseModelFactor {

  const T measurement_;
  const Expression<T> expression_;

public:

  /// Constructor
  ExpressionFactor(const SharedNoiseModel& noiseModel, //
      const T& measurement, const Expression<T>& expression) :
      NoiseModelFactor(noiseModel, expression.keys()), //
      measurement_(measurement), expression_(expression) {
    if (!noiseModel)
      throw std::invalid_argument("ExpressionFactor: no NoiseModel.");
    if (noiseModel->dim() != T::dimension)
      throw std::invalid_argument(
          "ExpressionFactor was created with a NoiseModel of incorrect dimension.");
  }

  /**
   * Error function *without* the NoiseModel, \f$ z-h(x) \f$.
   * We override this method to provide
   * both the function evaluation and its derivative(s) in H.
   */
  virtual Vector unwhitenedError(const Values& x,
      boost::optional<std::vector<Matrix>&> H = boost::none) const {
    if (H) {
      // H should be pre-allocated
      assert(H->size()==size());

      // Get dimensions of Jacobian matrices
      std::vector<size_t> dims = expression_.dimensions();

      // Create and zero out blocks to be passed to expression_
      JacobianMap blocks;
      for(DenseIndex i=0;i<size();i++) {
        Matrix& Hi = H->at(i);
        Hi.resize(T::dimension, dims[i]);
        Hi.setZero(); // zero out
        Eigen::Block<Matrix> block = Hi.block(0,0,T::dimension, dims[i]);
        blocks.insert(std::make_pair(keys_[i], block));
      }

      T value = expression_.value(x, blocks);
      return measurement_.localCoordinates(value);
    } else {
      const T& value = expression_.value(x);
      return measurement_.localCoordinates(value);
    }
  }

  // Internal function to allocate a VerticalBlockMatrix and
  // create Eigen::Block<Matrix> views into it
  VerticalBlockMatrix prepareBlocks(JacobianMap& blocks) const {

    // Get dimensions of Jacobian matrices
    std::vector<size_t> dims = expression_.dimensions();

    // Construct block matrix, is of right size but un-initialized
    VerticalBlockMatrix Ab(dims, T::dimension, true);
    Ab.matrix().setZero(); // zero out

    // Create blocks to be passed to expression_
    for(DenseIndex i=0;i<size();i++)
      blocks.insert(std::make_pair(keys_[i], Ab(i)));

    return Ab;
  }

  virtual boost::shared_ptr<GaussianFactor> linearize(const Values& x) const {

    // Construct VerticalBlockMatrix and views into it
    JacobianMap blocks;
    VerticalBlockMatrix Ab = prepareBlocks(blocks);

    // Evaluate error to get Jacobians and RHS vector b
    T value = expression_.value(x, blocks);
    Ab(size()).col(0) = -measurement_.localCoordinates(value);

    // Whiten the corresponding system now
    // TODO ! this->noiseModel_->WhitenSystem(Ab);

    // TODO pass unwhitened + noise model to Gaussian factor
    // For now, only linearized constrained factors have noise model at linear level!!!
    noiseModel::Constrained::shared_ptr constrained = //
        boost::dynamic_pointer_cast<noiseModel::Constrained>(this->noiseModel_);
    if (constrained) {
      return boost::make_shared<JacobianFactor>(this->keys(), Ab,
          constrained->unit());
    } else
      return boost::make_shared<JacobianFactor>(this->keys(), Ab);
  }
};
// ExpressionFactor

}
