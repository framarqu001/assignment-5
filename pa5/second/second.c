
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

unsigned int memRead = 0; 
unsigned int memWrite = 0;;

typedef struct {
    unsigned int metaData;
    unsigned long tag;
} Block;

typedef struct {
    unsigned int metaCount;
    bool filled;
    unsigned int count;
    Block** blocks;
} Set;

typedef struct Cache { 
    unsigned int setSize;
    unsigned int assocSize;
    unsigned int blockSize;
    unsigned int cacheHit;
    unsigned int cacheMiss;
    char* policy;
    struct Cache* l2Cache;
    Set** sets;
} Cache;


Block* allocateBlock() {
    Block* block =  malloc(sizeof(Block));
    block->tag = 0;
    block->metaData = 0;
    return block;
}

Set* allocateSet(unsigned int a) {
    Set* set = malloc(sizeof(Set));
    set->filled = false;
    set->count = 0;
    set->metaCount = 0;
    set->blocks = malloc(a *sizeof(Block*));
    for (int i = 0; i < a; i++) {
        set->blocks[i] = allocateBlock();
    }
    return set;
}

Cache* allocateCache(unsigned int s, unsigned int a, unsigned int b, char* policy) {
    Cache* cache = malloc(sizeof(Cache));
    cache->assocSize = a;
    cache->setSize = s;
    cache->blockSize = b;
    cache->cacheHit = 0;
    cache->cacheMiss =0;
    cache->policy = strdup(policy);
    cache->l2Cache = NULL;
    cache->sets = malloc(s * sizeof(Set*));
    for (int i = 0 ; i < s; i++) {
        cache->sets[i] = allocateSet(a);
    }
    return cache;
}

void freeCache(Cache* cache) {
    unsigned int s = cache->setSize;
    unsigned int a = cache->assocSize;
    for (int i = 0; i < s; i++) {
        for (int j = 0; j < a; j++) {
            free(cache->sets[i]->blocks[j]);
        }
        free(cache->sets[i]);
    }
    free(cache->sets);
    free(cache->policy);
    free(cache);
}

void printCache(Cache* cache){ //tester to see how cache looks
    unsigned int s = cache->setSize;
    unsigned int a = cache->assocSize;
    for (int i = 0; i < s; i++) {
        printf("Set %d\n", i);
        for (int j = 0; j < a; j++) {
            printf("Block %d: %lx -> %u\n", j,cache->sets[i]->blocks[j]->tag, cache->sets[i]->blocks[j]->metaData);
        }
        printf("\n\n");
    }
    printf("---------------\n\n");
}


unsigned int logBase2(unsigned int val) {
    unsigned int count = 0;;
    while (val > 1) {
        val = val >> 1;
        count++;
    }
    return count;
}



unsigned long unhashTag(Cache* cache, unsigned long tag, unsigned int index) {
    unsigned int blockBitSize = logBase2(cache->blockSize);
    unsigned int setBitSize = logBase2(cache->setSize);

    tag = tag << setBitSize;
    tag = tag + index;
    tag = tag << blockBitSize;
    return tag;
}

unsigned int hashAddress(Cache* cache, unsigned long* address) {
    unsigned int blockBitSize = logBase2(cache->blockSize);
    *address = *address >> blockBitSize;
    unsigned int setBitSize = logBase2(cache->setSize);
    unsigned int extractSetIndex = 1 << setBitSize;
    extractSetIndex = extractSetIndex - 1;
    unsigned int setIndex = *address & extractSetIndex;
    *address = *address >> setBitSize;
    return setIndex;
}

void setRemoveBlock(Set* set, unsigned int index) {
   
    set->filled = false;
    for (int i = index; i < set->count - 1; i++) {
        set->blocks[i]->tag = set->blocks[i +1 ]->tag;
        set->blocks[i]->metaData = set->blocks[i + 1] ->metaData;
    }
    set->count--;
}

bool setSearch(unsigned long tag, Set* set, unsigned int n, char* policy, bool remove) {
    for (int i = 0; i < n; i++) {
        if (set->blocks[i]->tag == tag) {
            if (strcmp(policy, "lru") == 0){ // if LRU we increment the block count to show its been used recently.
               set->blocks[i]->metaData = set->metaCount++; 
            }
            if (remove == true) {
                setRemoveBlock(set, i);
            }
            return true;
        }
    }
    return false;
}

unsigned int minBlockSearch(Set* set) {
    unsigned minData = set->blocks[0]->metaData;
    unsigned int minIndex = 0;
    for (int i = 0; i < set->count; i++) {
        if (set->blocks[i]->metaData < minData) {
            minIndex = i;
            minData = set->blocks[i]->metaData;
        }
    }
    return minIndex;
}



