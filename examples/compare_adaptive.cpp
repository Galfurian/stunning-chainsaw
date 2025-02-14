/// @file compare_adaptive.cpp
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

namespace comparison
{

/// @brief State of the system.
///     x[0] : Angle.
///     x[1] : Velocity.
using State = std::array<Variable, 2>;

/// @brief Parameters of our model.
struct Parameter {
    /// @brief Mass of the rod [kg].
    const Variable mr;
    /// @brief Lenght of the rod [m].
    const Variable l;
    /// @brief Rotational damping [N.m].
    const Variable b;
    /// @brief Gravitational force [N].
    const Variable g;
    /// @brief Rod's moment of inertia about its center of mass.
    const Variable I;
    Parameter(Variable _mr = 3.0, Variable _l = 0.19, Variable _b = 0.1, Variable _g = 9.81)
        : mr(_mr)
        , l(_l)
        , b(_b)
        , g(_g)
        , I((4. / 3.) * _mr * _l * _l)
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
#if 1
        const Variable u = (t < 3) ? 5 : 0;
#else
        const Variable u = .0;
#endif
        // Equation system.
        dxdt[0] = x[1];
        dxdt[1] = (u - mr * g * l * x[0] - b * x[1]) / (I + mr * l * l);
    }
};

template <std::size_t DECIMATION = 0>
struct ObserverSave : public numint::detail::ObserverDecimate<State, Time, DECIMATION> {
    inline void operator()(const State &x, const Time &t) noexcept override
    {
        if (this->observe()) {
            time.emplace_back(t);
            angle.emplace_back(x[0]);
            velocity.emplace_back(x[1]);
        }
    }
    std::vector<Variable> time, angle, velocity;
};

} // namespace comparison

template <class Stepper, class System, class Observer>
inline void run_test_adaptive_step(
    const std::string &name,
    Stepper &stepper,
    Observer &&observer,
    System &&system,
    const typename Stepper::state_type &initial_state,
    typename Stepper::time_type start_time,
    typename Stepper::time_type end_time,
    typename Stepper::time_type delta_time)
{
    stepper.set_min_delta(1e-03);
    stepper.set_max_delta(delta_time);
    stepper.set_tollerance(1e-06);

    // Instantiate the stopwatch.
    timelib::Stopwatch sw;
    // Initialize the state.
    typename Stepper::state_type x = initial_state;
    // Start the stopwatch.
    sw.start();
    // Run the integration.
    numint::integrate_adaptive(stepper, observer, system, x, start_time, end_time, 1e-03);
    // Stop the stopwatch.
    sw.round();
    // Output the info.
    std::cout << "    " << std::setw(16) << name;
    std::cout << " took " << std::setw(12) << stepper.steps() << " steps,";
    std::cout << " for a total of " << sw.last_round() << "\n";
}

