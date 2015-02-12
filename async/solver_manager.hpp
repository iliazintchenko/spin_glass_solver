#ifndef __SOLVER_MANAGER_H__
#define __SOLVER_MANAGER_H__

#include <hpx/hpx.hpp>
//
#include <boost/lexical_cast.hpp>
//
#include "sa_solver.hpp"
#include "solver_wrapper.hpp"

class solver_manager {
public:

  double                     _complexity;
  uint64_t                   _rank;
  uint64_t                   _nranks;
  std::size_t                _os_threads;
  std::vector<hpx::id_type>  _remotes;
  std::vector<hpx::id_type>  _localities;
  hpx::naming::id_type       _agas_Wrapper_id;

  typedef wrapped_solver_class<sa_solver>::result_type result_type;
  typedef boost::shared_ptr<wrapped_solver_class<sa_solver>> solver_ptr;
  solver_ptr _solver_instance;

  // for scheduling, we store info about threads/ranks
  void get_hpx_info() {
    _rank        = hpx::naming::get_locality_id_from_id(hpx::find_here());
    _nranks      = hpx::get_num_localities().get();
    _os_threads  = hpx::get_os_thread_count();
    _remotes     = hpx::find_remote_localities();
    _localities  = hpx::find_all_localities();
  }

  // we need to create an instance of the solver_wrapper on every node
  solver_manager(const hamiltonian_type &H) {
    get_hpx_info();
    //
    try {
      _agas_Wrapper_id = hpx::components::new_<wrapped_solver_class<sa_solver>>(hpx::find_here(), H).get();
    }
    catch (std::exception &e) {
      std::cout << "Exception creating solver_wrapper " << std::endl;
      std::cout << e.what() << std::endl;
    }
    // on each node register the solver manager with a unique name
    std::cout << "registering solver_wrapper with global name " << "/solver_wrapper/"
      << boost::lexical_cast<std::string>(_rank).c_str() << std::endl;
    //
    hpx::agas::register_name_sync("/solver_wrapper/" + boost::lexical_cast<std::string>(_rank), _agas_Wrapper_id);
    //
    _solver_instance = boost::dynamic_pointer_cast<wrapped_solver_class<sa_solver>>(
        hpx::get_ptr_sync<wrapped_solver_class<sa_solver>>(_agas_Wrapper_id)
    );
  }

  solver_ptr getSolver() {
    return _solver_instance;
  }

  hpx::naming::id_type getId() {
    return _agas_Wrapper_id;
  }
};

#endif
