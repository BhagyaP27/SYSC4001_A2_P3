/**
 *
 * @file interrupts.cpp
 * @author Bhagya Patel 101321450
 * 
 * @author Tomas Alvarez, 101310738
 * 
 * @brief Header file for interrupts.cpp the purpose of simulating interrupts, process forking, and program execution.
 * 
 *
 */

#ifndef INTERRUPTS_HPP_
#define INTERRUPTS_HPP_

#include<iostream>
#include<fstream>
#include<string>
#include<vector>
#include<random>
#include<utility>
#include<sstream>
#include<iomanip>
#include <algorithm>
#include<stdio.h>

#define ADDR_BASE   0
#define VECTOR_SIZE 2

struct memory_partition_t {
    const unsigned int partition_number;
    const unsigned int size;
    std::string code;

    memory_partition_t(unsigned int _pn, unsigned int _s, std::string _c):
        partition_number(_pn), size(_s), code(_c) {}
};

memory_partition_t memory[] = {
    memory_partition_t(1, 40, "empty"),
    memory_partition_t(2, 25, "empty"),
    memory_partition_t(3, 15, "empty"),
    memory_partition_t(4, 10, "empty"),
    memory_partition_t(5, 8, "empty"),
    memory_partition_t(6, 2, "empty")
};

struct PCB{
    unsigned int    PID;
    int             PPID;
    std::string     program_name;
    unsigned int    size;
    int             partition_number;

    PCB(unsigned int _pid, int _ppid, std::string _pn, unsigned int _size, int _part_num):
        PID(_pid), PPID(_ppid), program_name(_pn), size(_size), partition_number(_part_num) {}
};

struct external_file{
    std::string     program_name;
    unsigned int    size;
};

//Allocates a program to memory (if there is space)
//returns true if the allocation was sucessful, false if not.
bool allocate_memory(PCB* current) {
    for(int i = 5; i >= 0; i--) { //Start from smallest partition
        //check is the code will fit and if the partition is empty
        if(memory[i].size >= current->size && memory[i].code == "empty") {
            current->partition_number = memory[i].partition_number;
            memory[i].code = current->program_name;
            return true;
        }
    }
    return false;
}

//frees the memory given PCB.
void free_memory(PCB* process) {
    memory[process->partition_number - 1].code = "empty";
    process->partition_number = -1;
}

// Following function was taken from stackoverflow; helper function for splitting strings
std::vector<std::string> split_delim(std::string input, std::string delim) {
    std::vector<std::string> tokens;
    std::size_t pos = 0;
    std::string token;
    while ((pos = input.find(delim)) != std::string::npos) {
        token = input.substr(0, pos);
        tokens.push_back(token);
        input.erase(0, pos + delim.length());
    }
    tokens.push_back(input);

    return tokens;
}

/**
 * \brief parse the CLI arguments
 *
 * This helper function parses command line arguments and checks for errors 
 * 
 * @param argc number of command line arguments
 * @param argv the command line arguments
 * @return a vector of strings (the parsed vector table), a vector of delays, a vector of external files
 * 
 */
std::tuple<std::vector<std::string>, std::vector<int>, std::vector<external_file>>parse_args(int argc, char** argv) {
    if(argc != 5) {
        std::cout << "ERROR!\nExpected 4 argument, received " << argc - 1 << std::endl;
        std::cout << "To run the program, do: ./interrutps <your_trace_file.txt> <your_vector_table.txt> <your_device_table.txt> <your_external_files.txt>" << std::endl;
        exit(1);
    }

    std::ifstream input_file;
    input_file.open(argv[1]);
    if (!input_file.is_open()) {
        std::cerr << "Error: Unable to open file: " << argv[1] << std::endl;
        exit(1);
    }
    input_file.close();

    input_file.open(argv[2]);
    if (!input_file.is_open()) {
        std::cerr << "Error: Unable to open file: " << argv[2] << std::endl;
        exit(1);
    }

    std::string vector;
    std::vector<std::string> vectors;
    while(std::getline(input_file, vector)) {
        vectors.push_back(vector);
    }
    input_file.close();

    std::string duration;
    std::vector<int> delays;
    input_file.open(argv[3]);

    if (!input_file.is_open()) {
        std::cerr << "Error: Unable to open file: " << argv[3] << std::endl;
        exit(1);
    }

    while(std::getline(input_file, duration)) {
        delays.push_back(std::stoi(duration));
    }
    input_file.close();

    std::vector<external_file> external_files;
    input_file.open(argv[4]);
    if (!input_file.is_open()) {
        std::cerr << "Error: Unable to open file: " << argv[4] << std::endl;
        exit(1);
    }

    std::string file_content;
    while(std::getline(input_file, file_content)) {
        external_file entry;
        auto file_info      = split_delim(file_content, ",");

        entry.program_name  = file_info[0];
        entry.size          = std::stoi(file_info[1]);
        external_files.push_back(entry);
    }

    input_file.close();


    return {vectors, delays, external_files};
}

//Parces each trace and returns a tuple: {Tace activity, duration or interrupt number, program name (if applicable)}
std::tuple<std::string, int, std::string> parse_trace(std::string trace) {
    //split line by ','
    auto parts = split_delim(trace, ",");
    if (parts.size() < 2) {
        std::cerr << "Error: Malformed input line: " << trace << std::endl;
        return {"null", -1, "null"};
    }

    auto activity = parts[0];
    auto duration_intr = std::stoi(parts[1]);
    std::string extern_file = "null";

    auto exec = split_delim(parts[0], " ");
    if(exec[0] == "EXEC") {
        extern_file = exec[1];
        activity = "EXEC";
    }

    return {activity, duration_intr, extern_file};
}

