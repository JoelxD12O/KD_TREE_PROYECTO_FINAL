#include "Visualizer.h"
#include <SFML/Graphics.hpp>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <string>
#include <deque>
#include <functional>
#include <cmath>
#include <locale>
#include <codecvt>
#include <chrono>
#include <random>
#include <algorithm>
#include <cstdint>

// Reuse KDTree types: Punto2D, KDNode, KDTree

//---------------------- Soporte UTF-8 para tildes y ñ ---------------------
// Helper para convertir std::string UTF-8 a sf::String (soporta Unicode)
static sf::String toUtf8(const std::string& str) {
    return sf::String::fromUtf8(str.begin(), str.end());
}

//---------------------- Sistema de Animación ---------------------
enum class AnimationType {
    NONE,
    INSERT,
    SEARCH,
    RANGE_SEARCH,
    DELETE,
    KNN
};

struct AnimationStep {
    KDNode* currentNode;
    std::string description;
    bool isComparison;
    bool isLeaf;
    int axis; // 0=X, 1=Y
    Punto2D targetPoint;
};

struct AnimationState {
    AnimationType type = AnimationType::NONE;
    std::deque<AnimationStep> steps;
    int currentStepIndex = -1;
    sf::Clock stepClock;
    float stepDuration = 1.3f; // segundos por paso (más rápido: 0.5s)
    bool paused = false;
    bool autoAdvance = true;
    Punto2D searchTarget{0.f, 0.f};
    Punto2D foundNearest{0.f, 0.f};
    bool hasResult = false;
    double executionTimeMicros = 0.0; // Tiempo real de ejecución en microsegundos
};

//---------------------- Estado de Búsqueda por Rango ---------------------
struct RangeSearchState {
    bool modoActivo = false;
    bool dibujando = false;
    sf::Vector2f puntoInicio;
    sf::Vector2f puntoFin;
    std::vector<Punto2D> puntosEncontrados;
    bool tieneResultado = false;
    Rectangulo rectangulo{0.f, 0.f, 0.f, 0.f};
};

//---------------------- Estado de k-NN ---------------------
struct KNNState {
    bool modoActivo = false;
    int k = 3;  // Valor por defecto
    std::vector<Punto2D> puntosEncontrados;
    bool tieneResultado = false;
    Punto2D puntoObjetivo{0.f, 0.f};
};

//---------------------- Configuración de SFML ---------------------_
// Valores por defecto (se recalculan en runtime si abrimos fullscreen)
static int WINDOW_WIDTH  = 1000;
static int WINDOW_HEIGHT = 600;

// Panel izquierdo para el plano cartesiano (se actualizarán según la resolución)
static float PLANE_WIDTH  = 600.f;
static float PLANE_HEIGHT = 600.f;
static float PLANE_ORIGIN_X = 0.f;
static float PLANE_ORIGIN_Y = 0.f;

// Rango máximo de coordenadas reales (como el ejemplo del libro)
const float MAX_COORD = 128.f;

// Panel derecho: árbol
static float TREE_ORIGIN_X   = PLANE_WIDTH;           // empieza donde acaba el plano
const float TREE_PADDING_X  = 20.f;
const float TREE_PADDING_Y  = 40.f;
static float TREE_WIDTH      = WINDOW_WIDTH - TREE_ORIGIN_X - TREE_PADDING_X;
const float TREE_LEVEL_H    = 80.f;                  // separación vertical entre niveles

// Region struct used by KD drawing
struct Region {
    float minX, maxX;
    float minY, maxY;
};

//---------------------- Funciones de Generación de Animación ---------------------

// Genera pasos de animación para la inserción de un punto
static void generateInsertAnimation(KDNode* root, const Punto2D& punto, AnimationState& anim) {
    anim.steps.clear();
    anim.currentStepIndex = -1;
    anim.type = AnimationType::INSERT;
    
    KDNode* current = root;
    int nivel = 0;
    
    while (current) {
        int eje = nivel % 2;
        AnimationStep step;
        step.currentNode = current;
        step.targetPoint = punto;
        step.axis = eje;
        step.isComparison = true;
        
        if (eje == 0) {
            if (punto.x < current->punto.x) {
                step.description = "Comparar X: " + std::to_string((int)punto.x) + " < " + 
                                 std::to_string((int)current->punto.x) + " -> IR IZQUIERDA";
                anim.steps.push_back(step);
                if (!current->izquierdo) {
                    AnimationStep leafStep;
                    leafStep.currentNode = current;
                    leafStep.targetPoint = punto;
                    leafStep.isLeaf = true;
                    leafStep.description = "Insertar aquí (hijo izquierdo)";
                    anim.steps.push_back(leafStep);
                    break;
                }
                current = current->izquierdo;
            } else {
                step.description = "Comparar X: " + std::to_string((int)punto.x) + " >= " + 
                                 std::to_string((int)current->punto.x) + " -> IR DERECHA";
                anim.steps.push_back(step);
                if (!current->derecho) {
                    AnimationStep leafStep;
                    leafStep.currentNode = current;
                    leafStep.targetPoint = punto;
                    leafStep.isLeaf = true;
                    leafStep.description = "Insertar aquí (hijo derecho)";
                    anim.steps.push_back(leafStep);
                    break;
                }
                current = current->derecho;
            }
        } else {
            if (punto.y < current->punto.y) {
                step.description = "Comparar Y: " + std::to_string((int)punto.y) + " < " + 
                                 std::to_string((int)current->punto.y) + " -> IR IZQUIERDA";
                anim.steps.push_back(step);
                if (!current->izquierdo) {
                    AnimationStep leafStep;
                    leafStep.currentNode = current;
                    leafStep.targetPoint = punto;
                    leafStep.isLeaf = true;
                    leafStep.description = "Insertar aquí (hijo izquierdo)";
                    anim.steps.push_back(leafStep);
                    break;
                }
                current = current->izquierdo;
            } else {
                step.description = "Comparar Y: " + std::to_string((int)punto.y) + " >= " + 
                                 std::to_string((int)current->punto.y) + " -> IR DERECHA";
                anim.steps.push_back(step);
                if (!current->derecho) {
                    AnimationStep leafStep;
                    leafStep.currentNode = current;
                    leafStep.targetPoint = punto;
                    leafStep.isLeaf = true;
                    leafStep.description = "Insertar aquí (hijo derecho)";
                    anim.steps.push_back(leafStep);
                    break;
                }
                current = current->derecho;
            }
        }
        nivel++;
    }
}

// Genera pasos de animación para la búsqueda del nearest neighbor
// Esta función simula EXACTAMENTE el algoritmo con PODA
static void generateSearchAnimation(KDNode* root, const Punto2D& target, AnimationState& anim) {
    anim.steps.clear();
    anim.currentStepIndex = -1;
    anim.type = AnimationType::SEARCH;
    anim.searchTarget = target;
    
    // Variables para rastrear el mejor punto encontrado
    KDNode* bestSoFar = nullptr;
    float bestDistSoFar = std::numeric_limits<float>::infinity();
    
    // Función recursiva que simula nearestNeighbor CON PODA
    std::function<void(KDNode*, int)> nearestWithPruning = [&](KDNode* node, int depth) {
        if (!node) return;
        
        // Paso 1: Visitar el nodo actual
        AnimationStep visitStep;
        visitStep.currentNode = node;
        visitStep.targetPoint = target;
        visitStep.axis = depth % 2;
        visitStep.isComparison = true;
        
        float dx = target.x - node->punto.x;
        float dy = target.y - node->punto.y;
        float distSquared = dx*dx + dy*dy;
        float dist = std::sqrt(distSquared);
        
        visitStep.description = "Visitar (" + std::to_string((int)node->punto.x) + ", " + 
                               std::to_string((int)node->punto.y) + ") | Dist: " + 
                               std::to_string((int)dist);
        
        // Actualizar mejor si este es más cercano
        if (distSquared < bestDistSoFar) {
            bestDistSoFar = distSquared;
            bestSoFar = node;
            visitStep.description += " ← NUEVO MEJOR!";
        }
        anim.steps.push_back(visitStep);
        
        // Paso 2: Determinar nextBranch y otherBranch
        int axis = depth % 2;
        KDNode* nextBranch = nullptr;
        KDNode* otherBranch = nullptr;
        
        if (axis == 0) { // Eje X
            if (target.x < node->punto.x) {
                nextBranch = node->izquierdo;
                otherBranch = node->derecho;
            } else {
                nextBranch = node->derecho;
                otherBranch = node->izquierdo;
            }
        } else { // Eje Y
            if (target.y < node->punto.y) {
                nextBranch = node->izquierdo;
                otherBranch = node->derecho;
            } else {
                nextBranch = node->derecho;
                otherBranch = node->izquierdo;
            }
        }
        
        // Paso 3: Buscar en nextBranch (lado "correcto")
        if (nextBranch) {
            nearestWithPruning(nextBranch, depth + 1);
        }
        
        // Paso 4: PODA - ¿Explorar otherBranch?
        float planeDistance = (axis == 0) ? (target.x - node->punto.x) : (target.y - node->punto.y);
        float planeDistSquared = planeDistance * planeDistance;
        
        if (planeDistSquared < bestDistSoFar) {
            // El círculo intersecta el plano → explorar la otra rama
            AnimationStep pruneStep;
            pruneStep.currentNode = node;
            pruneStep.targetPoint = target;
            pruneStep.axis = axis;
            pruneStep.isComparison = false;
            pruneStep.description = "Radio r=" + std::to_string((int)std::sqrt(bestDistSoFar)) + 
                                   " INTERSECTA plano -> Explorar otra rama";
            anim.steps.push_back(pruneStep);
            
            if (otherBranch) {
                nearestWithPruning(otherBranch, depth + 1);
            }
        } else {
            // PODA: el círculo NO intersecta → NO explorar
            AnimationStep pruneStep;
            pruneStep.currentNode = node;
            pruneStep.targetPoint = target;
            pruneStep.axis = axis;
            pruneStep.isComparison = false;
            pruneStep.description = "Radio r=" + std::to_string((int)std::sqrt(bestDistSoFar)) + 
                                   " NO intersecta plano -> PODAR! (saltar rama)";
            anim.steps.push_back(pruneStep);
        }
    };
    
    nearestWithPruning(root, 0);
    
    // Paso final: mostrar resultado
    if (bestSoFar) {
        AnimationStep finalStep;
        finalStep.currentNode = bestSoFar;
        finalStep.targetPoint = target;
        finalStep.isLeaf = true;
        finalStep.description = "RESULTADO: Vecino más cercano encontrado!";
        anim.steps.push_back(finalStep);
    }
}

