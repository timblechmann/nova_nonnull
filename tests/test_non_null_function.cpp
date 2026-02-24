// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Tim Blechmann

#include <functional>
#include <type_traits>

#include <catch2/catch_test_macros.hpp>

#include <nova/non_null.hpp>

// =============================================================================
// non_null_function tests
// =============================================================================

TEST_CASE( "non_null_function - construction from lambda", "[non_null_function]" )
{
    nova::non_null_function< int( int ) > fn( []( int x ) {
        return x * 2;
    } );
    CHECK( fn( 21 ) == 42 );
}

TEST_CASE( "non_null_function - construction from function pointer", "[non_null_function]" )
{
    struct Helper
    {
        static int triple( int x )
        {
            return x * 3;
        }
    };

    nova::non_null_function< int( int ) > fn( &Helper::triple );
    CHECK( fn( 7 ) == 21 );
}

TEST_CASE( "non_null_function - construction from std::function", "[non_null_function]" )
{
    std::function< int( int ) > base = []( int x ) {
        return x + 1;
    };
    nova::non_null_function< int( int ) > fn( base );
    CHECK( fn( 41 ) == 42 );
}

TEST_CASE( "non_null_function - deduction guide from function pointer", "[non_null_function]" )
{
    struct Helper
    {
        static int inc( int x )
        {
            return x + 1;
        }
    };

    nova::non_null_function fn( &Helper::inc );
    static_assert( std::is_same_v< decltype( fn ), nova::non_null_function< int( int ) > > );
    CHECK( fn( 5 ) == 6 );
}

TEST_CASE( "non_null_function - operator bool is always true", "[non_null_function]" )
{
    nova::non_null_function< void() > fn( [] {} );
    CHECK( static_cast< bool >( fn ) );
}

TEST_CASE( "non_null_function - copy construction", "[non_null_function]" )
{
    int                                     counter = 0;
    nova::non_null_function< void() >       fn1( [ & ] {
        ++counter;
    } );
    const nova::non_null_function< void() > fn2( fn1 ); // copy
    fn1();
    fn2();
    CHECK( counter == 2 );
}

TEST_CASE( "non_null_function - copy assignment", "[non_null_function]" )
{
    int                               a = 0, b = 0;
    nova::non_null_function< void() > fn1( [ & ] {
        ++a;
    } );
    nova::non_null_function< void() > fn2( [ & ] {
        ++b;
    } );
    fn2 = fn1;
    fn2();
    CHECK( a == 1 );
    CHECK( b == 0 );
}

TEST_CASE( "non_null_function - move construction", "[non_null_function]" )
{
    nova::non_null_function< int() > fn1( [] {
        return 99;
    } );
    nova::non_null_function< int() > fn2( std::move( fn1 ) );
    CHECK( fn2() == 99 );
}

TEST_CASE( "non_null_function - move assignment", "[non_null_function]" )
{
    nova::non_null_function< int() > fn1( [] {
        return 7;
    } );
    nova::non_null_function< int() > fn2( [] {
        return 0;
    } );
    fn2 = std::move( fn1 );
    CHECK( fn2() == 7 );
}

TEST_CASE( "non_null_function - take() extracts underlying function", "[non_null_function]" )
{
    nova::non_null_function< int( int ) > fn( []( int x ) {
        return x + 10;
    } );
    auto                                  raw = take( std::move( fn ) );
    static_assert( std::is_same_v< decltype( raw ), std::function< int( int ) > > );
    CHECK( raw( 32 ) == 42 );
}

TEST_CASE( "non_null_function - swap", "[non_null_function]" )
{
    nova::non_null_function< int() > fn1( [] {
        return 1;
    } );
    nova::non_null_function< int() > fn2( [] {
        return 2;
    } );
    fn1.swap( fn2 );
    CHECK( fn1() == 2 );
    CHECK( fn2() == 1 );
}

TEST_CASE( "non_null_function - ADL swap", "[non_null_function]" )
{
    nova::non_null_function< int() > fn1( [] {
        return 10;
    } );
    nova::non_null_function< int() > fn2( [] {
        return 20;
    } );
    using std::swap;
    swap( fn1, fn2 );
    CHECK( fn1() == 20 );
    CHECK( fn2() == 10 );
}

TEST_CASE( "non_null_function - underlying() accessor", "[non_null_function]" )
{
    nova::non_null_function< int( int ) > fn( []( int x ) {
        return x;
    } );
    const std::function< int( int ) >&    underlying = fn.underlying();
    CHECK( underlying( 55 ) == 55 );
}

