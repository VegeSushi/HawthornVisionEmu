#ifndef LCD_SIMULATOR_H
#define LCD_SIMULATOR_H

#include <stdint.h>

class LcdSimulator {
public:
    LcdSimulator();
    ~LcdSimulator();

    bool init();
    void update(unsigned int delayMs = 16); 
    void close();
    bool isOpen() const { return is_open; }

    // LCD API
    void clear();
    void setCursor(uint8_t x, uint8_t y);
    void write(char c);
    void print(const char* str);
    
    // Getters for cursor (needed to manage interactive typing)
    uint8_t getCursorX() const { return cursor_x; }
    uint8_t getCursorY() const { return cursor_y; }

    // Keyboard Emulation API
    bool available(); // Returns true if a key is waiting in the PS/2 buffer
    char read();      // Pops a char out of the buffer

private:
    bool is_open;
    uint8_t cursor_x;
    uint8_t cursor_y;
    char grid[4][20];

    // Simple circular keyboard buffer (Simulating physical PS/2 matrix)
    char kbd_buffer[32];
    uint8_t kbd_head;
    uint8_t kbd_tail;
    void pushKey(char c);

#if defined(__linux__) || defined(_WIN32)
    struct SDL_Window* window;
    struct SDL_Renderer* renderer;
    void renderMatrix();
    void drawCharacter(int startX, int startY, char c);
#endif
};

#endif // LCD_SIMULATOR_H