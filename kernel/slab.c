#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
#include "slab.h"

void print_kmem_cache(struct kmem_cache *cache, void (*slab_obj_printer)(void *))
{
  // TODO: Implement print_kmem_cache
  printf("[SLAB] TODO: print_kmem_cache is not yet implemented \n");
}

struct kmem_cache *kmem_cache_create(char *name, uint object_size)
{
  uint max_objs = (MP2_SLAB_SIZE - sizeof(struct slab)) / object_size;
  struct kmem_cache *cache;
  cache = (struct kmem_cache *)kalloc();

  safestrcpy(cache->name, name, MP2_CACHE_MAX_NAME);
  cache->object_size = object_size;
  initlock(&cache->lock, name);

  printf("[SLAB] New kmem_cache (name: %s, object size: %u bytes, at: %p, max objects per slab: %u, support in cache obj: %u) is created\n", 
    name, object_size, cache, max_objs, 0); // FIXME: in Internal Fragmentation Optimization
  return cache;
}

void kmem_cache_destroy(struct kmem_cache *cache)
{
  // TODO: Implement kmem_cache_destroy (will not be tested)
  printf("[SLAB] TODO: kmem_cache_destroy is not yet implemented \n");
}

void *kmem_cache_alloc(struct kmem_cache *cache)
{
  // TODO: Implement kmem_cache_alloc
  printf("[SLAB] TODO: kmem_cache_alloc is not yet implemented \n");
  
  // acquire(&cache->lock); // acquire the lock before modification
  // ... (modify kmem_cache)
  // release(&cache->lock); // release the lock before return
  return 0;
}

void kmem_cache_free(struct kmem_cache *cache, void *obj)
{
  // TODO: Implement kmem_cache_free
  printf("[SLAB] TODO: kmem_cache_free is not yet implemented \n");

  // acquire(&cache->lock); // acquire the lock before modification
  // ... (modify kmem_cache)
  // release(&cache->lock); // release the lock before return
}
