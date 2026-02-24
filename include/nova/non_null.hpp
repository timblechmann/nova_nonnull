// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Tim Blechmann

#pragma once

#include <cassert>
#include <functional>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

#if defined( __has_cpp_attribute )
#    if __has_cpp_attribute( assume )
#        define NOVA_ASSUME( expr ) [[assume( expr )]]
#    endif
#endif

#ifndef NOVA_ASSUME
#    if defined( _MSC_VER )
#        define NOVA_ASSUME( expr ) __assume( expr )
#    elif defined( __clang__ )
#        define NOVA_ASSUME( expr ) __builtin_assume( expr )
#    elif defined( __GNUC__ )
#        define NOVA_ASSUME( expr ) ( ( expr ) ? (void)0 : __builtin_unreachable() )
#    else
#        define NOVA_ASSUME( expr )
#    endif
#endif

#if defined( __GNUC__ ) || defined( __clang__ )
#    define NOVA_RETURNS_NONNULL __attribute__( ( returns_nonnull ) )
#else
#    define NOVA_RETURNS_NONNULL
#endif

#if defined( __clang__ )
#    define NOVA_NONNULL _Nonnull
#else
#    define NOVA_NONNULL
#endif

namespace nova {

namespace detail {

template < typename T, typename = void >
struct element_type_trait
{
    using type = std::remove_pointer_t< T >;
};

template < typename T >
struct element_type_trait< T, std::void_t< typename T::element_type > >
{
    using type = typename T::element_type;
};

template < typename T >
using element_type_t = typename element_type_trait< T >::type;

/**
 * @brief Concept: pointer type is copy-constructible and copy-assignable.
 * True for raw pointers and std::shared_ptr, false for std::unique_ptr.
 */
template < typename T >
concept copyable_pointer = std::copy_constructible< T > && std::is_copy_assignable_v< T >;

template < typename T >
constexpr void assume_nonnull( const T& ptr ) noexcept
{
    [[maybe_unused]] const bool is_not_null = ( ptr != nullptr );
    assert( is_not_null && "nova::detail::assume_nonnull: pointer cannot be null" );
    NOVA_ASSUME( is_not_null );
}

template < typename F >
constexpr void assume_not_empty( const F& fn ) noexcept
{
    [[maybe_unused]] const bool is_not_empty = static_cast< bool >( fn );
    assert( is_not_empty && "nova::detail::assume_not_empty: callable cannot be empty" );
    NOVA_ASSUME( is_not_empty );
}

} // namespace detail

// =============================================================================
// non_null
// =============================================================================

/**
 * @brief A simple non-null wrapper for pointers and smart-pointers.
 *
 * @tparam T The pointer type (e.g., T*, std::unique_ptr<T>, std::shared_ptr<T>).
 */
template < typename T >
class non_null
{
public:
    using element_type = detail::element_type_t< T >;
    using pointer      = element_type*;

    template < typename >
    friend class non_null;

    static_assert( std::is_constructible_v< bool, T >,
                   "T must be contextually convertible to bool (to check for null)" );

    /**
     * @brief Constructs a non_null wrapper from a pointer.
     * @param p The pointer to wrap. Must not be null.
     */
    template < typename U >
    constexpr explicit non_null( U&& p )
        requires std::is_convertible_v< U, T >
        :
        ptr_( std::forward< U >( p ) )
    {
        assert( ptr_ != nullptr && "nova::non_null: pointer cannot be null" );
        detail::assume_nonnull( ptr_ );
    }

    /**
     * @brief Constructs a non_null wrapper from another compatible non_null wrapper.
     * @param other The non_null wrapper to copy from.
     */
    template < typename U >
    constexpr non_null( const non_null< U >& other )
        requires std::is_convertible_v< U, T >
        :
        ptr_( other.underlying() )
    {
        detail::assume_nonnull( ptr_ );
    }

    // Move operations are selectively enabled:
    // - For copyable pointers (raw pointers, shared_ptr): move allowed
    // - For move-only pointers (unique_ptr): move deleted; use take() instead
    // This prevents accidental moves of move-only types while enabling efficient
    // moves of copyable types.
    non_null( const non_null& )            = default;
    non_null& operator=( const non_null& ) = default;

    /**
     * @brief Move constructor, enabled only for copyable pointer types.
     * Raw pointers and std::shared_ptr support moves; std::unique_ptr does not.
     */
    constexpr non_null( non_null&& other ) noexcept
        requires detail::copyable_pointer< T >
        :
        ptr_( std::move( other.ptr_ ) )
    {}