//Default interrupt boilerplate
std::pair<std::string, int> intr_boilerplate(int current_time, int intr_num, int context_save_time, std::vector<std::string> vectors) {

    std::string execution = "";

    execution += std::to_string(current_time) + ", " + std::to_string(1) + ", switch to kernel mode\n";
    current_time++;

    execution += std::to_string(current_time) + ", " + std::to_string(context_save_time) + ", context saved\n";
    current_time += context_save_time;
    
    char vector_address_c[10];
    sprintf(vector_address_c, "0x%04X", (ADDR_BASE + (intr_num * VECTOR_SIZE)));
    std::string vector_address(vector_address_c);

    execution += std::to_string(current_time) + ", " + std::to_string(1) + ", find vector " + std::to_string(intr_num) 
                    + " in memory position " + vector_address + "\n";
    current_time++;

    execution += std::to_string(current_time) + ", " + std::to_string(1) + ", load address " + vectors.at(intr_num) + " into the PC\n";
    current_time++;

    return std::make_pair(execution, current_time);
}

//Writes a string to a file
void write_output(std::string execution, const char* filename) {
    std::ofstream output_file(filename);

    if (output_file.is_open()) {
        output_file << execution;
        output_file.close();  // Close the file when done
        std::cout << "File content overwritten successfully." << std::endl;
    } else {
        std::cerr << "Error opening file!" << std::endl;
    }

    std::cout << "Output generated in execution.txt" << std::endl;
}

//Helper function for a sanity check. Prints the external files table
void print_external_files(std::vector<external_file> files) {
    const int tableWidth = 24;

    std::cout << "List of external files (" << files.size() << " entry(s)): " << std::endl;
    
    // Print top border
    std::cout << "+" << std::setfill('-') << std::setw(tableWidth) << "+" << std::endl;
    
    // Print headers
    std::cout << "|"
              << std::setfill(' ') << std::setw(10) << "file name"
              << std::setw(2) << "|"
              << std::setfill(' ') << std::setw(10) << "files size"
              << std::setw(2) << "|" << std::endl;
    
    // Print separator
    std::cout << "+" << std::setfill('-') << std::setw(tableWidth) << "+" << std::endl;
    
    // Print each PCB entry
    for (const auto& file : files) {
        std::cout << "|"
                  << std::setfill(' ') << std::setw(10) << file.program_name
                  << std::setw(2) << "|"
                  << std::setw(10) << file.size
                  << std::setw(2) << "|" << std::endl;
    }
    
    // Print bottom border
    std::cout << "+" << std::setfill('-') << std::setw(tableWidth) << "+" << std::endl;
}

//This function takes as input: the current PCB and the waitqueue (which is a
//std::vector of the PCB struct); the function returns the information as a table
std::string print_PCB(PCB current, std::vector<PCB> _PCB) {
    const int tableWidth = 55;

    std::stringstream buffer;
    
    // Print top border
    buffer << "+" << std::setfill('-') << std::setw(tableWidth) << "+" << std::endl;
    
    // Print headers
    buffer << "|"
              << std::setfill(' ') << std::setw(4) << "PID"
              << std::setw(2) << "|"
              << std::setfill(' ') << std::setw(12) << "program name"
              << std::setw(2) << "|"
              << std::setfill(' ') << std::setw(16) << "partition number"
              << std::setw(2) << "|"
              << std::setfill(' ') << std::setw(5) << "size"
              << std::setw(2) << "|" 
              << std::setfill(' ') << std::setw(8) << "state"
              << std::setw(2) << "|" << std::endl;
    
    // Print separator
    buffer << "+" << std::setfill('-') << std::setw(tableWidth) << "+" << std::endl;

    buffer << "|"
                  << std::setfill(' ') << std::setw(4) << current.PID
                  << std::setw(2) << "|"
                  << std::setw(12) << current.program_name
                  << std::setw(2) << "|"
                  << std::setw(16) << current.partition_number
                  << std::setw(2) << "|"
                  << std::setw(5) << current.size
                  << std::setw(2) << "|"
                  << std::setw(8) << "running"
                  << std::setw(2) << "|" << std::endl;
    
    // Print each PCB entry
    for (const auto& program : _PCB) {
        buffer << "|"
                  << std::setfill(' ') << std::setw(4) << program.PID
                  << std::setw(2) << "|"
                  << std::setw(12) << program.program_name
                  << std::setw(2) << "|"
                  << std::setw(16) << program.partition_number
                  << std::setw(2) << "|"
                  << std::setw(5) << program.size
                  << std::setw(2) << "|"
                  << std::setw(8) << "waiting"
                  << std::setw(2) << "|" << std::endl;
    }
    
    // Print bottom border
    buffer << "+" << std::setfill('-') << std::setw(tableWidth) << "+" << std::endl;

    return buffer.str();
}


// Searches the external_files table and returns the size of the program
unsigned int get_size(std::string name, std::vector<external_file> external_files) {
    int size = -1;

    for (auto file : external_files) { 
        if(file.program_name == name){
            size = file.size;
            break;
        }
    }

    return size;
}

#endif
