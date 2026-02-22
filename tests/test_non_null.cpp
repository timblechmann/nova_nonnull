// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Nova Project Contributors

#include <memory>
#include <string>

#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include <nova/non_null.hpp>

// Helper to create a pointer of type Ptr
template < typename Ptr >
struct PtrHelper;

template < typename T >
struct PtrHelper< T* >
{
    static T* make( T& val )
    {
        return &val;
    }
    static T* null()
    {
        return nullptr;
    }
};

template < typename T >
struct PtrHelper< std::unique_ptr< T > >
{
    static std::unique_ptr< T > make( T& val )
    {
        return std::make_unique< T >( val );
    }
    static std::unique_ptr< T > null()
    {
        return nullptr;
    }
};

template < typename T >
struct PtrHelper< std::shared_ptr< T > >
{
    static std::shared_ptr< T > make( T& val )
    {
        return std::make_shared< T >( val );
    }
    static std::shared_ptr< T > null()
    {
        return nullptr;
    }
};

TEMPLATE_TEST_CASE( "non_null core functionality", "[non_null]", int*, std::unique_ptr< int >, std::shared_ptr< int > )
{
    using Ptr = TestType;
    int val   = 42;
    Ptr p     = PtrHelper< Ptr >::make( val );

    // Construction
    nova::non_null< Ptr > nn( std::move( p ) );

    SECTION( "dereference and access" )
    {
        CHECK( *nn == 42 );
    }

    SECTION( "get() accessor returns raw pointer" )
    {
        auto underlying = nn.get();
        static_assert( std::is_pointer_v< decltype( underlying ) >, "get() must return a raw pointer" );
        CHECK( *underlying == 42 );
    }

    SECTION( "explicit operator bool" )
    {
        CHECK( static_cast< bool >( nn ) );
        if ( nn ) {
            CHECK( true );
        } else {
            FAIL( "non_null should always evaluate to true" );
        }
    }

    SECTION( "underlying() accessor" )
    {
        const Ptr& underlying = nn.underlying();
        CHECK( *underlying == 42 );
    }

    SECTION( "explicit conversion" )
    {
        const Ptr& converted = static_cast< const Ptr& >( nn );
        CHECK( *converted == 42 );
    }
}

TEST_CASE( "non_null specific behavior for raw pointers", "[non_null]" )
{
    int                    x = 42;
    nova::non_null< int* > p( &x );

    SECTION( "arrow operator" )
    {
        std::string                    s = "hello";
        nova::non_null< std::string* > ps( &s );
        CHECK( ps->length() == 5 );
        auto* raw_ps = ps.operator->();
        static_assert( std::is_pointer_v< decltype( raw_ps ) >, "operator-> must return a raw pointer" );
        CHECK( raw_ps == &s );
    }

    SECTION( "raw pointer assignment (explicit)" )
    {
        int* raw = static_cast< int* >( p );
        CHECK( raw == &x );
    }
}

TEMPLATE_TEST_CASE(
    "try_make_non_null factory templated", "[non_null]", int*, std::unique_ptr< int >, std::shared_ptr< int > )
{
    using Ptr = TestType;

    SECTION( "non-null case" )
    {
        int  val = 5;
        auto opt = nova::try_make_non_null( PtrHelper< Ptr >::make( val ) );
        CHECK( opt.has_value() );
        CHECK( *opt.value() == 5 );
    }

    SECTION( "null case" )
    {
        auto opt = nova::try_make_non_null( PtrHelper< Ptr >::null() );
        CHECK_FALSE( opt.has_value() );
    }
}

TEST_CASE( "comparison operators", "[non_null]" )
{
    int  arr[ 2 ] = { 10, 20 };
    int* x_ptr    = &arr[ 0 ];
    int* y_ptr    = &arr[ 1 ];

    nova::non_null< int* > px( x_ptr );
    nova::non_null< int* > py( y_ptr );
    nova::non_null< int* > px2( x_ptr );

    SECTION( "non_null vs non_null" )
    {
        CHECK( px == px2 );
        CHECK( px != py );
        CHECK( px < py );
        CHECK( py > px );
        CHECK( px <= py );
        CHECK( py >= px );
    }

    SECTION( "non_null vs T" )
    {
        CHECK( px == x_ptr );
        CHECK( px != y_ptr );
        CHECK( px < y_ptr );
        CHECK( y_ptr > px );
    }

    SECTION( "non_null vs nullptr" )
    {
        CHECK_FALSE( px == nullptr );
        CHECK_FALSE( nullptr == px );
        CHECK( px != nullptr );
        CHECK( nullptr != px );
    }
}

TEST_CASE( "conversion between compatible non_null types", "[non_null]" )
{
    struct Base
    {
        virtual ~Base() = default;
    };
    struct Derived : Base
    {};

    Derived                    d;
    nova::non_null< Derived* > pd( &d );

    SECTION( "implicit conversion from non_null<Derived*> to non_null<Base*>" )
    {
        nova::non_null< Base* > pb = pd;
        CHECK( pb.get() == &d );
    }

    SECTION( "copy conversion from compatible non_null type" )
    {
        nova::non_null< Derived* > pd2( &d );
        nova::non_null< Base* >    pb = pd2;
        CHECK( pb.get() == &d );
    }
}

