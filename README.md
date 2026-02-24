# nova::non_null

[![CI](https://github.com/timblechmann/nova_nonnull/actions/workflows/ci.yml/badge.svg)](https://github.com/timblechmann/nova_nonnull/actions/workflows/ci.yml)

Non-null pointer adapters and callable wrappers for C++20, with compiler hints for better code generation. Inspired by [`gsl::not_null`](https://github.com/microsoft/GSL/blob/main/include/gsl/pointers), extended with optimization attributes and callable support.

## Components

| Type | Wraps | Notes |
|------|-------|-------|
| `non_null<T*>` | Raw pointer | Assert-checked on construction |
| `non_null<unique_ptr<T>>` | `std::unique_ptr<T>` | Move via `take()` only |
| `non_null<shared_ptr<T>>` | `std::shared_ptr<T>` | Move emulated by copy |
| `non_null_function<Sig>` | `std::function<Sig>` | Move emulated by copy |
| `non_null_move_only_function<Sig>` | `std::move_only_function<Sig>` (C++23) | Move via `take()` only |

## Pointer adapter usage

```cpp
#include <nova/non_null.hpp>

// Raw pointers — assert-checked in debug builds
nova::non_null<int*> p(&value);

// Smart pointer factories
auto u = nova::make_non_null_unique<Foo>(args...);  // non_null<unique_ptr<Foo>>
auto s = nova::make_non_null_shared<Foo>(args...);  // non_null<shared_ptr<Foo>>

// Promote nullable pointer (returns std::nullopt if null)
if (auto opt = nova::try_make_non_null(ptr))
    (*opt)->do_something();

// Transfer ownership out of a unique_ptr wrapper
auto nn2 = nova::non_null(take(std::move(nn1)));  // nn1 must not be used after
```

## non_null_function usage

```cpp
// Wrap any callable — asserts non-empty on construction
nova::non_null_function<int(int)> f = [](int x) { return x * 2; };
int result = f(21);  // 42 — no empty-callable check emitted

// Pass as parameter — callee guaranteed a valid callable
void process(nova::non_null_function<void(int)> callback) {
    callback(42);  // no branch for null check
}

// Move-only callable (C++23)
nova::non_null_move_only_function<void()> g(std::move(unique_callable));
// Extract ownership explicitly:
auto raw = take(std::move(g));
```

## API

**`non_null<T>` members:**

| Member | Notes |
|--------|-------|
| `get()` | Raw pointer; `returns_nonnull` / `_Nonnull` annotated |
| `underlying()` | Stored pointer object (e.g. `unique_ptr`, `shared_ptr`) |
| `*nn` / `nn->` | Dereference / member access |
| `swap(other)` | Exchange; both remain non-null |
| `operator bool()` | Always `true` |
| `operator==`, `operator<=>` | Compare by raw pointer |
| `get_deleter()` | `unique_ptr` only |
| `use_count()`, `owner_before()`, `owner_equal()` | `shared_ptr` only |

**Free functions:**

| Function | Notes |
|----------|-------|
| `take(rhs&&)` | Extracts underlying pointer; rhs must not be used after |
| `swap(lhs, rhs)` | ADL swap |
| `try_make_non_null(p)` | Returns `optional<non_null<T>>`; nullopt if null |
| `make_non_null_unique<T>(args...)` | Like `std::make_unique` |
| `make_non_null_shared<T>(args...)` | Like `std::make_shared` |

**Type aliases:**

| Alias | Equivalent |
|-------|-----------|
| `non_null_unique_ptr<T>` | `non_null<std::unique_ptr<T>>` |
| `non_null_shared_ptr<T>` | `non_null<std::shared_ptr<T>>` |

## Move semantics

Move is **conditionally enabled** based on whether the wrapped type is copyable:

| Type | Move | Rationale |
|------|------|-----------|
| `T*`, `shared_ptr<T>`, `non_null_function` | Allowed | Copyable; move is safe |
| `unique_ptr<T>`, `non_null_move_only_function` | Deleted — use `take()` | Implicit move would leave wrapper empty |

## Requirements

- C++20 (GCC 12+, Clang 17+, MSVC 2022+)
- `non_null_move_only_function` requires C++23
- Header-only; no dependencies

## Build & test

```sh
cmake -B build
cmake --build build
ctest --test-dir build
```

## License

MIT — see [LICENSE](LICENSE)
