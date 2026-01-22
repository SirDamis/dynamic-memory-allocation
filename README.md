# Simple Malloc

A minimal dynamic memory allocator implementation based on the **CS:APP** (Computer Systems: A Programmer's Perspective) textbook.

## Design

- **Implicit free list** with boundary tags (headers & footers)
- **First-fit** search strategy
- **Immediate coalescing** on free to reduce fragmentation
- **Block splitting** when remainder ≥ minimum block size

## Block Structure

```
+--------+---------------------------+--------+
| Header |        Payload            | Footer |
| 4 bytes|       (user data)         | 4 bytes|
+--------+---------------------------+--------+
```

## API

| Function | Description |
|----------|-------------|
| `mm_init()` | Initialize the heap with prologue/epilogue blocks |
| `mm_malloc(size)` | Allocate a block of the requested size |
| `mm_free(ptr)` | Free a previously allocated block |

## Build & Run

```bash
gcc simple_malloc.c -o simple_malloc
./simple_malloc
```

## Files

- `simple_malloc.c` - Main implementation
- `simple_malloc.h` - Header file with function declarations
# dynamic-memory-allocation
