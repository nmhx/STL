#include <iostream>
#include "allocation.h"

int main(void) {
    simple_alloc<char, alloc> simple;
    simple.allocate(1);
    simple.allocate(90);
    return 0;
}
