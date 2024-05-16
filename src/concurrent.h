#include <vector>
#include <stdlib.h>
#include <iostream>
#include <functional>
#include <ctime>
#include <list>
#include <mutex>

template <class T>
class CuckooConcurrentHashSet {
    // constants for cuckoo hashing
    const int MAX_BUCKET_SIZE = 8;
    const int MAX_PARTIAL_BUCKET_SIZE = MAX_BUCKET_SIZE / 2;

    int relocation_limit; // number of rounds for relocation
    int table_capacity; // capacity of the table
    std::vector<std::vector<std::list<T>>> buckets; 
    std::vector<std::vector<std::recursive_mutex*>> bucket_locks;

    // primary hash function
    int hash0(const T val) {
        std::hash<T> hasher;
        return hasher(val) % table_capacity;
    }

    // secondary + a shift to avoid same hash
    int hash1(const T val) {
        std::hash<T> hasher;
        return (hasher(val) >> 16) % table_capacity;
    }

    // reallocation if needed
    bool relocate(int current_table, int current_index) {
        int new_index = 0;
        int other_table = 1 - current_table;
        for (int round = 0; round < relocation_limit; round++) {
            T val = buckets[current_table][current_index].front(); // Get the front element of the list
            switch (current_table) {
                case 0: new_index = hash1(val) % table_capacity; break;
                case 1: new_index = hash0(val) % table_capacity; break;
            }
            acquire(val); // Lock both positions associated with val
            auto it = std::find(buckets[current_table][current_index].begin(), buckets[current_table][current_index].end(), val);
            if (it != buckets[current_table][current_index].end()) {
                buckets[current_table][current_index].erase(it); // 
                if (buckets[other_table][new_index].size() < MAX_PARTIAL_BUCKET_SIZE) {
                    buckets[other_table][new_index].push_back(val); // Insert in the new position if under threshold
                    release(val);
                    return true;
                } else if (buckets[other_table][new_index].size() < MAX_BUCKET_SIZE) {
                    buckets[other_table][new_index].push_back(val); // insert
                    current_table = 1 - current_table;
                    current_index = new_index;
                    other_table = 1 - other_table;
                    release(val);
                } else {
                    buckets[current_table][current_index].push_back(val); // Reinsert if both positions are full
                    release(val);
                    return false;
                }
            } else if (buckets[current_table][current_index].size() >= MAX_PARTIAL_BUCKET_SIZE) {
                release(val);
                continue;
            } else {
                release(val);
                return true;
            }
        }
        return false;
    }

    // function to acquire locks for the value
    void acquire(const T val) {
        bucket_locks[0][hash0(val) % bucket_locks[0].size()]->lock();
        bucket_locks[1][hash1(val) % bucket_locks[1].size()]->lock();
    }

    // function to release locks for the value
    void release(const T val) {
        bucket_locks[0][hash0(val) % bucket_locks[0].size()]->unlock();
        bucket_locks[1][hash1(val) % bucket_locks[1].size()]->unlock();
    }

    // function to resize the hash table
    void resize() {
        for (auto lock : bucket_locks[0]) {
            lock->lock(); // lock all buckets to avoid concurrent modifications
        }
        int oldCapacity = table_capacity;
        table_capacity *= 2;
        relocation_limit *= 2;
        std::vector<std::vector<std::list<T>>> old_buckets(buckets); // save the old table
        buckets.clear();
        for (int i = 0; i < 2; i++) {
            std::vector<std::list<T>> bucket_row;
            for (int j = 0; j < table_capacity; j++) {
                std::list<T> bucket;
                bucket_row.push_back(bucket);
            }
            buckets.push_back(bucket_row);
        }
        for (auto bucket_row : old_buckets) {
            for (auto bucket : bucket_row) {
                for (auto entry : bucket) {
                    add(entry); // rehash all entries into the new table
                }
            }
        }
        for (auto lock : bucket_locks[0]) {
            lock->unlock();
        }
    }

