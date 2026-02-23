### Verify
cmake --version
ninja --version
arm-none-eabi-gcc --version
echo %PICO_SDK_PATH%

### Cleanup
rm -rf build
Remove-Item -Recurse -Force build //windows

### Init configuration
cmake -S . -B build -G Ninja

### Build project
cmake --build build

---

### CMake options

| Option | Default | Description |
|---|---|---|
| `FPV_SL_PICO_PROBE_DEBUG` | `OFF` | Debug avec Pi Pico standard — utilise la LED onboard GP25 (patterns de clignotement) à la place du RGB WS2812 |

#### Usage

```bash
# Session de debug (Pi Pico classique + probe)
cmake -DFPV_SL_PICO_PROBE_DEBUG=ON -S . -B build -G Ninja

# Build production (LED RGB WS2812)
cmake -S . -B build -G Ninja
```

#### FPV_SL_PICO_PROBE_DEBUG — comportement LED

Active `USE_PICO_ONBOARD_LED` dans `status_indicator`, qui remplace les couleurs RGB
par des patterns de clignotement sur GP25 :

| État | Pattern |
|---|---|
| Alimenté / boot | Fixe allumée |
| USB MSC connecté | Blink lent — 500ms |
| Transfert USB | Blink rapide — 100ms |
| Prêt à enregistrer | Double flash répété |
| Enregistrement actif | Triple flash répété |
| Alerte disque (>80%) | Quadruple flash répété |
| Disque critique (>95%) | Blink très rapide — 50ms |