TEST_CASE( "non_null_function - result_type and function_type aliases", "[non_null_function]" )
{
    using Fn = nova::non_null_function< int( double ) >;
    static_assert( std::is_same_v< Fn::result_type, int > );
    static_assert( std::is_same_v< Fn::function_type, std::function< int( double ) > > );
}

// Null construction must be deleted (compile-time enforcement)
static_assert( !std::is_constructible_v< nova::non_null_function< int() >, std::nullptr_t > );

// =============================================================================
// non_null_move_only_function tests  (C++23 only)
// =============================================================================

#if defined( __cpp_lib_move_only_function ) && __cpp_lib_move_only_function >= 202110L

TEST_CASE( "non_null_move_only_function - construction from lambda", "[non_null_move_only_function]" )
{
    nova::non_null_move_only_function< int( int ) > fn( []( int x ) {
        return x * 2;
    } );
    CHECK( fn( 21 ) == 42 );
}

TEST_CASE( "non_null_move_only_function - move-only capture", "[non_null_move_only_function]" )
{
    // Capture a unique_ptr — only possible with move_only_function
    auto                                       up = std::make_unique< int >( 99 );
    nova::non_null_move_only_function< int() > fn( [ p = std::move( up ) ]() {
        return *p;
    } );
    CHECK( fn() == 99 );
}

TEST_CASE( "non_null_move_only_function - operator bool is always true", "[non_null_move_only_function]" )
{
    nova::non_null_move_only_function< void() > fn( [] {} );
    CHECK( static_cast< bool >( fn ) );
}

TEST_CASE( "non_null_move_only_function - take() extracts underlying function", "[non_null_move_only_function]" )
{
    nova::non_null_move_only_function< int( int ) > fn( []( int x ) {
        return x + 10;
    } );
    auto                                            raw = take( std::move( fn ) );
    static_assert( std::is_same_v< decltype( raw ), std::move_only_function< int( int ) > > );
    CHECK( raw( 32 ) == 42 );
}

TEST_CASE( "non_null_move_only_function - take() and re-wrap", "[non_null_move_only_function]" )
{
    nova::non_null_move_only_function< int() > fn1( [] {
        return 7;
    } );
    auto fn2 = nova::non_null_move_only_function< int() >( take( std::move( fn1 ) ) );
    CHECK( fn2() == 7 );
}

TEST_CASE( "non_null_move_only_function - swap", "[non_null_move_only_function]" )
{
    nova::non_null_move_only_function< int() > fn1( [] {
        return 1;
    } );
    nova::non_null_move_only_function< int() > fn2( [] {
        return 2;
    } );
    fn1.swap( fn2 );
    CHECK( fn1() == 2 );
    CHECK( fn2() == 1 );
}

TEST_CASE( "non_null_move_only_function - ADL swap", "[non_null_move_only_function]" )
{
    nova::non_null_move_only_function< int() > fn1( [] {
        return 10;
    } );
    nova::non_null_move_only_function< int() > fn2( [] {
        return 20;
    } );
    using std::swap;
    swap( fn1, fn2 );
    CHECK( fn1() == 20 );
    CHECK( fn2() == 10 );
}

TEST_CASE( "non_null_move_only_function - underlying() accessor", "[non_null_move_only_function]" )
{
    nova::non_null_move_only_function< int( int ) > fn( []( int x ) {
        return x;
    } );
    const std::move_only_function< int( int ) >&    underlying = fn.underlying();
    CHECK( underlying( 77 ) == 77 );
}

TEST_CASE( "non_null_move_only_function - result_type and function_type aliases", "[non_null_move_only_function]" )
{
    using Fn = nova::non_null_move_only_function< int( double ) >;
    static_assert( std::is_same_v< Fn::result_type, int > );
    static_assert( std::is_same_v< Fn::function_type, std::move_only_function< int( double ) > > );
}

// Copy must be deleted
static_assert( !std::is_copy_constructible_v< nova::non_null_move_only_function< int() > > );
static_assert( !std::is_copy_assignable_v< nova::non_null_move_only_function< int() > > );

// Implicit move must be deleted (use take() instead)
static_assert( !std::is_move_constructible_v< nova::non_null_move_only_function< int() > > );
static_assert( !std::is_move_assignable_v< nova::non_null_move_only_function< int() > > );

// Null construction must be deleted
static_assert( !std::is_constructible_v< nova::non_null_move_only_function< int() >, std::nullptr_t > );

#endif // __cpp_lib_move_only_function
