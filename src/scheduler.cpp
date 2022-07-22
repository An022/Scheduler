#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib> 
#include <vector>
#include <list>
#include <map>
#include <iomanip>
#include <deque>
#include <cmath>

using namespace std;

// ========== GLOBAL VARIABLE ==========
#pragma region global variable

enum Type { F, L, S, R, P, E};

enum State { CREATED ,READY , RUNNG , BLOCK , Done };
static const char *enum_str[] = { "CREATED" ,"READY" ,"RUNNG" ,"BLOCK" ,"Done"};

enum tranState { TRANS_TO_READY ,TRANS_TO_RUN ,TRANS_TO_BLOCK ,TRANS_TO_PREEMPT, TRANS_TO_DONE };

bool verbose;
bool callScheduler = false;
int scheType;
int numCounter = 0;
int currTime = 0;
int quantum; // TODO: non-preemptive schedulers have a very large quantum
int maxPri; 
double IO_utl = 0; // FOR REPORT IO Utilization
string reoportTittle;

#pragma endregion global variable


// ========== Class: Process & Event ========== 
class process {
public:
  process(int AT, int CT, int CB, int IO) {
    this->AT = AT;
    this->CT = CT;
    this->CB = CB;
    this->IO = IO;
  }

  int processNum;
  int AT;
  int CT;
  int CB;
  int IO;
  int CB_rem = 0; // cpu burst remaining time
  int CT_rem; // Total cpu remaining time
  int pri; // staticPRIO = dyn_PRIO-1 
  int dynPri; // default range ( 1 ~ 4)
  int ready_StartTime; // TODO: do you really need this?
  int preStateTime; // prevIO_utls event's initalTime
  double FT = 0; // finished time
  double TT = 0; // turnaround time
  double IT = 0; // I/O time
  int CW = 0; // cpu waiting time
  
  void resetPri () {
    this->dynPri = this->pri - 1 ;
  }

};

class event {
public:
  event(int timeStamp, int processID, int oldState, int newState, int transState) {
    this->timeStamp = timeStamp;
    this->processID = processID;
    this->oldState = oldState;
    this->newState = newState;
    this->transState = transState;
  }

  int timeStamp;
  int processID;
  int oldState;
  int newState;
  int transState;
};


// ========== Scheduler ========== 
class Scheduler {
public:
  deque <process*> readyQue;
  deque <process*> *activeQ, *expiredQ;
  virtual void addProcess(process* pro) = 0;
  virtual process* getNextProcess() = 0;
  virtual bool test_preempt (process* processRunning, process*pro, event* currEvent, deque<event*> eventQue, int currTime) = 0;
};

class FCFS : public Scheduler {
public:
  process* getNextProcess(){
    process* pro;
    if(readyQue.size() != 0) {
      pro = readyQue.front();
      readyQue.pop_front();
    }
    else {
      return NULL;
    }
    return pro;
  }

  void addProcess(process*pro) {
    readyQue.push_back(pro);
  }

  bool test_preempt (process* processRunning, process*pro, event* currEvent, deque<event*> eventQue, int currTime) {
    return false;
  }
};

class LCFS : public Scheduler {
public:
  process* getNextProcess(){
    process* pro;
    if(readyQue.size() != 0) {
      pro = readyQue.back();
      readyQue.pop_back();
    }
    else {
      return NULL;
    }
    return pro;
  }

  void addProcess(process*pro) {
    readyQue.push_back(pro);
  }

  bool test_preempt (process* processRunning, process*pro, event* currEvent, deque<event*> eventQue, int currTime) {
    return false;
  }
};

class SRTF : public Scheduler {
public:
  process * getNextProcess() {
    process* pro;
    if(readyQue.empty()) {
      return NULL;
    }
    int min = readyQue[0]->CT_rem;
    int shortest = 0;
    for (int i =1; i<readyQue.size();i++) {
      if (min> readyQue[i]->CT_rem) {
        min = readyQue[i]->CT_rem;
        shortest = i;
      }
    }
    pro = readyQue[shortest];
    readyQue.erase(readyQue.begin()+shortest);
    return pro;
  }

