#ifndef SA_SOLVER_HPP
#define SA_SOLVER_HPP

#include <memory>
#include "hamiltonian.hpp"
#include "result.hpp"

//#define FORCE_HAMILTONIAN_COPY 1

class sa_solver
// Simulated annealing algorithm to find ground state of spin glass
// Input: Hamiltonian of the spin glass (when constructing a class object)
// Ouput: Energy and Spin configuration (accessible via get_config())
//
// run(...) initialized and runs SA
{
public:

  typedef result result_type;

  // empty constructor required by HPX factory create function
  sa_solver() : N_(0) { };

  // Copy constructor we need to take new instances
  sa_solver(const sa_solver &other) {
    N_     = other.N_;
#ifdef FORCE_HAMILTONIAN_COPY
    H_     = std::make_shared<hamiltonian_type>(*other.H_.get());
#else
    H_     = other.H_;
#endif
    spins_ = other.spins_;
  }

  // initialize sa solver with hamiltonian
  sa_solver(const hamiltonian_type&);

  //single run of sa from random initial state on hamiltonian H_
  result run(const double, const double, const std::size_t, const std::size_t);

private:

  //compute total energy
  double compute_energy() const;

  //compute energy change of flipping a spin
  double delta_energy(const unsigned) const;

   std::size_t N_;
   std::shared_ptr<hamiltonian_type> H_;
  
  std::vector<int> spins_;
};

result solve(const hamiltonian_type& H,
             const double beta0,
             const double beta1,
             const std::size_t Ns,
             const std::size_t seed);
// SA solver
// Input: H is the Hamiltonian of the spin glass
//        beta0 is the starting inverse temperature (use 0.1)
//        beta1 is the ending inverse temperature (use 3.0)
//        Ns is the number of Monte Carlo sweeps per run of SA
//        seed initialized the random number generator, use a different one for each repetition
// Output: Configuration of spins and energy



#endif
