#include <signal.h>
#include <vector>
#include <iostream>
#include <sstream>
#include <sys/wait.h>
#include <stdlib.h>
#include<iomanip>
#include <unistd.h>
#include <stdio.h>
#include<cstring>
#include<cstdlib>
#include<fstream>
#include <fcntl.h>  
#include <unistd.h>
#include<exception>
using namespace std;




class IORedirections{
	public:
		char redirectMode[32];
                char redirectToFile[256];
		static const char inputRedirect[32];
		static const char outputRedirect[32];
		static const char outputAppend[32];
		static const char errorRedirect[32];
		static const char errorAppend[32];
		void setRedirectMode(char *mode);
		bool setFile(char *fileName);
		char *getMode(){
			return redirectMode; 
		}
		
		char *getFile(){
			return redirectToFile;
		}
};

const char IORedirections::inputRedirect[32] = "<";
const char IORedirections::outputRedirect[32] = ">";
const char IORedirections::outputAppend[32] = ">>";
const char IORedirections::errorRedirect[32] = "e>";
const char IORedirections::errorAppend[32] = "e>>";


class BookKeeping{
	public:
		string command;
		size_t jobId;
		string statusValue;
		int status;
		void updateBook();
		void displayContent();
};

vector<BookKeeping *> allJobs;


void BookKeeping::displayContent(){
	cout<<setw(10)<<jobId<<setw(20)<<statusValue<<setw(50)<<command<<endl;

}

void BookKeeping::updateBook(){
	int status_new;

	/** WNOHANG is added so that the command should not wait for chage of state **/	
	waitpid(jobId, &status_new, WNOHANG | WUNTRACED | WCONTINUED);

	if(status==status_new){
		return;
	}

	status = status_new;
	if (WIFEXITED(status_new)) {
		cout<<command<<" EXITED"<<endl;
		statusValue="EXITED";
	
	} else if (WIFSIGNALED(status_new)) {
		cout<<command<<" KILLED"<<endl;
		statusValue="KILLED";
	} else if (WIFSTOPPED(status_new)) {
		statusValue="STOPPED";
		cout<<command<<" STOPPED"<<endl;
	} else if (WIFCONTINUED(status_new)) {
		statusValue="CONTINUED";
		cout<<command<<" CONTINUED"<<endl;
	}
}


class LabFunctions
{
private:
        static const string stdioe_signatures[];
public:
        static BookKeeping * show_pipes_info(string str, bool&);
	static BookKeeping * processPipelinedCommand(string);
        static string preprocess(string str);
        static vector<string> split(string str, char delimiter);
};

const string LabFunctions::stdioe_signatures[] = {"<","e>>","e>",">>",">"};


/** global functions and variable declartion start **/
vector<string> parts;
string num;
pid_t cpid, w;

char environmentVariables[1024][256]={'\0', };
int varCount=0;
int ** numPipes;
void close_pipe(int pipefd [2]);
vector<char *> mk_cstrvec(vector<string> & strvec);
void dl_cstrvec(vector<char *> & cstrvec);
void nice_exec(vector<string> args);
inline void nope_out(const string & sc_name);
void help();
void cd(string );
void pwd();
/** global functions and variable declartion start **/


/** this function is used to change current directory **/
void cd(string directory){
	if(directory!="")
		chdir(directory.c_str());
	else
		cout<<"Invalid directory option"<<endl;
}

void set_read(int* lpipe)
{
    dup2(lpipe[0], STDIN_FILENO);
    close(lpipe[0]); // we have a copy already, so close it
    close(lpipe[1]); // not using this end
}

void set_write(int* rpipe)
{
    dup2(rpipe[1], STDOUT_FILENO);
    close(rpipe[0]); // not using this end
    close(rpipe[1]); // we have a copy already, so close it
}

/** this is to print help regarding shell **/
void help(){
	cout<<"Current builtins: help , cd, exit"<<endl<<endl;
	cout<<"exit will exit the shell program"<<endl;
	cout<<"help will display this message"<<endl;
	cout<<"cd will change the directory to the specified path"<<endl;
	cout<<"jobs will display valid commands executed in current session"<<endl;

}



