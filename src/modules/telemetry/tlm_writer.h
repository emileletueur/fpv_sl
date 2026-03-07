#pragma once

#include "fpv_sl_config.h"
#include <stdint.h>

/* Protocole source enregistré dans le header du fichier .tlm. */
#define TLM_PROTOCOL_MSP     1u
#define TLM_PROTOCOL_MAVLINK 2u

/* Header binaire en début de fichier .tlm (8 bytes, packed).
   Le champ `items` (bitmask tlm_item_flags_t) décrit le layout de chaque record :
     [uint32_t timestamp_ms]
     [uint16_t channels[8]]     si TLM_RC
     [int16_t  roll,pitch,yaw]  si TLM_ATTITUDE
     [uint8_t  fix,sats]        si TLM_GPS
     [int32_t  lat,lon]         si TLM_GPS
     [uint16_t alt,speed]       si TLM_GPS
     [uint16_t vbat_mv,mah]     si TLM_ANALOG
     [uint16_t rssi]            si TLM_ANALOG
     [int16_t  current_ca]      si TLM_ANALOG
   Tous les records d'une session ont la même taille (bitmask fixe à l'ouverture). */
typedef struct __attribute__((packed)) {
    char    magic[4];        /* "FPVT"                                      */
    uint8_t version;         /* version du format : 1                       */
    uint8_t items;           /* bitmask tlm_item_flags_t des champs présents */
    uint8_t source_protocol; /* TLM_PROTOCOL_MSP / TLM_PROTOCOL_MAVLINK     */
    uint8_t sample_rate_hz;  /* fréquence de polling en Hz (ex. 30)         */
} tlm_file_header_t;

/* Retourne la taille en bytes d'un record pour le bitmask donné.
   Sert de spec du format : toute modification ici doit être répercutée dans
   le README et le parser PC.
     timestamp_ms  : 4 B (toujours présent)
     TLM_RC        : 16 B (uint16 × 8 canaux)
     TLM_ATTITUDE  : 6 B  (int16 roll/pitch/yaw)
     TLM_GPS       : 14 B (fix,sats,lat,lon,alt,speed)
     TLM_ANALOG    : 7 B  (vbat_mv,mah,rssi,current_ca) */
static inline uint8_t tlm_record_size(uint8_t items) {
    uint8_t sz = 4u;
    if (items & TLM_RC)       sz += 16u;
    if (items & TLM_ATTITUDE) sz += 6u;
    if (items & TLM_GPS)      sz += 14u;
    if (items & TLM_ANALOG)   sz += 7u;
    return sz;
}

/* Ouvre (ou crée) le fichier télémétrie temporaire pour un enregistrement.
   Doit être appelé juste avant fpv_sl_core0_loop().
   Retourne 0 si OK, -1 si erreur FatFS. */
int8_t tlm_writer_open(uint8_t items, uint8_t protocol, uint8_t sample_rate_hz);

/* Écrit un record brut packagé (construit par msp_get_telemetry_record).
   No-op si le writer n'est pas ouvert ou si len == 0. */
int8_t tlm_writer_write(const uint8_t *buf, uint8_t len);

/* Ferme et renomme le fichier télémétrie.
   Utilise le même index, folder et prefix que le fichier WAV associé.
   No-op si le writer n'est pas ouvert. */
int8_t tlm_writer_close(uint16_t file_index, const char *folder, const char *prefix);
