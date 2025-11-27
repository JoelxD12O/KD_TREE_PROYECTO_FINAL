#include "KDTree.h"
#include <iostream>
using namespace std;


// Función recursiva para insertar un punto complejidad O(log n) en promedio
KDNode* KDTree::insertRec(KDNode* nodo, const Punto2D& punto, int nivel) {
    if (nodo == nullptr) {
        return new KDNode(punto, nivel);
    }

    int eje = nivel % 2;

    if (eje == 0) {
        if (punto.x < nodo->punto.x)
            nodo->izquierdo = insertRec(nodo->izquierdo, punto, nivel + 1);
        else
            nodo->derecho = insertRec(nodo->derecho, punto, nivel + 1);
    } else {
        if (punto.y < nodo->punto.y)
            nodo->izquierdo = insertRec(nodo->izquierdo, punto, nivel + 1);
        else
            nodo->derecho = insertRec(nodo->derecho, punto, nivel + 1);
    }

    return nodo;
}

void KDTree::insert(const Punto2D& punto) {
    root = insertRec(root, punto, 0);
}

KDNode* KDTree::getRoot() const {
    return root;
}
