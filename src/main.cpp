#include <string>
#include <iostream>
#include <cassert>

#include "hamiltonian.hpp"
#include "result.hpp"
#include "sa_solver.hpp"

int main(int argc, char* argv[]) {
// Runs SA with many inputfiles
// Input: beta0 is the starting temperature of SA (use 0.1 for bimodal instances)
//        beta1 is the ending temperature of SA (use 3.0 for bimodal instances)
//        Ns is the number of Monte Carlo Sweeps within each run of SA
//        num_rep is the number of repetitions of SA (with different seeds)
//        inputfile is the filename describing the Hamiltonian
//        outputfile is the file all the results are written
//
// Output: All written into outputfile (Energy and spin configuration for each repetition)
//
// Note: Seed of SA is the repetition number

  assert(argc == 7);

  const std::string infile(argv[1]);
  const std::string outfile(argv[2]);
  const unsigned Ns(std::stoul(argv[3]));
  const double beta0(std::stod(argv[4]));
  const double beta1(std::stod(argv[5]));
  const unsigned num_rep(std::stoul(argv[6]));

  const hamiltonian_type H(infile);

  std::ofstream out(outfile);

  out
    << "# infile=" + infile
    + " Ns=" + std::to_string(Ns)
    + " beta0=" + std::to_string(beta0)
    + " beta1=" + std::to_string(beta1)
    + " num_rep=" + std::to_string(num_rep)
    << std::endl;

  for(unsigned rep = 0; rep < num_rep; ++rep)
    out << solve(H,beta0,beta1,Ns,rep);

  out.close();
  
  return 0;
}
