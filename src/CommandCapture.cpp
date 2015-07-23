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
// utility display function
void DisplayCommand(const std::string &fullcommandline)
{
  std::string blanks(fullcommandline.size()>80?80:fullcommandline.size(),'-');
  std::cout << blanks.c_str() << std::endl << fullcommandline.c_str() << std::endl << blanks.c_str() << std::endl; 
}
//---------------------------------------------------------------------------
std::vector<std::string> ExecuteAndCapture(const std::vector<std::string> &commands, double timeout, bool verbose)
{
  std::vector<std::string> std_out;
  std::vector<const char*> commands_with_null_term;
  std::for_each(commands.begin(), commands.end(), [&](const std::string &piece){ commands_with_null_term.push_back(piece.c_str()); });
  commands_with_null_term.push_back(0);
  kwsysProcess* process = kwsysProcess_New();
  kwsysProcess_SetCommand(process, &commands_with_null_term[0]);
  kwsysProcess_SetTimeout(process, timeout);
  std::string PipeFileName = getTempFile("kwsys_tempfile.txt");
  kwsysProcess_SetPipeFile(process, kwsysProcess_Pipe_STDOUT, PipeFileName.c_str());
  //
  // Display the entire command for debug purposes and make it pretty
  //
  if (verbose) {
      std::string fullcommandline;
      std::for_each(commands.begin(), commands.end(), [&](const std::string &piece){ fullcommandline += piece + " "; });
      DisplayCommand(fullcommandline);
  }
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
    if (line.size()>0) {
        std_out.push_back(line);
        if (verbose) {
            std::cout << std::setw(5) << linenumber++ << " : " << line.c_str() << std::endl;
        }
    }
  }
  delete []filebuffer;
  boost::filesystem::remove(PipeFileName.c_str());
  return std_out;
}
//---------------------------------------------------------------------------
void ExecuteAndDetach(const std::vector<std::string> &commands, bool verbose)
{
  std::vector<std::string> std_out;
  std::vector<const char*> commands_with_null_term;
  std::for_each(commands.begin(), commands.end(), [&](const std::string &piece){ commands_with_null_term.push_back(piece.c_str()); });
  commands_with_null_term.push_back(0);
  kwsysProcess* process = kwsysProcess_New();
  kwsysProcess_SetCommand(process, &commands_with_null_term[0]);
  kwsysProcess_SetTimeout(process, -1);
  kwsysProcess_SetOption (process, kwsysProcess_Option_Detach, 1);
  std::string PipeFileName = getTempFile("kwsys_tempfile.txt");
  kwsysProcess_SetPipeFile(process, kwsysProcess_Pipe_STDOUT, PipeFileName.c_str());
  //
  // Display the entire command for debug purposes and make it pretty
  //
  if (verbose) {
      std::string fullcommandline;
      std::for_each(commands.begin(), commands.end(), [&](const std::string &piece){ fullcommandline += piece + " "; });
      DisplayCommand(fullcommandline);
  }
  //
  // Execute
  //
  try
  {
    kwsysProcess_Execute(process);
    kwsysProcess_Disown(process);
  }
  catch (...) {}
  kwsysProcess_Delete(process);
}
//---------------------------------------------------------------------------
std::vector<std::string> ExecuteAndCaptureSSH(const std::vector<std::string> &commands,
                          double timeout, 
                          const std::string &plink,
                          const std::string &user,
                          const std::string &key,
                          const std::string &loginnode,
                          bool verbose
                          )
{
  std::vector<std::string> full_command;
#ifdef WIN32
  full_command.push_back(plink);
  full_command.push_back("-ssh");
  full_command.push_back("-l");
  full_command.push_back(user);
  full_command.push_back("-i");
  full_command.push_back(key);
  full_command.push_back(loginnode);
#else
  full_command.push_back("ssh");
  full_command.push_back(loginnode);
#endif
  full_command.insert(full_command.end(), commands.begin(), commands.end());
  return ExecuteAndCapture(full_command, timeout, verbose);
}
