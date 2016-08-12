
#include <cmath>
#include <memory>

#include "drake/common/drake_path.h"
#include "drake/examples/Pendulum/Pendulum.h"
#include "drake/examples/Pendulum/pendulum_swing_up.h"
#include "drake/solvers/trajectoryOptimization/dircol_trajectory_optimization.h"
#include "drake/systems/cascade_system.h"
#include "drake/systems/controllers/open_loop_trajectory_controller.h"
#include "drake/systems/LCMSystem.h"
#include "drake/systems/plants/BotVisualizer.h"
#include "drake/systems/Simulation.h"
#include "drake/util/drakeAppUtil.h"

using drake::solvers::SolutionResult;

typedef PiecewisePolynomial<double> PiecewisePolynomialType;

int main(int argc, char* argv[]) {
  auto p = make_shared<Pendulum>();

  const int kNumTimeSamples = 21;
  const int kTrajectoryTimeLowerBound = 2;
  const int kTrajectoryTimeUpperBound = 6;

  const Eigen::Vector2d x0(0, 0);
  const Eigen::Vector2d xG(M_PI, 0);

  drake::solvers::DircolTrajectoryOptimization dircol_traj(
      drake::getNumInputs(*p), drake::getNumStates(*p),
      kNumTimeSamples,  kTrajectoryTimeLowerBound,
      kTrajectoryTimeUpperBound);
  drake::examples::pendulum::AddSwingUpTrajectoryParams(
      p, kNumTimeSamples, x0, xG, &dircol_traj);

  const double timespan_init = 4;
  auto traj_init_x = PiecewisePolynomialType::FirstOrderHold(
      {0, timespan_init}, {x0, xG});
  SolutionResult result =
      dircol_traj.SolveTraj(timespan_init, PiecewisePolynomialType(),
                            traj_init_x);
  if (result != SolutionResult::kSolutionFound) {
    std::cerr << "Result is an Error" << std::endl;
    return 1;
  }

  const PiecewisePolynomialType pp_traj =
      dircol_traj.ReconstructInputTrajectory();

  shared_ptr<lcm::LCM> lcm = make_shared<lcm::LCM>();
  if (!lcm->good()) return 1;

  auto visualizer = std::make_shared<drake::BotVisualizer<PendulumState>>(
      lcm, drake::GetDrakePath() + "/examples/Pendulum/Pendulum.urdf",
      DrakeJoint::FIXED);

  auto control = std::make_shared<
    drake::OpenLoopTrajectoryController<Pendulum>>(pp_traj);
  auto traj_sys = drake::cascade(control, p);
  auto sys = drake::cascade(traj_sys, visualizer);

  drake::SimulationOptions options;
  options.realtime_factor = 1.0;
  if (commandLineOptionExists(argv, argv + argc, "--non-realtime")) {
    options.warn_real_time_violation = true;
  }

  PendulumState<double> x0_state = x0;
  drake::runLCM(sys, lcm, 0, kTrajectoryTimeUpperBound, x0_state, options);
  return 0;
}