#define HPX_APPLICATION_STRING "spinsolve"
// HPX includes (before boost)
#include <hpx/hpx_init.hpp>
#include <hpx/hpx.hpp>
#include <hpx/include/iostreams.hpp>

// Boost includes
#include <boost/program_options.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

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
//
// Test we can execute a shell command and get the result
//
void test_shell_command(const std::string &name, int rank)
{
    std::vector<const char*> command_list;
    command_list.push_back("hostname");
    std::vector<std::string> result = ExecuteAndCapture(command_list, 30.0, false);
    for (auto & str_return : result) {
        std::cout << "Locality " << name.c_str() << " with rank : " << rank << " : ";
        std::cout << str_return.c_str() << std::endl;
    }
    //
    result = ExecuteAndCaptureSSH(command_list, 30.0, "", "", "", "monch.cscs.ch", false);
    for (auto & str_return : result) {
        std::cout << "Locality " << name.c_str() << " using ssh : ";
        std::cout << str_return.c_str() << std::endl;
    }
}

//----------------------------------------------------------------------------
// A monitoring thread which queries the amount of work on a locality
// Experimental for future use.
//----------------------------------------------------------------------------
void stop_monitor(hpx::promise<void> p)
{
    // Set the future to terminate the monitor thread
    p.set_value();
}

//----------------------------------------------------------------------------
hpx::naming::id_type get_counter(std::string &countername, int locality)
{
    std::string counter1 = "/threads{locality#*/total}/idle-rate";
    std::string replacement = "#" + boost::lexical_cast<std::string>(locality);
    boost::replace_all(countername, "#*", replacement);
    //std::cout << "Generated counter name " << final.c_str() << std::endl;
    hpx::naming::id_type id = hpx::performance_counters::get_counter(countername);
    if (!id) {
        std::cout << (boost::format("error: performance counter not found (%s)") % countername) << std::endl;
    }
    return id;
}
//----------------------------------------------------------------------------

int monitor(double runfor, boost::uint64_t pause)
{
#if defined(BOOST_WINDOWS) && HPX_USE_WINDOWS_PERFORMANCE_COUNTERS != 0
    hpx::register_shutdown_function(&uninstall_windows_counters);
#endif
    hpx::promise<void> stop_flag;
    hpx::register_shutdown_function(boost::bind(&stop_monitor, stop_flag));

    boost::int64_t zero_time = 0;
    hpx::future<void> f = stop_flag.get_future();

    uint64_t initial_ranks = hpx::get_num_localities().get();

    hpx::util::high_resolution_timer t;
    while (runfor<0 || t.elapsed()<runfor)
    {
        hpx::state st;
        hpx::runtime* rt = hpx::get_runtime_ptr();
        if (NULL == rt) {
            // we're probably either starting or stopping
            st = hpx::state_running;
        }
        else st = (rt->get_thread_manager().status());

        std::cout << "hpx runtime state is " << (int)st << std::endl;

        // stop collecting data when the runtime is exiting
        bool closing = hpx::threads::threadmanager_is(hpx::state_pre_shutdown);
        if (closing) return 0;
        if (f.is_ready()) return 0;

        uint64_t current_ranks = hpx::get_num_localities().get();
        std::cout << std::flush;
        if (initial_ranks!=current_ranks) {
            std::cout << "\nRanks changed from " << initial_ranks << " to " << current_ranks << std::endl << std::flush;
            initial_ranks = current_ranks;
        }

        // Query the performance counter for each locality.
        std::vector<hpx::id_type> localities  = hpx::find_all_localities();
        for (auto l : localities) {
            int rank = hpx::naming::get_locality_id_from_id(l);
            std::string counter_template = "/threads{locality#*/total}/idle-rate";
            hpx::naming::id_type id = get_counter(counter_template, rank);
            using namespace hpx::performance_counters;
            // get the current counter data and reset it to zero
            counter_value value = stubs::performance_counter::get_value(id, true);

            if (status_is_valid(value.status_))
            {
                if (!zero_time)
                    zero_time = value.time_;

                std::cout << (boost::format("Status ranks : %03i, counter : %s, %04d, %6.1f[s], %5.2f%%\n")
                % rank
                % counter_template
                % value.count_
                % double((value.time_ - zero_time) * 1e-9)
                % (value.value_*0.01));

    #if defined(BOOST_WINDOWS) && HPX_USE_WINDOWS_PERFORMANCE_COUNTERS != 0
                update_windows_counters(value.value_);
    #endif
            }
        }

        // Schedule a wakeup.
        hpx::this_thread::suspend(pause);
    }

    return 0;
}

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
int run_solvers()
{
    return 0;
}

