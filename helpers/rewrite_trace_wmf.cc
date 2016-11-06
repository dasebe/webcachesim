#include <fstream>
#include <unordered_map>
#include <string>
#include <sstream>
#include <unordered_map>
#include<iostream>

using namespace std;

int main (int argc, char* argv[])
{

  // parameters
  if(argc != 3) {
    return 1;
  }

  const char* inputFile = argv[1];
  const char* outputMem = argv[2];

  cout << "running..." << endl;

  ifstream infile(inputFile);
  ofstream outfile(outputMem);
  long id, size, t=0, simpleId=0;
  unordered_map<long,long> dSimpleId;

  const char row_delim = '\n';
  const char field_delim = '\t';
  const char xcache_delim = ' ';
  string row;

  getline(infile, row, row_delim);

  for (; getline(infile, row, row_delim); ) {
    // parse row
    istringstream ss(row);
    string field;
    getline(ss, field, field_delim);
    // get ID
    if(field.empty()) {
      cerr << "empty id " << row << endl;
      continue;
    }
    stringstream fieldstream( field );
    fieldstream >> id;
    int i;
    field.clear();
    for (i=2; i<=4; i++)
      getline(ss, field, field_delim);

    // get size
    if(field.empty()) {
      cerr << "empty size " << row << endl;
      continue;
    }
    stringstream fieldstream2( field );
    fieldstream2 >> size;
    // get cache id
    for (; i<=6; i++)
      getline(ss, field, field_delim);
    istringstream xcache(field);
    for (int j=1; j<=7; j++) {
      field.clear();
      getline(xcache, field, xcache_delim);
    }

    if(field.empty()) {
      //      cerr << "empty xcache " << row << endl;
      continue;
    }

    // match cp4006
    if(field.compare("cp4006") != 0) {
      continue;
    }

    //    cout << id << " " << size << endl;
    if (size <= 1)
      continue;
	
    if(dSimpleId.count(id)==0)
      dSimpleId[id]=simpleId++;
    t++;
    outfile << t << " " << dSimpleId[id] << " " << size << endl;
    
  }
  infile.close();

    cout << "rewrote " << t << " requests" << endl;

  return 0;
}