  void addProcess(process *pro) {
    readyQue.push_back(pro);
  }
  bool test_preempt (process* processRunning, process*pro, event* currEvent, deque<event*> eventQue, int currTime) {
    return false;
  }
};

class RR : public Scheduler {
public:
  process * getNextProcess() {
    process *pro;
    if (readyQue.empty()) {
      return NULL;
    }
    else {
      pro = readyQue.front();
      readyQue.pop_front();
    }
    return pro;
  }

  void addProcess(process *pro) {
    readyQue.push_back(pro);
  }

  bool test_preempt (process* processRunning, process*pro, event* currEvent, deque<event*> eventQue, int currTime) {
    return false;
  }
};

class PRIO : public Scheduler {
public:
  //constructor
  PRIO(int maxPri) {
    activeQ = new deque <process*>[maxPri];
    expiredQ = new deque <process*>[maxPri];
  }

  process* getOutQue(deque<process*>* getQue) {
    process* pro = NULL;
    for (int i = maxPri-1; i >= 0 ; --i) { // TODO: i = default dynPri
      if (getQue[i].size() == 0) {
        continue;
      }
      pro = getQue[i].front();
      getQue[i].pop_front();
      break;
    } 
    return pro;
  }

  process* getNextProcess() {
    process* pro = getOutQue(activeQ);

    if (pro == NULL) {
      deque <process*> *temp = activeQ;
      activeQ = expiredQ;
      expiredQ = temp;
      pro = getOutQue(activeQ);
      if (pro == NULL) {
        return NULL;
      }
    }
    return pro;
  }

  void addProcess(process*pro) {
    if (pro->dynPri < 0) {
      // cout << "BEFORE: RESET " << pro->dynPri << endl;
      pro->resetPri();
      // cout << "AFTER: RESET " << pro->dynPri << endl;
      expiredQ[pro->dynPri].push_back(pro);
    } 
    else {
      activeQ[pro->dynPri].push_back(pro);
    }
  }

  bool test_preempt (process* processRunning, process*pro, event* currEvent, deque<event*> eventQue, int currTime) {
    return false;
  }
};

class PREPRIO : public Scheduler {
public:
  //constructor
  PREPRIO(int maxPri) {
    activeQ = new deque <process*>[maxPri];
    expiredQ = new deque <process*>[maxPri];
  }

  process* getOutQue(deque<process*>* getQue) {
    process* pro = NULL;
    for (int i = maxPri-1; i >= 0 ; --i) { // TODO: i = default dynPri
      if (getQue[i].size() == 0) {
        continue;
      }
      pro = getQue[i].front();
      getQue[i].pop_front();
      break;
    } 
    return pro;
  }

  process* getNextProcess() {
    process* pro = getOutQue(activeQ);

    if (pro == NULL) {
      deque <process*> *temp = activeQ;
      activeQ = expiredQ;
      expiredQ = temp;
      pro = getOutQue(activeQ);
      if (pro == NULL) {
        return NULL;
      }
    }
    return pro;
  }

  void addProcess(process*pro) {
    
    if (pro->dynPri < 0) {
      // cout << "BEFORE: RESET " << pro->dynPri << endl;
      pro->resetPri();
      // cout << "AFTER: RESET " << pro->dynPri << endl;
      expiredQ[pro->dynPri].push_back(pro);
    } 
    else {
      activeQ[pro->dynPri].push_back(pro);
    }
  }

