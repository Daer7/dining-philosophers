#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>
#include <algorithm>

std::mutex m_cout;

struct Fork
{
    Fork(){};
    std::mutex m;
};

struct Philosopher
{
    Philosopher(int id, int left_fork_idx, int right_fork_idx) : id(id), left_fork_idx(left_fork_idx), right_fork_idx(right_fork_idx) {}
    int id;
    int left_fork_idx;
    int right_fork_idx;

    void feast(std::vector<Fork> &forks)
    {
        while (true)
        {
            std::lock(forks[left_fork_idx].m, forks[right_fork_idx].m);
            std::lock_guard<std::mutex> left(forks[left_fork_idx].m, std::adopt_lock);
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            std::lock_guard<std::mutex> right(forks[right_fork_idx].m, std::adopt_lock);
            {
                std::lock_guard<std::mutex> lockCout(m_cout);
                std::cout << "Philosopher " << id << " is done thinking and is eating now\n";
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(800));
            {
                std::lock_guard<std::mutex> lockCout(m_cout);
                std::cout << "Philosopher " << id << " is done eating and is thinking now\n";
            }
        }
    }
};

int main()
{
    int num_of_phils = 5;
    std::cout << "Enter the number of philosophers (i.e. number of threads; should be an int >= 5): ";
    std::cin >> num_of_phils;

    std::vector<Philosopher> philosophers;
    std::vector<Fork> forks(num_of_phils);

    for (int i = 0; i < num_of_phils; i++)
    {
        philosophers.emplace_back(Philosopher(i, i, (i + 1) % num_of_phils));
    }

    std::vector<std::thread> threadList;

    for (int i = 0; i < num_of_phils; i++)
    {
        threadList.emplace_back(std::thread(&Philosopher::feast, &philosophers[i], std::ref(forks)));
    }

    for (std::thread &t : threadList)
    {
        t.join();
    }

    return 0;
}