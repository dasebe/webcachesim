#include <cstddef>
#include <stdexcept>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <random>
#include <list>
#include <iomanip>

using namespace std;

typedef tuple<long double, unsigned long> fi_pair_t;
typedef list<fi_pair_t>::iterator list_iterator_t;

// inversion method for bounded Pareto
// uniform sample us, shape a (alpha), lower bound l, upper bound h
double rbpareto(double us, double a, double l, double h)
{
  //  return(pow((pow(l, a) / (us*pow((l/h), a) - us + 1)), (1.0/a)));
  return( l/ pow( 1+us*(pow(l/h,a)-1), 1.0/a) );
}

int main (int argc, char* argv[])
{
  // parameters
  if(argc!=7) {
    cout << "\n number_of_objects repetition_count pareto_shape lower_pareto_bound higher_pareto_bound outputname\n";
    return 1;
  }
  const long no_objs = atoi(argv[1]);
  const long reps = atoi(argv[2]);
  const double shape = atof(argv[3]);
  const double lowerb = atof(argv[4]);
  const double higherb = atof(argv[5]);
  const string outputname(argv[6]);

  auto * size = new long[no_objs];
  list<fi_pair_t> reqseq;

  // initialize object sizes
  random_device rd;
  mt19937 rnd_gen (rd ());
  double mean_size=0.0;
  uniform_real_distribution<> urng(0, 1);
  for (long i = 0; i < no_objs; i++) {
    double us = urng(rnd_gen);
    do
      {
	us = urng(rnd_gen);
      }
    while ((us == 0) || (us == 1));
    do
    {
        size[i]=rbpareto(us,shape,lowerb,higherb);
    }
    while (size[i]<lowerb || size[i]>higherb);
    mean_size+=size[i];
  }
  cout << "finished sizes. mean_size: " << mean_size/static_cast<double>(no_objs) << "\n";

  // initialize req sequence
  for (long i = 0; i < no_objs; i++) {
    const long double rateH = 1/(pow(i+1,0.9));
    exponential_distribution<> iaRandH (rateH);
    long double globalTime = iaRandH(rnd_gen);
    while(globalTime<reps) {
      reqseq.push_back(fi_pair_t(globalTime,i));
      globalTime += iaRandH(rnd_gen);
    }
    //    cout << "markov high " << testh/testhc << " number " << testhc << " markov low " << testl/testlc << " number " << testlc << " changed states " << testchange << "\n";
  }
  // sort tuples lexicographically, i.e., by time
  cout << "finished raw req sequence.\n";
  reqseq.sort();
  cout << "finished sorting req.\n";

  ofstream outfile;
  outfile.open(outputname);
  outfile<<fixed<<setprecision(0); // turn of scientific notation

  // output request sequence
  list_iterator_t rit;
  for (rit=reqseq.begin(); rit != reqseq.end(); ++rit) {
    outfile << 1000*get<0>(*rit) << " " << get<1>(*rit) << " " << size[get<1>(*rit)] << "\n"; 
  }

  cout << "finished output.\n";
  outfile.close();

  delete size;

  return 0;
}
