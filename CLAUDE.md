# CLAUDE.md

> Información **permanente** sobre el proyecto. No contiene tareas, decisiones de corto plazo ni estado de desarrollo — eso vive en [CLAUDE_CONTEXT.md](CLAUDE_CONTEXT.md).
>
> **Regla obligatoria:** antes de realizar cualquier cambio importante (nuevo módulo, refactor, cambio de arquitectura, decisión técnica relevante), Claude debe leer **CLAUDE_CONTEXT.md** completo. Ese archivo contiene el estado real del desarrollo, las brechas conocidas entre los forks fusionados y las próximas prioridades. Trabajar sin leerlo puede duplicar esfuerzo o revertir decisiones ya tomadas.

## Qué es este proyecto

**VxKex** es una capa de compatibilidad para Windows que permite ejecutar aplicaciones que dependen de APIs exclusivas de Windows 8/8.1/10/11 sobre versiones anteriores de Windows (principalmente **Windows 7**, y en esta variante también **Windows 8/8.1**).

No es un emulador ni modifica archivos del sistema. Funciona así:

1. Se registra una entrada IFEO (`Image File Execution Options`) con `VerifierDlls` apuntando a `KexDll`.
2. `KexDll` se inyecta en el proceso objetivo al arrancar.
3. `KexDll` reescribe la tabla de importación del `.exe`/`.dll` para que, en vez de importar `kernel32.dll`, `user32.dll`, etc. del sistema, importe las DLLs propias del proyecto (`Kx*`), que reimplementan las funciones que en Windows 7/8/8.1 no existen.

Es software de compatibilidad "dual-use" legítimo (parches de comportamiento, shims de API, spoofing de versión de SO) — **no** contiene malware ni técnicas de evasión maliciosas.

## Origen: este repositorio es un merge de 3 forks

Este árbol de código es el resultado de fusionar tres repositorios que comparten el mismo commit raíz de git (confirmado con `git merge-base`):

