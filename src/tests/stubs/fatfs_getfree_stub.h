#pragma once
#include "ff.h"

/* Configure le comportement de f_getfree pour les tests.
   total_clust : nombre de clusters total du volume
   fre_clust   : nombre de clusters libres
   result      : code retour simulé (FR_OK, FR_DISK_ERR, ...) */
void stub_set_disk(DWORD total_clust, DWORD fre_clust, FRESULT result);