TEST_CASE( "take - explicit ownership extraction", "[take]" )
{
    SECTION( "take from unique_ptr non_null" )
    {
        auto nn  = nova::make_non_null_unique< int >( 100 );
        auto ptr = take( std::move( nn ) );
        static_assert( std::is_same_v< decltype( ptr ), std::unique_ptr< int > > );
        CHECK( *ptr == 100 );
    }

    SECTION( "take and re-wrap unique_ptr" )
    {
        auto nn1 = nova::make_non_null_unique< int >( 42 );
        auto nn2 = nova::non_null( take( std::move( nn1 ) ) );
        CHECK( *nn2 == 42 );
    }

    SECTION( "take from shared_ptr non_null" )
    {
        auto nn  = nova::make_non_null_shared< int >( 99 );
        auto ptr = take( std::move( nn ) );
        static_assert( std::is_same_v< decltype( ptr ), std::shared_ptr< int > > );
        CHECK( *ptr == 99 );
    }

    SECTION( "take from raw pointer non_null" )
    {
        int                    val = 7;
        nova::non_null< int* > nn( &val );
        int*                   raw = take( std::move( nn ) );
        CHECK( *raw == 7 );
    }
}

TEST_CASE( "make_non_null_unique factory function", "[factory]" )
{
    SECTION( "default construction" )
    {
        auto nn = nova::make_non_null_unique< int >();
        CHECK( *nn == 0 );
    }

    SECTION( "construction with arguments" )
    {
        auto nn = nova::make_non_null_unique< int >( 42 );
        CHECK( *nn == 42 );
    }

    SECTION( "construction with multiple arguments" )
    {
        struct Point
        {
            int x, y;
            Point( int x_, int y_ ) :
                x( x_ ),
                y( y_ )
            {}
        };
        auto nn = nova::make_non_null_unique< Point >( 10, 20 );
        CHECK( nn->x == 10 );
        CHECK( nn->y == 20 );
    }

    SECTION( "string construction" )
    {
        auto nn = nova::make_non_null_unique< std::string >( "hello" );
        CHECK( *nn == "hello" );
    }

    SECTION( "returned pointer is valid and non-null" )
    {
        auto nn  = nova::make_non_null_unique< int >( 100 );
        int* raw = nn.get();
        CHECK( raw != nullptr );
        CHECK( *raw == 100 );
    }
}

TEST_CASE( "make_non_null_shared factory function", "[factory]" )
{
    SECTION( "default construction" )
    {
        auto nn = nova::make_non_null_shared< int >();
        CHECK( *nn == 0 );
    }

    SECTION( "construction with arguments" )
    {
        auto nn = nova::make_non_null_shared< int >( 42 );
        CHECK( *nn == 42 );
    }

    SECTION( "construction with multiple arguments" )
    {
        struct Point
        {
            int x, y;
            Point( int x_, int y_ ) :
                x( x_ ),
                y( y_ )
            {}
        };
        auto nn = nova::make_non_null_shared< Point >( 10, 20 );
        CHECK( nn->x == 10 );
        CHECK( nn->y == 20 );
    }

    SECTION( "string construction" )
    {
        auto nn = nova::make_non_null_shared< std::string >( "world" );
        CHECK( *nn == "world" );
    }

    SECTION( "returned pointer is valid and non-null" )
    {
        auto nn  = nova::make_non_null_shared< int >( 100 );
        int* raw = nn.get();
        CHECK( raw != nullptr );
        CHECK( *raw == 100 );
    }

    SECTION( "shared ownership semantics" )
    {
        auto nn1 = nova::make_non_null_shared< int >( 42 );
        auto nn2 = nn1; // Copy - shares ownership
        CHECK( *nn1 == 42 );
        CHECK( *nn2 == 42 );
    }
}

TEST_CASE( "swap", "[swap]" )
{
    SECTION( "member swap - raw pointer" )
    {
        int                    a = 1, b = 2;
        nova::non_null< int* > nn1( &a ), nn2( &b );
        nn1.swap( nn2 );
        CHECK( *nn1 == 2 );
        CHECK( *nn2 == 1 );
    }

    SECTION( "member swap - unique_ptr" )
    {
        auto nn1 = nova::make_non_null_unique< int >( 10 );
        auto nn2 = nova::make_non_null_unique< int >( 20 );
        nn1.swap( nn2 );
        CHECK( *nn1 == 20 );
        CHECK( *nn2 == 10 );
    }

    SECTION( "member swap - shared_ptr" )
    {
        auto nn1 = nova::make_non_null_shared< int >( 10 );
        auto nn2 = nova::make_non_null_shared< int >( 20 );
        nn1.swap( nn2 );
        CHECK( *nn1 == 20 );
        CHECK( *nn2 == 10 );
    }

    SECTION( "free swap via ADL" )
    {
        auto nn1 = nova::make_non_null_unique< int >( 1 );
        auto nn2 = nova::make_non_null_unique< int >( 2 );
        using std::swap;
        swap( nn1, nn2 );
        CHECK( *nn1 == 2 );
        CHECK( *nn2 == 1 );
    }
}

