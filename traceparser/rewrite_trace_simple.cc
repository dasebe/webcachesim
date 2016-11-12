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
  long id, size, told, other;
  long simpleId = 0, t=0;
  unordered_map<long,long> dSimpleId;

  while (infile >> told >> id >> size >> other ) {
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
