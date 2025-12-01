#include "KDTree.h"
#include <iostream>
#include <cmath>
#include <limits>
#include <algorithm>  // sort_heap
#include <queue>      // Para operaciones de heap
using namespace std;


// Complejidad: O(log n) promedio, O(n) peor caso
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

// ============ VECINO MAS CERCANO
// Complejidad: O(log n) promedio, O(n) peor caso

// Distancia al cuadrado entre dos puntos
static inline float distanciaCuadrado(const Punto2D& a, const Punto2D& b) {
    float diferenciaX = a.x - b.x;
    float diferenciaY = a.y - b.y;
    return diferenciaX * diferenciaX + diferenciaY * diferenciaY;
}

// Funcion auxiliar: devuelve el nodo mas cercano al objetivo entre temporal y actual
// (si temporal es null, devuelve actual; si actual es null, devuelve temporal)
static KDNode* masCercano(const Punto2D& objetivo, KDNode* temporal, KDNode* actual) {
    if (!temporal) return actual;
    if (!actual) return temporal;
    
    float distanciaTemporal = distanciaCuadrado(objetivo, temporal->punto);
    float distanciaActual = distanciaCuadrado(objetivo, actual->punto);
    
    return (distanciaTemporal < distanciaActual) ? temporal : actual;
}

static KDNode* vecinoMasCercano(KDNode* raiz, const Punto2D& objetivo, int profundidad) {
    if (raiz == nullptr)
        return nullptr;

    int eje = profundidad % 2;
    KDNode* ramaSiguiente = nullptr;
    KDNode* ramaOpuesta = nullptr;
    
    if (eje == 0) { // Comparar por X
        if (objetivo.x < raiz->punto.x) {
            ramaSiguiente = raiz->izquierdo;
            ramaOpuesta = raiz->derecho;
        } else {
            ramaSiguiente = raiz->derecho;
            ramaOpuesta = raiz->izquierdo;
        }
    } else { // Comparar por Y
        if (objetivo.y < raiz->punto.y) {
            ramaSiguiente = raiz->izquierdo;
            ramaOpuesta = raiz->derecho;
        } else {
            ramaSiguiente = raiz->derecho;
            ramaOpuesta = raiz->izquierdo;
        }
    }

    KDNode* temporal = vecinoMasCercano(ramaSiguiente, objetivo, profundidad + 1);
    KDNode* mejor = masCercano(objetivo, temporal, raiz);

    float radioCuadrado = distanciaCuadrado(objetivo, mejor->punto);

    // r' = distancia desde el objetivo al plano divisor (en el eje correspondiente)
    float distanciaPlano = (eje == 0) ? (objetivo.x - raiz->punto.x) : (objetivo.y - raiz->punto.y);

    // Poda: explorar rama opuesta si el círculo de radio r intersecta el plano divisor
    if (radioCuadrado >= distanciaPlano * distanciaPlano) {
        temporal = vecinoMasCercano(ramaOpuesta, objetivo, profundidad + 1);
        mejor = masCercano(objetivo, temporal, mejor);
    }

    return mejor;
}



// Metodo publico: encuentra el punto mas cercano a 'objetivo' en el KD-tree
//  - Tiempo: igual que la recursión interna; O(log n) promedio con poda efectiva, O(n) peor caso.
//  - Nota: en la UI se mide y muestra el tiempo real de la operación para el usuario.
Punto2D KDTree::nearest(const Punto2D& objetivo) const {
    if (!root) return {0.f, 0.f};
    
    KDNode* resultado = vecinoMasCercano(root, objetivo, 0);
    
    if (resultado) return resultado->punto;
    return {0.f, 0.f};
}



// ============ BUSQUEDA POR RANGO
// Complejidad: O(sqrt(n) + k) esperado, O(n) peor caso
void KDTree::rangeSearchRec(KDNode* nodo,
                            const Rectangulo& rectangulo,
                            int profundidad,
                            std::vector<Punto2D>& resultado) const
{
    if (nodo == nullptr) return;

    const Punto2D& puntoNodo = nodo->punto;

    // Paso 1: Verificar si el punto del nodo esta dentro del rectangulo
    if (puntoNodo.x >= rectangulo.xmin && puntoNodo.x <= rectangulo.xmax &&
        puntoNodo.y >= rectangulo.ymin && puntoNodo.y <= rectangulo.ymax) {
        resultado.push_back(puntoNodo);
    }

    int eje = profundidad % 2;

    if (eje == 0) {
        if (rectangulo.xmin <= puntoNodo.x) {
            rangeSearchRec(nodo->izquierdo, rectangulo, profundidad + 1, resultado);
        }
        if (rectangulo.xmax >= puntoNodo.x) {
            rangeSearchRec(nodo->derecho, rectangulo, profundidad + 1, resultado);
        }
    } else {
        if (rectangulo.ymin <= puntoNodo.y) {
            rangeSearchRec(nodo->izquierdo, rectangulo, profundidad + 1, resultado);
        }
        if (rectangulo.ymax >= puntoNodo.y) {
            rangeSearchRec(nodo->derecho, rectangulo, profundidad + 1, resultado);
        }
    }
}

std::vector<Punto2D> KDTree::rangeSearch(const Rectangulo& rectangulo) const {
    std::vector<Punto2D> resultado;
    rangeSearchRec(root, rectangulo, 0, resultado);
    return resultado;
}



