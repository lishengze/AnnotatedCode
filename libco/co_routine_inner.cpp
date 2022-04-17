#include "co_routine_inner.h"

int stCoRoutine_t::global_id = 0;

string stStackMem_t::str() { 
    string result = "occupy_co: " + std::to_string(occupy_co->id) 
                    + ", stack_size: " + std::to_string(stack_size)
                    + ", stack_bp: " + stack_bp
                    + ", stack_buffer: " + stack_buffer;
    return result;
}