///////////////////////////////////////////////////////////////////////////////
// BSD 3-Clause License
//
// Copyright (C) 2021-2023, LAAS-CNRS, University of Edinburgh, New York
// University,
//                          Heriot-Watt University
// Max Planck Gesellschaft, University of Trento
// Copyright note valid unless otherwise stated in individual files.
// All rights reserved.
///////////////////////////////////////////////////////////////////////////////

#include "crocoddyl/core/utils/exception.hpp"

namespace crocoddyl {

template <typename Scalar>
ControlParametrizationModelNumDiffTpl<Scalar>::
    ControlParametrizationModelNumDiffTpl(boost::shared_ptr<Base> model)
    : Base(model->get_nw(), model->get_nu()),
      model_(model),
      e_jac_(std::sqrt(2.0 * std::numeric_limits<Scalar>::epsilon())) {}

template <typename Scalar>
ControlParametrizationModelNumDiffTpl<
    Scalar>::~ControlParametrizationModelNumDiffTpl() {}

template <typename Scalar>
void ControlParametrizationModelNumDiffTpl<Scalar>::calc(
    const boost::shared_ptr<ControlParametrizationDataAbstract>& data,
    const Scalar t, const Eigen::Ref<const VectorXs>& u) const {
  boost::shared_ptr<Data> data_nd = boost::static_pointer_cast<Data>(data);
  model_->calc(data_nd->data_0, t, u);
}

template <typename Scalar>
void ControlParametrizationModelNumDiffTpl<Scalar>::calcDiff(
    const boost::shared_ptr<ControlParametrizationDataAbstract>& data,
    const Scalar t, const Eigen::Ref<const VectorXs>& u) const {
  boost::shared_ptr<Data> data_nd = boost::static_pointer_cast<Data>(data);
  data->w = data_nd->data_0->w;

  data_nd->du.setZero();
  const Scalar uh_jac = e_jac_ * std::max(1., u.norm());
  for (std::size_t i = 0; i < model_->get_nu(); ++i) {
    data_nd->du(i) += uh_jac;
    model_->calc(data_nd->data_u[i], t, u + data_nd->du);
    data->dw_du.col(i) = data_nd->data_u[i]->w - data->w;
    data_nd->du(i) = 0.;
  }
  data->dw_du /= uh_jac;
}

template <typename Scalar>
boost::shared_ptr<ControlParametrizationDataAbstractTpl<Scalar> >
ControlParametrizationModelNumDiffTpl<Scalar>::createData() {
  return boost::allocate_shared<Data>(Eigen::aligned_allocator<Data>(), this);
}

template <typename Scalar>
void ControlParametrizationModelNumDiffTpl<Scalar>::params(
    const boost::shared_ptr<ControlParametrizationDataAbstract>& data,
    const Scalar t, const Eigen::Ref<const VectorXs>& w) const {
  model_->params(data, t, w);
}

template <typename Scalar>
void ControlParametrizationModelNumDiffTpl<Scalar>::convertBounds(
    const Eigen::Ref<const VectorXs>& w_lb,
    const Eigen::Ref<const VectorXs>& w_ub, Eigen::Ref<VectorXs> u_lb,
    Eigen::Ref<VectorXs> u_ub) const {
  model_->convertBounds(w_lb, w_ub, u_lb, u_ub);
}

template <typename Scalar>
void ControlParametrizationModelNumDiffTpl<Scalar>::multiplyByJacobian(
    const boost::shared_ptr<ControlParametrizationDataAbstract>& data,
    const Eigen::Ref<const MatrixXs>& A, Eigen::Ref<MatrixXs> out,
    const AssignmentOp op) const {
  assert_pretty(is_a_AssignmentOp(op),
                ("op must be one of the AssignmentOp {settop, addto, rmfrom}"));
  MatrixXs J(nw_, nu_);
  switch (op) {
    case setto:
      out.noalias() = A * data->dw_du;
      break;
    case addto:
      out.noalias() += A * data->dw_du;
      break;
    case rmfrom:
      out.noalias() -= A * data->dw_du;
      break;
    default:
      throw_pretty("Invalid argument: allowed operators: setto, addto, rmfrom");
      break;
  }
}

template <typename Scalar>
void ControlParametrizationModelNumDiffTpl<Scalar>::multiplyJacobianTransposeBy(
    const boost::shared_ptr<ControlParametrizationDataAbstract>& data,
    const Eigen::Ref<const MatrixXs>& A, Eigen::Ref<MatrixXs> out,
    const AssignmentOp op) const {
  assert_pretty(is_a_AssignmentOp(op),
                ("op must be one of the AssignmentOp {settop, addto, rmfrom}"));
  MatrixXs J(nw_, nu_);
  switch (op) {
    case setto:
      out.noalias() = data->dw_du.transpose() * A;
      break;
    case addto:
      out.noalias() += data->dw_du.transpose() * A;
      break;
    case rmfrom:
      out.noalias() -= data->dw_du.transpose() * A;
      break;
    default:
      throw_pretty("Invalid argument: allowed operators: setto, addto, rmfrom");
      break;
  }
}

template <typename Scalar>
const boost::shared_ptr<ControlParametrizationModelAbstractTpl<Scalar> >&
ControlParametrizationModelNumDiffTpl<Scalar>::get_model() const {
  return model_;
}

template <typename Scalar>
const Scalar ControlParametrizationModelNumDiffTpl<Scalar>::get_disturbance()
    const {
  return e_jac_;
}

template <typename Scalar>
void ControlParametrizationModelNumDiffTpl<Scalar>::set_disturbance(
    const Scalar disturbance) {
  if (disturbance < 0.) {
    throw_pretty("Invalid argument: "
                 << "Disturbance constant is positive");
  }
  e_jac_ = disturbance;
}

}  // namespace crocoddyl
