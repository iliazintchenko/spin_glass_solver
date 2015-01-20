#include "result.hpp"
#include <iterator>

std::ostream& operator << (std::ostream& os, result const& res)
{
  os << res.E_ << ' ';
  std::copy(res.spins_.begin(),res.spins_.end(),std::ostream_iterator<int>(os));
  os << std::endl;
  return os;
}

