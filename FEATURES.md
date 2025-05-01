# Skylake Core library

## Features/components/utilities

- **SLogger**
    - Advanced, serialized logging
    - It serializes all logging data in a raw binary buffer and lets the *current* sink handle it
    - Sink types: `network(udp)`, `stdout`, `file_handle`
    ```cpp
    SDEBUG("SLogger example. Value: {}", value);
    SINFO("SLogger example. Value: {}", value);
    SWARNING("SLogger example. Value: {}", value);
    SERROR("SLogger example. Value: {}", value);
    SFATAL("SLogger example. Value: {}", value);
    ```
- **Assert**
    - Assert utilities
    ```cpp
    SKL_ASSERT(a && b);
    SKL_ASSERT_CRITICAL(a && b);
    SKL_ASSERT_PERMANENT(a && b);
    ```
- **Atomic**
    - Simple std::atomic wrapper for more specific semantics
    ```cpp
    std::relaxed_value<i32> a;
    const auto old = a.exchange(23);
    ```
- **BufferView, Stream**
    - Lightweight utilities to work with raw buffers
    ```cpp
    char buffer[N];
    skl::skl_buffer_view view{buffer};
    ...
    auto& stream = skl::skl_stream::make(view);
    stream.write(23);
    stream.write(23.4);
    stream.write("Text");
    ...
    const auto written = stream.position();
    stream.seek_forward(23);
    ```
- **Epoch**
    - Get epoch time in ms
    ```cpp
    const auto now = skl::get_current_epoch_time();
    ```
- **Vector, FixedVector**
    - Dynamic and fixed, array based vector abstractions with minimal header size
    ```cpp
    using my_fixed_vector_t = skl::skl_fixed_vector<i32, 1024U>;
    using my_fixed_heap_vector_t = skl::skl_fixed_heap_vector<i32, 1024U>;
    ```
- **CircularQueue, FixedCircularQueue**
    - Dynamic and fixed, array based circular queue abstractions with minimal header size
    ```cpp
    using my_fixed_queue_t = skl::skl_circular_queue<i32, 1024U>;
    ```
- **Def**
    - Macros and more macros
    ```cpp
    class Asd {
        SKL_DEFAULT_COPY_AND_MOVE(Asd);
    };
    ```
- **FSM**
    - *Awesome*, tick based fsm *crafting* toolset
    ```cpp
    // See <skl_fsm_example>
    ```
- **Guid**
    - 4,16 bytes guids
    ```cpp
    const GUID guid = skl::make_guid();
    const SGUID sguid = skl::make_sguid();
    ```
- **Hash**
    - SipHash impl
- **MagicEnum**
    - The magic enum header only library - https://github.com/Neargye/magic_enum
- **Report**
    - Report API - in a SCMP fashion, produce and collect binary reports of any kind
- **ThreadLocal**
    - Declare, define and use thread local singletons (better than just thread_local for non trivial types)
    ```cpp
    class MyThreadLocalState {
        ...
        i32 value;
    };
    SKL_MAKE_TLS_SINGLETON(MyThreadLocalState, g_thread_state);
    ...
    g_thread_state::tls_guarded().value = 23;
    ```
- **Result/Status**
    - API level status code + data(optional) return type `skl_status` / `skl_result<T>`
    ```cpp
    skl_status do_smth() {
        // return SKL_ERR_FAIL;
        return SKL_SUCCESS;
    }
    skl_result<i32> produce_value(i32 f_in) {
        if(f_in < 0) {
            return skl_fail{SKL_ERR_INPUT};
        }
        return f_in * 2;
    }
    ...
    const auto result = do_smth();
    if(result.is_failure()) {
        ...
    }
    ...
    const auto result = produce_value(input);
    if(result.is_failure()) {
        ...
    }
    const auto value = result.value();
    ...
    const auto result = produce_value(input);
    const auto value = result.value_or(0); //Default on failure
    ```
- **Signal**
    - Register program epilog (abnormal or normal) handlers `register_epilog_handler([]() { ... })`
- **Socket**
    - Stongly typed, tcp/udp socket api (typed wrappers on top of socket(), recv() etc) (`socket_t`, `alloc_ipv4_tcp_socket()`)
    ```cpp
    auto socket = skl::alloc_ipv4_tcp_socket();
    if(CInvalidSocket == socket) {
        ...
    }
    SKL_ASSERT_PERMANENT(skl::set_sock_blocking(socket, false));
    ...
    ```
- **Sleep**
    - Sleep utilities `skl_sleep(1500)` `skl_precise_sleep(1.5)`
- **Thread**
    - PThread based thread object abstraction (allows for setting numa affinity and more)
- **Timer**
    - Simple monotonic timer object
- **TypeTraits**
    - Several, lightweight type traits abstractions/utilities
