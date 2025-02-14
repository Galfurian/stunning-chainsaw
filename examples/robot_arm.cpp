/// @file robot_arm.cpp
/// @author Enrico Fraccaroli (enry.frak@gmail.com)
/// @brief
/// @version 0.1
/// @date 2024-04-09
///
/// @copyright Copyright (c) 2024
///

/// @file pendulum.cpp
/// @author Enrico Fraccaroli (enry.frak@gmail.com)
/// @brief

#include <cmath>
#include <exception>
#include <iomanip>
#include <iostream>

#include <timelib/stopwatch.hpp>

#ifdef ENABLE_PLOT
#include <gpcpp/gnuplot.hpp>
#endif

#include "defines.hpp"

#include <numint/detail/observer.hpp>
#include <numint/solver.hpp>
#include <numint/stepper/stepper_adaptive.hpp>
#include <numint/stepper/stepper_euler.hpp>
#include <numint/stepper/stepper_improved_euler.hpp>
#include <numint/stepper/stepper_midpoint.hpp>
#include <numint/stepper/stepper_rk4.hpp>
#include <numint/stepper/stepper_simpsons.hpp>
#include <numint/stepper/stepper_trapezoidal.hpp>

namespace pendulum
{

/// @brief State of the system.
///     x[0] : Angle.
///     x[1] : Velocity.
using State = std::array<Variable, 5>;

/// @brief Parameters of our model.
struct Parameter {
    Variable Fv;    ///< Viscous friction coefficient.
    Variable Fc;    ///< Coulomb friction coefficient.
    Variable Fcs;   ///< Striebeck friction coefficient.
    Variable alpha; ///< Striebeck smoothness coefficient.
    Variable beta;  ///< Friction smoothness coefficient.
    Variable J;     ///< Total moment of inertia.
    Variable am;    ///< Motor moment of inertia scale factor.
    Variable ag;    ///< Gear-box moment of inertia scale factor.
    Variable kg1;   ///< Gear-box stiffness parameter 1.
    Variable kg3;   ///< Gear-box stiffness parameter 3.
    Variable dg;    ///< ear-box damping parameter.
    Variable ka;    ///< rm structure stiffness parameter.
    Variable da;    ///< rm structure damping parameter.

    Parameter()
        : Fv(0.00986346744839)
        , Fc(0.74302635727901)
        , Fcs(3.98628540790595)
        , alpha(3.24015074090438)
        , beta(0.79943497008153)
        , J(0.03291699877416)
        , am(0.17910964111956)
        , ag(0.61206166914114)
        , kg1(20.59269827430799)
        , kg3(0.00000000000000)
        , dg(0.06241814047290)
        , ka(20.23072060978318)
        , da(0.00987527995798)
    {
        // Nothing to do.
    }
};

struct Model : public Parameter {
    Model(Parameter parameter = Parameter())
        : Parameter(parameter)
    {
        // Nothing to do.
    }

    /// @brief DC motor behaviour.
    /// @param x the current state.
    /// @param dxdt the final state.
    /// @param t the current time.
    inline void operator()(const State &x, State &dxdt, Time t) noexcept
    {
        (void)t;
        (void)t;
        double u = 1;

        /* Determine intermediate variables. */
        /* tauf: Gear friction torque. (sech(x) = 1/cosh(x)! */
        /* taus: Spring torque. */
        double tauf = Fv * x[2] + (Fc + Fcs / (std::cosh(alpha * x[2]))) * std::tanh(beta * x[2]);
        double taus = kg1 * x[0] + kg3 * std::pow(x[0], 3);

        /* x[0]: Rotational velocity difference between the motor and the gear-box. */
        /* x[1]: Rotational velocity difference between the gear-box and the arm. */
        /* x[2]: Rotational velocity of the motor. */
        /* x[3]: Rotational velocity after the gear-box. */
        /* x[4]: Rotational velocity of the robot arm. */
        dxdt[0] = x[2] - x[3];
        dxdt[1] = x[3] - x[4];
        dxdt[2] = 1 / (J * am) * (-taus - dg * (x[2] - x[3]) - tauf + u);
        dxdt[3] = 1 / (J * ag) * (taus + dg * (x[2] - x[3]) - ka * x[1] - da * (x[3] - x[4]));
        dxdt[4] = 1 / (J * (1.0 - am - ag)) * (ka * x[1] + da * (x[3] - x[4]));
    }
};

/// @brief The dc motor itself.
template <std::size_t DECIMATION = 0>
struct ObserverSave : public numint::detail::ObserverDecimate<State, Time, DECIMATION> {
    inline void operator()(const State &x, const Time &t) noexcept override
    {
        if (this->observe()) {
            time.emplace_back(t);
            y[0].emplace_back(x[0]);
            y[1].emplace_back(x[1]);
            y[2].emplace_back(x[2]);
            y[3].emplace_back(x[3]);
            y[4].emplace_back(x[4]);
        }
    }

