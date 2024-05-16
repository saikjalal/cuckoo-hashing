#include <vector>
#include <iostream>
#include <functional>
#include <ctime>

template <class T>
class Sequential {
    // Structure to represent each entry in the hash table
    struct Entry {
        bool occupied; // Indicates if the entry is occupied (true) or empty (false)
        T val;         // The value stored in the entry
        Entry() : occupied(false) {} 
        Entry(T val) : val(val), occupied(true) {} 
    };

    int relocation_limit; 
    int table_capacity;  
    bool resizing = false;
    std::vector<std::vector<Entry*>> buckets; 

    // Primary hash function
    int hashPrimary(const T val) {
        return std::hash<T>{}(val) % table_capacity;
    }

    // Secondary hash function
    int hashSecondary(const T val) {
        return std::hash<T>{}(val) % table_capacity;
    }

    // function to resize the hash table
    bool resize() {
        if (resizing) {
            return false;
        }
        resizing = true;
        
        int new_capacity = table_capacity * 2; // double the capacity
        int new_relocation_limit = new_capacity / 2; // update the relocation limit

        // create a new table with increased capacity
        std::vector<std::vector<Entry*>> new_buckets(2, std::vector<Entry*>(new_capacity, nullptr));

        // rehash all entries from the old table to the new table
        for (auto& bucket_row : buckets) {
            for (auto& entry : bucket_row) {
                if (entry != nullptr && entry->occupied) {
                    int new_index = hashPrimary(entry->val) % new_capacity; // get the new hash index
                    if (new_buckets[0][new_index] == nullptr) {
                        new_buckets[0][new_index] = entry; // place the entry in the new table
                    } else {
                        new_index = hashSecondary(entry->val) % new_capacity; // use the secondary hash function if collision
                        if (new_buckets[1][new_index] == nullptr) {
                            new_buckets[1][new_index] = entry; // place the entry in the new table
                        } else {
                            resizing = false;
                            return false; // unable to add entry to the new table
                        }
                    }
                }
            }
        }

        for (auto& bucket_row : buckets) {
            for (auto& entry : bucket_row) {
                if (entry != nullptr) {
                    delete entry;
                }
            }
        }

        // perform a swap
        buckets.swap(new_buckets);
        table_capacity = new_capacity;
        relocation_limit = new_relocation_limit;

        resizing = false;
        return true;
    }

public:
    // Constructor
    Sequential(int initial_capacity) : table_capacity(initial_capacity), relocation_limit(initial_capacity / 2) {
        for (int i = 0; i < 2; i++) {
            std::vector<Entry*> bucket_row(initial_capacity, nullptr);
            buckets.push_back(bucket_row);
        }
    }

    // Destructor
    ~Sequential() {
        for (auto& bucket_row : buckets) {
            for (auto& entry : bucket_row) {
                delete entry; // Deallocate each entry
            }
        }
    }

    // function add
    bool add(const T val) {
        if (contains(val)) {
            return false; // value already exists
        }
        Entry* new_entry = new Entry(val);
        for (int i = 0; i < relocation_limit; i++) {
            int index = hashPrimary(new_entry->val) % table_capacity;
            if (buckets[0][index] == nullptr || !buckets[0][index]->occupied) {
                if (buckets[0][index] != nullptr) delete buckets[0][index];
                buckets[0][index] = new_entry; // Place in primary bucket 
                return true;
            }
            index = hashSecondary(new_entry->val) % table_capacity;
            if (buckets[1][index] == nullptr || !buckets[1][index]->occupied) {
                if (buckets[1][index] != nullptr) delete buckets[1][index];
                buckets[1][index] = new_entry; // place in secondary bucket if cant in primary or first bucket
                return true;
            }
        }
        if (!resize()) {
            delete new_entry;
            return false; // resize failed
        }
        return add(new_entry->val); // retry
    }

    // function to remove a value from the hash table
    bool remove(const T val) {
        int index_primary = hashPrimary(val);
        int index_secondary = hashSecondary(val);
        if (buckets[0][index_primary] != nullptr && buckets[0][index_primary]->occupied && buckets[0][index_primary]->val == val) {
            buckets[0][index_primary]->occupied = false; // unoccupied
            return true;
        } else if (buckets[1][index_secondary] != nullptr && buckets[1][index_secondary]->occupied && buckets[1][index_secondary]->val == val) {
            buckets[1][index_secondary]->occupied = false; // unoccupied
            return true;
        }
        return false;
    }

    // function to check if a value exists in the hash table
    bool contains(const T val) {
        int index_primary = hashPrimary(val);
        int index_secondary = hashSecondary(val);
        if (buckets[0][index_primary] != nullptr && buckets[0][index_primary]->occupied && buckets[0][index_primary]->val == val) {
            return true;
        } else if (buckets[1][index_secondary] != nullptr && buckets[1][index_secondary]->occupied && buckets[1][index_secondary]->val == val) {
            return true;
        }
        return false;
    }

    // function to get the number of elements in the hash table
    int size() {
        int count = 0;
        for (auto& bucket_row : buckets) {
            for (auto& entry : bucket_row) {
                if (entry != nullptr && entry->occupied) {
                    count++;
                }
            }
        }
        return count;
    }

    // function populate
    bool populate(const std::vector<T> entries) {
        for (const T& entry : entries) {
            if (!add(entry)) {
                return false; // return false if cant
            }
        }
        return true; // return true if the entries have been added successfully
    }
};