#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
/*
The least significat bit is used to store the free flag since not all
bits of a 64-bit pointer are used
Memory layout:
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
*/ 
struct Book_keep {
	void* first_chunk;
  	void* last_chunk;
	size_t chunks_count;
    size_t free_chunks_count;
    void *free_history[10];
};
struct chunk_pointer {void* next;}; //makes it easier to read?
struct Book_keep data;
int initialized = 0;

int is_free(struct chunk_pointer *ptr) {
    return ((uintptr_t)ptr->next & 1);
}
size_t chunk_size(struct chunk_pointer *ptr) {
    return ((uintptr_t)ptr->next >> 1) - (uintptr_t)ptr - sizeof(void*);
}
void *local_allocation(size_t size) {
    //tries the 10 most recent frees
    for(int i = 0; i < 10; i++) {
        struct chunk_pointer *spot = data.free_history[i];
        if(is_free(spot) && chunk_size(spot) >= size) {
            spot->next = (struct chunk_pointer*)((uintptr_t)spot->next - 1);       //unset the free bit
            data.free_chunks_count -= 1;
            void *ptr = (void*)((uintptr_t)spot + sizeof(void*));
            for(int j = i; j > 0; j--) data.free_history[i] = data.free_history[i - 1];
            data.free_history[9] = NULL;
            return ptr;
        }
    }
    return NULL;
}
void* add_chunk(size_t size) {
    struct chunk_pointer *ptr = data.last_chunk;
    ptr = sbrk(sizeof(struct chunk_pointer));
    void *mem_pointer = sbrk(size);
    ptr->next = (void*)(((uintptr_t)sbrk(0) << 1) + 0);      //adds the free bit in the LS-bit
    data.chunks_count += 1;
    data.last_chunk = ptr;
    return mem_pointer;
}
void *try_reuse_memory(size_t size) {
   void *res = local_allocation(size);  //try if recent frees can be used
   if(res != NULL) return res;

    struct chunk_pointer *ptr = data.first_chunk;
    for(size_t i = 0; i < data.chunks_count; i++) {
        if(is_free(ptr) && chunk_size(ptr) >= size) {
            ptr->next = (struct chunk_pointer*)((uintptr_t)ptr->next - 1);       //unset the free bit
            data.free_chunks_count -= 1;
            if(chunk_size(ptr) - size >= sizeof(struct chunk_pointer) + 32) {  //split only when there would be 4 bytes available
                struct chunk_pointer *temp = (struct chunk_pointer*)((uintptr_t)ptr + chunk_size(ptr) + sizeof(void*));
                temp->next = (void*)((uintptr_t)ptr->next + 1);
                ptr->next = (void*)((uintptr_t)temp << 1);
                data.chunks_count += 1;
                data.free_chunks_count += 1;
            }
            return (void*)(uintptr_t)ptr + sizeof(void*);
        }
        ptr = (struct chunk_pointer*)((uintptr_t)ptr->next >> 1);
    }
    return add_chunk(size);
}
void initialize() {
    initialized = 1;
    data.first_chunk = sbrk(0);
    data.last_chunk = sbrk(0);
    data.free_chunks_count = 0;
    data.chunks_count = 0;
    for(int i = 0; i < 10; i++) data.free_history[i] = NULL;
}
void *mymalloc(size_t size) {
    if(!initialized) initialize(); 
    if(size == 0) return NULL;
    if(size % sizeof(long) != 0) size += sizeof(long) - size % sizeof(long);
    if(data.free_chunks_count == 0) return add_chunk(size);
    else return try_reuse_memory(size);
}
void *mycalloc(size_t nmemb, size_t size) {
    if((size * nmemb) == 0) return NULL;
    if(nmemb * size < size){
        fprintf(stderr , "ERROR: Overflow");
        return NULL;
    }
    void *start_ptr = mymalloc(size * nmemb);
    void *ptr = start_ptr;
    for(size_t i = 0; i < nmemb; i++) {
        memset(ptr , 0 , size);
        ptr = (void*)((uintptr_t)ptr + size);
    }
    return start_ptr;
}
void myfree(void *ptr) {
    if(data.chunks_count == 1) {
        //this puts break back to its original place and resets everything
        int res = brk(data.first_chunk);
        if(res < 0) fprintf(stderr , "ERROR while usig brk() in myfree()");
        initialized = 0;
        return;
    }
    if(ptr == NULL) return;
    struct chunk_pointer *current = data.first_chunk , *prev = data.first_chunk ,*place_holder;
    while((void*)((uintptr_t)current + sizeof(void*)) != ptr) {
        prev = current;
        current = (struct chunk_pointer*)((uintptr_t)current->next >> 1) ;
    }
    current->next = (void*)((uintptr_t)current->next + 1);   //set the free bit
    data.free_chunks_count += 1;
    place_holder = current;

    struct chunk_pointer *next_chunk = (struct chunk_pointer*)((uintptr_t)current->next >> 1);
    if(is_free(next_chunk)) {
        //merge if next block is free
        current->next = next_chunk->next;
        data.free_chunks_count -= 1;
        data.chunks_count -= 1;
    }
    if(is_free(prev)) {
        //merge if previous block is free
        prev->next = current->next;
        data.free_chunks_count -= 1;
        data.chunks_count -= 1;
        place_holder = prev;
    }
    // give memory back when last block is free
    struct chunk_pointer *temp = data.first_chunk;
    prev = NULL;
    int free = 0;
    for(size_t i = 0; i < data.chunks_count; i++) {
        prev = temp;
        free = is_free(temp);
        temp = (struct chunk_pointer*)((uintptr_t)temp->next >> 1);
    }
    //it ensure that there is always at least one block in the heap because it's handled at the start of myfree();
    if(free && data.chunks_count != 1) {
        int res = brk(prev + sizeof(void*));
        if(res < 0)fprintf(stderr , "ERROR: in myfree() while lowering brk");
        data.chunks_count -= 1;
        return;
    }
    for(int k = 9; k > 0; k--) data.free_history[k] = data.free_history[k-1];
    data.free_history[0] = place_holder;
}
void *myrealloc(void *ptr, size_t size)
{   
    if (ptr == NULL) return mymalloc(size);
    if(size == 0 && ptr != NULL) {
        myfree(ptr);
        return NULL;
    }
    struct chunk_pointer *current = data.first_chunk;
    while((void*)((uintptr_t)current + sizeof(void*)) != ptr) current = (struct chunk_pointer*)((intptr_t)current->next >> 1);
    if(size % 8 != 0) size += 8 - size % 8;
    size_t min_size = size;
    if(chunk_size(current) <= min_size) min_size = chunk_size(current);
    if(chunk_size(current) >= size && size > sizeof(long)) return ptr;

    struct chunk_pointer *next_chunk = (struct chunk_pointer*)((uintptr_t)current->next >> 1);
    if(is_free(next_chunk)) {
        //merge if next block is a free block and yeilds enough size if combined
        if(min_size != size && size - chunk_size(current) <= chunk_size(next_chunk) + sizeof(void*)) {
            current->next = next_chunk->next - 1;
            data.chunks_count -= 1;
            data.free_chunks_count -= 1;
            return ptr;
        } 
    } 
    //if previous cases fail, this will relocate the memory
    void *new_chunk_pointer = mymalloc(size);
    memcpy(new_chunk_pointer , ptr , min_size);
    myfree(ptr);
    return new_chunk_pointer;
} 


/*
 * Enable the code below to enable system allocator support for your allocator.
 * Doing so will make debugging much harder (e.g., using printf may result in
 * infinite loops).
 */
#if 0
void *malloc(size_t size) { return mymalloc(size); }
void *calloc(size_t nmemb, size_t size) { return mycalloc(nmemb, size); }
void *realloc(void *ptr, size_t size) { return myrealloc(ptr, size); }
void free(void *ptr) { myfree(ptr); }
#endif
