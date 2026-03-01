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

typedef struct { int _dummy; } FATFS;
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
