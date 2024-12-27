#ifndef PROGRESS_H
#define PROGRESS_H

#include "common.h"

static const double KB = 1024.0;
static const double MB = 1024.0 * 1024.0;
static const double GB = 1024.0 * 1024.0 * 1024.0;

struct sender_progress {
    size_t total_bytes;
    size_t current_bytes;
    size_t blocks_sent;
    struct timeval start_time;
    int last_percent;
    int bar_width;
    int unit_index;  // Store the unit we'll use throughout
    double unit_div; // Store the divisor for this unit
};

struct receiver_stats {
    size_t total_bytes;
    size_t blocks_received;
    struct timeval start_time;
};

void get_size_unit(size_t bytes, int *unit, double *divisor);

const char *get_unit_str(int unit_index, bool is_rate);

void init_progress(struct sender_progress *progress, size_t total_bytes);

void update_progress(struct sender_progress *progress, size_t new_bytes);

void show_sender_completion(struct sender_progress *progress);

void init_stats(struct receiver_stats *stats);

void update_stats(struct receiver_stats *stats, size_t bytes);

void show_receiver_stats(struct receiver_stats *stats);

#endif // PROGRESS_H
