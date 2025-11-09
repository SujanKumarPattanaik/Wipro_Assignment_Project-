#include <bits/stdc++.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
using namespace std;

struct Job {
    int id;
    pid_t pid;
    string command;
    bool running;
};

vector<Job> jobs;
int jobCounter = 1;

void executeCommand(vector<string> args, bool background);
void executePipe(vector<string> leftCmd, vector<string> rightCmd);
void handleRedirection(vector<string> args);
vector<string> tokenize(const string &input);

void showJobs() {
    for (auto &job : jobs)
        cout << "[" << job.id << "] PID: " << job.pid
             << " CMD: " << job.command
             << (job.running ? " (Running)" : " (Stopped)") << endl;
}

void bringToForeground(int id) {
    for (auto &job : jobs) {
        if (job.id == id) {
            int status;
            cout << "Bringing job [" << id << "] to foreground..." << endl;
            waitpid(job.pid, &status, 0);
            job.running = false;
            return;
        }
    }
    cout << "No such job.\n";
}

void killJob(int id) {
    for (auto &job : jobs) {
        if (job.id == id) {
            kill(job.pid, SIGTERM);
            job.running = false;
            cout << "Job [" << id << "] terminated.\n";
            return;
        }
    }
    cout << "No such job.\n";
}

int main() {
    string input;

    cout << "===== Custom Linux Shell =====" << endl;
    cout << "Type 'exit' to quit the shell." << endl;

    while (true) {
        cout << "myshell> ";
        getline(cin, input);
        if (input.empty()) continue;
        if (input == "exit") break;

        vector<string> tokens = tokenize(input);
        if (tokens.empty()) continue;

        // Built-in commands
        if (tokens[0] == "cd") {
            if (tokens.size() > 1)
                chdir(tokens[1].c_str());
            else
                cout << "Usage: cd <directory>\n";
        } else if (tokens[0] == "jobs") {
            showJobs();
        } else if (tokens[0] == "fg") {
            if (tokens.size() > 1) bringToForeground(stoi(tokens[1]));
            else cout << "Usage: fg <jobid>\n";
        } else if (tokens[0] == "kill") {
            if (tokens.size() > 1) killJob(stoi(tokens[1]));
            else cout << "Usage: kill <jobid>\n";
        } else {
            // Check for pipe
            auto it = find(tokens.begin(), tokens.end(), "|");
            if (it != tokens.end()) {
                vector<string> left(tokens.begin(), it);
                vector<string> right(it + 1, tokens.end());
                executePipe(left, right);
            } else if (find(tokens.begin(), tokens.end(), ">") != tokens.end() ||
                       find(tokens.begin(), tokens.end(), "<") != tokens.end()) {
                handleRedirection(tokens);
            } else {
                bool background = (tokens.back() == "&");
                if (background) tokens.pop_back();
                executeCommand(tokens, background);
            }
        }
    }
    return 0;
}

vector<string> tokenize(const string &input) {
    vector<string> tokens;
    string token;
    stringstream ss(input);
    while (ss >> token)
        tokens.push_back(token);
    return tokens;
}

void executeCommand(vector<string> args, bool background) {
    vector<char*> argv;
    for (auto &arg : args)
        argv.push_back((char*)arg.c_str());
    argv.push_back(nullptr);

    pid_t pid = fork();
    if (pid == 0) {
        // Child
        execvp(argv[0], argv.data());
        perror("Command failed");
        exit(1);
    } else if (pid > 0) {
        if (background) {
            cout << "Started background job [" << jobCounter << "] PID: " << pid << endl;
            jobs.push_back({jobCounter++, pid, args[0], true});
        } else {
            waitpid(pid, nullptr, 0);
        }
    } else {
        perror("fork");
    }
}

void handleRedirection(vector<string> args) {
    int in = -1, out = -1;
    vector<string> command;

    for (size_t i = 0; i < args.size(); i++) {
        if (args[i] == "<" && i + 1 < args.size()) {
            in = open(args[i + 1].c_str(), O_RDONLY);
            i++;
        } else if (args[i] == ">" && i + 1 < args.size()) {
            out = open(args[i + 1].c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            i++;
        } else {
            command.push_back(args[i]);
        }
    }

    vector<char*> argv;
    for (auto &arg : command)
        argv.push_back((char*)arg.c_str());
    argv.push_back(nullptr);

    pid_t pid = fork();
    if (pid == 0) {
        if (in != -1) {
            dup2(in, STDIN_FILENO);
            close(in);
        }
        if (out != -1) {
            dup2(out, STDOUT_FILENO);
            close(out);
        }
        execvp(argv[0], argv.data());
        perror("execvp");
        exit(1);
    } else {
        waitpid(pid, nullptr, 0);
    }
}

void executePipe(vector<string> leftCmd, vector<string> rightCmd) {
    int pipefd[2];
    pipe(pipefd);

    pid_t pid1 = fork();
    if (pid1 == 0) {
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);

        vector<char*> argv1;
        for (auto &arg : leftCmd)
            argv1.push_back((char*)arg.c_str());
        argv1.push_back(nullptr);

        execvp(argv1[0], argv1.data());
        perror("execvp");
        exit(1);
    }

    pid_t pid2 = fork();
    if (pid2 == 0) {
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);

        vector<char*> argv2;
        for (auto &arg : rightCmd)
            argv2.push_back((char*)arg.c_str());
        argv2.push_back(nullptr);

        execvp(argv2[0], argv2.data());
        perror("execvp");
        exit(1);
    }

    close(pipefd[0]);
    close(pipefd[1]);
    waitpid(pid1, nullptr, 0);
    waitpid(pid2, nullptr, 0);
}