    std::vector<Variable> time;
    std::array<std::vector<Variable>, 5> y;
};

} // namespace pendulum

int main(int, char **)
{
    using namespace pendulum;
    // Instantiate the model.
    Model model;
    // Change model's parameters.
    // Runtime state.
    State x;
    // Initial states.
    const State x0{0.0, 0.0, 0.0, 0.0, 0.0};
    // Simulation parameters.
    const Time time_start = 0.0;
    const Time time_end   = 20;
    const Time time_delta = 1e-03;
    // Setup the adaptive solver.
    const auto Iterations = 3;
    const auto Error      = numint::ErrorFormula::Mixed;
    using AdaptiveSolver  = numint::stepper_adaptive<numint::stepper_rk4<State, Time>, Iterations, Error>;
    // Instantiate the solvers.
    AdaptiveSolver solver_a;
    solver_a.set_tollerance(1e-09);
    solver_a.set_min_delta(1e-12);
    solver_a.set_max_delta(1e-01);

    // Instantiate the observers.
#ifdef ENABLE_PLOT
    using Observer = ObserverSave<0>;
#else
    using Observer = numint::detail::ObserverPrint<State, Time, 0>;
#endif
    Observer obs;

    // Instantiate the stopwatch.
    timelib::Stopwatch sw;
    std::cout << std::fixed;
    std::cout << "Simulating...\n";
    // Set the initial state.
    x = x0;
    // Start the simulation.
    sw.start();
    // Run the solver.
    numint::integrate_adaptive(solver_a, obs, model, x, time_start, time_end, time_delta);
    // Get the elapsed time.
    sw.round();

    std::cout << "\n";
    std::cout << "Integration steps and elapsed times:\n";
    std::cout << "    Adaptive solver computed " << std::setw(12) << solver_a.steps() << " steps, for a total of "
              << sw[0] << "\n";

#ifdef ENABLE_PLOT
    // Create a Gnuplot instance.
    gpcpp::Gnuplot gnuplot;

    // Set up the plot with grid, labels, and line widths
    gnuplot.set_title("Rotational Velocity vs Time")
        .set_terminal(gpcpp::terminal_type_t::wxt)
        .set_xlabel("Time (s)")
        .set_ylabel("Rotational Velocity")
        .set_grid()
        .set_legend();

    // Plot Rotational velocity of the motor
    gnuplot
        .set_line_width(2)                        // Line width
        .set_plot_type(gpcpp::plot_type_t::lines) // Line style
        .plot_xy(obs.time, obs.y[2], "Rotational velocity of the motor");

    // Plot Rotational velocity after the gear-box
    gnuplot
        .set_line_width(2)                        // Line width
        .set_plot_type(gpcpp::plot_type_t::lines) // Line style
        .plot_xy(obs.time, obs.y[3], "Rotational velocity after the gear-box");

    // Plot Rotational velocity of the robot arm
    gnuplot
        .set_line_width(2)                        // Line width
        .set_plot_type(gpcpp::plot_type_t::lines) // Line style
        .plot_xy(obs.time, obs.y[4], "Rotational velocity of the robot arm");

    // Show the plot
    gnuplot.show();

#endif
    return 0;
}