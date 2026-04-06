#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
#include "slab.h"
#include "list.h"


static void
slab_shuffle_freelist(struct slab *s, uint object_size)
{
  void *slots[128];  // max_objs <= 128 for any realistic object_size
  uint n = s->max_objs;
  uint64 seed = r_time();
  void *objs_start = (void *)((char *)s + sizeof(struct slab));

  for(uint i = 0; i < n; i++) {
    slots[i] = objs_start + i * object_size;
  }

  for (uint i = n-1 ; i > 0; i--) {
    seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
    uint j = (uint)(seed >> 33) % (i + 1);
    void *tmp = slots[i];
    slots[i] = slots[j];
    slots[j] = tmp;
  }

  for (uint i = 0; i < n-1 ; i++) {
    *(void **)slots[i] = slots[i+1];
  }
  *(void **)slots[n-1] = 0;
  s->freelist = (void **)slots[0];
}

void print_kmem_cache(struct kmem_cache *cache, void (*slab_obj_printer)(void *))
{
  acquire(&cache->lock);
  printf("[SLAB] kmem_cache { name: %s, object_size: %u, at: %p, in_cache_obj: %u }\n",
    cache->name, cache->object_size, cache, cache->cache_max_objs);

  // print cache
  if (cache->cache_max_objs > 0) {
    printf("[SLAB] \t[ cache\tslabs ]\n");
    printf("[SLAB] \t\t[ slab %p ] { freelist: %p, nxt: %p, max_objs: %u }\n",
      cache, cache->cache_freelist, (void *)0, cache->cache_max_objs);

    void *objs_start = (void *)((char *)cache + sizeof(struct kmem_cache));
    for (uint i = 0; i < cache->cache_max_objs; i++) {
      void **slot = (void **)((char *)objs_start + i * cache->object_size);
      printf("[SLAB] \t\t\t[ idx %u ] { addr: %p, as_ptr: %p, as_obj: {",
        i, slot, *slot);
      slab_obj_printer(slot);
      printf("} }\n");
    }
  }

  // print partial
  if (!list_empty(&cache->partial)) {
    struct slab *s;
    printf("[SLAB] \t[ partial\tslabs ]\n");
    list_for_each_entry(s, &cache->partial, list) {
      void *nxt = (s->list.next == &cache->partial)
            ? (void *)0
            : (void *)list_entry(s->list.next, struct slab, list);

      printf("[SLAB] \t\t[ slab %p ] { freelist: %p, nxt: %p, max_objs: %u }\n",
        s, s->freelist, nxt, s->max_objs);
      void *objs_start = (void *)((char *)s + sizeof(struct slab));
      for (uint i = 0; i < s->max_objs; i++) {
        void **slot = (void **)((char *)objs_start + i * cache->object_size);
        printf("[SLAB] \t\t\t[ idx %u ] { addr: %p, as_ptr: %p, as_obj: {",
          i, slot, *slot);
        slab_obj_printer(slot);
        printf("} }\n");
      }
    }
  }
  printf("[SLAB] print_kmem_cache end\n");
  release(&cache->lock);
}

struct kmem_cache *kmem_cache_create(char *name, uint object_size)
{
  uint max_objs = (MP2_SLAB_SIZE - sizeof(struct slab)) / object_size;
  uint cache_max_objs = (MP2_SLAB_SIZE - sizeof(struct kmem_cache)) / object_size;
  void *objs_start;
  struct kmem_cache *cache;
  cache = (struct kmem_cache *)kalloc();

  safestrcpy(cache->name, name, MP2_CACHE_MAX_NAME);
  cache->object_size = object_size;
  initlock(&cache->lock, name);
  INIT_LIST_HEAD(&cache->partial);

  objs_start = (void *)((char *)cache + sizeof(struct kmem_cache));
  cache->cache_freelist = cache_max_objs == 0 ? 0 : (void **)objs_start;
  for (uint i = 0; i < cache_max_objs; i++) {
    void **slot = (void **)((char *)objs_start + i * object_size);
    *slot = (i < cache_max_objs - 1) ? (char *)objs_start + (i+1) * object_size : 0;
  }
  cache->cache_max_objs = cache_max_objs;
  cache->cache_in_use = 0;

