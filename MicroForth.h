#ifndef MICRO_FORTH_H
#define MICRO_FORTH_H

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

class MicroForth {
public:
    enum Status { OK = 0, STACK_OVERFLOW, STACK_UNDERFLOW, COMPILE_ERROR, UNKNOWN_WORD };
    enum WordType { BUILTIN, USER_DEFINED };

    typedef void (*DotHook)(int32_t val);
    typedef void (*ExtHook)();

#if defined(__linux__) || defined(_WIN32)
    static const size_t DICT_SIZE = 8192;
    static const size_t STACK_SIZE = 256;
    static const size_t MAX_WORDS = 128; 
#else
    static const size_t DICT_SIZE = 384;
    static const size_t STACK_SIZE = 16;
    static const size_t MAX_WORDS = 48;
#endif

    struct Word {
        char name[8];
        WordType type;
        union {
            void (*xt)();
            uint16_t dict_ptr;
        };
    };

private:
    int32_t data_stack[STACK_SIZE];
    int16_t sp = -1; 
    
    // Return Stack required for DO...LOOP structures
    int32_t return_stack[STACK_SIZE];
    int16_t rsp = -1;

    // Compile Stack required to resolve nested branch addresses
    uint16_t compile_stack[16];
    int8_t csp = -1;

    Word words[MAX_WORDS];
    uint8_t word_count = 0;
    
    uint8_t dict[DICT_SIZE];
    uint16_t dp = 0;
    
    bool compile_mode = false;
    Status last_error = OK;

    DotHook dot_hook = nullptr;
    ExtHook ext_hooks[8] = {nullptr};
    static MicroForth* instance;

    void push(int32_t val) {
        if (sp < (int16_t)(STACK_SIZE - 1)) data_stack[++sp] = val;
        else last_error = STACK_OVERFLOW;
    }

    int32_t pop() {
        if (sp >= 0) return data_stack[sp--];
        last_error = STACK_UNDERFLOW;
        return 0;
    }

    void rpush(int32_t val) {
        if (rsp < (int16_t)(STACK_SIZE - 1)) return_stack[++rsp] = val;
        else last_error = STACK_OVERFLOW;
    }

    int32_t rpop() {
        if (rsp >= 0) return return_stack[rsp--];
        last_error = STACK_UNDERFLOW;
        return 0;
    }

    void cpush(uint16_t val) {
        if (csp < 15) compile_stack[++csp] = val;
        else last_error = COMPILE_ERROR;
    }

    uint16_t cpop() {
        if (csp >= 0) return compile_stack[csp--];
        last_error = COMPILE_ERROR;
        return 0;
    }

    void add_builtin(const char* name, void (*xt)()) {
        if (word_count >= MAX_WORDS) return;
        memset(words[word_count].name, 0, sizeof(words[word_count].name));
        strncpy(words[word_count].name, name, 7);
        words[word_count].type = BUILTIN;
        words[word_count].xt = xt;
        word_count++;
    }

    // Core Math & Stack
    static void w_add()  { instance->push(instance->pop() + instance->pop()); }
    static void w_sub()  { int32_t b = instance->pop(); instance->push(instance->pop() - b); }
    static void w_mul()  { instance->push(instance->pop() * instance->pop()); }
    static void w_div()  { int32_t b = instance->pop(); instance->push(instance->pop() / (b ? b : 1)); }
    static void w_mod()  { int32_t b = instance->pop(); instance->push(instance->pop() % (b ? b : 1)); }
    static void w_dup()  { if(instance->sp >= 0) instance->push(instance->data_stack[instance->sp]); else instance->last_error = STACK_UNDERFLOW; }
    static void w_drop() { instance->pop(); }
    static void w_swap() { int32_t a = instance->pop(); int32_t b = instance->pop(); instance->push(a); instance->push(b); }
    static void w_over() { int32_t b = instance->pop(); int32_t a = instance->pop(); instance->push(a); instance->push(b); instance->push(a); }
    static void w_rot()  { int32_t c = instance->pop(); int32_t b = instance->pop(); int32_t a = instance->pop(); instance->push(b); instance->push(c); instance->push(a); }
    static void w_dot()  { int32_t val = instance->pop(); if (instance->last_error == OK && instance->dot_hook) instance->dot_hook(val); }

    // Comparisons & Logic (Standard Forth true is -1)
    static void w_eq()     { int32_t b = instance->pop(); int32_t a = instance->pop(); instance->push(a == b ? -1 : 0); }
    static void w_lt()     { int32_t b = instance->pop(); int32_t a = instance->pop(); instance->push(a < b ? -1 : 0); }
    static void w_gt()     { int32_t b = instance->pop(); int32_t a = instance->pop(); instance->push(a > b ? -1 : 0); }
    static void w_and()    { instance->push(instance->pop() & instance->pop()); }
    static void w_or()     { instance->push(instance->pop() | instance->pop()); }
    static void w_invert() { instance->push(~instance->pop()); }
    
