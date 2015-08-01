// HPX includes (before boost)
#include <hpx/hpx_init.hpp>
#include <hpx/hpx.hpp>
#include <hpx/include/iostreams.hpp>
#include <hpx/util/asio_util.hpp>
#include <hpx/lcos/local/dataflow.hpp>
#include <hpx/lcos/local/mutex.hpp>
#include <hpx/lcos/local/counting_semaphore.hpp>
#include <hpx/lcos/local/no_mutex.hpp>
#include <hpx/lcos/local/shared_mutex.hpp>

// Boost includes
#include <boost/program_options.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

#include <boost/thread/lockable_traits.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/shared_mutex.hpp>

#include <boost/thread/locks.hpp>

// STL includes
#include <string>
#include <iostream>
#include <chrono>
#include <vector>
#include <map>

#include <readers_writer_lock.hpp>
std::atomic<int> readers;
std::atomic<int> writers;

//template <typename Mutex = hpx::lcos::local::shared_mutex>
void lock_test1(hpx::lcos::local::mutex &mutex) {
    boost::unique_lock<hpx::lcos::local::mutex> lock(mutex, boost::defer_lock);

    lock.lock();
    std::cout << "Acquiring the unique lock on thread " << hpx::this_thread::get_id() << std::endl;

    hpx::this_thread::sleep_for(boost::chrono::milliseconds(250));

    std::cout << "Releasing the unique lock on thread " << hpx::this_thread::get_id() << std::endl;
    lock.unlock();
}
//----------------------------------------------------------------------------

void lock_test2(hpx::lcos::local::shared_mutex &mutex) {
    boost::shared_lock<hpx::lcos::local::shared_mutex> lock(mutex, boost::defer_lock);

    lock.lock();
    std::cout << "Acquiring the shared lock on thread " << hpx::this_thread::get_id() << std::endl;

    hpx::this_thread::sleep_for(boost::chrono::milliseconds(250));

    std::cout << "Releasing the shared lock on thread " << hpx::this_thread::get_id() << std::endl;
    lock.unlock();
}
//----------------------------------------------------------------------------

void read_lock(hpx::lcos::local::shared_mutex &mutex, int delay)
{
    readers_writer_lock<hpx::lcos::local::shared_mutex> rw_lock(mutex);
    rw_lock.read_lock();
    readers++;
    std::cout << "Acquiring the read lock on thread " << hpx::this_thread::get_id() << " " << readers << " " << writers << std::endl;
    if (writers>0) {
        throw std::runtime_error("Reader should not take lock while writer has lock");
    }
    hpx::this_thread::sleep_for(boost::chrono::milliseconds(delay));
    readers--;
}

//----------------------------------------------------------------------------

void write_lock(hpx::lcos::local::shared_mutex &mutex, int delay)
{
    readers_writer_lock<hpx::lcos::local::shared_mutex> rw_lock(mutex);
    rw_lock.write_lock();
    writers++;
    std::cout << "Acquiring the write lock on thread " << hpx::this_thread::get_id() << " " << readers << " " << writers << std::endl;
    if (readers>0) {
        throw std::runtime_error("Writer should not take lock while reader has lock");
    }
    if (writers>1) {
        throw std::runtime_error("Number of writers should never exceed 1");
    }
    hpx::this_thread::sleep_for(boost::chrono::milliseconds(delay));
    writers--;
//    std::cout << "Releasing the write lock on thread " << hpx::this_thread::get_id() << " " << writers << std::endl;
}

//----------------------------------------------------------------------------
int hpx_main(boost::program_options::variables_map& vm)
{
    hpx::lcos::local::mutex mutex1;
    hpx::lcos::local::shared_mutex mutex2;
    std::vector<hpx::future<void>> futs;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 8);

    // randomly generate reader and writer lock requests
    // many readers can simultaneously have the lock, but only 1 writer
    // and writer may not have lock at same time as readers
    for (int i=0; i<1000; i++) {
        // don't create as many writers
        if (dis(gen) == 7) {
            futs.push_back(hpx::async(&write_lock, boost::ref(mutex2), dis(gen)));
        }
        hpx::this_thread::sleep_for(boost::chrono::milliseconds(dis(gen)));
        futs.push_back(hpx::async(&read_lock, boost::ref(mutex2), dis(gen)));
    }
    hpx::wait_all(futs);

/*
#if 0
    for (int i=0; i<10; i++) {
        futs.push_back(hpx::async(&lock_test1, boost::ref(mutex1)));
        futs.push_back(hpx::async(&lock_test2, boost::ref(mutex2)));
    }
    hpx::wait_all(futs);
#else
    for (int i=0; i<10; i++) {
        hpx::apply(&lock_test1, boost::ref(mutex1));
        hpx::apply(&lock_test2, boost::ref(mutex2));
    }
#endif
*/
    std::cout << "Finalize " << hpx::this_thread::get_id() << std::endl;
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
    return hpx::init(argc, argv);
}

