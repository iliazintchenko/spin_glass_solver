#define HPX_APPLICATION_STRING "spinsolve"
// HPX includes (before boost)
#include <hpx/hpx_init.hpp>
#include <hpx/hpx.hpp>
#include <hpx/include/iostreams.hpp>

// Boost includes
#include <boost/program_options.hpp>
#include <boost/lexical_cast.hpp>

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
//
#include "CommandCapture.h"
//
//----------------------------------------------------------------------------
// example command line
//----------------------------------------------------------------------------
// "c:\Program Files\MPICH2\bin\mpiexec.exe" -localonly 1 bin\Debug\spinsolve.exe --repetitions=5 -N 2 -b 0.1 -e 3.0 -o D:\build\spinglass\test.txt
// use --help to get this program help
// use --hpx:help to get help on all options including hpx options

//----------------------------------------------------------------------------
// Global vars and defs, Temporary, will be moved into classes...
//----------------------------------------------------------------------------
namespace spinsolver {
  boost::program_options::options_description desc("Usage: " HPX_APPLICATION_STRING " [options]");
  //
  hpx::id_type              here;                     
  uint64_t                  rank;                         
  std::string               name;                      
  uint64_t                  nranks;                       
  std::size_t               current;                   
  std::size_t               os_threads;          
  std::vector<hpx::id_type> remotes;     
  std::vector<hpx::id_type> localities;
  //
  // on each node, we have one solver_manager instance
  solver_manager            scheduler;
}

//----------------------------------------------------------------------------
// Create solver wrapper and register it with the runtime
//----------------------------------------------------------------------------
int initialize_solver_wrapper(const hamiltonian_type &H)
{
  // useful vars that each node can keep a copy of
  spinsolver::here        = hpx::find_here();
  spinsolver::rank        = hpx::naming::get_locality_id_from_id(spinsolver::here);
  spinsolver::name        = hpx::get_locality_name();
  spinsolver::nranks      = hpx::get_num_localities().get();
  spinsolver::current     = hpx::get_worker_thread_num();
  spinsolver::os_threads  = hpx::get_os_thread_count();
  spinsolver::remotes     = hpx::find_remote_localities();
  spinsolver::localities  = hpx::find_all_localities();
  // setup the solver manager
  spinsolver::scheduler.initialize(H);
  //
  char const* msg = "Creating Solver Wrapper from OS-thread %1% on locality %2% rank %3% hostname %4%";
  std::cout << (boost::format(msg) % spinsolver::current % hpx::get_locality_id() % spinsolver::rank % spinsolver::name.c_str()) << std::endl;
  return 1;
}

// Define the boilerplate code necessary for the function 'initialize_solver_wrapper'
// to be invoked as an HPX action (by a HPX future). This macro defines the
// type 'initialize_solver_wrapper_action'.
HPX_PLAIN_ACTION(initialize_solver_wrapper, initialize_solver_wrapper_action);

