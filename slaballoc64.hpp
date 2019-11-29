/*
    Simple caching allocator for small and uniform buffers, inspired by Bonwick's slab allocator.
    Objects are aligned to 8/16 byte if a multiple of 8/16 etc. is used, otherwise not. It is not
    thread-safe and  only 1-64 objects per page (64B minimum for 4096B page size) are supported.
    When it has a fixed working set, it is around 2x faster than malloc/free and uses less memory.
    It also can immediately give back unused pages to the OS.
    
    TODO:
        * Maximum/minimum free chain size
        * Allocate multiple pages at once
*/
#include <sys/mman.h>
#include <string.h>

#define LINKALIGN   (16)
#define PAGESIZE    (4096)
#define FREESPACE   (PAGESIZE - (sizeof(slablink)) - ((sizeof(slablink)) % LINKALIGN))
#define NUMOBJS     (FREESPACE / N)
#define PAGEADDR(p) ((char*)(((uintptr_t)(char*)(p)) &~ (PAGESIZE - 1)))
#define LINKADDR(p) (PAGEADDR(p) + FREESPACE)
#define S2ADDR(p,s) (PAGEADDR(p) + N * (s))
#define ADDR2S(p)   (((char*)(p) - PAGEADDR(p)) / N)
#define SLOTS_EMPTY (~0ul)              // 1111111111111111111111111111111111111111111111111111111111111111 Bit set = free
#define SLOTS_FULL  (~0ul << NUMOBJS)   // 1111111111111111111111111111111111111111111111111111111100000000 for NUMOBJS = 8

// Stored at the end of every page
struct slablink
{
    slablink* prev;
    slablink* next;
    uint64_t slots; // Bitfield for 64 slots. 1 indicates free space
    
    slablink(slablink* prev, slablink* next) : prev(prev), next(next), slots(SLOTS_EMPTY) { }
    
    inline unsigned find_and_fill_slot()
    {
        unsigned slot = ffsl(slots) - 1;
        slots &= ~(1ul << slot);
        return slot;
    }
    
    inline void free_slot(unsigned slot)
    {
        slots |= 1ul << slot;
    }
    
    void destroy()
    {
        // We're unmapping our own page, need to save next pointer
        // Also don't do recursion for this
        slablink* p = this;
        do {
            slablink* pnext = p->next;
            munmap(PAGEADDR(p), PAGESIZE);
            p = pnext;
        } while (p != nullptr);
    }
};

// Allocator for objects of size N
template <unsigned N> class slaballoc64
{
    static_assert(N > 0, "The object size must be non-null");
    static_assert(N <= FREESPACE, "The object size ist too large to fit into a single page");
    static_assert(NUMOBJS <= 64, "At most 64 objects can be stored per page");
    static_assert(sizeof(unsigned long) == 8, "Unsigned long must have 64 bits");
    
    slablink* empty;    // List of empty pages
    slablink* part;     // List of partially filled pages
    slablink* full;     // List of completely filled pages
    
    inline slablink* fresh_page()
    {
        char* page = (char*)mmap(NULL, PAGESIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_POPULATE, -1, 0);
        if (page == MAP_FAILED)
            return nullptr;
        return new LINKADDR(page) slablink(nullptr, nullptr);
    }
    
    // Remove first element from chain
    inline slablink* pop(slablink** chain)
    {
        if (*chain) {
            slablink* first = *chain;
            if (first->next) {
                // There are more elements
                first->next->prev = nullptr;
                *chain = first->next;
            } else {
                // We're removing the only element
                *chain = nullptr;
            }
            first->prev = nullptr;
            first->next = nullptr;
            return first;
        }
        return nullptr;
    }
    
    // Cut out given element at arbitrary position from chain and reconnect chain
    inline void excise(slablink** chain, slablink* elem)
    {
        if (*chain == elem) {
            // It's the first element of the chain
            (void)pop(chain);
        } else {
            // It's somewhere in the middle
            if (elem->prev)
                elem->prev->next = elem->next;
            if (elem->next)
                elem->next->prev = elem->prev;
            elem->prev = nullptr;
            elem->next = nullptr;
        }
    }
    
    // Add new element at the beginning of chain
    inline void push(slablink* elem, slablink** chain)
    {
        if (*chain) {
            elem->next = *chain;
            elem->prev = nullptr;
            (*chain)->prev = elem;
            *chain = elem;
        } else {
            *chain = elem;
            (*chain)->prev = nullptr;
            (*chain)->next = nullptr;
        }
    }
    
    // Move the first element of chain 'from' to chain 'to'
    void movefirst(slablink** from, slablink** to)
    {
        slablink* elem = pop(from);
        if (elem) {
            push(elem, to);
        }
    }
    
public:
    
    slaballoc64() : empty(nullptr), part(nullptr), full(nullptr) { }
    
    ~slaballoc64()
    {
        if (empty != nullptr)
            empty->destroy();
        if (part != nullptr)
            part->destroy();
        if (full != nullptr)
            full->destroy();
    }
    
    void release()
    {
        slablink* oldempty = empty;
        empty = nullptr;
        if (oldempty != nullptr)
            oldempty->destroy();
    }
    
    // Get new buffer or NULL (errno set)
    void* alloc()
    {
        if (part == nullptr) {
            // We have no partially filled page! Need to get one.
            if (empty == nullptr) {
                // Allocate a new empty page
                slablink* fresh = fresh_page();
                if (fresh == nullptr)
                    return nullptr;
                push(fresh, &part);
            } else {
                // Use the first empty page instead
                movefirst(&empty, &part);
            }
        }
        // Fill a slot of the first partially filled page
        char* addr = S2ADDR(part, part->find_and_fill_slot());
        // If the page is now full, move it to the full chain
        if (part->slots == SLOTS_FULL)
            movefirst(&part, &full);
        return addr;
    }
    
    // Free a pointer obtained by alloc()
    void free(void* buf)
    {
        slablink* elem = (slablink*)LINKADDR(buf);
        unsigned slot = ADDR2S(buf);
        if (elem->slots == SLOTS_FULL) {
            // Freeing an element of the full chain
            // Move it to the partial or empty chain afterwards
            elem->free_slot(slot);
            excise(&full, elem);
            if (elem->slots == SLOTS_EMPTY) {
                push(elem, &empty);
            } else {
                push(elem, &part);
            }
        } else {
            // Freeing an element of the partial chain
            // If it is empty afterwards, move to empty chain
            elem->free_slot(slot);
            if (elem->slots == SLOTS_EMPTY) {
                excise(&part, elem);
                push(elem, &empty);
            }
        }
    }
    
    unsigned per_obj()
    {
        return N;
    }
    
    unsigned per_page()
    {
        return NUMOBJS;
    }
};

#undef LINKALIGN
#undef PAGESIZE
#undef FREESPACE
#undef NUMOBJS
#undef PAGEADDR
#undef LINKADDR
#undef S2ADDR
#undef ADDR2S
#undef SLOTS_EMPTY
#undef SLOTS_FULL 
