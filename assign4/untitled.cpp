static void jobSignaler(char* tokens[], int signal) {
  if (tokens[0] == NULL) {
    throw STSHException("jobSignaler - Nah numbas provided young blood");
  }

  size_t endPosition;
  long long number1;
  long long number2;

  if (tokens[1] != NULL) {
    try {
          num = stoll(pipeline.commands[0].tokens[0], &endpos);
          num2 = stoll(pipeline.commands[0].tokens[1], &endpos);
        } catch (...) {
          throw STSHException("jobSignaler - Number not parsed young blood");
        }

        if (joblist.containsJob(num)) {
          blockSIGCHLD();
          STSHJob& job = joblist.getJob(num);
          size_t processNumber = 0;
          std::vector<STSHProcess>& processes = job.getProcesses();

          if (num2 >= processes.size()) {
            cout << processes.size() << " size" << endl;
            throw STSHException("jobSignaler - the process is out of scope brother bear");
          }

          STSHProcess& process = processes.at(num2);
          cout << "pid of process: " << process.getID() << endl;
          process.setState(kTerminated);

          joblist.synchronize(job);
          kill(process.getID(), signal);
          unblockSIGCHLD();
        } else {
          throw STSHException("jobSignaler brother that job doesn't exits");
        }
        return;
  }


      // THE ONE ARGUMENT VERSION
      try {
        num = stoll(pipeline.commands[0].tokens[0], &endpos);
      } catch (...) {
        throw STSHException("jobSignaler - Number not parsed young blood");
      }

      if (joblist.containsProcess(num)) {
        blockSIGCHLD();
        // STSHJob& job = joblist.getJob(num);
        // pid_t pid = job.getGroupID();
        cout << "-- jobSignaler pid: " << num << endl;
        if (num == 0) {
          throw STSHException("0 young blood");
        }

        // terminating the thingie
        STSHJob& job = joblist.getJobWithProcess(num);
        STSHProcess& process = job.getProcess(num);
        process.setState(kTerminated);
        joblist.synchronize(job);

        kill(num, signal);
        unblockSIGCHLD();
      } else {
        throw STSHException("jobSignaler brother that process no exist");
      }

}