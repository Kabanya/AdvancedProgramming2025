#pragma once

struct Health {
    int current;
    int max;

    Health(int maxHealth = 100) : current(maxHealth), max(maxHealth) {}

    void change(int delta) {
        current += delta;
        if (current > max) current = max;
        if (current < 0) current = 0;
    }
};