//----------------------------------------------------------------------------
// Runs SA with many inputfiles
// Args : beta0 is the starting temperature of SA (use 0.1 for bimodal instances)
//        beta1 is the ending temperature of SA (use 3.0 for bimodal instances)
//        Ns is the number of Monte Carlo Sweeps within each run of SA
//        num_rep is the number of repetitions of SA (with different seeds)
//        inputfile is the filename describing the Hamiltonian H() of the spin glass
//        outputfile is the file of Configuration of spins and energy
//
// Output: All written into outputfile (Energy and spin configuration for each repetition)
//
// Note: Seed of SA is the repetition number
//----------------------------------------------------------------------------
int hpx_main(boost::program_options::variables_map& vm)
{
  hpx::id_type here                     = hpx::find_here();
  uint64_t rank                         = hpx::naming::get_locality_id_from_id(here);
  std::string name                      = hpx::get_locality_name();
  uint64_t nranks                       = hpx::get_num_localities().get();
  std::size_t current                   = hpx::get_worker_thread_num();
  std::size_t const os_threads          = hpx::get_os_thread_count();
  std::vector<hpx::id_type> remotes     = hpx::find_remote_localities();
  std::vector<hpx::id_type> localities  = hpx::find_all_localities();

  const char *command_1 = "hostname";
  std::vector<const char*> command_list;
  std::vector<std::string> result;
  command_list.push_back(command_1);
  ExecuteAndCapture(command_list, result, 30.0);
  for (auto & str_return : result) {
    std::cout << str_return.c_str() << std::endl;
  }

  if (rank!=0) {
    // all the slave nodes need to do is wait for work requests to come in
    // so exit and allow the hpx runtime to do its thing
     // register the solver wrapper with a unique name
    std::cout << "exiting HPX main from rank " << boost::lexical_cast<std::string>(rank).c_str() << std::endl;
    return 0;
  }

  //
  // get command line params
  //
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
      std::cout << spinsolver::desc << std::endl;
    }
    return hpx::finalize();
  }

  //
  // Load the hamiltonian
  // @todo, add support for multiple solvers with their own H
  //
  const hamiltonian_type H(infile);

  // for each locality, trigger the initialize_solver action so that all ranks are initialized
  // and ready to receive work. For now we pass the Hamiltonian as a parameter, but
  // when we start multiple solvers with different H's we will change this
  typedef initialize_solver_wrapper_action action_type;
  std::vector<hpx::future<action_type::result_type>> futures;
  for (auto l : localities) {
    futures.push_back(hpx::async<action_type>(l,H));
  }
  // don't go any further until all localities have finished their initialization
  hpx::wait_all(futures);
  futures.clear();

  // every node has registered its own copy of the solver manager, we need the Ids on each node
  // so that we can invoke remote calls on them
  std::vector<hpx::future<hpx::id_type>> _SolverIdForRank;
  // for each locality, find the handle of the solver_manager and store it for later use.
  for (auto l : localities) {
    _SolverIdForRank.push_back(
      hpx::agas::resolve_name("/solver_wrapper/" + boost::lexical_cast<std::string>(hpx::naming::get_locality_id_from_id(l)))
    );
  }
  // unwrap the futures to get a vector of Ids
  std::vector<hpx::id_type> SolverIdForRank = hpx::util::unwrapped(_SolverIdForRank);

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

  // get a pointer to the local object that was created on this locality
  solver_manager::solver_ptr wrappedSolver = spinsolver::scheduler.getSolver();
  wrappedSolver->setSolverIds(SolverIdForRank);

  // start timer
  std::chrono::time_point<std::chrono::system_clock> start_calc, end_calc, start_io, end_io;
  start_calc = std::chrono::system_clock::now();

  // on locality 0 we will act as master and execute the solver via the wrapper
  // ranks will receive requests for solve operations from rank 0
  solver_manager::result_type x;
  if (rank==0) {
    x = wrappedSolver->spawn(num_rep, beta0, beta1, Ns);
  }

  // stop timer
  end_calc = std::chrono::system_clock::now();
  std::chrono::duration<double> elapsed_seconds = end_calc-start_calc;
  std::cout << "CSVData "             
            << ", Ns, "               << Ns
            << ", num_rep, "          << num_rep
            << ", nodes, "            << nranks 
            << ", threads, "          << os_threads 
            << ", Calculation_time, " << elapsed_seconds.count() << std::endl;

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

//----------------------------------------------------------------------------
// int main just sets up program options and then hands control to HPX
// our main entry is hpx_main()
//
// Caution. HPX adds a lot of command line options and some have the same shortcuts 
// (one char representation) - avoid using them until we work out the correct way
// to handle clashes/duplicates.
//
//----------------------------------------------------------------------------
int main(int argc, char* argv[])
{
  spinsolver::desc.add_options()
    ("help,h", "Display command line options help");
  spinsolver::desc.add_options()
    ("input,i",
    boost::program_options::value<std::string>()->default_value(SPINSOLVE_SOURCE_DIR "/testdata/Instances128Spins/lattice/128random0.lat"),
    "Specify the input file containing lattice with spin configuration");
  spinsolver::desc.add_options()
    ("output,o",
    boost::program_options::value<std::string>()->default_value(SPINSOLVE_BINARY_DIR "/128random0.lat"),
    "Specify the output file which will contain results of the solver");
  spinsolver::desc.add_options()
    ("Ns,N",
    boost::program_options::value<uint64_t>()->default_value(1000),
    "Specify the number of Monte Carlo sweeps per run of Solver");
  spinsolver::desc.add_options()
    ("beta0,b",
    boost::program_options::value<double>()->default_value(0.1),
    "beta0 is the starting inverse temperature");
  spinsolver::desc.add_options()
    ("beta1,e",
    boost::program_options::value<double>()->default_value(3.0),
    "beta1 is the ending inverse temperature");
  spinsolver::desc.add_options()
    ("repetitions,r",
    boost::program_options::value<uint64_t>()->default_value(1000),
    "The number of repetitions to perform of the random solve");
  spinsolver::desc.add_options()
    ("complexity,c",
    boost::program_options::value<double>()->default_value(1),
    "A logarithmic estimate of the computational requirements of a single solve step\n"
    "This figure is used as a helper to decide how to split up repetitions/iterations between threads\n"
    );

  // Initialize and run HPX, 
  // we want to run hpx_main on all all localities so that each can initialize
  // the stuff needed for the solver manager etc.
  // This command line option is added to tell hpx to run hpx_main on all localities and not just rank 0
  std::vector<std::string> cfg;
  cfg.push_back("hpx.run_hpx_main!=1");
  return hpx::init(spinsolver::desc, argc, argv, cfg);
}

