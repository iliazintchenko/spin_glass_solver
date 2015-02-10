#define HPX_APPLICATION_STRING "spinsolve"
// HPX includes (before boost)
#include <hpx/hpx_init.hpp>
#include <hpx/hpx.hpp>
#include <hpx/include/iostreams.hpp>

// Boost includes
#include <boost/program_options.hpp>

// STL includes
#include <string>
#include <iostream>
#include <chrono>

// Solver related includes
#include "hamiltonian.hpp"
#include "result.hpp"
#include "sa_solver.hpp"

// Wrapping solver in an HPX framework
#include "solver_wrapper.h"

//----------------------------------------------------------------------------
// Global vars and defs
//----------------------------------------------------------------------------
boost::program_options::options_description desc("Usage: " HPX_APPLICATION_STRING " [options]");

// example command line
// "c:\Program Files\MPICH2\bin\mpiexec.exe" -localonly 1 bin\Debug\spinsolve.exe --repetitions=5 -N 2 -b 0.125 -e 4.1 -o D:\build\spinglass\test.txt
// use --help to get this program help
// use --hpx:help to get help on all options including hpx options

//----------------------------------------------------------------------------
// int main just sets up program options and then hands control to HPX
// our main entry is hpx_main()
//----------------------------------------------------------------------------
int main(int argc, char* argv[])
{
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
    boost::program_options::value<uint64_t>()->default_value(1000),
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
    boost::program_options::value<uint64_t>()->default_value(1000),
    "The number of repetitions to perform of the random solve");
  desc.add_options()
    ("complexity,c",
    boost::program_options::value<double>()->default_value(1),
    "A logarithmic estimate of the computational requirements of a single solve step\n"
    "This figure is used as a helper to decide how to split up repetitions/iterations between threads\n"
    );

  return hpx::init(desc, argc, argv);
}

//----------------------------------------------------------------------------
// Input: H is the Hamiltonian of the spin glass
//        beta0 is the starting inverse temperature (use 0.1)
//        beta1 is the ending inverse temperature (use 3.0)
//        Ns is the number of Monte Carlo sweeps per run of SA
//        seed initialized the random number generator, use a different one for each repetition
// Output: Configuration of spins and energy
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
  const uint64_t Ns         = vm["Ns"].as<uint64_t>();
  const double beta0        = vm["beta0"].as<double>();
  const double beta1        = vm["beta1"].as<double>();
  const uint64_t num_rep    = vm["repetitions"].as<uint64_t>();
  //
  const double complexity   = vm["complexity"].as<double>();

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

  typedef wrapped_solver_class<sa_solver> wrappedSolver;
  wrappedSolver solver(H);

  // start timer
  std::chrono::time_point<std::chrono::system_clock> start_calc, end_calc, start_io, end_io;
  start_calc = std::chrono::system_clock::now();

  // execute the solver
  wrappedSolver::result_type x = solver.spawn(num_rep, beta0, beta1, Ns);

  // stop timer
  end_calc = std::chrono::system_clock::now();
  std::chrono::duration<double> elapsed_seconds = end_calc-start_calc;
  std::cout << "Calculation time: " << elapsed_seconds.count() << "s\n";

  // start timer
  start_io = std::chrono::system_clock::now();

  // Write out vector to file 
  // @TODO, make each task write out it's own results or collect?
  std::copy(x.begin(), x.end(), std::ostream_iterator<sa_solver::result_type>(out));
  out.close();

  // stop timer
  end_io = std::chrono::system_clock::now();
  elapsed_seconds = end_io-start_io;
  std::cout << "IO time: " << elapsed_seconds.count() << "s\n";
 
 

  return hpx::finalize();
}

