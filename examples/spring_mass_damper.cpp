/// @file spring_mass_damper.cpp
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

namespace spring_mass_damper
{

/// @brief State of the system.
///     x[0] : Position.
///     x[1] : Velocity.
using State = std::array<Variable, 2>;

/// @brief Parameters of our model.
struct Parameter {
    /// @brief Mass [kg].
    Variable m = 5.0;
    /// @brief Spring stiffness [N/m].
    Variable k = 40.0;
    /// @brief Damping constant.
    Variable c = 5;
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
        dxdt[0] = x[1];
        dxdt[1] = -c / m * x[1] - k / m * x[0];
    }
};

/// @brief The dc motor itself.
template <std::size_t DECIMATION = 0>
struct ObserverSave : public numint::detail::ObserverDecimate<State, Time, DECIMATION> {
    inline void operator()(const State &x, const Time &t) noexcept override
    {
        if (this->observe()) {
            time.emplace_back(t);
            position.emplace_back(x[0]);
            velocity.emplace_back(x[1]);
        }
    }
    std::vector<Variable> time, position, velocity;
};

} // namespace spring_mass_damper

int main(int, char **)
{
    using namespace spring_mass_damper;
    // Instantiate the model.
    Model model;
    // Change model's parameters.
    model.m = 4;
    model.c = 1;
    model.k = 2;
    // Runtime state.
    State x_f;
    State x_a;
    // Initial states.
    const State x0{1.0, 0.0};
    // Simulation parameters.
    const Time time_start = 0.0;
    const Time time_end   = 10;
    const Time time_delta = 1e-03;
    // Setup the fixed solver.
    using FixedSolver     = numint::stepper_rk4<State, Time>;
    // Setup the adaptive solver.
    const auto Iterations = 3;
    const auto Error      = numint::ErrorFormula::Mixed;
    using AdaptiveSolver  = numint::stepper_adaptive<numint::stepper_rk4<State, Time>, Iterations, Error>;
    // Instantiate the solvers.
    FixedSolver solver_f;
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
    Observer obs_f;
    Observer obs_a;

    // Instantiate the stopwatch.
    timelib::Stopwatch sw;
    std::cout << std::fixed;
    std::cout << "Simulating...\n";
    // Set the initial state.
    x_f = x0;
    x_a = x0;
    // Start the simulation.
    sw.start();
    // Run the solver.
    numint::integrate_fixed(solver_f, obs_f, model, x_f, time_start, time_end, time_delta);
    // Get the elapsed time.
    sw.round();
    // Run the solver.
    numint::integrate_adaptive(solver_a, obs_a, model, x_a, time_start, time_end, time_delta);
    // Get the elapsed time.
    sw.round();

    std::cout << "\n";
    std::cout << "Integration steps and elapsed times:\n";
    std::cout << "    Fixed solver computed    " << std::setw(12) << solver_f.steps() << " steps, for a total of "
              << sw[0] << "\n";
    std::cout << "    Adaptive solver computed " << std::setw(12) << solver_a.steps() << " steps, for a total of "
              << sw[1] << "\n";

#ifdef ENABLE_PLOT
    // Create a Gnuplot instance.
    gpcpp::Gnuplot gnuplot;

    // Set up the plot with grid, labels, and line widths
    gnuplot.set_title("Position and Speed vs Time")
        .set_terminal(gpcpp::terminal_type_t::wxt)
        .set_xlabel("Time (s)")
        .set_ylabel("Position (m) / Speed (m/s)") // Y-axis represents both position and speed
        .set_grid()
        .set_legend();

    // Plot Position F (m)
    gnuplot
        .set_line_width(2)                        // Line width
        .set_plot_type(gpcpp::plot_type_t::lines) // Line style
        .plot_xy(obs_f.time, obs_f.position, "Position F (m)");

    // Plot Position A (m)
    gnuplot
        .set_line_width(2)                        // Line width
        .set_plot_type(gpcpp::plot_type_t::lines) // Line style
        .plot_xy(obs_a.time, obs_a.position, "Position A (m)");

    // Plot Speed F (m/s) with dashed line style
    gnuplot
        .set_line_width(1)                         // Line width
        .set_plot_type(gpcpp::plot_type_t::lines)  // Line style
        .set_line_type(gpcpp::line_type_t::dashed) // Dashed line style
        .plot_xy(obs_f.time, obs_f.velocity, "Speed F (m/s)");

    // Plot Speed A (m/s) with dashed line style
    gnuplot
        .set_line_width(1)                         // Line width
        .set_plot_type(gpcpp::plot_type_t::lines)  // Line style
        .set_line_type(gpcpp::line_type_t::dashed) // Dashed line style
        .plot_xy(obs_a.time, obs_a.velocity, "Speed A (m/s)");

    // Show the plot
    gnuplot.show();

#endif
    return 0;
}