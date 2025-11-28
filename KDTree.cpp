#include "KDTree.h"
#include <iostream>
#include <cmath>
#include <limits>
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

// ============ NEAREST NEIGHBOR: Implementación fiel al pseudocódigo ============

// Función auxiliar: distancia al cuadrado entre dos puntos
static inline float distSquared(const Punto2D& a, const Punto2D& b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return dx*dx + dy*dy;
}

// Función auxiliar: devuelve el nodo más cercano al target entre temp y current
// (si temp es null, devuelve current; si current es null, devuelve temp)
static KDNode* closest(const Punto2D& target, KDNode* temp, KDNode* current) {
    if (!temp) return current;
    if (!current) return temp;
    
    float distTemp = distSquared(target, temp->punto);
    float distCurrent = distSquared(target, current->punto);
    
    return (distTemp < distCurrent) ? temp : current;
}

// Algoritmo recursivo de nearest neighbor en KD-tree
// Devuelve el nodo del árbol cuyo punto está más cerca de 'target'
static KDNode* nearestNeighbor(KDNode* root, const Punto2D& target, int depth) {
    // 1️⃣ Caso base
    if (root == nullptr)
        return nullptr;

    // 2️⃣ Elegir la rama principal según el eje (discriminador)
    // depth % 2: si depth es par → eje X (0), si es impar → eje Y (1)
    int axis = depth % 2;
    KDNode* nextBranch = nullptr;
    KDNode* otherBranch = nullptr;
    
    if (axis == 0) { // Comparar por X
        if (target.x < root->punto.x) {
            nextBranch = root->izquierdo;
            otherBranch = root->derecho;
        } else {
            nextBranch = root->derecho;
            otherBranch = root->izquierdo;
        }
    } else { // Comparar por Y
        if (target.y < root->punto.y) {
            nextBranch = root->izquierdo;
            otherBranch = root->derecho;
        } else {
            nextBranch = root->derecho;
            otherBranch = root->izquierdo;
        }
    }

    // 3️⃣ Búsqueda recursiva por la rama principal (lado "correcto")
    KDNode* temp = nearestNeighbor(nextBranch, target, depth + 1);

    // 4️⃣ Determinar el mejor entre lo encontrado y el nodo actual
    KDNode* best = closest(target, temp, root);

    // 5️⃣ Calcular radios r² y r'
    // r² = distancia al mejor punto encontrado hasta ahora
    float radiusSquared = distSquared(target, best->punto);

    // r' = distancia desde el target al plano divisor (en el eje correspondiente)
    float dist = (axis == 0) ? (target.x - root->punto.x) : (target.y - root->punto.y);

    // 6️⃣ ¿Buscar o no buscar la otra rama?
    // Si el círculo de radio r intersecta el plano divisor,
    // entonces el otro lado del árbol podría tener un punto mejor.
    if (radiusSquared >= dist * dist) {
        temp = nearestNeighbor(otherBranch, target, depth + 1);
        best = closest(target, temp, best);
    }

    // 7️⃣ Devolver el mejor punto final
    return best;
}

// Método público: encuentra el punto más cercano a 'target' en el KD-tree
Punto2D KDTree::nearest(const Punto2D& target) const {
    if (!root) return {0.f, 0.f};
    
    KDNode* result = nearestNeighbor(root, target, 0);
    
    if (result) return result->punto;
    return {0.f, 0.f};
}