/** this is used to display pwd **/
void pwd() {
	char * pwd = nullptr;
	if ((pwd = get_current_dir_name()) != nullptr) {
		stringstream ss;
		string pwd2;
		ss << pwd;
		ss >> pwd2;
		char homeDirectory = '~';
		string subPwd = pwd2.substr(pwd2.find_last_of('/',pwd2.find_last_of('/')-1));
		string newPwd = homeDirectory + subPwd;
		cout<<newPwd;
	} // if
} // pwd



/** this is used to free vector of IORedirection **/
void freeVector(vector<IORedirections *> &vec){
	for ( unsigned int i = 0; i < vec.size(); i++)
	{
		delete[] vec.at(i);
	}
}

/* signal canceller function */
void signal_callback_handler(int signum){
	if(signum==SIGINT){
		kill(cpid,2);
	}else if(signum==SIGQUIT){
		kill(cpid,3);
	}else if(signum==SIGTSTP){
		kill(cpid,20);
	}else if(signum==SIGTTIN){
		kill(cpid,21);
	}else if(signum==SIGTTOU){
		kill(cpid,22);
	}else if(signum==SIGCHLD){
		kill(cpid,17);
	}


}

/** This is helper function to set redirection mode **/
void IORedirections::setRedirectMode(char *mode){
	strcpy(redirectMode, mode);
}

bool IORedirections::setFile(char *fileName){
	bool status=true;
	if(redirectMode==inputRedirect){
		ifstream infile(fileName);
		status=infile.good();
		infile.close();
	}
	strcpy(redirectToFile, fileName);
	return status;
}


/** this function convert string vector into character array vector **/
vector<char *> mk_cstrvec(vector<string> & strvec) {
	vector<char *> cstrvec;
	for (unsigned int i = 0; i < strvec.size(); ++i) {
		cstrvec.push_back(new char [strvec.at(i).size() + 1]);
		strcpy(cstrvec.at(i), strvec.at(i).c_str());
	} // for
	cstrvec.push_back(nullptr);
	return cstrvec;
} // mk_cstrvec


/** this is to free memory allocated to character array vector **/
void dl_cstrvec(vector<char *> & cstrvec) {
	for (unsigned int i = 0; i < cstrvec.size(); ++i) {
		delete[] cstrvec.at(i);
	} // for
} // dl_cstrvec


/** this is used to execute command **/
void nice_exec(vector<string> strargs) {
	vector<char *> cstrargs = mk_cstrvec(strargs);
	vector<char *> sendArgs;
	for(char *item: cstrargs)
	{
		if(item!=nullptr){
			string temp=item;
			if(temp!= "&")
				sendArgs.push_back(item);

		}

	}
	execvp(sendArgs.at(0), &cstrargs.at(0));
	dl_cstrvec(cstrargs);
	exit(EXIT_FAILURE);
} // nice_exec



/** this function will identify wether a process is FOREGROUND or BACKGROUND **/
void getProcessType(vector<string> strargs, string& type) {
	vector<char *> cstrargs = mk_cstrvec(strargs);
	for(char *item: cstrargs)
	{
		if(item!=nullptr){
			string temp=item;
			if(temp=="&")
				type="BACKGROUND";
		}

	}
	dl_cstrvec(cstrargs);
} 



/** This function will identify all IO related redirection and send info to caller function**/
bool computeRedirections(string command, string& errMsg, int& redirectOccur, vector<IORedirections *>& redirectInfo){
	bool insertFile=false;
	bool alreadyRedirected=false;
	char item[1024];
	redirectOccur=0;
	IORedirections *redirected=nullptr;
	vector<string> cmd_tokens = LabFunctions::split(command,' ');
	for(string cmd_token: cmd_tokens)
	{
		strcpy(item, cmd_token.c_str());

		if(item!=nullptr){
			char *temp=item; 
			/** checks if identifier is valid redirection string **/
			if(!strcmp(temp, IORedirections::inputRedirect) || !strcmp(temp, IORedirections::outputRedirect) || !strcmp(temp, IORedirections::outputAppend) || !strcmp(temp, IORedirections::errorRedirect) || !strcmp(temp,IORedirections::errorAppend))
			{
				redirectOccur=1;
				if(!alreadyRedirected){
					redirected = new IORedirections();
					redirected->setRedirectMode(temp);
					/** redirection is to take place **/
					insertFile=true;
				}
				else{
					/** two redirection without mentioning file is not allowed **/
					errMsg="Invalid redirections";
					return false;
				}
			}
			else if(insertFile==true){

				/** if input redirection then check if file exists **/
				if(!(redirected->setFile(temp))){
					errMsg="Error in opening file";
					return false;
				}
				/** store data in vector **/
				redirectInfo.push_back(redirected);
				alreadyRedirected=false;
				insertFile=false;
			}
		}
	}
	return true;
}

