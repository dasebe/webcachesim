#include <fstream>
#include <string>
#include <sstream>
#include <unordered_map>
#include<iostream>
#include <vector>

using namespace std;

int main (int argc, char* argv[])
{

  // parameters
  if(argc < 3) {
    return 1;
  }

  const char* outputFile = argv[1];
  vector<string> inputFiles;
  int i;
  for(i=2; i<argc; i++)
    inputFiles.push_back(argv[i]);
  cout << "working with " << i-2 << " traces" << endl;

  ofstream outfile(outputFile);
  long id, size, t=0, simpleId=0;
  unordered_map<long,long> dSimpleId;

  const char row_delim = '\n';
  const char field_delim = '\t';
  const char xcache_delim = ' ';
  string row;

  for(auto it: inputFiles) {
    ifstream infile(it);

    while(getline(infile, row, row_delim)) {
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
      if (size < 1)
	continue;
	
      if(dSimpleId.count(id)==0)
	dSimpleId[id]=simpleId++;
      t++;
      outfile << t << " " << dSimpleId[id] << " " << size << endl;
    }
    infile.close();
  }


  cout << "rewrote " << t << " requests" << endl;

  return 0;
}
