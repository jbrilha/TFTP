#include "progress.h"

void get_size_unit(size_t bytes, int *unit, double *divisor) {
    if (bytes >= GB) {
        *unit = 3;
        *divisor = GB;
    } else if (bytes >= MB) {
        *unit = 2;
        *divisor = MB;
    } else if (bytes >= KB) {
        *unit = 1;
        *divisor = KB;
    } else {
        *unit = 0;
        *divisor = 1.0;
    }
}

const char *get_unit_str(int unit_index, bool is_rate) {
    static const char *units[] = {"B", "KB", "MB", "GB"};
    static const char *rate_units[] = {"B/s", "KB/s", "MB/s", "GB/s"};
    return is_rate ? rate_units[unit_index] : units[unit_index];
}

void init_progress(struct sender_progress *progress, size_t total_bytes) {
    progress->total_bytes = total_bytes;
    progress->blocks_sent = 0;
    progress->current_bytes = 0;
    progress->last_percent = -1;
    progress->bar_width = 50;
    get_size_unit(total_bytes, &progress->unit_index, &progress->unit_div);
    gettimeofday(&progress->start_time, NULL);
}

void update_progress(struct sender_progress *progress, size_t new_bytes) {
    struct timeval now;
    gettimeofday(&now, NULL);

    progress->current_bytes += new_bytes;
    progress->blocks_sent++;

    int percent = (int)((progress->current_bytes * 100) / progress->total_bytes);

    if (percent != progress->last_percent) {
        float elapsed =
            (now.tv_sec - progress->start_time.tv_sec) +
            (now.tv_usec - progress->start_time.tv_usec) / 1000000.0f;
        float rate = progress->current_bytes / (elapsed * progress->unit_div);

        int pos = progress->bar_width * percent / 100;

        printf("\r[");
        for (int i = 0; i < progress->bar_width; i++) {
            if (i < pos)
                printf("=");
            else if (i == pos)
                printf(">");
            else
                printf(" ");
        }

        printf("] %3d%% %.2f %s",
               percent,
               rate,
               get_unit_str(progress->unit_index, true));
        fflush(stdout);

        progress->last_percent = percent;
    }
}

void show_sender_completion(struct sender_progress *progress) {
    struct timeval end;
    gettimeofday(&end, NULL);

    float elapsed = (end.tv_sec - progress->start_time.tv_sec) +
                    (end.tv_usec - progress->start_time.tv_usec) / 1000000.0f;
    float avg_rate = progress->total_bytes / (elapsed * progress->unit_div);

    printf(
        "\nTransfer complete: Sent %zu blocks (%.2f %s) in %.1f seconds (%.2f "
        "%s)\n\n",
        progress->blocks_sent,
        progress->total_bytes / progress->unit_div,
        get_unit_str(progress->unit_index, false),
        elapsed,
        avg_rate,
        get_unit_str(progress->unit_index, true));
}

void init_stats(struct receiver_stats *stats) {
    stats->total_bytes = 0;
    stats->blocks_received = 0;
    gettimeofday(&stats->start_time, NULL);
}

void update_stats(struct receiver_stats *stats, size_t bytes) {
    struct timeval now;
    gettimeofday(&now, NULL);

    stats->total_bytes += bytes;
    stats->blocks_received++;

    // no one will even see this probably
    char* block = stats->blocks_received > 1 ? "blocks" : "block";

    int unit;
    double divisor;
    get_size_unit(stats->total_bytes, &unit, &divisor);

    float elapsed =
        (now.tv_sec - stats->start_time.tv_sec) +
        (now.tv_usec - stats->start_time.tv_usec) / 1000000.0f;
    float rate = stats->total_bytes / (elapsed * divisor);

    printf("\r\033[K"); // to fully clear line when units change
    printf("Receiving %zu data %s (%.2f %s) @ %.2f %s",
           stats->blocks_received,
           block,
           stats->total_bytes / divisor,
           get_unit_str(unit, false),
           rate,
           get_unit_str(unit, true));
    fflush(stdout);
}

void show_receiver_stats(struct receiver_stats *stats) {
    struct timeval end;
    gettimeofday(&end, NULL);

    float elapsed = (end.tv_sec - stats->start_time.tv_sec) +
                    (end.tv_usec - stats->start_time.tv_usec) / 1000000.0f;

    int unit;
    double divisor;
    get_size_unit(stats->total_bytes, &unit, &divisor);

    float rate = stats->total_bytes / (elapsed * divisor);

    printf("\nTransfer complete: Received %zu blocks (%.*f %s) in %.1f seconds "
           "(%.2f %s)\n\n",
           stats->blocks_received,
           unit == 0 ? 0 : 2,
           stats->total_bytes / divisor,
           get_unit_str(unit, false),
           elapsed,
           rate,
           get_unit_str(unit, true));
}
