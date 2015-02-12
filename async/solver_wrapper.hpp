#ifndef __SOLVER_WRAPPER_H__
#define __SOLVER_WRAPPER_H__

#include <hpx/hpx.hpp>
//
#include <vector>
#include <utility>
#include <tuple>
#include <queue>
#include <cmath>

template <class T>
struct wrapped_solver_class : hpx::components::simple_component_base<wrapped_solver_class<T>>
{
  // this is the internal solver we are wrapping
  T                         _theSolver;
  double                    _complexity;
  uint64_t                  _rank;
  uint64_t                  _nranks;
  std::size_t               _os_threads;
  std::vector<hpx::id_type> _remotes;
  std::vector<hpx::id_type> _localities;

  // provide a constructor, passing Args through to the internal class
  template <typename ...Args>
  wrapped_solver_class(Args... args) : _theSolver(args...) {
    get_hpx_info();
  };

  ~wrapped_solver_class() {
  }

  // for scheduling, we store info about threads/ranks
  void get_hpx_info() {
    _rank        = hpx::naming::get_locality_id_from_id(hpx::find_here());
    _nranks      = hpx::get_num_localities().get();
    _os_threads  = hpx::get_os_thread_count();
    _remotes     = hpx::find_remote_localities();
    _localities  = hpx::find_all_localities();
  }

  // The complexity is a logarithmic estimate of the time taken by a
  // single solve step and is used to guide the solver wrapper towards
  // finding a good balance on how many threads to launch
  // low complexity=fast solution
  // high complexity=slow solution
  void setComplexity(double c) { _complexity = c; }

  // the output of this solver wrapper is a vector of solutions
  typedef std::vector<typename T::result_type> result_type;

  // utility, get nth argument type and value
  template <size_t i, typename ...Args>
  struct arg {
    typedef typename std::tuple_element<i, std::tuple<Args...>>::type type;
    type get(Args... args) { return std::get<i>(std::tuple<Args...>(args...)); }
  };

  template <typename ...Args>
  result_type spawn(hpx::naming::id_type agas_id, uint64_t num_reps, Args... args) {
    result_type repetition_results_vector;
    repetition_results_vector.reserve(num_reps);
    //
    // @todo, use this complexity value to help break thread spawning into groups of appropriate size
    //
    double complex = std::pow(10,_complexity);
    //
    // threads should include num Sweeps if complexity is high
    double num_required_threads = num_reps;
    double num_hardware_threads = _nranks*_os_threads;
    int local_seed_offset = _rank*std::ceil(num_required_threads/_nranks);

    //
    // instantiate an action; we can call this on multiple threads asynchronously
    // on this node, or on remote nodes
    //
    wrapped_solver_class<T>::run_one_action<Args..., int> solve_step;

    // each solve step will return a future, create a queue for them
    // note: the futures are HPX_MOVABLE_BUT_NOT_COPYABLE
    typedef hpx::future<typename T::result_type> future_type;
    std::queue<future_type> async_results;

    std::cout
        << "Num_reps is " << num_reps
        << " num sweeps is " << arg<2, Args...>().get(args...)
        << " OS threads is " << _os_threads << std::endl;

    uint64_t remaining = num_reps;
    uint64_t seed = 0;
    const int default_sleep = 100;
    while (!async_results.empty() || remaining>0) {
      // as long as there are repetitions left and not too many launched
      // add new ones to the waiting queue
      // Use a guesstimate of how many threads per node to queue at a time
      // @todo, work on some scheduling to find out what a good N is
      const int THREAD_MULTIPLIER = 100;
      while (remaining>0 && async_results.size()<(_os_threads * THREAD_MULTIPLIER)) {
        future_type fut = hpx::async(solve_step, agas_id, args..., seed + local_seed_offset);
        async_results.push( std::move(fut) );
        seed ++;
        remaining --;
      }

      // if any of the threads have completed, pull them off the queue
      // @todo, if one thread takes much longer, finished jobs will be stuck in the queue
      // so we must only use threads which are from the same solver params for now
      bool ok = true;
      while (ok && !async_results.empty()) {
        future_type &fut = async_results.front();
        if (fut.is_ready()) {
          // the future completed, so put the result on our return vector
          repetition_results_vector.push_back(std::move(fut.get()));
          // and pop the completed future
          async_results.pop();
        }
        else {
          hpx::this_thread::sleep_for(boost::chrono::milliseconds(default_sleep));
          ok = false;
        }
      }
    }

    std::cout << "Here end loop " << std::endl;
    // c++11 will move the result to the caller without copying
    return repetition_results_vector;
  }

  template <typename ...Args>
  typename T::result_type run_one(Args... args) {
    // The solver class is not thread safe, so we cannot run N threads on the same instance
    // create a copy of our internal solver for each new request
    T newSolver = _theSolver;
    // std::cout << "Running a single solve " << std::endl;
    return newSolver.run(args...);
  }

  //
  // We wish to execute the run_one function from remote async calls
  // so we must wrap it up as an hpx::action for serialization and
  // execution by the runtime.
  // Normally we would use one of the provided hpx DEFINE_ACTION_MACRO's,
  // but as the function is templated over Args...
  // it is more readable to declare it directly here.
  //
  template <typename ...Args>
  struct run_one_action : hpx::actions::make_action<
    typename T::result_type (wrapped_solver_class<T>::*)(Args...),
      &wrapped_solver_class<T>::template run_one<Args...>, run_one_action<Args...> >
  {};
};

//
// Declare the necessary component action boilerplate code for actions taking template type arguments.
// It has to be visible in all translation units using the action,
// thus it is recommended to place it into the header file defining the component.
//
HPX_REGISTER_ACTION_DECLARATION_TEMPLATE(
    (template <typename ...Args>),
    (wrapped_solver_class<sa_solver>::run_one_action<Args...>)
)

//
// the following code should go in CXX : it must only appear once per action type
// for simplicity we keep it here for the time being
//
typedef hpx::components::simple_component<wrapped_solver_class<sa_solver>> wrapped_solver_type;
typedef wrapped_solver_class<sa_solver> solver_type;

// Define boilerplate required once per component module.
HPX_REGISTER_MINIMAL_COMPONENT_FACTORY(wrapped_solver_type, solver_type);

// If this code is part of a library, then we need this registration
// to expose/export the factory creation
//HPX_REGISTER_COMPONENT_MODULE();

#endif
