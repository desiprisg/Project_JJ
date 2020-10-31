#include "CliqueGroup.h"

#include <stdlib.h>
#include <stdio.h>

typedef struct ItemCliquePair {
    void* item;
    List* clique;
}ItemCliquePair;

ItemCliquePair* ItemCliquePair_New(void* item){
    ItemCliquePair* pair = malloc(sizeof(ItemCliquePair)); //value of the KeyValuePair struct

    pair->clique = malloc(sizeof(List)); //malloc list for the clique of the pair
    List_Init(pair->clique); 

    List_Append(pair->clique, item); //append the item to the clique(starting state)

    pair->item = item;

    return pair;
}

void ItemCliquePair_Free(void* value){
    ItemCliquePair* icp = (ItemCliquePair*)value;
    
    List_Destroy((List*)icp->clique);
    List_Free(icp->clique); // THIS BECAME LIST_FREE FROM FREE
    //free(icp->clique); // MISTAKE
    free(icp);
}

void CliqueGroup_Init(CliqueGroup* cg, int bucketSize,unsigned int (*hashFunction)(const void*, unsigned int), bool (*cmpFunction)(void*, void*)){
    Hash_Init(&cg->hash, bucketSize, hashFunction,cmpFunction);
    List_Init(&cg->cliques);
}

bool CliqueGroup_Add(CliqueGroup* cg, void* key, int keySize, void* value){
    if (Hash_GetValue(cg->hash, key, keySize) != NULL){
        return false;
    }

    ItemCliquePair* icp = ItemCliquePair_New(value); //create icp
    Hash_Add(&(cg->hash), key, keySize, icp);

    List_Append(&(cg->cliques), icp->clique); //append the clique into the list of cliques

    return true;
}

void CliqueGroup_Destroy(CliqueGroup cg){
    Hash_FreeValues(cg.hash, ItemCliquePair_Free);
    Hash_Destroy(cg.hash);
    List_FreeValues(cg.cliques, free); // THIS BECAME FREE FROM LIST_FREE
    List_Destroy(&(cg.cliques));
}

void CliqueGroup_FreeValues(CliqueGroup cg, void (*subFree)(void*)){
    Node* tempNode = cg.cliques.head;
    while (tempNode != NULL){
        List_FreeValues(*(List*)(tempNode->value), subFree);
        tempNode = tempNode->next;
    }
}
