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

    // Unit test for remove
    {
        std::cout << "Running unit test for remove..." << std::endl;
        KDTree testTree;
        testTree.insert({10, 10});
        testTree.remove({10, 10});
        if (testTree.getRoot() == nullptr) {
            std::cout << "[TEST] Remove single node: PASSED" << std::endl;
        } else {
            std::cout << "[TEST] Remove single node: FAILED" << std::endl;
        }
    }

    // Unit test for k-NN
    {
        std::cout << "\nRunning unit test for k-NN..." << std::endl;
        KDTree testTree;
        
        // Insertar varios puntos
        testTree.insert({40, 45});  // Cerca del objetivo
        testTree.insert({45, 55});  // Cerca del objetivo
        testTree.insert({70, 70});  // Lejos
        testTree.insert({10, 10});  // Muy lejos
        testTree.insert({85, 90});  // Muy lejos
        
        // Buscar los 2 más cercanos a (50, 50) - punto que NO está en el árbol
        auto vecinos = testTree.kNearest({50, 50}, 2);
        
        std::cout << "Buscando k=2 vecinos mas cercanos a (50, 50):" << std::endl;
        
        if (vecinos.size() == 2) {
            std::cout << "  Vecino 1: (" << vecinos[0].x << ", " << vecinos[0].y << ")" << std::endl;
            std::cout << "  Vecino 2: (" << vecinos[1].x << ", " << vecinos[1].y << ")" << std::endl;
            
            // Los más cercanos deberían ser (40,45) y (45,55)
            bool tiene_40_45 = false;
            bool tiene_45_55 = false;
            
            for (const auto& p : vecinos) {
                if (std::abs(p.x - 40) < 0.1 && std::abs(p.y - 45) < 0.1) tiene_40_45 = true;
                if (std::abs(p.x - 45) < 0.1 && std::abs(p.y - 55) < 0.1) tiene_45_55 = true;
            }
            
            if (tiene_40_45 && tiene_45_55) {
                std::cout << "[TEST] k-NN (k=2): PASSED ✓" << std::endl;
            } else {
                std::cout << "[TEST] k-NN (k=2): FAILED - Puntos incorrectos" << std::endl;
            }
        } else {
            std::cout << "[TEST] k-NN (k=2): FAILED - Se esperaban 2 vecinos, se obtuvieron " 
                      << vecinos.size() << std::endl;
        }
        
        // Test adicional: buscar k=3
        std::cout << "\nBuscando k=3 vecinos mas cercanos a (50, 50):" << std::endl;
        auto vecinos3 = testTree.kNearest({50, 50}, 3);
        std::cout << "  Encontrados: " << vecinos3.size() << " vecinos" << std::endl;
        for (size_t i = 0; i < vecinos3.size(); i++) {
            std::cout << "    " << (i+1) << ". (" << vecinos3[i].x << ", " << vecinos3[i].y << ")" << std::endl;
        }
    }

    // Llamamos al visualizador (todo lo relacionado con SFML está en Visualizer.cpp)
    runVisualizer(tree, puntos);

    return 0;
}
