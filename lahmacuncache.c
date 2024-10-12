#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#define MAX_KEY_SIZE 256
#define MAX_VALUE_SIZE 1024
#define INITIAL_TABLE_SIZE 10000  
#define LOAD_FACTOR_THRESHOLD 0.7 

typedef struct CacheEntry
{
    char key[MAX_KEY_SIZE];
    char value[MAX_VALUE_SIZE];
    time_t expry;
    int is_set;
    struct CacheEntry *next;
} CacheEntry;

typedef struct
{
    CacheEntry **entries; 
    size_t table_size;
    size_t count;        
    pthread_mutex_t lock; 
} Cache;

unsigned int hash(const char *key, size_t table_size)
{
    unsigned int hash = 5381;
    int c;
    while ((c = *key++))
    {
        hash = ((hash << 5) + hash) + c;
    }
    return hash % table_size; 

// cache resize
void resizeCache(Cache *cache)
{
    size_t new_size = cache->table_size * 2;                           
    CacheEntry **new_entries = calloc(new_size, sizeof(CacheEntry *)); 
    for (size_t i = 0; i < cache->table_size; i++)
    {
        CacheEntry *entry = cache->entries[i];
        while (entry)
        {
            unsigned int index = hash(entry->key, new_size);
            CacheEntry *next_entry = entry->next; 

            entry->next = new_entries[index]; /
            new_entries[index] = entry;

            entry = next_entry; 
        }
    }
    free(cache->entries);        
    cache->entries = new_entries; 
    cache->table_size = new_size; 
}

Cache *createCache()
{
    Cache *cache = malloc(sizeof(Cache));                             
    cache->table_size = INITIAL_TABLE_SIZE;                          
    cache->count = 0;                                                  
    cache->entries = calloc(INITIAL_TABLE_SIZE, sizeof(CacheEntry *)); 
    pthread_mutex_init(&cache->lock, NULL);                          
    return cache;
}

void setCache(Cache *cache, const char *key, const char *value, int ttl)
{
    pthread_mutex_lock(&cache->lock);

    if (((float)(cache->count + 1) / cache->table_size > LOAD_FACTOR_THRESHOLD))
    {
        resizeCache(cache);
    }

    unsigned int index = hash(key, cache->table_size); 
    CacheEntry *entry = malloc(sizeof(CacheEntry));
    strncpy(entry->key, key, MAX_KEY_SIZE);
    strncpy(entry->value, value, MAX_VALUE_SIZE);
    entry->expry = time(NULL) + ttl;
    entry->is_set = 1;
    entry->next = cache->entries[index];
    cache->entries[index] = entry;       
    cache->count++;                     

    pthread_mutex_unlock(&cache->lock);
    printf("Data added: %s -> %s (TTL: %d)\n", key, value, ttl);
}

const char *getCache(Cache *cache, const char *key)
{
    pthread_mutex_lock(&cache->lock);                 
    unsigned int index = hash(key, cache->table_size);
    CacheEntry *entry = cache->entries[index];        

    while (entry)
    {
        if (strcmp(entry->key, key) == 0)
        {
            if (time(NULL) < entry->expry)
            {
                pthread_mutex_unlock(&cache->lock);
                return entry->value;               
            }
            else
            {
                entry->is_set = 0;
                cache->count--;    
            }
            entry = entry->next;
        }
    }

    pthread_mutex_unlock(&cache->lock);
    return NULL;                       
}

// Deleting data
void deleteCache(Cache *cache, const char *key)
{
    pthread_mutex_lock(&cache->lock);                  
    unsigned int index = hash(key, cache->table_size);
    CacheEntry *entry = cache->entries[index];        
    CacheEntry *prev_entry = NULL;                  

    while (entry)
    {
        if (strcmp(entry->key, key) == 0)
        { 
            if (prev_entry)
            {
                prev_entry->next = entry->next; 
            }
            else
            {
                cache->entries[index] = entry->next; 
            }
            free(entry);                       
            cache->count--;                    
            pthread_mutex_unlock(&cache->lock); 
            printf("Data deleted %s\n", key);
            return;
        }
        prev_entry = entry;  
        entry = entry->next; 
    }
    pthread_mutex_unlock(&cache->lock); 
}

void freeCache(Cache *cache)
{
    for (size_t i = 0; i < cache->table_size; i++)
    {
        CacheEntry *entry = cache->entries[i];

        while (entry)
        {
            CacheEntry *next_entry = entry->next; 
            free(entry);                          
            entry = next_entry;                  
        }
    }
    free(cache->entries);                
    pthread_mutex_destroy(&cache->lock); 
    free(cache);                       
}

int main()
{
    Cache *cache = createCache(); 

    setCache(cache, "user:001", "Michael Jordan", 10);
    setCache(cache, "user:002", "Kobe Bryant", 20);

    printf("Read data: %s\n", getCache(cache, "user:001"));
    printf("Read data: %s\n", getCache(cache, "user:002"));

    // deleteCache(cache, "user:001");
    // printf("Read data: %s\n", getCache(cache, "user:1002")); // NULL (deleted)

    freeCache(cache);
    return 0;
}