// Generar animación para búsqueda por rango
static void generateRangeSearchAnimation(KDNode* root, const Rectangulo& rect, 
                                        AnimationState& anim, 
                                        std::vector<Punto2D>& foundPoints) {
    anim.type = AnimationType::RANGE_SEARCH;
    anim.steps.clear();
    anim.currentStepIndex = -1;
    anim.stepDuration = 0.5f;
    anim.autoAdvance = true;
    anim.paused = false;
    foundPoints.clear();
    
    // Paso inicial
    AnimationStep startStep;
    startStep.currentNode = root;
    startStep.targetPoint = {(rect.xmin + rect.xmax) / 2.f, (rect.ymin + rect.ymax) / 2.f};
    startStep.isComparison = false;
    startStep.description = "Iniciando búsqueda por rango [" + 
                           std::to_string((int)rect.xmin) + "," + std::to_string((int)rect.xmax) + 
                           "] x [" + std::to_string((int)rect.ymin) + "," + std::to_string((int)rect.ymax) + "]";
    anim.steps.push_back(startStep);
    
    std::function<void(KDNode*, int)> rangeSearchRec = [&](KDNode* node, int depth) {
        if (!node) return;
        
        int axis = depth % 2;
        
        // Paso: visitar nodo
        AnimationStep visitStep;
        visitStep.currentNode = node;
        visitStep.targetPoint = {(rect.xmin + rect.xmax) / 2.f, (rect.ymin + rect.ymax) / 2.f};
        visitStep.axis = axis;
        visitStep.isComparison = true;
        visitStep.description = "Visitando nodo (" + std::to_string((int)node->punto.x) + 
                               ", " + std::to_string((int)node->punto.y) + 
                               ") - Eje: " + (axis == 0 ? "X" : "Y");
        anim.steps.push_back(visitStep);
        
        // Verificar si el punto está en el rectángulo
        bool inRange = (node->punto.x >= rect.xmin && node->punto.x <= rect.xmax &&
                       node->punto.y >= rect.ymin && node->punto.y <= rect.ymax);
        
        if (inRange) {
            AnimationStep foundStep;
            foundStep.currentNode = node;
            foundStep.targetPoint = {(rect.xmin + rect.xmax) / 2.f, (rect.ymin + rect.ymax) / 2.f};
            foundStep.isLeaf = true;
            foundStep.description = "[+] Punto DENTRO del rectangulo -> agregar a resultados";
            anim.steps.push_back(foundStep);
            foundPoints.push_back(node->punto);
        } else {
            AnimationStep notFoundStep;
            notFoundStep.currentNode = node;
            notFoundStep.targetPoint = {(rect.xmin + rect.xmax) / 2.f, (rect.ymin + rect.ymax) / 2.f};
            notFoundStep.isComparison = false;
            notFoundStep.description = "[-] Punto FUERA del rectangulo";
            anim.steps.push_back(notFoundStep);
        }
        
        // Decidir qué ramas explorar
        float nodeVal = (axis == 0) ? node->punto.x : node->punto.y;
        float rectMin = (axis == 0) ? rect.xmin : rect.ymin;
        float rectMax = (axis == 0) ? rect.xmax : rect.ymax;
        
        bool exploreLeft = (nodeVal >= rectMin);
        bool exploreRight = (nodeVal <= rectMax);
        
        if (exploreLeft && node->izquierdo) {
            AnimationStep leftStep;
            leftStep.currentNode = node;
            leftStep.targetPoint = {(rect.xmin + rect.xmax) / 2.f, (rect.ymin + rect.ymax) / 2.f};
            leftStep.axis = axis;
            leftStep.isComparison = false;
            leftStep.description = std::string("Plano ") + (axis == 0 ? "X=" : "Y=") + std::to_string((int)nodeVal) + 
                                 " intersecta rango -> Explorar rama IZQUIERDA";
            anim.steps.push_back(leftStep);
            rangeSearchRec(node->izquierdo, depth + 1);
        } else if (node->izquierdo) {
            AnimationStep pruneLeftStep;
            pruneLeftStep.currentNode = node;
            pruneLeftStep.targetPoint = {(rect.xmin + rect.xmax) / 2.f, (rect.ymin + rect.ymax) / 2.f};
            pruneLeftStep.axis = axis;
            pruneLeftStep.isComparison = false;
            pruneLeftStep.description = std::string("Plano ") + (axis == 0 ? "X=" : "Y=") + std::to_string((int)nodeVal) + 
                                       " NO intersecta -> PODAR rama izquierda";
            anim.steps.push_back(pruneLeftStep);
        }
        
        if (exploreRight && node->derecho) {
            AnimationStep rightStep;
            rightStep.currentNode = node;
            rightStep.targetPoint = {(rect.xmin + rect.xmax) / 2.f, (rect.ymin + rect.ymax) / 2.f};
            rightStep.axis = axis;
            rightStep.isComparison = false;
            rightStep.description = std::string("Plano ") + (axis == 0 ? "X=" : "Y=") + std::to_string((int)nodeVal) + 
                                  " intersecta rango -> Explorar rama DERECHA";
            anim.steps.push_back(rightStep);
            rangeSearchRec(node->derecho, depth + 1);
        } else if (node->derecho) {
            AnimationStep pruneRightStep;
            pruneRightStep.currentNode = node;
            pruneRightStep.targetPoint = {(rect.xmin + rect.xmax) / 2.f, (rect.ymin + rect.ymax) / 2.f};
            pruneRightStep.axis = axis;
            pruneRightStep.isComparison = false;
            pruneRightStep.description = std::string("Plano ") + (axis == 0 ? "X=" : "Y=") + std::to_string((int)nodeVal) + 
                                        " NO intersecta -> PODAR rama derecha";
            anim.steps.push_back(pruneRightStep);
        }
    };
    
    rangeSearchRec(root, 0);
    
    // Paso final
    AnimationStep finalStep;
    finalStep.currentNode = nullptr;
    finalStep.targetPoint = {(rect.xmin + rect.xmax) / 2.f, (rect.ymin + rect.ymax) / 2.f};
    finalStep.isLeaf = false;
    finalStep.description = "COMPLETADO: Encontrados " + std::to_string(foundPoints.size()) + " puntos";
    anim.steps.push_back(finalStep);
}

// Generar animación para eliminación (simulada, ya que la eliminación real modifica la estructura)
// En este caso, simplemente mostraremos qué punto se va a eliminar
static void generateDeleteAnimation(const Punto2D& target, AnimationState& anim) {
    anim.type = AnimationType::DELETE;
    anim.steps.clear();
    anim.currentStepIndex = -1;
    anim.stepDuration = 1.0f;
    anim.autoAdvance = true;
    anim.paused = false;
    
    AnimationStep step;
    step.currentNode = nullptr;
    step.targetPoint = target;
    step.isLeaf = true; // Para resaltar
    step.description = "ELIMINANDO punto (" + std::to_string((int)target.x) + ", " + std::to_string((int)target.y) + ")";
    anim.steps.push_back(step);
}

// Generar animación para k-NN
static void generateKNNAnimation(const Punto2D& target, int k, 
                                 const std::vector<Punto2D>& results, 
                                 AnimationState& anim) {
    anim.type = AnimationType::KNN;
    anim.steps.clear();
    anim.currentStepIndex = -1;
    anim.stepDuration = 0.8f;
    anim.autoAdvance = true;
    anim.paused = false;
    
    // Paso inicial
    AnimationStep initStep;
    initStep.currentNode = nullptr;
    initStep.targetPoint = target;
    initStep.isComparison = false;
    initStep.description = "Buscando " + std::to_string(k) + " vecinos mas cercanos a (" + 
                           std::to_string((int)target.x) + ", " + std::to_string((int)target.y) + ")";
    anim.steps.push_back(initStep);
    
    // Paso final con resultados
    AnimationStep finalStep;
    finalStep.currentNode = nullptr;
    finalStep.targetPoint = target;
    finalStep.isLeaf = true;
    finalStep.description = "Encontrados " + std::to_string(results.size()) + " vecinos!";
    anim.steps.push_back(finalStep);
}

static sf::Vector2f mapToPlane(const Punto2D& p) {
    float normX = p.x / MAX_COORD; // 0..1
    float normY = p.y / MAX_COORD; // 0..1

    float screenX = PLANE_ORIGIN_X + normX * PLANE_WIDTH;
    float screenY = PLANE_ORIGIN_Y + (1.f - normY) * PLANE_HEIGHT;

    return {screenX, screenY};
}

static sf::Vector2f mapToPlane(float x, float y) {
    float normX = x / MAX_COORD;
    float normY = y / MAX_COORD;

    float screenX = PLANE_ORIGIN_X + normX * PLANE_WIDTH;
    float screenY = PLANE_ORIGIN_Y + (1.f - normY) * PLANE_HEIGHT;
    return {screenX, screenY};
}

// Mapea edad (20..90) a un color (azul->rojo)
static sf::Color mapAgeToColor(float age) {
    float t = (age - 20.f) / (90.f - 20.f);
    t = std::max(0.f, std::min(1.f, t));
    uint8_t r = (uint8_t)std::round((0.2f + 0.8f * t) * 255.f);
    uint8_t g = (uint8_t)std::round((0.2f + 0.2f * (1.f - t)) * 255.f);
    uint8_t b = (uint8_t)std::round((0.8f - 0.8f * t) * 255.f);
    return sf::Color(r, g, b);
}

// Mapea edad a radio en pixeles
static float mapAgeToRadius(float age) {
    float t = (age - 20.f) / (90.f - 20.f);
    t = std::max(0.f, std::min(1.f, t));
    return 4.f + t * 6.f; // radio entre 4 y 10
}

static void drawPoint(sf::RenderWindow& window, const Punto2D& p, sf::Color color = sf::Color::Red, float radius = 5.f) {
    sf::Vector2f pos = mapToPlane(p);

    sf::CircleShape circle(radius);
    circle.setFillColor(color);
    circle.setOrigin(sf::Vector2f(radius, radius));
    circle.setPosition(pos);

    window.draw(circle);
}

static void drawPointLabel(sf::RenderWindow& window, const Punto2D& p, const sf::Font* font) {
    if (!font) return;

    std::string label = "(" + std::to_string((int)p.x) + ", " + std::to_string((int)p.y) + ")";
    sf::Text text(*font, label);
    text.setCharacterSize(14);
    text.setFillColor(sf::Color::White);

    sf::Vector2f pos = mapToPlane(p);
    text.setPosition({pos.x + 8.f, pos.y - 10.f});

    window.draw(text);
}

