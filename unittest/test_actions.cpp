///////////////////////////////////////////////////////////////////////////////
// BSD 3-Clause License
//
// Copyright (C) 2018-2019, LAAS-CNRS, New York University, Max Planck Gesellshaft
// Copyright note valid unless otherwise stated in individual files.
// All rights reserved.
///////////////////////////////////////////////////////////////////////////////

#define BOOST_TEST_NO_MAIN
#define BOOST_TEST_ALTERNATIVE_INIT_API
#include <iterator>
#include <Eigen/Dense>
#include <pinocchio/fwd.hpp>
#include <boost/test/included/unit_test.hpp>
#include <boost/bind.hpp>
#include "crocoddyl/core/action-base.hpp"
#include "crocoddyl/core/actions/lqr.hpp"
#include "crocoddyl/core/actions/unicycle.hpp"
#include "crocoddyl/core/actions/diff-lqr.hpp"
#include "crocoddyl/core/numdiff/action.hpp"
#include "crocoddyl_unittest_common.hpp"

using namespace boost::unit_test;

struct TestTypes {
  enum Type { ActionModelUnicycle, ActionModelLQRDriftFree, ActionModelLQR, NbTestTypes };
  static std::vector<Type> init_all() {
    std::vector<Type> v;
    v.clear();
    for (int i = 0; i < NbTestTypes; ++i) {
      v.push_back((Type)i);
    }
    return v;
  }
  static const std::vector<Type> all;
};
const std::vector<TestTypes::Type> TestTypes::all(TestTypes::init_all());

class ActionModelFactory {
 public:
  ActionModelFactory(TestTypes::Type type) {
    num_diff_modifier_ = 1e4;
    action_model_ = NULL;
    action_type_ = type;

    switch (action_type_) {
      case TestTypes::ActionModelUnicycle:
        nx_ = 3;
        nu_ = 2;
        action_model_ = new crocoddyl::ActionModelUnicycle();
        break;
      case TestTypes::ActionModelLQRDriftFree:
        nx_ = 80;
        nu_ = 40;
        action_model_ = new crocoddyl::ActionModelLQR(nx_, nu_, true);
        break;
      case TestTypes::ActionModelLQR:
        nx_ = 80;
        nu_ = 40;
        action_model_ = new crocoddyl::ActionModelLQR(nx_, nu_, false);
        break;
      default:
        throw std::runtime_error(__FILE__ ": Wrong TestTypes::Type given");
        break;
    }
  }

  ~ActionModelFactory() {
    switch (action_type_) {
      case TestTypes::ActionModelUnicycle:
        crocoddyl_unit_test::delete_pointer((crocoddyl::ActionModelUnicycle*)action_model_);
        break;
      case TestTypes::ActionModelLQRDriftFree:
        crocoddyl_unit_test::delete_pointer((crocoddyl::ActionModelLQR*)action_model_);
        break;
      case TestTypes::ActionModelLQR:
        crocoddyl_unit_test::delete_pointer((crocoddyl::ActionModelLQR*)action_model_);
        break;
      default:
        throw std::runtime_error(__FILE__ ": Wrong TestTypes::Type given");
        break;
    }
    action_model_ = NULL;
  }

  crocoddyl::ActionModelAbstract* get_action_model() { return action_model_; }

  double num_diff_modifier_;
  unsigned int nx_;

 private:
  unsigned int nu_;
  bool driftfree_;
  TestTypes::Type action_type_;
  crocoddyl::ActionModelAbstract* action_model_;
};

//----------------------------------------------------------------------------//

void test_construct_data(TestTypes::Type action_model_type) {
  // create the model
  ActionModelFactory factory(action_model_type);
  crocoddyl::ActionModelAbstract* model = factory.get_action_model();

  // create the corresponding data object
  boost::shared_ptr<crocoddyl::ActionDataAbstract> data = model->createData();
}

void test_calc_returns_state(TestTypes::Type action_model_type) {
  // create the model
  ActionModelFactory factory(action_model_type);
  crocoddyl::ActionModelAbstract* model = factory.get_action_model();

  // create the corresponding data object
  boost::shared_ptr<crocoddyl::ActionDataAbstract> data = model->createData();

  // Generating random state and control vectors
  Eigen::VectorXd x = model->get_state().rand();
  Eigen::VectorXd u = Eigen::VectorXd::Random(model->get_nu());

  // Getting the state dimension from calc() call
  model->calc(data, x, u);

  BOOST_CHECK(data->xnext.size() == model->get_state().get_nx());
  BOOST_CHECK(factory.nx_ == model->get_state().get_nx());
}