    // function to check if an element is present in the table
    bool present(const T val) {
        std::list<T> set0 = buckets[0][hash0(val) % table_capacity];
        if (std::find(set0.begin(), set0.end(), val) != set0.end()) {
            return true;
        } else {
            std::list<T> set1 = buckets[1][hash1(val) % table_capacity];
            if (std::find(set1.begin(), set1.end(), val) != set1.end()) {
                return true;
            }
        }
        return false;
    }

public:
    // constructor to initialize the hash set with given capacity
    CuckooConcurrentHashSet(int initial_capacity) : table_capacity(initial_capacity), relocation_limit(initial_capacity / 2) {
        for (int i = 0; i < 2; i++) {
            std::vector<std::list<T>> bucket_row;
            std::vector<std::recursive_mutex*> locks_row;
            for (int j = 0; j < table_capacity; j++) {
                std::list<T> bucket;
                bucket_row.push_back(bucket);
                locks_row.emplace_back(new std::recursive_mutex());
            }
            buckets.emplace_back(bucket_row);
            bucket_locks.emplace_back(locks_row);
        }
    }

    // destructor to clean up resources
    ~CuckooConcurrentHashSet() {
        for (auto bucket_row : buckets) {
            for (auto bucket : bucket_row) {
                bucket.clear();
            }
            bucket_row.clear();
        }
        buckets.clear();
    }

    // function to add a value to the hash set
    bool add(const T val) {
        acquire(val);
        int hash0_index = hash0(val) % table_capacity;
        int hash1_index = hash1(val) % table_capacity;
        int current_table = -1;
        int current_index = -1;
        bool mustResize = false;
        if (present(val)) { // if value is already present, do nothing
            release(val);
            return false;
        }
        if (buckets[0][hash0_index].size() < MAX_PARTIAL_BUCKET_SIZE) {
            buckets[0][hash0_index].push_back(val); // first table if under threshold
            release(val);
            return true;
        } else if (buckets[1][hash1_index].size() < MAX_PARTIAL_BUCKET_SIZE) {
            buckets[1][hash1_index].push_back(val); // second table if under threshold
            release(val);
            return true;
        } else if (buckets[0][hash0_index].size() < MAX_BUCKET_SIZE) {
            buckets[0][hash0_index].push_back(val); // first table if under max size
            current_table = 0;
            current_index = hash0_index;
        } else if (buckets[1][hash1_index].size() < MAX_BUCKET_SIZE) {
            buckets[1][hash1_index].push_back(val); // second table if under max size
            current_table = 1;
            current_index = hash1_index;
        } else {
            mustResize = true; // table are full resize
        }
        release(val);

        if (mustResize) {
            resize(); //
            add(val);
        } else if (!relocate(current_table, current_index)) {
            resize(); // 
        }
        return true;
    }

    
    bool remove(const T val) {
        acquire(val);
        int hash0_index = hash0(val) % table_capacity;
        auto it0 = std::find(buckets[0][hash0_index].begin(), buckets[0][hash0_index].end(), val);
        if (it0 != buckets[0][hash0_index].end()) {
            buckets[0][hash0_index].erase(it0); 
            release(val);
            return true;
        } else {
            int hash1_index = hash1(val) % table_capacity;
            auto it1 = std::find(buckets[1][hash1_index].begin(), buckets[1][hash1_index].end(), val);
            if (it1 != buckets[1][hash1_index].end()) {
                buckets[1][hash1_index].erase(it1); 
                release(val);
                return true;
            }
        }
        release(val);
        return false;
    }

    
    bool contains(const T val) {
        acquire(val);
        std::list<T> set0 = buckets[0][hash0(val) % table_capacity];
        if (std::find(set0.begin(), set0.end(), val) != set0.cend()) {
            release(val);
            return true;
        } else {
            std::list<T> set1 = buckets[1][hash1(val) % table_capacity];
            if (std::find(set1.begin(), set1.end(), val) != set1.cend()) {
                release(val);
                return true;
            }
        }
        release(val);
        return false;
    }

    // function to get the number of elements in the hash set
    int size() {
        int size = 0;
        for (auto bucket_row : buckets) {
            for (auto bucket : bucket_row) {
                size += bucket.size(); // sum up sizes of all buckets
            }
        }
        return size;
    }

    // function to populate the hash set with a list of entries
    bool populate(const std::vector<T> entries) {
        for (T entry : entries) {
            if (!add(entry)) {
                return false; // return false if any duplicate entry is found
            }
        }
        return true; // successfully added all entries
    }
};