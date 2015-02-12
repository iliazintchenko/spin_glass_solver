#include "sa_solver.hpp"
#include <chrono>
#include <random>

sa_solver::sa_solver(const hamiltonian_type& H)
  : N_(H.size())
{
  H_ = std::make_shared<hamiltonian_type>(H);
}

result sa_solver::run(
                    const double beta0
                    , const double beta1
                    , const std::size_t Ns
                    , const std::size_t seed
                    )
{
  // use system clock for seeding random number generator
  //auto seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
  
  // instantiate a generator for random numbers
  std::minstd_rand0 linear_congruential_generator;
  linear_congruential_generator.seed(seed);

  // uniform [0,1] binary distribution
  std::uniform_int_distribution<int> binaries(0, 1);
  int dice_roll = binaries(linear_congruential_generator);

  // uniform [0,1) double distribution
  std::uniform_real_distribution<double> realnums(0.0, 1.0);
  double d_roll = realnums(linear_congruential_generator);

  // stop STL from reallocating space as vector grows
  spins_.reserve(N_);

  // generate N random binary states
  for(unsigned i = 0; i < N_; ++i)
    spins_.push_back(binaries(linear_congruential_generator));

  double E = compute_energy();

  for(unsigned s = 0; s < Ns; ++s){
    const double beta(beta0 + (beta1-beta0)/(Ns-1)*s);
    for(unsigned i = 0; i < N_; ++i){
      const double dE(delta_energy(i));
      if(dE <= 0.0 || realnums(linear_congruential_generator) < std::exp(-beta*dE)){
        spins_[i] = spins_[i] ^ 1;
        E += dE;
      }
    }
  }
  
  result res;
  res.E_=E;
  res.spins_.swap(spins_);
  return res;

}

double sa_solver::compute_energy() const
{
  double E(0.0);
  for(unsigned i = 0; i < N_; ++i)
    for(const auto& edge : H_->operator[](i)) {
      bool tmp = 0;
      for(const auto b : edge.first)
        tmp ^= spins_[b];
      E += ((2*tmp-1) * edge.second * (2*int(edge.first.size()%2) - 1))/edge.first.size();
    }

  return E;
}

double sa_solver::delta_energy(const unsigned ind) const
{
  double E(0.0);
  for(const auto& edge : H_->operator[](ind)) {
    bool tmp(0);
    for(const auto b : edge.first)
      tmp ^= spins_[b];
    E += (1-2*tmp) * edge.second * (2*int(edge.first.size()%2) - 1);
  }
  E *= 2;
    
  return E;
}

result solve(const hamiltonian_type& H
                  , const double beta0
                  , const double beta1
                  , const std::size_t Ns
                  , const std::size_t seed) {
  sa_solver solver(H);
  return solver.run(beta0, beta1, Ns, seed);
}
