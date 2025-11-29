#include "KDTree.h"
#include "Visualizer.h"
#include <vector>
#include <iostream>

int main() {
    // Construimos el KDTree con algunos puntos 
    KDTree tree;
    std::vector<Punto2D> puntos = {
        {40, 45},
        {15, 70},
        {70, 10},
        {69, 50},
        {66, 85},
        {85, 90}
    };

    for (const auto& p : puntos) tree.insert(p);

    // Llamamos al visualizador (todo lo relacionado con SFML está en Visualizer.cpp)
    runVisualizer(tree, puntos);

    return 0;
}
