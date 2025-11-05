/**
 *
 * @file interrupts.cpp
 * @author Bhagya Patel
 * 
 *
 */

#include<interrupts.hpp>
#include<filesystem>  
// Global PID counter for child processes
unsigned int next_pid = 1;

std::tuple<std::string, std::string, int> simulate_trace(std::vector<std::string> trace_file, int time, std::vector<std::string> vectors, std::vector<int> delays, std::vector<external_file> external_files, PCB current, std::vector<PCB> wait_queue) {

    std::string trace; 
    std::string execution = ""; 
    std::string system_status = ""; 
    int current_time = time;

    //parse each line of the input trace file. 'for' loop to keep track of indices.
    for(size_t i = 0; i < trace_file.size(); i++) {
        auto trace = trace_file[i];

        auto [activity, duration_intr, program_name] = parse_trace(trace);

        if(activity == "CPU") { 
            execution += std::to_string(current_time) + ", " + std::to_string(duration_intr) + ", CPU Burst\n";
            current_time += duration_intr;
        } else if(activity == "SYSCALL") { 
            auto [intr, time] = intr_boilerplate(current_time, duration_intr, 10, vectors);
            execution += intr;
            current_time = time;

            execution += std::to_string(current_time) + ", " + std::to_string(delays[duration_intr]) + ", SYSCALL ISR\n";
            current_time += delays[duration_intr];

            execution +=  std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;
        } else if(activity == "END_IO") {
            auto [intr, time] = intr_boilerplate(current_time, duration_intr, 10, vectors);
            current_time = time;
            execution += intr;

            execution += std::to_string(current_time) + ", " + std::to_string(delays[duration_intr]) + ", ENDIO ISR\n";
            current_time += delays[duration_intr];

            execution +=  std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;
        } else if(activity == "FORK") {
            auto [intr, time] = intr_boilerplate(current_time, 2, 10, vectors);
            execution += intr;
            current_time = time;

            ///////////////////////////////////////////////////////////////////////////////////////////
            //Add your FORK output here
            
            //ISR clones the PCB (duration from trace file)
            execution += std::to_string(current_time) + ", " + std::to_string(duration_intr) + ", cloning the PCB\n";
            current_time += duration_intr;
            
            // Create child process - initially without partition assignment
            PCB child(next_pid++, current.PID, current.program_name, current.size, -1);
            
            // Allocate memory for child process
            if(!allocate_memory(&child)) {
                std::cerr << "ERROR! Memory allocation failed for child process!" << std::endl;
                break;
            }
            
            //Call scheduler (duration = 0)
            execution += std::to_string(current_time) + ", 0, scheduler called\n";
            
            // Return from ISR (IRET)
            execution += std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;
            
            // Generate system status snapshot showing child as running, parent as waiting
            wait_queue.push_back(current); // Parent goes to wait queue
            system_status += "time: " + std::to_string(current_time) + "; current trace: FORK, " + std::to_string(duration_intr) + "\n";
            system_status += print_PCB(child, wait_queue);

            ///////////////////////////////////////////////////////////////////////////////////////////

            //The following loop helps you do 2 things:
            // * Collect the trace of the child (and only the child, skip parent)
            // * Get the index of where the parent is supposed to start executing from
            std::vector<std::string> child_trace;
            std::vector<std::string> parent_trace;
            bool in_child = false;
            bool in_parent = false;
            int endif_index = -1;

            for(size_t j = i + 1; j < trace_file.size(); j++) {
                auto [_activity, _duration, _pn] = parse_trace(trace_file[j]);
                
                if(_activity == "IF_CHILD") {
                    in_child = true;
                    in_parent = false;
                } 
                else if(_activity == "IF_PARENT") {
                    in_child = false;
                    in_parent = true;
                } 
                else if(_activity == "ENDIF") {
                    endif_index = j;
                    break;
                }
                else {
                    // Add trace to appropriate vector(s)
                    if(in_child) {
                        child_trace.push_back(trace_file[j]);
                    }
                    if(in_parent) {
                        parent_trace.push_back(trace_file[j]);
                    }
                }
            }
            
            // Add everything after ENDIF to both child and parent traces
            if(endif_index != -1) {
                for(size_t j = endif_index + 1; j < trace_file.size(); j++) {
                    child_trace.push_back(trace_file[j]);
                    parent_trace.push_back(trace_file[j]);
                }
            }

            ///////////////////////////////////////////////////////////////////////////////////////////
            //With the child's trace, run the child (HINT: think recursion)

            // Pass the full wait_queue to child so it can see all ancestors
            auto [child_exec, child_status, child_time] = simulate_trace(
                child_trace,
                current_time,
                vectors,
                delays,
                external_files,
                child,
                wait_queue  // Pass wait_queue containing parent(s)
            );
            
            execution += child_exec;
            system_status += child_status;
            current_time = child_time;

            ///////////////////////////////////////////////////////////////////////////////////////////

            // After child completes, restore parent from wait_queue
            // Create copy to preserve ancestors in parent's wait queue
            PCB parent = wait_queue.back();
            std::vector<PCB> parent_wait_queue = wait_queue;
            parent_wait_queue.pop_back();  // Remove parent from COPY only
            
            auto [parent_exec, parent_status, parent_time] = simulate_trace(
                parent_trace,
                current_time,
                vectors,
                delays,
                external_files,
                parent,
                parent_wait_queue  // Pass modified copy
            );
            
            execution += parent_exec;
            system_status += parent_status;
            current_time = parent_time;
            
            // Break because we've handled the rest of the trace through recursion
            break;

        } else if(activity == "EXEC") {
            auto [intr, time] = intr_boilerplate(current_time, 3, 10, vectors);
            current_time = time;
            execution += intr;

            ///////////////////////////////////////////////////////////////////////////////////////////
            //Add your EXEC output here

            // ISR searches file in external_files and gets memory size
            unsigned int program_size = get_size(program_name, external_files);
            if(program_size == (unsigned int)-1) {
                std::cerr << "ERROR: Program " << program_name << " not found in external files!" << std::endl;
                break;
            }
            
            execution += std::to_string(current_time) + ", " + std::to_string(duration_intr) + ", Program is " + std::to_string(program_size) + " Mb large\n";
            current_time += duration_intr;
            
            //Simulate execution of the loader (15ms per MB)
            int load_time = program_size * 15;
            execution += std::to_string(current_time) + ", " + std::to_string(load_time) + ", loading program into memory\n";
            current_time += load_time;
            
            //Free old partition and find empty partition where executable fits
            if(current.partition_number != -1) {
                free_memory(&current);
            }
            
            // Update PCB with new program information
            current.program_name = program_name;
            current.size = program_size;
            
            // Allocate new partition (best-fit from smallest)
            if(!allocate_memory(&current)) {
                std::cerr << "ERROR! Memory allocation failed for " << program_name << std::endl;
                break;
            }
            
            //Mark partition as occupied (random 1-10ms)
            int mark_time = (rand() % 10) + 1;
            execution += std::to_string(current_time) + ", " + std::to_string(mark_time) + ", marking partition as occupied\n";
            current_time += mark_time;
            
            //Update PCB with new information (random 1-10ms)
            int update_time = (rand() % 10) + 1;
            execution += std::to_string(current_time) + ", " + std::to_string(update_time) + ", updating PCB\n";
            current_time += update_time;
            
            //Call scheduler (duration = 0)
            execution += std::to_string(current_time) + ", 0, scheduler called\n";
            
            // Return from ISR (IRET)
            execution += std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;
            
            // Generate system status snapshot
            system_status += "time: " + std::to_string(current_time) + "; current trace: EXEC " + program_name + ", " + std::to_string(duration_intr) + "\n";
            system_status += print_PCB(current, wait_queue);

            ///////////////////////////////////////////////////////////////////////////////////////////


            std::ifstream exec_trace_file(program_name + ".txt");

            std::vector<std::string> exec_traces;
            std::string exec_trace;
            while(std::getline(exec_trace_file, exec_trace)) {
                exec_traces.push_back(exec_trace);
            }

            ///////////////////////////////////////////////////////////////////////////////////////////
            //With the exec's trace (i.e. trace of external program), run the exec (HINT: think recursion)

            auto [exec_exec, exec_status, exec_time] = simulate_trace(
                exec_traces,
                current_time,
                vectors,
                delays,
                external_files,
                current,
                wait_queue
            );
            
            execution += exec_exec;
            system_status += exec_status;
            current_time = exec_time;

            ///////////////////////////////////////////////////////////////////////////////////////////

            break; //Why is this important? (answer in report)
            // Answer: EXEC replaces the process image entirely. The original process no longer exists,
            // so the remaining trace should not execute. The new program has taken over.

        }
    }

    return {execution, system_status, current_time};
}

