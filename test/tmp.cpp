//
// Created by zhenyus on 11/16/18.
//

#include <iostream>
#include <string>
#include <boost/bimap.hpp>
#include <boost/bimap/list_of.hpp>

int main()
{
    using namespace std;

    using namespace boost::bimaps;
    bimap<set_of<std::pair<uint64_t, uint64_t>>, list_of<uint64_t>> myMap2;
    myMap2.left[std::make_pair<uint64_t, uint64_t>(1,1)] = 3;
    myMap2.left[std::make_pair<uint64_t, uint64_t>(1,1)] = 1;
    myMap2.left[std::make_pair<uint64_t, uint64_t>(1,1)] = 2;
    myMap2.left[std::make_pair<uint64_t, uint64_t>(1,1)] = 4;
    myMap2.left[std::make_pair<uint64_t, uint64_t>(1,1)] = 5;
    myMap2.left[std::make_pair<uint64_t, uint64_t>(1,1)] = 0;
    // myMap2.left["key2"] = 3;

    // for (auto&& elem : myMap2.left)
    // std::cout << "{"  << ", " << elem.second << "}, ";
    // std::cout << "\n";

    // auto res1 = myMap2.left.find("key1");
    // std::cout << "{" << res1->first << ", " << res1->second << "} \n";

    auto it = myMap2.right.begin();
    std::cout<<it->first<<" "<<(it->second).first<<endl;
}