  bool test_preempt (process* processRunning, process*pro, event* currEvent, deque<event*> eventQue, int currTime) {
    if (scheType == E && (currEvent->oldState == BLOCK || currEvent->oldState == CREATED) && processRunning != NULL) {
      // rule1
      bool rule1 = pro->dynPri > processRunning->dynPri;
      bool rule2 = false;
      int TS;
      // rule2
      for (deque<event*>::iterator it = eventQue.begin(); it != eventQue.end(); ++it) {
        if ((*it)->processID == processRunning->processNum) {
          TS = (*it)->timeStamp;
          if (currTime < (*it)->timeStamp) {
            rule2 = true;
          }
          break;
        }
      }
      if (verbose) {
        printf("---> PRIO preemption %d by %d ? %d TS=%d now=%d) --> %s\n", processRunning->processNum, pro->processNum, rule1, TS, currTime, (rule1 && rule2) ? "YES" : "NO" );
      }
      return (rule1 && rule2);
    }
    return false;
  }
};


// ========== VECTOR & QUEUE ========== 
#pragma region Storage

vector <process*> _process;
vector <int> randomNums;
deque <event*> eventQue; 
Scheduler* sche;
process* runProcess = NULL;

#pragma endregion Storage


// ========== RANDOM NUMBER ==========
void readRNum(string randomFile) {
  fstream file(randomFile.c_str());
  string line;
  if(file.is_open()) {
    while (getline(file, line) && !file.eof()) {
      stringstream tempLine (line);
      string numString;
      while (tempLine >> numString) {
        randomNums.push_back(atoi(numString.c_str()));
      }
    }
  }
}

int randomNum (int limit) {
  return 1 + (randomNums[++numCounter] % limit);
}


// ========== READ FILE ==========
// TODO: renew your code, it's too ugly!!
void readFile (string fileName) {
  string line;
  fstream myFile (fileName.c_str());
  int processCounter = 0;
  if (myFile.is_open()) {
    while (getline (myFile, line)) {
      stringstream tempLine (line);
      string currLine;

      // For counting each str in line.
      int readCount = 0;

      process* currProcess = new process(0, 0 ,0 ,0);
      while (tempLine >> currLine) {
        currProcess->processNum = processCounter;
        if (readCount%4 == 0) {
          currProcess->AT = atoi(currLine.c_str());
        } 
        else if (readCount%4 == 1) {
          currProcess->CT = atoi(currLine.c_str());
        }
        else if (readCount%4 == 2) {
          currProcess->CB = atoi(currLine.c_str());
        }
        else if (readCount%4 == 3) {
          currProcess->IO = atoi(currLine.c_str());
        }
        readCount++;
      }
      processCounter++;
      currProcess->pri = randomNum(maxPri);
      currProcess->dynPri = currProcess->pri - 1;
      currProcess->preStateTime = currProcess->AT;
      _process.push_back(currProcess);
    }
  }
  for (vector<process *>::iterator i = _process.begin(); i != _process.end(); ++i) {
    event* transEvent = new event((*i)->AT, (*i)->processNum, CREATED, READY,TRANS_TO_READY);
    eventQue.push_back(transEvent);
  }
}


// ========== Event ==========
event* getNextEvent() {
  if (eventQue.size() == 0) {
    return NULL;
  }
  event* evt;
  evt = eventQue.front();
  eventQue.pop_front();
  return evt;
}

void putEvent(event* evt) {
  int i = 0;
  while (i < eventQue.size() && evt->timeStamp >= eventQue[i]->timeStamp) {
    i++;
  }
  eventQue.insert(eventQue.begin()+i, evt);
}

int getNextEventTime() {
  if (eventQue.size() == 0) {
    return -1;
  }
  return eventQue.front()->timeStamp;
}


