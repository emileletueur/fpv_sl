# PIO & State Machines du RP2040 — Maîtriser l'I2S de A à Z

Je vais te faire un cours complet sur le PIO du RP2040, en utilisant l'I2S comme fil conducteur. C'est le meilleur exemple possible car il exige une synchronisation cycle-exact entre horloge et données.

---

## 1. Architecture PIO du RP2040

Le RP2040 possède **2 blocs PIO** (PIO0 et PIO1), chacun contenant **4 state machines** (SM) indépendantes. Chaque bloc PIO dispose d'une mémoire d'instructions partagée de **32 instructions** (32 slots, pas plus).

### Pourquoi le PIO existe

Le problème fondamental : les périphériques matériels classiques (SPI, I2C, UART) sont figés. Si ton protocole dévie un peu (comme l'I2S qui ressemble à du SPI mais avec un signal Word Select en plus), tu es coincé. Le PIO te donne des **machines à états programmables en assembleur** qui manipulent directement les GPIO, cycle par cycle, avec un timing déterministe absolu.

### Les ressources de chaque State Machine

Chaque SM possède :

- **2 registres de shift** : ISR (Input Shift Register) et OSR (Output Shift Register), chacun de 32 bits
- **2 registres scratch** : X et Y (32 bits chacun, usage général)
- **Un compteur de programme** (PC) propre
- **Un diviseur d'horloge** fractionnaire (16 bits entier + 8 bits fraction) qui dérive l'horloge système
- **Des FIFOs** TX et RX (4 mots de 32 bits chacune) pour communiquer avec le CPU
- **Un mappage de pins** configurable : chaque SM peut contrôler n'importe quels GPIO

### Le jeu d'instructions — 9 instructions, c'est tout

L'assembleur PIO ne comporte que 9 instructions. Chacune s'exécute en **exactement 1 cycle** (sauf si un délai ou une condition de blocage est ajouté) :

| Instruction | Rôle |
|---|---|
| `jmp` | Saut conditionnel ou inconditionnel |
| `wait` | Bloque jusqu'à ce qu'une condition sur un pin ou IRQ soit vraie |
| `in` | Shift des bits depuis une source vers l'ISR |
| `out` | Shift des bits depuis l'OSR vers une destination |
| `push` | Pousse l'ISR vers le FIFO RX (vers le CPU) |
| `pull` | Tire un mot du FIFO TX (depuis le CPU) vers l'OSR |
| `mov` | Copie entre registres |
| `irq` | Lève ou attend un flag IRQ (communication inter-SM) |
| `set` | Écrit une valeur immédiate (0–31) sur des pins ou registres |

### Le mécanisme de délai (side-set et delay)

Chaque instruction a un champ de 5 bits partagé entre **delay** et **side-set** :

- **Delay** : ajoute 0 à 31 cycles d'attente APRÈS l'exécution de l'instruction. L'instruction elle-même prend 1 cycle, donc `nop [3]` prend 4 cycles au total (1 + 3).
- **Side-set** : permet de changer l'état d'un ou plusieurs GPIO **en même temps** que l'instruction s'exécute. C'est fondamental pour générer des horloges.

Si tu configures 1 bit de side-set, il te reste 4 bits pour le delay (max 15). Avec 2 bits de side-set, max delay = 7. C'est un compromis.

---

## 2. Le protocole I2S — Norme Philips

### Les signaux

L'I2S utilise 3 lignes (côté maître, notre cas) :

- **BCK** (Bit Clock) : horloge qui cadence chaque bit de données
- **WS** (Word Select / LRCLK) : indique canal gauche (0) ou droit (1). Transition 1 bit AVANT le premier bit du canal correspondant (c'est le piège de la norme Philips !)
- **SD** (Serial Data) : les données, MSB first, échantillonnées sur le front montant de BCK

### Timing critique de la norme Philips

```
        ┌──┐  ┌──┐  ┌──┐  ┌──┐  ┌──┐  ┌──┐  ┌──┐  ┌──┐  ┌──┐
BCK  ───┘  └──┘  └──┘  └──┘  └──┘  └──┘  └──┘  └──┘  └──┘  └──
        │     │     │     │     │
WS   ───┘     │     │     │     └──────────────────────────────── ...
        │  b31   b30   b29  ...              b31   b30
        │  ◄─── Canal Gauche ──►  │  ◄─── Canal Droit ──►
        │                         │
     WS transite ici           WS transite ici
     1 BCK AVANT le            1 BCK AVANT le
     MSB du canal L            MSB du canal R
```

Points clés :
- WS change sur le **front descendant** de BCK
- Les données sont valides sur le **front montant** de BCK
- WS transite **1 cycle d'horloge avant** le MSB — c'est ça le "Philips standard"
- Typiquement 16 ou 24 ou 32 bits par canal, pour un total de 32 ou 48 ou 64 BCK par frame stéréo

### Relation des fréquences

Pour de l'audio 48 kHz, 32 bits par canal :

- **WS (LRCLK)** = 48 kHz (fréquence d'échantillonnage)
- **BCK** = 48000 × 2 × 32 = 3.072 MHz
- **MCLK** (si nécessaire, certains codecs en ont besoin) = 256 × Fs = 12.288 MHz

---

## 3. Programmation PIO pour I2S — Étape par étape

### 3.1 Stratégie de conception

On va utiliser **2 state machines** synchronisées dans le même bloc PIO :

- **SM0** : génère BCK et WS (les horloges)
- **SM1** : émet les données sur SD, synchronisée avec SM0

Pourquoi 2 SM ? Parce que générer BCK + WS + SD dans une seule SM consommerait trop de cycles. Séparer permet de garder chaque programme simple et le timing parfait.

### 3.2 Programme PIO pour les horloges (BCK + WS)

Voici le programme ASM PIO commenté en détail :

```pio
; ================================================================
; I2S Clock Generator — BCK et WS
; ================================================================
; Side-set : 1 bit = BCK (on toggle BCK avec le side-set)
; SET pins : 1 pin = WS
; 
; Concept : on utilise le registre X comme compteur de bits.
; Pour 32 bits par canal (64 BCK par frame), on compte 
; les fronts de BCK et on toggle WS au bon moment.
;
; Convention side-set :
;   side 0 = BCK bas
;   side 1 = BCK haut

.program i2s_clock
.side_set 1        ; 1 bit de side-set = BCK, il reste 4 bits delay

; --- Canal Gauche (WS = 0) ---
    set pins, 0         side 0      ; WS = 0 (canal gauche), BCK descend
                                     ; C'est ici que WS transite, 1 BCK avant le MSB
    set x, 30           side 0      ; X = 30 (on va compter 31 paires de fronts)
                                     ; 31 et non 32 car le premier bit est "consommé"
                                     ; par l'instruction set pins elle-même
left_loop:
    nop                  side 1      ; BCK monte (front montant = data valide)
    jmp x-- left_loop    side 0      ; BCK descend, décrément X, boucle si X > 0
    
    nop                  side 1      ; 32ème front montant

; --- Canal Droit (WS = 1) ---
    set pins, 1          side 0      ; WS = 1 (canal droit), BCK descend
    set x, 30            side 0      ; X = 30, même logique
right_loop:
    nop                  side 1      ; BCK monte
    jmp x-- right_loop   side 0      ; BCK descend, décrément
    
    nop                  side 1      ; 32ème front montant
                                     ; Le programme boucle (wrap) au début
```

**Analyse cycle par cycle du timing :**

Chaque itération de la boucle fait exactement 2 instructions = 2 cycles SM = 1 période de BCK. Donc la fréquence de BCK = fréquence SM / 2.

Pour BCK = 3.072 MHz avec une horloge système de 125 MHz :
```
div = 125_000_000 / (3_072_000 × 2) = 20.345
```
On configure le diviseur fractionnaire à 20 + 88/256 ≈ 20.344 (très proche).

### 3.3 Programme PIO pour les données (SD)

```pio
; ================================================================
; I2S Data Output — SD
; ================================================================
; OUT pins : 1 pin = SD (data line)
; Shift : MSB first, autopull à 32 bits
;
; Cette SM doit être synchronisée avec la SM horloge.
; Elle est cadencée à la MÊME fréquence et démarrée simultanément.
; 
; Principe : pour chaque bit clock, on sort 1 bit sur SD.
; On utilise autopull : quand l'OSR est vide après 32 bits,
; le hardware tire automatiquement le mot suivant du FIFO TX.

.program i2s_data
.side_set 0         ; pas de side-set ici

    ; Attente initiale pour s'aligner avec le programme d'horloge
    ; Le premier bit (MSB) doit apparaître sur SD quand BCK monte
    ; Comme la SM horloge fait : set WS / set X / [nop side1 / jmp side0]
    ; on doit retarder de 2 cycles avant de commencer à sortir des bits
    
bitloop:
    out pins, 1      [1]   ; Sort 1 bit MSB de l'OSR sur SD
                            ; [1] = 1 cycle de délai supplémentaire
                            ; Total = 2 cycles par bit = synchrone avec BCK
    jmp bitloop             ; Boucle infinie, autopull fait le reste
```

Le `[1]` après `out pins, 1` est essentiel : sans lui, chaque bit ne durerait qu'un cycle. Avec le délai de 1, on a 2 cycles par bit (out + delay + jmp), ce qui correspond à la période de BCK (2 cycles dans la SM horloge aussi). En fait, `out` prend 1 cycle + 1 delay = 2 cycles, puis `jmp` prend 1 cycle... ça fait 3 ? Non. Corrigeons :

```pio
; Version corrigée — 2 cycles par itération exactement
.program i2s_data

bitloop:
    out pins, 1             ; Sort 1 bit, 1 cycle
    jmp bitloop             ; Boucle, 1 cycle
                            ; Total = 2 cycles = 1 période BCK ✓
```

C'est plus simple et plus correct. Chaque paire `out`/`jmp` = 2 cycles = 1 période de BCK.

### 3.4 Le mécanisme d'Autopull (crucial)

L'autopull est configuré dans le code C, pas dans l'ASM. Quand on configure `autopull = true` avec `pull_threshold = 32`, voici ce qui se passe :

1. L'OSR contient 32 bits
2. Chaque `out pins, 1` shift 1 bit vers la sortie et décrémente le compteur interne de shift
3. Quand 32 bits ont été shiftés, le hardware **automatiquement** tire le mot suivant du FIFO TX dans l'OSR
4. Si le FIFO est vide, l'instruction `out` **bloque** (la SM stalle) jusqu'à ce que le CPU écrive un nouveau mot

C'est ce qui rend le tout élégant : le programme PIO n'a jamais besoin de faire `pull` explicitement.

---

## 4. Code C du SDK — Tout assembler

```c
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"

// Les fichiers .pio.h sont générés par le build system (CMake + pioasm)
#include "i2s_clock.pio.h"
#include "i2s_data.pio.h"

// Assignation des GPIO
#define PIN_BCK   18    // Bit Clock
#define PIN_WS    19    // Word Select
#define PIN_SD    20    // Serial Data

#define SAMPLE_RATE   48000
#define BIT_DEPTH     32
#define BCK_FREQ      (SAMPLE_RATE * 2 * BIT_DEPTH)  // 3.072 MHz

// -----------------------------------------------------------
// Initialisation de la SM horloge (BCK + WS)
// -----------------------------------------------------------
void i2s_clock_program_init(PIO pio, uint sm, uint offset,
                            uint pin_bck, uint pin_ws) {
    
    // Configuration des GPIO pour le PIO
    pio_gpio_init(pio, pin_bck);
    pio_gpio_init(pio, pin_ws);
    
    // Direction : output
    pio_sm_set_consecutive_pindirs(pio, sm, pin_bck, 1, true);
    pio_sm_set_consecutive_pindirs(pio, sm, pin_ws, 1, true);
    
    // Récupérer la config par défaut générée par pioasm
    pio_sm_config c = i2s_clock_program_get_default_config(offset);
    
    // Side-set = BCK
    sm_config_set_sideset_pins(&c, pin_bck);
    
    // SET pins = WS
    sm_config_set_set_pins(&c, pin_ws, 1);
    
    // Diviseur d'horloge pour obtenir la bonne fréquence BCK
    // BCK = clk_sys / (div * 2)  car 2 cycles SM par période BCK
    float div = (float)clock_get_hz(clk_sys) / (BCK_FREQ * 2.0f);
    sm_config_set_clkdiv(&c, div);
    
    // Appliquer la config et activer la SM
    pio_sm_init(pio, sm, offset, &c);
    // NE PAS encore activer — on synchronise le démarrage plus tard
}

// -----------------------------------------------------------
// Initialisation de la SM données (SD)
// -----------------------------------------------------------
void i2s_data_program_init(PIO pio, uint sm, uint offset,
                           uint pin_sd) {
    
    pio_gpio_init(pio, pin_sd);
    pio_sm_set_consecutive_pindirs(pio, sm, pin_sd, 1, true);
    
    pio_sm_config c = i2s_data_program_get_default_config(offset);
    
    // OUT pins = SD
    sm_config_set_out_pins(&c, pin_sd, 1);
    
    // Shift : MSB first, autopull à 32 bits
    sm_config_set_out_shift(
        &c,
        false,    // shift_right = false → MSB first
        true,     // autopull = true
        32        // pull_threshold = 32 bits
    );
    
    // Joindre les FIFOs : on n'utilise que TX, donc on fusionne
    // les 2 FIFOs (RX+TX) en un seul FIFO TX de 8 mots
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
    
    // Même diviseur d'horloge que la SM horloge !
    float div = (float)clock_get_hz(clk_sys) / (BCK_FREQ * 2.0f);
    sm_config_set_clkdiv(&c, div);
    
    pio_sm_init(pio, sm, offset, &c);
}

// -----------------------------------------------------------
// Configuration DMA pour alimenter le FIFO TX en continu
// -----------------------------------------------------------
static int dma_chan;
static int32_t audio_buffer[2][256];  // Double buffer, stéréo entrelacé
static volatile int current_buf = 0;

void dma_handler() {
    // Acquitter l'IRQ
    dma_hw->ints0 = 1u << dma_chan;
    
    // Basculer le buffer
    current_buf ^= 1;
    
    // Relancer le DMA sur le prochain buffer
    dma_channel_set_read_addr(dma_chan, audio_buffer[current_buf], true);
    
    // Ici : remplir audio_buffer[current_buf ^ 1] avec de nouvelles données
    // C'est là que ton code audio intervient
}

void setup_dma(PIO pio, uint sm) {
    dma_chan = dma_claim_unused_channel(true);
    
    dma_channel_config dma_cfg = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&dma_cfg, DMA_SIZE_32);
    channel_config_set_read_increment(&dma_cfg, true);
    channel_config_set_write_increment(&dma_cfg, false);
    
    // DREQ : le DMA attend que le FIFO TX ait de la place
    channel_config_set_dreq(&dma_cfg, pio_get_dreq(pio, sm, true));
    
    dma_channel_configure(
        dma_chan,
        &dma_cfg,
        &pio->txf[sm],              // Destination : FIFO TX de la SM
        audio_buffer[0],             // Source : buffer audio
        256,                         // Nombre de transferts
        false                        // Ne pas démarrer tout de suite
    );
    
    // IRQ quand le transfert est fini → on enchaîne avec l'autre buffer
    dma_channel_set_irq0_enabled(dma_chan, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);
}

// -----------------------------------------------------------
// Main
// -----------------------------------------------------------
int main() {
    stdio_init_all();
    
    PIO pio = pio0;
    
    // Charger les programmes dans la mémoire PIO
    uint offset_clk  = pio_add_program(pio, &i2s_clock_program);
    uint offset_data = pio_add_program(pio, &i2s_data_program);
    
    // SM0 = horloges, SM1 = données
    uint sm_clk  = 0;
    uint sm_data = 1;
    
    // Initialiser (sans démarrer)
    i2s_clock_program_init(pio, sm_clk, offset_clk, PIN_BCK, PIN_WS);
    i2s_data_program_init(pio, sm_data, offset_data, PIN_SD);
    
    // Configurer le DMA
    setup_dma(pio, sm_data);
    
    // Remplir le premier buffer avec du silence (ou un signal test)
    for (int i = 0; i < 256; i++) {
        audio_buffer[0][i] = 0;  // Silence
    }
    
    // Pré-remplir le FIFO pour éviter un underrun au démarrage
    pio_sm_put_blocking(pio, sm_data, 0);
    pio_sm_put_blocking(pio, sm_data, 0);
    
    // Lancer le DMA
    dma_channel_start(dma_chan);
    
    // *** DÉMARRAGE SYNCHRONE DES 2 SM ***
    // C'est critique : les 2 SM doivent démarrer au même cycle exact
    pio_enable_sm_mask_in_sync(pio, (1u << sm_clk) | (1u << sm_data));
    
    // La magie opère maintenant en hardware.
    // Le CPU est libre de faire autre chose.
    while (true) {
        tight_loop_contents();
        // Ici : générer des échantillons audio, traiter des données, etc.
    }
}
```

---

## 5. Concepts avancés et subtilités

### 5.1 `pio_enable_sm_mask_in_sync`

Cette fonction est la clé de la synchronisation. Elle écrit un masque dans le registre `CTRL` du bloc PIO pour activer plusieurs SM **dans la même opération d'écriture bus**. Le hardware garantit qu'elles commencent à exécuter leur première instruction au même cycle d'horloge.

### 5.2 FIFO Join

Par défaut, chaque SM a 4 mots TX + 4 mots RX. Comme notre SM data n'a besoin que de TX, `PIO_FIFO_JOIN_TX` fusionne les deux en un seul FIFO de 8 mots. Cela donne plus de marge avant un underrun et réduit la fréquence à laquelle le DMA doit intervenir.

### 5.3 Communication inter-SM avec IRQ

Si tu as besoin d'une synchronisation plus fine (par exemple, la SM data attend que la SM clock ait fait un événement spécifique), tu peux utiliser les flags IRQ :

```pio
; Dans la SM horloge, au début de chaque frame :
    irq set 0            side 0    ; Lève le flag IRQ 0

; Dans la SM données :
    wait 1 irq 0                   ; Bloque jusqu'à ce que IRQ 0 soit levé
                                    ; Le flag est automatiquement cleared
```

Les IRQ PIO (0–7) sont **internes au bloc PIO** : les SM peuvent se signaler entre elles sans passer par le CPU. Les IRQ 0–3 peuvent aussi déclencher des interruptions CPU si configurées.

### 5.4 La lecture synchrone (I2S en réception — micro MEMS)

Pour lire un micro MEMS I2S, c'est l'inverse : la SM horloge reste identique (on génère toujours BCK et WS en tant que maître), mais la SM données fait de l'input :

```pio
.program i2s_data_input

bitloop:
    in pins, 1                ; Lit 1 bit depuis SD dans l'ISR
    jmp bitloop               ; 2 cycles = 1 période BCK
    
; Avec autopush configuré à 32 bits, quand l'ISR est plein,
; il est automatiquement poussé vers le FIFO RX.
; Le CPU (ou le DMA) récupère les mots de 32 bits.
```

Configuration C correspondante :

```c
void i2s_data_input_init(PIO pio, uint sm, uint offset, uint pin_sd) {
    pio_gpio_init(pio, pin_sd);
    pio_sm_set_consecutive_pindirs(pio, sm, pin_sd, 1, false);  // INPUT
    
    pio_sm_config c = i2s_data_input_program_get_default_config(offset);
    
    sm_config_set_in_pins(&c, pin_sd);
    
    // Shift : MSB first, autopush à 32 bits
    sm_config_set_in_shift(
        &c,
        false,    // MSB first
        true,     // autopush
        32        // 32 bits
    );
    
    // Joindre les FIFOs en RX cette fois
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);
    
    float div = (float)clock_get_hz(clk_sys) / (BCK_FREQ * 2.0f);
    sm_config_set_clkdiv(&c, div);
    
    pio_sm_init(pio, sm, offset, &c);
}
```

### 5.5 Le fichier CMakeLists.txt

Le build system du SDK Pico traite automatiquement les fichiers `.pio` :

```cmake
cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(i2s_pio C CXX ASM)
pico_sdk_init()

add_executable(i2s_pio main.c)

# pico_generate_pio_header transforme les .pio en .pio.h
pico_generate_pio_header(i2s_pio ${CMAKE_CURRENT_LIST_DIR}/i2s_clock.pio)
pico_generate_pio_header(i2s_pio ${CMAKE_CURRENT_LIST_DIR}/i2s_data.pio)

target_link_libraries(i2s_pio
    pico_stdlib
    hardware_pio
    hardware_dma
    hardware_clocks
)

pico_add_extra_outputs(i2s_pio)  # Génère .uf2, .hex, etc.
```

`pico_generate_pio_header` invoque l'outil `pioasm` qui compile le `.pio` en un header C contenant le programme sous forme de tableau d'entiers et des fonctions helper comme `i2s_clock_program_get_default_config()`.

### 5.6 Structure d'un fichier .pio complet

```pio
; i2s_clock.pio — fichier complet
; 
; La directive .program nomme le programme
; La directive .side_set configure le side-set
; Le bloc % c-sdk {} génère du code C helper dans le .pio.h

.program i2s_clock
.side_set 1

    set pins, 0         side 0
    set x, 30           side 0
left_loop:
    nop                  side 1
    jmp x-- left_loop    side 0
    nop                  side 1
    set pins, 1          side 0
    set x, 30            side 0
right_loop:
    nop                  side 1
    jmp x-- right_loop   side 0
    nop                  side 1

% c-sdk {
// Ce bloc est copié tel quel dans le .pio.h
static inline void i2s_clock_program_init(PIO pio, uint sm, uint offset,
                                          uint pin_bck, uint pin_ws) {
    pio_gpio_init(pio, pin_bck);
    pio_gpio_init(pio, pin_ws);
    pio_sm_set_consecutive_pindirs(pio, sm, pin_bck, 1, true);
    pio_sm_set_consecutive_pindirs(pio, sm, pin_ws, 1, true);
    
    pio_sm_config c = i2s_clock_program_get_default_config(offset);
    sm_config_set_sideset_pins(&c, pin_bck);
    sm_config_set_set_pins(&c, pin_ws, 1);
    
    pio_sm_init(pio, sm, offset, &c);
}
%}
```

---

## 6. Debugging et vérification

### Vérifier le timing avec un analyseur logique

Branche un analyseur logique (Saleae, sigrok, etc.) sur BCK, WS, et SD. Ce que tu dois voir :

- BCK : signal carré propre à 3.072 MHz
- WS : transition exactement 1 BCK **avant** le MSB de chaque canal
- SD : données stables pendant la phase haute de BCK, transitions pendant la phase basse

### Erreurs classiques

1. **WS décalé d'un bit** : tu as oublié le cycle d'avance de la norme Philips. Le `set pins` dans le programme clock doit se faire 1 BCK complet avant le premier bit de data.

2. **Audio qui grésille** : underrun du FIFO. Augmente la taille du buffer DMA ou utilise FIFO_JOIN.

3. **Canaux inversés** : WS=0 = gauche en norme Philips. Vérifie la polarité.

4. **Fréquence légèrement fausse** : le diviseur fractionnaire a une résolution de 1/256. Pour certaines fréquences, il faut overclocker le système (par exemple 147.456 MHz qui est un multiple exact de 48000 × 64 × N).

---

## 7. Résumé mental — Le modèle d'exécution PIO

```
    CPU                    PIO Block 0
 ┌────────┐          ┌─────────────────────┐
 │        │  DMA     │  Instruction Memory  │
 │  Code  │─────────►│  (32 slots partagés) │
 │  Audio │          │                      │
 │        │          │  SM0 ──► BCK, WS     │
 │        │  FIFO TX │  SM1 ──► SD (ou ◄─)  │
 │        │─────────►│  SM2    (libre)       │
 │        │          │  SM3    (libre)       │
 └────────┘          └─────────────────────┘
                              │
                         GPIO Pins
                      BCK ──── pin 18
                      WS  ──── pin 19
                      SD  ──── pin 20
```

Le CPU prépare les échantillons audio et les écrit dans la mémoire. Le DMA les transfère automatiquement dans le FIFO TX de la SM données. La SM données sort les bits un par un. La SM horloge génère BCK et WS en parallèle. Tout est en hardware, cycle-exact, sans intervention CPU pendant le streaming.

C'est la puissance du PIO : une fois configuré et lancé, le CPU est entièrement libre pendant que le PIO gère le protocole avec un timing parfait au cycle près.