static void drawAxesAndGrid(sf::RenderWindow& window, const sf::Font* font = nullptr) {
    sf::RectangleShape planeBg({PLANE_WIDTH, PLANE_HEIGHT});
    planeBg.setPosition(sf::Vector2f(PLANE_ORIGIN_X, PLANE_ORIGIN_Y));
    planeBg.setFillColor(sf::Color(30, 30, 30));
    window.draw(planeBg);

    const float step = 16.f;
    for (float x = 0; x <= MAX_COORD; x += step) {
        float normX = x / MAX_COORD;
        float screenX = PLANE_ORIGIN_X + normX * PLANE_WIDTH;

        sf::VertexArray line(sf::PrimitiveType::Lines, 2);
        line[0].position = sf::Vector2f(screenX, PLANE_ORIGIN_Y);
        line[1].position = sf::Vector2f(screenX, PLANE_ORIGIN_Y + PLANE_HEIGHT);
        line[0].color = line[1].color = sf::Color(60, 60, 60);

        window.draw(line);
    }

    for (float y = 0; y <= MAX_COORD; y += step) {
        float normY = y / MAX_COORD;
        float screenY = PLANE_ORIGIN_Y + (1.f - normY) * PLANE_HEIGHT;

        sf::VertexArray line(sf::PrimitiveType::Lines, 2);
        line[0].position = sf::Vector2f(PLANE_ORIGIN_X, screenY);
        line[1].position = sf::Vector2f(PLANE_ORIGIN_X + PLANE_WIDTH, screenY);
        line[0].color = line[1].color = sf::Color(60, 60, 60);

        window.draw(line);
    }

    sf::VertexArray ejeX(sf::PrimitiveType::Lines, 2);
    ejeX[0].position = sf::Vector2f(PLANE_ORIGIN_X, PLANE_ORIGIN_Y + PLANE_HEIGHT);
    ejeX[1].position = sf::Vector2f(PLANE_ORIGIN_X + PLANE_WIDTH, PLANE_ORIGIN_Y + PLANE_HEIGHT);
    ejeX[0].color = ejeX[1].color = sf::Color::White;
    window.draw(ejeX);

    sf::VertexArray ejeY(sf::PrimitiveType::Lines, 2);
    ejeY[0].position = sf::Vector2f(PLANE_ORIGIN_X, PLANE_ORIGIN_Y);
    ejeY[1].position = sf::Vector2f(PLANE_ORIGIN_X, PLANE_ORIGIN_Y + PLANE_HEIGHT);
    ejeY[0].color = ejeY[1].color = sf::Color::White;
    window.draw(ejeY);
    
    // Dibujar etiquetas en los ejes
    if (font) {
        // Etiquetas en eje X (cada 20 unidades)
        for (float x = 0; x <= MAX_COORD; x += 20.f) {
            float normX = x / MAX_COORD;
            float screenX = PLANE_ORIGIN_X + normX * PLANE_WIDTH;
            
            sf::Text label(*font, std::to_string((int)x), 10);
            label.setFillColor(sf::Color(180, 180, 180));
            sf::FloatRect bounds = label.getLocalBounds();
            label.setPosition(sf::Vector2f(screenX - bounds.size.x / 2.f, PLANE_ORIGIN_Y + PLANE_HEIGHT + 5.f));
            window.draw(label);
            
            // Marca en el eje
            sf::VertexArray tick(sf::PrimitiveType::Lines, 2);
            tick[0].position = sf::Vector2f(screenX, PLANE_ORIGIN_Y + PLANE_HEIGHT);
            tick[1].position = sf::Vector2f(screenX, PLANE_ORIGIN_Y + PLANE_HEIGHT + 5.f);
            tick[0].color = tick[1].color = sf::Color::White;
            window.draw(tick);
        }
        
        // Etiquetas en eje Y (cada 20 unidades)
        for (float y = 0; y <= MAX_COORD; y += 20.f) {
            float normY = y / MAX_COORD;
            float screenY = PLANE_ORIGIN_Y + (1.f - normY) * PLANE_HEIGHT;
            
            sf::Text label(*font, std::to_string((int)y), 10);
            label.setFillColor(sf::Color(180, 180, 180));
            sf::FloatRect bounds = label.getLocalBounds();
            label.setPosition(sf::Vector2f(PLANE_ORIGIN_X - bounds.size.x - 8.f, screenY - bounds.size.y / 2.f));
            window.draw(label);
            
            // Marca en el eje
            sf::VertexArray tick(sf::PrimitiveType::Lines, 2);
            tick[0].position = sf::Vector2f(PLANE_ORIGIN_X, screenY);
            tick[1].position = sf::Vector2f(PLANE_ORIGIN_X - 5.f, screenY);
            tick[0].color = tick[1].color = sf::Color::White;
            window.draw(tick);
        }
        
        // Etiquetas "X" e "Y" en los ejes
        sf::Text labelX(*font, "X", 14);
        labelX.setFillColor(sf::Color::White);
        labelX.setStyle(sf::Text::Bold);
        labelX.setPosition(sf::Vector2f(PLANE_ORIGIN_X + PLANE_WIDTH + 10.f, PLANE_ORIGIN_Y + PLANE_HEIGHT - 10.f));
        window.draw(labelX);
        
        sf::Text labelY(*font, "Y", 14);
        labelY.setFillColor(sf::Color::White);
        labelY.setStyle(sf::Text::Bold);
        labelY.setPosition(sf::Vector2f(PLANE_ORIGIN_X + 5.f, PLANE_ORIGIN_Y - 20.f));
        window.draw(labelY);
    }
}

static void drawKDLinesRec(sf::RenderWindow& window, KDNode* nodo, const Region& region) {
    if (!nodo) return;

    int eje = nodo->nivel % 2;
    if (eje == 0) {
        float x = nodo->punto.x;
        sf::Vector2f p1 = mapToPlane(x, region.minY);
        sf::Vector2f p2 = mapToPlane(x, region.maxY);
        sf::VertexArray line(sf::PrimitiveType::Lines, 2);
        line[0].position = p1; line[1].position = p2;
        line[0].color = line[1].color = sf::Color(0,200,255);
        window.draw(line);

        Region leftRegion = region; leftRegion.maxX = x;
        Region rightRegion = region; rightRegion.minX = x;
        drawKDLinesRec(window, nodo->izquierdo, leftRegion);
        drawKDLinesRec(window, nodo->derecho, rightRegion);
    } else {
        float y = nodo->punto.y;
        sf::Vector2f p1 = mapToPlane(region.minX, y);
        sf::Vector2f p2 = mapToPlane(region.maxX, y);
        sf::VertexArray line(sf::PrimitiveType::Lines, 2);
        line[0].position = p1; line[1].position = p2;
        line[0].color = line[1].color = sf::Color(0,200,255);
        window.draw(line);

        Region lowerRegion = region; lowerRegion.maxY = y;
        Region upperRegion = region; upperRegion.minY = y;
        drawKDLinesRec(window, nodo->izquierdo, lowerRegion);
        drawKDLinesRec(window, nodo->derecho, upperRegion);
    }
}

static void drawKDLines(sf::RenderWindow& window, KDNode* root) {
    if (!root) return;
    Region initialRegion{0.f, MAX_COORD, 0.f, MAX_COORD};
    drawKDLinesRec(window, root, initialRegion);
}

// -------------------- Tree layout helpers --------------------
// Separación mínima entre hermanos (en píxeles)
const float MIN_SIBLING_SEPARATION = 50.f;
const float MIN_NODE_SEPARATION = 25.f;

// Calcula el ancho que ocupa un subárbol
static float calculateSubtreeWidth(KDNode* node, int& nodeCount) {
    if (!node) return 0.f;
    
    nodeCount++;
    
    // Si es hoja, solo ocupa su propio espacio
    if (!node->izquierdo && !node->derecho) {
        return MIN_NODE_SEPARATION;
    }
    
    int leftCount = 0, rightCount = 0;
    float leftWidth = calculateSubtreeWidth(node->izquierdo, leftCount);
    float rightWidth = calculateSubtreeWidth(node->derecho, rightCount);
    
    // El ancho total es la suma de ambos subárboles más la separación entre hermanos
    float totalWidth = leftWidth + rightWidth;
    if (node->izquierdo && node->derecho) {
        totalWidth += MIN_SIBLING_SEPARATION;
    }
    
    return totalWidth;
}

// Asigna posiciones X recursivamente con separación garantizada
static void assignPositionsImproved(KDNode* node,
                                   std::unordered_map<KDNode*, float>& xpos,
                                   float leftBound) {
    if (!node) return;
    
    // Si es hoja, centrarse en el espacio disponible
    if (!node->izquierdo && !node->derecho) {
        xpos[node] = leftBound + MIN_NODE_SEPARATION / 2.f;
        return;
    }
    
    // Calcular anchos de subárboles
    int leftCount = 0, rightCount = 0;
    float leftWidth = calculateSubtreeWidth(node->izquierdo, leftCount);
    float rightWidth = calculateSubtreeWidth(node->derecho, rightCount);
    
    // Asignar posiciones a los hijos
    if (node->izquierdo) {
        assignPositionsImproved(node->izquierdo, xpos, leftBound);
    }
    
    if (node->derecho) {
        float rightStart = leftBound + leftWidth;
        if (node->izquierdo) rightStart += MIN_SIBLING_SEPARATION;
        assignPositionsImproved(node->derecho, xpos, rightStart);
    }
    
    // Centrar el nodo padre sobre sus hijos
    if (node->izquierdo && node->derecho) {
        float leftX = xpos[node->izquierdo];
        float rightX = xpos[node->derecho];
        xpos[node] = (leftX + rightX) / 2.f;
    } else if (node->izquierdo) {
        xpos[node] = xpos[node->izquierdo];
    } else if (node->derecho) {
        xpos[node] = xpos[node->derecho];
    }
}

