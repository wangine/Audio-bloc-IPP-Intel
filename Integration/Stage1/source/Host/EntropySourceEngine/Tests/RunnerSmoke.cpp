#include <cstdint>
#include <iostream>
int main()
{
    static_assert(sizeof(void *) == 8);
    static_assert(sizeof(double) == 8);
    std::cout << "PASS Windows x64 FP64 runner\n";
}
