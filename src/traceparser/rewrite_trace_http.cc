#include <fstream>
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
  string line[13];
  long size, simpleId = 0, t=0;
  unordered_map<string,long> dSimpleId;

  const char row_delim = '\n';
  const char field_delim = ' ';
  string row;

  getline(infile, row, row_delim);

  for (; getline(infile, row, row_delim); ) {
    // parse row
    istringstream ss(row);
    string id1;
    string id2;
    // 2nd and 3rd field hold id
    getline(ss, id1, field_delim);
    getline(ss, id1, field_delim);
    getline(ss, id2, field_delim);
    string id= id1+id2;

    string field;
    for (int i=4; i<=10; i++)
      getline(ss, field, field_delim);
    stringstream fieldstream( field );
    fieldstream >> size;

    if (size < 1)
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
