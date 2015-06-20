#define HPX_APPLICATION_STRING "spinsolve"
// HPX includes (before boost)
#include <hpx/hpx_fwd.hpp>
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
#include "solver_wrapper.hpp"
#include "solver_manager.hpp"

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
    boost::program_options::value<std::string>()->default_value(SPINSOLVE_SOURCE_DIR "/testdata/Instances128Spins/lattice/128random0.lat"),
    "Specify the input file containing lattice with spin configuration");
  desc.add_options()
    ("output,o",
    boost::program_options::value<std::string>()->default_value(SPINSOLVE_BINARY_DIR "/128random0.lat"),
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

  boost::program_options::variables_map vm; 
  boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc),  vm); 

  const std::string infile  = vm["input"].as<std::string>();
  const std::string outfile = vm["output"].as<std::string>();
  const uint64_t Ns         = vm["Ns"].as<uint64_t>();
  const double beta0        = vm["beta0"].as<double>();
  const double beta1        = vm["beta1"].as<double>();
  const uint64_t num_rep    = vm["repetitions"].as<uint64_t>();
  //
  const double complexity   = vm["complexity"].as<double>();

  int rank=0;

  if (vm.count("help")) {
    if (rank==0) {
      std::cout << desc << std::endl;
    }
    return 1;
  }

  //
  // Load the hamiltonian
  // @todo, add support for multiple solvers with their own H
  //
  const hamiltonian_type H(infile);

  //
  // Prepare putput file
  //
  std::ofstream out(outfile);
  out
    << "# infile=" + infile
    + " Ns=" + std::to_string(Ns)
    + " beta0=" + std::to_string(beta0)
    + " beta1=" + std::to_string(beta1)
    + " num_rep=" + std::to_string(num_rep)
    << std::endl;

  // start timer
  std::chrono::time_point<std::chrono::system_clock> start_calc, end_calc, start_io, end_io;
  start_calc = std::chrono::system_clock::now();

  // execute the solver

  std::vector<result> x;
  x.reserve(num_rep);
  for (int i=0; i<num_rep; ++i) {  
    //
    // create a single local solver instance
    //
    sa_solver solver(H);
    x.push_back(solver.run(beta0, beta1, Ns, i));
  }

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
 
  return 0; 
}

