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
        if (punto.x < nodo->punto.x)//punto.x es la coordenada x del punto a insertar y nodo->punto.x es la coordenada x del nodo actual 
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

// ============ VECINO MAS CERCANO: Implementacion fiel al pseudocodigo ============

// Funcion auxiliar: distancia al cuadrado entre dos puntos
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

// Algoritmo recursivo de vecino mas cercano en KD-tree
// Devuelve el nodo del arbol cuyo punto esta mas cerca de 'objetivo'
static KDNode* vecinoMasCercano(KDNode* raiz, const Punto2D& objetivo, int profundidad) {
    if (raiz == nullptr)
        return nullptr;

    // Paso 2: Elegir la rama principal segun el eje (discriminador)
    // profundidad % 2: si profundidad es par -> eje X (0), si es impar -> eje Y (1)
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

    // Paso 3: Busqueda recursiva por la rama principal (lado "correcto")
    KDNode* temporal = vecinoMasCercano(ramaSiguiente, objetivo, profundidad + 1);

    // Paso 4: Determinar el mejor entre lo encontrado y el nodo actual
    KDNode* mejor = masCercano(objetivo, temporal, raiz);

    // Paso 5: Calcular radios r² y r'
    // r² = distancia al mejor punto encontrado hasta ahora
    float radioCuadrado = distanciaCuadrado(objetivo, mejor->punto);

    // r' = distancia desde el objetivo al plano divisor (en el eje correspondiente)
    float distanciaPlano = (eje == 0) ? (objetivo.x - raiz->punto.x) : (objetivo.y - raiz->punto.y);

    // Paso 6: Buscar o no buscar la otra rama (poda)
    // Si el circulo de radio r intersecta el plano divisor,
    // entonces el otro lado del arbol podria tener un punto mejor.
    if (radioCuadrado >= distanciaPlano * distanciaPlano) {
        temporal = vecinoMasCercano(ramaOpuesta, objetivo, profundidad + 1);
        mejor = masCercano(objetivo, temporal, mejor);
    }

    // Paso 7: Devolver el mejor punto final
    return mejor;
}

// Metodo publico: encuentra el punto mas cercano a 'objetivo' en el KD-tree
Punto2D KDTree::nearest(const Punto2D& objetivo) const {
    if (!root) return {0.f, 0.f};
    
    KDNode* resultado = vecinoMasCercano(root, objetivo, 0);
    
    if (resultado) return resultado->punto;
    return {0.f, 0.f};
}

// ============ BUSQUEDA POR RANGO: Busqueda dentro de un rectangulo ============

// Funcion recursiva que explora el arbol buscando puntos dentro del rectangulo
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
        // Nivel X: linea vertical en x = puntoNodo.x
        // El espacio se divide en: izquierda (x < puntoNodo.x) y derecha (x >= puntoNodo.x)

        // El rectangulo toca o esta a la izquierda del plano divisor
        if (rectangulo.xmin <= puntoNodo.x) {
            rangeSearchRec(nodo->izquierdo, rectangulo, profundidad + 1, resultado);
        }

        // El rectangulo toca o esta a la derecha del plano divisor
        if (rectangulo.xmax >= puntoNodo.x) {
            rangeSearchRec(nodo->derecho, rectangulo, profundidad + 1, resultado);
        }
    } else {
        // Nivel Y: linea horizontal en y = puntoNodo.y
        // El espacio se divide en: abajo (y < puntoNodo.y) y arriba (y >= puntoNodo.y)

        // El rectangulo toca o esta abajo del plano divisor
        if (rectangulo.ymin <= puntoNodo.y) {
            rangeSearchRec(nodo->izquierdo, rectangulo, profundidad + 1, resultado);
        }

        // El rectangulo toca o esta arriba del plano divisor
        if (rectangulo.ymax >= puntoNodo.y) {
            rangeSearchRec(nodo->derecho, rectangulo, profundidad + 1, resultado);
        }
    }
}

// Metodo publico: devuelve todos los puntos dentro del rectangulo
std::vector<Punto2D> KDTree::rangeSearch(const Rectangulo& rectangulo) const {
    std::vector<Punto2D> resultado;
    rangeSearchRec(root, rectangulo, 0, resultado);
    return resultado;
}