// ============ ELIMINACION
// Complejidad: O(log n) promedio, O(n) peor caso
KDNode* KDTree::findMin(KDNode* nodo, int d, int profundidad) {
    if (nodo == nullptr) return nullptr;

    int eje = profundidad % 2;

    if (eje == d) {
        if (nodo->izquierdo == nullptr) return nodo;
        return findMin(nodo->izquierdo, d, profundidad + 1);
    }

    KDNode* minIzquierdo = findMin(nodo->izquierdo, d, profundidad + 1);
    KDNode* minDerecho = findMin(nodo->derecho, d, profundidad + 1);
    KDNode* res = nodo;

    if (minIzquierdo != nullptr && (d == 0 ? minIzquierdo->punto.x : minIzquierdo->punto.y) < (d == 0 ? res->punto.x : res->punto.y))
        res = minIzquierdo;
    if (minDerecho != nullptr && (d == 0 ? minDerecho->punto.x : minDerecho->punto.y) < (d == 0 ? res->punto.x : res->punto.y))
        res = minDerecho;

    return res;
}

KDNode* KDTree::removeRec(KDNode* nodo, const Punto2D& punto, int profundidad) {
    if (nodo == nullptr) return nullptr;

    int eje = profundidad % 2;
    float p_val = (eje == 0) ? punto.x : punto.y;
    float n_val = (eje == 0) ? nodo->punto.x : nodo->punto.y;

    if (nodo->punto.x == punto.x && nodo->punto.y == punto.y) {
        if (nodo->derecho == nullptr && nodo->izquierdo == nullptr) {
            delete nodo;
            return nullptr;
        }

        if (nodo->derecho != nullptr) {
            KDNode* minNode = findMin(nodo->derecho, eje, profundidad + 1);
            nodo->punto = minNode->punto;
            nodo->derecho = removeRec(nodo->derecho, minNode->punto, profundidad + 1);
        }
        else {
            KDNode* minNode = findMin(nodo->izquierdo, eje, profundidad + 1);
            nodo->punto = minNode->punto;
            nodo->derecho = nodo->izquierdo;
            nodo->izquierdo = nullptr;
            nodo->derecho = removeRec(nodo->derecho, minNode->punto, profundidad + 1);
        }
    }
    else if (p_val < n_val) {
        nodo->izquierdo = removeRec(nodo->izquierdo, punto, profundidad + 1);
    } else {
        nodo->derecho = removeRec(nodo->derecho, punto, profundidad + 1);
    }

    return nodo;
}

void KDTree::remove(const Punto2D& punto) {
    root = removeRec(root, punto, 0);
}


// ============ K VECINOS MAS CERCANOS (k-NN)
// Complejidad: O(k * log n) promedio, O(n) peor caso
void KDTree::kNearestRec(KDNode* nodo, const Punto2D& objetivo, int profundidad, int k,
                         std::vector<std::pair<float, Punto2D>>& pq) const {
    if (nodo == nullptr) return;

    // Paso 1: Calcular la distancia al cuadrado del nodo actual al objetivo
    float distSq = distanciaCuadrado(objetivo, nodo->punto);

    // Paso 2: Intentar agregar el punto actual a la lista de candidatos
    // Mantenemos un max-heap de tamaño k (el mayor está en pq.front())
    if (pq.size() < (size_t)k) {
        pq.push_back({distSq, nodo->punto});
        std::push_heap(pq.begin(), pq.end());
    } else {
        if (distSq < pq.front().first) {
            std::pop_heap(pq.begin(), pq.end());
            pq.pop_back();
            pq.push_back({distSq, nodo->punto});
            std::push_heap(pq.begin(), pq.end());
        }
    }

    int eje = profundidad % 2;
    float diff = (eje == 0) ? (objetivo.x - nodo->punto.x) : (objetivo.y - nodo->punto.y);
    
    KDNode* ramaCercana = (diff < 0) ? nodo->izquierdo : nodo->derecho;
    KDNode* ramaLejana = (diff < 0) ? nodo->derecho : nodo->izquierdo;

    kNearestRec(ramaCercana, objetivo, profundidad + 1, k, pq);

    // Poda: explorar rama lejana solo si puede contener mejores candidatos
    // Comparamos |q[axis] - node.point[axis]|² < distancia_máxima(H)²
    // (usamos cuadrados para evitar sqrt, consistente con distanciaCuadrado)
    float distanciaPlanoCuadrado = diff * diff;
    if (pq.size() < (size_t)k || distanciaPlanoCuadrado < pq.front().first) {
        kNearestRec(ramaLejana, objetivo, profundidad + 1, k, pq);
    }
}

std::vector<Punto2D> KDTree::kNearest(const Punto2D& objetivo, int k) const {
    std::vector<Punto2D> resultado;
    if (root == nullptr || k <= 0) return resultado;

    std::vector<std::pair<float, Punto2D>> pq;
    pq.reserve(k);  // Optimización: pre-reservar espacio

    kNearestRec(root, objetivo, 0, k, pq);
    std::sort_heap(pq.begin(), pq.end());
    
    for (const auto& item : pq) {
        resultado.push_back(item.second);
    }

    return resultado;
}