    /**
     * @brief Move assignment, enabled only for copyable pointer types.
     * Raw pointers and std::shared_ptr support moves; std::unique_ptr does not.
     */
    constexpr non_null& operator=( non_null&& other ) noexcept
        requires detail::copyable_pointer< T >
    {
        ptr_ = std::move( other.ptr_ );
        return *this;
    }

    // Deleted move operations for move-only pointer types
    non_null( non_null&& )            = delete;
    non_null& operator=( non_null&& ) = delete;

    // Disable null construction and null assignment
    non_null( std::nullptr_t )            = delete;
    non_null& operator=( std::nullptr_t ) = delete;

    /**
     * @brief Explicitly extracts the underlying pointer, consuming the non_null wrapper.
     *
     * After this call the non_null object is in a moved-from state and must not be used
     * (it will be destroyed normally, but accessing it is undefined behaviour).
     *
     * This is the only way to transfer ownership out of a non_null<std::unique_ptr>.
     * For copyable pointer types the result can be re-wrapped immediately:
     *   auto nn2 = non_null( take( std::move(nn1) ) );
     */
    friend constexpr T NOVA_NONNULL take( non_null&& nn ) noexcept
#if defined( __clang__ ) && ( __clang_major__ >= 20 )
        NOVA_RETURNS_NONNULL
#endif
    {
        return std::move( nn.ptr_ );
    }

    /**
     * @brief Swaps the managed pointers of two non_null objects.
     * Both objects remain non-null after the swap.
     */
    constexpr void swap( non_null& other ) noexcept
    {
        using std::swap;
        swap( ptr_, other.ptr_ );
    }

    /**
     * @brief Returns the underlying raw pointer.
     * Semantically equivalent to std::shared_ptr::get().
     * @return The raw pointer.
     */
    constexpr pointer NOVA_NONNULL get() const noexcept NOVA_RETURNS_NONNULL
    {
        detail::assume_nonnull( ptr_ );
        if constexpr ( std::is_pointer_v< T > )
            return ptr_;
        else
            return ptr_.get();
    }

    /**
     * @brief Returns the underlying pointer object (e.g., T* or smart pointer).
     * @return A const reference to the underlying pointer object.
     */
    constexpr const T& underlying() const& noexcept
#if defined( __clang__ ) && ( __clang_major__ >= 20 )
        NOVA_RETURNS_NONNULL
#endif
    {
        detail::assume_nonnull( ptr_ );
        return ptr_;
    }

    T underlying() && noexcept = delete;

    /**
     * @brief Accesses the members of the object pointed to.
     * @return The underlying raw pointer.
     */
    constexpr pointer NOVA_NONNULL operator->() const noexcept NOVA_RETURNS_NONNULL
    {
        return get();
    }

    /**
     * @brief Dereferences the underlying pointer.
     * @return A reference to the pointed-to object.
     */
    constexpr decltype( auto ) operator*() const noexcept
    {
        detail::assume_nonnull( ptr_ );
        return *ptr_;
    }

    /**
     * @brief Explicitly converts the non_null wrapper to its underlying pointer type.
     * @return A const reference to the underlying pointer object.
     */
    constexpr explicit operator const T&() const& noexcept
#if defined( __clang__ ) && ( __clang_major__ >= 20 )
        NOVA_RETURNS_NONNULL
#endif
    {
        detail::assume_nonnull( ptr_ );
        return ptr_;
    }

    /**
     * @brief Always returns true as non_null objects are guaranteed to be non-null.
     * @return true.
     */
    constexpr explicit operator bool() const noexcept
    {
        return true;
    }

    // --- unique_ptr-specific observers (concept-gated) ----------------------------

    /**
     * @brief Returns the deleter associated with the managed pointer.
     * Available only when T provides get_deleter() (e.g. std::unique_ptr).
     */
    constexpr decltype( auto ) get_deleter() noexcept
        requires requires { std::declval< T& >().get_deleter(); }
    {
        return ptr_.get_deleter();
    }

    constexpr decltype( auto ) get_deleter() const noexcept
        requires requires { std::declval< const T& >().get_deleter(); }
    {
        return ptr_.get_deleter();
    }

    // --- shared_ptr-specific observers (concept-gated) ----------------------------

    /**
     * @brief Returns the number of shared_ptr instances sharing ownership.
     * Available only when T is std::shared_ptr.
     */
    constexpr long use_count() const noexcept
        requires requires { std::declval< const T& >().use_count(); }
    {
        return ptr_.use_count();
    }

