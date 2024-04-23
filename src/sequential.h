#include <vector>
#include <iostream>
#include <functional>
#include <ctime>

template <class T>
class Sequential {

    struct Entry {
        const T val;
        Entry(T val) : val(val) {}
    };

    int LIMIT;
    // limit that is randomized. this is used to prevent an infinite loop
    // limit maximum number of attempts to relocate an
    // entry during an insertion operation before triggering a resize of the hash set
    int capacity;
    bool resizing = false;
    std::vector<std::vector<Entry*>> table;

/*
   I used the standard hash function std::hash
   Source: https://en.cppreference.com/w/cpp/utility/hash
*/

    int hashZero(const T val) {
        return std::hash<T>{}(val) % capacity;
    }

    int hashOne(const T val) {
        return std::hash<T>{}(val) % capacity;
    }

    bool resize() {
        if (resizing) {
            return false;
        }
        resizing = true;
        
        int new_capacity = capacity * 2; // Double the capacity
        int new_limit = new_capacity / 2; // Update the limit

        // Create a new table with the doubled capacity
        std::vector<std::vector<Entry*>> new_table(2, std::vector<Entry*>(new_capacity, nullptr));

        // Move elements from the old table to the new table
        for (auto& row : table) {
            for (auto& entry : row) {
                if (entry != nullptr) {
                    int hash_index = hashZero(entry->val) % new_capacity; // Get the new hash index
                    if (new_table[0][hash_index] == nullptr) {
                        new_table[0][hash_index] = entry;
                    } else {
                        hash_index = hashOne(entry->val) % new_capacity; // Use the second hash function if collision
                        if (new_table[1][hash_index] == nullptr) {
                            new_table[1][hash_index] = entry;
                        } else {
                            resizing = false;
                            return false; // Unable to add entry to new table
                        }
                    }
                }
            }
        }

        // swap the old and new table
        table.swap(new_table);
        capacity = new_capacity;
        LIMIT = new_limit;

        resizing = false;
        return true;
    }

public:
    Sequential(int capacity) : capacity(capacity), LIMIT(capacity / 2) {
        for (int i = 0; i < 2; i++) {
            std::vector<Entry*> row;
            row.assign(capacity, nullptr);
            table.push_back(row);
        }
    }

    ~Sequential() {
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

    Entry* swap(const int table_index, const int index, Entry *val) {
        Entry *swap_val = table[table_index][index];
        table[table_index][index] = val;
        return swap_val;
    }

    bool add(const T val) {
        if (contains(val)) {
            return false;
        }
        Entry *value = new Entry(val);
        for (int i = 0; i < LIMIT; i++) {
            if ((value = swap(0, hashZero(value->val), value)) == nullptr) {
                return true;
            } else if ((value = swap(1, hashOne(value->val), value)) == nullptr) {
                return true;
            }
        }
        if (!resize())
            return false;
        return add(value->val);
    }

    bool remove(const T val) {
        int index0 = hashZero(val);
        int index1 = hashOne(val);
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

    bool contains(const T val) {
        int index0 = hashZero(val);
        int index1 = hashOne(val);
        if (table[0][index0] != nullptr && table[0][index0]->val == val) {
            return true;
        } else if (table[1][index1] != nullptr && table[1][index1]->val == val) {
            return true;
        }
        return false;
    }

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

    bool populate(const std::vector<T> entries) {
        for (T entry : entries) {
            if (!add(entry)) {
                return false;
            }
        }
        return true;
    }
};