void test_calc_returns_a_cost(TestTypes::Type action_model_type) {
  // create the model
  ActionModelFactory factory(action_model_type);
  crocoddyl::ActionModelAbstract* model = factory.get_action_model();

  // create the corresponding data object and set the cost to nan
  boost::shared_ptr<crocoddyl::ActionDataAbstract> data = model->createData();
  data->cost = nan("");

  // Getting the cost value computed by calc()
  Eigen::VectorXd x = model->get_state().rand();
  Eigen::VectorXd u = Eigen::VectorXd::Random(model->get_nu());
  model->calc(data, x, u);

  // Checking that calc returns a cost value
  BOOST_CHECK(!std::isnan(data->cost));
}

void test_partial_derivatives_against_numdiff(TestTypes::Type action_model_type) {
  // create the model
  ActionModelFactory factory(action_model_type);
  crocoddyl::ActionModelAbstract* model = factory.get_action_model();

  // create the corresponding data object and set the cost to nan
  boost::shared_ptr<crocoddyl::ActionDataAbstract> data = model->createData();

  crocoddyl::ActionModelNumDiff model_num_diff(*model);
  boost::shared_ptr<crocoddyl::ActionDataAbstract> data_num_diff = model_num_diff.createData();

  // Generating random values for the state and control
  Eigen::VectorXd x = model->get_state().rand();
  Eigen::VectorXd u = Eigen::VectorXd::Random(model->get_nu());

  // Computing the action derivatives
  model->calcDiff(data, x, u);
  model_num_diff.calcDiff(data_num_diff, x, u);

  // Checking the partial derivatives against NumDiff
  double tol = factory.num_diff_modifier_ * model_num_diff.get_disturbance();
  BOOST_CHECK((data->Fx - data_num_diff->Fx).isMuchSmallerThan(1.0, tol));
  BOOST_CHECK((data->Fu - data_num_diff->Fu).isMuchSmallerThan(1.0, tol));
  BOOST_CHECK((data->Lx - data_num_diff->Lx).isMuchSmallerThan(1.0, tol));
  BOOST_CHECK((data->Lu - data_num_diff->Lu).isMuchSmallerThan(1.0, tol));
  if (model_num_diff.get_with_gauss_approx()) {
    BOOST_CHECK((data->Lxx - data_num_diff->Lxx).isMuchSmallerThan(1.0, tol));
    BOOST_CHECK((data->Lxu - data_num_diff->Lxu).isMuchSmallerThan(1.0, tol));
    BOOST_CHECK((data->Luu - data_num_diff->Luu).isMuchSmallerThan(1.0, tol));
  } else {
    BOOST_CHECK((data_num_diff->Lxx).isMuchSmallerThan(1.0, tol));
    BOOST_CHECK((data_num_diff->Lxu).isMuchSmallerThan(1.0, tol));
    BOOST_CHECK((data_num_diff->Luu).isMuchSmallerThan(1.0, tol));
  }
}

//----------------------------------------------------------------------------//

void register_action_model_unit_tests(TestTypes::Type action_model_type) {
  framework::master_test_suite().add(BOOST_TEST_CASE(boost::bind(&test_construct_data, action_model_type)));
  framework::master_test_suite().add(BOOST_TEST_CASE(boost::bind(&test_calc_returns_state, action_model_type)));
  framework::master_test_suite().add(BOOST_TEST_CASE(boost::bind(&test_calc_returns_a_cost, action_model_type)));
  framework::master_test_suite().add(
      BOOST_TEST_CASE(boost::bind(&test_partial_derivatives_against_numdiff, action_model_type)));
}

bool init_function() {
  for (size_t i = 0; i < TestTypes::all.size(); ++i) {
    register_action_model_unit_tests(TestTypes::all[i]);
  }
  return true;
}

int main(int argc, char** argv) { return ::boost::unit_test::unit_test_main(&init_function, argc, argv); }