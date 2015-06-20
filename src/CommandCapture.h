#ifndef _COMMANDCAPTURE_H_
#define _COMMANDCAPTURE_H_

#include <vector>
#include <string>

void DisplayCommand(const std::string &fullcommandline);
void ExecuteAndCapture(std::vector<const char*> &commands, std::vector<std::string> &std_out, double timeout);
void ExecuteAndCaptureSSH(std::vector<const char*> &commands, std::vector<std::string> &std_out, double timeout, 
  std::string &plink, std::string &user, std::string &key, std::string &loginnode);

#endif
