// Export tool -- dumps VTObjectPool.cpp's real, hand-authored pool
// (BuildObjectPool()) to a raw .iop file, so it can be opened in
// AgIsoTerminalDesigner (https://open-agriculture.github.io/AgIsoTerminalDesigner/)
// for visual inspection/editing.
//
// Builds natively against the same test/native/support Arduino shims used by
// the AUnit test harness -- see export_pool_msvc.ps1 for the actual build
// command. VTObjectPool.cpp/.hpp have no AgIsoStack/CAN dependency at all
// (confirmed by reading them), only <Arduino.h> for integer typedefs and the
// ARDUINO compile-time guard -- so this links clean with nothing but the
// stub and this file.
#include "../../lib/PloegbesturingCore/src/isobus/VTObjectPool.hpp"

#include <cstdio>

int main(int argc, char** argv) {
    const char* outPath = (argc > 1) ? argv[1] : "tools/vt_pool/generated/plough_pool.iop";

    triton::BuildObjectPool();

    if (triton::VT3PoolData == nullptr || triton::VT3PoolSize == 0) {
        fprintf(stderr, "BuildObjectPool() produced an empty pool -- check VTObjectPool.cpp's\n"
                         "VT_POOL_USE_AGISOSTACK_REFERENCE/VT_POOL_MINIMAL_BISECT_TEST toggles are off.\n");
        return 1;
    }

    FILE* f = fopen(outPath, "wb");
    if (!f) {
        fprintf(stderr, "Could not open '%s' for writing\n", outPath);
        return 1;
    }
    size_t written = fwrite(triton::VT3PoolData, 1, triton::VT3PoolSize, f);
    fclose(f);

    if (written != triton::VT3PoolSize) {
        fprintf(stderr, "Short write: expected %u bytes, wrote %zu\n", triton::VT3PoolSize, written);
        return 1;
    }

    printf("Wrote %u bytes to %s\n", triton::VT3PoolSize, outPath);
    return 0;
}