// ========== Simulation ==========
void Simulation() {
  event* currEvent;
  process* processRunning = NULL;
  process* processBlocked = NULL;
  int statePeriod; // TODO: do we need this ? 
  int transState; // TODO: really initail this in 0 ?
  int burst;
  int ioBurst;
  int ioSum;
  int ioEnd;
  
  string showOldStage;

  while ((currEvent = getNextEvent()) != NULL) {
    int pid = currEvent->processID;

    process* pro = _process[pid];
    int timeInPreState = currTime - _process[pid]->preStateTime;

    transState = currEvent->transState;
    // time jumps discretely.
    currTime = currEvent->timeStamp;
    
    switch (transState) {
      // created, running, blocked --> READY
      case TRANS_TO_READY:
        currEvent->newState = READY;
        currEvent->transState = RUNNG;
        // REOPOT : IO utl (maintain)
        if (processBlocked == _process[pid]) {
          processBlocked = NULL;
        }
        
        if (currEvent->oldState == BLOCK) {
          _process[pid]->dynPri = _process[pid]->pri - 1;
        }
        
        timeInPreState = currTime - _process[pid]->preStateTime;
        // REPORT : IT (maintain)
        _process[pid]->IT += timeInPreState;

        if (verbose) {
          cout << currTime << " " << pid << " " << timeInPreState << ": " << enum_str[currEvent->oldState] << " -> " << enum_str[currEvent->newState] << endl;
        }

        if (sche->test_preempt(processRunning, _process[pid], currEvent, eventQue, currTime)) {
          for (deque<event *>::iterator it = eventQue.begin(); it != eventQue.end(); ++it) {
            if ((*it)->processID == processRunning->processNum) {
              eventQue.erase(it);
              break;
            }
          }

          putEvent (new event(currTime, processRunning->processNum, RUNNG, READY, TRANS_TO_PREEMPT));
        }


        sche->addProcess(_process[pid]);
        
        callScheduler = true;
        
        if (_process[pid]->CT_rem == _process[pid]->CT) {
            _process[pid]->CT_rem = _process[pid]->CT;
        }
        else if (currEvent->timeStamp == _process[pid]->AT) {
          _process[pid]->CT_rem =_process[pid]->CT - timeInPreState;
        }
          _process[pid]->preStateTime = currTime;
          currEvent->oldState = READY;
        break;

      // running --> end, blocked(waiting), READY
      case TRANS_TO_RUN:
        processRunning = _process[pid];
        // REPORT : CW (maintain)
        _process[pid]->CW += timeInPreState;

        if (_process[pid]->CB_rem == 0) {
          burst = randomNum(_process[pid]->CB); // TODO update to randomNum
          _process[pid]->CB_rem = burst;
        } 

        if (_process[pid]->CT_rem <= _process[pid]->CB_rem) {
          _process[pid]->CB_rem = _process[pid]->CT_rem;
        }

        
        // --> DONE
        if (_process[pid]->CB_rem >= _process[pid]->CT_rem && quantum >= _process[pid]->CT_rem)  {
          if (_process[pid]->CB_rem > _process[pid]->CT_rem) {
            _process[pid]->CB_rem = _process[pid]->CT_rem;
          }
          putEvent(new event(currTime + _process[pid]->CB_rem, pid, RUNNG, Done, TRANS_TO_DONE));
          _process[pid]->preStateTime = currTime; 
        }

        // --> preempt
        else if (_process[pid]->CB_rem > quantum && _process[pid]->CT_rem > quantum) {
          // REPORT : CW (maintain)
          _process[pid]->ready_StartTime = currTime;
          putEvent(new event(currTime+quantum, pid, RUNNG, READY, TRANS_TO_PREEMPT));
          _process[pid]->preStateTime = currTime;

        }

        // --> blocked
        else if (_process[pid]->CB_rem <= quantum && _process[pid]->CT_rem >= _process[pid]->CB_rem) {

          currEvent->newState = RUNNG;
          putEvent(new event(currTime+_process[pid]->CB_rem, pid, RUNNG, BLOCK, TRANS_TO_BLOCK));
          _process[pid]->preStateTime = currTime; 

        }

        if (verbose) {
          cout << currTime << " " << pid << " " << timeInPreState << ": " << enum_str[currEvent->oldState] << " -> " << "RUNNG";
          cout << " cb=" << _process[pid]->CB_rem << " rem=" << _process[pid]->CT_rem << " prio=" << _process[pid]->dynPri << endl;
        }
        break;

      case TRANS_TO_PREEMPT:
        processRunning = NULL;
        timeInPreState = currTime - _process[pid]->preStateTime;
        _process[pid]->CT_rem -= timeInPreState;
        _process[pid]->CB_rem -= timeInPreState;
        callScheduler = true;

        if (verbose) {
          printf("%d %d %d: RUNNG -> READY", currTime, pid, timeInPreState);
          printf("  cb=%d rem=%d prio=%d\n", _process[pid]->CB_rem, _process[pid]->CT_rem, _process[pid]->dynPri);
        }
        _process[pid]->preStateTime = currTime;

        if (scheType == P || scheType == E) {
          _process[pid]->dynPri -= 1; // PRIO
        }

        sche->addProcess(_process[pid]);
        break;

      case TRANS_TO_BLOCK:
        ioBurst = randomNum(_process[pid]->IO);
        if (_process[pid]->CT_rem == _process[pid]->CT) {
            _process[pid]->CT_rem = _process[pid]->CT - _process[pid]->CB_rem;
          } 
          else {
            _process[pid]->CT_rem = _process[pid]->CT_rem - _process[pid]->CB_rem;
          }

        callScheduler = true;
        processRunning = NULL;

        // REPORT: IO Utalization (maintain)
        if (processBlocked == NULL) { // no other process in blocked
          ioSum += ioBurst;
          ioEnd = ioBurst + currTime;
          processBlocked = _process[pid];
        } 
        else if (currTime + ioBurst > ioEnd) { // other process in block && current process stay longer
            ioSum += ioBurst + currTime - ioEnd;
            ioEnd = ioBurst + currTime;
            processBlocked = _process[pid];
        }

        currEvent->newState = READY;
        timeInPreState = currTime - _process[pid]->preStateTime;
        putEvent(new event(currTime+ioBurst, pid, BLOCK, READY, TRANS_TO_READY));
        
        if (verbose) {
          cout << currTime << " " << pid << " " << timeInPreState << ": " << enum_str[currEvent->oldState] << " -> " << "BLOCK";
          cout << "  ib=" << ioBurst << " rem=" << _process[pid]->CT_rem << endl;
        }
          _process[pid]->preStateTime = currTime; 

        _process[pid]->CB_rem -= timeInPreState;

        break;

      case TRANS_TO_DONE:
        currEvent->newState = Done;
        // CALCULATING REPORT
        _process[pid]->FT = currTime;
        _process[pid]->TT = currTime - _process[pid]->AT;
        IO_utl = (ioSum / _process[pid]->FT) * 100.00;

        timeInPreState = currTime - _process[pid]->preStateTime;
        if (verbose) {
          printf("%d %d %d: Done\n", currTime, pid, timeInPreState);
        }
        processRunning = NULL;
        callScheduler = true;
        break;
    }

    if (callScheduler) {
      if (getNextEventTime() == currTime) continue;
      callScheduler = false;

      if (processRunning == NULL) {
        processRunning = sche->getNextProcess();
        if (processRunning == NULL) {
          continue;
        }
        putEvent(new event(currTime , processRunning->processNum, READY, RUNNG, TRANS_TO_RUN));
      }
    }
  }
}

