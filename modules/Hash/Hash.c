#include "Hash.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>


KeyValuePair* KeyValuePair_New(void* key, int size, void* value, bool shouldMallocKey){
    KeyValuePair* kvp = malloc(sizeof(KeyValuePair));

    if(shouldMallocKey) {
        kvp->key = malloc(size * sizeof(char));
        memcpy(kvp->key, key, size * sizeof(char));
    }else{
        kvp->key = key;
    }
    kvp->value = value;

    return kvp;
}

void Hash_Init(Hash* hash, int bucketSize, unsigned int (*hashFunction)(const void*, unsigned int), bool (*cmpFunction)(void*, void*), bool shouldMallocKeys) {
    hash->bucketSize = bucketSize; // size of the array.
    hash->hashFunction = hashFunction; // used in get.
    hash->cmpFunction = cmpFunction; // used in get.
    hash->buckets = malloc(bucketSize * sizeof(List));
    hash->shouldMallocKeys = shouldMallocKeys;
    List_Init(&hash->keyValuePairs);

    for(int i = 0; i < bucketSize; i++)
        List_Init(&hash->buckets[i]);

}

void* Hash_GetValue(Hash hash, void* key, int keySize){
    unsigned int index = hash.hashFunction(key,keySize) % hash.bucketSize;
    List bucketItems = hash.buckets[index];

    Node* currNode = bucketItems.head;
    while(currNode != NULL){
        KeyValuePair* pair = (KeyValuePair*)currNode->value;
        if(hash.cmpFunction(key,pair->key)){
            return pair->value;
        }

        currNode = currNode->next;
    }
    
    return NULL;
}

bool Hash_Add(Hash* hash, void* key, int keySize, void* value){
    void* val = Hash_GetValue(*hash, key, keySize);
    if(val != NULL)
        return false;

    unsigned int index = hash->hashFunction(key,keySize) % hash->bucketSize;
    List *bucketItems = &(hash->buckets[index]);

    KeyValuePair *kvp = KeyValuePair_New(key, keySize, value, hash->shouldMallocKeys);
    List_Append(bucketItems, kvp);

    List_Append(&hash->keyValuePairs,kvp);

    return true;
}

void KeyValuePair_Free_WithKey(void* value){
    KeyValuePair* kvp = (KeyValuePair*)value;
    free(kvp->key);
    free(value);
}

void Hash_Destroy(Hash hash){
    for(int i = 0; i < hash.bucketSize; i++){
        List* list =  &(hash.buckets[i]);
        if(hash.shouldMallocKeys)
            List_FreeValues(*list, KeyValuePair_Free_WithKey);
        else
            List_FreeValues(*list, free);
        List_Destroy(list);
    }

    List_Destroy(&hash.keyValuePairs);

    free(hash.buckets);
}

void Hash_FreeValues(Hash hash, void (*freeMethod)(void*)){
    for(int i = 0; i < hash.bucketSize; i++){
        List* list =  &(hash.buckets[i]);
        Node* tempNode = list->head;
        while (tempNode != NULL){
            KeyValuePair* kvp = (KeyValuePair*)(tempNode->value);
            freeMethod(kvp->value);

            tempNode = tempNode->next;
        }
    }
}