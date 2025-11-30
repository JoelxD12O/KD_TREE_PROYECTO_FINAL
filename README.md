# KD-Tree Visualizer — Proyecto Final

Este repositorio contiene un visualizador interactivo de un KD‑Tree hecho en C++ usando SFML (versión 3). El foco principal de este documento es describir con detalle la demo interactiva (modo "hospital") que implementamos, cómo usarla, cómo personalizarla y cómo resolver problemas de compilación/ejecución que surgieron durante el desarrollo.

**No** se explican aquí los detalles internos de los algoritmos (nearest neighbor, range search, inserción), sólo se mencionan cuándo y dónde se usan en la demo.

**Archivos principales**
- `Visualizer.cpp`: UI, generación de demo, animaciones paso a paso, dibujo del plano y del árbol.
- `KDTree.cpp` / `KDTree.h`, `main.cpp`: implementación del KD‑Tree y el punto de entrada.
- `CMakeLists.txt`: configuración de compilación.

**Objetivo del cambio**
- Migrar el visualizador a SFML v3 y ajustar la UI/laying para fullscreen.
- Implementar una demo sintética (modo "hospital") con pocos puntos para su visualización clara.
- Añadir animaciones paso a paso para mostrar comparaciones, visitas y podas en las búsquedas.

---

**Demo (modo "hospital") — explicación detallada**

Qué es
- La demo es un dataset sintético de pacientes y sirve para mostrar visualmente cómo el KD‑Tree organiza puntos en 2D y cómo funcionan las búsquedas (nearest neighbor y range search) con animación.

Cómo se activa
- Pulsa el botón `Demo` en el panel izquierdo de la interfaz (junto a `Añadir`, `Buscar`, `Rango`).

Qué hace exactamente al activarse
- Reinicia el KD‑Tree (`tree = KDTree()`), limpia vectores auxiliares y genera `N` puntos aleatorios.
- Por cada punto genera una edad aleatoria (20..90) y la guarda en `puntosAge` (vector paralelo a `puntos`).
- Inserta los puntos en el árbol y los pinta en el plano.
- Actualmente `N` está fijado a `25` para que los puntos se vean bien en la pantalla.

Cómo se visualizan los atributos
- **Color:** la edad se mapea a color mediante `mapAgeToColor(age)` (gradiente azul→rojo).
- **Tamaño:** la edad se mapea a radio con `mapAgeToRadius(age)` (4..10 px aprox.).
- **Etiquetas:** al seleccionar un punto demo se muestra su índice, edad y coordenadas (WBC, BP en el ejemplo de datos).

Interacción y controles relevantes para la demo
- Click en un punto: selecciona el punto (`selectedIndex`) y muestra detalles.
- `demoNeighbors`: vector con índices de vecinos resaltados — se usan para marcar k vecinos.
- `Rango` (botón) + arrastrar mouse: dibuja un rectángulo para búsqueda por rango.
- `Buscar` con coordenadas X/Y en los inputs inicia una búsqueda nearest con animación.

Animaciones asociadas
- Generadores de animación:
  - `generateInsertAnimation(...)`: pasos para mostrar la inserción (comparaciones y punto objetivo).
  - `generateSearchAnimation(...)`: pasos que simulan la búsqueda nearest neighbor con poda (se añaden pasos de visita y poda).
  - `generateRangeSearchAnimation(...)`: pasos para la búsqueda por rango (visitas, adición a resultados, podas).
- `AnimationState` controla reproducción (pausa, velocidad `stepDuration`, avance automático, índice de paso).
- Panel inferior: mientras hay animación muestra descripción del paso, contador, y tiempo real de ejecución medido.

Dónde modificar el tamaño del dataset (N) y k
- Abrir `Visualizer.cpp` y buscar el bloque que maneja el botón `Demo` (handler del click). Allí se define:
  - `const int N = 25;`  ← cambia este número para generar más/menos puntos.
  - `int demoK = 5;`  ← cambiando `demoK` se controla cuántos vecinos se consideran visualmente.

