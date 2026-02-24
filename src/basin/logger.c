
#include "basin/logger.h"

#include "basin/types.h"
#include "platform/platform.h"

bool enabled_logging_driver = true;


void dump_hex(void* address, int size, int stride) {
    u8* data = address;
    log__printf("hexdump 0x"FL"x + 0x%x:\n", (uint64_t)address, size);
    int col = 0;
    uint64_t head = 0;
    while (head < size) {
        if (col == 0)
            log__printf("  "FL"x: ", (char*)address + head);

        log__printf("%x%x ", data[head] >> 4, data[head] & 0xF);
        col++;
        if (col >= stride) {
            col = 0;
            log__printf("\n");
        }
        head++;
    }
    if (col != 0)
        log__printf("\n");
}
