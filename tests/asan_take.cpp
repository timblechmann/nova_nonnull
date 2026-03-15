// Simple ASAN smoke test: use-after-take should be reported by AddressSanitizer.
#include <iostream>

#include <nova/non_null.hpp>

int main()
{
    using namespace nova;

    auto nn = make_non_null_unique< int >( 42 );

    // Extract the unique_ptr out of the non_null wrapper.
    auto up = take( std::move( nn ) );

    // nn is now moved-from; accessing it should trigger ASAN when built with -fsanitize=address
    // Intentional use-after-take:
    std::cerr << "About to do use-after-take (expect ASAN)\n";
    // Force a call that will read the wrapper state
    volatile auto p = nn.get();
    (void)p;

    (void)up;
    return 0;
}