Cómo ajustar apariencia y layout
- La ventana se abre en fullscreen y el layout coloca el **plano (panel izquierdo)** ocupando la mitad izquierda de la pantalla y el **árbol (panel derecho)** ocupando la mitad derecha. Los parámetros calculados en runtime son:
  - `PLANE_WIDTH = WINDOW_WIDTH / 2`
  - `PLANE_HEIGHT = WINDOW_HEIGHT`
  - `TREE_ORIGIN_X = PLANE_WIDTH` y `TREE_WIDTH` se calcula con padding.
- Los ejes, cuadrícula y planos de partición se dibujan con `drawAxesAndGrid(...)` y `drawKDLines(...)`.

Cómo ejecutar la demo (build / run)
1. Desde la raíz del repo:
```bash
rm -rf build
cmake -S . -B build
cmake --build build -j$(nproc)
```
2. Ejecutar el binario (abrirá ventana SFML):
```bash
./build/sfml-app
```

Nota: ejecutar la aplicación abrirá una ventana gráfica. Asegúrate de tener un entorno gráfico disponible.

Problemas observados y pasos de diagnóstico
- Síntoma que apareció: al intentar ejecutar `./build/sfml-app` se veía `zsh: exec format error` y `file build/sfml-app` devolvía `data` con un hexdump lleno de ceros.
- Causa probable: el ejecutable fue sobrescrito o quedó corrupto durante un enlace fallido; además en una de las sesiones el directorio `build/` terminó siendo eliminado/alterado.
- Comandos útiles para diagnosticar (pega salidas si necesitas ayuda):
```bash
file build/sfml-app
readelf -h build/sfml-app || true
ls -l build/CMakeFiles/sfml-app.dir/*.o 2>/dev/null
hexdump -n 64 -C build/CMakeFiles/sfml-app.dir/Visualizer.cpp.o | sed -n '1,8p'
```
- Si ves `file` → `data` o `Visualizer.cpp.o` no es reconocido: limpia y recompila:
```bash
rm -rf build
cmake -S . -B build
cmake --build build -j$(nproc)
```

Notas técnicas y compatibilidad
- El código adaptado usa SFML 3 APIs (p. ej. `sf::State::Fullscreen` y `window.getSize()`). Asegúrate de tener SFML 3 instalado en el sistema.
- En el `CMakeLists.txt` se linkea contra las librerías `libsfml-graphics.so.3`, `libsfml-window.so.3` y `libsfml-system.so.3`.

Personalizaciones recomendadas (rápidas)
- Reducir `N` a 12–25 para presentaciones con puntos bien visibles.
- Añadir un input UI para cambiar `N` o `demoK` en tiempo de ejecución (handler del botón `Demo`).
- Ajustar `mapAgeToColor` y `mapAgeToRadius` para cambiar la paleta o rango visual.

Resumen rápido
- La demo es una herramienta visual e interactiva para mostrar cómo el KD‑Tree organiza puntos y cómo funcionan búsquedas con poda, con especial énfasis en la visualización (color/tamaño por edad, selección, vecinos resaltados, animaciones paso a paso).
- Para cualquier problema de compilación/ejecución pega las salidas de los comandos de diagnóstico arriba y te ayudo a solucionarlo.

---

Si quieres, actualizo la demo ahora para exponer `N` y `demoK` como parámetros editables en la UI (pequeño cambio), o puedo añadir un script `run-demo.sh` que haga `cmake` + `run` y capture logs. Dime qué prefieres.

DEMO 

![alt text](Screenshot_2025-11-29_20_02_37.png)

busqueda por rango:
![alt text](Screenshot_2025-11-29_20_04_37.png)

busqueda por 1 vecino mas cercano
![
](Screenshot_2025-11-29_20_06_52.png)

inserccion:
![alt text](Screenshot_2025-11-29_20_07_34.png)