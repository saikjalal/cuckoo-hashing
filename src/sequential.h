#include <vector>
#include <iostream>
#include <functional>
#include <ctime>

template <class T>
class CuckooSerialHashSet {
    struct Entry {
        const T val;
        Entry(T val) : val(val) {}
    };

    int LIMIT;
    int capacity;
    bool resizing = false;
    std::vector<std::vector<Entry*>> table;

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
        bool done;
        std::vector<std::vector<Entry*>> old_table;
        do {
            done = true;
            capacity *= 2;
            LIMIT *= 2;
            old_table = table;
            table.clear();
            for (int i = 0; i < 2; i++) {
                std::vector<Entry*> row;
                row.assign(capacity, nullptr);
                table.push_back(row);
            }

            for (auto row : old_table) {
                for (auto entry : row) {
                    if (entry != nullptr && !add(entry->val)) {
                        done = false;
                        table = old_table;
                        resizing = false; // Reset resizing flag if resizing fails
                        return false;
                    }
                }
            }
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
    CuckooSerialHashSet(int capacity) : capacity(capacity), LIMIT(capacity / 2) {
        for (int i = 0; i < 2; i++) {
            std::vector<Entry*> row;
            row.assign(capacity, nullptr);
            table.push_back(row);
        }
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
                std::cout << "Duplicate entry attempted for populate!" << std::endl;
                return false;
            }
        }
        return true;
    }
};