void prtProInfo() {
  // TODO if FCFS
  int max_FT = 0;
  double CPU_utl; // FOR REPORT CPU Utilization
  double avg_TT;
  double avg_CW;
  double cpuSum = 0;
  double TTSum = 0;
  double CWSum = 0;
  cout << reoportTittle << endl;
  for (vector<process *>::iterator it = _process.begin(); it != _process.end(); ++it) {
    // process original info
    cout << setw(4) << setfill('0') << (*it)->processNum << ": ";
    cout << setw(4) << setfill(' ') << (*it)->AT << " ";
    cout << setw(4) << (*it)->CT << " ";
    cout << setw(4) << (*it)->CB << " ";
    cout << setw(4) << (*it)->IO << " ";
    cout << (*it)->pri << " | ";
    // processing info
    cout << setw(5) << (*it)->FT << " ";
    cout << setw(5) << (*it)->TT << " ";
    cout << setw(5) << (*it)->IT << " ";
    cout << setw(5) << (*it)->CW << endl;
    cpuSum += (*it)->CT;
    TTSum += (*it)->TT;
    CWSum += (*it)->CW;
    if ((*it)->FT > max_FT) {
      max_FT = (*it)->FT;
    }
  }
  // Summary all info
  CPU_utl = (cpuSum / max_FT) * 100.00;
  avg_TT = (TTSum / _process.size());
  avg_CW = (CWSum / (_process.size()));
  printf("SUM: %d %.2lf %.2lf %.2lf %.2lf %.3lf\n", max_FT, CPU_utl, IO_utl, avg_TT, avg_CW, ((double)_process.size()/max_FT) * 100.00);
}


