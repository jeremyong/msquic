QUIC_REGISTRATION_CONFIG structure
======

The structure used to configure the execution context of a application registration.

# Syntax

```C
typedef struct QUIC_REGISTRATION_CONFIG {
    const char* AppName;
    uint8_t* AffinityMask;
    uint8_t AffinityMaskLength;
    uint16_t MaxWorkers;
    QUIC_EXECUTION_PROFILE ExecutionProfile;
} QUIC_REGISTRATION_CONFIG;
```

# Members

`AppName`

An optional (may be `NULL`), null-terminated string describing the application that created the registration. This field is primarily used for debugging purposes.

`AffinityMask`

An optional (may be `NULL`) contiguous range of bytes indicating which CPU processors the runtime may schedule threads on. The lowest addressable bit in the range corresponds to CPU 0.

The affinity mask is ignored if the execution profile selected doesn't affinitize worker threads (e.g. as is the case for `QUIC_EXECUTION_PROFILE_TYPE_MAX_THROUGHPUT` and `QUIC_EXECUTION_PROFILE_REAL_TIME`).

Note that when workers are spawned, the affinity mask is traversed destructively.

`AffinityMaskLength`

If an `AffinityMask` is supplied, this field must be set to the length of the affinity mask. This is ignored if `AffinityMask` is `NULL`.

`MaxWorkers`

An optional (may be 0) value that restricts the number of workers the runtime is allowed to spawn. Each worker corresponds to two threads that may be pinned to a core to respond to kernel IO completion requests and perform packet processing and other transport level work.

If an `AffinityMask` is also supplied, the number of workers spawned will be the max of `MaxWorkers` and the number of bits set in `AffinityMask`.

`ExecutionProfile`

Provides a hint to MsQuic on how to optimize its thread scheduling operations.

**Value** | **Meaning**
------ | ------
**QUIC_EXECUTION_PROFILE_LOW_LATENCY**<br>0 | Indicates that scheduling should be generally optimized for reducing response latency. *The default execution profile.*
**QUIC_EXECUTION_PROFILE_TYPE_MAX_THROUGHPUT**<br>1 | Indicates that scheduling should be optimized for maximum single connection throughput.
**QUIC_EXECUTION_PROFILE_TYPE_SCAVENGER**<br>2 | Indicates that minimal responsiveness is required by the scheduling logic. For instance, a background transfer or process.
**QUIC_EXECUTION_PROFILE_TYPE_REAL_TIME**<br>3 | Indicates responsiveness is of paramount importance to the scheduler.

# See Also

[RegistrationOpen](RegistrationOpen.md)<br>
