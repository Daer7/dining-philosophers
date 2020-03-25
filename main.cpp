#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>
#include <algorithm>
#include <experimental/random>
#include <atomic>
#include <ncurses.h>
#include <condition_variable>

void clear_line(WINDOW *win)
{
    int y, x;
    getyx(win, y, x);
    wmove(win, y, 1);
    wclrtoeol(win);
}

std::mutex writing_mutex;
std::atomic<bool> cancellation_token{false};

void draw_border(WINDOW *win, char type)
{
    if (type == 'P')
    {
        wattron(win, COLOR_PAIR(1));
        box(win, 0, 0);
        wattroff(win, COLOR_PAIR(1));
    }
    else
    {
        wattron(win, COLOR_PAIR(2));
        box(win, 0, 0);
        wattroff(win, COLOR_PAIR(2));
    }
}

struct Fork
{
    Fork(){};
    std::mutex m;
    std::atomic<bool> in_use{false};
    WINDOW *fork_window;
};

struct Philosopher
{
    Philosopher(int id, int left_fork_idx, int right_fork_idx, WINDOW *phil_window,
                std::vector<Fork> &forks, std::vector<std::condition_variable> &cvs) : id(id), left_fork_idx(left_fork_idx),
                                                                                       right_fork_idx(right_fork_idx), phil_window(phil_window),
                                                                                       forks(forks), cvs(cvs) {}
    int id;
    int left_fork_idx;
    int right_fork_idx;
    WINDOW *phil_window;
    std::vector<Fork> &forks;
    std::vector<std::condition_variable> &cvs;

    void get_forks()
    {
        std::unique_lock<std::mutex> lock_left_fork(forks[left_fork_idx].m);
        while (forks[left_fork_idx].in_use)
        {
            cvs[left_fork_idx].wait(lock_left_fork);
        }
        forks[left_fork_idx].in_use = true;
        fork_grabbed(left_fork_idx);
        std::unique_lock<std::mutex> lock_right_fork(forks[right_fork_idx].m);
        while (forks[right_fork_idx].in_use)
        {
            cvs[right_fork_idx].wait(lock_right_fork);
        }
        forks[right_fork_idx].in_use = true;
        fork_grabbed(right_fork_idx);
        eat();
    }

    void release_forks()
    {
        std::unique_lock<std::mutex> lock_right_fork(forks[right_fork_idx].m);
        forks[right_fork_idx].in_use = false;
        fork_released(right_fork_idx);
        cvs[right_fork_idx].notify_all();
        std::unique_lock<std::mutex> lock_left_fork(forks[left_fork_idx].m);
        forks[left_fork_idx].in_use = false;
        fork_released(left_fork_idx);
        cvs[left_fork_idx].notify_all();
    }

    void fork_grabbed(int fork_idx)
    {
        {
            std::lock_guard<std::mutex> writing_lock(writing_mutex);
            mvwprintw(forks[fork_idx].fork_window, 2, 1, "FORK %2d STATE USED BY PHILOSOPHER %2d", fork_idx, this->id);
            wrefresh(forks[fork_idx].fork_window);
        }
    }

    void fork_released(int fork_idx)
    {
        {
            std::lock_guard<std::mutex> writing_lock(writing_mutex);
            mvwprintw(forks[fork_idx].fork_window, 2, 1, "FORK %2d STATE FREE BY PHILOSOPHER xx", fork_idx);
            wrefresh(forks[fork_idx].fork_window);
        }
    }

    void eat()
    {
        for (int i = 15; i >= 0; i--)
        {
            {
                std::lock_guard<std::mutex> writing_lock(writing_mutex);
                mvwprintw(this->phil_window, 2, 1, "PHILOSOPHER %2d STATE E COUNTDOWN %2d", this->id, i);
                wrefresh(this->phil_window);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(std::experimental::randint(400, 600)));
        }
    }

    void think()
    {
        for (int i = 15; i >= 0; i--)
        {
            {
                std::lock_guard<std::mutex> writing_lock(writing_mutex);
                mvwprintw(this->phil_window, 2, 1, "PHILOSOPHER %2d STATE T COUNTDOWN %2d", this->id, i);
                wrefresh(this->phil_window);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(std::experimental::randint(400, 600)));
        }
    }
    void feast()
    {
        while (!cancellation_token)
        {
            think();
            get_forks();
            release_forks();
        }
    }
};

int main()
{
    initscr();
    echo();
    nocbreak();

    start_color();
    init_pair(1, COLOR_BLUE, COLOR_BLACK);
    init_pair(2, COLOR_GREEN, COLOR_BLACK);

    int y_max_size, x_max_size;
    getmaxyx(stdscr, y_max_size, x_max_size);

    int num_of_phils = 5;
    int y_single_window = 4;

    WINDOW *inputwin = newwin(y_single_window, x_max_size - 10, 0, 5);
    box(inputwin, 0, 0);
    refresh();
    wmove(inputwin, 2, 2);
    wrefresh(inputwin);

    mvwprintw(inputwin, 1, 1, "Enter the number of philosophers (i.e. number of threads; should be an int >= 5): ");
    wrefresh(inputwin);
    wscanw(inputwin, "%d", &num_of_phils);
    wrefresh(inputwin);

    std::vector<WINDOW *> phil_windows;
    std::vector<WINDOW *> fork_windows;

    std::vector<Philosopher> philosophers;
    std::vector<Fork> forks(num_of_phils);
    std::vector<std::condition_variable> cvs(num_of_phils);

    for (int i = 0; i < num_of_phils; i++)
    {
        phil_windows.emplace_back(newwin(y_single_window, (x_max_size - 10) / 2, (i + 1) * y_single_window, 5));
        fork_windows.emplace_back(newwin(y_single_window, (x_max_size - 10) / 2, (i + 1) * y_single_window, x_max_size / 2));
        draw_border(phil_windows[i], 'P');
        draw_border(fork_windows[i], 'F');
        wrefresh(phil_windows[i]);
        wrefresh(fork_windows[i]);
    }

    for (int i = 0; i < num_of_phils; i++)
    {
        forks[i].fork_window = fork_windows[i];
        philosophers.emplace_back(Philosopher(i, i, (i + 1) % num_of_phils, phil_windows[i], forks, cvs));
    }

    std::vector<std::thread> threadList;

    for (int i = 0; i < num_of_phils; i++)
    {
        threadList.emplace_back(std::thread(&Philosopher::feast, &philosophers[i]));
    }

    while (true)
    {
        if (wgetch(inputwin) == 'q')
        {
            cancellation_token = true;
            break;
        }
    }

    for (std::thread &t : threadList)
    {
        t.join();
    }

    endwin();
    return 0;
}