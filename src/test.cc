#include <stdlib.h>
#include <iostream>
#include <vector>
#include <unordered_set>
#include <chrono>
#include <random>
#include <mutex>
#include <thread>

#include "sequential.h"
#include "concurrent.h"

const int NUM_OPS = 15000000;
const int CAPACITY = 12000;
const int KEY_MAX = 1500;
const int INITIAL_SIZE = KEY_MAX / 2;
const int NUM_THREADS = 8;

struct Operation {
    int val;
    int type; // 0 => contains, 1 => add, 2 => remove
    Operation(int val, int type) : val(val), type(type) {}
};

struct Metrics {
    long long exec_time = 0;
};

std::vector<int> generate_entries(int num_entries) {
    auto seed = std::chrono::high_resolution_clock::now()
            .time_since_epoch()
            .count();
    static thread_local std::mt19937 generator(seed);
    std::uniform_int_distribution<int> entry_generator(0, KEY_MAX);

    std::unordered_set<int> entries;
    while (entries.size() < num_entries) {
        entries.insert(entry_generator(generator));
    }
    return {entries.begin(), entries.end()};
}

std::vector<Operation> generate_operations(int num_ops, std::vector<int>* entries) {
    // 80% contains, 10% add, 10% remove
    auto seed = std::chrono::high_resolution_clock::now()
            .time_since_epoch()
            .count();
    static thread_local std::mt19937 generator(seed);
    std::uniform_int_distribution<int> distribution_percentage(1, 100);
    std::uniform_int_distribution<int> distribution_entries(0, KEY_MAX);
    std::vector<Operation> ops;
    for (int i = 0; i < num_ops; i++) {
        std::uniform_int_distribution<int> distribution_existing_entries(0, entries->size() - 1);
        int which_op = distribution_percentage(generator);
        if (which_op <= 80) {
    
            ops.emplace_back(distribution_entries(generator), 0);
        } else if (which_op <= 90) {
            int entry = distribution_entries(generator);
            entries->push_back(entry);
            ops.emplace_back(entry, 1);
        } else {
            ops.emplace_back(entries->at(distribution_existing_entries(generator)), 2);
        }
    }
    return ops;
}

long long sequential_workload(Sequential<int> *cuckoo_sequential, std::vector<Operation> ops) {

    auto exec_time_start = std::chrono::high_resolution_clock::now();
    for (auto op : ops) {
        switch (op.type) {
            case 0:
                cuckoo_sequential->contains(op.val); //contains operation
                break;
            case 1:
                cuckoo_sequential->add(op.val); //add operation
                break;
            default:
                cuckoo_sequential->remove(op.val); //remove operation
                break;
        }
    }
    auto exec_time_end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(exec_time_end - exec_time_start).count();
}

//concurrent
long long do_work_concurrent(CuckooConcurrentHashSet<int> *cuckoo_concurrent, std::vector<int> concurrent_entries) {
    // Start doing work
    auto exec_time_start = std::chrono::high_resolution_clock::now();
    auto ops = generate_operations(NUM_OPS, &concurrent_entries);
    for (auto op : ops) {
        switch (op.type) {
            // Contains
            case 0:
                cuckoo_concurrent->contains(op.val);
                break;
            // Insert
            case 1:
                cuckoo_concurrent->add(op.val);
                break;
            // Remove
            default:
                cuckoo_concurrent->remove(op.val);
                break;
        }
    }
    auto exec_time_end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(exec_time_end - exec_time_start).count();
}

int main(int argc, char *argv[]) {
    Sequential<int> *cuckoo_sequential = new Sequential<int>(CAPACITY);
    auto sequential_operations = generate_entries(INITIAL_SIZE);
    if (!cuckoo_sequential->populate(sequential_operations))
        return 0;
    auto sequential_ops = generate_operations(NUM_OPS * NUM_THREADS, &sequential_operations);
    long long serial_exec_time = sequential_workload(cuckoo_sequential, sequential_ops);
    std::cout << "Sequential time (milliseconds):\t\t" << serial_exec_time << std::endl;
    delete cuckoo_sequential;

    CuckooConcurrentHashSet<int> *cuckoo_concurrent = new CuckooConcurrentHashSet<int>(CAPACITY);
    auto concurrent_entries = generate_entries(INITIAL_SIZE);
    if (!cuckoo_concurrent->populate(concurrent_entries))
        return 0;
    long long total_concurrent_exec_time = 0;
    for (int thread = 0; thread < NUM_THREADS; thread++) {
        total_concurrent_exec_time += do_work_concurrent(cuckoo_concurrent, concurrent_entries);
    }
    long long avg_concurrent_exec_time = total_concurrent_exec_time / NUM_THREADS;
    std::cout << "Concurrent time (miliseconds):\t\t" << avg_concurrent_exec_time << std::endl;
    delete cuckoo_concurrent;

    return 0;
}
