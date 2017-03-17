// cpu_debug.h

#ifndef CPU_DEBUG_H
#define CPU_DEBUG_H

#include <stdlib.h>
#include <inttypes.h>

// Register 0 must be the program counter
#define DBG_REG_PC 0

typedef struct {
  const char *cpu_name;                                               // Name/model of CPU.
  int      (*debug_enable)(int newvalue);                             // enable/disable debugging on this CPU, returns previous value.
  uint32_t (*memread)(uint32_t addr);                                 // CPU's usual memory read function for data.
  void     (*memwrite)(uint32_t addr, uint32_t value);                // CPU's usual memory write function.
  uint32_t (*disassemble)(uint32_t addr, char *buf, size_t bufsize);  // disassemble one line, returns next address
  const char **reg_names;                                             // NULL pointer terminated list of register names.
  uint32_t (*reg_get)(int which);                                     // Get a register - which is the index into the names above
  void     (*reg_set)(int which, uint32_t value);                     // Set a register.
  size_t   (*reg_print)(int which, char *buf, size_t bufsize);        // Print register value in CPU standard form.
  void     (*reg_parse)(int which, char *strval);                     // Parse a value into a register.
  uint32_t (*get_instr_addr)();                                       // Returns the base address of the currently executing instruction

} cpu_debug_t;

extern void debug_memread (cpu_debug_t *cpu, uint32_t addr, uint32_t value, uint8_t size);
extern void debug_memwrite(cpu_debug_t *cpu, uint32_t addr, uint32_t value, uint8_t size);
extern void debug_preexec (cpu_debug_t *cpu, uint32_t addr);

#endif
