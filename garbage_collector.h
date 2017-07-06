#ifndef __GARBAGE_COLLECTOR_H__
#define __GARBAGE_COLLECTOR_H__

#pragma once

#include <set>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>

#include <cassert>

namespace gc {

  // global gc control constants
  namespace control {
    constexpr std::chrono::nanoseconds minimum_gc_poll_interval{ 500 };
  }


  template<typename ActualType> class heap_ptr;

// Main class, every class which wants to be managed by the garbage
// collector should inherit.
  class collectable {
    friend class god;

  private:
    using ChildrenContainer = typename std::multiset<collectable *>;
    std::atomic_flag children_lock{ false };
    ChildrenContainer children{ nullptr };

  protected:
    struct spinlocker {
      std::atomic_flag& lock;

     ~spinlocker() { lock.clear(std::memory_order_release); }
      spinlocker() = delete;
      spinlocker(std::atomic_flag &flag) : lock{ flag }
      {
        while (lock.test_and_set(std::memory_order_acquire));
      }
    };

  protected:
    template <typename T> friend class heap_ptr;

    inline spinlocker acquire_children_lock() { return children_lock; }

    void erase(collectable *child);
    void insert(collectable *child);

  public: // Used for the mark phase(DFS)
    enum {
      white,
      gray,
      black,
      unknown,
    } color{ unknown };

    virtual ~collectable() = default;

    template<typename CollectedType>
    heap_ptr<CollectedType> make(CollectedType *parent);
  };
  using object = collectable *;


  template<typename ActualType> class heap_ptr {
  private:
    object parent{ nullptr };
    ActualType *core{ nullptr };
  public:
    operator ActualType*() { return core; }
    ActualType *operator->() { return core; }
    ActualType *get() { return core; }
  public:
    heap_ptr() = default;
  public:
   ~heap_ptr() { if (nullptr != parent && nullptr != core) parent->erase(core); }
    heap_ptr(object parent, ActualType *obj);
    heap_ptr(const heap_ptr &other);
    heap_ptr(heap_ptr &&other);
  public:
    heap_ptr &operator=(heap_ptr &&other);
    heap_ptr &operator=(const heap_ptr &other);
  };


// ----------------------------------------------------------------------------
// Garbage collector `god` class.
// This is the real garbage collecting entity. Multiple instances of this
// class can be instansiated to pontentially collect totally disjoint sets of
// objects - hence a bit clunky interface. If there is no real need of multiple
// instances a global object should work just fine.
// _Important_! To run the garbage collector thread method `run_thread` must be
// called, the object will accept new objects, but will not clean anything
// unless the thread is run.

  class god final : public collectable {
  public:
// `stop_thread` is generally called on destruction, but may be called manually
    void stop_thread();

    void run_thread(std::chrono::nanoseconds poll_interval,
      std::chrono::nanoseconds minimum_poll_interval = control::minimum_gc_poll_interval);

    ~god() override;

// Implementation
  private:
    inline spinlocker acquire_new_objects_lock() { return new_objects_lock; }

  private:
    std::atomic_flag new_objects_lock;
    std::vector<collectable *> new_objects;

    std::vector<collectable *> objects;

    std::atomic<bool> sweeping{ false };
    std::atomic<bool> marking{ false };

    std::thread th;
    std::atomic<bool> suspend_thread{ false };

    void transfer_new_objects_to_objects_list();

  private: // Main garbage collection algorithm
    // Method mark returns the number of
    // reachable items from the root
    long mark();
    // Method sweep iterates over all objects and
    // deletes(synchronously) the ones not found by mark
    void sweep();


  public:
    template<typename ObjectType, typename... Arguments,
      typename = typename std::enable_if<std::is_base_of<collectable, ObjectType>::value>::type>
    ObjectType *allocate(Arguments&&... Params)
    {
      auto *ret = new ObjectType{ std::forward<Arguments>(Params)... };

      { // Ensure lock gets freed as soon as possible
        auto lock = acquire_new_objects_lock();
        new_objects.push_back(ret);
      }

      return ret;
    }

  };
}

// ============================================================================
// Needed to include the heap_ptr constructors and operator overload implementations
#include "heap_ptr_impl.h"

#endif // __GARBAGE_COLLECTOR_H__