unsigned int long insertBlock(Cache* cache, unsigned long address) { // Returns true if eviction occured.
    // char* policy = cache->policy;
    unsigned int index = hashAddress(cache, &address);
    Set* set = cache->sets[index];
    unsigned int count = set->count;

    if (set->filled == false) { // If set not filled we just add block to next index
        Block* block = set->blocks[count];
        block->tag = address;
        set->count++;
        if (set->count == cache->assocSize) {
            set->filled = true;
        }
        block->metaData = set->metaCount++;
        return 0;                            
    } else { 
        unsigned int bIndex = minBlockSearch(set); 
        Block* block = set->blocks[bIndex];
        unsigned long unhashedAddress = unhashTag(cache, block->tag, index);
        block->tag = address;
        block->metaData = set->metaCount++;
        printf("%u\n", set->metaCount++);
        return unhashedAddress;
    }
    
}

bool cacheSearch(Cache* cache, unsigned long address, bool remove) {
    unsigned setIndex = hashAddress(cache, &address);
    Set* set = cache->sets[setIndex];
    unsigned int n = cache->assocSize;
    char* policy = cache->policy;
    if (setSearch(address, set, n, policy, remove) == true) { // If address is in set then hit.
        return true;
    } 
    return false;
}

void blockRemove(Cache* cache, unsigned long address) {
    
}

void l1l2CacheSearch(Cache* l1Cache, unsigned long address, char operation) {
    if (operation == 'W') {
        memWrite++;
    }
    
    Cache* l2cache = l1Cache->l2Cache;
    if (cacheSearch(l1Cache, address, false) == true) { //address found in L1.
        l1Cache->cacheHit++;
        return;
    } 
    l1Cache->cacheMiss++;
   
    if (cacheSearch(l2cache, address, true) == true) { //will remove if found
        l2cache->cacheHit++;
        unsigned long unHashTag = insertBlock(l1Cache, address);
        if (unHashTag > 0) { //block was evicted
            printf("unhashtag is %lx\n", unHashTag);
            insertBlock(l2cache, unHashTag);
        }
    } else { // not in l1 or l2. Read from mem
        l2cache->cacheMiss++;
        memRead++;
        unsigned long unHashTag = insertBlock(l1Cache, address);
        printf("unhashtag is %lx\n", unHashTag);
        if (unHashTag > 0) { //block was evicted
            insertBlock(l2cache, unHashTag); //in lru dont update
        } 
    }

}

int main(int argc, char** argv) {

    // if (argc < 9) {
    //     printf("Please enter in the inputs");
    //     exit(EXIT_FAILURE);
    // }


    unsigned int l1cacheSize, l1blockSize, l1assocSize, l1setSize;
    unsigned int l2cacheSize, l2blockSize, l2assocSize, l2setSize;

    // char l1policy[10], l2policy[10], file[256];
    // sscanf(argv[1], "%u", &l1cacheSize);
    // sscanf((argv[2] + 6), "%u", &l1assocSize); // Gets 6th element in string. and converts that 
    // sscanf(argv[3], "%s", l1policy);
    // sscanf(argv[4], "%u", &l1blockSize);
    // sscanf(argv[5], "%u", &l2cacheSize);
    // sscanf((argv[6] + 6), "%u", &l2assocSize); // Gets 6th element in string. and converts that 
    // sscanf(argv[7], "%s", l2policy);
    // sscanf(argv[8], "%s", file);

    l1cacheSize = 512;
    l1blockSize = 8;
    l1assocSize = 4;
    l2cacheSize = 512;
    l2assocSize = 64;
    char* l1policy = "fifo";
    char* l2policy = "fifo";
    char* file = "trace.txt";

    l2blockSize = l1blockSize;

    l1setSize = l1cacheSize / (l1blockSize * l1assocSize);
    l2setSize = l2cacheSize / (l2blockSize * l2assocSize);

    Cache* l1Cache = allocateCache(l1setSize, l1assocSize, l1blockSize, l1policy);
    Cache* l2Cache = allocateCache(l2setSize, l2assocSize, l2blockSize, l2policy);

    l1Cache->l2Cache = l2Cache;

    FILE* fp = fopen(file, "r");
    if (fp == NULL) {
        printf("unable to open input file \n");
        exit(EXIT_SUCCESS);
    } 
    
    char operation;
    unsigned long address;
    
    int count = 0;
    while (fscanf(fp, "%c %lx\n", &operation, &address) != EOF) {
        l1l2CacheSearch(l1Cache, address, operation);
        if (count == 75000) {
            count = 0;
        }
    }
    printf("memread:%u\nmemwrite:%u\nl1cachehit:%u\nl1cachemiss:%u\n", memRead, memWrite, l1Cache->cacheHit, l1Cache->cacheMiss);
    printf("l2cachehit:%u\nl2cachemiss:%u\n", l2Cache->cacheHit, l2Cache->cacheMiss);
    
    free(l1Cache);
    free(l2Cache);
    return EXIT_SUCCESS;
}