#include "fatfs_getfree_stub.h"

static FRESULT stub_result    = FR_OK;
static DWORD   stub_fre_clust = 0;
static FATFS   stub_fatfs     = {0};

void stub_set_disk(DWORD total_clust, DWORD fre_clust, FRESULT result) {
    stub_fatfs.n_fatent = total_clust + 2;
    stub_fre_clust      = fre_clust;
    stub_result         = result;
}

FRESULT f_getfree(const char *path, DWORD *nclst, FATFS **fatfs) {
    (void)path;
    *nclst = stub_fre_clust;
    *fatfs = &stub_fatfs;
    return stub_result;
}
