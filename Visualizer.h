#pragma once

#include "KDTree.h"
#include <vector>

// Ejecuta la ventana gráfica para visualizar el KD-tree y puntos.
// Esta función contiene toda la dependencia de SFML para mantener `main.cpp` limpio.
void runVisualizer(KDTree& tree, const std::vector<Punto2D>& puntos);
