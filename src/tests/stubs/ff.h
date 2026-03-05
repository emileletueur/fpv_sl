#pragma once
/* Stub — remplace FatFS ff.h pour la compilation host (tests natifs).
   Toutes les fonctions sont des no-ops inline ; f_gets retourne NULL
   pour que les boucles de lecture de fichier s'arrêtent immédiatement. */

#include <stdint.h>
#include <stddef.h>

typedef unsigned int  UINT;
typedef uint8_t       BYTE;
typedef uint32_t      DWORD;
typedef uint64_t      QWORD;

typedef enum {
    FR_OK = 0,
    FR_DISK_ERR,
    FR_INT_ERR,
    FR_NOT_READY,
    FR_NO_FILE,
    FR_NO_PATH,
    FR_INVALID_NAME,
    FR_DENIED,
    FR_EXIST,
    FR_INVALID_OBJECT,
    FR_WRITE_PROTECTED,
    FR_INVALID_DRIVE,
    FR_NOT_ENABLED,
    FR_NO_FILESYSTEM,
    FR_MKFS_ABORTED,
    FR_TIMEOUT,
    FR_LOCKED,
    FR_NOT_ENOUGH_CORE,
    FR_TOO_MANY_OPEN_FILES,
    FR_INVALID_PARAMETER,
} FRESULT;

typedef struct {
    DWORD n_fatent;  /* nombre d'entrées FAT (clusters totaux + 2) */
} FATFS;
typedef struct { int _dummy; } FIL;

#define FA_READ          0x01
#define FA_WRITE         0x02
#define FA_CREATE_ALWAYS 0x08
#define FA_OPEN_ALWAYS   0x10
#define FA_OPEN_APPEND   0x30

static inline FRESULT f_mount(FATFS *fs, const char *path, BYTE opt)
    { (void)fs; (void)path; (void)opt; return FR_OK; }

static inline FRESULT f_open(FIL *fp, const char *path, BYTE mode)
    { (void)fp; (void)path; (void)mode; return FR_OK; }

static inline FRESULT f_close(FIL *fp)
    { (void)fp; return FR_OK; }

/* Retourne NULL → la boucle while(read_line(...)) s'arrête dès le premier appel */
static inline char *f_gets(char *buf, int len, FIL *fp)
    { (void)len; (void)fp; if (buf) buf[0] = '\0'; return NULL; }

static inline FRESULT f_write(FIL *fp, const void *buf, UINT btr, UINT *bw)
    { (void)fp; (void)buf; if (bw) *bw = btr; return FR_OK; }

static inline FRESULT f_read(FIL *fp, void *buf, UINT btr, UINT *br)
    { (void)fp; (void)buf; (void)btr; if (br) *br = 0; return FR_OK; }

static inline FRESULT f_lseek(FIL *fp, QWORD ofs)
    { (void)fp; (void)ofs; return FR_OK; }

static inline FRESULT f_rename(const char *old_name, const char *new_name)
    { (void)old_name; (void)new_name; return FR_OK; }

/* f_getfree : implémentation fournie par fatfs_getfree_stub.c dans les tests
   qui en ont besoin (test_disk_usage). */
FRESULT f_getfree(const char *path, DWORD *nclst, FATFS **fatfs);

/* Métadonnées de fichier. */
typedef DWORD FSIZE_t;
#define AM_DIR 0x10
typedef struct {
    FSIZE_t fsize;
    BYTE    fattrib;
    char    fname[256];
} FILINFO;

/* Répertoire — stub minimal pour f_opendir/f_readdir/f_closedir. */
typedef struct { int _dummy; } DIR;

/* f_stat : retourne FR_NO_FILE dans les tests (pas de fichier temporaire). */
static inline FRESULT f_stat(const char *path, FILINFO *fno)
    { (void)path; if (fno) { fno->fsize = 0; fno->fattrib = 0; fno->fname[0] = '\0'; } return FR_NO_FILE; }

/* Répertoire — f_readdir retourne fname[0]=='\0' → fin de liste immédiate. */
static inline FRESULT f_opendir(DIR *dp, const char *path)
    { (void)dp; (void)path; return FR_OK; }
static inline FRESULT f_readdir(DIR *dp, FILINFO *fno)
    { (void)dp; if (fno) { fno->fname[0] = '\0'; } return FR_OK; }
static inline FRESULT f_closedir(DIR *dp)
    { (void)dp; return FR_OK; }

static inline FRESULT f_unlink(const char *path)
    { (void)path; return FR_OK; }

/* f_size : retourne 0 dans les tests. */
static inline FSIZE_t f_size(FIL *fp) { (void)fp; return 0; }

static inline FRESULT f_sync(FIL *fp) { (void)fp; return FR_OK; }

/* f_expand : no-op — retourne FR_OK pour que le chemin nominal (pré-allocation réussie)
   soit exercé dans les tests host. */
static inline FRESULT f_expand(FIL *fp, FSIZE_t fsz, BYTE opt)
    { (void)fp; (void)fsz; (void)opt; return FR_OK; }

/* f_truncate : no-op. */
static inline FRESULT f_truncate(FIL *fp) { (void)fp; return FR_OK; }
