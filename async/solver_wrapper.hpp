#ifndef __SOLVER_WRAPPER_H__
#define __SOLVER_WRAPPER_H__

#include <hpx/hpx.hpp>
//
#include <hpx/lcos/local/mutex.hpp>
#include <hpx/lcos/local/shared_mutex.hpp>
//
#include <vector>
#include <utility>
#include <tuple>
#include <queue>
#include <cmath>
#include <map>

//#define RDMAHELPER_DISABLE_LOGGING 1
#include "RdmaLogging.h"
//
// This class represents a single solver type that has been wrapped 
// via the template parameter into an HPX callable layer 
// so that it can be invoked by remote localities.
// (It is not necessary to wrap the solver to run it on hpx threads
// if they are local, but we wish to spawn them on many nodes).
//
// Note that for each solver 'type' (class) we must create a new wrapper.
//
template <class T>
struct wrapped_solver_class : hpx::components::simple_component_base<wrapped_solver_class<T>>
{
    // each solve step will return a future, create a queue for them
    // note: the futures are HPX_MOVABLE_BUT_NOT_COPYABLE
    typedef hpx::future<typename T::result_type> future_type;
    typedef std::queue<future_type> task_queue;
    //
    // the output of this solver wrapper is a vector of solutions
    typedef std::vector<typename T::result_type> result_type;
    //
    typedef hpx::lcos::local::shared_mutex solver_mutex_type;

    // this is the internal solver we are wrapping
    T                                   _theSolver;
    double                              _complexity;
    uint64_t                            _rank;
    uint64_t                            _nranks;
    std::size_t                         _os_threads;
    bool                                _abort;
    solver_mutex_type                   _solver_id_mutex;
    std::vector<hpx::id_type>           _solver_ids;
    std::map<hpx::id_type, int>         _solver_counter;
    std::atomic<bool>                   _new_solver;
    std::map<hpx::id_type, task_queue>  _async_results;
    result_type                         _repetition_results_vector;
    int                                 _total_completed;

    // provide a constructor, passing Args through to the internal class
    template <typename ...Args>
    wrapped_solver_class(Args&&... args) : _theSolver(std::forward<Args>(args)...) {
        _abort = false;
        get_hpx_info();
    };

    ~wrapped_solver_class() {
    }

    // for scheduling, we store info about threads/ranks
    void get_hpx_info() {
        _rank        = hpx::naming::get_locality_id_from_id(hpx::find_here());
        _os_threads  = hpx::get_os_thread_count();
    }

    // The complexity is a logarithmic estimate of the time taken by a
    // single solve step and is used to guide the solver wrapper towards
    // finding a good balance on how many threads to launch
    // low complexity=fast solution
    // high complexity=slow solution
    void setComplexity(double c) { _complexity = c; }

    // utility, get nth argument type and value
    template <size_t i, typename ...Args>
    struct arg {
        typedef typename std::tuple_element<i, std::tuple<Args...>>::type type;
        type get(Args... args) { return std::get<i>(std::tuple<Args...>(args...)); }
    };

    // deprecated
    void setSolverIds(const std::vector<hpx::id_type> &ids) {
        // unique lock for exclusive write access
        LOG_DEBUG_MSG("taking solver_id_mutex in setSolverIds");
        boost::unique_lock<solver_mutex_type> lock(_solver_id_mutex);
        _solver_ids = ids;
        _nranks = _solver_ids.size();
        _new_solver.store(true);
        for (auto s : _solver_ids) {
            _solver_counter[s] = 0;
        }
        std::cout << "After setSolverIds : size is " << _solver_ids.size() << std::endl;
        LOG_DEBUG_MSG("releasing solver_id_mutex in setSolverIds");
    }

    void addSolverId(const hpx::id_type &id) {
        // unique lock for exclusive write access
        LOG_DEBUG_MSG("taking solver_id_mutex in addSolverId");
        boost::unique_lock<solver_mutex_type> lock(_solver_id_mutex);
        _solver_ids.push_back(id);
        _nranks = _solver_ids.size();
        _new_solver.store(true);
        _solver_counter[id] = 0;
        std::cout << "After addSolverId : size is " << _solver_ids.size() << std::endl;
        LOG_DEBUG_MSG("releasing solver_id_mutex in addSolverId");
    }