    /**
     * @brief Owner-based ordering; consistent with std::owner_less.
     * Available only when T is std::shared_ptr.
     */
    template < typename Y >
    constexpr bool owner_before( const non_null< std::shared_ptr< Y > >& other ) const noexcept
        requires requires { std::declval< const T& >().owner_before( std::declval< const std::shared_ptr< Y >& >() ); }
    {
        return ptr_.owner_before( other.underlying() );
    }

    template < typename Y >
    constexpr bool owner_before( const std::shared_ptr< Y >& other ) const noexcept
        requires requires { std::declval< const T& >().owner_before( std::declval< const std::shared_ptr< Y >& >() ); }
    {
        return ptr_.owner_before( other );
    }

    template < typename Y >
    constexpr bool owner_before( const std::weak_ptr< Y >& other ) const noexcept
        requires requires { std::declval< const T& >().owner_before( std::declval< const std::weak_ptr< Y >& >() ); }
    {
        return ptr_.owner_before( other );
    }

    /**
     * @brief Owner-based hashing (C++26).
     * Available only when T is std::shared_ptr.
     */
    constexpr std::size_t owner_hash() const noexcept
        requires requires { std::declval< const T& >().owner_hash(); }
    {
        return ptr_.owner_hash();
    }

    /**
     * @brief Owner-based equal comparison (C++26).
     * Available only when T is std::shared_ptr.
     */
    template < typename Y >
    constexpr bool owner_equal( const non_null< std::shared_ptr< Y > >& other ) const noexcept
        requires requires { std::declval< const T& >().owner_equal( std::declval< const std::shared_ptr< Y >& >() ); }
    {
        return ptr_.owner_equal( other.underlying() );
    }

    template < typename Y >
    constexpr bool owner_equal( const std::shared_ptr< Y >& other ) const noexcept
        requires requires { std::declval< const T& >().owner_equal( std::declval< const std::shared_ptr< Y >& >() ); }
    {
        return ptr_.owner_equal( other );
    }

    template < typename Y >
    constexpr bool owner_equal( const std::weak_ptr< Y >& other ) const noexcept
        requires requires { std::declval< const T& >().owner_equal( std::declval< const std::weak_ptr< Y >& >() ); }
    {
        return ptr_.owner_equal( other );
    }

    /**
     * @brief Compares two non_null wrappers for equality.
     */
    template < typename U >
    friend constexpr bool operator==( const non_null& lhs, const non_null< U >& rhs )
    {
        return lhs.get() == rhs.get();
    }

    /**
     * @brief Performs a three-way comparison between two non_null wrappers.
     */
    template < typename U >
    friend constexpr auto operator<=>( const non_null& lhs, const non_null< U >& rhs )
    {
        return lhs.get() <=> rhs.get();
    }

    /**
     * @brief Compares a non_null wrapper with a compatible pointer for equality.
     */
    template < typename U >
    friend constexpr bool operator==( const non_null& lhs, const U& rhs )
    {
        if constexpr ( std::is_same_v< U, std::nullptr_t > )
            return false;
        else
            return lhs.get() == rhs;
    }

