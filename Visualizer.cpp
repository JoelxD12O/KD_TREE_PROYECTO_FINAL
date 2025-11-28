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
    SEARCH
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
};

//---------------------- Configuración de SFML ---------------------_
const int WINDOW_WIDTH  = 1000;
const int WINDOW_HEIGHT = 600;

// Panel izquierdo para el plano cartesiano
const float PLANE_WIDTH  = 600.f;
const float PLANE_HEIGHT = 600.f;
const float PLANE_ORIGIN_X = 0.f;
const float PLANE_ORIGIN_Y = 0.f;

// Rango máximo de coordenadas reales (como el ejemplo del libro)
const float MAX_COORD = 128.f;

// Panel derecho: árbol
const float TREE_ORIGIN_X   = PLANE_WIDTH;           // empieza donde acaba el plano
const float TREE_PADDING_X  = 20.f;
const float TREE_PADDING_Y  = 40.f;
const float TREE_WIDTH      = WINDOW_WIDTH - TREE_ORIGIN_X - TREE_PADDING_X;
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
                                 std::to_string((int)current->punto.x) + " → IR IZQUIERDA";
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
                                 std::to_string((int)current->punto.x) + " → IR DERECHA";
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
                                 std::to_string((int)current->punto.y) + " → IR IZQUIERDA";
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
                                 std::to_string((int)current->punto.y) + " → IR DERECHA";
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
                                   " INTERSECTA plano → Explorar otra rama";
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
                                   " NO intersecta plano → ¡PODAR! (saltar rama)";
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