| Fork | Autor | Rol en el merge | Última actividad conocida |
|---|---|---|---|
| [i486/VxKex](https://github.com/i486/VxKex) | vxiiduu (autor original) | Upstream canónico. Solo soporta Windows 7. | 2026-07-22 (activo) |
| [tester9071348/VxKex-Extended](https://github.com/tester9071348/VxKex-Extended) | tester9071348 | Fork de i486 (marzo 2024). Añadió `CpiwBypa` y ampliada lista de compatibilidad. | 2025-03-17 (aparentemente inactivo) |
| [YuZhouRen86/VxKex-NEXT](https://github.com/YuZhouRen86/VxKex-NEXT) | YuZhouRen86 | Fork de **Extended** (no de i486 directamente, feb 2025). Añadió localización (i18n), soporte Windows 8/8.1, pila TLS 1.2/1.3 propia (`KxSChanl`), más módulos. | 2026-08-25 (el más activo) |

**Base elegida: VxKex-NEXT**, por ser el fork más completo y más activamente mantenido (hereda las mejoras de Extended y añade las suyas). Un `git merge` literal entre los tres produjo más de 236 conflictos "add/add" sin valor real, porque el historial de cada fork está compuesto por *commits de volcado completo* ("Add files via upload") sin diffs incrementales aprovechables — fusionarlos a ciegas habría producido código C potencialmente incorrecto e imposible de validar sin compilar. En su lugar se hizo un análisis comparativo dirigido, documentado en detalle en [CLAUDE_CONTEXT.md](CLAUDE_CONTEXT.md).

**Importante:** no se encontró archivo `LICENSE` en ninguno de los tres repositorios origen. Ver nota en CLAUDE_CONTEXT.md.

## Arquitectura y estructura de carpetas

```
00-Common Headers/       Headers compartidos por todos los módulos (KexDll.h, KxUser.h, NtDll.h, etc.)
00-Documentation/        Changelog.txt, notas técnicas, lista de compatibilidad de apps (.docx)
00-Import Libraries/     .lib de importación para enlazar contra DLLs del sistema/terceros
01-Development Utilities/  Scripts de build, vautogen (versionado), KexExprt, moz2sst, mlsbdic
01-Extended DLLs/        Las DLLs "Kx*" — el corazón de la reimplementación de APIs (ver tabla abajo)
01-Tests/                Proyectos de prueba manuales (muy limitado, ver CLAUDE_CONTEXT.md)
02-Prebuilt DLLs/        Binarios de terceros pre-compilados que el instalador distribuye (ICU, d12ms, etc.)
02-Prebuilt Data/        Datos de terceros (ICU globalization, diccionarios de ortografía)
CpiwBypa/, CpiwBypaLdr/  Módulo y loader para bypass de compatibilidad (0patch/MacType u otros hooks conflictivos)
KexCfg/                  Lógica de configuración (habilitar/deshabilitar VxKex por aplicación)
KexDll/                  El núcleo: inyección, hooking, redirección de imports, shims NT, sistema de logging .vxl
KexGui/                  Componentes de interfaz gráfica compartidos
KexKMSD/                 Driver de modo KERNEL (.sys) — mínimo/stub (DriverEntry/DriverUnload). Da sentido al nombre "FULL-Kernel" de este repo.
KexMLS/                  Sistema de localización (multi-language support), exclusivo de la línea NEXT
KexPathCch/              Implementación de las API PathCch* (introducidas en Windows 8)
KexSetup/                Instalador (KexSetup.exe)
KexShlEx/                Extensión de shell — la pestaña "VxKex" en Propiedades de un .exe
KexSmp/                  Mapeo de strings (string mapping), módulo dedicado en la línea NEXT
KexW32ML/                Manifest / multi-language helpers de Win32
KxCfgHlp/                Proceso auxiliar de configuración e IPC entre KexShlEx y KexCfg
README_IN_OTHER_LANGUAGES/  README traducido (chino tradicional, ruso, ucraniano, checo)
VxKexLdr/                Loader que lanza procesos con VxKex habilitado
VxlView/                 Visor de logs .vxl (formato propio de KexDll)
VxKex.sln                Solución de Visual Studio — punto de entrada del build
```

### Las DLLs "Extended" (`01-Extended DLLs/`)

Cada una reimplementa una porción del API de Windows moderno:

| Módulo | Área que reimplementa |
|---|---|
| `KxAdvapi` | advapi32.dll (registro, tokens) |
| `KxBase` | kernelbase/kernel32 (app model, procesos, memoria virtual, wow64) |
| `KxCom` | COM / activación WinRT (roapi, actfact, uri) |
| `KxCrt` | Runtime de C |
| `KxCryp` | bcrypt / criptografía |
| `KxDw` | DirectWrite (renderizado de texto) |
| `KxDx` | Direct3D, DXGI, Direct2D, D3D12 stub |
| `KxMi` | powrprof, TBS (TPM), userenv, version spoofing, WLDP, WTS |
| `KxNet` | Redes (NetJoinDomain, etc.) |
| `KxNt` | Capa NT de bajo nivel |
| `KxSChanl` | Pila TLS/SSL completa (handshake TLS 1.2/1.3, HKDF, certificados) — exclusivo de NEXT, más avanzado que el `schannel.c` único de Extended |
| `KxUia` | UI Automation |
| `KxUser` | user32 (ventanas, DPI/scaling, punteros, bluetooth, parches específicos para Chromium/Firefox) |
| `KxXp` | Subconjunto de APIs Vista/Win7 (GetThreadId, GetProcessId, GetTickCount64, grupos de procesador, etc.) ausentes en Windows XP/Server 2003 — módulo nuevo, ver CLAUDE_CONTEXT.md |

## Tecnologías

- **Lenguaje:** C (mayoría), algo de ensamblador x64 (`.asm`, p. ej. `syscal64.asm`), recursos `.rc`
- **Build system:** MSBuild + solución de Visual Studio (`VxKex.sln`), no CMake ni Makefiles
- **Toolsets soportados** (elegibles vía variable de entorno o prompt interactivo en los `.bat`):
  - `Windows7.1SDK` (Windows SDK 7.1 standalone — el que usa CI)
  - `v100` (Visual Studio 2010)
  - `v110_xp` / `v120_xp` / `v140_xp` (VS2012/2013/2015 con soporte XP)
- **Arquitecturas:** x86 y x64. El build de x86 debe hacerse **antes** que x64 (x64 depende de binarios producidos por el build x86).
- **Versionado:** herramienta propia `vautogen` (`01-Development Utilities/vautogen/`), incrementa el build number automáticamente y genera `00-Common Headers/vautogen.h` a partir de `vautogen.ini`.
- **CI/CD:** GitHub Actions (`.github/workflows/build.yml`), corre en `windows-2022`, descarga un Windows 7 SDK empaquetado por terceros, compila Debug+Release para x64+Win32, publica `KexSetup_*.exe` como release al pushear un tag.
- **Instalador:** `KexSetup` + `01-Development Utilities/7zSfx` (SFX autoextraíble basado en 7-Zip).
- **Localización:** `KexMLS` + diccionarios en `02-Prebuilt Data/Dictionaries` + `README_IN_OTHER_LANGUAGES/`.

## Comandos importantes

> ⚠️ Este proyecto **solo se compila en Windows** con Visual Studio o el Windows SDK 7.1 instalado. No hay forma de compilarlo ni probarlo dentro de un entorno Linux/sandbox sin ese toolchain.

Build completo (Debug + Release, x86 + x64), arrastrando el script a una consola de VS/SDK ya inicializada:

```bat
01-Development Utilities\Build_Stable_Version_And_Clean.bat
```

Build individual:

```bat
01-Development Utilities\Build_Debug.bat
01-Development Utilities\Build_Release.bat
```

Limpieza:

```bat
01-Development Utilities\Clean_VxKex_NEXT.bat
```

Build manual vía MSBuild (lo que realmente ejecutan los `.bat`, ver `.github/workflows/build.yml` para el ejemplo completo de CI):

```bat
MSBuild.exe VxKex.sln /p:Configuration=Release /p:Platform=Win32 /p:PlatformToolset=Windows7.1SDK
MSBuild.exe VxKex.sln /p:Configuration=Release /p:Platform=x64   /p:PlatformToolset=Windows7.1SDK
```

No existe suite de pruebas automatizada. `01-Tests/loggingtest` es el único proyecto de prueba y se ejecuta manualmente.

## Convenciones de trabajo para Claude

1. **Leer siempre [CLAUDE_CONTEXT.md](CLAUDE_CONTEXT.md) antes de cambios importantes.** Contiene las brechas de funcionalidad detectadas entre los 3 forks, decisiones ya tomadas y qué falta por reconciliar.
2. **No se puede compilar ni ejecutar este código en este entorno.** Cualquier cambio a archivos `.c`/`.h`/`.asm` debe tratarse con el mismo cuidado que un cambio "a ciegas": revisar la lógica dos veces, preferir cambios mínimos y localizados, y dejar constancia en CLAUDE_CONTEXT.md de qué quedó sin verificar por compilación.
3. **No renombrar ni reorganizar módulos existentes** sin una razón concreta — varios archivos que parecen "faltantes" al comparar forks en realidad fueron renombrados o movidos a otro módulo (ver ejemplos documentados en CLAUDE_CONTEXT.md). Verificar antes de asumir que algo no existe.
4. **Prefijos de nombres:** `Kex*` = componentes propios del núcleo/infraestructura del proyecto. `Kx*` = DLLs de reimplementación de API ("Extended DLLs"). Mantener esta convención al añadir módulos nuevos.
5. **Encabezado de archivos fuente nuevos:** usar la plantilla en `00-Documentation/Source File Header.txt`.
6. **No commitear cambios a git a menos que el usuario lo pida explícitamente.**
7. **Actualizar CLAUDE_CONTEXT.md** cada vez que se complete una tarea importante, se tome una decisión técnica relevante, o se descubra información nueva sobre el estado del proyecto — ese archivo es la memoria viva entre sesiones.
