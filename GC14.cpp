#include "garbage_collector.h"

#include <iostream>
#include <chrono>
#include <deque>
#include <memory>

// Tests and examples of how to use the garbage collector

#define USE_GC
#define TEST_TREE 1
#define TEST_GRAPH 0

namespace {
# ifdef USE_GC
  gc::god collector;

  template<typename CollectedType>
  gc::heap_ptr<CollectedType> make(CollectedType *obj)
  {
    assert(nullptr != obj);
    return {&::collector, obj};
  }
# endif
  volatile long INNER_LOOP_VAL = 1'000;
}

#ifdef gcnew
# undef gcnew
#endif

#define gcnew ::collector.allocate

class Garbage : public gc::collectable {
# ifdef USE_GC
  std::vector<gc::heap_ptr<Garbage>> more_garbage;
# else
  std::vector<std::shared_ptr<Garbage>> more_garbage;
# endif

public:
  Garbage() = default;
  Garbage(Garbage *init)
  {
    more_garbage.push_back(make(init));
  }

public:
  auto test()
  {
# ifdef USE_GC
    more_garbage.emplace_back(make(gcnew<Garbage>()));
# else
    more_garbage.push_back(std::make_shared<Garbage>());
# endif
    return more_garbage.back();
  }

  Garbage *make_cyclic_reference()
  {
    auto kyp = gcnew<Garbage>(this);
    auto ret = make(kyp);

    for (auto itr : more_garbage)
      itr->make_cyclic_reference();

    more_garbage.push_back(ret);

    return ret.get();
  }
};

class cycle : public gc::collectable {
public:
  gc::heap_ptr<cycle> cirlce_ref;
  cycle() = default;
  cycle(const gc::heap_ptr<cycle> &other) : cirlce_ref(other) { }
  ~cycle() = default;
};

int main(int argc, const char **argv)
{
  ::INNER_LOOP_VAL = std::atoi(argv[1]);

  std::cout.sync_with_stdio(false);

# ifdef USE_GC
  collector.run_thread(std::chrono::milliseconds(100));

  auto fun{
# if (TEST_TREE > 0)
    []() {
    gc::heap_ptr<Garbage> grbg;
    for (int i = 0; i < 1'000; ++i) {
      grbg = gc::heap_ptr<Garbage>{ &collector, gcnew<Garbage>() };
      auto moar_grbg = grbg->test();
      for (int j = 0; j < ::INNER_LOOP_VAL; ++j) {
        moar_grbg = moar_grbg->test();
      }
      //std::this_thread::sleep_for(std::chrono::microseconds(500));
    }
  }
# elif (TEST_GRAPH > 0)
    [] () {
      gc::heap_ptr<Garbage> grbg = make(gcnew<Garbage>());
      auto ffdf = make(grbg->make_cyclic_reference());
    }
# endif
  };

# else
  auto fun{ []() {
    std::shared_ptr<Garbage> bokluk;
    for (int i = 0; i < 1'000; ++i) {
      bokluk = std::make_shared<Garbage>();
      auto moar_bokluk = bokluk->test();
      for (int j = 0; j < ::INNER_LOOP_VAL; ++j) {
        moar_bokluk = moar_bokluk->test();
      }
      //std::this_thread::sleep_for(std::chrono::microseconds(500));
    }
  } };
# endif

  auto start = std::chrono::system_clock::now();
  std::thread th0{ fun };
# if 0
  std::thread th1{ fun };
  std::thread th2{ fun };
# endif

  th0.join();
# if 0
  th1.join();
  th2.join();
# endif
  auto f = (std::chrono::system_clock::now() - start);
  std::cout << f.count() << std::endl;
  //system("pause");
}
