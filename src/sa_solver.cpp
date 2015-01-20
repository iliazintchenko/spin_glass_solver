#include "sa_solver.hpp"

sa_solver::sa_solver(const hamiltonian_type& H)
  : N_(H.size())
  , H_(H)
{
}

result sa_solver::run(
                    const double beta0
                    , const double beta1
                    , const std::size_t Ns
                    , const std::size_t seed
                    )
{
  for(unsigned i = 0; i < N_; ++i)
    spins_.push_back( drand48() < 0.5 ? 0 : 1);

  double E = compute_energy();

  for(unsigned s = 0; s < Ns; ++s){
    const double beta(beta0 + (beta1-beta0)/(Ns-1)*s);
    for(unsigned i = 0; i < N_; ++i){
      const double dE(delta_energy(i));
      if(dE <= 0.0 || drand48() < std::exp(-beta*dE)){
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
    for(const auto& edge : H_[i]){
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
  for(const auto& edge : H_[ind]){
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
