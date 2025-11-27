#include <iostream>
#include <SFML/Graphics.hpp>
#include "KDTree.h"
#include <vector>

using namespace std;


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


sf::Vector2f mapToPlane(const Punto2D& p) {
    float normX = p.x / MAX_COORD; // 0..1
    float normY = p.y / MAX_COORD; // 0..1

    float screenX = PLANE_ORIGIN_X + normX * PLANE_WIDTH;
    // SFML tiene el origen (0,0) arriba a la izquierda, así que invertimos Y
    float screenY = PLANE_ORIGIN_Y + (1.f - normY) * PLANE_HEIGHT;

    return {screenX, screenY};
}

//----------------------------------------------------------------------------
void printPreorder(KDNode* nodo) {
    if (!nodo) return;
    cout << "Nivel " << nodo->nivel
         << " -> (" << nodo->punto.x << ", " << nodo->punto.y << ")\n";
    printPreorder(nodo->izquierdo);
    printPreorder(nodo->derecho);
}

void drawPoint(sf::RenderWindow& window, const Punto2D& p, sf::Color color = sf::Color::Red) {
    sf::Vector2f pos = mapToPlane(p);

    float radius = 5.f;
    sf::CircleShape circle(radius);
    circle.setFillColor(color);
    circle.setOrigin(sf::Vector2f(radius, radius)); // para que el centro del círculo sea el punto
    circle.setPosition(pos);

    window.draw(circle);
}


// Dibuja fondo, grilla y ejes del plano izquierdo (SFML 3)
void drawAxesAndGrid(sf::RenderWindow& window) {
    // Fondo del plano
    sf::RectangleShape planeBg({PLANE_WIDTH, PLANE_HEIGHT});
    planeBg.setPosition(sf::Vector2f(PLANE_ORIGIN_X, PLANE_ORIGIN_Y));
    planeBg.setFillColor(sf::Color(30, 30, 30));
    window.draw(planeBg);

    // ====== GRILLA ======
    const float step = 16.f; // cada 16 unidades en coordenadas reales

    // Líneas verticales
    for (float x = 0; x <= MAX_COORD; x += step) {
        float normX = x / MAX_COORD;
        float screenX = PLANE_ORIGIN_X + normX * PLANE_WIDTH;

        sf::VertexArray line(sf::PrimitiveType::Lines, 2);
        line[0].position = sf::Vector2f(screenX, PLANE_ORIGIN_Y);
        line[1].position = sf::Vector2f(screenX, PLANE_ORIGIN_Y + PLANE_HEIGHT);
        line[0].color = line[1].color = sf::Color(60, 60, 60);

        window.draw(line);
    }

    // Líneas horizontales
    for (float y = 0; y <= MAX_COORD; y += step) {
        float normY = y / MAX_COORD;
        float screenY = PLANE_ORIGIN_Y + (1.f - normY) * PLANE_HEIGHT;

        sf::VertexArray line(sf::PrimitiveType::Lines, 2);
        line[0].position = sf::Vector2f(PLANE_ORIGIN_X, screenY);
        line[1].position = sf::Vector2f(PLANE_ORIGIN_X + PLANE_WIDTH, screenY);
        line[0].color = line[1].color = sf::Color(60, 60, 60);

        window.draw(line);
    }

    // ====== EJES PRINCIPALES ======

    // Eje X (abajo)
    sf::VertexArray ejeX(sf::PrimitiveType::Lines, 2);
    ejeX[0].position = sf::Vector2f(PLANE_ORIGIN_X, PLANE_ORIGIN_Y + PLANE_HEIGHT);
    ejeX[1].position = sf::Vector2f(PLANE_ORIGIN_X + PLANE_WIDTH, PLANE_ORIGIN_Y + PLANE_HEIGHT);
    ejeX[0].color = ejeX[1].color = sf::Color::White;
    window.draw(ejeX);

    // Eje Y (izquierda)
    sf::VertexArray ejeY(sf::PrimitiveType::Lines, 2);
    ejeY[0].position = sf::Vector2f(PLANE_ORIGIN_X, PLANE_ORIGIN_Y);
    ejeY[1].position = sf::Vector2f(PLANE_ORIGIN_X, PLANE_ORIGIN_Y + PLANE_HEIGHT);
    ejeY[0].color = ejeY[1].color = sf::Color::White;
    window.draw(ejeY);
}


int main() {
    sf::RenderWindow window(sf::VideoMode({WINDOW_WIDTH, WINDOW_HEIGHT}), "KD-Tree Visualizer");

    // 1. Construimos el KDTree con algunos puntos
    KDTree tree;
    vector<Punto2D> puntos = {
        {40, 45},
        {15, 70},
        {70, 10},
        {69, 50},
        {66, 85},
        {85, 90}
    };

    for (const auto& p : puntos) {
        tree.insert(p);
    }

    while (window.isOpen()) {
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>())
                window.close();
            else if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                // Cerrar con Escape
                if (key->scancode == sf::Keyboard::Scancode::Escape)
                    window.close();
            }
        }

        window.clear(sf::Color::Black);

        // DIBUJAR PLANO + GRILLA + EJES
        drawAxesAndGrid(window);

        // Dibujar puntos del KDTree
        for (const auto& p : puntos) {
            drawPoint(window, p, sf::Color::Red);
        }

        window.display();
    }

    return 0;
}