    // if any of the threads have completed, pull them off the queue
    // @todo, if one thread takes much longer, finished jobs will be stuck in the queue
    // so we must only use threads which are from the same solver params for now
    // shared lock for read access
    static void remove_completed_solver_steps(wrapped_solver_class<T> *self)
    {
        while (hpx::is_running() && !self->_abort) {
            // shared lock solver_ids for read access
            LOG_DEBUG_MSG("taking solver_id_mutex in remove_completed_solver_steps");
            boost::shared_lock<solver_mutex_type> lock(self->_solver_id_mutex);
            for (auto s : self->_solver_ids) {
                std::queue<future_type> &tasks = self->_async_results[s];
                while (!tasks.empty()) {
                    future_type &fut = tasks.front();
                    if (fut.is_ready()) {
                        // the future completed, so put the result on our return vector
                        // @todo, we should perform IO as soon as the thread completes
                        // the solve step.
                        self->_repetition_results_vector.push_back(std::move(fut.get()));
                        // and pop the completed future
                        tasks.pop();
                        self->_total_completed++;
                    }
                    else {
                        break;
                    }
                }
            }
            lock.unlock();
            LOG_DEBUG_MSG("releasing solver_id_mutex in remove_completed_solver_steps");
            hpx::this_thread::sleep_for(boost::chrono::milliseconds(250));
        }
    }

    template <typename ...Args>
    result_type spawn(uint64_t num_reps, Args&&... args)
    {
        // final results go here
        _repetition_results_vector.reserve(num_reps);
        _total_completed = 0;
        //
        // threads should include num Sweeps if complexity is high
        // assume all nodes have same core counts for now
        double num_required_threads = num_reps;
        double num_hardware_threads = _nranks*_os_threads;
        int local_seed_offset = _rank*std::ceil(num_required_threads/_nranks);

        //
        // instantiate an action; we can call this on multiple threads asynchronously
        // on this node, or on remote nodes
        //
        wrapped_solver_class<T>::run_one_action<Args&&..., int> solve_step;

        std::cout
            << "Num_reps is " << num_reps
            << " num sweeps is " << arg<2, Args&&...>().get(std::forward<Args>(args)...)
            << " OS threads is " << _os_threads << std::endl;

        // start a thread which will remove completed steps from the work lists
        hpx::future<void> future_completed = hpx::async(remove_completed_solver_steps, this);

        uint64_t remaining = num_reps;
        double last_remaining = num_reps;
        uint64_t seed = 0;
        const int default_sleep = 100;
        bool futures_waiting = false;
        //
        // Measure time for solves/s
        std::chrono::time_point<std::chrono::system_clock> t_start = std::chrono::system_clock::now();
        int reps_this_loop = 0;
        //
        while (!_abort && (futures_waiting || remaining>0)) {
            // as long as there are repetitions left and not too many launched
            // add new ones to the waiting queue
            //
            // @todo, work on some scheduling to find out what a good launch rate is
            // @todo, traverse list in sorted order so least occupied nodes get first slot

            // shared lock for read access
            bool enough;
            if (remaining>0) do {
                enough = true;
                LOG_DEBUG_MSG("taking sover_id_mutex in spawn loop");
                boost::shared_lock<solver_mutex_type> lock(_solver_id_mutex);
                for (auto s : _solver_ids) {
                    if (_async_results[s].size() < (_os_threads*5)) {
                        future_type fut = hpx::async(solve_step, s, args..., seed + local_seed_offset);
                        _async_results[s].push( std::move(fut) );
                        seed ++;
                        remaining --;
                        enough = false;
                    }
                    if (remaining==0) break;
                }
                LOG_DEBUG_MSG("releasing sover_id_mutex in spawn loop");
            } while (!enough && remaining>0);
            //
            std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
            std::chrono::duration<double> elapsed_seconds = now - t_start;
            t_start = now;

            int _total_remaining = num_reps - _total_completed;
            double solves_this_iteration = (last_remaining - _total_remaining);
            double solves_per_second = (solves_this_iteration)/elapsed_seconds.count();
            last_remaining = _total_remaining;
//            std::cout << "Solves this iteration " << solves_this_iteration << std::endl;
//            std::cout << "Time this iteration " << elapsed << std::endl;
            std::cout << "Solves per second = " << solves_per_second << std::endl;
            hpx::this_thread::sleep_for(boost::chrono::milliseconds(1000));
        }

        std::cout << "Solver Wrapper, waiting for completed thread" << std::endl;
        future_completed.get();

        std::cout << "Solver Wrapper, end of spawn loop " << std::endl;
        // c++11 will move the result to the caller without copying
        return _repetition_results_vector;
    }

    template <typename ...Args>
    typename T::result_type run_one(Args... args) {
        // The solver class is not thread safe, so we cannot run N threads on the same instance
        // create a copy of our internal solver for each new request
        T newSolver(_theSolver);
        // std::cout << "Running a single solve " << std::endl;
        return newSolver.run(args...);
    }

    int abort() {
        _abort = true;
        return 1;
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
// the following code should go in CXX : it must only appear once per action type
// for simplicity we keep it here for the time being
//
typedef hpx::components::simple_component< wrapped_solver_class<sa_solver> > wrapped_solver_type;
typedef wrapped_solver_class<sa_solver> solver_type;

// Define boilerplate required once per component module.
HPX_REGISTER_COMPONENT(wrapped_solver_type, solver_type);

// If this code is part of a library, then we need this registration
// to expose/export the factory creation
//HPX_REGISTER_COMPONENT_MODULE();

#endif
