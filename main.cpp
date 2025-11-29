#include "KDTree.h"
#include "Visualizer.h"
#include <vector>
#include <iostream>

int main() {
    // Construimos el KDTree con algunos puntos (lógica pura)
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

    // 🧪 PRUEBA RÁPIDA DE RANGE SEARCH (comentar después)
    std::cout << "\n=== PRUEBA DE RANGE SEARCH ===\n";
    Rectangulo r{60, 90, 40, 100}; // xmin, xmax, ymin, ymax
    auto puntosEnRango = tree.rangeSearch(r);
    
    std::cout << "Rectángulo: [" << r.xmin << ", " << r.xmax << "] x [" 
              << r.ymin << ", " << r.ymax << "]\n";
    std::cout << "Puntos encontrados: " << puntosEnRango.size() << "\n";
    for (const auto& p : puntosEnRango) {
        std::cout << "  (" << p.x << ", " << p.y << ")\n";
    }
    std::cout << "==============================\n\n";

    // Llamamos al visualizador (todo lo relacionado con SFML está en Visualizer.cpp)
    runVisualizer(tree, puntos);

    return 0;
}