  printf("[SLAB] New kmem_cache (name: %s, object size: %u bytes, at: %p, max objects per slab: %u, support in cache obj: %u) is created\n",
    name, object_size, cache, max_objs, cache->cache_max_objs);
  return cache;
}

void kmem_cache_destroy(struct kmem_cache *cache)
{
  // TODO: Implement kmem_cache_destroy (will not be tested)
  printf("[SLAB] TODO: kmem_cache_destroy is not yet implemented \n");
}

void *kmem_cache_alloc(struct kmem_cache *cache)
{
  void *obj;
  struct slab *s;
  acquire(&cache->lock);
  printf("[SLAB] Alloc request on cache %s\n", cache->name);

  if (cache->cache_freelist) {
    obj = cache->cache_freelist;
    cache->cache_freelist = (void **)*(cache->cache_freelist);
    cache->cache_in_use += 1;
    printf("[SLAB] Object %p in slab %p (%s) is allocated and initialized\n", obj, cache, cache->name);
    release(&cache->lock);
    return obj;
  }

  if (list_empty(&cache->partial)) {
    struct slab *new_s = (struct slab *)kalloc();
    uint object_size = cache->object_size;
    uint max_objs = (MP2_SLAB_SIZE - sizeof(struct slab)) / object_size;

    // void *objs_start = (void *)((char *)new_s + sizeof(struct slab));
    // new_s->freelist = (void **)objs_start;
    // for (uint i = 0; i < max_objs; i++) {
    //   void **slot = (void **)((char *)objs_start + i * object_size);
    //   *slot = (i < max_objs - 1) ? (char *)objs_start + (i+1) * object_size : 0;
    // }

    new_s->max_objs = max_objs;
    new_s->in_use = 0;
    slab_shuffle_freelist(new_s, object_size);
    list_add(&new_s->list, &cache->partial);
    printf("[SLAB] A new slab %p (%s) is allocated\n", new_s, cache->name);
  }

  s = list_first_entry(&cache->partial, struct slab, list);
  obj = s->freelist;
  s->in_use += 1;
  if (s->in_use < s->max_objs) {
    s->freelist = (void **)*(s->freelist);
  } else { // remove from partial
    s->freelist = 0;
    list_del_init(&s->list);
  }

  printf("[SLAB] Object %p in slab %p (%s) is allocated and initialized\n", obj, s, cache->name);
  release(&cache->lock);
  return obj;
}

void kmem_cache_free(struct kmem_cache *cache, void *obj)
{
  struct slab *s = (struct slab *)((uint64)obj & ~(MP2_SLAB_SIZE - 1));
  acquire(&cache->lock);
  printf("[SLAB] Free %p in slab %p (%s)\n", obj, s, cache->name);
  
  if (s == (void *)cache) {
    void **slot = (void **)obj;
    *slot = (void *)(cache->cache_freelist);
    cache->cache_freelist = (void **)obj;
    cache->cache_in_use -= 1;
    printf("[SLAB] End of free\n");
    release(&cache->lock);
    return;
  }

  void **slot = (void **)obj;
  *slot = (void *)s->freelist;
  s->freelist = (void **)obj;

  
  if (s->in_use == s->max_objs) { // add to partial
    list_add(&s->list, &cache->partial);
  }
  s->in_use -= 1;

  uint cnt = 0;
  struct slab *ptr;
  list_for_each_entry(ptr, &cache->partial, list) {
    cnt++;
  }

  if (s->in_use == 0 && cnt > MP2_MIN_AVAIL_SLAB) {
    list_del_init(&s->list);
    kfree(s);
    printf("[SLAB] Slab %p (%s) is freed due to save memory\n", s, cache->name);
  }
  printf("[SLAB] End of free\n");
  release(&cache->lock);
}