static void drawPoint(sf::RenderWindow& window, const Punto2D& p, sf::Color color = sf::Color::Red) {
    sf::Vector2f pos = mapToPlane(p);

    float radius = 5.f;
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

static void drawAxesAndGrid(sf::RenderWindow& window) {
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
    sf::RectangleShape panel({WINDOW_WIDTH, 80.f});
    panel.setPosition({0.f, panelY});
    panel.setFillColor(sf::Color(20, 20, 20, 240));
    window.draw(panel);
    
    // Título del algoritmo
    sf::String title = (anim.type == AnimationType::INSERT) ? toUtf8("ANIMACIÓN: INSERCIÓN") : toUtf8("ANIMACIÓN: BÚSQUEDA NEAREST NEIGHBOR");
    sf::Text titleText(*font, title, 16);
    titleText.setFillColor(sf::Color::Yellow);
    titleText.setPosition({20.f, panelY + 10.f});
    titleText.setStyle(sf::Text::Bold);
    window.draw(titleText);
    
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
    
    // Controles
    std::string controls = "[ESPACIO] Pausar  [→←] Pasos  [↑↓] Velocidad  [ESC] Cancelar";
    sf::Text controlsText(*font, controls, 11);
    controlsText.setFillColor(sf::Color(150, 150, 150));
    controlsText.setPosition({300.f, panelY + 60.f});
    window.draw(controlsText);
    
    // Mostrar velocidad actual
    std::string speedInfo = "Velocidad: " + std::to_string(anim.stepDuration).substr(0, 3) + "s";
    sf::Text speedText(*font, speedInfo, 11);
    speedText.setFillColor(sf::Color(100, 255, 100));
    speedText.setPosition({WINDOW_WIDTH - 150.f, panelY + 60.f});
    window.draw(speedText);
}

void runVisualizer(KDTree& tree, std::vector<Punto2D>& puntos) {
    // Configurar locale para soporte UTF-8
    std::locale::global(std::locale("es_ES.UTF-8"));
    std::cout.imbue(std::locale());
    
    sf::RenderWindow window(sf::VideoMode({WINDOW_WIDTH, WINDOW_HEIGHT}), toUtf8("Visualizador KD-Tree"));

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

    std::unique_ptr<sf::Text> textX, textY, textBtn, textSearch, labelX, labelY;
    if (fontPtr) {
        textX = std::make_unique<sf::Text>(*fontPtr, "", 14);
        textY = std::make_unique<sf::Text>(*fontPtr, "", 14);
        textBtn = std::make_unique<sf::Text>(*fontPtr, toUtf8("Añadir"), 14);
        textSearch = std::make_unique<sf::Text>(*fontPtr, toUtf8("Buscar"), 14);
        labelX = std::make_unique<sf::Text>(*fontPtr, "X:", 12);
        labelY = std::make_unique<sf::Text>(*fontPtr, "Y:", 12);
        textX->setFillColor(sf::Color::White); textY->setFillColor(sf::Color::White); textBtn->setFillColor(sf::Color::Black);
        textSearch->setFillColor(sf::Color::Black);
        labelX->setFillColor(sf::Color::White); labelY->setFillColor(sf::Color::White);
    }

    // Nearest search result
    bool hasNearest = false;
    Punto2D nearestPoint{0.f,0.f};
    
    // Estado de animación
    AnimationState animState;

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
                if (key->scancode == sf::Keyboard::Scancode::Backspace) {
                    if (activeX && !inputX.empty()) inputX.pop_back();
                    else if (activeY && !inputY.empty()) inputY.pop_back();
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
                }
            }

            if (const auto* mouse = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (mouse->button == sf::Mouse::Button::Left) {
                    sf::Vector2f mpos((float)mouse->position.x, (float)mouse->position.y);
                    if (boxX.getGlobalBounds().contains(mpos)) { activeX=true; activeY=false; }
                    else if (boxY.getGlobalBounds().contains(mpos)) { activeX=false; activeY=true; }
                    else if (button.getGlobalBounds().contains(mpos)) {
                        try {
                            if (!inputX.empty() && !inputY.empty()) {
                                float rx = std::stof(inputX); float ry = std::stof(inputY);
                                Punto2D p{rx, ry};
                                
                                // Iniciar animación de inserción si hay árbol
                                if (tree.getRoot() != nullptr) {
                                    generateInsertAnimation(tree.getRoot(), p, animState);
                                    animState.currentStepIndex = 0;
                                    animState.stepClock.restart();
                                    animState.paused = false;
                                }
                                
                                tree.insert(p); 
                                puntos.push_back(p); 
                                inputX.clear(); inputY.clear();
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
                                
                                // Calcular resultado
                                nearestPoint = tree.nearest(target);
                                animState.foundNearest = nearestPoint;
                                animState.hasResult = true;
                            }
                        } catch(...) { }
                        activeX = activeY = false;
                    } else {
                        // click in plane -> insert point by coordinates
                        int mx = mouse->position.x; int my = mouse->position.y;
                        if (mx >= (int)PLANE_ORIGIN_X && mx <= (int)(PLANE_ORIGIN_X + PLANE_WIDTH)
                            && my >= (int)PLANE_ORIGIN_Y && my <= (int)(PLANE_ORIGIN_Y + PLANE_HEIGHT)) {
                            float realX = (float)(mx - PLANE_ORIGIN_X) / PLANE_WIDTH * MAX_COORD;
                            float realY = (1.f - (float)(my - PLANE_ORIGIN_Y) / PLANE_HEIGHT) * MAX_COORD;
                            Punto2D p{realX, realY}; tree.insert(p); puntos.push_back(p);
                        }
                        activeX = activeY = false;
                    }
                }
            }
        }

        window.clear(sf::Color::Black);

        drawAxesAndGrid(window);
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
            // center labels
            sf::FloatRect lbx = labelX->getLocalBounds(); sf::FloatRect lby = labelY->getLocalBounds();
            labelX->setPosition(sf::Vector2f(boxX.getPosition().x + (INPUT_W - lbx.size.x)/2.f, boxX.getPosition().y - 8.f));
            labelY->setPosition(sf::Vector2f(boxY.getPosition().x + (INPUT_W - lby.size.x)/2.f, boxY.getPosition().y - 8.f));
            window.draw(boxX); window.draw(boxY); window.draw(button); window.draw(buttonSearch);
            window.draw(*labelX); window.draw(*labelY); window.draw(*textX); window.draw(*textY); window.draw(*textBtn); window.draw(*textSearch);
        } else {
            window.draw(boxX); window.draw(boxY); window.draw(button); window.draw(buttonSearch);
        }

        for (const auto& p : puntos) { drawPoint(window, p, sf::Color::Red); drawPointLabel(window, p, fontPtr); }
        
        // Resaltar punto objetivo durante animación
        if (animState.type != AnimationType::NONE && animState.currentStepIndex >= 0 && 
            animState.currentStepIndex < (int)animState.steps.size()) {
            const auto& step = animState.steps[animState.currentStepIndex];
            sf::Vector2f targetPos = mapToPlane(step.targetPoint);
            
            float r = 7.f;
            sf::CircleShape targetCircle(r);
            targetCircle.setOrigin(sf::Vector2f(r, r));
            targetCircle.setPosition(targetPos);
            targetCircle.setFillColor(sf::Color(255, 100, 255)); // Magenta para target
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