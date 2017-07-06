#ifndef __HEAP_PTR_IMPL__
#define __HEAP_PTR_IMPL__

#pragma once

namespace gc {
  template<typename ActualType>
  heap_ptr<ActualType>::heap_ptr(gc::object parent, ActualType *obj)
    : parent{ parent }
    , core{ obj }
  {
    assert(nullptr != parent);
    this->parent->insert(obj);
  }

  template<typename ActualType>
  heap_ptr<ActualType>::heap_ptr(const heap_ptr<ActualType> &other)
    : parent{ other.parent }
    , core{ other.core }
  {
    assert(nullptr != parent);
    parent->insert(core);
  }

  template<typename ActualType>
  heap_ptr<ActualType>::heap_ptr(gc::heap_ptr<ActualType> &&other)
    : parent{ std::move(other.parent) }
    , core{ std::move(other.core) }
  {
    assert(nullptr != parent);
    other.core = nullptr;
  }

  template<typename ActualType>
  heap_ptr<ActualType> &gc::heap_ptr<ActualType>::operator=(gc::heap_ptr<ActualType> &&other)
  {
    assert(nullptr != other.parent);

    std::swap(parent, other.parent);
    std::swap(core, other.core);

    return *this;
  }

  template<typename ActualType>
  heap_ptr<ActualType> &gc::heap_ptr<ActualType>::operator=(const heap_ptr<ActualType> &other)
  {
    assert(nullptr != other.parent);

    parent = other.parent;
    core = other.core;

    parent->insert(core);

    return *this;
  }

  template<typename CollectedType>
  heap_ptr<CollectedType> collectable::make(CollectedType *obj)
  {
    assert(nullptr != obj);
    return {this, obj};
  }
}

#endif // __HEAP_PTR_IMPL__
