# nova::non_null

[![CI](https://github.com/timblechmann/nova_nonnull/actions/workflows/ci.yml/badge.svg)](https://github.com/timblechmann/nova_nonnull/actions/workflows/ci.yml)

A C++20 non-null pointer wrapper with compiler hints for better code generation. Inspired by
[`gsl::non_null`](https://github.com/microsoft/GSL/blob/main/include/gsl/pointers), which
enforces nullability contracts but lacks compiler attributes for optimization.

## Usage

```cpp
#include <nova/non_null.hpp>

// Raw pointers — asserts non-null on construction, checked in debug builds
nova::non_null<int*> p(&value);

// Smart pointers — created via factory functions
nova::non_null<std::unique_ptr<Foo>> u = nova::make_non_null_unique<Foo>(args...);
nova::non_null<std::shared_ptr<Foo>> s = nova::make_non_null_shared<Foo>(args...);

// Safe promotion of nullable pointers
std::optional<nova::non_null<T*>> opt = nova::try_make_non_null(ptr);
if (opt) { /* guaranteed non-null */ }

// Explicit ownership transfer (unique_ptr ownership cannot be copied)
auto nn1 = nova::make_non_null_unique<int>(42);
auto nn2 = nova::non_null(take(std::move(nn1)));  // Safe, explicit extraction
// nn1 is now moved-from; implicit moves are compile errors
```

## API

| Member | Notes |
|--------|-------|
| `get()` | Raw pointer; `returns_nonnull` and `_Nonnull` annotated |
| `underlying()` | Stored pointer object (e.g. `unique_ptr`, `shared_ptr`) |
| `*nn` / `nn->` | Standard dereference / member access |
| `swap(other)` | Exchange managed pointers |
| `operator bool()` | Always `true`; enables `if (nn) { ... }` without branching |
| `operator==`, `operator<=>` | Compare raw pointers |

**Smart pointer-specific APIs** (concept-gated):

| Member | Available for | Notes |
|--------|---------------|-------|
| `get_deleter()` | `unique_ptr` | Access the deleter |
| `use_count()` | `shared_ptr` | Shared ownership count |
| `owner_before()` | `shared_ptr` | Ordering by ownership |
| `owner_equal()` | `shared_ptr` | Equality by owner |

**Free functions:**

| Function | Notes |
|----------|-------|
| `take(rhs&&)` | Explicitly extract underlying pointer (breaks non-null invariant on rhs) |
| `swap(lhs, rhs)` | ADL-found standard swap |

## Move Safety

Move semantics are **conditionally enabled** based on pointer type:

| Pointer Type | Move Supported | Rationale |
|--------------|-----------------|-----------|
| Raw pointers (`T*`) | ✅ Yes | Copyable value type; move is safe |
| `shared_ptr<T>` | ✅ Yes | Reference counting handles move semantics correctly |
| `unique_ptr<T>` | ❌ No (use `take()`) | Move-only; implicit move breaks invariant |

**For copyable types**, move is emulated by copying:

```cpp
auto nn1 = nova::non_null(raw_ptr);
auto nn2 = std::move(nn1);  // Allowed; raw pointer value copied
```

**For move-only types** (`unique_ptr`), use explicit `take()`:

```cpp
auto nn1 = nova::make_non_null_unique<int>(42);
// auto nn2 = std::move(nn1);  // COMPILE ERROR

// Correct — explicit:
auto nn2 = nova::non_null(take(std::move(nn1)));
```

## Requirements

- C++20 (GCC 12+, Clang 17+, MSVC 2022+)
- Header-only; no dependencies

## Build & test

```sh
cmake -B build
cmake --build build
ctest --test-dir build
```

## License

MIT — see [LICENSE](LICENSE)