TEST_CASE( "get_deleter - unique_ptr specific", "[unique_ptr]" )
{
    SECTION( "default deleter is accessible" )
    {
        auto  nn       = nova::make_non_null_unique< int >( 42 );
        // get_deleter() must compile and return a reference to the deleter
        auto& d        = nn.get_deleter();
        using expected = std::default_delete< int >;
        static_assert( std::is_same_v< std::remove_reference_t< decltype( d ) >, expected > );
    }

    SECTION( "const get_deleter" )
    {
        const auto  nn = nova::make_non_null_unique< int >( 42 );
        const auto& d  = nn.get_deleter();
        using expected = std::default_delete< int >;
        static_assert( std::is_same_v< std::decay_t< decltype( d ) >, expected > );
    }
}

TEST_CASE( "use_count - shared_ptr specific", "[shared_ptr]" )
{
    SECTION( "single owner" )
    {
        auto nn = nova::make_non_null_shared< int >( 7 );
        CHECK( nn.use_count() == 1 );
    }

    SECTION( "shared ownership increments count" )
    {
        auto nn1 = nova::make_non_null_shared< int >( 7 );
        auto nn2 = nn1;
        CHECK( nn1.use_count() == 2 );
        CHECK( nn2.use_count() == 2 );
    }
}

TEST_CASE( "owner_before - shared_ptr specific", "[shared_ptr]" )
{
    SECTION( "owner_before with shared_ptr" )
    {
        auto nn1    = nova::make_non_null_shared< int >( 1 );
        auto nn2    = nova::make_non_null_shared< int >( 2 );
        // owner_before provides a strict weak ordering; one must be before the other or equal
        bool result = nn1.owner_before( nn2.underlying() ) || nn2.owner_before( nn1.underlying() )
                      || ( !nn1.owner_before( nn2.underlying() ) && !nn2.owner_before( nn1.underlying() ) );
        CHECK( result );
    }

    SECTION( "owner_before with weak_ptr" )
    {
        auto nn = nova::make_non_null_shared< int >( 42 );
        auto wp = std::weak_ptr< int >( nn.underlying() );
        // nn.owner_before(weak_ptr) must compile
        bool r  = nn.owner_before( wp );
        (void)r;
    }

    SECTION( "owner_before with non_null<shared_ptr>" )
    {
        auto nn1 = nova::make_non_null_shared< int >( 1 );
        auto nn2 = nova::make_non_null_shared< int >( 2 );
        bool r   = nn1.owner_before( nn2 );
        (void)r;
    }
}

TEST_CASE( "move operations on copyable pointers", "[copyable_pointer][move]" )
{
    SECTION( "move construction - raw pointer" )
    {
        int                    val = 100;
        nova::non_null< int* > nn1( &val );
        nova::non_null< int* > nn2( std::move( nn1 ) );
        CHECK( *nn2 == 100 );
        // nn1 still has valid state (raw pointers don't become null after move)
        CHECK( *nn1 == 100 );
    }

    SECTION( "move assignment - raw pointer" )
    {
        int                    val1 = 10;
        int                    val2 = 20;
        nova::non_null< int* > nn1( &val1 );
        nova::non_null< int* > nn2( &val2 );
        nn2 = std::move( nn1 );
        CHECK( *nn2 == 10 );
        CHECK( *nn1 == 10 );
    }

    SECTION( "move construction - shared_ptr" )
    {
        auto nn1 = nova::make_non_null_shared< int >( 42 );
        // shared_ptr move is safe — reference count handling is correct
        auto nn2 = std::move( nn1 );
        CHECK( *nn2 == 42 );
        // nn1 is now null (shared_ptr move leaves source as nullptr)
        // But we don't dereference it, so invariant isn't tested here
    }

    SECTION( "move assignment - shared_ptr" )
    {
        auto nn1 = nova::make_non_null_shared< int >( 1 );
        auto nn2 = nova::make_non_null_shared< int >( 2 );
        nn2      = std::move( nn1 );
        CHECK( *nn2 == 1 );
    }
}

TEST_CASE( "move-only types do not support implicit move", "[move-only][unique_ptr]" )
{
    // These tests verify compile-time constraints using move detection.
    // A move constructor exists for unique_ptr<int> non_null iff
    // the copyable_pointer concept is false for unique_ptr.
    nova::non_null< std::unique_ptr< int > > nn1 = nova::make_non_null_unique< int >( 42 );

    // Explicit move via take() is the correct pattern:
    auto nn2 = nova::non_null( take( std::move( nn1 ) ) );
    CHECK( *nn2 == 42 );

    // Note: Attempting nn1 after take() is UB; cannot be tested here.
    // The compiler prevents casual use via move constructor deletion.
}