//----------------------------------------------------------------------------
// spawn instances of this program to demonstrate job resizing
// this happens on the local node and is only for testing
//----------------------------------------------------------------------------
int add_nodes(int N)
{
    std::vector<const char*> command_list;
    command_list.push_back("/Users/biddisco/build/spinmaster/bin/spinsolve");
    command_list.push_back("-Ihpx.parcel.port=7910");
    command_list.push_back("--hpx:threads=2");
    command_list.push_back("--hpx:connect");
    ExecuteAndDetach(command_list, true);
    return 0;
}

//----------------------------------------------------------------------------
// execute slurm command to add nodes to running job
// This is for productions use
//----------------------------------------------------------------------------
int add_nodes_slurm(int N)
{
    return 0;
}

//----------------------------------------------------------------------------
// utility function to avoid blocking on cin getline
//----------------------------------------------------------------------------
bool inputAvailable()
{
  struct timeval tv;
  fd_set fds;
  tv.tv_sec = 0;
  tv.tv_usec = 0;
  FD_ZERO(&fds);
  FD_SET(STDIN_FILENO, &fds);
  select(STDIN_FILENO+1, &fds, NULL, NULL, &tv);
  return (FD_ISSET(0, &fds));
}

//----------------------------------------------------------------------------
// read commands from stdin for demo purposes
//----------------------------------------------------------------------------
int poll_stdin()
{
    // Enter the interpreter loop.
    bool abort = false;
    std::string line;
    while (hpx::is_running() && !abort) {
        if (inputAvailable()) {
            std::getline(std::cin, line);

            boost::algorithm::trim(line);

            std::vector<std::string> cmd;
            boost::algorithm::split(cmd, line,
                    boost::algorithm::is_any_of(" \t\n"),
                    boost::algorithm::token_compress_on);

            if (!cmd.empty() && !cmd[0].empty())
            {
                // try to interpret the entered command
                if (cmd[0] == "reset") {
                }
                else if (cmd[0] == "add") {
                    if (cmd.size() == 2) {
                        int N = boost::lexical_cast<int>(cmd[1]);
                        if (N>0) add_nodes(N);
                    }
                }
                else if (cmd[0] == "sadd") {
                    if (cmd.size() == 2) {
                        int N = boost::lexical_cast<int>(cmd[1]);
                        if (N>0) add_nodes_slurm(N);
                    }
                }
                else if (cmd[0] == "query") {
                }
                else if (cmd[0] == "help") {
                    std::cout << "commands are add <n>, query, reset, quit " << std::endl;
                }
                else if (cmd[0] == "quit") {
                    spinsolver::scheduler.abort();
                    abort = true;
                    break;
                }
                else {
                    std::cout << "error: invalid command '"
                            << line << "'" << std::endl;
                }
            }
            std::cout << "\n> ";
        }
        else {
            hpx::this_thread::suspend(500);
        }
    }
    std::cout << "Query thread terminated " << std::endl;
    return 0;
}
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

    int port = boost::lexical_cast<std::size_t>(hpx::get_config_entry("hpx.parcel.port", 0));
    int agas = boost::lexical_cast<std::size_t>(hpx::get_config_entry("hpx.agas.port", 0));
    std::cout << "Rank " << rank << " Using port number " << port << " and agas " << agas << std::endl;

    //test_shell_command(name, rank);

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
    typedef initialize_solver_wrapper_action action_init;
    std::vector<hpx::future<action_init::result_type>> futures;
    for (auto l : localities) {
        futures.push_back(hpx::async<action_init>(l,H));
    }
    // don't go any further until all localities have finished their initialization
    hpx::wait_all(futures);
    futures.clear();

    // instead of executing a normal async call using
    //   hpx::async(monitor, -1, 1000);
    // we will manually create a high-priority thread to poll the performance counters
    // we do this so that we get as much info as possible, but it still can be made to wait
    // until a free work queue is available.
    // @TODO This will be moved into a custom scheduler once it is ready.
    hpx::error_code ec(hpx::lightweight);
    hpx::applier::register_thread_nullary(
            hpx::util::bind(&monitor, -1, 1000),
            "monitor",
            hpx::threads::pending, true, hpx::threads::thread_priority_critical,
            -1, hpx::threads::thread_stacksize_default, ec);

    //
    // create a fire and forget poll stdin thread for input commands
    // for demonstration of node add/remove capabilties
    // this will be replaced by a MongoDB database read thread
    //
    hpx::apply(poll_stdin);

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

    //
    // get a pointer to the local solver scheduler object that was created on this locality
    //
    solver_manager::solver_ptr wrappedSolver = spinsolver::scheduler.getSolver();
    wrappedSolver->setSolverIds(SolverIdForRank);

    // start timer
    std::chrono::time_point<std::chrono::system_clock> start_calc, end_calc, start_io, end_io;
    start_calc = std::chrono::system_clock::now();

    // on locality 0 we will act as master and execute the solver via the wrapper
    // ranks will receive requests for solve operations from master process
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