    // Loop Variable
    static void w_i()      { if(instance->rsp >= 0) instance->push(instance->return_stack[instance->rsp]); else instance->last_error = STACK_UNDERFLOW; }


    void execute_word(uint8_t idx) {
        if (last_error != OK) return;
        if (words[idx].type == BUILTIN) {
            words[idx].xt();
        } else {
            uint16_t ptr = words[idx].dict_ptr;
            while (ptr < DICT_SIZE && dict[ptr] != 0xFF && last_error == OK) {
                uint8_t opcode = dict[ptr];
                
                if (opcode == 0xFE) { // LITERAL
                    ptr++; 
                    int32_t val;
                    memcpy(&val, &dict[ptr], sizeof(int32_t)); 
                    push(val);
                    ptr += sizeof(int32_t);
                } 
                else if (opcode == 0xFD) { // JUMP UNCONDITIONAL
                    ptr++;
                    memcpy(&ptr, &dict[ptr], sizeof(uint16_t));
                } 
                else if (opcode == 0xFC) { // JUMP IF ZERO
                    ptr++;
                    int32_t cond = pop();
                    if (cond == 0) memcpy(&ptr, &dict[ptr], sizeof(uint16_t));
                    else ptr += sizeof(uint16_t);
                } 
                else if (opcode == 0xFB) { // DO_SETUP
                    ptr++;
                    int32_t start = pop();
                    int32_t limit = pop();
                    rpush(limit);
                    rpush(start);
                } 
                else if (opcode == 0xFA) { // LOOP_STEP
                    ptr++;
                    int32_t index = rpop() + 1;
                    int32_t limit = rpop();
                    if (index < limit) {
                        rpush(limit);
                        rpush(index);
                        memcpy(&ptr, &dict[ptr], sizeof(uint16_t));
                    } else {
                        ptr += sizeof(uint16_t);
                    }
                } 
                else { // STANDARD WORD
                    execute_word(opcode);
                    ptr++;
                }
            }
        }
    }

public:
    MicroForth() {
        instance = this;
        add_builtin("+", w_add); add_builtin("-", w_sub);
        add_builtin("*", w_mul); add_builtin("/", w_div);
        add_builtin("MOD", w_mod); add_builtin(".", w_dot);
        
        add_builtin("DUP", w_dup); add_builtin("DROP", w_drop);
        add_builtin("SWAP", w_swap); add_builtin("OVER", w_over);
        add_builtin("ROT", w_rot);
        
        add_builtin("=", w_eq); add_builtin("<", w_lt); add_builtin(">", w_gt);
        add_builtin("AND", w_and); add_builtin("OR", w_or); add_builtin("INVERT", w_invert);
        add_builtin("I", w_i);
    }

    void setDotHook(DotHook hook) { dot_hook = hook; }
    
    bool registerHook(uint8_t index, const char* name, ExtHook hook) {
        if (index >= 8 || word_count >= MAX_WORDS) return false;
        ext_hooks[index] = hook;
        
        #define HOOK_SLOT(N) static void w_hook_##N() { if(instance->ext_hooks[N]) instance->ext_hooks[N](); }
        struct Trampolines {
            HOOK_SLOT(0) HOOK_SLOT(1) HOOK_SLOT(2) HOOK_SLOT(3)
            HOOK_SLOT(4) HOOK_SLOT(5) HOOK_SLOT(6) HOOK_SLOT(7)
        };
        void (*slots[])() = { Trampolines::w_hook_0, Trampolines::w_hook_1, Trampolines::w_hook_2, Trampolines::w_hook_3,
                              Trampolines::w_hook_4, Trampolines::w_hook_5, Trampolines::w_hook_6, Trampolines::w_hook_7 };
        add_builtin(name, slots[index]);
        return true;
    }