int main(int argc, char** argv) {

    // Seed random number generator for random times (1-10ms) in EXEC
    srand(time(NULL));

    //vectors is a C++ std::vector of strings that contain the address of the ISR
    //delays  is a C++ std::vector of ints that contain the delays of each device
    //the index of these elemens is the device number, starting from 0
    //external_files is a C++ std::vector of the struct 'external_file'. Check the struct in 
    //interrupt.hpp to know more.
    auto [vectors, delays, external_files] = parse_args(argc, argv);
    std::ifstream input_file(argv[1]);

    //check to know what files you have
    print_external_files(external_files);

    //Make initial PCB 
    PCB current(0, -1, "init", 1, -1);
    //Update memory
    if(!allocate_memory(&current)) {
        std::cerr << "ERROR! Memory allocation failed!" << std::endl;
    }

    std::vector<PCB> wait_queue;

    /******************ADD YOUR VARIABLES HERE*************************/


    /******************************************************************/

    //Converting the trace file into a vector of strings.
    std::vector<std::string> trace_file;
    std::string trace;
    while(std::getline(input_file, trace)) {
        trace_file.push_back(trace);
    }

    auto [execution, system_status, _] = simulate_trace(   trace_file, 
                                            0, 
                                            vectors, 
                                            delays,
                                            external_files, 
                                            current, 
                                            wait_queue);

    input_file.close();

    // Build unique output file names based on the trace input file
    std::string traceFile = argv[1];
    std::string base = std::filesystem::path(traceFile).stem().string(); // e.g. "trace1"
    std::string executionFile = "execution_" + base + ".txt";
    std::string statusFile = "system_status_" + base + ".txt";
    
    // Write the accumulated execution string into the execution file
    std::ofstream outfile(executionFile);
    if (!outfile.is_open()) {
        std::cerr << "Error: Could not open " << executionFile << std::endl;
        return 1;
    }
    outfile << execution;
    outfile.close();
    std::cout << "Output generated in " << executionFile << std::endl;
    
    // Write the system status to the status file
    std::ofstream statusOutfile(statusFile);
    if (!statusOutfile.is_open()) {
        std::cerr << "Error: Could not open " << statusFile << std::endl;
        return 1;
    }
    statusOutfile << system_status;
    statusOutfile.close();
    std::cout << "System status generated in " << statusFile << std::endl;

    return 0;
}