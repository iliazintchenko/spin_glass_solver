//
// kwsys
//
#include <kwsys/Process.h>
//
// STL
//
#include <iostream>                                   
#include <iomanip>
#include <fstream>
//
#include <boost/filesystem.hpp>
// 
#include "CommandCapture.h"
//---------------------------------------------------------------------------
std::string getTempFile(const char *name) {
#ifdef _WIN32
  return std::string(::getenv("TEMP")) + std::string("/") + name;
#else
  char myfile[256];
  sprintf(myfile,"/tmp/%s.XXXXXX",name);
  if (mkstemp(myfile)!=-1){
    return myfile;
  } 
  else {
    return "Unable_to_make_a_unique_filenamen";
  }
#endif
}
//---------------------------------------------------------------------------
void DisplayCommand(const std::string &fullcommandline)
{
  std::string blanks(fullcommandline.size()>80?80:fullcommandline.size(),'-');
  std::cout << blanks.c_str() << std::endl << fullcommandline.c_str() << std::endl << blanks.c_str() << std::endl; 
}
//---------------------------------------------------------------------------
void ExecuteAndCapture(std::vector<const char*> &commands, std::vector<std::string> &std_out, double timeout)
{
  commands.push_back(0);
  kwsysProcess* process = kwsysProcess_New();
  kwsysProcess_SetCommand(process, &commands[0]);
  kwsysProcess_SetTimeout(process, timeout);
  std::string PipeFileName = getTempFile("crayviz_Pipe.txt");
  kwsysProcess_SetPipeFile(process, kwsysProcess_Pipe_STDOUT, PipeFileName.c_str());
  //
  // Display the entire command for debug purposes and make it pretty
  //
  std::string fullcommandline;
  for (std::vector<const char*>::iterator it=commands.begin(); it!=commands.end()-1; ++it) fullcommandline += std::string(*it) + " ";
  DisplayCommand(fullcommandline);
  //
  // Execute
  //
  try 
  {
    kwsysProcess_Execute(process);
    double tout = timeout;
    kwsysProcess_WaitForExit(process, &tout);
  }
  catch (...) {}
  kwsysProcess_Delete(process);
  //
  // Collect the generated file and return as list of strings
  //
  int filesize = boost::filesystem::file_size(PipeFileName.c_str());
  char *filebuffer = new char[filesize];
  std::ifstream PipeFile(PipeFileName.c_str(), std::ios::in);
  // scan results
  std::string line;
  int linenumber = 0;
  while (PipeFile.good()) {    
    getline(PipeFile,line);
    std_out.push_back(line);
    std::cout << std::setw(5) << linenumber++ << " : " << line.c_str() << std::endl;
  }
  delete []filebuffer;
  boost::filesystem::remove(PipeFileName.c_str());
}
//---------------------------------------------------------------------------
void ExecuteAndCaptureSSH(std::vector<const char*> &commands, 
                          std::vector<std::string> &std_out, 
                          double timeout, 
                          std::string &plink,
                          std::string &user,
                          std::string &key,
                          std::string &loginnode
                          )
{
  std::vector<const char*> full_command;
#ifdef WIN32
  full_command.push_back(plink.c_str());
  full_command.push_back("-ssh");
  full_command.push_back("-l");
  full_command.push_back(user.c_str());
  full_command.push_back("-i");
  full_command.push_back(key.c_str());
  full_command.push_back(loginnode.c_str());
#else
  full_command.push_back("ssh");
  full_command.push_back(loginnode.c_str());
#endif
  full_command.insert(full_command.end(),commands.begin(),commands.end());
  ExecuteAndCapture(full_command, std_out, timeout);
}
