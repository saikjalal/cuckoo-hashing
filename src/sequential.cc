#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <chrono>

// Entry struct declaration outside of the CuckooSerialHashSet class
struct Entry {
    const int val;
    Entry(int val) : val(val) {}
};

template <class T>
class CuckooSerialHashSet {
    // Wrapper class for entries to allow for nullptr to be the default
    // struct Entry {
    //     const T val;
    //     Entry(T val) : val(val) {}
    // };

    int limit;
    size_t salt0;
    size_t salt1;
    int capacity;
    bool resizing = false;
    std::vector<std::vector<Entry*>> table;

    // Taken from boost hash_combine
    template <class D>
    inline void hash_combine(std::size_t& seed, const D& v) {
        std::hash<D> hasher;
        seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }

    int hash0(const T val) {
        size_t seed = 0;
        hash_combine(seed, val);
        hash_combine(seed, salt0);
        return seed % capacity;
    }

    int hash1(const T val) {
        size_t seed = 0;
        hash_combine(seed, val);
        hash_combine(seed, salt1);
        return seed % capacity;
    }

    /**
     * Resizes the table to be twice as big. Changes salt0 and salt1.
     */
    bool resize() {
        if (resizing) {
            return false;
        }
        resizing = true;
        bool done;
        std::vector<std::vector<Entry*>> old_table;
        do {
            done = true;
            // Get new salt values to change the hashes
            hash_combine(salt0, time(NULL));
            hash_combine(salt1, time(NULL));

            capacity *= 2;
            limit *= 2;
            old_table = table;
            table.clear();
            for (int i = 0; i < 2; i++) {
                std::vector<Entry*> row;
                row.assign(capacity, nullptr);
                table.push_back(row);
            }

            // Add the elements back into the bigger table
            [&] {
                for (auto row : old_table) {
                    for (auto entry : row) {
                        if (entry != nullptr && !add(entry)) {
                            done = false;
                            table = old_table;
                            return;
                        }
                    }
                }
            }();
        } while (!done);
        for (auto row : old_table) {
            for (auto entry : row) {
                if (entry != nullptr)
                    delete entry;
            }
            row.clear();
        }
        old_table.clear();
        resizing = false;
        return true;
    }

public:
    CuckooSerialHashSet(int capacity) : capacity(capacity), limit(capacity / 2) {
        for (int i = 0; i < 2; i++) {
            std::vector<Entry*> row;
            row.assign(capacity, nullptr);
            table.push_back(row);
        }
        salt0 = time(NULL);
        salt1 = salt0;
        hash_combine(salt1, capacity);
    }

    ~CuckooSerialHashSet() {
        for (auto row : table) {
            for (auto entry : row) {
                if (entry != nullptr) {
                    delete entry;
                }
            }
            row.clear();
        }
        table.clear();
    }

    /**
     * Swaps the element at table[table_index][index] with val.
     * return: The old val
     */
    Entry* swap(const int table_index, const int index, Entry* val) {
        Entry* swap_val = table[table_index][index];
        table[table_index][index] = val;
        return swap_val;
    }

    /**
     * Adds val
     * return: true if add was successful
     */
    bool add(Entry* value) {
        if (contains(value->val)) {
            return false;
        }
        for (int i = 0; i < limit; i++) {
            if ((value = swap(0, hash0(value->val), value)) == nullptr) {
                return true;
            } else if ((value = swap(1, hash1(value->val), value)) == nullptr) {
                return true;
            }
        }
        if (!resize())
            return false;
        return add(value);
    }

    /**
     * Removes val
     * return: true if remove was successful
     */
    bool remove(const T val) {
        int index0 = hash0(val);
        int index1 = hash1(val);
        if (table[0][index0] != nullptr && table[0][index0]->val == val) {
            delete table[0][index0];
            table[0][index0] = nullptr;
            return true;
        } else if (table[1][index1] != nullptr && table[1][index1]->val == val) {
            delete table[1][index1];
            table[1][index1] = nullptr;
            return true;
        }
        return false;
    }

    /**
     * Checks if the table contains val
     * return: true if the table contains val
     */
    bool contains(const T val) {
        int index0 = hash0(val);
        int index1 = hash1(val);
        if (table[0][index0] != nullptr && table[0][index0]->val == val) {
            return true;
        } else if (table[1][index1] != nullptr && table[1][index1]->val == val) {
            return true;
        }
        return false;
    }

    /**
     * Counts the number of elements in the table
     * return: The number of elements in the table
     */
    int size() {
        int size = 0;
        for (auto row : table) {
            for (auto entry : row) {
                if (entry != nullptr) {
                    size++;
                }
            }
        }
        return size;
    }

    /**
     * Populates the table to some predetermined size
     * return: true if successful
     */
    bool populate(const std::vector<T> entries) {
        for (T entry : entries) {
            Entry* new_entry = new Entry(entry);
            if (!add(new_entry)) {
                std::cout << "Duplicate entry attempted for populate!" << std::endl;
                delete new_entry;
                return false;
            }
        }
        return true;
    }
};

int main() {
    const int CAPACITY = 100; // Choose an appropriate capacity
    CuckooSerialHashSet<int> hash_set(CAPACITY);

    // Populate the hash set with random elements
    std::vector<int> test_elements;
    for (int i = 0; i < 1000; ++i) {
        test_elements.push_back(rand() % 1000);
    }
    hash_set.populate(test_elements);

    // Test adding elements and measure execution time
    auto start_time = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; ++i) {
        int rand_val = rand() % 1000;
        Entry* entry = new Entry(rand_val); // Create Entry object dynamically
        hash_set.add(entry); // Pass the entry pointer
    }
    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> add_time = end_time - start_time;
    std::cout << "Add operation executed in: " << add_time.count() << " milliseconds." << std::endl;

    // Test checking containment and measure execution time
    start_time = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; ++i) {
        hash_set.contains(rand() % 1000);
    }
    end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> contains_time = end_time - start_time;
    std::cout << "Contains operation executed in: " << contains_time.count() << " milliseconds." << std::endl;

    // Test removing elements and measure execution time
    start_time = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; ++i) {
        hash_set.remove(rand() % 1000);
    }
    end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> remove_time = end_time - start_time;
    std::cout << "Remove operation executed in: " << remove_time.count() << " milliseconds." << std::endl;

    return 0;
}