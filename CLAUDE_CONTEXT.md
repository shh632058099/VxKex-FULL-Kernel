# CLAUDE_CONTEXT.md

> Memoria **viva** del proyecto. Se actualiza cada vez que se completa una tarea importante o se toma una decisión técnica relevante. A diferencia de [CLAUDE.md](CLAUDE.md) (permanente), este archivo refleja el estado *actual* y cambia con frecuencia.
>
> Última actualización: **2026-08-25** — sesión de merge inicial de los 3 forks + build de verificación + módulo `KxXp` (soporte experimental Windows XP/Server 2003) + conexión de `KxXp` al mecanismo de redirección vía capa de fallback en `KxBase`.

## Soporte Windows XP / Server 2003 (2026-08-25)

El usuario pidió incorporar dos proyectos de terceros para extender el soporte a XP/Server 2003: [shorthorn-project/One-Core-API-Binaries](https://github.com/shorthorn-project/One-Core-API-Binaries) y [zeroclear/xpext](https://github.com/zeroclear/xpext). Se investigó la licencia y el contenido real de ambos **antes** de integrar nada:

- **One-Core-API-Binaries: NO se incorporó ningún archivo.** Es la distribución binaria de [shorthorn-project/One-Core-API-Source](https://github.com/shorthorn-project/One-Core-API-Source) (GPL-2.0, "A complete layer to get compatibility on XP/2003 for newer applications"). Al inspeccionar el paquete completo (~1.2GB, `Packages/`) se confirmó que **redistribuye DLLs reales de Microsoft extraídas de instalaciones de Windows más nuevas** (la propia documentación del proyecto dice literalmente "Dxva2 (from win7)" en `Documents/dlladd.txt`). Eso es infracción de copyright de Microsoft, no una cuestión de licencia GPL ambigua — se decidió no copiar nada de ese repo al árbol, pese a que el usuario autorizó asumir el riesgo, porque cruza una línea que no depende del permiso del usuario. Queda solo como referencia/enlace.
  - Arquitectura, para contexto futuro: One-Core-API reemplaza archivos de sistema completos (modelo de "component servicing", como un Windows Update), mientras que VxKex inyecta una DLL vía IFEO y redirige el import table — son mecanismos incompatibles, no es tan simple como "copiar los DLLs".
- **xpext: NO se copió su código** (no tiene licencia declarada — confirmado vía `gh api repos/zeroclear/xpext` → `license: null` — y el propio autor dice en su README que el código suelto "no sirve compilado", pensado solo como referencia). En su lugar se usó su lista de funciones (un hecho, no una expresión protegida por copyright) como especificación funcional para escribir una implementación propia desde cero: el módulo nuevo **`01-Extended DLLs/KxXp/`**.

### Módulo `KxXp` (nuevo, escrito desde cero esta sesión)

Cubre APIs introducidas en Vista/Windows 7 que Windows XP/Server 2003 no tiene nativamente — el mismo hueco que documenta `xpext`, pero con implementación propia basada en las NT API ya usadas en el resto de este código (`NtQueryInformationThread/Process`, técnicas estándar y documentadas, no en el código de xpext).

**Implementado y compila limpio (x64 y Win32 Release, toolset `v141_xp`):**
- `GetThreadId`, `GetProcessId`, `GetProcessIdOfThread` — vía `NtQueryInformationThread`/`NtQueryInformationProcess`.
- `GetTickCount64` — contador de 64 bits sin locks, construido sobre `GetTickCount()` con detección de desbordamiento vía `InterlockedCompareExchange64`.
- `GetActiveProcessorGroupCount`, `GetMaximumProcessorGroupCount` — devuelven 1 (XP/2003 no tienen el concepto de grupos de procesador, introducido en Win7).
- `GetActiveProcessorCount`, `GetMaximumProcessorCount` — vía `GetSystemInfo`.
- `GetCurrentProcessorNumber`, `GetCurrentProcessorNumberEx` — **stub que siempre devuelve el procesador 0.** No hay forma documentada y portable de saber el procesador actual antes de Vista sin asembler no verificado o asumir el layout de `KUSER_SHARED_DATA`/KPCR — se optó por una "mentira segura" (degrada a "sin pista de afinidad", no a comportamiento incorrecto) en vez de arriesgar código no verificable. Documentado en el propio archivo `processor.c`.
- `RaiseFailFastException` — versión simplificada: termina el proceso de forma incondicional e incapturable (la garantía que de verdad importa a quien la llama), pero **no** notifica a los vectored exception handlers registrados como hace la versión real. Documentado en `procthread.c`.

**NO implementado (deliberadamente, por riesgo):** `InitializeSRWLock` y toda la familia SRWLock, `InitializeConditionVariable` y toda la familia ConditionVariable, `InitOnceExecuteOnce` y familia, y las extensiones de fibers (`ConvertThreadToFiberEx`, `FlsAlloc`/`FlsFree`/etc.). Son primitivas de sincronización — un bug ahí puede producir deadlocks o corrupción en cualquier programa que las use, y no hay forma de verificarlas sin un compilador Windows real corriendo pruebas de concurrencia con múltiples hilos, algo que esta sesión no pudo hacer. **Quedan pendientes para quien pueda probarlas de verdad.**

**Conectado a la redirección de imports (2026-08-25, segunda mitad de la sesión):** se implementó la opción de "fallback en runtime" que estaba pendiente de decisión, siguiendo la técnica YY-Thunks y el patrón ya existente en `KxBase/module.c` para `AddDllDirectory` (funciones ausentes en Win7 sin KB2533623):

- Las 3 fuentes de implementación de `KxXp` (`procthread.c`, `tickcount.c`, `processor.c`) ahora **se compilan también dentro de `KxBase`** (referenciadas con ruta relativa `..\KxXp\*.c` en `kxbase.vcxproj`). Sus funciones internas fueron renombradas de `Ext_*` a `Kxxp*` para evitar colisión de símbolos dentro de KxBase. No hay dependencia en runtime de que `KxXp.dll` exista o esté cargado.
- Nuevo archivo **`KxBase/xpfallback.c`**: capa de 11 wrappers exportados (`Ext_GetThreadId`, `Ext_GetTickCount64`, etc.) que en la primera llamada sondean el kernel32 REAL (`LdrGetProcedureAddress` sobre `KexData->BaseDllBase`) con caché en puntero estático; si la función nativa existe (siempre en Vista/7/8/8.1) reenvían a ella — comportamiento idéntico al anterior — y si no existe (XP/Server 2003) caen a las implementaciones `Kxxp*`. El helper deliberadamente NO toca last error al fallar el sondeo (a diferencia de `BasepGetDllDirectoryProcedure`), porque los fallbacks normalmente tienen éxito.
- En `forwards.c` se comentaron los 11 hard-forwards `/EXPORT:X=kernel32.X` correspondientes (quedó intacto `GetThreadIdealProcessorEx`, que no es nuestro) y en `kxbase.def` se añadieron las 11 entradas mapeando al nombre público.
- **Verificado con build real (v141_xp): x64 Release compila toda la solución limpia (21/21 binarios)** y `dumpbin /exports` confirma que las 11 funciones son exportaciones locales de `KxBase.dll` (= Ext_*) mientras `GetThreadIdealProcessorEx` sigue siendo forwarder. `KxXp.dll` Win32 también compila. **Win32: `KxBase` falla al enlazar por el MISMO problema preexistente de `__except_handler4` del toolset sustituto** (ver sección siguiente) — probado por A/B con `git stash`: falla idéntico sin estos cambios, así que no fue introducido aquí; se espera que desaparezca al instalar el toolset real.

**Pendiente del módulo:** empaquetado en `KexSetup` (el instalador aún no lo incluye; hoy es inocuo porque nada depende de `KxXp.dll` en runtime), prueba real sobre Windows XP/2003 (aquí no hay forma de ejecutarlo), y las primitivas de sincronización listadas arriba.

## Estado actual del desarrollo

El repositorio estaba **vacío** (un único "Initial commit" con solo `.gitattributes`, sin código) al comenzar la sesión del merge. En esa sesión se pobló con el árbol de código fusionado de los 3 forks de VxKex, y se verificó que **sí compila de verdad** (ver sección siguiente). El árbol fusionado quedó commiteado a git; la conexión de `KxXp` descrita arriba quedó sin commitear (pendiente de que el usuario lo pida).

## Estado de compilación verificado (2026-08-25)

Esta máquina tiene **Visual Studio 18 Enterprise** instalado en `F:\Program Files\Microsoft Visual Studio\18\Enterprise`, pero **ninguno** de los toolsets que el proyecto pide explícitamente (`Windows7.1SDK`, `v100`, `v110_xp`, `v120_xp`, `v140_xp`) está presente. El toolset XP-compatible más cercano disponible es **`v141_xp`** (de VS2017), así que se usó como sustituto para un build de prueba real:

- **`Platform=x64`, `Configuration=Release` → ✅ compila y enlaza limpio, 0 errores.** Los ~40 proyectos de la solución generaron sus 20 binarios (`.dll`/`.exe`), incluyendo `KexDll.dll` (el núcleo inyectado), `KxSChanl.dll` (pila TLS completa, 182 funciones), `VxKexLdr.exe`, `CpiwBypaLdr.exe`. Esto **confirma que el árbol fusionado es sano** — no es una mezcla rota de código incompatible.
- **`Platform=Win32`, `Configuration=Release` → ⚠️ la mayoría compila y enlaza bien**, pero **4 proyectos fallan** con `LNK2001: símbolo externo __except_handler4 sin resolver`:
  - `KexDll.vcxproj` (el núcleo — el más importante)
  - `KexShlEx.vcxproj`
  - `CpiwBypaLdr.vcxproj`
  - `KxBase.vcxproj` (confirmado como cuarta víctima del mismo problema en la sesión del 25/08 por la tarde, mediante prueba A/B con `git stash`: el fallo es idéntico con y sin los cambios de KxXp, o sea preexistente y no causado por ellos)

  **Causa raíz identificada:** `__except_handler4` es el manejador de SEH (Structured Exception Handling) basado en cookies que usa el CRT antiguo (VS2005–VS2013) cuando se compila x86 con `/SAFESEH`. El toolset `v141_xp` (VS2017) trae un CRT distinto que no expone ese símbolo de la misma forma para binarios x86 antiguos — es una incompatibilidad de ABI entre el toolset legacy que el proyecto espera realmente (`v100`/`v140_xp`/`Windows7.1SDK`) y el sustituto más moderno disponible. **Nota:** SAFESEH solo existe en x86; por eso x64 no se topa con este problema — consistente con lo observado.
  - **No se intentó "arreglar" esto deshabilitando `/SAFESEH` o forzando una lib alternativa** — KexDll es el módulo de seguridad más sensible del proyecto (inyección de DLL, redirección de imports) y tocar su modelo de manejo de excepciones sin poder validar las implicaciones de seguridad/estabilidad sería irresponsable. Instalar el toolset real (`v100` = VS2010, o `v140_xp` = VS2015 con soporte XP, o el Windows SDK 7.1 standalone) es la solución correcta — ver "Próximos pasos".
- No se probó `Configuration=Debug` en esta sesión (se priorizó Release, que es lo pedido).
- Logs completos disponibles en la sesión que hizo el build (no se conservaron en el repo, ya que `Release/`, `x64/`, `Debug/` están en `.gitignore`).

## Metodología del merge (para entender las decisiones tomadas)

1. Se clonaron los 3 repos con historial completo (`git fetch --unshallow`) y se confirmó que comparten el mismo commit raíz.
2. Se descubrió la topología real de forking: **i486 → Extended (marzo 2024) → NEXT (febrero 2025)**. NEXT desciende de Extended, no directamente de i486.
3. Se intentó un `git merge` real de Extended sobre i486: produjo **236 conflictos de 764 archivos**, casi todos `add/add`, porque el historial de los 3 repos está hecho de commits de "volcado completo" (`Add files via upload`) sin diffs incrementales limpios. Un merge automático a ciegas habría requerido resolver cientos de conflictos de código C sin poder compilar para validar — se descartó por ser demasiado arriesgado.
4. Se comparó el contenido de módulos compartidos (`KxDx`, `KxMi`, `KxUser`, `KxCom`, `KxAdvapi`, `KexDll`) archivo por archivo entre los 3 forks. Se confirmó que varios archivos que parecían "faltar" en NEXT en realidad fueron **renombrados o reorganizados**, no perdidos:
   - `KexDll/strmap.c` (Extended) → movido a su propio módulo `KexSmp/strmap.c` en NEXT.
   - `KxCryp/schannel.c` (Extended, un solo archivo) → reemplazado por el módulo completo `KxSChanl/` en NEXT (pila TLS 1.2/1.3 completa: handshake, HKDF, certificados — estrictamente superior).
   - `KexDll/syscal32.c` (i486/Extended) → renombrado a `KexDll/syscal.c` en NEXT.
   - `KxMi/uia.c` (i486) → reemplazado por el módulo dedicado `KxUia/` en NEXT.
   - `01-Development Utilities/moz2sst.exe` (binario precompilado en i486) → NEXT tiene el **código fuente** (`moz2sst.c` + `.vcxproj`), mejor que el binario.
5. Se eligió **VxKex-NEXT como base completa** (vía `git archive` del árbol tracked) por ser el fork más completo y activo.
6. Se añadieron solo los archivos de i486 que son **puramente aditivos y de riesgo cero** (no compiten con nada existente en NEXT): `.gitignore`, y 3 documentos de texto (`ApiSetSchema.txt`, `NCrypt SSL Provider Key Types.txt`, `Notes on Zig.txt`).
7. **No** se copiaron archivos `.c`/`.h` de i486/Extended que aparentaban estar "ausentes" en NEXT sin haber podido confirmar que no estaban duplicados/reorganizados — ver la lista de brechas reales abajo, documentadas para revisión humana con compilador a mano, en vez de arriesgar código no verificado.

Los clones de trabajo (`base_i486`, `fork_extended`, `fork_next`, `merge_test`) quedaron en el directorio scratchpad de la sesión, no en este repositorio.

## Funcionalidades implementadas (heredadas de NEXT, confirmadas presentes)

- Inyección vía IFEO + redirección de tabla de importaciones (mecanismo central, heredado de i486 original).
- Soporte para **Windows 7, 8 y 8.1** (i486 y Extended solo soportan Windows 7; el soporte 8/8.1 se añadió en NEXT v1.2.3.2455).
- Localización completa (i18n): interfaz y READMEs en chino tradicional, ruso, ucraniano, checo (`KexMLS`, `README_IN_OTHER_LANGUAGES/`).
- Pila TLS 1.2 + TLS 1.3 propia con cifrados adicionales (`KxSChanl`, añadido en v1.2.0.2202).
- `CpiwBypa` / `CpiwBypaLdr`: bypass para conflictos con 0patch Agent y MacType (documentado en el Changelog, ej. v1.2.1.2247 fuerza deshabilitar MacType en procesos con VxKex activo).
- Parches específicos por aplicación: Chromium (`chromiumpatch.c`), Firefox (`firefoxpatch.c`), Java (touch en v1.2.2.2378), Qt6, PowerShell (ANSI escape sequences), Unity6.
- Driver de modo kernel `KexKMSD` (`.sys`) — actualmente un **stub mínimo** (`DriverEntry`/`DriverUnload` vacíos, sin lógica real). Da sentido al nombre de este repositorio (`VxKex-FULL-Kernel`), ya que incluye el componente de modo kernel que las descargas binarias normales de VxKex no siempre exponen como fuente.
- Módulos gráficos: `KxDw` (DirectWrite), `KxDx` (Direct3D/DXGI/Direct2D + stub D3D12), `KxUia` (UI Automation) — todos exclusivos de la línea NEXT, no existen en i486.
- Lista amplia de compatibilidad de aplicaciones (Blender, GIMP, HandBrake, Spotify, VSCode, WinDbg, Yuzu, osu!lazer, etc. — ver `00-Documentation/Application Compatibility List.docx` y los README por fork para notas específicas de configuración).

## Brechas conocidas / funcionalidad rescatable NO incorporada todavía

Estas son candidatas reales a portar desde i486/Extended, pero requieren compilar y probar antes de integrarse — **no se copiaron a ciegas**:

| Brecha | Presente en | Detalle | Riesgo de portar sin verificar |
|---|---|---|---|
| Soporte OpenGL (`KxDx/opengl.c`) | i486 | NEXT tiene `dx11.c`/`dx12stub.c`/`direct2d.c`/`dxgi.c` pero no OpenGL. Podría ser una regresión real (afecta apps como GIMP con ciertos backends). | Medio — necesita revisar si NEXT resolvió esto de otra forma antes de sumar el archivo. |
| `KxDx/d3d11on12.c` | i486 | NEXT usa `dx11.c` en su lugar — podría ser un enfoque alternativo ya cubierto, o una pieza distinta. Sin compilar no se puede confirmar solapamiento de símbolos. | Medio |
| IME / Input Method Editor (`KxMi/imm.c`) | i486 | Relevante para entrada de texto en idiomas CJK — llamativo que falte justamente en el fork con más foco en localización. | Bajo-medio, revisar primero si haría falta para los idiomas ya soportados (RUS/UKR/CZE/CHT no son IME-heavy, pero CHT sí puede beneficiarse). |
| `KxUser/msgbox.c` | i486 | Podría estar cubierto por `window.c`/`misc.c` en NEXT bajo otra organización — **verificar antes de portar**, no asumir que falta. | Bajo (probable duplicado) |
| RNG shim (`KexDll/rtlrng.c`) | Extended | No aparece en NEXT bajo ningún nombre obvio. Genuinamente podría faltar. | Medio — funciones de números aleatorios son sensibles a compatibilidad. |
| ~~Binarios precompilados: `icu.dll`, `vulkan-1.dll`, `XAudio2_9.dll`, `mfdevmgr.dll`~~ **RESUELTO 2026-08-26** | i486 | Copiados desde i486/VxKex a `02-Prebuilt DLLs\x86|x64` (más `dwrw10.dll` actualizado a la versión 3 MB que usa el release oficial 1.2.1.2229) y datos ICU completos a `02-Prebuilt Data\KexDir\Globalization\ICU\` (icudtl.dat de 27 MB + 4 `.res`). El makesfx.bat existente los empaqueta solo (wildcards). Motivado por: el primer installer CI (v1.0.0) pesaba 4.8 MB vs 12.45 MB del oficial por esta falta. `Kx.dll` del instalador i486 = su KxMi renombrado vía TargetName — nosotros ya lo enviamos como KxMi.dll, no falta nada. | Resuelto |
| Fixes upstream de i486 no reconciliados | i486 | i486 continuó desarrollo independiente **después** del punto de fork (marzo 2024) con ~22 commits/80 commits totales de diferencia según el punto de comparación, llegando hasta el build `1.2.1.2229` (2026-07-22). Ninguno de esos cambios está en la línea Extended→NEXT. Se desconoce el contenido exacto sin diffear release por release. | **Alto** — esto es lo más importante para revisar a fondo en una sesión futura con más tiempo/compilador. Ver "Próximos pasos". |
| Últimos commits de Extended no heredados por NEXT | Extended | Extended siguió activo unas semanas después de que NEXT se bifurcara de él (hasta 2025-03-17). Contenido no auditado. | Medio |

## Problemas conocidos

- **No hay archivo `LICENSE`** en ninguno de los 3 repos origen. El README original solo dice "si eres desarrollador, el código fuente está disponible" — no aclara términos de uso/redistribución. **Esto debería aclararse con el usuario o con el autor original (vxiiduu) antes de redistribuir binarios o hacer el repo público.**
- El árbol **sí compila** (ver "Estado de compilación verificado" arriba) en x64 Release. En Win32 Release, 3 de 40 proyectos (incluyendo `KexDll`, el núcleo) no enlazan con el toolset sustituto `v141_xp` por incompatibilidad de ABI de SEH (`__except_handler4`) — se espera que esto se resuelva instalando el toolset real del proyecto (`v100`/`v140_xp`/`Windows7.1SDK`), no es un defecto del merge en sí.
- `KexKMSD.sys` es un stub sin lógica real — si el objetivo del proyecto es tener funcionalidad de kernel real, ahí hay trabajo pendiente de diseño, no solo de "fusión".
- Los archivos `Changelog.txt` y varios `.txt` en `00-Documentation` están en UTF-16 (formato original de Windows); tenerlo en cuenta al editarlos para no corromper la codificación.
- `01-Tests/loggingtest` es la única prueba existente y es manual — no hay CI de pruebas, solo CI de compilación.

## Decisiones técnicas tomadas

- **Base del merge: VxKex-NEXT**, no i486 ni Extended. Motivo: es la más completa (hereda Extended + i486 original + sus propias features) y la más activamente mantenida (commits hasta el mismo día de este merge).
- **Se descartó un `git merge` automático de los 3 remotos.** Motivo: el historial de "volcado completo" por commit hace que git no tenga contexto fino para resolver conflictos de forma confiable; se prefirió copiar el árbol completo de NEXT (`git archive`) y añadir solo lo verificablemente aditivo.
- **Se documentan las brechas en vez de "adivinar" el merge de código C de bajo nivel** (syscalls, hooks, NT internals) sin poder compilar — el costo de un bug silencioso en este tipo de código (inyección de DLL, redirección de imports) es alto, y "parece razonable" no es suficiente sin pruebas reales.

## Próximos pasos recomendados

1. **Instalar el toolset `v100`, `v140_xp` o el Windows SDK 7.1 standalone** para conseguir un build Win32 Release 100% limpio (hoy falla solo por el sustituto `v141_xp` en 4 proyectos — ver "Estado de compilación verificado"). x64 ya compila limpio con `v141_xp`.
2. **Diffear release por release el Changelog.txt de i486** (builds 1.1.0.1291 → 1.2.1.2229) contra el Changelog.txt de NEXT para extraer qué fixes/features del upstream original no están reconciliados, y portarlos manualmente uno por uno (no en bloque).
3. **Revisar las brechas de la tabla de arriba**, empezando por las de riesgo "Alto"/"Medio": el RNG shim y los fixes upstream de i486.
4. **Decidir sobre la licencia** — preguntar al usuario o contactar a vxiiduu (autor de i486) antes de publicar o redistribuir.
5. **Configurar un entorno de build real** (VM Windows con VS2010/Windows SDK 7.1, y una VM con Windows XP/2003 para probar KxXp de verdad).
6. **Empaquetar `KxXp.dll` en `KexSetup`** y commitear los cambios de conexión de KxXp (quedaron sin commit a propósito). Hoy nada depende de ese DLL en runtime, así que añadirlo es seguro e incremental.
7. **Implementar (con pruebas de concurrencia reales) las primitivas de sincronización que `KxXp` dejó pendientes**: SRWLock, ConditionVariable, InitOnce, extensiones de fibers.

## Cómo mantener actualizado este archivo

Después de completar una tarea significativa (nuevo módulo integrado, bug corregido, decisión de arquitectura tomada, brecha resuelta o descartada), actualizar la sección correspondiente arriba:
- Mover ítems de "Brechas conocidas" a "Funcionalidades implementadas" cuando se integren y compilen con éxito.
- Añadir nuevas decisiones técnicas con su justificación breve.
- Actualizar la fecha en el encabezado.
- Mantener "Próximos pasos" con máximo 5-7 ítems priorizados, no una lista acumulativa infinita.
