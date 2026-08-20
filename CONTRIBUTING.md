# 🤝 Guía de Contribución: LINX_MCP_LECTOR

> **Powered by [uam.lol/c](https://uam.lol/c)**

¡Gracias por tu interés en contribuir a **LINX_MCP_LECTOR**! Este es un proyecto Open Source creado para que la comunidad de desarrollo, reverse engineering e Inteligencia Artificial pueda investigar y crear herramientas de lectura de memoria en Android.

---

## 🎯 Áreas Principales de Interés

1. **Offsets y Firmas para Nuevas Versiones de UE4**:
   - Actualización de `GWorld`, `FNamePool`, `GUObjectArray` en `ue4_auto_scanner.cpp`.
2. **Matriz de Cámara y Perspectiva (WorldToScreen)**:
   - Mejora de la fórmula de proyección 3D a 2D en `src/Main/ESP.h`.
3. **Módulos MCP e Integración con Modelos de Lenguaje**:
   - Creación de nuevas herramientas JSON-RPC para análisis predictivo y telemetría de partida en tiempo real.
4. **Mejoras en la UI Web**:
   - Visualizadores 3D de modelos (Three.js / WebGL), exportador de mapas y gráficos de calor.

---

## 🚀 Flujo de Trabajo para Pull Requests

1. **Haz un Fork** del repositorio.
2. Crea una rama descriptiva para tu funcionalidad:
   ```bash
   git checkout -b feature/nueva-funcionalidad-offsets
   ```
3. Realiza tus cambios asegurándote de que el código compile limpiamente con Android NDK (`build.bat`).
4. Haz commit de tus cambios con mensajes claros:
   ```bash
   git commit -m "feat(ue4): update GWorld signature for v1.0.1"
   ```
5. Sube tu rama a GitHub:
   ```bash
   git push origin feature/nueva-funcionalidad-offsets
   ```
6. Abre un **Pull Request** explicando el cambio y cómo reproducir la prueba.

---

## 📜 Código de Conducta
* Mantén el respeto y la colaboración en todos los issues y discusiones.
* Este proyecto se distribuye con fines educativos, de investigación y desarrollo de software.

> Comunidad y soporte: **[uam.lol/c](https://uam.lol/c)**
