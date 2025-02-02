#include "LogisticRegression.h"
#include "Util.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

#include "JobScheduler.h"

extern JobScheduler jobScheduler;

double EuclideanDistance(double* x, double* y, unsigned int size){
    double sum = 0;
    for (int i = 0; i < size; i++){
        sum += (x[i] - y[i]) * (x[i] - y[i]);
    }

    return sqrt(sum);
}

double LinearFunction(double* weights, double bWeight, Hash* leftVector, Hash* rightVector, int dimension){
    double result = bWeight;

    Node* leftNode = leftVector->keyValuePairs.head;
    while(leftNode != NULL){
        KeyValuePair* kvp = (KeyValuePair*)leftNode->value;
        result += weights[*(int*)kvp->key] * *(double*)(kvp->value);

        leftNode = leftNode->next;
    }

    Node* rightNode = rightVector->keyValuePairs.head;
    while(rightNode != NULL){
        KeyValuePair* kvp = (KeyValuePair*)rightNode->value;
        result += weights[dimension/2 + *(int*)kvp->key] * *(double*)(kvp->value);

        rightNode = rightNode->next;
    }

    return result;
}

double SigmoidFunction(double x){
    double val = 1.0 / (1.0 + exp(-x));
    return val;
}

double PartialDerivative(LogisticRegression* model, unsigned int** xIndexes, double* yVals, unsigned int height, int derivativeIndex, int j, int batchSize){
    double result = 0;
    
    int limit = j + batchSize;
    if (limit > height){
        limit = height;
    }

    for(int k = j; k < limit; k++){
        Hash *leftVector = &model->xVals[xIndexes[k][0]]; //Hash1
        Hash *rightVector = &model->xVals[xIndexes[k][1]]; //Hash2
        double xij;
        double* tfidf;
        if(derivativeIndex < model->width / 2){
            tfidf = Hash_GetValue(*leftVector, &derivativeIndex, sizeof(derivativeIndex));
        }else{
            int newDerivativeIndex = derivativeIndex - model->width/2;
            tfidf = Hash_GetValue(*rightVector, &newDerivativeIndex, sizeof(newDerivativeIndex));
        }

        if(tfidf){
            xij = *tfidf;
        }else{
            xij = 0;
        }

        if(xij > 0){
            result += (SigmoidFunction(LinearFunction(model->weights, model->bWeight, leftVector, rightVector, model->width)) - yVals[k]) * xij;
        }
    }

    return result;
}

void GradientVector(LogisticRegression* model,unsigned int** xIndexes, double* yVals, unsigned int height, double* vector, int j, int batchSize){
    for (int i = 0; i < model->width; i++) {
        vector[i] = PartialDerivative(model, xIndexes, yVals, height, i, j, batchSize);
        //printf("Partial derivative (%d) : %.14f\n", i, vector[i]);
    }
}

void LogisticRegression_Init(LogisticRegression* model, double bWeight, Hash* xVals, unsigned int width, unsigned int itemCount){
    model->bWeight = bWeight;

    model->xVals = xVals;

    model->itemCount = itemCount;
    model->width = width;

    //setting the starting weights
    model->weights = calloc(width, sizeof(double));
    for (int i = 0; i < width; i++){
        model->weights[i] = 1.0;
    }
}

void LogisticRegression_Destroy(LogisticRegression model){
    free(model.weights);
}

void* CalculateGradient(void** args){
    LogisticRegression* model = args[0];
    unsigned int** xIndexes = args[1];
    double* yVals = args[2];
    unsigned int* height = args[3];
    double* subGradient = args[4];
    int* startIndx = args[5];
    int* batchSize = args[6];

    GradientVector(model, xIndexes, yVals, *height, subGradient, *startIndx, *batchSize);

    free(startIndx);
    free(batchSize);
    free(args);

    return NULL; // Changed already made into the sub gradient arrays.
}