    /**
     * @brief Performs a three-way comparison between a non_null wrapper and a compatible pointer.
     */
    template < typename U >
    friend constexpr auto operator<=>( const non_null& lhs, const U& rhs )
    {
        return lhs.get() <=> rhs;
    }

private:
    T NOVA_NONNULL ptr_;
};

template < typename T >
non_null( T* NOVA_NONNULL ) -> non_null< T* >;

template < typename T, typename D >
non_null( std::unique_ptr< T, D > ) -> non_null< std::unique_ptr< T, D > >;

template < typename T >
non_null( std::shared_ptr< T > ) -> non_null< std::shared_ptr< T > >;

template < typename T >
non_null( non_null< T > ) -> non_null< T >;

/**
 * @brief Swaps the contents of two non_null objects (found via ADL).
 */
template < typename T >
void swap( non_null< T >& lhs, non_null< T >& rhs ) noexcept
{
    lhs.swap( rhs );
}

template < typename T >
using non_null_unique_ptr = non_null< std::unique_ptr< T > >;

template < typename T >
using non_null_shared_ptr = non_null< std::shared_ptr< T > >;

/**
 * @brief Factory function that creates a std::optional<non_null<T>>.
 * Returns std::nullopt if the pointer is null.
 * @param p The pointer to wrap.
 * @return A std::optional containing the non_null wrapper if p is not null, otherwise std::nullopt.
 */
template < typename T >
constexpr std::optional< non_null< std::decay_t< T > > > try_make_non_null( T&& p )
{
    if ( p == nullptr )
        return std::nullopt;

    return std::optional< non_null< std::decay_t< T > > >( std::in_place, std::forward< T >( p ) );
}

/**
 * @brief Factory function that creates a non_null<std::unique_ptr<T>>.
 * Similar to std::make_unique, but returns a non-null wrapper.
 * @tparam T The type to construct.
 * @tparam Args The argument types for T's constructor.
 * @param args Arguments to forward to T's constructor.
 * @return A non_null<std::unique_ptr<T>> wrapping the newly created object.
 */
template < typename T, typename... Args >
inline non_null< std::unique_ptr< T > > make_non_null_unique( Args&&... args )
{
    return non_null( std::make_unique< T >( std::forward< Args >( args )... ) );
}

/**
 * @brief Factory function that creates a non_null<std::shared_ptr<T>>.
 * Similar to std::make_shared, but returns a non-null wrapper.
 * @tparam T The type to construct.
 * @tparam Args The argument types for T's constructor.
 * @param args Arguments to forward to T's constructor.
 * @return A non_null<std::shared_ptr<T>> wrapping the newly created object.
 */
template < typename T, typename... Args >
inline non_null< std::shared_ptr< T > > make_non_null_shared( Args&&... args )
{
    return non_null( std::make_shared< T >( std::forward< Args >( args )... ) );
}

// =============================================================================
// non_null_function
// =============================================================================

/**
 * @brief Primary template declaration — only the function-signature
 *        specialisation below is defined.
 */
template < typename Signature >
class non_null_function;

/**
 * @brief A non-null wrapper for std::function<R(Args...)>.
 *
 * Guarantees the stored callable is never empty (i.e. bool(fn_) == true).
 * Copy and move are both fully supported, mirroring the behaviour of
 * non_null for copyable pointer types (raw pointers, shared_ptr).
 *
 * The call operator uses NOVA_ASSUME to let the optimiser eliminate the
 * empty-callable check that std::function::operator() would otherwise emit.
 */
template < typename R, typename... Args >
class non_null_function< R( Args... ) >
{
public:
    using result_type   = R;
    using function_type = std::function< R( Args... ) >;

    /**
     * @brief Constructs from any callable that is invocable with (Args...) -> R.
     * @param f The callable to wrap. Must not be empty (assert-checked).
     */
    template < typename F >
        requires std::is_invocable_r_v< R, F, Args... > && (!std::is_same_v< std::decay_t< F >, non_null_function >)
    constexpr explicit non_null_function( F&& f ) :
        fn_( std::forward< F >( f ) )
    {
        detail::assume_not_empty( fn_ );
    }

    // Copy ctor and copy assignment are defaulted (std::function is copyable)
    non_null_function( const non_null_function& )            = default;
    non_null_function& operator=( const non_null_function& ) = default;

    // Move is emulated by copy
    non_null_function( non_null_function&& other ) noexcept :
        fn_ {
            function_type {
                other.fn_,
            },
        }
    {
        detail::assume_not_empty( fn_ );
    }
    non_null_function& operator=( non_null_function&& other ) noexcept
    {
        fn_ = function_type { other.fn_ };
        detail::assume_not_empty( fn_ );
        return *this;
    }

    // Prevent null assignment / null construction
    non_null_function( std::nullptr_t )            = delete;
    non_null_function& operator=( std::nullptr_t ) = delete;

    /**
     * @brief Invokes the stored callable.
     *
     * NOVA_ASSUME tells the optimiser that fn_ is non-empty, eliminating the
     * bad_function_call guard that std::function::operator() normally emits.
     */
    template < typename... CallArgs >
    R operator()( CallArgs&&... args ) const
    {
        detail::assume_not_empty( fn_ );
        return fn_( std::forward< CallArgs >( args )... );
    }

    /**
     * @brief Returns a const reference to the underlying std::function.
     * The rvalue overload is deleted to prevent accidental moves leaving this
     * object empty.
     */
    constexpr const function_type& underlying() const& noexcept
    {
        return fn_;
    }
    function_type underlying() && = delete;

    /**
     * @brief Always returns true; the callable is guaranteed non-empty.
     */
    constexpr explicit operator bool() const noexcept
    {
        return true;
    }

    /**
     * @brief Swaps the managed callables. Both objects remain non-empty.
     */
    constexpr void swap( non_null_function& other ) noexcept
    {
        fn_.swap( other.fn_ );
    }

