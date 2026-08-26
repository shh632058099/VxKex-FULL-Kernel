# CLAUDE_CONTEXT.md

> Memoria **viva** del proyecto. Se actualiza cada vez que se completa una tarea importante o se toma una decisión técnica relevante. A diferencia de [CLAUDE.md](CLAUDE.md) (permanente), este archivo refleja el estado *actual* y cambia con frecuencia.
>
> Última actualización: **2026-08-25** — sesión de merge inicial de los 3 forks + primer build de verificación.

## Estado actual del desarrollo

El repositorio estaba **vacío** (un único "Initial commit" con solo `.gitattributes`, sin código) al comenzar esta sesión. En esta sesión se pobló con el árbol de código fusionado de los 3 forks de VxKex, y se verificó que **sí compila de verdad** (ver sección siguiente). El árbol fusionado quedó commiteado a git.

## Estado de compilación verificado (2026-08-25)

Esta máquina tiene **Visual Studio 18 Enterprise** instalado en `F:\Program Files\Microsoft Visual Studio\18\Enterprise`, pero **ninguno** de los toolsets que el proyecto pide explícitamente (`Windows7.1SDK`, `v100`, `v110_xp`, `v120_xp`, `v140_xp`) está presente. El toolset XP-compatible más cercano disponible es **`v141_xp`** (de VS2017), así que se usó como sustituto para un build de prueba real:

- **`Platform=x64`, `Configuration=Release` → ✅ compila y enlaza limpio, 0 errores.** Los ~40 proyectos de la solución generaron sus 20 binarios (`.dll`/`.exe`), incluyendo `KexDll.dll` (el núcleo inyectado), `KxSChanl.dll` (pila TLS completa, 182 funciones), `VxKexLdr.exe`, `CpiwBypaLdr.exe`. Esto **confirma que el árbol fusionado es sano** — no es una mezcla rota de código incompatible.
- **`Platform=Win32`, `Configuration=Release` → ⚠️ 37/40 proyectos compilan y enlazan bien**, pero **3 fallan** con `LNK2001: símbolo externo __except_handler4 sin resolver`:
  - `KexDll.vcxproj` (el núcleo — el más importante)
  - `KexShlEx.vcxproj`
  - `CpiwBypaLdr.vcxproj`

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
| Binarios precompilados: `icu.dll`, `vulkan-1.dll`, `XAudio2_9.dll`, `mfdevmgr.dll` | i486 (`02-Prebuilt DLLs`) | NEXT ya trae `icuuc.dll` (parcial ICU) pero no el resto. No integrados en este merge — no hay ningún `.vcxproj` en NEXT que los referencie, así que copiarlos sin más no cambia nada; haría falta wiring real en el instalador. | Bajo (son binarios de terceros, no hay riesgo de romper compilación, pero tampoco aportan nada sin integrarlos al instalador). |
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

1. **Instalar el toolset `v100`, `v140_xp` o el Windows SDK 7.1 standalone** para conseguir un build Win32 Release 100% limpio (hoy falla solo por el sustituto `v141_xp` en 3 proyectos — ver "Estado de compilación verificado"). x64 ya compila limpio con `v141_xp`.
2. **Diffear release por release el Changelog.txt de i486** (builds 1.1.0.1291 → 1.2.1.2229) contra el Changelog.txt de NEXT para extraer qué fixes/features del upstream original no están reconciliados, y portarlos manualmente uno por uno (no en bloque).
3. **Revisar las brechas de la tabla de arriba**, empezando por las de riesgo "Alto"/"Medio": el RNG shim y los fixes upstream de i486.
4. **Decidir sobre la licencia** — preguntar al usuario o contactar a vxiiduu (autor de i486) antes de publicar o redistribuir.
5. **Configurar un entorno de build real** (VM Windows con VS2010/Windows SDK 7.1) para poder validar cambios futuros, en vez de editar código a ciegas indefinidamente.
6. Considerar si conviene rastrear la relación con los repos origen vía `git remote` (por si en el futuro se quiere traer actualizaciones nuevas de i486/NEXT con `git fetch` + merge selectivo) en vez de solo el snapshot actual.

## Cómo mantener actualizado este archivo

Después de completar una tarea significativa (nuevo módulo integrado, bug corregido, decisión de arquitectura tomada, brecha resuelta o descartada), actualizar la sección correspondiente arriba:
- Mover ítems de "Brechas conocidas" a "Funcionalidades implementadas" cuando se integren y compilen con éxito.
- Añadir nuevas decisiones técnicas con su justificación breve.
- Actualizar la fecha en el encabezado.
- Mantener "Próximos pasos" con máximo 5-7 ítems priorizados, no una lista acumulativa infinita.