/** This function will redirect IO operation **/
void redirectFileDescriptors(vector<IORedirections *> &vec){
	for ( unsigned int i = 0; i < vec.size(); i++)
	{
		if(!strcmp(vec.at(i)->getMode(), IORedirections::inputRedirect)){
			int fd = open(vec.at(i)->getFile(), O_RDONLY);
			dup2(fd, STDIN_FILENO);
			close(fd);
		}else if(!strcmp(vec.at(i)->redirectMode, IORedirections::outputRedirect)){
			int fd = open(vec.at(i)->getFile(), O_WRONLY|O_CREAT|O_TRUNC);
			dup2(fd, STDOUT_FILENO);
			close(fd);
		}else if(!strcmp(vec.at(i)->redirectMode, IORedirections::outputAppend)){
			int fd = open(vec.at(i)->getFile(), O_WRONLY|O_CREAT|O_APPEND);
			dup2(fd, STDOUT_FILENO);
			close(fd);
		}else if(strcmp(vec.at(i)->redirectMode, IORedirections::errorRedirect)){
			int fd = open(vec.at(i)->getFile(), O_WRONLY|O_CREAT|O_TRUNC);
			dup2(fd, STDERR_FILENO);
			close(fd);
		}else if(vec.at(i)->redirectMode==IORedirections::errorAppend){
			int fd = open(vec.at(i)->getFile(), O_WRONLY|O_CREAT|O_APPEND);
			dup2(fd, STDERR_FILENO);
			close(fd);
		}       
	}

}


