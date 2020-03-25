#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>
#include <algorithm>
#include <experimental/random>
#include <atomic>
#include <ncurses.h>

void clear_line(WINDOW *win)
{
    int y, x;
    getyx(win, y, x);
    wmove(win, y, 1);
    wclrtoeol(win);
}

std::mutex writing_mutex;
std::atomic<bool> cancellation_token(false);

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
    Philosopher(int id, int left_fork_idx, int right_fork_idx, WINDOW *phil_window) : id(id), left_fork_idx(left_fork_idx),
                                                                                      right_fork_idx(right_fork_idx), phil_window(phil_window) {}
    int id;
    int left_fork_idx;
    int right_fork_idx;
    WINDOW *phil_window;

    void eat_or_think(char symbol)
    {
        for (int i = 15; i >= 0; i--)
        {
            {
                std::lock_guard<std::mutex> writing_lock(writing_mutex);
                mvwprintw(this->phil_window, 2, 1, "PHILOSOPHER %2d STATE %c COUNTDOWN %2d", this->id, symbol, i);
                wrefresh(this->phil_window);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(std::experimental::randint(400, 600)));
        }
    }

    void feast(std::vector<Fork> &forks)
    {
        while (!cancellation_token)
        {
            if (!forks[left_fork_idx].in_use && !forks[right_fork_idx].in_use)
            {
                std::lock(forks[left_fork_idx].m, forks[right_fork_idx].m);
                std::lock_guard<std::mutex> left(forks[left_fork_idx].m, std::adopt_lock);
                std::lock_guard<std::mutex> right(forks[right_fork_idx].m, std::adopt_lock);
                forks[left_fork_idx].in_use = true;
                forks[right_fork_idx].in_use = true;
                eat_or_think('E');
                forks[left_fork_idx].in_use = false;
                forks[right_fork_idx].in_use = false;
            }
            else
            {
                eat_or_think('T');
            }
        }
    }
};

int main()
{
    initscr();
    echo();
    cbreak();

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
        philosophers.emplace_back(Philosopher(i, i, (i + 1) % num_of_phils, phil_windows[i]));
    }

    std::vector<std::thread> threadList;

    for (int i = 0; i < num_of_phils; i++)
    {
        threadList.emplace_back(std::thread(&Philosopher::feast, &philosophers[i], std::ref(forks)));
    }

    while (true)
    {
        if (wgetch(inputwin) == 'q')
        {
            cancellation_token = true;
            mvwprintw(inputwin, 2, 5, "lolz");
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