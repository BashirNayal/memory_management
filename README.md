# memory_management
Implementation of low-overhead memory allocator

The overhead per allocation of this implementation is 8 bytes. This is because a pointer is also used for metadata because not all bits are used in a 64 bits void pointer.
Memory layout:
```
--------------------- ----------------------- --------
|       |           ||       |              ||       |
|       |           ||       |              ||       |
|chunk_ |  memory   ||chunk_ |    memory    ||chunk_ |
|pointer|           ||pointer|              ||pointer|<----sbrk(0)
|       |           ||       |              ||       |
|       |           ||       |              ||       |
|       |           ||       |              ||       |
--------------------- ----------------------- --------              
    |                 ^ |                     ^  |   ^
    |                 | |                     |  |   |
    |                 | |                     |  |   |
    -------------------  ----------------------   ---
