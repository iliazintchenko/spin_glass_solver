/*
 * readers_writer_lock.hpp
 *
 *  Created on: Jul 28, 2015
 *      Author: biddisco
 */

#ifndef SPIN_GLASS_SOLVER_ASYNC_READERS_WRITER_LOCK_HPP_
#define SPIN_GLASS_SOLVER_ASYNC_READERS_WRITER_LOCK_HPP_

#include <hpx/lcos/local/mutex.hpp>
#include <hpx/lcos/local/shared_mutex.hpp>

#include <boost/thread/lockable_traits.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/shared_mutex.hpp>

#include <boost/thread/locks.hpp>

/*
 * readers_writer_lock is a RAII object that can be used to lock a
 * shared mutex for either multiple concurrent readers or a single writer.
 * The constructor does not lock the lock, it must be requested via
 * either read_lock, or write_lock depending upon requirements.
 *
 * example of usage is
 *   declare a mutex
 *      hpx::lcos::local::shared_mutex> mutex;
 *   take read access to a resource
 *      readers_writer_lock<hpx::lcos::local::shared_mutex> rw_lock(mutex);
 *      rw_lock.read_lock();
 *   elsewhere...
 *      readers_writer_lock<hpx::lcos::local::shared_mutex> rw_lock(mutex);
 *      rw_lock.write_lock();
 */

template <typename MutexType = hpx::lcos::local::shared_mutex>
class readers_writer_lock
{
public:
    readers_writer_lock(MutexType &mutex) :
        _mutex(&mutex),
        _shared_lock(mutex, boost::defer_lock),
        _upgrade_lock(mutex, boost::defer_lock),
        _read_lock(this),
        _write_lock(this)
    {
    }

    void unlock() {
        _read_lock.unlock();
        _write_lock.unlock();
    }

    void read_lock() {
        _read_lock.lock();
    }

    void write_lock() {
        _write_lock.lock();
    }

protected:
    typedef struct read_lock_impl
    {
        readers_writer_lock *self;
        read_lock_impl(readers_writer_lock *rwl) : self(rwl) {}
        ~read_lock_impl()
        {
            this->unlock();
        }
        void lock() {
            self->_shared_lock.lock();
        }
        void unlock() {
            if (self->_shared_lock.owns_lock()) {
                self->_shared_lock.unlock();
            }
        }
    } read_lock_impl;

    typedef struct write_lock_impl
     {
         readers_writer_lock *self;
         boost::unique_lock<MutexType> _unique_lock;

         write_lock_impl(readers_writer_lock *rwl) : self(rwl) {}
         ~write_lock_impl()
         {
             this->unlock();
         }
         void lock() {
             self->_upgrade_lock.lock();
             _unique_lock = boost::move(self->_upgrade_lock);
         }
         void unlock()
         {
             self->_upgrade_lock = boost::upgrade_lock<MutexType> (boost::move(_unique_lock));
             if (self->_upgrade_lock.owns_lock()) {
                 self->_upgrade_lock.unlock();
             }
         }

     } write_lock_impl;

private:

    MutexType                               *_mutex;
    boost::shared_lock<MutexType>            _shared_lock;
    boost::upgrade_lock<MutexType>           _upgrade_lock;
    //
    readers_writer_lock::read_lock_impl  _read_lock;
    readers_writer_lock::write_lock_impl _write_lock;

};

#endif /* SPIN_GLASS_SOLVER_ASYNC_READERS_WRITER_LOCK_HPP_ */
