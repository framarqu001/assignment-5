
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

unsigned int memRead = 0; 
unsigned int memWrite = 0;;

typedef struct {
    unsigned int metaData;
    unsigned long int tag;
} Block;

typedef struct {
    unsigned int metaCount;
    bool filled;
    unsigned int count;
    Block** blocks;
} Set;

typedef struct {
    unsigned int setSize;
    unsigned int assocSize;
    unsigned int blockSize;
    unsigned int cacheHit;
    unsigned int cacheMiss;
    char* policy;
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

unsigned int hashAddress(Cache* cache, unsigned long* address) {
    unsigned int blockBitSize = logBase2(cache->blockSize);
    // unsigned int extractBlockOffset = 1 << blockBitSize;
    // extractBlockOffset = extractBlockOffset - 1;
    // unsigned int blockOffset = *address & extractBlockOffset; // delete these liens later dont need for this
    *address = *address >> blockBitSize;

    unsigned int setBitSize = logBase2(cache->setSize);
    unsigned int extractSetIndex = 1 << setBitSize;
    extractSetIndex = extractSetIndex - 1;
    unsigned int setIndex = *address & extractSetIndex;
    *address = *address >> setBitSize;
    return setIndex;

}

bool setSearch(unsigned long tag, Set* set, unsigned int n, char* policy) {
    for (int i = 0; i < n; i++) {
        if (set->blocks[i]->tag == tag) {
            if (strcmp(policy, "lru") == 0) { // if LRU we increment the block count to show its been used recently.
               set->blocks[i]->metaData = set->metaCount++; 
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

void insertBlock(Cache* cache, unsigned long address, unsigned int index) {
    Set* set = cache->sets[index];
    unsigned int count = set->count;
    if (set->filled == false) { // If set not filled we just add block to next index
        Block* block = set->blocks[count];
        block->tag = address;
        set->count++;
        if (set->count == cache->assocSize) {
            set->filled = true;
        }
        block->metaData = set->metaCount++; //Every block insert increments the setMetaCount and the blockmetacount. Block metacount is used to kick                                
    } else { //Set is filled so we kick the block with the lowest metadata counter. Blocks metadata counters are incremented based on replacement policy.
        unsigned int bIndex = minBlockSearch(set); 
        Block* block = set->blocks[bIndex];
        block->tag = address;
        block->metaData = set->metaCount++;
    }
    
}

bool cacheSearch(Cache* cache, unsigned long address, char operation) {
    unsigned setIndex = hashAddress(cache, &address);
    Set* set = cache->sets[setIndex];
    unsigned int n = cache->assocSize;
    char* policy = cache->policy;

    
    if (setSearch(address, set, n, policy) == true) { // If address is in set then hit.
        cache->cacheHit++;
    } else { // else miss. Insert block into cache
        cache->cacheMiss++;
        memRead++;
        insertBlock(cache, address, setIndex);
    }

    if (operation == 'W') { // if its a write we also write back to mem. Write through.
            memWrite++;
        }
    
    // printCache(cache);
    return false;
}


int main(int argc, char** argv) {

    if (argc < 5) {
        printf("Please enter in the inputs");
        exit(EXIT_FAILURE);
    }


    unsigned int cacheSize, blockSize, assocSize, setSize;
    char policy[10], file[256];

    sscanf(argv[1], "%u", &cacheSize);
    sscanf((argv[2] + 6), "%u", &assocSize); // Gets 6th element in string. and converts that 
    sscanf(argv[3], "%s", policy);
    sscanf(argv[4], "%u", &blockSize);
    sscanf(argv[5], "%s", file);
    // cacheSize = 512;
    // blockSize =16;
    // assocSize = 1;
    // char* policy = "fifo";
    // char* file = "trace.txt";

    setSize = cacheSize / (blockSize * assocSize);



    FILE* fp = fopen(file, "r");
    if (fp == NULL) {
        printf("unable to open input file \n");
        exit(EXIT_SUCCESS);
    } 
    
    char operation;
    unsigned long address;
    Cache* cache = allocateCache(setSize, assocSize, blockSize, policy);
    
    
    
    while (fscanf(fp, "%c %lx\n", &operation, &address) != EOF) {
        cacheSearch(cache, address, operation);
    }
    printf("memread:%u\nmemwrite:%u\ncachehit:%u\ncachemiss:%u", memRead, memWrite, cache->cacheHit, cache->cacheMiss);

    
    free(cache);

    return EXIT_SUCCESS;
}