    Status interpret(const char* input) {
        instance = this; 
        last_error = OK; 

        if (!input || strlen(input) == 0) return OK;

        size_t len = strlen(input);
        size_t idx = 0;
        char token[16];

        while (idx < len && last_error == OK) {
            while (idx < len && (input[idx] == ' ' || input[idx] == '\t' || input[idx] == '\r' || input[idx] == '\n')) idx++;
            if (idx >= len) break;

            size_t t_len = 0;
            while (idx < len && input[idx] != ' ' && input[idx] != '\t' && input[idx] != '\r' && input[idx] != '\n') {
                if (t_len < sizeof(token) - 1) {
                    char c = input[idx];
                    if (c >= 'a' && c <= 'z') c -= 32;
                    token[t_len++] = c;
                }
                idx++;
            }
            token[t_len] = '\0';
            if (t_len == 0) continue;

            if (strcmp(token, ":") == 0) {
                compile_mode = true;
                csp = -1; // Reset compile stack
                
                while (idx < len && (input[idx] == ' ' || input[idx] == '\t' || input[idx] == '\r' || input[idx] == '\n')) idx++;
                
                t_len = 0;
                while (idx < len && input[idx] != ' ' && input[idx] != '\t' && input[idx] != '\r' && input[idx] != '\n') {
                    if (t_len < 7) {
                        char c = input[idx];
                        if (c >= 'a' && c <= 'z') c -= 32;
                        token[t_len++] = c;
                    }
                    idx++;
                }
                token[t_len] = '\0';

                if (t_len > 0 && word_count < MAX_WORDS) {
                    memset(words[word_count].name, 0, sizeof(words[word_count].name));
                    strncpy(words[word_count].name, token, 7);
                    words[word_count].type = USER_DEFINED;
                    words[word_count].dict_ptr = dp;
                } else {
                    last_error = COMPILE_ERROR;
                }
                continue;
            }

            if (compile_mode) {
                if (strcmp(token, ";") == 0) {
                    if (dp < DICT_SIZE) {
                        dict[dp++] = 0xFF; 
                        word_count++;
                    } else last_error = COMPILE_ERROR;
                    compile_mode = false;
                } 
                else if (strcmp(token, "IF") == 0) {
                    if (dp + 3 <= DICT_SIZE) { dict[dp++] = 0xFC; cpush(dp); dp += 2; }
                    else last_error = COMPILE_ERROR;
                }
                else if (strcmp(token, "THEN") == 0) {
                    uint16_t target_ptr = cpop();
                    if (last_error == OK) memcpy(&dict[target_ptr], &dp, sizeof(uint16_t));
                }
                else if (strcmp(token, "ELSE") == 0) {
                    if (dp + 3 <= DICT_SIZE) {
                        dict[dp++] = 0xFD; 
                        uint16_t jmp_target = dp; dp += 2;
                        uint16_t if_target = cpop();
                        if (last_error == OK) memcpy(&dict[if_target], &dp, sizeof(uint16_t));
                        cpush(jmp_target);
                    } else last_error = COMPILE_ERROR;
                }
                else if (strcmp(token, "BEGIN") == 0) {
                    cpush(dp);
                }
                else if (strcmp(token, "UNTIL") == 0) {
                    if (dp + 3 <= DICT_SIZE) {
                        dict[dp++] = 0xFC; 
                        uint16_t target = cpop();
                        if (last_error == OK) { memcpy(&dict[dp], &target, sizeof(uint16_t)); dp += 2; }
                    } else last_error = COMPILE_ERROR;
                }
                else if (strcmp(token, "DO") == 0) {
                    if (dp + 1 <= DICT_SIZE) { dict[dp++] = 0xFB; cpush(dp); }
                    else last_error = COMPILE_ERROR;
                }
                else if (strcmp(token, "LOOP") == 0) {
                    if (dp + 3 <= DICT_SIZE) {
                        dict[dp++] = 0xFA;
                        uint16_t target = cpop();
                        if (last_error == OK) { memcpy(&dict[dp], &target, sizeof(uint16_t)); dp += 2; }
                    } else last_error = COMPILE_ERROR;
                }
                else {
                    int8_t found = -1;
                    for (int8_t i = word_count - 1; i >= 0; i--) {
                        if (strcmp(words[i].name, token) == 0) { found = i; break; }
                    }
                    
                    if (found >= 0 && dp < DICT_SIZE) {
                        dict[dp++] = (uint8_t)found;
                    } else {
                        bool is_numeric = true;
                        size_t start = (token[0] == '-' && strlen(token) > 1) ? 1 : 0;
                        for (size_t i = start; i < strlen(token); i++) {
                            if (token[i] < '0' || token[i] > '9') { is_numeric = false; break; }
                        }

                        if (is_numeric && dp + 5 < DICT_SIZE) {
                            dict[dp++] = 0xFE; 
                            int32_t val = (int32_t)strtol(token, nullptr, 10);
                            memcpy(&dict[dp], &val, sizeof(int32_t)); 
                            dp += sizeof(int32_t);
                        } else {
                            last_error = COMPILE_ERROR;
                        }
                    }
                }
                continue;
            }

            int8_t found_idx = -1;
            for (int8_t i = word_count - 1; i >= 0; i--) {
                if (strcmp(words[i].name, token) == 0) { found_idx = i; break; }
            }

            if (found_idx >= 0) {
                execute_word(found_idx);
            } else {
                bool is_numeric = true;
                size_t start = (token[0] == '-' && strlen(token) > 1) ? 1 : 0;
                for (size_t i = start; i < strlen(token); i++) {
                    if (token[i] < '0' || token[i] > '9') { is_numeric = false; break; }
                }

                if (is_numeric) {
                    int32_t val = (int32_t)strtol(token, nullptr, 10);
                    push(val);
                } else {
                    last_error = UNKNOWN_WORD;
                }
            }
        }

        return last_error;
    }
};

#if defined(__linux__) || defined(_WIN32)
__attribute__((weak)) MicroForth* MicroForth::instance = nullptr;
#else
MicroForth* MicroForth::instance = nullptr;
#endif

#endif