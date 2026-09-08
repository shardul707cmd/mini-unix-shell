#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <array>
#include <algorithm>
#include <csignal>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <termios.h>
#include <cerrno>
#include <cctype>
#include <stdexcept>

struct Job {
    int job_id;
    pid_t pgid;
    std::vector<pid_t> pids;
    std::string command;
    bool stopped;
};

struct Redirections {
    std::string input_file;
    std::string output_file;
    std::string error_file;
    bool append_output = false;
};

bool is_redirection_operator(const std::string& token) {
    return token == "<" ||
           token == ">" ||
           token == ">>" ||
           token == "2>";
}

bool parse_redirections(
    std::vector<std::string>& args,
    Redirections& redirections
) {
    for (size_t i = 0; i < args.size();) {
        const std::string& operator_token = args[i];

        if (!is_redirection_operator(operator_token)) {
            ++i;
            continue;
        }

        if (i + 1 >= args.size() ||
            is_redirection_operator(args[i + 1]) ||
            args[i + 1] == "|") {
            if (operator_token == "<") {
                std::cerr << "shell: missing input file\n";
            }
            else if (operator_token == "2>") {
                std::cerr << "shell: missing error file\n";
            }
            else {
                std::cerr << "shell: missing output file\n";
            }

            return false;
        }

        if (operator_token == "<") {
            redirections.input_file = args[i + 1];
        }
        else if (operator_token == ">") {
            redirections.output_file = args[i + 1];
            redirections.append_output = false;
        }
        else if (operator_token == ">>") {
            redirections.output_file = args[i + 1];
            redirections.append_output = true;
        }
        else {
            redirections.error_file = args[i + 1];
        }

        args.erase(args.begin() + i, args.begin() + i + 2);
    }

    return true;
}

bool apply_redirections(const Redirections& redirections) {
    if (!redirections.input_file.empty()) {
        int fd = open(redirections.input_file.c_str(), O_RDONLY);

        if (fd == -1) {
            perror("open input");
            return false;
        }

        if (dup2(fd, STDIN_FILENO) == -1) {
            perror("dup2 stdin");
            close(fd);
            return false;
        }

        close(fd);
    }

    if (!redirections.output_file.empty()) {
        int flags = O_WRONLY | O_CREAT;

        if (redirections.append_output) {
            flags |= O_APPEND;
        }
        else {
            flags |= O_TRUNC;
        }

        int fd = open(redirections.output_file.c_str(), flags, 0644);

        if (fd == -1) {
            perror("open output");
            return false;
        }

        if (dup2(fd, STDOUT_FILENO) == -1) {
            perror("dup2 stdout");
            close(fd);
            return false;
        }

        close(fd);
    }

    if (!redirections.error_file.empty()) {
        int fd = open(
            redirections.error_file.c_str(),
            O_WRONLY | O_CREAT | O_TRUNC,
            0644
        );

        if (fd == -1) {
            perror("open error");
            return false;
        }

        if (dup2(fd, STDERR_FILENO) == -1) {
            perror("dup2 stderr");
            close(fd);
            return false;
        }

        close(fd);
    }

    return true;
}

volatile sig_atomic_t child_finished = 0;
void handle_sigchld(int) {
    child_finished = 1;
}

