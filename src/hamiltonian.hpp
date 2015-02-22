#ifndef HAMILTONIAN_HPP
#define HAMILTONIAN_HPP

#include <map>
#include <string>
#include <vector>
#include <unordered_set>
#include <cassert>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

template <class T>
std::string tostring(const std::vector<T>& vec)
{
  std::string str;
  for(const auto a : vec)
    str += std::to_string(a) + " ";

  return str;
}

class hamiltonian_type {
// Stores the Hamiltonian of the spin glass
  typedef std::pair<std::vector<unsigned>,double> edge_type;
  typedef std::vector<edge_type> node_type;

public:

  hamiltonian_type() {};

  // construct with filname containing hamiltonian
  hamiltonian_type(const std::string&);

  // copy constructor
  hamiltonian_type(const hamiltonian_type &other) : nodes_(other.nodes_) {
//    std::cout << "Copy constructor of hamiltonian" << std::endl;
  };

  // for efficient hpx forwarding, a move constructor is preffered
  hamiltonian_type(hamiltonian_type &&other) : nodes_(std::move(other.nodes_)) {
//    std::cout << "Move constructor of hamiltonian" << std::endl;
  };

  std::size_t size() const {return nodes_.size();}

  node_type& operator[](const unsigned i) {return nodes_[i];}
  const node_type& operator[](const unsigned i) const {return nodes_[i];}

  template <typename Archive>
  void serialize(Archive & ar, unsigned)
  {
      ar & nodes_;
  }

private:

  std::vector<node_type> nodes_;
};

#endif
