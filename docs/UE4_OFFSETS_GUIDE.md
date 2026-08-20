# 🧭 Guía de Offsets e Ingeniería Inversa en Unreal Engine 4 (Arena Breakout)

> **LINX_MCP_LECTOR — Powered by [uam.lol/c](https://uam.lol/c)**

Este documento detalla la estructura interna de memoria del motor **Unreal Engine 4** en Android ARM64 para *Arena Breakout*, cómo resolver sus estructuras globales, cómo funciona la calibración de offsets en tiempo real y qué áreas están abiertas para contribuciones de la comunidad.

---

## 🗺️ 1. Jerarquía de Punteros en UE4 (World ➔ Actores)

```text
libUE4.so (Base)
  │
  ├── GWorld (Offset global)
  │     │
  │     └── PersistentLevel (+0x30)
  │           │
  │           └── AActors (Array TArray<AActor*>) (+0x98)
  │                 │
  │                 ├── [0] LocalPlayer (BP_UamCharacter_C)
  │                 │     ├── RootComponent (+0x158) ➔ Posición 3D (X, Y, Z)
  │                 │     ├── Mesh (CharacterMesh0) (+0x370)
  │                 │     │     ├── BoneTransforms (+0x5C0) ➔ Matriz de Huesos
  │                 │     │     └── ComponentToWorld (+0x250) ➔ Transformación Mundial
  │                 │     └── PlayerState (+0x290) ➔ Nickname, Team ID, Camp ID
  │                 │
  │                 ├── [1] Enemy Player 1
  │                 ├── [2] Bot / Scav
  │                 └── [N] Loot Boxes / Safes / Items
  │
  ├── GUObjectArray (Array global de objetos UObject)
  │
  └── FNamePool (Tabla global de nombres FName)
```

---

## 🧬 2. Estructuras Clave en C++ (`Structs.h`)

### A. Vector 3D y Transformación de Coordenadas
```cpp
struct FVector {
    float X, Y, Z;
};

struct FRotator {
    float Pitch, Yaw, Roll;
};

struct FTransform {
    FQuat Rotation;
    FVector Translation;
    float pad;
    FVector Scale3D;
};
```

### B. Matriz de Huesos (`BoneList`)
El orden anatómico de los índices de huesos en el skeletal mesh de Arena Breakout es:
```cpp
inline int AB_BoneList[][2] = {
    {15, 82}, // Clavícula -> Cuello
    {15, 1},  // Clavícula -> Pelvis
    {15, 53}, // Clavícula -> Hombro izquierdo
    {53, 54}, // Hombro izq -> Codo izquierdo
    {54, 87}, // Codo izq -> Muñeca izquierda
    {15, 23}, // Clavícula -> Hombro derecho
    {23, 24}, // Hombro der -> Codo derecho
    {24, 86}, // Codo der -> Muñeca derecha
    {1, 2},   // Pelvis -> Cadera izquierda
    {2, 4},   // Cadera izq -> Rodilla izquierda
    {4, 92},  // Rodilla izq -> Talón izquierdo
    {1, 7},   // Pelvis -> Cadera derecha
    {7, 9},   // Cadera der -> Rodilla derecha
    {9, 94},  // Rodilla der -> Talón derecho
};
```

---

## 🎯 3. Calibración en Vivo (Hot-Tuning de Offsets)

**LINX_MCP_LECTOR** incluye un sistema de variables dinámicas que permite a la IA o al usuario reajustar los offsets en tiempo real **sin necesidad de recompilar el binario en C++ ni reiniciar el juego**:

| Clave de Configuración | Offset por Defecto | Descripción |
|---|---|---|
| `"camera"` | `0x1100` | Matriz de ViewProjection / POV de la cámara del jugador. |
| `"persistent_level"` | `0x30` | Puntero a `ULevel` dentro de `UWorld`. |
| `"actors"` | `0x98` | Offset del array `TArray<AActor*>` dentro de `ULevel`. |
| `"root_comp"` | `0x158` | Puntero `USceneComponent*` dentro de `AActor`. |
| `"mesh"` | `0x370` | Puntero `USkeletalMeshComponent*` dentro del personaje. |
| `"bone_array"` | `0x5C0` | Array de matrices de transformación de huesos. |
| `"comp_to_world"` | `0x250` | Matriz 4x3 de transformación de componente a mundo. |

### Cómo cambiar un offset en caliente:
* **Desde la Web UI**: Pestaña *UE4 World* ➔ Sección *Offset Tuner*.
* **Vía MCP (IA)**:
  ```json
  {
    "tool": "mem_set_ue4_config",
    "arguments": { "key": "actors", "value": "0xA0" }
  }
  ```
* **Vía Socket TCP directo**:
  ```text
  set_config actors 0xA0
  ```

---

## 🔬 4. Volcado y Reparación de `libUE4.so` (`elf_fixer.cpp`)

Cuando un juego protegido se carga en la memoria de Android, el cargador dinámico y los sistemas de protección a menudo borran u ofuscan la tabla de secciones del archivo ELF (`SHT_NULL`, `PT_LOAD` modificados).

El módulo `elf_fixer.cpp` incluido en este proyecto:
1. Lee los segmentos `PT_LOAD` directamente de la RAM.
2. Reconstruye las cabeceras `Elf64_Ehdr` y `Elf64_Phdr`.
3. Restaura las secciones de código ejecutable (`.text`), datos (`.rodata`, `.data`, `.bss`) y tablas de símbolos dinámicos (`.dynsym`, `.dynstr`).
4. Genera un archivo `.so` listo para ser analizado sin errores en **IDA Pro 7.x / 8.x** o **Ghidra**.

---

## 🤝 5. Oportunidades de Contribución para la Comunidad

Si eres desarrollador o investigador de ingeniería inversa, estas son áreas clave donde puedes contribuir:

1. **Matriz de Cámara WorldToScreen (W2S)**:
   - Afinar la extracción de la rotación Pitch/Yaw/FOV desde `APlayerCameraManager` para una proyección 100% libre de jitter en diferentes relaciones de aspecto (20:9, 16:9).
2. **Filtros de Loot por Tipo y Rareza**:
   - Mapeo de los IDs de ítems de alto valor (Tarjetas de acceso, leones de oro, llaves maestras).
3. **Soporte para Nuevas Versiones de Unreal Engine**:
   - Adaptar las firmas de escaneo (`ue4_auto_scanner.cpp`) para detectar automáticamente `GWorld` tras parches del juego.

---

> Más información, tutoriales y comunidad: **[uam.lol/c](https://uam.lol/c)**