int main(int, char **)
{
    using namespace comparison;
    // Instantiate the model.
    Model model;
    // Initial and runtime states.
    const State x0{0.0, 0.0};
    // Simulation parameters.
    const Time start_time = 0.0, end_time = 2.0, delta_time = 0.05;

    // Setup the adaptive solver.
    const auto Iterations = 1;
    const auto Error      = numint::ErrorFormula::Mixed;
    // Instantiate the solvers and observers.
    numint::stepper_adaptive<numint::stepper_euler<State, Time>, Iterations, Error> euler;
    numint::stepper_adaptive<numint::stepper_improved_euler<State, Time>, Iterations, Error> improved_euler;
    numint::stepper_adaptive<numint::stepper_midpoint<State, Time>, Iterations, Error> midpoint;
    numint::stepper_adaptive<numint::stepper_trapezoidal<State, Time>, Iterations, Error> trapezoidal;
    numint::stepper_adaptive<numint::stepper_simpsons<State, Time>, Iterations, Error> simpsons;
    numint::stepper_adaptive<numint::stepper_rk4<State, Time>, Iterations, Error> rk4;
    numint::stepper_adaptive<numint::stepper_rk4<State, Time>, Iterations, Error> reference;

    // Setup the observers.
#ifdef ENABLE_PLOT
    using Observer = ObserverSave<0>;
#else
    using Observer = numint::detail::ObserverPrint<State, Time, 0>;
#endif
    Observer obs_euler;
    Observer obs_improved_euler;
    Observer obs_midpoint;
    Observer obs_trapezoidal;
    Observer obs_simpsons;
    Observer obs_rk4;
    Observer obs_reference;

    // Run the integration.

    std::cout << "\n";
    std::cout << "Running integration...\n";
    run_test_adaptive_step("euler", euler, obs_euler, model, x0, start_time, end_time, delta_time);
    run_test_adaptive_step(
        "improved_euler", improved_euler, obs_improved_euler, model, x0, start_time, end_time, delta_time);
    run_test_adaptive_step("midpoint", midpoint, obs_midpoint, model, x0, start_time, end_time, delta_time);
    run_test_adaptive_step("trapezoidal", trapezoidal, obs_trapezoidal, model, x0, start_time, end_time, delta_time);
    run_test_adaptive_step("simpsons", simpsons, obs_simpsons, model, x0, start_time, end_time, delta_time);
    run_test_adaptive_step("rk4", rk4, obs_rk4, model, x0, start_time, end_time, delta_time);
    run_test_adaptive_step("reference", reference, obs_reference, model, x0, start_time, end_time, 5e-04);

#ifdef ENABLE_PLOT
    // Create a Gnuplot instance.
    gpcpp::Gnuplot gnuplot;

    // Set up the plot with grid, labels, and line widths
    gnuplot.set_title("Comparison of Numerical Methods")
        .set_terminal(gpcpp::terminal_type_t::wxt)
        .set_xlabel("Time (s)")
        .set_ylabel("Angle (radians)")
        .set_grid()
        .set_legend();

    // Plot Euler method
    gnuplot.set_line_width(2)
        .set_plot_type(gpcpp::plot_type_t::lines)  // Line style: lines
        .set_line_type(gpcpp::line_type_t::dotted) // Line style: dotted (":")
        .plot_xy(obs_euler.time, obs_euler.angle, "euler.angle");

    // Plot Improved Euler method
    gnuplot.set_line_width(2)
        .set_plot_type(gpcpp::plot_type_t::lines)  // Line style: lines
        .set_line_type(gpcpp::line_type_t::dashed) // Line style: dashed ("--")
        .plot_xy(obs_improved_euler.time, obs_improved_euler.angle, "improved_euler.angle");

    // Plot Midpoint method
    gnuplot.set_line_width(2)
        .set_plot_type(gpcpp::plot_type_t::lines)    // Line style: lines
        .set_line_type(gpcpp::line_type_t::dash_dot) // Line style: dash-dot ("-.")
        .plot_xy(obs_midpoint.time, obs_midpoint.angle, "midpoint.angle");

    // Plot Trapezoidal method
    gnuplot.set_line_width(2)
        .set_plot_type(gpcpp::plot_type_t::lines)        // Line style: lines
        .set_line_type(gpcpp::line_type_t::dash_dot_dot) // Line style: dash-dot-dot ("-..")
        .plot_xy(obs_trapezoidal.time, obs_trapezoidal.angle, "trapezoidal.angle");

    // Plot Simpson's method
    gnuplot.set_line_width(3)
        .set_plot_type(gpcpp::plot_type_t::lines)    // Line style: lines
        .set_line_type(gpcpp::line_type_t::dash_dot) // Line style: dash-dot ("-.")
        .plot_xy(obs_simpsons.time, obs_simpsons.angle, "simpsons.angle");

    // Plot RK4 method
    gnuplot.set_line_width(2)
        .set_plot_type(gpcpp::plot_type_t::lines) // Line style: lines
        .set_line_type(gpcpp::line_type_t::solid) // Line style: solid ("-")
        .plot_xy(obs_rk4.time, obs_rk4.angle, "rk4.angle");

    // Plot Reference method
    gnuplot.set_line_width(2)
        .set_plot_type(gpcpp::plot_type_t::lines) // Line style: lines
        .set_line_type(gpcpp::line_type_t::solid) // Line style: solid ("-")
        .plot_xy(obs_reference.time, obs_reference.angle, "reference.angle");

    gnuplot.show();
#endif
    return 0;
}