static void drawTreeRec2(sf::RenderWindow& window, KDNode* node,
                         const std::unordered_map<KDNode*, float>& xpos,
                         const sf::Font* font,
                         KDNode* highlightNode = nullptr,
                         bool isPulse = false) {
    if (!node) return;
    float x = xpos.at(node);
    float y = TREE_PADDING_Y + node->nivel * TREE_LEVEL_H;
    sf::Vector2f posNodo(x, y);

    // draw connectors
    if (node->izquierdo) {
        sf::Vector2f childPos(xpos.at(node->izquierdo), TREE_PADDING_Y + node->izquierdo->nivel * TREE_LEVEL_H);
        sf::VertexArray linea(sf::PrimitiveType::Lines, 2);
        linea[0].position = posNodo; linea[1].position = childPos;
        linea[0].color = linea[1].color = sf::Color::White;
        window.draw(linea);
    }
    if (node->derecho) {
        sf::Vector2f childPos(xpos.at(node->derecho), TREE_PADDING_Y + node->derecho->nivel * TREE_LEVEL_H);
        sf::VertexArray linea(sf::PrimitiveType::Lines, 2);
        linea[0].position = posNodo; linea[1].position = childPos;
        linea[0].color = linea[1].color = sf::Color::White;
        window.draw(linea);
    }

    // draw node
    float radio = 12.f;
    sf::CircleShape circle(radio);
    
    // Resaltar nodo durante animación
    if (node == highlightNode) {
        if (isPulse) {
            circle.setFillColor(sf::Color(255, 200, 0)); // Amarillo pulsante
            radio = 15.f;
            circle.setRadius(radio);
        } else {
            circle.setFillColor(sf::Color(255, 150, 0)); // Naranja
        }
    } else {
        circle.setFillColor(sf::Color(50, 120, 255));
    }
    
    circle.setOrigin(sf::Vector2f(radio, radio));
    circle.setPosition(posNodo);
    window.draw(circle);

    if (font) {
        std::string label = "(" + std::to_string((int)node->punto.x) + ", " + std::to_string((int)node->punto.y) + ")";
        sf::Text text(*font, label);
        text.setCharacterSize(12);
        text.setFillColor(sf::Color::White);
        sf::FloatRect tb = text.getLocalBounds();
        text.setOrigin({tb.size.x/2.f, tb.size.y});
        text.setPosition({posNodo.x, posNodo.y - radio - 6.f});
        window.draw(text);
        
        // Etiqueta del eje de comparación
        int eje = node->nivel % 2;
        std::string ejeLabel = (eje == 0) ? "X" : "Y";
        sf::Text ejeText(*font, ejeLabel);
        ejeText.setCharacterSize(10);
        ejeText.setFillColor((eje == 0) ? sf::Color(100, 255, 100) : sf::Color(255, 100, 255)); // Verde para X, Magenta para Y
        ejeText.setStyle(sf::Text::Bold);
        sf::FloatRect ejeBounds = ejeText.getLocalBounds();
        ejeText.setOrigin({ejeBounds.size.x/2.f, 0.f});
        ejeText.setPosition({posNodo.x, posNodo.y + radio + 4.f});
        window.draw(ejeText);
    }

    if (node->izquierdo) drawTreeRec2(window, node->izquierdo, xpos, font, highlightNode, isPulse);
    if (node->derecho) drawTreeRec2(window, node->derecho, xpos, font, highlightNode, isPulse);
}

void drawTree(sf::RenderWindow& window, KDNode* root, const sf::Font* font) {
    drawTree(window, root, font, nullptr, false);
}

void drawTree(sf::RenderWindow& window, KDNode* root, const sf::Font* font, 
             KDNode* highlightNode, bool isPulse) {
    if (!root) return;
    
    // Calcular ancho total del árbol
    int totalNodes = 0;
    float treeWidth = calculateSubtreeWidth(root, totalNodes);
    
    // Asignar posiciones comenzando desde 0
    std::unordered_map<KDNode*, float> xpos;
    assignPositionsImproved(root, xpos, 0.f);
    
    // Centrar el árbol en el panel derecho
    float offset = TREE_ORIGIN_X + TREE_PADDING_X + (TREE_WIDTH - treeWidth) / 2.f;
    for (auto &kv : xpos) {
        kv.second += offset;
    }

    drawTreeRec2(window, root, xpos, font, highlightNode, isPulse);
}

// Dibuja el panel de información de la animación
static void drawAnimationInfo(sf::RenderWindow& window, const AnimationState& anim, const sf::Font* font) {
    if (!font || anim.type == AnimationType::NONE || anim.currentStepIndex < 0) return;
    if (anim.currentStepIndex >= (int)anim.steps.size()) return;
    
    const auto& step = anim.steps[anim.currentStepIndex];
    
    // Panel de información en la parte inferior
    float panelY = WINDOW_HEIGHT - 80.f;
    sf::RectangleShape panel({(float)WINDOW_WIDTH, 80.f});
    panel.setPosition({0.f, panelY});
    panel.setFillColor(sf::Color(20, 20, 20, 240));
    window.draw(panel);
    
    // Título del algoritmo con complejidad
    sf::String title;
    std::string complexity;
    if (anim.type == AnimationType::INSERT) {
        title = toUtf8("ANIMACIÓN: INSERCIÓN");
        complexity = "O(log n) promedio";
    } else if (anim.type == AnimationType::SEARCH) {
        title = toUtf8("ANIMACIÓN: BÚSQUEDA NEAREST NEIGHBOR");
        complexity = "O(log n) promedio con poda";
    } else if (anim.type == AnimationType::RANGE_SEARCH) {
        title = toUtf8("ANIMACIÓN: BÚSQUEDA POR RANGO");
        complexity = "O(sqrt(n) + k) donde k = resultados";
    } else if (anim.type == AnimationType::DELETE) {
        title = toUtf8("ANIMACIÓN: ELIMINACIÓN");
        complexity = "O(log n) promedio";
    }
    sf::Text titleText(*font, title, 16);
    titleText.setFillColor(sf::Color::Yellow);
    titleText.setPosition({20.f, panelY + 10.f});
    titleText.setStyle(sf::Text::Bold);
    window.draw(titleText);
    
    // Mostrar complejidad
    sf::Text complexityText(*font, complexity, 11);
    complexityText.setFillColor(sf::Color(150, 200, 255));
    complexityText.setPosition({WINDOW_WIDTH - 400.f, panelY + 10.f});
    window.draw(complexityText);
    
    // Descripción del paso actual
    sf::Text desc(*font, toUtf8(step.description), 14);
    desc.setFillColor(sf::Color::White);
    desc.setPosition({20.f, panelY + 35.f});
    window.draw(desc);
    
    // Contador de pasos
    std::string counter = "Paso " + std::to_string(anim.currentStepIndex + 1) + " / " + std::to_string(anim.steps.size());
    sf::Text counterText(*font, counter, 12);
    counterText.setFillColor(sf::Color(200, 200, 200));
    counterText.setPosition({20.f, panelY + 60.f});
    window.draw(counterText);
    
    // Mostrar tiempo de ejecución real
    if (anim.executionTimeMicros > 0.0) {
        std::string timeInfo;
        if (anim.executionTimeMicros < 1000.0) {
            // Mostrar en microsegundos si es menor a 1ms con valor exacto
            timeInfo = "Tiempo: " + std::to_string(anim.executionTimeMicros) + " us";
        } else {
            // Mostrar en milisegundos con valor exacto
            double ms = anim.executionTimeMicros / 1000.0;
            timeInfo = "Tiempo: " + std::to_string(ms) + " ms";
        }
        sf::Text timeText(*font, timeInfo, 12);
        timeText.setFillColor(sf::Color(255, 200, 100));
        timeText.setStyle(sf::Text::Bold);
        timeText.setPosition({WINDOW_WIDTH - 250.f, panelY + 35.f});
        window.draw(timeText);
    }
    
    // Controles
    std::string controls = "[ESPACIO] Pausar  [<->] Pasos  [^v] Velocidad  [ESC] Cancelar  [R] Reset";
    sf::Text controlsText(*font, controls, 10);
    controlsText.setFillColor(sf::Color(150, 150, 150));
    controlsText.setPosition({250.f, panelY + 60.f});
    window.draw(controlsText);
    
    // Mostrar velocidad actual de animación
    std::string speedInfo = "Anim: " + std::to_string(anim.stepDuration).substr(0, 3) + "s";
    sf::Text speedText(*font, speedInfo, 11);
    speedText.setFillColor(sf::Color(100, 255, 100));
    speedText.setPosition({WINDOW_WIDTH - 100.f, panelY + 60.f});
    window.draw(speedText);
}

