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

#define THINK_COL 1
#define EAT_COL 2
#define PHIL_COL 3
#define FORK_COL 4

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
        wattron(win, COLOR_PAIR(PHIL_COL));
        box(win, 0, 0);
        wattroff(win, COLOR_PAIR(PHIL_COL));
    }
    else
    {
        wattron(win, COLOR_PAIR(FORK_COL));
        box(win, 0, 0);
        wattroff(win, COLOR_PAIR(FORK_COL));
    }
}

struct Fork
{
    Fork(){};
    std::mutex m;
    std::condition_variable cv;
    std::atomic<bool> in_use{false};
    WINDOW *fork_window;
};

struct Philosopher
{
    Philosopher(int id, int left_fork_idx, int right_fork_idx, int window_width, WINDOW *phil_window,
                WINDOW *state_window, WINDOW *resource_window,
                std::vector<Fork> &forks) : id(id), left_fork_idx(left_fork_idx),
                                            right_fork_idx(right_fork_idx), window_width(window_width),
                                            phil_window(phil_window), state_window(state_window),
                                            resource_window(resource_window), forks(forks) {}
    int id;
    int left_fork_idx;
    int right_fork_idx;
    int window_width;
    WINDOW *phil_window;
    WINDOW *state_window;
    WINDOW *resource_window;
    std::vector<Fork> &forks;

    //try to acquire both forks
    void get_forks()
    {
        std::unique_lock<std::mutex> lock_left_fork(forks[left_fork_idx].m);
        while (forks[left_fork_idx].in_use)
        {
            forks[left_fork_idx].cv.wait(lock_left_fork);
        }
        forks[left_fork_idx].in_use = true;
        fork_grabbed_info(left_fork_idx, 'L');
        std::unique_lock<std::mutex> lock_right_fork(forks[right_fork_idx].m);
        while (forks[right_fork_idx].in_use)
        {
            forks[right_fork_idx].cv.wait(lock_right_fork);
        }
        forks[right_fork_idx].in_use = true;
        fork_grabbed_info(right_fork_idx, 'R');
    }

    //release forks when done eating
    void release_forks()
    {
        std::unique_lock<std::mutex> lock_left_fork(forks[left_fork_idx].m);
        forks[left_fork_idx].in_use = false;
        fork_released_info(left_fork_idx, 'L');
        forks[left_fork_idx].cv.notify_one();
        std::unique_lock<std::mutex> lock_right_fork(forks[right_fork_idx].m);
        forks[right_fork_idx].in_use = false;
        fork_released_info(right_fork_idx, 'R');
        forks[right_fork_idx].cv.notify_one();
    }

    //display info about grabbed fork
    void fork_grabbed_info(int fork_idx, char c)
    {
        int where_to_print = 14;
        {
            std::lock_guard<std::mutex> writing_lock(writing_mutex);
            mvwprintw(forks[fork_idx].fork_window, 1, 9, "USED BY PHIL %2d", this->id);
            wrefresh(forks[fork_idx].fork_window);
        }
        if (c == 'R')
        {
            where_to_print = 17;
        }
        {
            std::lock_guard<std::mutex> writing_lock(writing_mutex);
            mvwprintw(this->resource_window, 1, where_to_print, "%2d", fork_idx);
            wrefresh(this->resource_window);
        }
    }

    //display info about released fork
    void fork_released_info(int fork_idx, char c)
    {
        int where_to_print = 14;
        {
            std::lock_guard<std::mutex> writing_lock(writing_mutex);
            mvwprintw(forks[fork_idx].fork_window, 1, 9, "FREE TO GRAB   ");
            wrefresh(forks[fork_idx].fork_window);
        }
        if (c == 'R')
        {
            where_to_print = 17;
        }
        {
            std::lock_guard<std::mutex> writing_lock(writing_mutex);
            mvwprintw(this->resource_window, 1, where_to_print, "XX");
            wrefresh(this->resource_window);
        }
    }

    //eat with two forks
    void eat()
    {
        {
            werase(this->state_window);
            draw_border(this->state_window, 'P');
            std::lock_guard<std::mutex> writing_lock(writing_mutex);
            wattron(this->phil_window, COLOR_PAIR(EAT_COL));
            mvwprintw(this->phil_window, 1, 9, "EATING  ");
            wattroff(this->phil_window, COLOR_PAIR(EAT_COL));
            wrefresh(this->phil_window);
            wattron(this->state_window, COLOR_PAIR(EAT_COL));
        }
        int eating_time = std::experimental::randint(2500, 3500) / this->window_width;
        for (int i = 1; i < this->window_width - 1; i++)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(eating_time));
            {
                std::lock_guard<std::mutex> writing_lock(writing_mutex);
                mvwprintw(this->state_window, 1, i, "$");
                wrefresh(this->state_window);
            }
        }
        wattroff(this->state_window, COLOR_PAIR(EAT_COL));
    }

    //think when not eating
    void think()
    {
        {
            werase(this->state_window);
            draw_border(this->state_window, 'P');
            std::lock_guard<std::mutex> writing_lock(writing_mutex);
            wattron(this->phil_window, COLOR_PAIR(THINK_COL));
            mvwprintw(this->phil_window, 1, 9, "THINKING");
            wattron(this->phil_window, COLOR_PAIR(THINK_COL));
            wrefresh(this->phil_window);
            wattron(this->state_window, COLOR_PAIR(THINK_COL));
        }
        int thinking_time = std::experimental::randint(2500, 3500) / this->window_width;
        for (int i = 1; i < this->window_width - 1; i++)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(thinking_time));
            {
                std::lock_guard<std::mutex> writing_lock(writing_mutex);
                mvwprintw(this->state_window, 1, i, "$");
                wrefresh(this->state_window);
            }
        }
        wattroff(this->state_window, COLOR_PAIR(THINK_COL));
    }

    //thread lifetime
    void feast()
    {
        while (!cancellation_token)
        {
            think();
            get_forks();
            eat();
            release_forks();
        }
    }
};

