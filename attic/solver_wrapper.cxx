#include <hpx/hpx.hpp>
//
#include <vector>
#include <utility>
#include <tuple>
#include <cmath>

class sa_solver {
public:
  sa_solver() {
  }
  ;

  typedef int result_type;

  result_type run(int a) {
    std::cout << "here " << a << std::endl;
    return 0;
  }

};

template<class T>
struct wrapped_solver_class: hpx::components::simple_component_base<wrapped_solver_class<T>> {
// this is the internal solver we are wrapping
  T _theSolver;

// provide a constructor, passing Args through to the internal class
  template<typename ...Args>
  wrapped_solver_class(Args ... args) :
      _theSolver(args...) {
    std::cout << "Entering constructor " << std::endl;
  }
  ;

  ~wrapped_solver_class() {
  }

// provide a default constructor, we need one for the component factory
// which will create instances for remote execution
// wrapped_solver_class() {
// std::cout << "Entering constructor " << std::endl;
// };

// the output of this solver wrapper is a vector of solutions
  typedef std::vector<typename T::result_type> result_type;

  template<typename ...Args>
  result_type spawn(uint64_t num_reps, Args ... args) {
    result_type temp;
    temp.reserve(num_reps);
//
    for (int i = 0; i < num_reps; i++) {
    }

// c++11 will move the result to the caller without copying
    return temp;
  }

  template<typename ... Args>
  typename T::result_type run_one(Args ... args) {
//
    std::cout << "Running a single solve " << std::endl;
    return _theSolver.run(args...);
  }

//
// We wish to execute the run_one function from remote async calls
// so we must wrap it up as an hpx::action for serialization and
// execution by the runtime.
// Normally we would use one of the provided hpx DEFINE_ACTION_MACRO's,
// but as the function is templated over Args...
// it is more readable to declare it directly here.
//
  template<typename ... Args>
  struct run_one_action: hpx::actions::make_action<typename T::result_type (wrapped_solver_class<T>::*)(Args...),
      &wrapped_solver_class::template run_one<Args...>, run_one_action<Args...> > {
  };
};

//
// Declare the necessary component action boilerplate code for actions taking template type arguments.
// It has to be visible in all translation units using the action,
// thus it is recommended to place it into the header file defining the component.
//
HPX_REGISTER_ACTION_DECLARATION_TEMPLATE(
    (template <typename... Args>),
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

// Define boilerplate required once per component module.
// HPX_REGISTER_COMPONENT_MODULE();
// HPX_REGISTER_COMPONENT_MODULE()

int main(int argc, char* argv[]) {
  return hpx::init(argc, argv);
}

int hpx_main(boost::program_options::variables_map& vm) {
  hpx::naming::id_type _agas_Wrapper_id;
  boost::shared_ptr<wrapped_solver_class<sa_solver>> _solver_instance;

  std::cout << "Creating solver_wrapper 3" << std::endl;
  _agas_Wrapper_id = hpx::components::new_ < wrapped_solver_class < sa_solver >> (hpx::find_here()).get();
  std::cout << "Creating solver_wrapper 4" << std::endl;

  int _rank = 0;
  std::cout << "registering solver_wrapper with global name " << "/solver_wrapper/" << boost::lexical_cast < std::string
      > (_rank).c_str() << std::endl;
//
  hpx::agas::register_name_sync("/solver_wrapper/" + boost::lexical_cast < std::string > (_rank), _agas_Wrapper_id);
//
  _solver_instance = hpx::get_ptr_sync < wrapped_solver_class < sa_solver >> (_agas_Wrapper_id);

  return hpx::finalize();
}
