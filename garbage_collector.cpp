#include "garbage_collector.h"

#include <stack>
#include <algorithm>

// Helper template
template <typename AtomicValueType> struct atomic_value_holder {
  using ContainerType = typename std::atomic<AtomicValueType>;
  ContainerType &flag;
  atomic_value_holder() = delete;
  atomic_value_holder(ContainerType &init) : flag{ init }
  {
    flag.store(true, std::memory_order_release);
  }
 ~atomic_value_holder() { flag.store(false, std::memory_order_release); }
};

// gc::collectable methods
// ----------------------------------------------------------------------------
void gc::collectable::erase(collectable *child)
{
  assert(nullptr != child);

  auto lock = acquire_children_lock();

  auto del = children.find(child);
  if (children.end() != del)
    children.erase(del);
}

void gc::collectable::insert(collectable *child) {
  assert(nullptr != child);

  auto lock = acquire_children_lock();
  children.insert(child);
}


// gc::god methods
// ----------------------------------------------------------------------------
void gc::god::transfer_new_objects_to_objects_list()
{
  auto lock = acquire_new_objects_lock();

  for (auto itr : new_objects)
    objects.push_back(itr);

  new_objects.clear();
}

long gc::god::mark()
{
  if (sweeping.load(std::memory_order_acquire))
    return -1;

  transfer_new_objects_to_objects_list();

  long marked_objects{ 0 };

  auto marking_lock = atomic_value_holder<bool>(marking);

  // Run DFS on the child objects graph
  std::stack<gc::collectable *> ids;
  ids.push(this);
  do {
    auto current_object = ids.top();
    assert(nullptr != current_object);

    ids.pop();
    current_object->color = gray;

    {
      auto lock = current_object->acquire_children_lock();

      for (auto *itr : current_object->children)
      {
        if ((nullptr != itr) && (white == itr->color))
        {
          ++marked_objects;
          itr->color = gray;
          ids.push(itr);
        }
      }
    }

    current_object->color = black;
    current_object = nullptr;
  } while (ids.size() > 0);

  return marked_objects;
}

static bool object_delete(gc::object obj)
{
  assert(nullptr != obj);

  switch (obj->color) {
  case gc::collectable::white:
    delete obj;
    return true;
    break;
  case gc::collectable::gray:
  case gc::collectable::black:
  default:
    obj->color = gc::collectable::white;
    return false;
    break;
  }
}

void gc::god::sweep()
{
  if (marking.load(std::memory_order_acquire))
    return;

  auto sweeping_lock = atomic_value_holder<bool>(sweeping);

  auto erase_from = std::remove_if(objects.begin(), objects.end(), object_delete);
  objects.erase(erase_from, objects.end());
}

void gc::god::run_thread(std::chrono::nanoseconds poll_interval,
    std::chrono::nanoseconds minimum_poll_interval)
{
  th = std::thread([this](std::chrono::nanoseconds maximum_poll_interval,
                          std::chrono::nanoseconds minimum_poll_interval)
  {
    auto current_poll_interval = maximum_poll_interval;
    auto last_marked = 0;

    do {
      auto marked = mark();

      if ((0 < marked) && (objects.size() != marked)) {
        sweep();
      } else if (0 == last_marked) {
        last_marked = 0;
        current_poll_interval = (current_poll_interval + maximum_poll_interval) / 2;
        continue;
      }

      if ((0 == last_marked) && (0 != marked)) {
        last_marked = marked;
        current_poll_interval = (current_poll_interval + minimum_poll_interval) / 2;
        continue;
      }

      // TODO: the algorithm for determining the current_poll_interval
      // value needs reworking to something more advanced
      if (marked >= last_marked) {
        current_poll_interval = (current_poll_interval) / 6;
      } else {
        current_poll_interval = (current_poll_interval) * 4;
      }

      if (current_poll_interval > maximum_poll_interval) {
        current_poll_interval = maximum_poll_interval;
      } else if (current_poll_interval < minimum_poll_interval) {
        current_poll_interval = minimum_poll_interval;
      }

      last_marked = marked;

      std::this_thread::sleep_for(current_poll_interval);

    } while (!suspend_thread.load(std::memory_order_acquire));
  }, poll_interval, gc::control::minimum_gc_poll_interval);
}

void gc::god::stop_thread()
{
  suspend_thread.store(true, std::memory_order_release);
  th.join();
}

gc::god::~god()
{
  stop_thread();

  for (auto itr : objects)
    delete itr;
}
