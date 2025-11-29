#pragma once
#include <vector>

// Contenedor para coordenadas 2D
struct Punto2D {
    float x;
    float y;
};

// Rectangulo para busqueda por rango
struct Rectangulo {
    float xmin, xmax;
    float ymin, ymax;
};

struct KDNode {
    Punto2D punto;      // coordenadas del nodo
    KDNode* izquierdo;  // hijo izquierdo
    KDNode* derecho;    // hijo derecho
    int nivel;          // nivel en el arbol (0 = raiz, 1, 2, ...)

    KDNode(const Punto2D& p, int lvl)
        : punto(p), izquierdo(nullptr), derecho(nullptr), nivel(lvl) {}
};


class KDTree {
public:
    KDTree() : root(nullptr) {}

    // Declaraciones: definiciones en KDTree.cpp
    void insert(const Punto2D& punto);

    KDNode* getRoot() const;
    
    // Busqueda de vecino mas cercano: devuelve el punto del arbol mas cercano al objetivo
    Punto2D nearest(const Punto2D& objetivo) const;
    
    // Busqueda por rango: devuelve todos los puntos dentro del rectangulo
    std::vector<Punto2D> rangeSearch(const Rectangulo& rectangulo) const;

private:
    KDNode* root;

    KDNode* insertRec(KDNode* nodo, const Punto2D& punto, int nivel);
    
    // Busqueda por rango recursiva
    void rangeSearchRec(KDNode* nodo,
                        const Rectangulo& rectangulo,
                        int profundidad,
                        std::vector<Punto2D>& resultado) const;
};