void runVisualizer(KDTree& tree, std::vector<Punto2D>& puntos) {
    // Abrir la ventana en modo fullscreen usando la resolución del escritorio
    sf::VideoMode desktopMode = sf::VideoMode::getDesktopMode();
    // SFML3 moved window styles; use sf::State::Fullscreen
    sf::RenderWindow window(desktopMode, toUtf8("Visualizador KD-Tree"), sf::State::Fullscreen);

    // Ajustar tamaños de layout para fullscreen: el plano ocupa la mitad izquierda
    // Obtener tamaño real de la ventana creada (SFML3 compatible)
    sf::Vector2u winSize = window.getSize();
    WINDOW_WIDTH = static_cast<int>(winSize.x);
    WINDOW_HEIGHT = static_cast<int>(winSize.y);
    PLANE_WIDTH = static_cast<float>(WINDOW_WIDTH) / 2.f; // mitad izquierda
    PLANE_HEIGHT = static_cast<float>(WINDOW_HEIGHT);     // ocupar toda la altura
    PLANE_ORIGIN_X = 0.f;
    PLANE_ORIGIN_Y = 0.f;
    TREE_ORIGIN_X = PLANE_WIDTH;
    TREE_WIDTH = static_cast<float>(WINDOW_WIDTH) - TREE_ORIGIN_X - TREE_PADDING_X;

    // Cargar fuente para labels si está disponible
    sf::Font labelFont;
    const sf::Font* fontPtr = nullptr;
    if (labelFont.openFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf")) fontPtr = &labelFont;
    else if (labelFont.openFromFile("/usr/share/fonts/truetype/freefont/FreeSans.ttf")) fontPtr = &labelFont;
    else std::cerr << "Warning: no system font found; coordinate labels disabled\n";

    // --- Simple UI: input X/Y + button ---
    std::string inputX;
    std::string inputY;
    bool activeX = false, activeY = false;

    const float INPUT_W = 120.f;
    const float INPUT_H = 28.f;
    const float INPUT_MARGIN = 10.f;
    const float INPUT_DOWN_OFFSET = 20.f; // pushed down as requested
    const float BUTTON_W = 100.f;
    const float BUTTON_H = 28.f;

    sf::RectangleShape boxX({INPUT_W, INPUT_H});
    boxX.setPosition({PLANE_ORIGIN_X + INPUT_MARGIN, PLANE_ORIGIN_Y + INPUT_MARGIN + INPUT_DOWN_OFFSET});
    boxX.setFillColor(sf::Color(50,50,50)); boxX.setOutlineThickness(2.f); boxX.setOutlineColor(sf::Color(100,100,100));

    sf::RectangleShape boxY({INPUT_W, INPUT_H});
    boxY.setPosition({PLANE_ORIGIN_X + INPUT_MARGIN + INPUT_W + INPUT_MARGIN, PLANE_ORIGIN_Y + INPUT_MARGIN + INPUT_DOWN_OFFSET});
    boxY.setFillColor(sf::Color(50,50,50)); boxY.setOutlineThickness(2.f); boxY.setOutlineColor(sf::Color(100,100,100));

    sf::RectangleShape button({BUTTON_W, BUTTON_H});
    button.setPosition({boxY.getPosition().x + INPUT_W + INPUT_MARGIN, PLANE_ORIGIN_Y + INPUT_MARGIN + INPUT_DOWN_OFFSET});
    button.setFillColor(sf::Color(80,160,80)); button.setOutlineThickness(2.f); button.setOutlineColor(sf::Color(60,120,60));

    // Search button (Buscar) placed to the right of the Add button
    sf::RectangleShape buttonSearch({BUTTON_W, BUTTON_H});
    buttonSearch.setPosition({button.getPosition().x + BUTTON_W + INPUT_MARGIN, PLANE_ORIGIN_Y + INPUT_MARGIN + INPUT_DOWN_OFFSET});
    buttonSearch.setFillColor(sf::Color(80,120,200)); buttonSearch.setOutlineThickness(2.f); buttonSearch.setOutlineColor(sf::Color(60,90,160));
    
    // Range button (Rango) placed to the right of the Search button
    sf::RectangleShape buttonRange({BUTTON_W, BUTTON_H});
    buttonRange.setPosition({buttonSearch.getPosition().x + BUTTON_W + INPUT_MARGIN, PLANE_ORIGIN_Y + INPUT_MARGIN + INPUT_DOWN_OFFSET});
    buttonRange.setFillColor(sf::Color(200,100,80)); buttonRange.setOutlineThickness(2.f); buttonRange.setOutlineColor(sf::Color(160,80,60));

    std::unique_ptr<sf::Text> textX, textY, textBtn, textSearch, textRange, labelX, labelY;
    if (fontPtr) {
        textX = std::make_unique<sf::Text>(*fontPtr, "", 14);
        textY = std::make_unique<sf::Text>(*fontPtr, "", 14);
        textBtn = std::make_unique<sf::Text>(*fontPtr, toUtf8("Añadir"), 14);
        textSearch = std::make_unique<sf::Text>(*fontPtr, toUtf8("Buscar"), 14);
        textRange = std::make_unique<sf::Text>(*fontPtr, toUtf8("Rango"), 14);
        labelX = std::make_unique<sf::Text>(*fontPtr, "X:", 12);
        labelY = std::make_unique<sf::Text>(*fontPtr, "Y:", 12);
        textX->setFillColor(sf::Color::White); textY->setFillColor(sf::Color::White); textBtn->setFillColor(sf::Color::Black);
        textSearch->setFillColor(sf::Color::Black);
        textRange->setFillColor(sf::Color::Black);
        // Demo button label
        // se inicializa más abajo cuando se crea el botón
        labelX->setFillColor(sf::Color::White); labelY->setFillColor(sf::Color::White);
    }

    // Demo button (carga dataset sintético)
    sf::RectangleShape buttonDemo({BUTTON_W, BUTTON_H});
    buttonDemo.setPosition({buttonRange.getPosition().x + BUTTON_W + INPUT_MARGIN, PLANE_ORIGIN_Y + INPUT_MARGIN + INPUT_DOWN_OFFSET});
    buttonDemo.setFillColor(sf::Color(140,160,220)); buttonDemo.setOutlineThickness(2.f); buttonDemo.setOutlineColor(sf::Color(100,120,180));
    std::unique_ptr<sf::Text> textDemo;
    if (fontPtr) {
        textDemo = std::make_unique<sf::Text>(*fontPtr, toUtf8("Demo"), 14);
        textDemo->setFillColor(sf::Color::Black);
    }

    // Botón k-NN
    sf::RectangleShape buttonKNN({BUTTON_W, BUTTON_H});
    buttonKNN.setPosition({buttonDemo.getPosition().x + BUTTON_W + INPUT_MARGIN, PLANE_ORIGIN_Y + INPUT_MARGIN + INPUT_DOWN_OFFSET});
    buttonKNN.setFillColor(sf::Color(150, 50, 150)); 
    buttonKNN.setOutlineThickness(2.f); 
    buttonKNN.setOutlineColor(sf::Color(100, 30, 100));
    
    std::unique_ptr<sf::Text> textKNN;
    if (fontPtr) {
        textKNN = std::make_unique<sf::Text>(*fontPtr, toUtf8("k-NN"), 14);
        textKNN->setFillColor(sf::Color::White);
    }

    // Input para k
    std::string inputK = "3";
    bool activeK = false;
    const float INPUT_K_W = 40.f;
    sf::RectangleShape boxK({INPUT_K_W, INPUT_H});
    boxK.setPosition({buttonKNN.getPosition().x + BUTTON_W + 5.f, PLANE_ORIGIN_Y + INPUT_MARGIN + INPUT_DOWN_OFFSET});
    boxK.setFillColor(sf::Color(50,50,50)); 
    boxK.setOutlineThickness(2.f); 
    boxK.setOutlineColor(sf::Color(100,100,100));
    
    std::unique_ptr<sf::Text> textInputK;
    if (fontPtr) {
        textInputK = std::make_unique<sf::Text>(*fontPtr, inputK, 14);
        textInputK->setFillColor(sf::Color::White);
    }

    // Botón Eliminar
    sf::RectangleShape buttonDelete({BUTTON_W, BUTTON_H});
    buttonDelete.setPosition({boxK.getPosition().x + INPUT_K_W + INPUT_MARGIN, PLANE_ORIGIN_Y + INPUT_MARGIN + INPUT_DOWN_OFFSET});
    buttonDelete.setFillColor(sf::Color(200, 50, 50));
    buttonDelete.setOutlineThickness(2.f); 
    buttonDelete.setOutlineColor(sf::Color(160, 40, 40));
    
    std::unique_ptr<sf::Text> textDelete;
    if (fontPtr) {
        textDelete = std::make_unique<sf::Text>(*fontPtr, toUtf8("Eliminar"), 14);
        textDelete->setFillColor(sf::Color::White);
    }

    // Nearest search result
    bool hasNearest = false;
    Punto2D nearestPoint{0.f,0.f};
    
    // Estado de animación
    AnimationState animState;
    
    // Estado de búsqueda por rango
    RangeSearchState rangeState;
    
    // Estado k-NN
    KNNState knnState;
    
    // Modo delete
    bool deleteMode = false;
    
    // Estado demo (hospital)
    bool demoLoaded = false;
    std::vector<float> puntosAge; // edad por punto, alineado con 'puntos'
    std::vector<int> demoNeighbors; // indices de vecinos resaltados
    int selectedIndex = -1;
    int demoK = 5;

    while (window.isOpen()) {
        // Avance automático de animación
        if (animState.type != AnimationType::NONE && !animState.paused && animState.autoAdvance) {
            if (animState.stepClock.getElapsedTime().asSeconds() >= animState.stepDuration) {
                animState.currentStepIndex++;
                if (animState.currentStepIndex >= (int)animState.steps.size()) {
                    // Animación terminada
                    if (animState.type == AnimationType::SEARCH && animState.hasResult) {
                        hasNearest = true;
                        nearestPoint = animState.foundNearest;
                    }
                    // Para RANGE_SEARCH, los resultados ya están en rangeState.puntosEncontrados
                    animState.type = AnimationType::NONE;
                    animState.currentStepIndex = -1;
                } else {
                    animState.stepClock.restart();
                }
            }
        }
        
        while (auto event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) { window.close(); break; }

            if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                if (key->scancode == sf::Keyboard::Scancode::Escape) {
                    if (animState.type != AnimationType::NONE) {
                        // Cancelar animación
                        animState.type = AnimationType::NONE;
                        animState.currentStepIndex = -1;
                    } else {
                        window.close();
                    }
                    break;
                }
                
                // Tecla R para resetear visualizaciones
                if (key->scancode == sf::Keyboard::Scancode::R) {
                    // Cancelar cualquier animación
                    animState.type = AnimationType::NONE;
                    animState.currentStepIndex = -1;
                    animState.steps.clear();
                    
                    // Limpiar resultado de búsqueda nearest
                    hasNearest = false;
                    
                    // Limpiar estado de búsqueda por rango
                    rangeState.modoActivo = false;
                    rangeState.dibujando = false;
                    rangeState.tieneResultado = false;
                    rangeState.puntosEncontrados.clear();
                    
                    // Limpiar inputs
                    activeX = false;
                    activeY = false;
                    
                    std::cout << "Reset: Se limpiaron todas las visualizaciones\n";
                }
                
                // Controles de animación
                if (animState.type != AnimationType::NONE) {
                    if (key->scancode == sf::Keyboard::Scancode::Space) {
                        animState.paused = !animState.paused;
                        animState.stepClock.restart();
                    }
                    if (key->scancode == sf::Keyboard::Scancode::Right) {
                        if (animState.currentStepIndex < (int)animState.steps.size() - 1) {
                            animState.currentStepIndex++;
                            animState.stepClock.restart();
                        }
                    }
                    if (key->scancode == sf::Keyboard::Scancode::Left) {
                        if (animState.currentStepIndex > 0) {
                            animState.currentStepIndex--;
                            animState.stepClock.restart();
                        }
                    }
                    // Control de velocidad con teclas Up/Down
                    if (key->scancode == sf::Keyboard::Scancode::Up) {
                        animState.stepDuration = std::max(0.1f, animState.stepDuration - 0.1f);
                        std::cout << "Velocidad aumentada: " << animState.stepDuration << "s por paso\n";
                    }
                    if (key->scancode == sf::Keyboard::Scancode::Down) {
                        animState.stepDuration = std::min(2.0f, animState.stepDuration + 0.1f);
                        std::cout << "Velocidad reducida: " << animState.stepDuration << "s por paso\n";
                    }
                    continue;
                }
                // Backspace handling
                if (key->scancode == sf::Keyboard::Scancode::Backspace) {
                    if (activeX && !inputX.empty()) inputX.pop_back();
                    else if (activeY && !inputY.empty()) inputY.pop_back();
                    else if (activeK && !inputK.empty()) inputK.pop_back();
                }
                if (key->scancode == sf::Keyboard::Scancode::Enter || key->scancode == sf::Keyboard::Scancode::NumpadEnter) {
                    try {
                        if (!inputX.empty() && !inputY.empty()) {
                            float rx = std::stof(inputX);
                            float ry = std::stof(inputY);
                            Punto2D p{rx, ry};
                            tree.insert(p);
                            puntos.push_back(p);
                            inputX.clear(); inputY.clear();
                        }
                    } catch(...){}
                }
            }

            if (const auto* textE = event->getIf<sf::Event::TextEntered>()) {
                char32_t code = textE->unicode;
                if (code < 128) {
                    char c = static_cast<char>(code);
                    if (activeX) { if ((c>='0'&&c<='9')||c=='.'||c=='-') inputX.push_back(c); }
                    else if (activeY) { if ((c>='0'&&c<='9')||c=='.'||c=='-') inputY.push_back(c); }
                    else if (activeK) { if (c>='0'&&c<='9') inputK.push_back(c); }  // Solo dígitos para k
                }
            }

            if (const auto* mouse = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (mouse->button == sf::Mouse::Button::Left) {
                    sf::Vector2f mpos((float)mouse->position.x, (float)mouse->position.y);
                    if (boxX.getGlobalBounds().contains(mpos)) { activeX=true; activeY=false; activeK=false; }
                    else if (boxY.getGlobalBounds().contains(mpos)) { activeX=false; activeY=true; activeK=false; }
                    else if (button.getGlobalBounds().contains(mpos)) {
                        // Cambiar comportamiento según modo
                        try {
                            if (!inputX.empty() && !inputY.empty()) {
                                float x = std::stof(inputX);
                                float y = std::stof(inputY);
                                Punto2D p{x, y};
                                
                                if (deleteMode) {
                                    // Método 2: Eliminar por coordenadas
                                    bool found = false;
                                    int foundIdx = -1;
                                    for (size_t i = 0; i < puntos.size(); ++i) {
                                        if (std::abs(puntos[i].x - p.x) < 0.01f && 
                                            std::abs(puntos[i].y - p.y) < 0.01f) {
                                            found = true;
                                            foundIdx = i;
                                            break;
                                        }
                                    }
                                    
                                    if (found) {
                                        // Eliminar
                                        generateDeleteAnimation(p, animState);
                                        animState.currentStepIndex = 0;
                                        animState.stepClock.restart();
                                        animState.paused = false;
                                        
                                        tree.remove(p);
                                        puntos.erase(puntos.begin() + foundIdx);
                                        if (demoLoaded && foundIdx < (int)puntosAge.size()) {
                                            puntosAge.erase(puntosAge.begin() + foundIdx);
                                        }
                                        
                                        inputX.clear(); inputY.clear();
                                        std::cout << "Punto eliminado (coords): (" << p.x << ", " << p.y << ")\n";
                                    } else {
                                        std::cout << "Punto no encontrado: (" << p.x << ", " << p.y << ")\n";
                                    }
                                } else {
                                    // Modo normal: Insertar
                                    // Iniciar animación de inserción si hay árbol
                                    if (tree.getRoot() != nullptr) {
                                        generateInsertAnimation(tree.getRoot(), p, animState);
                                        animState.currentStepIndex = 0;
                                        animState.stepClock.restart();
                                        animState.paused = false;
                                    }
                                    
                                    // Medir tiempo de inserción
                                    auto start = std::chrono::high_resolution_clock::now();
                                    tree.insert(p); 
                                    auto end = std::chrono::high_resolution_clock::now();
                                    animState.executionTimeMicros = std::chrono::duration<double, std::micro>(end - start).count();
                                    
                                    puntos.push_back(p); 
                                    inputX.clear(); inputY.clear();
                                    
                                    std::cout << "Tiempo de ejecución insert: " << animState.executionTimeMicros << " us\n";
                                }
                            }
                        } catch(...){}
                        activeX = activeY = false;
                    } else if (buttonSearch.getGlobalBounds().contains(mpos)) {
                        // Iniciar animación de búsqueda
                        try {
                            if (!inputX.empty() && !inputY.empty() && tree.getRoot()!=nullptr) {
                                float tx = std::stof(inputX); float ty = std::stof(inputY);
                                Punto2D target{tx, ty};
                                
                                // Generar animación de búsqueda
                                generateSearchAnimation(tree.getRoot(), target, animState);
                                animState.currentStepIndex = 0;
                                animState.stepClock.restart();
                                animState.paused = false;
                                
                                // Calcular resultado y medir tiempo
                                auto start = std::chrono::high_resolution_clock::now();
                                nearestPoint = tree.nearest(target);
                                auto end = std::chrono::high_resolution_clock::now();
                                animState.executionTimeMicros = std::chrono::duration<double, std::micro>(end - start).count();
                                
                                animState.foundNearest = nearestPoint;
                                animState.hasResult = true;
                                
                                std::cout << "Tiempo de ejecución nearest neighbor: " << animState.executionTimeMicros << " us\n";
                                    }
                                } catch(...) { }
                                activeX = activeY = false;
                            } else if (buttonDemo.getGlobalBounds().contains(mpos)) {
                                // Cargar dataset sintético (modo demo)
                                try {
                                    // Reducir a 25 puntos para que quepan en el visualizador
                                    const int N = 25;
                                    puntos.clear(); puntosAge.clear(); demoNeighbors.clear(); selectedIndex = -1;
                                    // Reiniciar KDTree (nota: posibles fugas de memoria en demo, tolerable aquí)
                                    tree = KDTree();
                                    std::random_device rd; std::mt19937 gen(rd());
                                    // Generar puntos repartidos uniformemente en todo el rango del plano
                                    // dejar un pequeño margen interior (2% del MAX_COORD)
                                    float margin = MAX_COORD * 0.02f;
                                    std::uniform_real_distribution<float> distX(margin, MAX_COORD - margin);
                                    std::uniform_real_distribution<float> distY(margin, MAX_COORD - margin);
                                    std::uniform_real_distribution<float> distA(20.f, 90.f);
                                    for (int i = 0; i < N; ++i) {
                                        Punto2D p{distX(gen), distY(gen)};
                                        puntos.push_back(p);
                                        puntosAge.push_back(distA(gen));
                                        tree.insert(p);
                                    }
                                    demoLoaded = true;
                                    std::cout << "Demo cargado: " << N << " puntos\n";
                                } catch(...){}
                                activeX = activeY = false;
                    } else if (buttonRange.getGlobalBounds().contains(mpos)) {
                        // Activar/desactivar modo de búsqueda por rango
                        rangeState.modoActivo = !rangeState.modoActivo;
                        knnState.modoActivo = false;
                        deleteMode = false;
                        if (!rangeState.modoActivo) {
                            // Limpiar estado al desactivar
                            rangeState.dibujando = false;
                            rangeState.tieneResultado = false;
                            rangeState.puntosEncontrados.clear();
                        }
                        activeX = activeY = activeK = false;
                    } else if (buttonKNN.getGlobalBounds().contains(mpos)) {
                        // Activar/desactivar modo k-NN
                        knnState.modoActivo = !knnState.modoActivo;
                        rangeState.modoActivo = false;
                        deleteMode = false;
                        if (!knnState.modoActivo) {
                            knnState.tieneResultado = false;
                            knnState.puntosEncontrados.clear();
                        }
                        activeX = activeY = activeK = false;
                    } else if (boxK.getGlobalBounds().contains(mpos)) {
                        // Activar input de k
                        activeK = true;
                        activeX = activeY = false;
                    } else if (buttonDelete.getGlobalBounds().contains(mpos)) {
                        // Activar/desactivar modo eliminar
                        deleteMode = !deleteMode;
                        rangeState.modoActivo = false;
                        knnState.modoActivo = false;
                        activeX = activeY = activeK = false;
                    } else {
                        // click in plane
                        int mx = mouse->position.x; int my = mouse->position.y;
                        if (mx >= (int)PLANE_ORIGIN_X && mx <= (int)(PLANE_ORIGIN_X + PLANE_WIDTH)
                            && my >= (int)PLANE_ORIGIN_Y && my <= (int)(PLANE_ORIGIN_Y + PLANE_HEIGHT)) {
                            
                            if (rangeState.modoActivo) {
                                // Iniciar dibujo de rectángulo
                                rangeState.dibujando = true;
                                rangeState.puntoInicio = mpos;
                                rangeState.puntoFin = mpos;
                                rangeState.tieneResultado = false;
                                rangeState.puntosEncontrados.clear();
                            } else if (knnState.modoActivo) {
                                // Modo k-NN: buscar k vecinos más cercanos
                                try {
                                    int k = std::stoi(inputK);
                                    if (k > 0 && tree.getRoot() != nullptr) {
                                        float realX = (float)(mx - PLANE_ORIGIN_X) / PLANE_WIDTH * MAX_COORD;
                                        float realY = (1.f - (float)(my - PLANE_ORIGIN_Y) / PLANE_HEIGHT) * MAX_COORD;
                                        Punto2D target{realX, realY};
                                        knnState.puntoObjetivo = target;
                                        
                                        // Ejecutar k-NN y medir tiempo
                                        auto start = std::chrono::high_resolution_clock::now();
                                        knnState.puntosEncontrados = tree.kNearest(target, k);
                                        auto end = std::chrono::high_resolution_clock::now();
                                        animState.executionTimeMicros = std::chrono::duration<double, std::micro>(end - start).count();
                                        
                                        // Generar animación
                                        generateKNNAnimation(target, k, knnState.puntosEncontrados, animState);
                                        animState.currentStepIndex = 0;
                                        animState.stepClock.restart();
                                        animState.paused = false;
                                        knnState.tieneResultado = true;
                                        
                                        std::cout << "k-NN (k=" << k << ") ejecutado en " 
                                                  << animState.executionTimeMicros << " us\n";
                                        std::cout << "Encontrados: " << knnState.puntosEncontrados.size() << " vecinos\n";
                                    }
                                } catch(...) {
                                    std::cout << "Error: k debe ser un número válido > 0\n";
                                }
                            } else if (deleteMode) {
                                // Modo eliminar
                                // Método 1: Click directo en punto
                                if (inputX.empty() && inputY.empty()) {
                                    // Buscar punto más cercano al click
                                    if (!puntos.empty()) {
                                        float bestDist2 = 1e12f;
                                        int bestIdx = -1;
                                        for (size_t i = 0; i < puntos.size(); ++i) {
                                            sf::Vector2f sp = mapToPlane(puntos[i]);
                                            float dx = sp.x - mpos.x;
                                            float dy = sp.y - mpos.y;
                                            float d2 = dx*dx + dy*dy;
                                            if (d2 < bestDist2) { bestDist2 = d2; bestIdx = (int)i; }
                                        }
                                        const float PICK_RADIUS = 15.f;
                                        if (bestIdx >= 0 && bestDist2 <= PICK_RADIUS*PICK_RADIUS) {
                                            Punto2D pToDelete = puntos[bestIdx];
                                            
                                            // Generar animación
                                            generateDeleteAnimation(pToDelete, animState);
                                            animState.currentStepIndex = 0;
                                            animState.stepClock.restart();
                                            animState.paused = false;
                                            
                                            // Eliminar del árbol y del vector
                                            tree.remove(pToDelete);
                                            puntos.erase(puntos.begin() + bestIdx);
                                            if (demoLoaded && bestIdx < (int)puntosAge.size()) {
                                                puntosAge.erase(puntosAge.begin() + bestIdx);
                                            }
                                            
                                            std::cout << "Punto eliminado (click): (" << pToDelete.x << ", " << pToDelete.y << ")\n";
                                        }
                                    }
                                }
                            } else {
                                // Si modo demo cargado: seleccionar punto cercano en vez de insertar
                                if (demoLoaded && !puntos.empty()) {
                                    float bestDist2 = 1e12f; int bestIdx = -1;
                                    for (size_t i=0;i<puntos.size();++i) {
                                        sf::Vector2f sp = mapToPlane(puntos[i]);
                                        float dx = sp.x - mpos.x; float dy = sp.y - mpos.y;
                                        float d2 = dx*dx + dy*dy;
                                        if (d2 < bestDist2) { bestDist2 = d2; bestIdx = (int)i; }
                                    }
                                    const float PICK_RADIUS = 10.f;
                                    if (bestIdx >= 0 && bestDist2 <= PICK_RADIUS*PICK_RADIUS) {
                                        // Seleccionado paciente
                                        selectedIndex = bestIdx;
                                        Punto2D target = puntos[selectedIndex];

                                        // Generar animación nearest
                                        if (tree.getRoot() != nullptr) {
                                            generateSearchAnimation(tree.getRoot(), target, animState);
                                            animState.currentStepIndex = 0;
                                            animState.stepClock.restart();
                                            animState.paused = false;
                                        }

                                        // Medir nearest real
                                        auto start = std::chrono::high_resolution_clock::now();
                                        Punto2D nn = tree.nearest(target);
                                        auto end = std::chrono::high_resolution_clock::now();
                                        animState.executionTimeMicros = std::chrono::duration<double, std::micro>(end - start).count();
                                        animState.foundNearest = nn; animState.hasResult = true;

                                        // Calcular k vecinos por fuerza bruta (2D)
                                        demoNeighbors.clear();
                                        std::vector<std::pair<float,int>> tmp;
                                        for (size_t i=0;i<puntos.size();++i) {
                                            if ((int)i == selectedIndex) continue;
                                            float dx = puntos[i].x - target.x; float dy = puntos[i].y - target.y;
                                            tmp.push_back({dx*dx+dy*dy, (int)i});
                                        }
                                        std::sort(tmp.begin(), tmp.end());
                                        for (int k=0;k<demoK && k<(int)tmp.size(); ++k) demoNeighbors.push_back(tmp[k].second);

                                        std::cout << "Paciente seleccionado: idx=" << selectedIndex << ", tiempo nearest: " << animState.executionTimeMicros << " us\n";
                                    } else {
                                        // No cercano: insertar nuevo punto
                                        float realX = (float)(mx - PLANE_ORIGIN_X) / PLANE_WIDTH * MAX_COORD;
                                        float realY = (1.f - (float)(my - PLANE_ORIGIN_Y) / PLANE_HEIGHT) * MAX_COORD;
                                        Punto2D p{realX, realY}; tree.insert(p); puntos.push_back(p); puntosAge.push_back(45.f);
                                    }
                                } else {
                                    // Insertar punto por coordenadas
                                    float realX = (float)(mx - PLANE_ORIGIN_X) / PLANE_WIDTH * MAX_COORD;
                                    float realY = (1.f - (float)(my - PLANE_ORIGIN_Y) / PLANE_HEIGHT) * MAX_COORD;
                                    Punto2D p{realX, realY}; tree.insert(p); puntos.push_back(p);
                                }
                            }
                        }
                        activeX = activeY = false;
                    }
                }
            }
            
            if (const auto* mouse = event->getIf<sf::Event::MouseMoved>()) {
                if (rangeState.dibujando) {
                    // Actualizar punto final mientras se dibuja
                    rangeState.puntoFin = sf::Vector2f((float)mouse->position.x, (float)mouse->position.y);
                }
            }
            
            if (const auto* mouse = event->getIf<sf::Event::MouseButtonReleased>()) {
                if (mouse->button == sf::Mouse::Button::Left && rangeState.dibujando) {
                    rangeState.dibujando = false;
                    
                    // Convertir coordenadas de pantalla a coordenadas del plano
                    float x1 = (rangeState.puntoInicio.x - PLANE_ORIGIN_X) / PLANE_WIDTH * MAX_COORD;
                    float y1 = (1.f - (rangeState.puntoInicio.y - PLANE_ORIGIN_Y) / PLANE_HEIGHT) * MAX_COORD;
                    float x2 = (rangeState.puntoFin.x - PLANE_ORIGIN_X) / PLANE_WIDTH * MAX_COORD;
                    float y2 = (1.f - (rangeState.puntoFin.y - PLANE_ORIGIN_Y) / PLANE_HEIGHT) * MAX_COORD;
                    
                    // Crear rectángulo (min/max para que funcione en cualquier dirección)
                    rangeState.rectangulo.xmin = std::min(x1, x2);
                    rangeState.rectangulo.xmax = std::max(x1, x2);
                    rangeState.rectangulo.ymin = std::min(y1, y2);
                    rangeState.rectangulo.ymax = std::max(y1, y2);
                    
                    // Iniciar animación de búsqueda por rango y medir tiempo
                    if (tree.getRoot() != nullptr) {
                        // Primero hacer la búsqueda real para medir el tiempo
                        auto start = std::chrono::high_resolution_clock::now();
                        std::vector<Punto2D> resultados = tree.rangeSearch(rangeState.rectangulo);
                        auto end = std::chrono::high_resolution_clock::now();
                        animState.executionTimeMicros = std::chrono::duration<double, std::micro>(end - start).count();
                        
                        // Luego generar la animación (usa el mismo algoritmo internamente)
                        generateRangeSearchAnimation(tree.getRoot(), rangeState.rectangulo, 
                                                    animState, rangeState.puntosEncontrados);
                        animState.currentStepIndex = 0;
                        animState.stepClock.restart();
                        animState.paused = false;
                        rangeState.tieneResultado = true;
                        
                        std::cout << "Tiempo de ejecución range search: " << animState.executionTimeMicros 
                                  << " us (" << (animState.executionTimeMicros / 1000.0) << " ms)\n";
                    }
                }
            }
        }

        window.clear(sf::Color::Black);

        drawAxesAndGrid(window, fontPtr);
        drawKDLines(window, tree.getRoot());

        // draw UI
        if (fontPtr) {
            textX->setString(inputX); textY->setString(inputY);
            sf::FloatRect tbx = textX->getLocalBounds(); sf::FloatRect tby = textY->getLocalBounds();
            textX->setPosition(sf::Vector2f(boxX.getPosition().x + 6.f, boxX.getPosition().y + (INPUT_H - tbx.size.y)/2.f));
            textY->setPosition(sf::Vector2f(boxY.getPosition().x + 6.f, boxY.getPosition().y + (INPUT_H - tby.size.y)/2.f));
            sf::FloatRect tbb = textBtn->getLocalBounds();
            textBtn->setPosition(sf::Vector2f(button.getPosition().x + (BUTTON_W - tbb.size.x)/2.f, button.getPosition().y + (BUTTON_H - tbb.size.y)/2.f));
            sf::FloatRect tsb = textSearch->getLocalBounds();
            textSearch->setPosition(sf::Vector2f(buttonSearch.getPosition().x + (BUTTON_W - tsb.size.x)/2.f, buttonSearch.getPosition().y + (BUTTON_H - tsb.size.y)/2.f));
            sf::FloatRect trb = textRange->getLocalBounds();
            textRange->setPosition(sf::Vector2f(buttonRange.getPosition().x + (BUTTON_W - trb.size.x)/2.f, buttonRange.getPosition().y + (BUTTON_H - trb.size.y)/2.f));
            
            // Posicionar texto de botones nuevos
            sf::FloatRect tkb = textKNN->getLocalBounds();
            textKNN->setPosition(sf::Vector2f(buttonKNN.getPosition().x + (BUTTON_W - tkb.size.x)/2.f, buttonKNN.getPosition().y + (BUTTON_H - tkb.size.y)/2.f));
            
            textInputK->setString(inputK);
            sf::FloatRect tik = textInputK->getLocalBounds();
            textInputK->setPosition(sf::Vector2f(boxK.getPosition().x + (INPUT_K_W - tik.size.x)/2.f, boxK.getPosition().y + (INPUT_H - tik.size.y)/2.f));
            
            sf::FloatRect tdb = textDelete->getLocalBounds();
            textDelete->setPosition(sf::Vector2f(buttonDelete.getPosition().x + (BUTTON_W - tdb.size.x)/2.f, buttonDelete.getPosition().y + (BUTTON_H - tdb.size.y)/2.f));
            
            // Cambiar colores según modo activo
            if (rangeState.modoActivo) {
                rangeState.modoActivo = true;
                buttonRange.setFillColor(sf::Color(255, 150, 100));
            } else {
                buttonRange.setFillColor(sf::Color(200, 100, 80));
            }
            
            // Cambiar color k-NN si está activo
            if (knnState.modoActivo) {
                buttonKNN.setFillColor(sf::Color(200, 80, 200));
            } else {
                buttonKNN.setFillColor(sf::Color(150, 50, 150));
            }
            
            // Cambiar color Delete si está activo
            if (deleteMode) {
                buttonDelete.setFillColor(sf::Color(255, 80, 80));
                // Cambiar color de inputs para indicar modo delete
                boxX.setFillColor(sf::Color(80, 30, 30));
                boxY.setFillColor(sf::Color(80, 30, 30));
                // Deshabilitar visualmente botón Añadir
                button.setFillColor(sf::Color(50, 80, 50));
            } else {
                buttonDelete.setFillColor(sf::Color(200, 50, 50));
                // Restaurar colores normales
                boxX.setFillColor(sf::Color(50,50,50));
                boxY.setFillColor(sf::Color(50,50,50));
                button.setFillColor(sf::Color(80,160,80));
            }
            
            // center labels
            sf::FloatRect lbx = labelX->getLocalBounds(); sf::FloatRect lby = labelY->getLocalBounds();
            labelX->setPosition(sf::Vector2f(boxX.getPosition().x + (INPUT_W - lbx.size.x)/2.f, boxX.getPosition().y - 8.f));
            labelY->setPosition(sf::Vector2f(boxY.getPosition().x + (INPUT_W - lby.size.x)/2.f, boxY.getPosition().y - 8.f));
            
            // Draw all UI elements
            window.draw(boxX); window.draw(boxY); window.draw(button); window.draw(buttonSearch); window.draw(buttonRange); window.draw(buttonDemo);
            window.draw(buttonKNN); window.draw(boxK); window.draw(buttonDelete);
            window.draw(*labelX); window.draw(*labelY); window.draw(*textX); window.draw(*textY); window.draw(*textBtn); window.draw(*textSearch); window.draw(*textRange);
            window.draw(*textKNN); window.draw(*textInputK); window.draw(*textDelete);
            if (textDemo) {
                // position the demo label centered in its button
                sf::FloatRect tdd = textDemo->getLocalBounds();
                textDemo->setPosition(sf::Vector2f(buttonDemo.getPosition().x + (BUTTON_W - tdd.size.x)/2.f, buttonDemo.getPosition().y + (BUTTON_H - tdd.size.y)/2.f));
                window.draw(*textDemo);
            }
        } else {
            window.draw(boxX); window.draw(boxY); window.draw(button); window.draw(buttonSearch); window.draw(buttonRange); window.draw(buttonDemo);
        }

        for (size_t i = 0; i < puntos.size(); ++i) {
            const auto &p = puntos[i];
            sf::Color col = sf::Color::Red;
            float radius = 5.f;

            if (demoLoaded && i < puntosAge.size()) {
                col = mapAgeToColor(puntosAge[i]);
                radius = mapAgeToRadius(puntosAge[i]);
            }

            // Highlight selected patient
            if ((int)i == selectedIndex) {
                col = sf::Color::Yellow;
                radius += 4.f;
            } else {
                // Highlight neighbors
                if (std::find(demoNeighbors.begin(), demoNeighbors.end(), (int)i) != demoNeighbors.end()) {
                    // give them a green outline by drawing a slightly larger dark circle behind
                    drawPoint(window, p, sf::Color(40, 120, 40), radius + 3.f);
                }
            }

            drawPoint(window, p, col, radius);

            // Draw label only for non-demo mode or for the selected demo point
            if (!demoLoaded) {
                drawPointLabel(window, p, fontPtr);
            } else if ((int)i == selectedIndex && fontPtr) {
                std::ostringstream ss;
                ss << "Idx:" << i << " Age:" << (int)puntosAge[i] << " WBC:" << (int)p.x << " BP:" << (int)p.y;
                sf::Text info(*fontPtr, ss.str());
                info.setCharacterSize(14);
                info.setFillColor(sf::Color::White);
                sf::Vector2f pos = mapToPlane(p);
                info.setPosition({pos.x + 10.f, pos.y - 18.f});
                window.draw(info);
            }
        }
        
        // Dibujar rectángulo de búsqueda por rango
        if (rangeState.modoActivo || animState.type == AnimationType::RANGE_SEARCH) {
            if (rangeState.dibujando) {
                // Dibujar rectángulo mientras se arrastra
                sf::Vector2f topLeft(
                    std::min(rangeState.puntoInicio.x, rangeState.puntoFin.x),
                    std::min(rangeState.puntoInicio.y, rangeState.puntoFin.y)
                );
                sf::Vector2f size(
                    std::abs(rangeState.puntoFin.x - rangeState.puntoInicio.x),
                    std::abs(rangeState.puntoFin.y - rangeState.puntoInicio.y)
                );
                sf::RectangleShape rect(size);
                rect.setPosition(topLeft);
                rect.setFillColor(sf::Color(100, 150, 255, 50)); // Azul translúcido
                rect.setOutlineThickness(2.f);
                rect.setOutlineColor(sf::Color(100, 150, 255, 200));
                window.draw(rect);
            } else if (rangeState.tieneResultado || animState.type == AnimationType::RANGE_SEARCH) {
                // Dibujar rectángulo final con resultados
                sf::Vector2f topLeft(
                    std::min(rangeState.puntoInicio.x, rangeState.puntoFin.x),
                    std::min(rangeState.puntoInicio.y, rangeState.puntoFin.y)
                );
                sf::Vector2f size(
                    std::abs(rangeState.puntoFin.x - rangeState.puntoInicio.x),
                    std::abs(rangeState.puntoFin.y - rangeState.puntoInicio.y)
                );
                sf::RectangleShape rect(size);
                rect.setPosition(topLeft);
                rect.setFillColor(sf::Color(100, 255, 100, 30)); // Verde translúcido
                rect.setOutlineThickness(2.f);
                rect.setOutlineColor(sf::Color(100, 255, 100, 200));
                window.draw(rect);
                
                // Resaltar puntos encontrados
                for (const auto& p : rangeState.puntosEncontrados) {
                    sf::Vector2f pos = mapToPlane(p);
                    float r = 8.f;
                    sf::CircleShape c(r);
                    c.setOrigin(sf::Vector2f(r, r));
                    c.setPosition(pos);
                    c.setFillColor(sf::Color(100, 255, 100)); // Verde brillante
                    c.setOutlineThickness(2.f);
                    c.setOutlineColor(sf::Color::White);
                    window.draw(c);
                }
                
                // Mostrar contador de resultados
                if (fontPtr) {
                    std::string lab = "Encontrados: " + std::to_string(rangeState.puntosEncontrados.size());
                    sf::Text t(*fontPtr, toUtf8(lab));
                    t.setCharacterSize(14);
                    t.setFillColor(sf::Color::White);
                    t.setPosition(sf::Vector2f(PLANE_ORIGIN_X + 10.f, PLANE_ORIGIN_Y + PLANE_HEIGHT - 30.f));
                    window.draw(t);
                }
            }
        }
        
        // Dibujar resultados k-NN
        if (knnState.tieneResultado || animState.type == AnimationType::KNN) {
            // Dibujar punto objetivo (magenta)
            sf::Vector2f targetPos = mapToPlane(knnState.puntoObjetivo);
            float r = 7.f;
            sf::CircleShape targetCircle(r);
            targetCircle.setOrigin(sf::Vector2f(r, r));
            targetCircle.setPosition(targetPos);
            targetCircle.setFillColor(sf::Color(255, 100, 255)); // Magenta
            targetCircle.setOutlineThickness(2.f);
            targetCircle.setOutlineColor(sf::Color::White);
            window.draw(targetCircle);
            
            // Dibujar vecinos encontrados (violeta)
            for (const auto& p : knnState.puntosEncontrados) {
                sf::Vector2f pos = mapToPlane(p);
                float nr = 8.f;
                sf::CircleShape c(nr);
                c.setOrigin(sf::Vector2f(nr, nr));
                c.setPosition(pos);
                c.setFillColor(sf::Color(200, 80, 200)); // Violeta
                c.setOutlineThickness(2.f);
                c.setOutlineColor(sf::Color::White);
                window.draw(c);
            }
        }
        
        // Resaltar punto objetivo durante animación (INSERT, SEARCH, DELETE)
        if ((animState.type == AnimationType::INSERT || 
             animState.type == AnimationType::SEARCH || 
             animState.type == AnimationType::DELETE) && 
            animState.currentStepIndex >= 0 && animState.currentStepIndex < (int)animState.steps.size()) {
            const auto& step = animState.steps[animState.currentStepIndex];
            sf::Vector2f targetPos = mapToPlane(step.targetPoint);
            
            float r = 7.f;
            sf::CircleShape targetCircle(r);
            targetCircle.setOrigin(sf::Vector2f(r, r));
            targetCircle.setPosition(targetPos);
            
            // Color según tipo de animación
            if (animState.type == AnimationType::DELETE) {
                targetCircle.setFillColor(sf::Color(255, 50, 50)); // Rojo para delete
            } else {
                targetCircle.setFillColor(sf::Color(255, 100, 255)); // Magenta para insert/search
            }
            
            targetCircle.setOutlineThickness(2.f);
            targetCircle.setOutlineColor(sf::Color::White);
            window.draw(targetCircle);
        }

        // draw nearest result if exists (y no hay animación activa)
        if (hasNearest && animState.type == AnimationType::NONE) {
            // highlight with a larger yellow circle and label
            sf::Vector2f np = mapToPlane(nearestPoint);
            float r = 9.f;
            sf::CircleShape c(r);
            c.setOrigin(sf::Vector2f(r,r));
            c.setPosition(np);
            c.setFillColor(sf::Color::Yellow);
            window.draw(c);
            if (fontPtr) {
                std::string lab = "Más cercano: (" + std::to_string((int)nearestPoint.x) + ", " + std::to_string((int)nearestPoint.y) + ")";
                sf::Text t(*fontPtr, toUtf8(lab));
                t.setCharacterSize(13);
                t.setFillColor(sf::Color::Black);
                t.setPosition(sf::Vector2f(np.x + 12.f, np.y - 6.f));
                window.draw(t);
            }
        }

        // draw tree (right panel) con highlight si hay animación
        KDNode* highlightNode = nullptr;
        bool isPulse = false;
        if (animState.type != AnimationType::NONE && animState.currentStepIndex >= 0 && 
            animState.currentStepIndex < (int)animState.steps.size()) {
            highlightNode = animState.steps[animState.currentStepIndex].currentNode;
            isPulse = true;
        }
        drawTree(window, tree.getRoot(), fontPtr, highlightNode, isPulse);
        
        // Dibujar panel de información de animación
        drawAnimationInfo(window, animState, fontPtr);

        window.display();
    }
}
//eesto es del sfml