    /**
     * @brief Explicitly extracts the underlying std::function, consuming the
     *        non_null_function wrapper.
     *
     * After this call the non_null_function is in a moved-from state and must
     * not be used.
     */
    friend function_type take( non_null_function&& nn ) noexcept
    {
        return std::move( nn.fn_ );
    }

private:
    function_type NOVA_NONNULL fn_;
};

/**
 * @brief Deduction guide: deduce the function signature from a plain function
 *        pointer.
 */
template < typename R, typename... Args >
non_null_function( R ( *NOVA_NONNULL )( Args... ) ) -> non_null_function< R( Args... ) >;

/**
 * @brief ADL swap for non_null_function.
 */
template < typename Sig >
void swap( non_null_function< Sig >& lhs, non_null_function< Sig >& rhs ) noexcept
{
    lhs.swap( rhs );
}

#if defined( __cpp_lib_move_only_function ) && __cpp_lib_move_only_function >= 202110L

// =============================================================================
// non_null_move_only_function  (C++23)
// =============================================================================

/**
 * @brief Primary template declaration.
 */
template < typename Signature >
class non_null_move_only_function;

/**
 * @brief A non-null wrapper for std::move_only_function<R(Args...)>.
 *
 * Guarantees the stored callable is never empty. Because
 * std::move_only_function is move-only, implicit move is **deleted** to prevent
 * moving the wrapper into an empty state — mirroring the behaviour of
 * non_null<unique_ptr>.  Use take() to explicitly extract ownership.
 */
template < typename R, typename... Args >
class non_null_move_only_function< R( Args... ) >
{
public:
    using result_type   = R;
    using function_type = std::move_only_function< R( Args... ) >;

    /**
     * @brief Constructs from any callable that is invocable with (Args...) -> R.
     * @param f The callable to wrap. Must not be empty (assert-checked).
     */
    template < typename F >
        requires std::is_invocable_r_v< R, F, Args... >
                 && (!std::is_same_v< std::decay_t< F >, non_null_move_only_function >)
    constexpr explicit non_null_move_only_function( F&& f ) :
        fn_( std::forward< F >( f ) )
    {
        detail::assume_not_empty( fn_ );
    }

    // Copy: deleted (move_only_function is not copyable)
    non_null_move_only_function( const non_null_move_only_function& )            = delete;
    non_null_move_only_function& operator=( const non_null_move_only_function& ) = delete;

    // Implicit move: deleted to prevent accidental moves that leave the
    // wrapper in an unusable (empty) state, exactly as non_null<unique_ptr>.
    // Use take() to transfer ownership explicitly.
    non_null_move_only_function( non_null_move_only_function&& )            = delete;
    non_null_move_only_function& operator=( non_null_move_only_function&& ) = delete;

    // Prevent null assignment / null construction
    non_null_move_only_function( std::nullptr_t )            = delete;
    non_null_move_only_function& operator=( std::nullptr_t ) = delete;

    /**
     * @brief Invokes the stored callable.
     *
     * NOVA_ASSUME tells the optimiser that fn_ is non-empty, eliminating the
     * bad_function_call guard that move_only_function::operator() normally emits.
     */
    template < typename... CallArgs >
    R operator()( CallArgs&&... args )
    {
        detail::assume_not_empty( fn_ );
        return fn_( std::forward< CallArgs >( args )... );
    }

    /**
     * @brief Returns a const reference to the underlying move_only_function.
     */
    constexpr const function_type& underlying() const& noexcept
    {
        return fn_;
    }
    function_type underlying() && = delete;

    /**
     * @brief Always returns true; the callable is guaranteed non-empty.
     */
    constexpr explicit operator bool() const noexcept
    {
        return true;
    }

    /**
     * @brief Swaps the managed callables. Both objects remain non-empty.
     */
    constexpr void swap( non_null_move_only_function& other ) noexcept
    {
        fn_.swap( other.fn_ );
    }

    /**
     * @brief Explicitly extracts the underlying move_only_function, consuming
     *        the non_null_move_only_function wrapper.
     *
     * After this call the wrapper is in a moved-from state and must not be used.
     * This is the only safe way to transfer ownership out of a
     * non_null_move_only_function, mirroring take() for non_null<unique_ptr>.
     */
    friend function_type take( non_null_move_only_function&& nn ) noexcept
    {
        return std::move( nn.fn_ );
    }

private:
    function_type NOVA_NONNULL fn_;
};

/**
 * @brief ADL swap for non_null_move_only_function.
 */
template < typename Sig >
void swap( non_null_move_only_function< Sig >& lhs, non_null_move_only_function< Sig >& rhs ) noexcept
{
    lhs.swap( rhs );
}

#endif // __cpp_lib_move_only_function

} // namespace nova

#undef NOVA_ASSUME
#undef NOVA_RETURNS_NONNULL
#undef NOVA_NONNULL
