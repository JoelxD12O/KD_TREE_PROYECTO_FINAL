#include "Visualizer.h"
#include <SFML/Graphics.hpp>
#include <iostream>

// Reuse KDTree types: Punto2D, KDNode, KDTree

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

// -------------------- Draw tree (panel derecho) --------------------
static void drawTreeRec(sf::RenderWindow& window,
                        KDNode* nodo,
                        float xMin, float xMax,
                        float yActual,
                        const sf::Font* font) {
    if (!nodo) return;

    float xCentro = xMin + (xMax - xMin) / 2.f;
    sf::Vector2f posNodo(xCentro, TREE_PADDING_Y + yActual);

    float yHijo = yActual + TREE_LEVEL_H;
    float xMid = xMin + (xMax - xMin) / 2.f;

    // hijo izquierdo
    if (nodo->izquierdo) {
        float xHijoIzq = xMin + (xMid - xMin) / 2.f;
        sf::Vector2f posHijoIzq(xHijoIzq, TREE_PADDING_Y + yHijo);

        sf::VertexArray linea(sf::PrimitiveType::Lines, 2);
        linea[0].position = posNodo;
        linea[1].position = posHijoIzq;
        linea[0].color = linea[1].color = sf::Color::White;
        window.draw(linea);

        drawTreeRec(window, nodo->izquierdo, xMin, xMid, yHijo, font);
    }

    // hijo derecho
    if (nodo->derecho) {
        float xHijoDer = xMid + (xMax - xMid) / 2.f;
        sf::Vector2f posHijoDer(xHijoDer, TREE_PADDING_Y + yHijo);

        sf::VertexArray linea(sf::PrimitiveType::Lines, 2);
        linea[0].position = posNodo;
        linea[1].position = posHijoDer;
        linea[0].color = linea[1].color = sf::Color::White;
        window.draw(linea);

        drawTreeRec(window, nodo->derecho, xMid, xMax, yHijo, font);
    }

    // dibujar nodo
    float radio = 14.f;
    sf::CircleShape circle(radio);
    circle.setFillColor(sf::Color(50, 120, 255));
    circle.setOrigin(sf::Vector2f(radio, radio));
    circle.setPosition(posNodo);
    window.draw(circle);

    // label opcional con coordenadas
    if (font) {
        std::string label = "(" + std::to_string((int)nodo->punto.x) + ", " + std::to_string((int)nodo->punto.y) + ")";
        sf::Text text(*font, label);
        text.setCharacterSize(12);
        text.setFillColor(sf::Color::White);
        // centrar el texto debajo del nodo usando la nueva API (position/size)
        sf::FloatRect tb = text.getLocalBounds();
        float textWidth = tb.size.x;
        text.setOrigin({textWidth/2.f, 0.f});
        text.setPosition({posNodo.x, posNodo.y + radio + 6.f});
        window.draw(text);
    }
}

void drawTree(sf::RenderWindow& window, KDNode* root, const sf::Font* font) {
    if (!root) return;
    float xIni = TREE_ORIGIN_X + TREE_PADDING_X;
    float xFin = WINDOW_WIDTH - TREE_PADDING_X;
    drawTreeRec(window, root, xIni, xFin, 0.f, font);
}

void runVisualizer(KDTree& tree, const std::vector<Punto2D>& puntos) {
    sf::RenderWindow window(sf::VideoMode({WINDOW_WIDTH, WINDOW_HEIGHT}), "KD-Tree Visualizer");

    // Cargar fuente para labels si está disponible
    sf::Font labelFont;
    const sf::Font* fontPtr = nullptr;
    if (labelFont.openFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf")) fontPtr = &labelFont;
    else if (labelFont.openFromFile("/usr/share/fonts/truetype/freefont/FreeSans.ttf")) fontPtr = &labelFont;
    else std::cerr << "Warning: no system font found; coordinate labels disabled\n";

    while (window.isOpen()) {
        while (auto event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) window.close();
            else if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                if (key->scancode == sf::Keyboard::Scancode::Escape) window.close();
            }
        }

        window.clear(sf::Color::Black);

        drawAxesAndGrid(window);
        drawKDLines(window, tree.getRoot());

        for (const auto& p : puntos) {
            drawPoint(window, p, sf::Color::Red);
            drawPointLabel(window, p, fontPtr);
        }

            // DIBUJAR ÁRBOL EN EL PANEL DERECHO
            drawTree(window, tree.getRoot(), fontPtr);

        window.display();
    }
}
