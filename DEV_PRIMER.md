## Naming
 - **PascalCase**: all classes, enums and constexpr values must be named using PascalCase 
    ```cpp
    class MyClass {};
    class IInterface {};
    enum class EObjectType {};
    constexpr u32 CMyConstant = 0U;
 - **snake_case**: all data, members, functions, POD and structs must be named using snake case 
    ```cpp
    struct my_struct_t{};
    my_struct_t my_value{};
    void my_fn(u32 f_arg);
    ```
## Post/Prefixes
 - All parameter names must be prefixed with `f_`
    ```cpp
    void my_fn(u32 f_arg_1, const char* f_str)
    ```
 - All private data member names must be prefixed with `m_`
    ```cpp
    u32 m_member_1;
    my_type_t m_value;
    ```
 - All enum type names must be prefixed wtih `E`
    ```cpp
    enum class ELogType
    enum EMyType
    ```
 - All template parameters names must be prefixed using `_`
    ```cpp
    template<typename _Arg> class MyClass { ... };
    ```
 - All constexpr constant names must be prefixed with `C`
    ```cpp
    constexpr u32 CMyConstant = 0U;
    ```
 - (Recommended) Non class and non enum type names must be postfixed using `_t`
    ```cpp
    using my_alias_t = u32;
    struct my_struct_t {};
    ```
    - the goal is to use `_t` to disambiguate between type names and other snake_case names eg, value names
    - keep in minde that `_t` adds overhead to writing a type name so where the name is obviously a type name, `_t` can be omitted 
        - eg `u32` `u16` lack `_t` for efficiency of writing

## Method/Function commenting
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
* `[SCSP]` - SingleConsummer SingleProducer API - Followed by a description of the consumer and the producer
* `[SCMP]` - SingleConsummer MultiProducer API - Followed by a description of the producer and optionally the consumer
* `[MCSP]` - MultiConsummer SingleProducer API - Followed by a description of the consumer and optionally the producer
* `[MCMP]` - MultiConsummer MultiProducer API  - Optionally followed by a description of the consumer and the producer
* `[Init]` - Function, method or data used only during the initialization phase
* `[Shutdown]` - Function, method or data used only during the shutdown phase
* `[Reset]` - Function, method or data used only during the reset phase
* `[LibInit]` - Requires the parent library to be initialized before calling/using