int main()
{
    initscr();
    echo();
    cbreak();

    start_color();
    init_pair(THINK_COL, COLOR_YELLOW, COLOR_BLACK);
    init_pair(EAT_COL, COLOR_RED, COLOR_BLACK);
    init_pair(PHIL_COL, COLOR_BLUE, COLOR_BLACK);
    init_pair(FORK_COL, COLOR_GREEN, COLOR_BLACK);

    int y_max_size, x_max_size;
    getmaxyx(stdscr, y_max_size, x_max_size);

    int num_of_phils = 5;
    int y_single_window = 3;
    int x_single_window = (x_max_size - 10) / 4;

    WINDOW *inputwin = newwin(y_single_window, (x_max_size - 10) / 2, 0, 5);
    WINDOW *exitwin = newwin(y_single_window, (x_max_size - 10) / 2, 0, x_max_size / 2);
    box(inputwin, 0, 0);
    box(exitwin, 0, 0);
    refresh();
    wrefresh(exitwin);
    mvwprintw(inputwin, 1, 1, "Enter the number of philosophers (i.e. number of threads; should be an int >= 5): ");
    wrefresh(inputwin);

    wscanw(inputwin, "%d", &num_of_phils);
    wrefresh(inputwin);

    mvwprintw(exitwin, 1, 1, "Press q antytime to exit (it takes some time to safely end running threads): ");
    wrefresh(exitwin);

    std::vector<WINDOW *> phil_windows;
    std::vector<WINDOW *> state_windows;
    std::vector<WINDOW *> resource_windows;
    std::vector<WINDOW *> fork_windows;

    std::vector<Philosopher> philosophers;
    std::vector<Fork> forks(num_of_phils);

    //boxes initialization
    for (int i = 0; i < num_of_phils; i++)
    {
        phil_windows.emplace_back(newwin(y_single_window, x_single_window, (i + 1) * y_single_window, 5));
        state_windows.emplace_back(newwin(y_single_window, x_single_window, (i + 1) * y_single_window, (x_max_size + 10) / 4));
        resource_windows.emplace_back(newwin(y_single_window, x_single_window, (i + 1) * y_single_window, x_max_size / 2));
        fork_windows.emplace_back(newwin(y_single_window, x_single_window, (i + 1) * y_single_window, (3 * x_max_size - 10) / 4));
        draw_border(phil_windows[i], 'P');
        draw_border(state_windows[i], 'P');
        draw_border(resource_windows[i], 'P');
        draw_border(fork_windows[i], 'F');
        wrefresh(phil_windows[i]);
        wrefresh(state_windows[i]);
        wrefresh(resource_windows[i]);
        wrefresh(fork_windows[i]);
    }

    for (int i = 0; i < num_of_phils; i++)
    {
        forks[i].fork_window = fork_windows[i];
        philosophers.emplace_back(Philosopher(i, i, (i + 1) % num_of_phils, x_single_window, phil_windows[i], state_windows[i], resource_windows[i], forks));
        mvwprintw(phil_windows[i], 1, 1, "PHIL %2d ", i);
        mvwprintw(resource_windows[i], 1, 1, "FORKS OWNED: ");
        mvwprintw(fork_windows[i], 1, 1, "FORK %2d ", i);
        wrefresh(phil_windows[i]);
        wrefresh(resource_windows[i]);
        wrefresh(fork_windows[i]);
    }

    for (Philosopher p : philosophers)
    {
        p.fork_released_info(p.left_fork_idx, 'L');
    }

    //last phil reaches for fork 0 before last fork
    std::swap(philosophers[philosophers.size() - 1].left_fork_idx, philosophers[philosophers.size() - 1].right_fork_idx);

    std::vector<std::thread> threadList;

    for (int i = 0; i < num_of_phils; i++)
    {
        threadList.emplace_back(std::thread(&Philosopher::feast, &philosophers[i]));
    }

    while (true)
    {
        char c = wgetch(exitwin);
        if (c == 'q')
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