int main() {

    std::vector<Job> jobs;
    int next_job_id = 1;


    // ---------------------------------------------------------

    // Initialize interactive job control

    // ---------------------------------------------------------

    pid_t shell_pid = getpid();

    // Put the shell in its own process group.

    if (setpgid(shell_pid, shell_pid) == -1) {

        perror("setpgid shell");

    }

    pid_t shell_pgid = getpgrp();

    // The shell ignores terminal job-control signals.

    signal(SIGINT, SIG_IGN);

    signal(SIGTSTP, SIG_IGN);

    signal(SIGTTOU, SIG_IGN);

    signal(SIGTTIN, SIG_IGN);

    signal(SIGCHLD, handle_sigchld);

    // Make the shell's process group the foreground group.

    if (tcsetpgrp(STDIN_FILENO, shell_pgid) == -1) {

        perror("tcsetpgrp shell");
    }
    
    while (true) {
       if (child_finished) {
    child_finished = 0;

    while (true) {
        int status;
        pid_t changed_pid = waitpid(
            -1,
            &status,
            WNOHANG | WUNTRACED | WCONTINUED
        );

        if (changed_pid <= 0) {
            break;
        }

        for (auto it = jobs.begin(); it != jobs.end(); ++it) {
            auto pid_it = std::find(
                it->pids.begin(),
                it->pids.end(),
                changed_pid
            );

            if (pid_it == it->pids.end()) {
                continue;
            }

            if (WIFSTOPPED(status)) {
                it->stopped = true;

                std::cout << "[" << it->job_id << "] Stopped "
                          << it->command << "\n";
            }
            else if (WIFCONTINUED(status)) {
                it->stopped = false;
            }
            else if (WIFEXITED(status) || WIFSIGNALED(status)) {
                it->pids.erase(pid_it);

                if (it->pids.empty()) {
                    std::cout << "[" << it->job_id << "] Done "
                              << it->command << "\n";

                    jobs.erase(it);
                }
            }

            break;
        }
    }
}

        std::cout << "myshell$ ";
        std::cout.flush();

        std::string input;
        std::getline(std::cin, input);

        if (std::cin.eof()) {
            break;
        }

        if (input.empty()) {
            continue;
        }

std::vector<std::string> args;
std::string argument;
char quote = '\0';

for (size_t i = 0; i < input.size(); ++i) { 

    char c = input[i];

    // Start or end a quoted section.
    if (c == '\'' || c == '"') {

        if (quote == '\0') {
            quote = c;
        }
        else if (quote == c) {
            quote = '\0';
        }
        else {
            argument += c;
        }

        continue;
    }

    // Whitespace separates arguments outside quotes.
    if (std::isspace(static_cast<unsigned char>(c)) &&
        quote == '\0') {

        if (!argument.empty()) {
            args.push_back(argument);
            argument.clear();
        }

        continue;
    }

    // Shell operators should become separate tokens.
    if (quote == '\0' &&
        (c == '|' || c == '<' || c == '>' || c == '&')) {

        if (!argument.empty()) {
            args.push_back(argument);
            argument.clear();
        }
         if (c == '>' &&
        !args.empty() &&
        args.back() == "2")
         {
        args.pop_back();
        args.push_back("2>");
        continue;
    }

        // Handle >>.
        if (c == '>' &&
            i + 1 < input.size() &&
            input[i + 1] == '>') {

            args.push_back(">>");
            ++i;
        }
        else {
            args.push_back(std::string(1, c));
        }

        continue;
    }

    argument += c;
}
if (quote != '\0') {
    std::cerr << "shell: unmatched quote\n";
    continue;
}

if (!argument.empty()) {
    args.push_back(argument);
}
        if (args.empty()) {
            continue;
        }
        // Check whether the command should run in the background.

        bool background = false;

        if (args.back() == "&") {

            background = true;

            args.pop_back();

        }

        if (args.empty()) {

            continue;

        }

        // Built-in: exit
        if (args[0] == "exit") {

            break;
        }
        // Built-in: jobs
        if (args[0] == "jobs") {

            for (const auto& job : jobs) {

                std::cout << "[" << job.job_id << "] ";

                if (job.stopped) {
                    std::cout << "Stopped ";
                } 
                else {
                    std::cout << "Running ";
                    }

                std::cout << job.command << "\n";
    }

    continue;
}
        // Built-in: fg
if (args[0] == "fg") {

    if (args.size() < 2) {
        std::cerr << "fg: usage: fg <job_id>\n";
        continue;
    }

    int job_id;

try {
    size_t position;
    job_id = std::stoi(args[1], &position);

    if (position != args[1].size()) {
        throw std::invalid_argument("invalid job id");
    }
}
catch (const std::exception&) {
    std::cerr << "fg: invalid job id\n";
    continue;
}

    auto it = std::find_if(
        jobs.begin(),
        jobs.end(),
        [job_id](const Job& job) {
            return job.job_id == job_id;
        }
    );

    if (it == jobs.end()) {
        std::cerr << "fg: no such job\n";
        continue;
    }

    Job job = *it;

    if (tcsetpgrp(STDIN_FILENO, job.pgid) == -1) {
        perror("tcsetpgrp");
        continue;
    }
    if (job.stopped) {
        if (kill(-job.pgid, SIGCONT) == -1) {
            perror("kill SIGCONT");
        }
    }

    bool job_stopped = false;
for (pid_t child_pid : job.pids) {

    int status;

    while (true) {
        pid_t result = waitpid(child_pid, &status, WUNTRACED);

        if (result == -1) {
            if (errno == EINTR) {
                continue;
            }

            perror("waitpid");
            break;
        }

        if (WIFSTOPPED(status)) {
            job_stopped = true;
        }

        break;
    }
}

    if (tcsetpgrp(STDIN_FILENO, shell_pgid) == -1) {
        perror("tcsetpgrp shell");
    }

    if (job_stopped) {
        it->stopped = true;
    }
    else {
        jobs.erase(it);
    }

    continue;
}
// Built-in: bg
if (args[0] == "bg") {

    if (args.size() < 2) {
        std::cerr << "bg: usage: bg <job_id>\n";
        continue;
    }
\
    int job_id;

try {
    size_t position;
    job_id = std::stoi(args[1], &position);

    if (position != args[1].size()) {
        throw std::invalid_argument("invalid job id");
    }
}
catch (const std::exception&) {
    std::cerr << "bg: invalid job id\n";
    continue;
}

    auto it = std::find_if(
        jobs.begin(),
        jobs.end(),
        [job_id](const Job& job) {
            return job.job_id == job_id;
        }
    );

    if (it == jobs.end()) {
        std::cerr << "bg: no such job\n";
        continue;
    }

    if (!it->stopped) {
        std::cerr << "bg: job is already running\n";
        continue;
    }

    if (kill(-it->pgid, SIGCONT) == -1) {
        perror("kill SIGCONT");
        continue;
    }

    it->stopped = false;

    std::cout << "[" << it->job_id << "] Running "
              << it->command << "\n";

    continue;
}

        // Built-in: cd
        if (args[0] == "cd") {

            if (args.size() < 2) {
                std::cerr << "cd: missing argument\n";
                continue;
            }

            if (chdir(args[1].c_str()) == -1) {
                perror("cd");
            }

            continue;
        }

        // ---------------------------------------------------------
        // Detect and execute a pipeline
        // ---------------------------------------------------------

        std::vector<std::vector<std::string>> commands;
        std::vector<std::string> current_command;
        bool pipeline_error = false;

        for (const auto& arg : args) {

            if (arg == "|") {

                if (current_command.empty()) {
                    std::cerr << "shell: invalid pipe\n";
                    pipeline_error = true;
                    break;
                }

                commands.push_back(current_command);
                current_command.clear();

            } else {
                current_command.push_back(arg);
            }
        }
      // A pipe at the end has no command after it.
if (!pipeline_error) {

    if (current_command.empty()) {
        if (!commands.empty()) {
            std::cerr << "shell: invalid pipe\n";
            pipeline_error = true;
        }

    } else {
        commands.push_back(current_command);
    }

}

if (pipeline_error) {
  continue;
}

        // Execute pipeline if there is more than one command
        if (commands.size() > 1) {

            std::vector<Redirections> pipeline_redirections;
            pipeline_redirections.reserve(commands.size());

            for (auto& command : commands) {
                Redirections redirections;

                if (!parse_redirections(command, redirections)) {
                    pipeline_error = true;
                    break;
                }

                if (command.empty()) {
                    std::cerr << "shell: missing command\n";
                    pipeline_error = true;
                    break;
                }

                pipeline_redirections.push_back(redirections);
            }

            if (pipeline_error) {
                continue;
            }

            const size_t command_count = commands.size();
            const size_t pipe_count = command_count - 1;

            // Create N - 1 pipes for N commands
            std::vector<std::array<int, 2>> pipes(pipe_count);

            bool pipe_creation_failed = false;

            for (size_t i = 0; i < pipe_count; ++i) {

                if (pipe(pipes[i].data()) == -1) {
                    perror("pipe");
                    pipe_creation_failed = true;
                    break;
                }
            }

            if (pipe_creation_failed) {

                for (size_t i = 0; i < pipe_count; ++i) {
                    close(pipes[i][0]);
                    close(pipes[i][1]);
                }

                continue;
            }

            // Store all child PIDs
            pid_t pipeline_pgid = 0;
            std::vector<pid_t> child_pids;

            // Create one child for every command
            for (size_t i = 0; i < command_count; ++i) {

                // Convert command arguments to argv format
                std::vector<char*> argv;

                for (auto& arg : commands[i]) {
                    argv.push_back(arg.data());
                }

                argv.push_back(nullptr);

                pid_t pid = fork();

                if (pid == -1) {
                    perror("fork");
                    continue;
                }
                if (i == 0) {
                     pipeline_pgid = pid;
                     setpgid(pid, pipeline_pgid);
                    }                    
                else {
                    setpgid(pid, pipeline_pgid);
            }

                // -----------------------------------------------------
                // CHILD
                // -----------------------------------------------------

                if (pid == 0) {
                    // Make the child the leader of its own process group.
              // Put all pipeline children into the same process group.
            if (i == 0) {
            if (setpgid(0, 0) == -1) {
        perror("setpgid child");
        _exit(1);
    }
}
            else {
            if (setpgid(0, pipeline_pgid) == -1) {
        perror("setpgid child");
        _exit(1);
    }
}

                    // Every command except the first
                    // receives stdin from the previous pipe.
                    if (i > 0) {

                        if (dup2(
                                pipes[i - 1][0],
                                STDIN_FILENO
                            ) == -1) {

                            perror("dup2 stdin");
                            _exit(1);
                        }
                    }

                    // Every command except the last
                    // sends stdout to the next pipe.
                    if (i < command_count - 1) {

                        if (dup2(
                                pipes[i][1],
                                STDOUT_FILENO
                            ) == -1) {

                            perror("dup2 stdout");
                            _exit(1);
                        }
                    }

                    // The child no longer needs the original
                    // pipe file descriptors.
                    for (size_t j = 0; j < pipe_count; ++j) {
                        close(pipes[j][0]);
                        close(pipes[j][1]);
                    }

                    if (!apply_redirections(pipeline_redirections[i])) {
                        _exit(1);
                    }

                    signal(SIGINT, SIG_DFL);
                    signal(SIGTSTP, SIG_DFL);
                    signal(SIGTTIN, SIG_DFL);
                    signal(SIGTTOU, SIG_DFL);
                    // Replace child with the actual command.
                    execvp(argv[0], argv.data());

                    // Only reached if execvp() fails.
                    perror("execvp");
                    _exit(127);
                }

                // -----------------------------------------------------
                // PARENT
                // -----------------------------------------------------

                child_pids.push_back(pid);
                if (i == 0) {
                    pipeline_pgid = pid;
                    }
            }

            // Parent does not use any pipe descriptors.
            for (size_t i = 0; i < pipe_count; ++i) {
                close(pipes[i][0]);
                close(pipes[i][1]);
            }
            // Give the terminal to the pipeline's process group.
if (!background) {

    if (tcsetpgrp(STDIN_FILENO, pipeline_pgid) == -1) {
        perror("tcsetpgrp");
    }

    bool pipeline_stopped = false;

    for (pid_t child_pid : child_pids) {

        int status;

        while (true) {

            pid_t result = waitpid(

                child_pid,

                &status,

                WUNTRACED

            );
            if (result == -1) {
                if (errno == EINTR) {
                    continue;
                }
                perror("waitpid");

                break;
            }
            if (WIFSTOPPED(status)) {

                pipeline_stopped = true;
            }
            break;
        }
    }

    if (tcsetpgrp(STDIN_FILENO, shell_pgid) == -1) {
        perror("tcsetpgrp");
    }

    if (pipeline_stopped) {

        Job job{
            next_job_id++,
            pipeline_pgid,
            child_pids,
            input,
            true
        };

        jobs.push_back(job);

        std::cout << "\n[pipeline stopped]\n";
    }
}
else {

    Job job{
        next_job_id++,
        pipeline_pgid,
        child_pids,
        input,
        false
    };

    jobs.push_back(job);

    std::cout << "[" << job.job_id << "] "
              << job.pgid << " "
              << job.command << "\n";
}

continue;
        }

        // =========================================================
        // NORMAL COMMAND + REDIRECTION
        // =========================================================

        Redirections redirections;

        if (!parse_redirections(args, redirections)) {
            continue;
        }

        if (args.empty()) {
            std::cerr << "shell: missing command\n";
            continue;
        }


        // Build argv
        std::vector<char*> argv;

        for (auto& arg : args) {
            argv.push_back(arg.data());
        }

        argv.push_back(nullptr);

        // ---------------------------------------------------------
        // Fork normal command
        // ---------------------------------------------------------

        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            continue;
        }
        // Put the normal command into its own process group.
        if (setpgid(pid, pid) == -1) {
        perror("setpgid command");
        }   

        if (pid == 0) {

            if (!apply_redirections(redirections)) {
                _exit(1);
            }
            signal(SIGINT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            signal(SIGTTIN, SIG_DFL);
            signal(SIGTTOU, SIG_DFL);
            execvp(argv[0], argv.data());

            perror("execvp");
            _exit(127);
        }
        if (!background) {
        // Give the terminal to the foreground command.
        if (tcsetpgrp(STDIN_FILENO, pid) == -1) {
        perror("tcsetpgrp command");
        }
        

        int status;
       

while (true) { 
    pid_t result = waitpid(pid, &status, WUNTRACED);
    if (result == -1) {
        if (errno == EINTR) {
            continue;
        }
        perror("waitpid");
        break;
    }
    break;

}

        // Give the terminal back to the shell.
        if (tcsetpgrp(STDIN_FILENO, shell_pgid) == -1) {
        perror("tcsetpgrp shell");
        }       

        if (WIFSTOPPED(status)) {

        Job job{
            next_job_id++,
            pid,
            {pid},
            input,
            true
        };
        jobs.push_back(job);
        std::cout << "\n[" << job.job_id << "] Stopped "
                  << job.command << "\n";
        }
    }
        else{
            Job job{
        next_job_id++,
        pid,
        {pid},
        input,
        false
    };
        jobs.push_back(job);
        std::cout << "[" << job.job_id << "] "
        << job.pgid << " "
        << job.command << "\n";
        }
}

    return 0;
}
