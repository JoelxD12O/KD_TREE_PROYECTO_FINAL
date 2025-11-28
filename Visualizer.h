#pragma once

#include "KDTree.h"
#include <vector>
#include <SFML/Graphics.hpp>

// Ejecuta la ventana gráfica para visualizar el KD-tree y puntos.
// Esta función contiene toda la dependencia de SFML para mantener `main.cpp` limpio.
// Note: `puntos` is a mutable vector so the visualizer can append new points
void runVisualizer(KDTree& tree, std::vector<Punto2D>& puntos);

// Dibuja el árbol (panel derecho). El parámetro `font` es opcional para etiquetas.
void drawTree(sf::RenderWindow& window, KDNode* root, const sf::Font* font = nullptr);
void drawTree(sf::RenderWindow& window, KDNode* root, const sf::Font* font, KDNode* highlightNode, bool isPulse);
