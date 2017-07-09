#GC14
-----

##Introduction
GC14 is a C++14 intrusive tracing garbage collector implemented using only the
C++14 standard template library.

##Usage
GC14 consists of 2 header files and a single cpp file, whoever wants to use it
can just drop in into their own project and use it as-is.

###Core concepts
----------------

####namespace `gc`
This is the namespace in which GC14 is implemented entirely.

####class `gc::god`
This is the real garbage collecting entity. Multiple instances of this
class can be instansiated to pontentially collect totally disjoint sets of
objects - hence a bit clunky interface. If there is no real need of multiple
instances a global object should work just fine.
_Important_! The garbage collector runs in a separate thread, to run the
garbage collector thread method `run_thread` must be called, the object will
allocate new objects, but will not clean anything unless the thread is run.

Any instance of the `god` class is the root of an object tree(graph) and will
run a connectivity search of the objects graph to see if any objects are
unreachable from it.

Objects are allocated/managed by the garbage collector using templated method
`allocate`, which should work about the same way as `std::make_shared` works.
Note that objects of types not publicly inheriting class `gc::collectable` can
not be managed by the garbage collector and compilation will fail.

####class `gc::collectable`
Main class. Every type which wants to be managed by the garbage collector
should inherit this publicly.

####class `heap_ptr`
This is a graph wrapper class which connects an allocated object to its `god`
object. Also serves as keeping track of a reference to an object on the stack
in a way similar to `std::shared_ptr`.

##License
GC14 is licensed under the MIT license, a copy of it can be found in file
[LICENSE](LICENSE).
