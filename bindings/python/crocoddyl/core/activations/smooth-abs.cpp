///////////////////////////////////////////////////////////////////////////////
// BSD 3-Clause License
//
// Copyright (C) 2018-2020, LAAS-CNRS, University of Edinburgh
// Copyright note valid unless otherwise stated in individual files.
// All rights reserved.
///////////////////////////////////////////////////////////////////////////////

#include "python/crocoddyl/core/core.hpp"
#include "python/crocoddyl/core/activation-base.hpp"
#include "crocoddyl/core/activations/smooth-abs.hpp"

namespace crocoddyl {
namespace python {

void exposeActivationSmoothAbs() {
  bp::class_<ActivationModelSmoothAbs, bp::bases<ActivationModelAbstract> >(
      "ActivationModelSmoothAbs",
      "Smooth-absolute activation model.\n\n"
      "It describes a smooth representation of an absolute activation (1-norm), i.e.\n"
      "sum^nr_{i=0} sqrt{eps + ||ri||^2}, where ri is the scalar residual for the i constraints,\n."
      "and nr is the dimension of the residual vector.",
      bp::init<int, bp::optional<double> >(bp::args("self", "nr", "eps"),
                                           "Initialize the activation model.\n\n"
                                           ":param nr: dimension of the residual vector\n"
                                           ":param eps: smoothing factor (default: 1.)"))
      .def("calc", &ActivationModelSmoothAbs::calc, bp::args("self", "data", "r"),
           "Compute the smooth-abs function.\n\n"
           ":param data: activation data\n"
           ":param r: residual vector")
      .def("calcDiff", &ActivationModelSmoothAbs::calcDiff, bp::args("self", "data", "r"),
           "Compute the derivatives of a smooth-abs function.\n\n"
           ":param data: activation data\n"
           ":param r: residual vector \n")
      .def("createData", &ActivationModelSmoothAbs::createData, bp::args("self"),
           "Create the smooth-abs activation data.\n\n");
}

}  // namespace python
}  // namespace crocoddyl
