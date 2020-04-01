///////////////////////////////////////////////////////////////////////////////
// BSD 3-Clause License
//
// Copyright (C) 2018-2019, LAAS-CNRS, IRI: CSIC-UPC
// Copyright note valid unless otherwise stated in individual files.
// All rights reserved.
///////////////////////////////////////////////////////////////////////////////

#ifndef CROCODDYL_MULTIBODY_ACTUATIONS_MULTICOPTER_BASE_HPP_
#define CROCODDYL_MULTIBODY_ACTUATIONS_MULTICOPTER_BASE_HPP_

#include "crocoddyl/multibody/fwd.hpp"
#include "crocoddyl/core/utils/exception.hpp"
#include "crocoddyl/core/actuation-base.hpp"
#include "crocoddyl/multibody/states/multibody.hpp"

namespace crocoddyl {
template <typename _Scalar>
class ActuationModelMultiCopterBaseTpl : public ActuationModelAbstractTpl<_Scalar> {
 public:
  typedef _Scalar Scalar;
  typedef MathBaseTpl<Scalar> MathBase;
  typedef ActuationModelAbstractTpl<Scalar> Base;
  typedef StateMultibodyTpl<Scalar> StateMultibody;
  typedef ActuationDataAbstractTpl<Scalar> ActuationDataAbstract;
  typedef typename MathBase::VectorXs VectorXs;
  typedef typename MathBase::MatrixXs MatrixXs;

  explicit ActuationModelMultiCopterBaseTpl(boost::shared_ptr<StateMultibody> state, const std::size_t& n_rotors,
                                            const Eigen::Ref<const MatrixXs>& tau_f)
      : Base(state, state->get_nv() - 6 + n_rotors), n_rotors_(n_rotors) {
    pinocchio::JointModelFreeFlyerTpl<Scalar> ff_joint;
    if (state->get_pinocchio()->joints[1].shortname() != ff_joint.shortname()) {
      throw_pretty("Invalid argument: "
                   << "the first joint has to be free-flyer");
    }

    tau_f_ = MatrixXs::Zero(state_->get_nv(), nu_);
    tau_f_.block(0, 0, 6, n_rotors_) = tau_f;
    if (nu_ > n_rotors_) {
      tau_f_.bottomRightCorner(nu_ - n_rotors_, nu_ - n_rotors_) =
          MatrixXs::Identity(nu_ - n_rotors_, nu_ - n_rotors_);
    }
  };
  ~ActuationModelMultiCopterBaseTpl(){};

  void calc(const boost::shared_ptr<ActuationDataAbstract>& data, const Eigen::Ref<const VectorXs>& x,
            const Eigen::Ref<const VectorXs>& u) {
    if (static_cast<std::size_t>(u.size()) != nu_) {
      throw_pretty("Invalid argument: "
                   << "u has wrong dimension (it should be " + std::to_string(nu_) + ")");
    }

    data->tau.noalias() = tau_f_ * u;
  }
  void calcDiff(const boost::shared_ptr<ActuationDataAbstract>& data, const Eigen::Ref<const Eigen::VectorXd>& x,
                const Eigen::Ref<const Eigen::VectorXd>& u) {
    // The derivatives has constant values which were set in createData.
  }

  boost::shared_ptr<ActuationDataAbstract> createData() {
    boost::shared_ptr<ActuationDataAbstract> data = boost::make_shared<ActuationDataAbstract>(this);

    data->dtau_du = tau_f_;

    return data;
  }

  const std::size_t& get_nrotors() const { return n_rotors_; };
  const MatrixXs& get_tauf() const { return tau_f_; };
  void set_tauf(const Eigen::Ref<const MatrixXs>& tau_f) { tau_f_ = tau_f; }

 protected:
  // Specific of multicopter
  MatrixXs tau_f_;  // Matrix from rotors thrust to body force/moments
  std::size_t n_rotors_;

  using Base::nu_;
  using Base::state_;
};

}  // namespace crocoddyl

#endif  // CROCODDYL_MULTIBODY_ACTUATIONS_MULTICOPTER_BASE_HPP_