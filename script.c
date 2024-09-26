#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#define MAX_KEY_SIZE 256
#define MAX_VALUE_SIZE 1024
#define INITIAL_TABLE_SIZE 10000  // Initial hash table size
#define LOAD_FACTOR_THRESHOLD 0.7 // Load factor threshold for resizing

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
    CacheEntry **entries; // array for hash table
    size_t table_size;
    size_t count;         // Total number of entries
    pthread_mutex_t lock; // Mutex for access control
} Cache;

unsigned int hash(const char *key, size_t table_size)
{
    unsigned int hash = 5381;
    int c;
    while ((c = *key++))
    {
        hash = ((hash << 5) + hash) + c;
    }
    return hash % table_size; // mode based on the size of the hash table
}

// cache resize
void resizeCache(Cache *cache)
{
    size_t new_size = cache->table_size * 2;                           // double the new size
    CacheEntry **new_entries = calloc(new_size, sizeof(CacheEntry *)); // Allocate memory for new table
    for (size_t i = 0; i < cache->table_size; i++)
    {
        CacheEntry *entry = cache->entries[i];
        while (entry)
        {
            // calculate new index
            unsigned int index = hash(entry->key, new_size);
            CacheEntry *next_entry = entry->next; // save next entry

            // add entry to new table
            entry->next = new_entries[index]; // add by chaining
            new_entries[index] = entry;

            entry = next_entry; // move to the next entry
        }
    }
    free(cache->entries);         // Release the old table
    cache->entries = new_entries; // Switch to new table
    cache->table_size = new_size; // Update new table size
}

// Create cache
Cache *createCache()
{
    Cache *cache = malloc(sizeof(Cache));                              // allocate memory for cache
    cache->table_size = INITIAL_TABLE_SIZE;                            // initial size
    cache->count = 0;                                                  // there is no entry at the beginning
    cache->entries = calloc(INITIAL_TABLE_SIZE, sizeof(CacheEntry *)); // allocate memory
    pthread_mutex_init(&cache->lock, NULL);                            // initialize mutex
    return cache;
}

// Adding data
void setCache(Cache *cache, const char *key, const char *value, int ttl)
{
    pthread_mutex_lock(&cache->lock); // get access lock

    // resize if table is full
    if (((float)(cache->count + 1) / cache->table_size > LOAD_FACTOR_THRESHOLD))
    {
        resizeCache(cache);
    }

    unsigned int index = hash(key, cache->table_size); // calculate hash index for key

    // allocate memory for new entry
    CacheEntry *entry = malloc(sizeof(CacheEntry));
    strncpy(entry->key, key, MAX_KEY_SIZE);
    strncpy(entry->value, value, MAX_VALUE_SIZE);
    entry->expry = time(NULL) + ttl;
    entry->is_set = 1;
    entry->next = cache->entries[index]; // save old entries with chaining
    cache->entries[index] = entry;       // add new entry
    cache->count++;                      //  increase the number of entries

    pthread_mutex_unlock(&cache->lock); // release access lock
    printf("Data added: %s -> %s (TTL: %d)\n", key, value, ttl);
}

// Reading data
const char *getCache(Cache *cache, const char *key)
{
    pthread_mutex_lock(&cache->lock);                  // get access lock
    unsigned int index = hash(key, cache->table_size); // calculate hash index for key
    CacheEntry *entry = cache->entries[index];         // get the relevant entry

    while (entry) // check through chaining
    {
        // if keys match
        if (strcmp(entry->key, key) == 0)
        {
            // if expiration time has not passed
            if (time(NULL) < entry->expry)
            {
                pthread_mutex_unlock(&cache->lock); // release access lock
                return entry->value;                // return value
            }
            else
            {
                // expired, delete
                entry->is_set = 0; // invalidate the entry
                cache->count--;    // decrease the entry count
            }
            entry = entry->next; // check the next entry through chaining
        }
    }

    pthread_mutex_unlock(&cache->lock); // release access lock
    return NULL;                        // return null if no value exists
}

// Deleting data
void deleteCache(Cache *cache, const char *key)
{
    pthread_mutex_lock(&cache->lock);                  // Get access lock
    unsigned int index = hash(key, cache->table_size); // Calculate hash index for key
    CacheEntry *entry = cache->entries[index];         // Get the relevant entry
    CacheEntry *prev_entry = NULL;                     // Save the previous entry

    while (entry)
    {
        if (strcmp(entry->key, key) == 0)
        { // if key matches
            if (prev_entry)
            {
                prev_entry->next = entry->next; // unlink from chain
            }
            else
            {
                cache->entries[index] = entry->next; // update head if it's the first element
            }
            free(entry);                        // free the entry
            cache->count--;                     // decrease the entry count
            pthread_mutex_unlock(&cache->lock); // release access lock
            printf("Data deleted %s\n", key);
            return;
        }
        prev_entry = entry;  // update previous entry
        entry = entry->next; // check the next entry through chaining
    }
    pthread_mutex_unlock(&cache->lock); // release access lock
}

// Freeing cache
void freeCache(Cache *cache)
{
    for (size_t i = 0; i < cache->table_size; i++)
    {
        CacheEntry *entry = cache->entries[i];

        while (entry)
        {
            CacheEntry *next_entry = entry->next; // save the next entry
            free(entry);                          // free the entry
            entry = next_entry;                   // move to the next entry
        }
    }
    free(cache->entries);                // free the entire table
    pthread_mutex_destroy(&cache->lock); // destroy the mutex
    free(cache);                         // free the allocated memory for cache
}

int main()
{
    Cache *cache = createCache(); // Create the cache

    setCache(cache, "user:001", "Michael Jordan", 10);
    setCache(cache, "user:002", "Kobe Bryant", 20);

    printf("Read data: %s\n", getCache(cache, "user:001"));
    printf("Read data: %s\n", getCache(cache, "user:002"));

    // deleteCache(cache, "user:001");
    // printf("Read data: %s\n", getCache(cache, "user:1002")); // NULL (deleted)

    // Free the cache
    freeCache(cache);
    return 0;
}
