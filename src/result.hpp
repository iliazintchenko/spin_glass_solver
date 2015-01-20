#ifndef RESULT_HPP
#define RESULT_HPP

#include <vector>
#include <iostream>

/// a class to store the results of the optimization
struct result
{
  /// the final energy
  double E_;
  
  /// the final spin configuration
  std::vector<int> spins_; // vector<bool> gives horrible performamce hots
};

std::ostream& operator << (std::ostream&, result const&);

#endif
