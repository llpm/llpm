#include "linear_algebra_accel.hpp"

unsigned LinearAlg::capacity() {
    return CAPACITY;
}

bool LinearAlg::resetA(unsigned w, unsigned h) {
    if (w*h > CAPACITY)
        return false;
    posA = 0;
    widthA = w;
    heightA = h;
    return true;
}
bool LinearAlg::pushA(uint64_t v) {
    if (posA > widthA*heightA)
        return false;
    matrixA[posA] = v;
    posA++;
    return true;
}

bool LinearAlg::resetB(unsigned w, unsigned h) {
    if (w*h > CAPACITY)
        return false;
    posB = 0;
    widthB = w;
    heightB = h;
    return true;
}
bool LinearAlg::pushB(uint64_t v) {
    if (posB > widthB*heightB)
        return false;
    matrixB[posB] = v;
    posB++;
    return true;
}

unsigned LinearAlg::resetC() {
    if (!validC)
        return -1;
    posC = 0;
    return widthC * heightC;
}
uint64_t LinearAlg::iterC() {
    if (posC > widthC * heightC)
        return -1;
    return matrixC[posC++];
}

// TODO: Is this correct? I'm both a moron and tired right now.
bool LinearAlg::multiply() {
    if (widthA != heightB)
        return false;
    if (heightA * widthB > CAPACITY)
        return false;

    heightC = heightA;
    widthC = widthB;

    for (unsigned i=0; i<heightA; i++) {
        for (unsigned j=0; j<widthB; j++) {
            uint64_t sum = 0;
            for (unsigned k=0; k<widthA; k++) {
                sum += matrixA[i + (k*heightA)] * 
                       matrixB[k + (j*heightB)];
            }
            matrixC[i + (j*heightC)] = sum;
        }
    }
    return true;
}

bool LinearAlg::add() {
    if (widthA != widthB ||
        heightA != heightB)
        return false;

    widthC = widthA;
    heightC = heightA;
    validC = true;

    for (unsigned i=0; i<widthA; i++) {
        for (unsigned j=0; j<heightA; j++) {
            unsigned idx = i + (j*heightC);
            matrixC[idx] = matrixA[idx] + matrixB[idx];
        }
    }
    return true;
}
