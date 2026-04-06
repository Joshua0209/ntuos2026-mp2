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
  // FIXME: in Internal Fragmentation Optimization
  acquire(&cache->lock); // acquire the lock before modification
  printf("[SLAB] kmem_cache { name: %s, object_size: %u, at: %p, in_cache_obj: %u }\n",
    cache->name, cache->object_size, cache, 0);

  // print partial
  printf("[SLAB] \t[ partial\tslabs ]\n");
  for (struct slab *s = cache->partial; s != 0; s = s->next) {
    printf("[SLAB] \t\t[ slab %p ] { freelist: %p, nxt: %p, max_objs: %u }\n",
      s, s->freelist, s->next, s->max_objs);

    // uint idx = 0;
    void *objs_start = (void *)((char *)s + sizeof(struct slab));
    for (uint i = 0; i < s->max_objs; i++) {
      void **slot = (void **)((char *)objs_start + i * cache->object_size);
      printf("[SLAB] \t\t\t[ idx %u ] { addr: %p, as_ptr: %p, as_obj: {",
        i, slot, *slot);
      slab_obj_printer(slot);
      printf("} }\n");
    }
  }
  printf("[SLAB] print_kmem_cache end\n");
  release(&cache->lock); // release the lock before return
}

struct kmem_cache *kmem_cache_create(char *name, uint object_size)
{
  uint max_objs = (MP2_SLAB_SIZE - sizeof(struct slab)) / object_size;
  struct kmem_cache *cache;
  cache = (struct kmem_cache *)kalloc();

  safestrcpy(cache->name, name, MP2_CACHE_MAX_NAME);
  cache->object_size = object_size;
  initlock(&cache->lock, name);
  cache->partial = 0;

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
  void *obj;
  struct slab *s;
  acquire(&cache->lock); // acquire the lock before modification
  printf("[SLAB] Alloc request on cache %s\n", cache->name);
  // FIXME: Internal Fragmentation Optimization
  if (!cache->partial) {
    struct slab *new_s = (struct slab *)kalloc();
    uint object_size = cache->object_size;
    uint max_objs = (MP2_SLAB_SIZE - sizeof(struct slab)) / object_size;

    void *objs_start = (void *)((char *)new_s + sizeof(struct slab));
    new_s->freelist = (void **)objs_start;
    for (uint i = 0; i < max_objs; i++) {
      void **slot = (void **)((char *)objs_start + i * object_size);
      *slot = (i < max_objs - 1) ? (char *)objs_start + (i+1) * object_size : 0;
    }

    new_s->next = 0;
    new_s->max_objs = max_objs;
    new_s->in_use = 0;
    cache->partial = new_s;
    printf("[SLAB] A new slab %p (%s) is allocated\n", new_s, cache->name);
  }

  s = cache->partial;
  obj = s->freelist;
  s->in_use += 1;
  if (s->in_use < s->max_objs) {
    s->freelist = (void **)*(s->freelist);
  } else { // remove from partial
    s->freelist = 0;
    cache->partial = s->next;
  }

  printf("[SLAB] Object %p in slab %p (%s) is allocated and initialized\n", obj, s, cache->name);
  release(&cache->lock); // release the lock before return
  return obj;
}

void kmem_cache_free(struct kmem_cache *cache, void *obj)
{
  // TODO: Implement kmem_cache_free
  struct slab *s = (struct slab *)((uint64)obj & ~(MP2_SLAB_SIZE - 1));
  acquire(&cache->lock); // acquire the lock before modification
  printf("[SLAB] Free %p in slab %p (%s)\n", obj, s, cache->name);
  
  void **slot = (void **)obj;
  *slot = (void *)s->freelist;
  s->freelist = (void **)obj;

  
  if (s->in_use == s->max_objs) { // add to partial
    s->next = cache->partial;
    cache->partial = s;
  }
  s->in_use -= 1;

  uint cnt = 0;
  struct slab *prev = 0;
  for (struct slab *ptr = cache->partial; ptr != 0; ptr = ptr->next) {
    cnt++;
    if (ptr->next == s)
      prev = ptr;
  }

  if (s->in_use == 0 && cnt > MP2_MIN_AVAIL_SLAB) {
    if (cache->partial == s)
      cache->partial = s->next;
    else
      prev->next = s->next;
    kfree(s);
    printf("[SLAB] Slab %p (%s) is freed due to save memory\n", s, cache->name);
  }
  printf("[SLAB] End of free\n");
  release(&cache->lock); // release the lock before return
}
