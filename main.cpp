#include "MicroForth.h"
#include "LcdSimulator.h"
#include <iostream>
#include <fstream>
#include <string>

LcdSimulator lcd;
MicroForth vm;

char virtual_eeprom[1024] = ": DEMO 10 20 + . BEEP ; DEMO"; 
char load_buffer[1024] = {0};

void nativeBeep() { std::cout << "[\aPIEZO SPEAKER BEEP!]" << std::endl; }

void nativeLoad() { 
    strncpy(load_buffer, virtual_eeprom, sizeof(load_buffer) - 1); 
    lcd.print("EEPROM LOADED."); 
    lcd.write('\n'); 
}

void nativeRun() {
    if (strlen(load_buffer) == 0) { 
        lcd.print("ERR: EMPTY ROM"); 
        lcd.write('\n'); 
        return; 
    }
    if (vm.interpret(load_buffer) != MicroForth::OK) { 
        lcd.print("ERR IN EXEC"); 
        lcd.write('\n'); 
    }
}

void vmToLcdPrint(int32_t val) {
    char buf[12];
    snprintf(buf, sizeof(buf), "%d ", val);
    lcd.print(buf);
}

// Map exact error codes to human readable LCD responses
void printStatus(MicroForth::Status status) {
    if (status == MicroForth::OK) lcd.print("OK");
    else if (status == MicroForth::STACK_UNDERFLOW) lcd.print("ERR: STACK <"); // Tried to pop from empty stack
    else if (status == MicroForth::STACK_OVERFLOW) lcd.print("ERR: STACK >");  // Too many numbers
    else if (status == MicroForth::UNKNOWN_WORD) lcd.print("ERR: WORD?");      // Token not found
    else if (status == MicroForth::COMPILE_ERROR) lcd.print("ERR: COMPIL");    // Issue inside : ... ;
    else lcd.print("ERR?");
}

int main(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-rom") == 0 && i + 1 < argc) {
            const char* filename = argv[i + 1];
            std::ifstream file(filename, std::ios::binary | std::ios::ate);
            if (file.is_open()) {
                std::streamsize size = file.tellg();
                file.seekg(0, std::ios::beg);
                if (size < (std::streamsize)sizeof(virtual_eeprom)) {
                    if (file.read(virtual_eeprom, size)) {
                        virtual_eeprom[size] = '\0';
                        std::cout << "Attached ROM File: " << filename << "\n";
                    }
                }
                file.close();
            }
        }
    }

    if (!lcd.init()) return -1;

    vm.setDotHook(vmToLcdPrint);
    vm.registerHook(0, "BEEP", nativeBeep);
    vm.registerHook(1, "LOAD", nativeLoad);
    vm.registerHook(2, "RUN", nativeRun);

    lcd.print("FORTH OS V1.8");
    lcd.write('\n');
    lcd.print("> ");

    std::string current_line = "";

    while (lcd.isOpen()) {
        lcd.update(16);

        while (lcd.available()) {
            char c = lcd.read();

            if (c == '\n') {
                lcd.write('\n'); 
                
                while (!current_line.empty() && (current_line.back() == ' ' || current_line.back() == '\r' || current_line.back() == '\t')) {
                    current_line.pop_back();
                }

                if (!current_line.empty()) {
                    MicroForth::Status status = vm.interpret(current_line.c_str());
                    
                    if (lcd.getCursorX() > 0) lcd.write('\n');
                    printStatus(status);
                    current_line.clear();
                } else {
                    lcd.print("OK"); 
                }
                
                lcd.write('\n');
                lcd.print("> ");
                
            } else if (c == '\b') {
                if (!current_line.empty()) {
                    current_line.pop_back();
                    uint8_t cx = lcd.getCursorX();
                    uint8_t cy = lcd.getCursorY();
                    if (cx > 2) { 
                        lcd.setCursor(cx - 1, cy);
                        lcd.write(' ');
                        lcd.setCursor(cx - 1, cy);
                    }
                }
            } else {
                if (current_line.length() < 17) {
                    current_line += c;
                    lcd.write(c);
                }
            }
        }
    }
    return 0;
}