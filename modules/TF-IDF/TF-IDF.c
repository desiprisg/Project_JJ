#include "TF-IDF.h"

#include <assert.h>
#include <math.h>

#include "Util.h"
#include "Hashes.h"
#include "StringUtil.h"
#include "Tuple.h"

void UpdateUniqueWordsFromICP(ItemCliquePair icp, Hash* dictionary, Hash processedWords){
    /* Increment a counter in the global dictionary (Or creates it if needed) for every unique
     * word in the icp. Helps with finding how many times a word appears in the icps
     * To be used in the global dictionary. */

    //Assisting Hash to not increment counters 2 times for the same words, prevents duplicating
    Hash currCliqueWordsInserted;
    Hash_Init(&currCliqueWordsInserted,DEFAULT_HASH_SIZE,RSHash,StringCmp);

    //Get the list of words for the specific icp
    List* words = Hash_GetValue(processedWords, &icp.id, sizeof(icp.id));

    //For each word in the list
    Node* currWordNode = words->head;
    while(currWordNode != NULL){
        char* currWord = currWordNode->value;
        int keySize = (int)strlen(currWord) + 1;

        //If it has not yet been counted for this word list(meaning it is included 2 or more times in the list)
        if(!Hash_GetValue(currCliqueWordsInserted,currWord,keySize)){
            double* count = Hash_GetValue(*dictionary,currWord,keySize);
            //If it is has not been found in any other icps, malloc it and increment
            if(!count){
                count = malloc(sizeof(double));
                *count = 1;
                Hash_Add(dictionary, currWord, keySize, count);
            }else{ //just increment
                (*count)++;
            }

            //Add it to assisting hash so we will not increment it again(uniquely increment word counts)
            Hash_Add(&currCliqueWordsInserted,currWord,keySize,"-");
        }

        currWordNode = currWordNode->next;
    }

    Hash_Destroy(currCliqueWordsInserted);
}

Hash CreateDictionary(CliqueGroup group, Hash processedWords){
    /* Creates a dictionary of all the unique words for every item
     * Key : word , Value : in how many files the word appeared.
     * If a word appears twice in an isp it will only be counted once. */

    Hash dictionary;
    Hash_Init(&dictionary,DEFAULT_HASH_SIZE,RSHash,StringCmp);

    //Insert unique words to dict for each icp correlated to the clique
    Node* currCliqueNode = group.cliques.head;
    while(currCliqueNode != NULL){
        Clique* currClique = (Clique*)currCliqueNode->value;
        Node* currIcpNode = currClique->similar.head;

        UpdateUniqueWordsFromICP(*(ItemCliquePair *) currIcpNode->value, &dictionary, processedWords);

        currCliqueNode = currCliqueNode->next;
    }

    return dictionary;
}

int IDF_Index_Cmp(const void* value1, const void* value2){
    KeyValuePair* kvp1 = *(KeyValuePair**)value1;
    KeyValuePair* kvp2 = *(KeyValuePair**)value2;

    double idf1 = *(double*)(kvp1->value);
    double idf2 = *(double*)(kvp2->value);

    if (idf1 > idf2){
        return -1;
    }else if(idf1 < idf2){
        return 1;
    }else{
        return 0;
    }
}

Hash TF_Trim(Hash dictionary, Hash averageTFIDF, int dimensionLimit){
    // Trims dictionary

    // Convert List to Array
    KeyValuePair** kvpArray = (KeyValuePair**)List_ToArray(averageTFIDF.keyValuePairs);

    //Sorting the array based on IDF values(descending)
    qsort(kvpArray, averageTFIDF.keyValuePairs.size, sizeof(KeyValuePair*), IDF_Index_Cmp);
    for (int j = 0; j < 100; ++j) {
        //printf("%s\n",kvpArray[j]->key); TODO:
    }

    //Now Trim
    Hash trimmedDictionary;
    Hash_Init(&trimmedDictionary, DEFAULT_HASH_SIZE, RSHash, StringCmp);

    if (dimensionLimit >= averageTFIDF.keyValuePairs.size || dimensionLimit == -1){
        dimensionLimit = averageTFIDF.keyValuePairs.size;
    }

    for (int i = 0; i < dimensionLimit; i++){
        int keySize = strlen(kvpArray[i]->key) + 1;
        double* oldIDF = Hash_GetValue(dictionary, kvpArray[i]->key, keySize);

        double* newIDFValue = malloc(sizeof(double));
        *newIDFValue = *(double*)oldIDF;
        Hash_Add(&trimmedDictionary, kvpArray[i]->key, keySize, newIDFValue);
    }

    free(kvpArray);

    return trimmedDictionary;
}

