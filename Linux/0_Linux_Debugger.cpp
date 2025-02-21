#include <iostream>

int main(int argc, char* argv[]) {
    std::cerr << "Program Name Not Specified" << std::endl;
    return 0;
}

auto progr = argv[1];

auto pid = fork();
if (pid == 0)
{
    //We're in the building process
    //Execute Debugee
}
else if (pid >= 1){
    //parent process
    std::cerr << "Debugger PID: " << pid << std::endl;
    debugger dbg{progr, pid};
    dbg.run();
}
else{
    //Error
    std::cerr << "Error: Fork Failed" << std::endl;
    return 1;
}

class debugger
{
private:
    std::string m_prog_name;
    pid_t m_pid;
public:
    debugger(std::string prog_name, pid_t pid)
        : m_prog_name{std::move(prog_name)}, m_pid{pid} {}
    void run(); 
}

void debugger::run()
{
    int wait_status;
    auto options = 0;
    waitpid(m_pid, &wait_status, options);

    char* line = nullptr;
    while ((line = linenoise("minidbg> ")) != nullptr)
    {
        handle_command(line);
        linenoiseHistoryAdd(line);
        linenoiseFree(line);
    }
}

void debugger::handle_command(const std::string& line)
{
    auto args = split(line, ' ');
    auto command = args[0];

    if (is_prefix(command, "continue"))
    {
        continue_execution();
    }
    else if (is_prefix(command, "break"))
    {
        std::string addr{args[1], 2}; //Remove the 0x prefix
        set_breakpoint_at_address(std::stol(addr, 0, 16));
    }
    else
    {
        std::cerr << "Unknown command\n";
    }
}

std::vector<std::string> split(const std::string& s, char delimiter)
{
    std::vector<std::string> out;
    std::stringstream ss{s};
    std::string item;
    while (std::getline(ss, item, delimiter))
    {
        out.push_back(item);
    }
    return out;
}

bool is_prefix(const std::string& s, const std::string& of)
{
    if (s.size() > of.size())
    {
        return false;
    }
    return std::equal(s.begin(), s.end(), of.begin());
}

void continue_execution()
{
    ptrace(PTRACE_CONT, m_pid, nullptr, nullptr);
    int wait_status;
    auto options = 0;
    waitpid(m_pid, &wait_status, options);
}