// ========== Main ==========
int main (int argc,  char* argv[]) {
  setvbuf(stdout, NULL, _IONBF, 0);
  if (argc < 4) {
    cout << "wrong input !" << endl;
    return 0;
  }

  int c;
  while ((c = getopt(argc, argv, "vs:")) != -1) {
    switch (c) {
      case 'v':
        verbose = true;
        break;

      case 's':
        if (optarg[0] == 'F') {
          sche = new FCFS();
          scheType = F;
          reoportTittle = "FCFS";
          quantum = 10000;
          maxPri = 4;
        }
        else if (optarg[0] == 'L') {
          sche = new LCFS();
          scheType = L;
          reoportTittle = "LCFS";
          quantum = 10000;
          maxPri = 4;
        }
        else if (optarg[0] == 'S') {
          sche = new SRTF();
          scheType = S;
          reoportTittle = "SRTF";
          quantum = 10000;
          maxPri = 4;
        }
        else if (optarg[0] == 'R') {
          sche = new RR();
          scheType = R;
          string rrQuantum;
          for (int i = 1; i < strlen(optarg); ++i) {
            rrQuantum.append(1, optarg[i]);
          }
          quantum = atoi(rrQuantum.c_str());
          reoportTittle = "RR " + rrQuantum;
          maxPri = 4;
        }
        else if (optarg[0] == 'P') {
          string prioQuantum;
          string prioPRIO;
          scheType = P;
          for (int i = 1; i < strlen(optarg); ++i) {
            if (optarg[i] != ':') {
              prioQuantum.append(1, optarg[i]);
            } 
            else {
              for (int j = i+1; j < strlen(optarg); ++j) {
                prioPRIO.append(1, optarg[j]);
              }
              break;
            }
          }
          quantum = atoi(prioQuantum.c_str());
          reoportTittle = "PRIO " + prioQuantum;
          if (prioPRIO != "") {
            maxPri = atoi(prioPRIO.c_str());
          } 
          else {
            maxPri = 4;
          }
          sche = new PRIO(maxPri);
        }                        
        else if (optarg[0] == 'E') {
          string prioQuantum;
          string prioPRIO;
          scheType = E;
          for (int i = 1; i < strlen(optarg); ++i) {
            if (optarg[i] != ':') {
              prioQuantum.append(1, optarg[i]);
            } 
            else {
              for (int j = i+1; j < strlen(optarg); ++j) {
                prioPRIO.append(1, optarg[j]);
              }
              break;
            }
          }
          quantum = atoi(prioQuantum.c_str());
          reoportTittle = "PREPRIO " + prioQuantum;
          if (prioPRIO != "") {
            maxPri = atoi(prioPRIO.c_str());
          } 
          else {
            maxPri = 4;
          }
          sche = new PREPRIO(maxPri);
        }   
        break;
      case '?':
        return -1 ; // TODO: do we really need this?
      default: cout << "Invalid Arguments!";
      return -1;
    }
  }
  readRNum(argv[argc-1]);
  readFile(argv[argc-2]);
  while (!eventQue.empty()) {
    Simulation();
  }
  prtProInfo();
  return 0;
};