/** this is main function and it process command along with sending object of book keeping process **/
BookKeeping * LabFunctions::show_pipes_info(const string cmd_line, bool& pipelined)
{

	BookKeeping *newJob;	
	string temp_cmd;
	string temp_val[10];
	for(int i=0; i<10; i++){
		temp_val[i]="";
	}

	/** fetch each attribute of command **/
	vector<string> cmd_tokens = LabFunctions::split(cmd_line,' ');
	int i=0;
	/** fill command and its arguments for inbuilt process **/
	for(string cmd_token: cmd_tokens)
	{
		if(i==0){
			temp_cmd=cmd_token;
		}else{
			temp_val[i-1]=cmd_token;
		}
		i++;	
		if(i==9)
			break;
	}


	if(cmd_line=="help"){
		help();
		return nullptr;
	}else if(temp_cmd=="exit"){
		if(temp_val[0]==""){
			cout<<"EXITING Status:"<<w<<endl;
			exit(w);
		}else{
			if(isdigit(temp_val[0][0])){
				cout<<"EXITING Status:"<<stoi(temp_val[0])<<endl;
				exit(stoi(temp_val[0]));
			}else{
				cout<<"Invalid argument"<<endl;
				return nullptr;
			}
		}
	}
	/** set environment variable **/
	else if(temp_cmd=="export"){
		string variable="";
		string value="";
		vector<string> envVariabl = LabFunctions::split(temp_val[0],'=');	

		i=0;
		for(string env_par: envVariabl)
		{
			if(i==0){
				variable=env_par;
			}
			else if(i==1){
				value=env_par;
			}
			else{
				break;
			}
			i++;	
		}
		if(value!=""){
			char tempVar[256];
			strcpy(tempVar, temp_val[0].c_str());	
			setenv(variable.c_str(), value.c_str(), 1);
			strcpy(environmentVariables[varCount++], tempVar);
			putenv(tempVar);
		}
		else{
			getenv(variable.c_str());
		}
		return nullptr;
	}
	/** print jobs executed till now **/
	else if(temp_cmd=="jobs"){
		cout<<setw(10)<<"JID"<<setw(20)<<"STATUS"<<setw(50)<<"COMMAND"<<endl;
		for ( unsigned int i = 0; i < allJobs.size(); i++)
		{
			allJobs.at(i)->displayContent();
		}
		return nullptr;
	}
	else if(temp_cmd=="cd"){
		if(temp_val[0][0]!='\0'){
			cd(temp_val[0]);
		}else{
			cout<<"Invalid path"<<endl;
		}
		return nullptr;
	}	

	int size = sizeof(stdioe_signatures)/sizeof(stdioe_signatures[0]);
	vector<string>::iterator ita;
	string type; /** changed **/

	string stdin_val="STDIN_FILENO ", 
	       stdout_val="STDOUT_FILENO ", 
	       stderr_val="STDERR_FILENO ",
	       preprocessed_str = preprocess(cmd_line);
	/** split command into string literals **/
	vector<string> processes = split(preprocessed_str,'\r'), argv;
	stringstream buffer;
	string buffer1;
	string buffer2;
	string processName;
	int counter = 0;
	bool flag;
	vector<string> indProcess;
	string file;                                    //splitting the process side of the string with 2 stringstreams and getline
	//string token;
	stringstream ss(cmd_line);
	//stringstream ss2;
	string token;
	//int printOK = 0;
	for(unsigned int x = 0;x<processes.size();x++){

		getline (ss,file,'|');
		stringstream ss2(file);
		while( getline(ss2, token)){
			indProcess.push_back(token);
		}
	}//for


	/** remove redirections from input data **/
	for(vector<string>::iterator it = processes.begin(); it != processes.end(); ++it)
	{
		argv = split(*it,'\n');
		ita = argv.begin(); 
		  while(ita != argv.end())
		  {

		  flag=false;
		  for(int j=0;j<size;j++)
		  {
		  if(*ita==stdioe_signatures[j])
		  {	
		  flag = true;			
		  if(*ita==">>")
		  {
		  stdout_val = *(ita+1) + " (append)";
		  ita = argv.erase(ita);
		  }
		  else if(*ita==">")
		  {
		  stdout_val = *(ita+1) + " (truncate)";
		  ita = argv.erase(ita);
		  }
		  else if(*ita=="<")
		  {
		  stdin_val = *(ita+1);
		  ita = argv.erase(ita);
		  }
		  else if(*ita=="e>>")
		  {
		  stderr_val = *(ita+1) + " (append)";
		  ita = argv.erase(ita);
		  }
		  else if(*ita=="e>")
		  {
		  stderr_val = *(ita+1) + " (truncate)";
		  ita = argv.erase(ita);
		  } 
		  ita = argv.erase(ita);
		  break; 
		  }
		  }
		  if(!flag)
		  ++ita; 
		  } 
		buffer<<"Process "<<counter<<" argv:\n";
		for (size_t i = 0; i!=argv.size(); ++i) 
		{
			buffer<<i<<": "<<argv[i]<<endl;
		}
		buffer<<endl;

	} //for// I added this

	int numPipes = processes.size()-1;
	int status;
	string processType; 

	/** process pipelined data **/
	/**if (numPipes > 0){ 
	  int ** pipes = new int * [numPipes];
	  for(int i =0; i<numPipes;++i ){
	  pipes[i] = new int[2];
	  pipe(pipes[i]);
	  }
	  for(int i = 0;i<=numPipes;i++){     // hardcoded this

	  cpid = fork();
	  if (cpid == -1) { 
	  perror("fork"); exit(EXIT_FAILURE); 
	  }

	  if (cpid == 0) { 
	  for(int j = 0;j<=numPipes;j++){
	  if (j ==0){
	  int n = dup2(pipes[j][1], STDOUT_FILENO);
	  if(n == -1)perror("dup2");
	  }
	  if ((j > 0) & (j<=numPipes)){
	  int q = dup2(pipes[j][1], STDOUT_FILENO);
	  if(q == -1)perror("dup2");
	  int m = dup2(pipes[j][0], STDIN_FILENO);
	  if(m == -1) perror("dup2");
	  }
	  if(j == numPipes){
	  int u = dup2(pipes[j][0], STDIN_FILENO);
	  if(u == -1)perror("dup2");

	  }

	  nice_exec(argv);
	  }
	  }
	  }
	  } **/

	if (numPipes > 0){
		pipelined=true; 
		/*vector<string> processes = split(cmd_line,'|');		
		int processNo=0;
		string tempProcessType;

		cout<<"seperate processes are:"<<endl;
		for(string command_part:processes){
			cout<<command_part<<endl;
			cout<<numPipes<<":"<<processNo<<endl;
			string commandOnly=command_part;

				getProcessType(command_part, tempProcessType);
			  if(processNo!=numPipes && tempProcessType=="BACKGROUND"){
			  cout<<"Syntax error"<<endl;	
			  return nullptr;
			  }else if(tempProcessType=="BACKGROUND"){
			  processType="BACKGROUND";			
			  }

			  vector<IORedirections *> redirectionInfo;
			  if(!computeRedirections(commandOnly, errMessage, redirectionNeeded, redirectionInfo)){
			  freeVector(redirectionInfo);
			  return nullptr;
			  }
		}**/
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/** process normal commands **/
	else if (numPipes == 0){
		/** fetch wethjer process is BACKGROUND **/
		getProcessType(argv, processType);
		string errMessage = "";
		int redirectionNeeded=0;
		string tempCommand=cmd_line;

		/** fetch type of redirections **/
		vector<IORedirections *> redirectionInfo;
		if(!computeRedirections(tempCommand, errMessage, redirectionNeeded, redirectionInfo)){
			freeVector(redirectionInfo);
			return nullptr;
		}

		for (size_t i = 0; i!=argv.size(); ++i){
			if(i==0){
				processName=argv[i];
				buffer2="/bin/";
			}else{
				buffer1=buffer1+argv[i]+" ";
			}

		}
		cpid = fork();

		if (cpid == 0) { /* Code executed by child */

			for (size_t i = 0; i!=argv.size(); ++i){
				if(i==0){
					processName=argv[i];
					buffer2="/bin/";
				}else{
					buffer1=buffer1+argv[i]+" ";
				}

			}


			if(redirectionNeeded==1){
				redirectFileDescriptors(redirectionInfo);
			}

			nice_exec(argv);             /***********/
			_exit(0);


		}
		/** free memory occupied by IORedirection **/
		freeVector(redirectionInfo);


		if(cpid >0) {                    /* Code executed by parent */

			/** update bookkeeping  **/
			newJob = new BookKeeping();
			newJob->command =cmd_line;
			newJob->jobId=cpid;	
			/** update bookkeeping  **/
			if(processName=="cd"){
				cd(argv[1]);
			}

			if(processType!="BACKGROUND"){
				do {     
					w = waitpid(cpid, &status, WUNTRACED | WCONTINUED);
					newJob->status=status;
					if (w == -1) { 	
						perror("waitpid"); 
					}


					if (WIFEXITED(status)) {
						cout<<cmd_line<<" EXITED"<<endl;
						newJob->statusValue="EXITED";
					} else if (WIFSIGNALED(status)) {
						cout<<cmd_line<<" KILLED"<<endl;
						newJob->statusValue="KILLED";
					} else if (WIFSTOPPED(status)) {
						cout<<cmd_line<<" STOPPED"<<endl;
						newJob->statusValue="STOPPED";
					} else if (WIFCONTINUED(status)) {
						newJob->statusValue="CONTINUED";
						cout<<cmd_line<<" CONTINUED"<<endl;
					}
				} while (!WIFEXITED(status) && !WIFSIGNALED(status));
			}
		}
	}
	counter++;

	cout<<endl;
	return newJob;

}/** end of how_pipes_info **/ 



/** process string to handle special symbolls like escape sequence and multiple string as one argument **/
string LabFunctions::preprocess(string str)
{	
	bool in_string = false;
	int size = sizeof(stdioe_signatures)/sizeof(stdioe_signatures[0]);
	for(size_t i=0;i<str.size();i++)
	{

		if(!in_string)
		{
			switch(str[i])
			{
				case '|':
					str[i]='\r';
					break;
				case ' ':
					str[i]='\n';
					break;
				case '\t':
					str[i]='\n';
					break;
				case '"':
					if(str[i-1]!='\\')
					{
						in_string = true;
						str.erase (i, 1);
						i--;
					}
				default:			
					for(int j=0;j<size;j++)
					{
						size_t idx = str.find(stdioe_signatures[j], i);
						if(i==idx)
						{
							str = str.insert(idx, "\n");
							str = str.insert(idx+stdioe_signatures[j].size()+1, "\n");
							i+=stdioe_signatures[j].size();
							break;
						}
					}
			}
		}
		else
		{
			if(in_string && i!=0 && str[i]=='"' && str[i-1]!='\\')
			{
				in_string = false;
				str.erase (i, 1);
				i--;
				continue;
			}
			else if(in_string && i!=0 && str[i]=='"' && str[i-1]=='\\')
			{
				str.erase (i-1, 1);
			}
		}
	}
	return str;
}

// split string on not null parts by delimeter
vector<string> LabFunctions::split(string str, char delimiter)
{
	vector<string> parts; 
	size_t i, k=0;
	for(i=0;i<str.size();i++)
	{
		if( (str[i]==delimiter))
		{
			if(i-k>0)
				parts.push_back(str.substr(k,i-k));
			k=i+1;	
		}
	}
	// last stripped block
	if(i-k>0)
	{
		parts.push_back(str.substr(k,i-k+1));
	}
	return parts;
}


BookKeeping * LabFunctions::processPipelinedCommand(string command){
	BookKeeping *newJob;
	vector<string> processes = split(command,'|');
	int processNo=0;
	string processType;
	string errMessage;
	int redirectionNeeded = 0;
	int numPipes = processes.size();
	int lpipe[2], rpipe[2];
	size_t cpid;
	cout<<"Total pipes:"<<numPipes<<endl;
	cout<<"seperate processes are:"<<endl;
	for(string command_part:processes){
		cpid=0;
		processNo++;
		string tempCommand = command_part;	
		cout<<command_part<<endl;
		cout<<processNo<<endl;
		string commandOnly=command_part;
		string preprocessed_str = preprocess(command_part);
		vector<string> processes = split(preprocessed_str,'\r'), argv;
		bool flag;
		int size = sizeof(stdioe_signatures)/sizeof(stdioe_signatures[0]);
		
		vector<string>::iterator ita;
		for(vector<string>::iterator it = processes.begin(); it != processes.end(); ++it)
	        {
        	        argv = split(*it,'\n');

			ita = argv.begin();
                  while(ita != argv.end())
                  {

                  flag=false;
                  for(int j=0;j<size;j++)
                  {
                  if(*ita==stdioe_signatures[j])
                  {
                  flag = true;
                  if(*ita==">>")
                  {
                  ita = argv.erase(ita);
                  }
                  else if(*ita==">")
                  {
                  ita = argv.erase(ita);
                  }
                  else if(*ita=="<")
                  {
                  ita = argv.erase(ita);
                  }
                  else if(*ita=="e>>")
                  {
                  ita = argv.erase(ita);
                  }
                  else if(*ita=="e>")
                  	{
        	          ita = argv.erase(ita);
	                  }
			ita = argv.erase(ita);
                  break;
                  }
                  }
                  if(!flag)
                  ++ita;
                  }

		}
		
		getProcessType(argv, processType);
		if(processNo!=numPipes && processType=="BACKGROUND"){
			cout<<"Syntax error cannot execute Intermediate process as Background"<<endl;     
			return nullptr;
		}else if(processType=="BACKGROUND"){
			processType="BACKGROUND";                       
		}

		vector<IORedirections *> redirectionInfo;
		redirectionNeeded = 0;
		errMessage="";
		if(!computeRedirections(tempCommand, errMessage, redirectionNeeded, redirectionInfo)){
			freeVector(redirectionInfo);
			return nullptr;
		}

		if(processNo==1){
			pipe(rpipe);
			
			cpid = fork();
			if(cpid==0)
    			{	
            			set_write(rpipe);
				if(redirectionNeeded==1){
	                                redirectFileDescriptors(redirectionInfo);
        	                }
	
        	                nice_exec(argv);             /***********/
                	        _exit(0);
			}else{
				lpipe[0] = rpipe[0];
        			lpipe[1] = rpipe[1];
			}
		}else if(processNo==numPipes){
			cpid = fork();
                        if(cpid==0)
                        {
				set_read(lpipe);
				if(redirectionNeeded==1){
                                        redirectFileDescriptors(redirectionInfo);
                                }

                                nice_exec(argv);             /***********/
                                _exit(0);
			}else{
				close(lpipe[0]);
        			close(lpipe[1]);
			}
		}else{
			pipe(rpipe);		
			cpid = fork();
                        if(cpid==0)
                        {
                                set_read(lpipe);
				set_write(rpipe);
                                if(redirectionNeeded==1){
                                        redirectFileDescriptors(redirectionInfo);
                                }

                                nice_exec(argv);             /***********/
                                _exit(0);
                        }			
			else{
				close(lpipe[0]); // both ends are attached, close them on parent
                		close(lpipe[1]);
				lpipe[0] = rpipe[0]; // output pipe becomes input pipe
		                lpipe[1] = rpipe[1];
			}
		}
		freeVector(redirectionInfo);	
	
	}

	if(cpid >0) {                    /* Code executed by parent */
                        int status=0;
                        /** update bookkeeping  **/
                        newJob = new BookKeeping();
                        newJob->command = command;
                        newJob->jobId=cpid;
                        /** update bookkeeping  **/
                        if(processType!="BACKGROUND"){
                                do {      
                                        w = waitpid(cpid, &status, WUNTRACED | WCONTINUED);
                                        newJob->status=status;
                                        if (w == -1) {
                                                perror("waitpid");
                                        }
                               
                    
                                        if (WIFEXITED(status)) {
                                                cout<<command<<" EXITED"<<endl;
                                                newJob->statusValue="EXITED";
                                        } else if (WIFSIGNALED(status)) {
                                                cout<<command<<" KILLED"<<endl;
                                                newJob->statusValue="KILLED";
                                        } else if (WIFSTOPPED(status)) {
                                                cout<<command<<" STOPPED"<<endl;
                                                newJob->statusValue="STOPPED";
                                        } else if (WIFCONTINUED(status)) {
                                                newJob->statusValue="CONTINUED";
                                                cout<<command<<" CONTINUED"<<endl;
                                        }
                                } while (!WIFEXITED(status) && !WIFSIGNALED(status));
                        }
                }

	return newJob;		
}

int main()
{

	signal(SIGINT, signal_callback_handler);
	signal(SIGQUIT, signal_callback_handler);
	signal(SIGTSTP, signal_callback_handler);
	signal(SIGTTIN, signal_callback_handler);
	signal(SIGTTOU, signal_callback_handler);
	signal(SIGCHLD, signal_callback_handler);

	bool isPipelined=false;

	string cmd_line;
	while(1)
	{
		cout<<"1730sh: ";
		pwd();
		cout<<"$ "<<flush;
		/** update all background processes data **/
		for ( unsigned int i = 0; i < allJobs.size(); i++)
		{
			allJobs.at(i)->updateBook();
		}

		getline (cin,cmd_line);

		/** handle NULL line **/
		if(cmd_line==""){
			continue;
		}
		/** check command **/
		isPipelined=false;
		BookKeeping *newJob = LabFunctions::show_pipes_info(cmd_line, isPipelined);
		
		if(isPipelined){
			cout<<"Pipelined"<<endl;
			newJob = LabFunctions::processPipelinedCommand(cmd_line);
		}		
		/** if valid normal command add to bookkeeping **/
		else if(newJob!=nullptr)
			allJobs.push_back(newJob);

	}	
	return 0;
}
