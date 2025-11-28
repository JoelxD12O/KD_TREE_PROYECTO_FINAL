#pragma once//esto significa que si ya se incluyo este archivo no se vuelve a incluir

//contenedor para coordenadas 2D
struct Punto2D {
    float x;
    float y;
};



struct KDNode {
    Punto2D punto;      // coordenadas del nodo
    KDNode* izquierdo;  // hijo izquierdo
    KDNode* derecho;    // hijo derecho
    int nivel;          // nivel en el árbol (0 = raíz, 1, 2, ...)

    KDNode(const Punto2D& p, int lvl)
        : punto(p), izquierdo(nullptr), derecho(nullptr), nivel(lvl) {}
};


class KDTree {
public:
    KDTree() : root(nullptr) {}

    // Declaraciones: definiciones en KDTree.cpp
    void insert(const Punto2D& punto);

    KDNode* getRoot() const;
    // nearest neighbor search: devuelve el punto del árbol más cercano a 'target'
    Punto2D nearest(const Punto2D& target) const;

private:
    KDNode* root;

    KDNode* insertRec(KDNode* nodo, const Punto2D& punto, int nivel);
};
