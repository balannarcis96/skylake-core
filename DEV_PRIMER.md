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

## Expression Parenthesization (AUTOSAR Style)
Expressions with multiple operators must be parenthesized to eliminate ambiguity and make operator precedence explicit. This follows AUTOSAR C++ guidelines.

**Rules:**
- Parenthesize sub-expressions when multiple operators are present at different precedence levels
- NOT needed for: single operator expressions, standalone values, function calls, array subscripts
- This applies to arithmetic, logical, bitwise, and comparison operators

**Examples:**

✅ **Correct:**
```cpp
// Single operator - NO parentheses needed
const u32 simple = a + b;
const u32 divide = count / 8u;
if (flag) { }
if (value != 0u) { }

// Multiple operators - parentheses required
const u32 result = (a + b) * (c - d);        // Multiplication and addition
const u32 value  = (x * y) + (z / w);        // Different operators, same precedence
const u32 index  = (i * CSize) + offset;     // Multiplication then addition

// Logical - parenthesize when mixing operators
if (flag1 && flag2) { }                      // OK: same operator
if ((a > b) && (c < d)) { }                  // Required: comparison AND logical
if (flag1 || (flag2 && flag3)) { }           // Required: mixing || and &&
if ((a == b) && ((c != d) || (e > f))) { }   // Required: nested mixed operators

// Bitwise
const u32 single = bits & mask;              // OK: single operator
const u32 multi  = (bits & mask1) | (bits2 & mask2);  // Required: mixing & and |
const u32 val    = (value << 8u) | lower;    // Required: shift and OR

// Mixed operators (critical)
if (((a + b) > c) && (d < e)) { }            // Required: arithmetic, comparison, logical
const u32 v = ((x & mask) == 0u) ? a : b;    // Required: bitwise, comparison, ternary
```

❌ **Incorrect:**
```cpp
// Missing parentheses with multiple operators
const u32 result = a + b * c;           // Should be: a + (b * c) or (a + b) * c
const u32 value  = x * y + z;           // Should be: (x * y) + z
const u32 index  = i * CSize + offset;  // Should be: (i * CSize) + offset

// Missing parentheses mixing logical operators
if (flag1 || flag2 && flag3) { }        // Should be: flag1 || (flag2 && flag3)
if (a > b && c < d || e == f) { }       // Should be: ((a > b) && (c < d)) || (e == f)

// Missing parentheses in bitwise operations
const u32 mask = bits & 0xFF | bits2;   // Should be: (bits & 0xFFu) | bits2
const u32 val  = value << 8 | lower;    // Should be: (value << 8u) | lower

// Missing parentheses in mixed expressions
if (a + b > c && d < e) { }             // Should be: ((a + b) > c) && (d < e)
```

**Rationale:**
- Eliminates reliance on operator precedence knowledge
- Prevents bugs from precedence misunderstandings
- Makes code reviewer/maintainer intent crystal clear
- Aligns with safety-critical coding standards (MISRA, AUTOSAR)

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
