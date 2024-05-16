#include <vector>
#include <iostream>
#include <functional>
#include <ctime>

template <class T>
class Sequential {

    //I just made a struct to check if a value would check
    struct Entry {
        bool occupied; // false == empty true == occupied
        T val;
        Entry() : occupied(false) {} 
        Entry(T val) : val(val), occupied(true) {} 
    };

    int LIMIT;
    // limit that is randomized. this is used to prevent an infinite loop
    // limit maximum number of attempts to relocate an
    // entry during an insertion operation before triggering a resize of the hash set
    int capacity;
    bool resizing = false;
    std::vector<std::vector<Entry>> table;

    //for the first hash table
    // standard hash function
    int hashZero(const T val) {
        return std::hash<T>{}(val) % capacity;
    }

    //for the second hash table 
    //standard hash function
    int hashOne(const T val) {
        return std::hash<T>{}(val) % capacity;
    }

    bool resize() {
        if (resizing) {
            return false;
        }
        resizing = true;
        
        int new_capacity = capacity * 2; // we can double the capacity if we need to resize
        int new_limit = new_capacity / 2; // the limit would need to be updated

        // this creates a new table
        std::vector<std::vector<Entry>> new_table(2, std::vector<Entry>(new_capacity));

        
        for (auto& row : table) {
            for (auto& entry : row) {
                if (entry.occupied) {
                    int hash_index = hashZero(entry.val) % new_capacity; // Get the new hash index
                    if (!new_table[0][hash_index].occupied) {
                        new_table[0][hash_index] = entry;
                    } else {
                        hash_index = hashOne(entry.val) % new_capacity; // Use the second hash function if collision
                        if (!new_table[1][hash_index].occupied) {
                            new_table[1][hash_index] = entry;
                        } else {
                            resizing = false;
                            return false; // Unable to add entry to new table
                        }
                    }
                }
            }
        }

        // performs a swap
        table.swap(new_table);
        capacity = new_capacity;
        LIMIT = new_limit;

        resizing = false;
        return true;
    }

public:
    Sequential(int capacity) : capacity(capacity), LIMIT(capacity / 2) {
        for (int i = 0; i < 2; i++) {
            std::vector<Entry> row(capacity); 
            table.push_back(row);
        }
    }

    ~Sequential() {
        // no dynamic allocation for entries
    }

    bool add(const T val) {
        if (contains(val)) {
            return false;
        }
        Entry value(val); 
        for (int i = 0; i < LIMIT; i++) {
            int index = hashZero(value.val) % capacity;
            if (!table[0][index].occupied) { // Check if the slot is unoccupied
                table[0][index] = value; // Assign the value to the slot
                return true;
            }
            index = hashOne(value.val) % capacity;
            if (!table[1][index].occupied) { // check if it is an unoccpied
                table[1][index] = value; 
                return true;
            }
        }
        if (!resize())
            return false;
        return add(value.val);
    }

    bool remove(const T val) {
        int index0 = hashZero(val);
        int index1 = hashOne(val);
        if (table[0][index0].occupied && table[0][index0].val == val) {
            table[0][index0].occupied = false; // this will mark the entry as occupied
            return true;
        } else if (table[1][index1].occupied && table[1][index1].val == val) {
            table[1][index1].occupied = false; // this means it isn't occupied
            return true;
        }
        return false;
    }

    bool contains(const T val) {
        int index0 = hashZero(val);
        int index1 = hashOne(val);
        if (table[0][index0].occupied && table[0][index0].val == val) {
            return true;
        } else if (table[1][index1].occupied && table[1][index1].val == val) {
            return true;
        }
        return false;
    }

    int size() {
        int size = 0;
        for (auto row : table) {
            for (auto entry : row) {
                if (entry.occupied) {
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
