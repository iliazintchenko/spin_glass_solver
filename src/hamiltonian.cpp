#include "hamiltonian.hpp"

hamiltonian_type::hamiltonian_type(const std::string& file_name)
{
  std::unordered_set<std::string> edge_set;

  std::ifstream in(file_name);
  assert(in);

  std::size_t N(0);
  std::map<std::string,unsigned> index;

  std::size_t count(0);
  std::vector<edge_type> edges;
  while(in){

    std::string input_str;

    if(!std::getline(in,input_str)) break;

    if(input_str.find(' ') != std::string::npos && input_str.find('#') == std::string::npos){

      std::istringstream tmp0(input_str);
      std::vector<std::string> input;
      while (tmp0){
        std::string s;
        if (!std::getline(tmp0, s, ' ')) break;
        input.push_back(s);
      }

      const double val(std::stod(input[input.size()-1]));

      if(val != 0.0){

        edge_type edge;

        edge.second = val;

        for(unsigned i = 0; i < input.size()-1; ++i){
          const std::string site(input[i]);
          if(index.find(site) == index.end())
            index[site] = N++;
          edge.first.push_back(index[site]);
        }

        std::sort(edge.first.begin(),edge.first.end(),std::less<unsigned>());

        const std::string mark(tostring(edge.first));

        if(edge_set.find(mark) == edge_set.end()){
          edges.push_back(edge);
          edge_set.insert(mark);
        }
        else
          std::cout << "warning: duplicate edge (" << input_str << ")   line: " << count+1 << std::endl;
      }
    }

    ++count;
  }
  in.close();

  nodes_.resize(N);

  std::vector<std::unordered_set<std::string> > edge_sets(N);

  for(const auto& edge : edges){

    const std::string mark(tostring(edge.first));

    for(const auto a : edge.first)
      if(edge_sets[a].find(mark) == edge_sets[a].end()){
        nodes_[a].push_back(edge);
        edge_sets[a].insert(mark);
      }
  }
}

