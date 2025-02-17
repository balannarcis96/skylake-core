# Dev primer
* `[ThreadSafe]` - This method/function is thread safe with respect to the a specific scope. Usually for methods the scope is the object they are defined on.
* `[ThreadUnsafe]` - This method/function is NOT thread safe.
* `[ThreadLocal]` - This method/function operates on thread local data
* `[Callback]` - This method/function is a callback, it is called under specific circumstances as an event handler
* `[Internal]` - This method should've been marked protected or private but the design prevented that so its marked internal.
* `[Util]` - This method is a utility method/function
* `[Compiletime]` - This a compiletime function
* `[System]` - This is a system method/function, it is part of the framework, it relies on system invariants.
* `[Getter]` - This method/function is usually a one-liner reading or producing a result in a constant time O(1)
* `[Setter]` - This method/function is usually a one-liner writing or producing a result then saving it in a constant time O(1)
* `[Coroutine]` - This is a method/function that acts as a coroutine, usually subsequent calls with persistent state are required to finish a particular operation
* `[Query]` - This is a method/function similar to a `[Getter]` but with a higher complexity f(x) >= O(1)
* `[Const]` - This is a constant entry [DO NOT MODIFY]!
* `[Tune]` - This is a tunable compile time value [Change with at most care]!
* `[Invariant]` - An invariant that is held by the code or is required by the code
* `[ATRP]` - ATRP type, is a type to be used with the ATRP idiom ( [API Through Ref/Pointer] idiom )
* `[DOD]` - Data Oriented Design
* `[TestUtil]` - Function/Method/Type used only for testing the underlying lib, system, abstraction etc
* `[Cached]` - Field that holds a cached value
* `[Async]` - Async API (perform main action async)
* `[Allocator]` - Allocator type or API
* `[KPI]` - KPI type or API
* `[Net]` - API that accesses the network
* `[DPDK]` - API that accesses the network using DPDK
* `[SCSP]` - SingleConsummer SingleProducer API - Followed by a description of the consumer and the producer
* `[SCMP]` - SingleConsummer MultiProducer API - Followed by a description of the producer and optionally the consumer
* `[MCSP]` - MultiConsummer SingleProducer API - Followed by a description of the consumer and optionally the producer
* `[MCMP]` - MultiConsummer MultiProducer API  - Optionally followed by a description of the consumer and the producer
* `[Init]` - Function, method or data used only during the initialization phase
* `[Shutdown]` - Function, method or data used only during the shutdown phase
* `[Reset]` - Function, method or data used only during the reset phase
* `[LibInit]` - Requires the parent library to be initialized before calling/using
