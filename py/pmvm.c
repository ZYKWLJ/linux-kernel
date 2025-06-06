#include <stdio.h>

int main() {
    // Virtual Address Structure
    printf("Virtual Address (32-bit) Structure:\n");
    printf("+-----------------+-----------------+---------------------+\n");
    printf("| Page Directory  |   Page Table    |     Offset (12)     |\n");
    printf("|   Index (10)    |    Index (10)   |  (Low 12 bits)      |\n");
    printf("+-----------------+-----------------+---------------------+\n");
    printf("|      0x004      |      0x001      |        0x234        |  (Example)\n");
    printf("+-----------------+-----------------+---------------------+\n");
    printf("        ^               ^                   ^\n");
    printf("        |               |                   |\n");
    printf("        +---------------+-------------------+----------------+\n");
    printf("        |                                   |                |\n");
    printf("        |           Page Table Index        | Physical Offset|\n");
    printf("        |             → Find PTE            |                |\n");
    printf("        |                                   |                |\n");
    printf("        +--- Page Directory Index           |                |\n");
    printf("                → Find PDE                  |                |\n\n");

    // Physical Address Translation
    printf("Physical Address Translation:\n");
    printf("+-----------------+---------------------+\n");
    printf("|  Page Frame     |     Offset (12)     |\n");
    printf("|   Number (20)   |  (From Virtual Addr)|\n");
    printf("+-----------------+---------------------+\n");
    printf("|      0x00123    |        0x234        |  (Example)\n");
    printf("+-----------------+---------------------+\n");
    printf("        ^                     ^\n");
    printf("        |                     |\n");
    printf("        +--- Shift Left 12    |\n");
    printf("              bits → 0x00123 << 12 = 0x00123000 (Base Address)\n");
    printf("                            +\n");
    printf("                            |\n");
    printf("                            v\n\n");

    // Final Physical Address
    printf("Final Physical Address:\n");
    printf("+---------------------------------------+\n");
    printf("|        0x00123000 + 0x234 = 0x00123234        |\n");
    printf("+---------------------------------------+\n");

    return 0;
}