#include <vector>
#include <utility>

// wrapper which will take a function signature and function pointer
template <typename T, T* funcptr> struct wrapped_solver;

template <class T> 
struct wrapped_solver_class 
{
  // this is the internal solver we are wrapping
  T _theSolver;

  // provide a constructor, passing Args through to the internal class
  template <typename ...Args>
  wrapped_solver_class(Args... args) : _theSolver(args...) {};

  // the output of this solver wrapper is a vector of solutions
  typedef std::vector<typename T::result_type> result_type;

  template <typename ...Args>
  result_type spawn(uint64_t num_reps, Args... args) {
    result_type temp;
    temp.reserve(num_reps);
    //
    for (int i=0; i<num_reps; i++) {
       temp.push_back( _theSolver.run(args..., i) );
    }

    // c++11 will move the result to the caller without copying
    return temp;
  }

};