double* LogisticRegression_Train(LogisticRegression *model,unsigned int** xIndexes, double* yVals, unsigned int height, double learningRate, int epochs, int batchSize) {
    double* newW = malloc(model->width * sizeof(double));
    double* gradientVector = malloc(model->width * sizeof(double));
    double* previousEpochWeights = calloc(model->width, sizeof(double));

    int batches = (int)height / batchSize;
    if (height % batchSize != 0){
        batches++;
    }

    int jobsPerUpdate = JOBS_PER_UPDATE; //jobScheduler.numberOfThreads;

    double** subGradients = malloc(jobsPerUpdate * sizeof(double*));
    for (int l = 0; l < jobsPerUpdate; ++l) {
        subGradients[l] = calloc(model->width, sizeof(double));
    }

    bool earlyStop = false;
    int epochsCompleted = 0;

    for(int k = 0; k < epochs; k++) {
        int m = 0;
        while (m < batches) {
            int numberOfJobs = jobsPerUpdate;
            if(numberOfJobs >= batches - m){
                numberOfJobs = batches - m;
            }

            // Calculate #threads sub-weights for each batch of train batches.
            for (int j = 0; j < numberOfJobs; j++) {

                // Creating the thread args.
                Job* newJob = malloc(sizeof(Job));
                void** args = malloc(7 * sizeof(void*));

                int* startIndx = malloc(sizeof(int));
                *startIndx = m++ * batchSize;

                int* batchSizePtr = malloc(sizeof(int));
                *batchSizePtr = batchSize;

                args[0] = model;
                args[1] = xIndexes;
                args[2] = yVals;
                args[3] = &height;
                args[4] = subGradients[j];
                args[5] = startIndx;
                args[6] = batchSizePtr;

                // Pushing the job to the thread pool queue.
                Job_Init(newJob, CalculateGradient, NULL, args);
                JobScheduler_AddJob(&jobScheduler, newJob);
            }

            // Sync threads to calculate new mean gradient.
            JobScheduler_WaitForJobs(&jobScheduler, numberOfJobs);
            // Empty result queue since we dont require the NULL results.
            while(jobScheduler.results.head != NULL){
                free(Queue_Pop(&jobScheduler.results));
            }

            for (int l = 0; l < model->width; ++l) {
                double sum = 0;
                for (int i = 0; i < numberOfJobs; ++i) {
                    sum += subGradients[i][l];
                }
                gradientVector[l] = (double)sum / (double)numberOfJobs;
            }

            for (int i = 0; i < model->width; ++i) {
                newW[i] = model->weights[i] - learningRate * gradientVector[i];
            }


            double *temp = model->weights;
            model->weights = newW;
            newW = temp;
        }

        //stop if euclidean norm of new vs old weights is small enough
        double euclideanWeightDist = 0.0;
        if (k > 0){
            euclideanWeightDist =  EuclideanDistance(model->weights, previousEpochWeights, model->width);
            if (euclideanWeightDist < EARLY_STOP_EUCLIDEAN_DISTANCE){
                earlyStop = true;
            }
        }

        for (int i = 0; i < model->width; ++i) {
            previousEpochWeights[i] = model->weights[i];
        }

        epochsCompleted++;

        printf("\r%d out of %d epochs, Dist is %.15f", k+1, epochs, euclideanWeightDist);
        fflush(stdout);

        if(earlyStop) break;
    }

    free(gradientVector);
    free(newW);
    free(previousEpochWeights);

    for (int l = 0; l < jobsPerUpdate; ++l) {
        free(subGradients[l]);
    }
    free(subGradients);

    printf("\rTraining completed with %d epochs                           \n\n", epochsCompleted);

    return model->weights;
}

double LogisticRegression_Predict(LogisticRegression* model, Hash* leftVector, Hash* rightVector){
    return SigmoidFunction(LinearFunction(model->weights, model->bWeight, leftVector, rightVector, model->width));
}