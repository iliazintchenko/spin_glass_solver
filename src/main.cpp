// HPX includes (before boost)
#include <hpx/hpx_init.hpp>
#include <hpx/hpx.hpp>
#include <hpx/include/iostreams.hpp>

// Boost includes
#include <boost/program_options.hpp>

// STL includes
#include <string>
#include <iostream>
#include <cassert>

// Solver related includes
#include "hamiltonian.hpp"
#include "result.hpp"
#include "sa_solver.hpp"

#define HPX_APPLICATION_STRING "Spin Glass Solver"
// Configure application-specific options
boost::program_options::options_description desc("Usage: " HPX_APPLICATION_STRING " [options]");

// "D:\Code\spin_glass_solver\testdata\Instances128Spins\lattice\128random0.lat" 
// "D:\Code\spin_glass_solver\testdata\Instances128Spins\results\128random0.lat" 10000 0.1 3.0 100
// # infile=D:\Code\spin_glass_solver\testdata\Instances128Spins\lattice\128random0.lat Ns=10000 beta0=0.100000 beta1=3.000000 num_rep=100

//----------------------------------------------------------------------------
int main(int argc, char* argv[])
{

  // Input: H is the Hamiltonian of the spin glass
//        beta0 is the starting inverse temperature (use 0.1)
//        beta1 is the ending inverse temperature (use 3.0)
//        Ns is the number of Monte Carlo sweeps per run of SA
//        seed initialized the random number generator, use a different one for each repetition
// Output: Configuration of spins and energy

  desc.add_options()
    ("help,h", "Display command line options help");
  desc.add_options()
    ("input,i",
    boost::program_options::value<std::string>()->default_value("D:\\Code\\spin_glass_solver\\testdata\\Instances128Spins\\lattice\\128random0.lat"),
    "Specify the input file containing lattice with spin configuration");
  desc.add_options()
    ("output,o",
    boost::program_options::value<std::string>()->default_value("D:\\Code\\spin_glass_solver\\testdata\\Instances128Spins\\results\\128random0.lat"),
    "Specify the output file which will contain results of the solver");
  desc.add_options()
    ("Ns,N",
    boost::program_options::value<boost::uint64_t>()->default_value(1000),
    "Specify the number of Monte Carlo sweeps per run of Solver");
  desc.add_options()
    ("beta0,b",
    boost::program_options::value<double>()->default_value(0.1),
    "beta0 is the starting inverse temperature");
  desc.add_options()
    ("beta1,e",
    boost::program_options::value<double>()->default_value(3.0),
    "beta1 is the ending inverse temperature");
  desc.add_options()
    ("repetitions,r",
    boost::program_options::value<boost::uint64_t>()->default_value(1000),
    "The number of repetitions to perform of the random solve");

  return hpx::init(desc, argc, argv);
}

//----------------------------------------------------------------------------
// Note: int main() is used by HPX and our main entry is hpx_main()
//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
// 
//----------------------------------------------------------------------------
int hpx_main(boost::program_options::variables_map& vm)
{
  hpx::id_type here                     = hpx::find_here();
  uint64_t rank                         = hpx::naming::get_locality_id_from_id(here);
  std::string name                      = hpx::get_locality_name();
  uint64_t nranks                       = hpx::get_num_localities().get();
  std::size_t current                   = hpx::get_worker_thread_num();
  std::vector<hpx::id_type> remotes     = hpx::find_remote_localities();
  std::vector<hpx::id_type> localities  = hpx::find_all_localities();
  //
  char const* msg = "hello world from OS-thread %1% on locality %2% rank %3% hostname %4%";
  std::cout << (boost::format(msg) % current % hpx::get_locality_id() % rank % name.c_str()) << std::endl;


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

  const std::string infile  = vm["input"].as<std::string>();
  const std::string outfile = vm["output"].as<std::string>();
  const unsigned Ns         = vm["Ns"].as<boost::uint64_t>();
  const double beta0        = vm["beta0"].as<double>();
  const double beta1        = vm["beta1"].as<double>();
  const unsigned num_rep    = vm["repetitions"].as<boost::uint64_t>();

  if (vm.count("help")) {
    if (rank==0) {
      std::cout << desc << std::endl;
    }
    return hpx::finalize();
  }
  const hamiltonian_type H(infile);

  std::ofstream out(outfile);

  out
    << "# infile=" + infile
    + " Ns=" + std::to_string(Ns)
    + " beta0=" + std::to_string(beta0)
    + " beta1=" + std::to_string(beta1)
    + " num_rep=" + std::to_string(num_rep)
    << std::endl;


  for (unsigned rep = 0; rep < num_rep; ++rep)
    out << solve(H, beta0, beta1, Ns, rep);

  out.close();

  return hpx::finalize();
}

