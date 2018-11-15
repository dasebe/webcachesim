//
// Created by zhenyus on 11/14/18.
//
#include "pickset.h"
#include "utils.h"
#include <iostream>

using namespace std;

PickSet<pair<uint64_t , uint64_t >> myset;

void myfunc() {
    myset.insert({1,1});
}

int main(int argc, char ** argv) {
    myfunc();
    cout<<(myset.exist({1,1}))<<endl;
    return 0;
}