Hash IDF_Calculate(CliqueGroup cliqueGroup, Hash proccesedWords, int dimensionLimit) {
    // Calculated IDF value. The dictionary should contain values that correspond to the number
    // of times a word appeared uniquely in each item.

    Hash dictionary = CreateDictionary(cliqueGroup, proccesedWords);

    List items = CliqueGroup_GetAllItems(cliqueGroup);

    Node* currWordCountNode  = dictionary.keyValuePairs.head;
    while(currWordCountNode != NULL){
        double* val = ((KeyValuePair*)currWordCountNode->value)->value;
        *val = log(items.size / *val);

        currWordCountNode = currWordCountNode->next;
    }

    // Remove small frequencies for smaller dimensionality.
    Hash newAverageDictionary;
    Hash_Init(&newAverageDictionary, DEFAULT_HASH_SIZE, RSHash, StringCmp);

    Hash* xVals = CreateX(items, dictionary, proccesedWords);
    for (int i = 0; i < items.size; ++i) {
        Hash* currVector = &xVals[i];
        Node* currTFIDFNode = currVector->keyValuePairs.head;

        while(currTFIDFNode != NULL){
            KeyValuePair* kvp = currTFIDFNode->value;
            char* word = kvp->key;
            double currVectorTFIDF = *(double*)kvp->value;
            int keySize = strlen(word) + 1;

            double* averageTFIDF = Hash_GetValue(newAverageDictionary, word, keySize);
            if(averageTFIDF){
                *averageTFIDF += currVectorTFIDF;
            }else{
                double* newTFIDF = malloc(sizeof(double));
                *newTFIDF = currVectorTFIDF;
                Hash_Add(&newAverageDictionary, word, keySize, newTFIDF);
            }

            currTFIDFNode = currTFIDFNode->next;
        }

        Hash_FreeValues(xVals[i], free);
        Hash_Destroy(xVals[i]);
    }

    Hash newDictionary = TF_Trim(dictionary, newAverageDictionary, dimensionLimit);

    free(xVals);
    List_Destroy(&items);

    // Clean up old dictionary.
    Hash_FreeValues(dictionary, free);
    Hash_Destroy(dictionary);

    // Clean up average values.
    Hash_FreeValues(newAverageDictionary, free);
    Hash_Destroy(newAverageDictionary);

    return newDictionary;
}

Hash TF_IDF_Calculate(Hash dictionary, List processedWords){
    /* Calculates a tfidf vector for said Word list based on
     * the given dictionary */

    Hash vector;
    Hash_Init(&vector, DEFAULT_HASH_SIZE, RSHash, StringCmp);

    //Calculating BoW
    //Dictionary Hash has the indexes as values that are used in the vector
    Node* currWordNode = processedWords.head;
    while(currWordNode != NULL){
        int keySize = strlen(currWordNode->value)+1;
        double* idfValue = Hash_GetValue(dictionary, currWordNode->value, keySize);

        //if word is in dictionary
        if (idfValue){
            double* oldBowValue = Hash_GetValue(vector, idfValue, sizeof(double));
            if(oldBowValue){
                *oldBowValue = *oldBowValue + 1;
            }else{
                double* newBowValue = malloc(sizeof(double));
                *newBowValue = 1;
                Hash_Add(&vector, currWordNode->value, keySize, newBowValue);
            }
        }

        currWordNode = currWordNode->next;
    }

    //Calculate TF-IDF for every number in vector
    Node* currKVPNode = vector.keyValuePairs.head;
    while(currKVPNode != NULL){
        KeyValuePair* kvp = currKVPNode->value;

        char* word = kvp->key;
        double* bow = kvp->value;
        double* idf = Hash_GetValue(dictionary, word, strlen(word) + 1);
        *bow = *idf * (*bow / processedWords.size);

        currKVPNode = currKVPNode->next;
    }

    return vector;
}

Hash* CreateX(List xVals, Hash dictionary, Hash itemProcessedWords){
    unsigned int height = (unsigned int)xVals.size;

    //Malloc arrays for X and Y data
    Hash* vectors = malloc(height * sizeof(Hash));

    int index = 0;
    Node* currItemNode = xVals.head;
    while (currItemNode != NULL){
        ItemCliquePair* icp = currItemNode->value;
        List* processedWords = Hash_GetValue(itemProcessedWords, &icp->id, sizeof(icp->id));

        //X
        vectors[index] = TF_IDF_Calculate(dictionary, *processedWords);

        index++;
        currItemNode = currItemNode->next;
    }

    return vectors;
}

double* CreateY(List pairs){
    unsigned int height = (unsigned int)pairs.size;
    double* results = malloc(height * sizeof(double));

    int index = 0;
    Node* currPairNode = pairs.head;
    while (currPairNode != NULL){
        Tuple* currTuple = currPairNode->value;
        ItemCliquePair* icp1 = currTuple->value1;
        ItemCliquePair* icp2 = currTuple->value2;

        //Y
        results[index] = (icp1->clique->id == icp2->clique->id) ?  1.0 :  0.0;

        index++;
        currPairNode = currPairNode->next;
    }

    return results;
}

double* TF_IDF_ToArray(Hash hash, Hash dictionary){
    // Allocates values.
    double* array = malloc(dictionary.keyValuePairs.size * sizeof(double));
    int iter = 0;

    Node* currNode = dictionary.keyValuePairs.head;
    while(currNode != NULL){
        KeyValuePair* kvp = currNode->value;

        double* tfidf = Hash_GetValue(hash, kvp->key , strlen(kvp->key) + 1);
        if(tfidf){
            array[iter] = *(double*)kvp->value;
        }else{
            array[iter] = 0.0;
        }

        currNode = currNode->next;
        iter++;
    }

    return array;
}