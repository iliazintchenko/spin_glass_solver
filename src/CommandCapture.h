#ifndef _COMMANDCAPTURE_H_
#define _COMMANDCAPTURE_H_

#include <vector>
#include <string>

// Execute a shell/OS command and return the resulting output as a string
std::vector<std::string> ExecuteAndCapture(const std::vector<const char*> &commands, double timeout, bool verbose=false);

// Execute a shell/OS command and detach from it and leave it running
void ExecuteAndDetach(const std::vector<const char*> &commands, bool verbose);

// Execute a shell command with an ssh into a remote host
// linux ssh, windows plink,
// Assumes ssh keys have been setup
std::vector<std::string> ExecuteAndCaptureSSH(const std::vector<const char*> &commands, double timeout,
  const std::string &plink, const std::string &user, const std::string &key,
  const std::string &loginnode, bool verbose=false);

#endif
